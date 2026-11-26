#include "Settings.h"

#include <vector>

namespace {

constexpr wchar_t kAppKey[] = L"Software\\ClassicDefenderUI";
constexpr wchar_t kThemeSubKey[] = L"Software\\ClassicDefenderUI\\Theme";
constexpr wchar_t kUiSubKey[] = L"Software\\ClassicDefenderUI\\Ui";
constexpr wchar_t kValuePreset[] = L"Preset";
constexpr wchar_t kValueCustomColors[] = L"CustomColors";
constexpr wchar_t kValueLastTab[] = L"LastTab";

bool ReadDword(HKEY root, const wchar_t* subKey, const wchar_t* name, DWORD& value) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, subKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return false;
    DWORD type = 0;
    DWORD size = sizeof(DWORD);
    const LONG rc = RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size);
    RegCloseKey(key);
    return rc == ERROR_SUCCESS && type == REG_DWORD && size == sizeof(DWORD);
}

bool WriteDword(HKEY root, const wchar_t* subKey, const wchar_t* name, DWORD value) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(root, subKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return false;
    const LONG rc = RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
    RegCloseKey(key);
    return rc == ERROR_SUCCESS;
}

bool ReadBinary(HKEY root, const wchar_t* subKey, const wchar_t* name, std::vector<BYTE>& out) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, subKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return false;

    DWORD type = 0;
    DWORD size = 0;
    if (RegQueryValueExW(key, name, nullptr, &type, nullptr, &size) != ERROR_SUCCESS || type != REG_BINARY) {
        RegCloseKey(key);
        return false;
    }
    out.resize(size);
    const LONG rc = RegQueryValueExW(key, name, nullptr, &type, out.data(), &size);
    RegCloseKey(key);
    if (rc != ERROR_SUCCESS) return false;
    out.resize(size);
    return true;
}

bool WriteBinary(HKEY root, const wchar_t* subKey, const wchar_t* name, const void* data, DWORD size) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(root, subKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return false;
    const LONG rc = RegSetValueExW(key, name, 0, REG_BINARY, static_cast<const BYTE*>(data), size);
    RegCloseKey(key);
    return rc == ERROR_SUCCESS;
}

}  // namespace

namespace settings {

bool Load(AppSettings& out, HKEY root) {
    out = AppSettings{};

    DWORD preset = 0;
    if (ReadDword(root, kThemeSubKey, kValuePreset, preset) && preset <= static_cast<DWORD>(kCustomThemeIndex))
        out.presetIndex = static_cast<int>(preset);

    out.theme = PresetTheme(out.presetIndex);

    for (const ThemeFieldInfo& info : kThemeFields) {
        DWORD packed = 0;
        if (ReadDword(root, kThemeSubKey, info.key, packed)) ThemeFieldRef(out.theme, info.field) = static_cast<COLORREF>(packed);
    }

    std::vector<BYTE> palette;
    if (ReadBinary(root, kThemeSubKey, kValueCustomColors, palette) && palette.size() == out.customColors.size() * sizeof(COLORREF))
        memcpy(out.customColors.data(), palette.data(), palette.size());

    DWORD lastTab = 0;
    if (ReadDword(root, kUiSubKey, kValueLastTab, lastTab) && lastTab <= 2) out.lastTab = static_cast<int>(lastTab);

    return true;
}

bool Save(const AppSettings& in, HKEY root) {
    bool ok = true;

    ok = WriteDword(root, kThemeSubKey, kValuePreset, static_cast<DWORD>(in.presetIndex)) && ok;
    for (const ThemeFieldInfo& info : kThemeFields)
        ok = WriteDword(root, kThemeSubKey, info.key, static_cast<DWORD>(ThemeFieldColor(in.theme, info.field))) && ok;

    ok = WriteBinary(root, kThemeSubKey, kValueCustomColors, in.customColors.data(),
                     static_cast<DWORD>(in.customColors.size() * sizeof(COLORREF))) &&
         ok;
    ok = WriteDword(root, kUiSubKey, kValueLastTab, static_cast<DWORD>(in.lastTab)) && ok;

    return ok;
}

}  // namespace settings
