#include "DefenderWmi.h"

#include <oleauto.h>
#include <wbemidl.h>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <map>
#include <mutex>

namespace {

constexpr wchar_t kWmiNamespace[] = L"ROOT\\Microsoft\\Windows\\Defender";

template <typename T>
class ComPtr {
public:
    ComPtr() = default;
    explicit ComPtr(T* raw) : ptr_(raw) {}
    ~ComPtr() { reset(); }

    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    ComPtr(ComPtr&& other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }
    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            reset();
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    T** out() {
        reset();
        return &ptr_;
    }
    T* get() const { return ptr_; }
    T* operator->() const { return ptr_; }
    explicit operator bool() const { return ptr_ != nullptr; }

    void reset() {
        if (ptr_) {
            ptr_->Release();
            ptr_ = nullptr;
        }
    }

private:
    T* ptr_ = nullptr;
};

// Owning BSTR so no call site has to pair SysAllocString with SysFreeString.
class Bstr {
public:
    Bstr() = default;
    explicit Bstr(const wchar_t* text) : value_(SysAllocString(text ? text : L"")) {}
    ~Bstr() { SysFreeString(value_); }

    Bstr(const Bstr&) = delete;
    Bstr& operator=(const Bstr&) = delete;

    BSTR get() const { return value_; }

private:
    BSTR value_ = nullptr;
};

std::wstring HresultCode(HRESULT hr) {
    wchar_t buffer[16] = {};
    swprintf(buffer, 16, L"0x%08lX", static_cast<unsigned long>(hr));
    return buffer;
}

std::wstring SystemMessage(HRESULT hr) {
    wchar_t buffer[512] = {};
    const DWORD length = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, hr, 0,
                                        buffer, 512, nullptr);
    if (length == 0) return L"unknown error";
    std::wstring text(buffer, length);
    while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n' || text.back() == L' ')) text.pop_back();
    return text;
}

// WBEM_E_* codes are not translated by FormatMessage, so name the ones the user
// will actually hit (access denied is the Tamper Protection case).
std::wstring DescribeHresult(HRESULT hr, const std::wstring& context) {
    std::wstring detail;
    switch (static_cast<unsigned long>(hr)) {
        case 0x80041003UL:
            detail = L"access denied - run elevated and check Tamper Protection in Windows Security";
            break;
        case 0x80041002UL:
            detail = L"object not found";
            break;
        case 0x8004100EUL:
            detail = L"invalid WMI namespace";
            break;
        case 0x80041010UL:
            detail = L"invalid WMI class";
            break;
        case 0x80041011UL:
            detail = L"WMI provider not found (Windows Defender may be disabled or replaced)";
            break;
        case 0x80041017UL:
            detail = L"invalid method parameter";
            break;
        default:
            detail = SystemMessage(hr);
            break;
    }
    return context + L": " + detail + L" (" + HresultCode(hr) + L")";
}

void EnsureComSecurity() {
    static std::once_flag flag;
    std::call_once(flag, [] {
        // RPC_E_TOO_LATE is fine: the host already picked a security model.
        CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
                             nullptr, EOAC_NONE, nullptr);
    });
}

bool PropertyVariant(IWbemClassObject* object, const wchar_t* name, VARIANT& value) {
    VariantInit(&value);
    if (FAILED(object->Get(name, 0, &value, nullptr, nullptr))) return false;
    if (value.vt == VT_NULL || value.vt == VT_EMPTY) {
        VariantClear(&value);
        return false;
    }
    return true;
}

std::wstring PropertyString(IWbemClassObject* object, const wchar_t* name) {
    VARIANT value;
    if (!PropertyVariant(object, name, value)) return std::wstring();
    std::wstring text;
    if (value.vt == VT_BSTR && value.bstrVal) {
        text.assign(value.bstrVal, SysStringLen(value.bstrVal));
    } else {
        VARIANT converted;
        VariantInit(&converted);
        if (SUCCEEDED(VariantChangeType(&converted, &value, 0, VT_BSTR)) && converted.vt == VT_BSTR &&
            converted.bstrVal) {
            text.assign(converted.bstrVal, SysStringLen(converted.bstrVal));
        }
        VariantClear(&converted);
    }
    VariantClear(&value);
    return text;
}

