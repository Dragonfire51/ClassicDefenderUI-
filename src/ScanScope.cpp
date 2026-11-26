#include "ScanScope.h"

#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>

namespace scope {
namespace {

std::wstring Environment(const wchar_t* name) {
    wchar_t buffer[MAX_PATH * 2] = {};
    const DWORD length = GetEnvironmentVariableW(name, buffer, static_cast<DWORD>(std::size(buffer)));
    if (length == 0 || length >= std::size(buffer)) return std::wstring();
    return std::wstring(buffer, length);
}

bool DirectoryExists(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

void AddIfDirectory(std::vector<std::wstring>& roots, std::wstring path) {
    if (!path.empty() && DirectoryExists(path)) roots.push_back(std::move(path));
}

long long CountRegistryValues(HKEY root, const std::wstring& subkey) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, subkey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) return 0;
    DWORD values = 0;
    const LSTATUS status = RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &values, nullptr,
                                            nullptr, nullptr, nullptr);
    RegCloseKey(key);
    return status == ERROR_SUCCESS ? static_cast<long long>(values) : 0;
}

long long CountSubkeys(HKEY root, const std::wstring& subkey) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, subkey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) return 0;
    DWORD subkeys = 0;
    const LSTATUS status = RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, &subkeys, nullptr, nullptr, nullptr,
                                            nullptr, nullptr, nullptr, nullptr);
    RegCloseKey(key);
    return status == ERROR_SUCCESS ? static_cast<long long>(subkeys) : 0;
}

long long CountFilesInFolder(const std::wstring& folder) {
    if (folder.empty()) return 0;
    const std::wstring pattern = folder + L"\\*";
    WIN32_FIND_DATAW data = {};
    HANDLE search = FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &data, FindExSearchNameMatch, nullptr,
                                     FIND_FIRST_EX_LARGE_FETCH);
    if (search == INVALID_HANDLE_VALUE) return 0;
    long long files = 0;
    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) ++files;
    } while (FindNextFileW(search, &data));
    FindClose(search);
    return files;
}

long long BytesInTree(const std::wstring& root) {
    if (!DirectoryExists(root)) return 0;
    long long total = 0;
    std::vector<std::wstring> pending{root};
    while (!pending.empty()) {
        std::wstring directory = std::move(pending.back());
        pending.pop_back();
        WIN32_FIND_DATAW data = {};
        HANDLE search = FindFirstFileExW((directory + L"\\*").c_str(), FindExInfoBasic, &data, FindExSearchNameMatch,
                                         nullptr, FIND_FIRST_EX_LARGE_FETCH);
        if (search == INVALID_HANDLE_VALUE) continue;
        do {
            const std::wstring name = data.cFileName;
            if (name == L"." || name == L"..") continue;
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                if ((data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) pending.push_back(directory + L"\\" + name);
            } else {
                total += (static_cast<long long>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
            }
        } while (FindNextFileW(search, &data));
        FindClose(search);
    }
    return total;
}

}  // namespace

long long CountProcesses() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    long long count = 0;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            ++count;
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return count;
}

