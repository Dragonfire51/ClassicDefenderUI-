#pragma once

#include <windows.h>

#include <cstddef>

// Every colour the classic 2002 skin needs. Stored one registry value per field
// so a single colour can be changed without rewriting the whole theme.
enum class ThemeField {
    TopBarBg = 0,
    SidebarBg,
    SidebarAccent,
    BrandPrimary,
    BrandSecondary,
    ContentBg,
    SectionHeaderBg,
    Border,
    TextPrimary,
    TextMuted,
    OkGreen,
    WarnRed,
    Count
};

struct AppTheme {
    COLORREF topBarBg;
    COLORREF sidebarBg;
    COLORREF sidebarAccent;
    COLORREF brandPrimary;
    COLORREF brandSecondary;
    COLORREF contentBg;
    COLORREF sectionHeaderBg;
    COLORREF border;
    COLORREF textPrimary;
    COLORREF textMuted;
    COLORREF okGreen;
    COLORREF warnRed;
};

struct ThemeFieldInfo {
    ThemeField field;
    const wchar_t* key;    // registry value name under HKCU\Software\ClassicDefenderUI\Theme
    const wchar_t* label;  // shown in the options dialog
    size_t offset;         // offsetof(AppTheme, ...)
};

inline constexpr int kThemeFieldCapacity = static_cast<int>(ThemeField::Count);

// One row per ThemeField, in enum order. The offset feeds the generic
// accessors, the key is the registry value name inside ...\ClassicDefenderUI\Theme.
inline constexpr ThemeFieldInfo kThemeFields[kThemeFieldCapacity] = {
    {ThemeField::TopBarBg, L"TopBarBg", L"Top bar background", offsetof(AppTheme, topBarBg)},
    {ThemeField::SidebarBg, L"SidebarBg", L"Sidebar background", offsetof(AppTheme, sidebarBg)},
    {ThemeField::SidebarAccent, L"SidebarAccent", L"Sidebar sweep", offsetof(AppTheme, sidebarAccent)},
    {ThemeField::BrandPrimary, L"BrandPrimary", L"Brand colour", offsetof(AppTheme, brandPrimary)},
    {ThemeField::BrandSecondary, L"BrandSecondary", L"Brand shade", offsetof(AppTheme, brandSecondary)},
    {ThemeField::ContentBg, L"ContentBg", L"Content background", offsetof(AppTheme, contentBg)},
    {ThemeField::SectionHeaderBg, L"SectionHeaderBg", L"Section header / button face",
     offsetof(AppTheme, sectionHeaderBg)},
    {ThemeField::Border, L"Border", L"Borders", offsetof(AppTheme, border)},
    {ThemeField::TextPrimary, L"TextPrimary", L"Primary text", offsetof(AppTheme, textPrimary)},
    {ThemeField::TextMuted, L"TextMuted", L"Secondary text", offsetof(AppTheme, textMuted)},
    {ThemeField::OkGreen, L"OkGreen", L"Healthy indicator", offsetof(AppTheme, okGreen)},
    {ThemeField::WarnRed, L"WarnRed", L"Attention indicator", offsetof(AppTheme, warnRed)},
};

inline constexpr int kThemeFieldCount = kThemeFieldCapacity;

// Catches a ThemeField added to the enum but forgotten in the table: the entry
// would fall back to a null registry name instead of failing loudly at runtime.
constexpr bool AreThemeFieldsComplete() {
    for (int index = 0; index < kThemeFieldCapacity; ++index) {
        if (kThemeFields[index].key == nullptr || kThemeFields[index].label == nullptr) return false;
        if (kThemeFields[index].field != static_cast<ThemeField>(index)) return false;
    }
    return true;
}
static_assert(AreThemeFieldsComplete(), "kThemeFields must list every ThemeField, in enum order");

COLORREF& ThemeFieldRef(AppTheme& theme, ThemeField field);
COLORREF ThemeFieldColor(const AppTheme& theme, ThemeField field);

struct ThemePreset {
    const wchar_t* name;
    AppTheme theme;
};

extern const ThemePreset kThemePresets[];
extern const int kThemePresetCount;

// Preset index meaning "hand-edited, does not match any preset".
extern const int kCustomThemeIndex;

const AppTheme& PresetTheme(int index);
bool IsDarkColor(COLORREF color);