bool PropertyBool(IWbemClassObject* object, const wchar_t* name, bool fallback) {
    VARIANT value;
    if (!PropertyVariant(object, name, value)) return fallback;
    bool result = fallback;
    if (value.vt == VT_BOOL) {
        result = value.boolVal != VARIANT_FALSE;
    } else {
        VARIANT converted;
        VariantInit(&converted);
        if (SUCCEEDED(VariantChangeType(&converted, &value, 0, VT_I4))) result = converted.lVal != 0;
        VariantClear(&converted);
    }
    VariantClear(&value);
    return result;
}

long long PropertyInteger(IWbemClassObject* object, const wchar_t* name, long long fallback) {
    VARIANT value;
    if (!PropertyVariant(object, name, value)) return fallback;
    long long result = fallback;
    VARIANT converted;
    VariantInit(&converted);
    if (SUCCEEDED(VariantChangeType(&converted, &value, 0, VT_I8))) result = converted.llVal;
    VariantClear(&converted);
    VariantClear(&value);
    return result;
}

std::vector<std::wstring> PropertyStringArray(IWbemClassObject* object, const wchar_t* name) {
    std::vector<std::wstring> items;
    VARIANT value;
    if (!PropertyVariant(object, name, value)) return items;

    if ((value.vt & VT_ARRAY) != 0 && (value.vt & VT_BSTR) != 0 && value.parray != nullptr) {
        LONG lower = 0;
        LONG upper = -1;
        if (SUCCEEDED(SafeArrayGetLBound(value.parray, 1, &lower)) &&
            SUCCEEDED(SafeArrayGetUBound(value.parray, 1, &upper))) {
            for (LONG index = lower; index <= upper; ++index) {
                BSTR item = nullptr;
                if (SUCCEEDED(SafeArrayGetElement(value.parray, &index, &item)) && item != nullptr) {
                    items.emplace_back(item, SysStringLen(item));
                    SysFreeString(item);
                }
            }
        }
    }
    VariantClear(&value);
    return items;
}

VARIANT StringArrayVariant(const std::vector<std::wstring>& items) {
    VARIANT value;
    VariantInit(&value);
    SAFEARRAY* array = SafeArrayCreateVector(VT_BSTR, 0, static_cast<ULONG>(items.size()));
    if (array == nullptr) return value;

    for (LONG index = 0; index < static_cast<LONG>(items.size()); ++index) {
        BSTR item = SysAllocString(items[static_cast<size_t>(index)].c_str());
        if (item == nullptr || FAILED(SafeArrayPutElement(array, &index, item))) {
            SysFreeString(item);
            SafeArrayDestroy(array);
            return value;
        }
        SysFreeString(item);
    }
    value.vt = VT_ARRAY | VT_BSTR;
    value.parray = array;
    return value;
}

// WMI DATETIME is YYYYMMDDHHMMSS.ffffff+UUU; the classic UI only shows the date.
std::wstring FormatWmiDate(const std::wstring& wmiDate) {
    if (wmiDate.size() < 8) return std::wstring();
    for (size_t index = 0; index < 8; ++index) {
        if (wmiDate[index] < L'0' || wmiDate[index] > L'9') return std::wstring();
    }
    if (wmiDate.compare(0, 4, L"0000") == 0) return std::wstring();
    return wmiDate.substr(6, 2) + L"/" + wmiDate.substr(4, 2) + L"/" + wmiDate.substr(0, 4);
}

std::wstring JoinResources(const std::vector<std::wstring>& items) {
    std::wstring joined;
    for (const std::wstring& item : items) {
        if (!joined.empty()) joined += L"; ";
        joined += item;
    }
    return joined;
}

}  // namespace