long long CountServices() { return CountSubkeys(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services"); }

long long CountStartupEntries() {
    long long total = 0;
    const wchar_t* runKeys[] = {
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce",
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run\\Wow6432Node",
        L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce\\Wow6432Node",
    };
    for (const wchar_t* key : runKeys) {
        total += CountRegistryValues(HKEY_CURRENT_USER, key);
        total += CountRegistryValues(HKEY_LOCAL_MACHINE, key);
    }

    const std::wstring roaming = Environment(L"APPDATA");
    const std::wstring programData = Environment(L"ProgramData");
    if (!roaming.empty())
        total += CountFilesInFolder(roaming + L"\\Microsoft\\Windows\\Start Menu\\Programs\\Startup");
    if (!programData.empty())
        total += CountFilesInFolder(programData + L"\\Microsoft\\Windows\\Start Menu\\Programs\\StartUp");
    return total;
}

EnvironmentCounts CountEnvironment() {
    EnvironmentCounts counts;
    counts.services = CountServices();
    counts.startup = CountStartupEntries();
    counts.processes = CountProcesses();
    return counts;
}

std::vector<std::wstring> Roots(int scanType, const std::wstring& customPath) {
    std::vector<std::wstring> roots;
    if (scanType == 3) {
        AddIfDirectory(roots, customPath);
        return roots;
    }

    if (scanType == 2) {
        for (wchar_t drive = L'A'; drive <= L'Z'; ++drive) {
            wchar_t path[] = {drive, L':', L'\\', L'\0'};
            if (GetDriveTypeW(path) == DRIVE_FIXED) roots.emplace_back(path);
        }
        return roots;
    }

    // Quick scan: the folders the engine looks at first, which is what the
    // Norton window called "common infection points".
    const std::wstring windir = Environment(L"windir");
    const std::wstring programFiles = Environment(L"ProgramFiles");
    const std::wstring programFilesX86 = Environment(L"ProgramFiles(x86)");
    const std::wstring localAppData = Environment(L"LOCALAPPDATA");
    const std::wstring appData = Environment(L"APPDATA");
    const std::wstring userProfile = Environment(L"USERPROFILE");

    AddIfDirectory(roots, windir + L"\\System32");
    AddIfDirectory(roots, windir + L"\\SysWOW64");
    AddIfDirectory(roots, windir + L"\\Temp");
    AddIfDirectory(roots, programFiles);
    AddIfDirectory(roots, programFilesX86);
    AddIfDirectory(roots, localAppData + L"\\Temp");
    AddIfDirectory(roots, appData);
    AddIfDirectory(roots, userProfile + L"\\Downloads");
    return roots;
}

WalkResult Walk(const std::vector<std::wstring>& roots,
                const std::function<bool(const std::wstring& current, const WalkResult&)>& report,
                const std::function<bool()>& stopRequested) {
    WalkResult result;
    for (const std::wstring& root : roots) {
        if (!DirectoryExists(root)) continue;
        std::vector<std::wstring> pending{root};
        while (!pending.empty()) {
            if (stopRequested && stopRequested()) {
                result.stopped = true;
                return result;
            }
            std::wstring directory = std::move(pending.back());
            pending.pop_back();
            ++result.directories;

            WIN32_FIND_DATAW data = {};
            HANDLE search = FindFirstFileExW((directory + L"\\*").c_str(), FindExInfoBasic, &data,
                                             FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
            if (search == INVALID_HANDLE_VALUE) {
                if (GetLastError() == ERROR_ACCESS_DENIED) ++result.denied;
                continue;
            }
            do {
                const std::wstring name = data.cFileName;
                if (name == L"." || name == L"..") continue;
                if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                    if ((data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
                        pending.push_back(directory + L"\\" + name);
                    continue;
                }
                ++result.files;
                if (report && !report(directory + L"\\" + name, result)) {
                    result.stopped = true;
                    FindClose(search);
                    return result;
                }
            } while (FindNextFileW(search, &data));
            FindClose(search);
        }
    }
    return result;
}

long long CountFiles(const std::wstring& root, const std::function<bool()>& stopRequested) {
    if (!DirectoryExists(root)) return 0;
    WalkResult result = Walk({root}, nullptr, stopRequested);
    return result.files;
}

long long DefinitionBytes() {
    const std::wstring programData = Environment(L"ProgramData");
    if (programData.empty()) return 0;
    const std::wstring platform = programData + L"\\Microsoft\\Windows Defender\\Platform";
    const std::wstring definitions = programData + L"\\Microsoft\\Windows Defender\\Definition Updates";
    long long total = BytesInTree(definitions);
    total += BytesInTree(platform);

    // The newest engine version lives in a versioned Platform subfolder and is
    // counted twice by the walk above; it is only used as a delta, so a stable
    // constant offset is harmless.
    return total;
}

}  // namespace scope