ComApartment::ComApartment() : result_(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {
    // RPC_E_CHANGED_MODE means the thread already joined an apartment, which is
    // still usable for WMI; only a hard failure disables the client.
    usable_ = SUCCEEDED(result_) || result_ == RPC_E_CHANGED_MODE;
}

ComApartment::~ComApartment() {
    if (SUCCEEDED(result_)) CoUninitialize();
}

std::wstring SeverityName(int severityId) {
    switch (severityId) {
        case 1:
            return L"Low";
        case 2:
            return L"Moderate";
        case 4:
            return L"High";
        case 5:
            return L"Severe";
        default:
            return L"Unknown";
    }
}


namespace {

std::wstring EnvValue(const wchar_t* name) {
    wchar_t buffer[MAX_PATH * 2] = {};
    const DWORD length = GetEnvironmentVariableW(name, buffer, static_cast<DWORD>(std::size(buffer)));
    if (length == 0 || length >= std::size(buffer)) return std::wstring();
    return std::wstring(buffer, length);
}

// MpCmdRun lives in the newest versioned Platform folder; the legacy location is
// only a fallback for old builds.
std::wstring FindMpCmdRun() {
    const std::wstring programData = EnvValue(L"ProgramData");
    if (!programData.empty()) {
        const std::wstring platform = programData + L"\\Microsoft\\Windows Defender\\Platform";
        WIN32_FIND_DATAW data = {};
        HANDLE search = FindFirstFileW((platform + L"\\*").c_str(), &data);
        if (search != INVALID_HANDLE_VALUE) {
            std::wstring newest;
            do {
                if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) continue;
                const std::wstring name = data.cFileName;
                if (name == L"." || name == L"..") continue;
                const std::wstring candidate = platform + L"\\" + name + L"\\MpCmdRun.exe";
                if (GetFileAttributesW(candidate.c_str()) == INVALID_FILE_ATTRIBUTES) continue;
                if (newest.empty() || wcscmp(name.c_str(), newest.c_str()) > 0) newest = name;
            } while (FindNextFileW(search, &data));
            FindClose(search);
            if (!newest.empty()) return platform + L"\\" + newest + L"\\MpCmdRun.exe";
        }
    }

    const std::wstring programFiles = EnvValue(L"ProgramFiles");
    if (!programFiles.empty()) {
        const std::wstring legacy = programFiles + L"\\Windows Defender\\MpCmdRun.exe";
        if (GetFileAttributesW(legacy.c_str()) != INVALID_FILE_ATTRIBUTES) return legacy;
    }
    return std::wstring();
}

bool RunAndWait(const std::wstring& exe, const std::wstring& args, DWORD& exitCode) {
    exitCode = 1;
    if (exe.empty()) return false;
    std::wstring commandLine = L"\"" + exe + L"\" " + args;
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                        &startup, &process))
        return false;
    WaitForSingleObject(process.hProcess, 180000);
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}

}  // namespace

std::wstring ThreatStatusName(int statusId) {
    switch (statusId) {
        case 1:
            return L"Detected";
        case 2:
            return L"Cleaned";
        case 3:
            return L"Quarantined";
        case 4:
            return L"Removed";
        case 5:
            return L"Allowed";
        case 6:
            return L"Blocked";
        default:
            return L"Unknown";
    }
}

struct DefenderClient::Impl {
    ComPtr<IWbemLocator> locator;
    ComPtr<IWbemServices> service;
    std::wstring lastError;

    bool Connect();
    bool QueryAll(const wchar_t* wql, const std::function<void(IWbemClassObject*)>& visit);
    bool QueryFirst(const wchar_t* wql, ComPtr<IWbemClassObject>& object);
    bool Invoke(const std::wstring& objectPath, const wchar_t* method,
                const std::function<void(IWbemClassObject*)>& fill);
    bool InvokeStatic(const wchar_t* className, const wchar_t* method,
                      const std::function<void(IWbemClassObject*)>& fill);
    bool PreferenceInstance(std::wstring& path, ComPtr<IWbemClassObject>& instance);
    bool PreferenceMethod(const wchar_t* method, const std::function<void(IWbemClassObject*)>& fill);
    bool WritePreference(const wchar_t* property, VARIANT& value);
    // Add/Remove take one array parameter per supported preference; only
    // ExclusionPath is populated, the rest keep their null defaults.
    bool ModifyPathList(const wchar_t* method, const std::wstring& item,
                        const std::function<void(std::vector<std::wstring>&)>& editFallback);
};

bool DefenderClient::Impl::Connect() {
    if (service) return true;

    EnsureComSecurity();
    HRESULT hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator,
                                  reinterpret_cast<void**>(locator.out()));
    if (FAILED(hr)) {
        lastError = DescribeHresult(hr, L"WMI is unavailable");
        return false;
    }

    const Bstr nameSpace(kWmiNamespace);
    hr = locator->ConnectServer(nameSpace.get(), nullptr, nullptr, nullptr, 0, 0, nullptr, service.out());
    if (FAILED(hr)) {
        lastError = DescribeHresult(hr, L"Cannot connect to ROOT\\Microsoft\\Windows\\Defender");
        return false;
    }

    hr = CoSetProxyBlanket(service.get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL,
                           RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    if (FAILED(hr)) {
        lastError = DescribeHresult(hr, L"CoSetProxyBlanket failed");
        service.reset();
        return false;
    }
    return true;
}

bool DefenderClient::Impl::QueryAll(const wchar_t* wql, const std::function<void(IWbemClassObject*)>& visit) {
    if (!Connect()) return false;

    const Bstr language(L"WQL");
    const Bstr query(wql);
    ComPtr<IEnumWbemClassObject> enumerator;
    HRESULT hr = service->ExecQuery(language.get(), query.get(), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                                    nullptr, enumerator.out());
    if (FAILED(hr) || !enumerator) {
        lastError = DescribeHresult(hr, wql);
        return false;
    }

    for (;;) {
        IWbemClassObject* raw = nullptr;
        ULONG returned = 0;
        hr = enumerator->Next(WBEM_INFINITE, 1, &raw, &returned);
        if (FAILED(hr) || returned == 0 || raw == nullptr) break;
        ComPtr<IWbemClassObject> object(raw);
        visit(object.get());
    }
    return true;
}

bool DefenderClient::Impl::QueryFirst(const wchar_t* wql, ComPtr<IWbemClassObject>& object) {
    bool found = false;
    if (!QueryAll(wql, [&](IWbemClassObject* candidate) {
            if (found) return;
            candidate->AddRef();
            object = ComPtr<IWbemClassObject>(candidate);
            found = true;
        }))
        return false;

    if (!found) {
        lastError = std::wstring(L"No instance returned by: ") + wql;
        return false;
    }
    return true;
}

bool DefenderClient::Impl::Invoke(const std::wstring& objectPath, const wchar_t* method,
                                  const std::function<void(IWbemClassObject*)>& fill) {
    if (!Connect()) return false;

    const Bstr className(L"MSFT_MpPreference");
    ComPtr<IWbemClassObject> klass;
    HRESULT hr = service->GetObject(className.get(), 0, nullptr, klass.out(), nullptr);
    if (FAILED(hr)) {
        lastError = DescribeHresult(hr, L"GetObject MSFT_MpPreference");
        return false;
    }

    ComPtr<IWbemClassObject> definition;
    hr = klass->GetMethod(method, 0, definition.out(), nullptr);
    if (FAILED(hr) || !definition) {
        lastError = DescribeHresult(hr, std::wstring(L"MSFT_MpPreference has no method ") + method);
        return false;
    }

    ComPtr<IWbemClassObject> parameters;
    hr = definition->SpawnInstance(0, parameters.out());
    if (FAILED(hr) || !parameters) {
        lastError = DescribeHresult(hr, L"SpawnInstance failed");
        return false;
    }

    fill(parameters.get());

    const Bstr path(objectPath.c_str());
    const Bstr methodName(method);
    hr = service->ExecMethod(path.get(), methodName.get(), 0, nullptr, parameters.get(), nullptr, nullptr);
    if (FAILED(hr)) {
        lastError = DescribeHresult(hr, method);
        return false;
    }
    return true;
}

bool DefenderClient::Impl::InvokeStatic(const wchar_t* className, const wchar_t* method,
                                        const std::function<void(IWbemClassObject*)>& fill) {
    if (!Connect()) return false;

    const Bstr classBstr(className);
    ComPtr<IWbemClassObject> klass;
    HRESULT hr = service->GetObject(classBstr.get(), 0, nullptr, klass.out(), nullptr);
    if (FAILED(hr)) {
        lastError = DescribeHresult(hr, className);
        return false;
    }

    ComPtr<IWbemClassObject> definition;
    hr = klass->GetMethod(method, 0, definition.out(), nullptr);
    if (FAILED(hr) || !definition) {
        lastError = DescribeHresult(hr, std::wstring(className) + L"." + method);
        return false;
    }

    ComPtr<IWbemClassObject> parameters;
    hr = definition->SpawnInstance(0, parameters.out());
    if (FAILED(hr) || !parameters) {
        lastError = DescribeHresult(hr, L"SpawnInstance failed");
        return false;
    }
    if (fill) fill(parameters.get());

    const Bstr methodName(method);
    hr = service->ExecMethod(classBstr.get(), methodName.get(), 0, nullptr, parameters.get(), nullptr, nullptr);
    if (FAILED(hr)) {
        lastError = DescribeHresult(hr, std::wstring(className) + L"." + method);
        return false;
    }
    return true;
}

bool DefenderClient::Impl::PreferenceInstance(std::wstring& path, ComPtr<IWbemClassObject>& instance) {
    if (!QueryFirst(L"SELECT * FROM MSFT_MpPreference", instance)) return false;

    VARIANT value;
    VariantInit(&value);
    if (FAILED(instance->Get(L"__PATH", 0, &value, nullptr, nullptr)) || value.vt != VT_BSTR || !value.bstrVal) {
        VariantClear(&value);
        lastError = L"MSFT_MpPreference exposes no __PATH";
        return false;
    }
    path.assign(value.bstrVal, SysStringLen(value.bstrVal));
    VariantClear(&value);
    return true;
}

bool DefenderClient::Impl::PreferenceMethod(const wchar_t* method,
                                            const std::function<void(IWbemClassObject*)>& fill) {
    std::wstring path;
    ComPtr<IWbemClassObject> instance;
    if (!PreferenceInstance(path, instance)) return false;
    return Invoke(path, method, fill);
}

// Set() is the documented way to toggle a single preference. Older builds only
// accept PutInstance, so fall back to writing the instance property directly.
bool DefenderClient::Impl::WritePreference(const wchar_t* property, VARIANT& value) {
    VARIANT copy;
    VariantInit(&copy);
    if (FAILED(VariantCopy(&copy, &value))) {
        lastError = L"VariantCopy failed";
        return false;
    }

    const bool methodWorked = PreferenceMethod(L"Set", [&](IWbemClassObject* parameters) {
        parameters->Put(property, 0, &copy, 0);
    });
    VariantClear(&copy);
    if (methodWorked) return true;

    const std::wstring methodError = lastError;

    std::wstring path;
    ComPtr<IWbemClassObject> instance;
    if (!PreferenceInstance(path, instance)) {
        lastError = methodError + L"; " + lastError;
        return false;
    }

    const Bstr name(property);
    HRESULT hr = instance->Put(name.get(), 0, &value, 0);
    if (FAILED(hr)) {
        lastError = DescribeHresult(hr, property);
        return false;
    }
    hr = service->PutInstance(instance.get(), WBEM_FLAG_UPDATE_ONLY, nullptr, nullptr);
    if (FAILED(hr)) {
        lastError = DescribeHresult(hr, std::wstring(L"PutInstance ") + property);
        return false;
    }
    return true;
}

DefenderClient::DefenderClient() : impl_(std::make_unique<Impl>()) {}
DefenderClient::~DefenderClient() = default;

const std::wstring& DefenderClient::LastError() const {
    return impl_->lastError;
}

bool DefenderClient::QueryStatus(DefenderStatus& status) {
    status = DefenderStatus{};

    ComPtr<IWbemClassObject> object;
    if (!impl_->QueryFirst(L"SELECT * FROM MSFT_MpComputerStatus", object)) return false;

    status.wmiConnected = true;
    status.antivirusEnabled = PropertyBool(object.get(), L"AntivirusEnabled", true);
    status.realTimeProtection = PropertyBool(object.get(), L"RealTimeProtectionEnabled", false);
    status.behaviorMonitoring = PropertyBool(object.get(), L"BehaviorMonitorEnabled", false);
    status.cloudProtection = PropertyBool(object.get(), L"IoavProtectionEnabled", false);
    status.tamperProtection = PropertyBool(object.get(), L"IsTamperProtected", false);

    status.signatureVersion = PropertyString(object.get(), L"AntivirusSignatureVersion");
    status.engineVersion = PropertyString(object.get(), L"AMEngineVersion");
    status.serviceVersion = PropertyString(object.get(), L"AMServiceVersion");

    status.lastQuickScan = FormatWmiDate(PropertyString(object.get(), L"QuickScanEndTime"));
    status.lastFullScan = FormatWmiDate(PropertyString(object.get(), L"FullScanEndTime"));
    status.lastSignatureUpdate = FormatWmiDate(PropertyString(object.get(), L"AntivirusSignatureLastUpdated"));

    if (status.lastQuickScan.empty()) status.lastQuickScan = L"Never";
    if (status.lastFullScan.empty()) status.lastFullScan = L"Never";
    if (status.lastSignatureUpdate.empty()) status.lastSignatureUpdate = L"Unknown";
    if (status.signatureVersion.empty()) status.signatureVersion = L"Unknown";
    if (status.engineVersion.empty()) status.engineVersion = L"Unknown";
    if (status.serviceVersion.empty()) status.serviceVersion = L"Unknown";
    return true;
}

bool DefenderClient::QueryThreats(std::vector<ThreatRecord>& threats) {
    threats.clear();

    struct ThreatInfo {
        std::wstring name;
        int severityId = 0;
        int statusId = 0;
        std::wstring detected;
        std::wstring resources;
    };

    std::map<long long, ThreatInfo> catalogue;
    const bool catalogueOk = impl_->QueryAll(L"SELECT * FROM MSFT_MpThreat", [&](IWbemClassObject* object) {
        const long long id = PropertyInteger(object, L"ThreatID", 0);
        ThreatInfo info;
        info.name = PropertyString(object, L"ThreatName");
        info.severityId = static_cast<int>(PropertyInteger(object, L"SeverityID", 0));
        info.statusId = static_cast<int>(PropertyInteger(object, L"ThreatStatusID", 0));
        info.detected = FormatWmiDate(PropertyString(object, L"InitialDetectionTime"));
        info.resources = JoinResources(PropertyStringArray(object, L"Resources"));
        if (id != 0 && !info.name.empty()) catalogue[id] = std::move(info);
    });

    if (!catalogueOk) return false;

    bool detectionsOk = impl_->QueryAll(L"SELECT * FROM MSFT_MpThreatDetection", [&](IWbemClassObject* object) {
        ThreatRecord record;
        record.threatId = PropertyInteger(object, L"ThreatID", 0);
        const auto known = catalogue.find(record.threatId);
        if (known != catalogue.end()) {
            record.name = known->second.name;
            record.severityId = known->second.severityId;
        } else {
            record.name = L"Threat " + std::to_wstring(record.threatId);
        }
        record.statusId = static_cast<int>(PropertyInteger(object, L"ThreatStatusID", 0));
        record.detected = FormatWmiDate(PropertyString(object, L"InitialDetectionTime"));
        if (record.detected.empty() && known != catalogue.end()) record.detected = known->second.detected;
        record.resources = JoinResources(PropertyStringArray(object, L"Resources"));
        if (record.resources.empty() && known != catalogue.end()) record.resources = known->second.resources;
        record.actionSucceeded = PropertyBool(object, L"ActionSuccess", true);
        threats.push_back(std::move(record));
    });

    if (!detectionsOk) return false;

    if (threats.empty() && !catalogue.empty()) {
        for (const auto& entry : catalogue) {
            ThreatRecord record;
            record.threatId = entry.first;
            record.name = entry.second.name;
            record.severityId = entry.second.severityId;
            record.statusId = entry.second.statusId;
            record.detected = entry.second.detected;
            record.resources = entry.second.resources;
            record.actionSucceeded = true;
            threats.push_back(std::move(record));
        }
    }
    return true;
}

bool DefenderClient::QueryExclusions(std::vector<std::wstring>& paths) {
    paths.clear();
    ComPtr<IWbemClassObject> instance;
    if (!impl_->QueryFirst(L"SELECT * FROM MSFT_MpPreference", instance)) return false;
    paths = PropertyStringArray(instance.get(), L"ExclusionPath");
    return true;
}

bool DefenderClient::StartScan(long scanType, const std::wstring& path) {
    return impl_->InvokeStatic(L"MSFT_MpScan", L"Start", [&](IWbemClassObject* parameters) {
        const Bstr scanTypeName(L"ScanType");
        VARIANT type;
        VariantInit(&type);
        type.vt = VT_I4;
        type.lVal = scanType;
        parameters->Put(scanTypeName.get(), 0, &type, 0);

        if (!path.empty()) {
            const Bstr pathName(L"ScanPath");
            const Bstr value(path.c_str());
            VARIANT target;
            VariantInit(&target);
            target.vt = VT_BSTR;
            target.bstrVal = value.get();
            parameters->Put(pathName.get(), 0, &target, 0);
        }
    });
}

bool DefenderClient::UpdateSignatures() {
    return impl_->InvokeStatic(L"MSFT_MpSignature", L"Update", nullptr);
}

bool DefenderClient::Impl::ModifyPathList(const wchar_t* method, const std::wstring& item,
                                          const std::function<void(std::vector<std::wstring>&)>& editFallback) {
    if (PreferenceMethod(method, [&](IWbemClassObject* parameters) {
            const Bstr name(L"ExclusionPath");
            VARIANT value = StringArrayVariant({item});
            parameters->Put(name.get(), 0, &value, 0);
            VariantClear(&value);
        }))
        return true;

    // Some builds reject the array methods; rewrite the property in place.
    std::wstring instancePath;
    ComPtr<IWbemClassObject> instance;
    if (!PreferenceInstance(instancePath, instance)) return false;

    std::vector<std::wstring> paths = PropertyStringArray(instance.get(), L"ExclusionPath");
    const size_t before = paths.size();
    editFallback(paths);
    if (paths.size() == before) return true;  // already in the desired state

    const Bstr name(L"ExclusionPath");
    VARIANT value = StringArrayVariant(paths);
    if (value.vt == VT_EMPTY) return false;
    const HRESULT putHr = instance->Put(name.get(), 0, &value, 0);
    VariantClear(&value);
    if (FAILED(putHr)) return false;
    return SUCCEEDED(service->PutInstance(instance.get(), WBEM_FLAG_UPDATE_ONLY, nullptr, nullptr));
}


bool DefenderClient::RestoreThreat(const std::wstring& threatName) {
    DWORD exitCode = 1;
    const std::wstring exe = FindMpCmdRun();
    if (!RunAndWait(exe, L"-Restore -Name \"" + threatName + L"\"", exitCode)) {
        impl_->lastError = L"MpCmdRun.exe could not be started; quarantine actions need the Defender platform "
                           L"folder.";
        return false;
    }
    if (exitCode != 0) {
        impl_->lastError = L"MpCmdRun -Restore returned " + std::to_wstring(exitCode) +
                           L". Tamper Protection blocks quarantine changes, and the item must still exist.";
        return false;
    }
    return true;
}

bool DefenderClient::RemoveThreat(const std::wstring& threatName) {
    DWORD exitCode = 1;
    const std::wstring exe = FindMpCmdRun();
    if (!RunAndWait(exe, L"-Remove -Name \"" + threatName + L"\"", exitCode)) {
        impl_->lastError = L"MpCmdRun.exe could not be started; quarantine actions need the Defender platform "
                           L"folder.";
        return false;
    }
    if (exitCode != 0) {
        impl_->lastError = L"MpCmdRun -Remove returned " + std::to_wstring(exitCode) +
                           L". Tamper Protection blocks quarantine changes.";
        return false;
    }
    return true;
}

bool DefenderClient::AddExclusion(const std::wstring& path) {
    return impl_->ModifyPathList(L"Add", path, [&](std::vector<std::wstring>& paths) { paths.push_back(path); });
}

bool DefenderClient::RemoveExclusion(const std::wstring& path) {
    return impl_->ModifyPathList(L"Remove", path, [&](std::vector<std::wstring>& paths) {
        paths.erase(std::remove_if(paths.begin(), paths.end(),
                                   [&](const std::wstring& candidate) {
                                       return _wcsicmp(candidate.c_str(), path.c_str()) == 0;
                                   }),
                    paths.end());
    });
}

bool DefenderClient::SetRealTimeProtection(bool enabled) {
    const Bstr name(L"DisableRealtimeMonitoring");
    VARIANT value;
    VariantInit(&value);
    value.vt = VT_BOOL;
    value.boolVal = enabled ? VARIANT_FALSE : VARIANT_TRUE;
    const bool ok = impl_->WritePreference(name.get(), value);
    VariantClear(&value);
    return ok;
}

bool DefenderClient::SetCloudProtection(bool enabled) {
    const Bstr name(L"MAPSReporting");
    VARIANT value;
    VariantInit(&value);
    value.vt = VT_I4;
    value.lVal = enabled ? 2 : 0;  // 0 = off, 1 = basic, 2 = advanced
    const bool ok = impl_->WritePreference(name.get(), value);
    VariantClear(&value);
    return ok;
}
