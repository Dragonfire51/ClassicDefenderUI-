#include "AppTheme.h"

COLORREF& ThemeFieldRef(AppTheme& theme, ThemeField field) {
    return *reinterpret_cast<COLORREF*>(reinterpret_cast<unsigned char*>(&theme) + kThemeFields[static_cast<int>(field)].offset);
}

COLORREF ThemeFieldColor(const AppTheme& theme, ThemeField field) {
    return *reinterpret_cast<const COLORREF*>(reinterpret_cast<const unsigned char*>(&theme) +
                                              kThemeFields[static_cast<int>(field)].offset);
}

const ThemePreset kThemePresets[] = {
    {L"Classic 2002 (yellow / red)",
     {/*topBarBg*/ RGB(236, 233, 216), /*sidebarBg*/ RGB(245, 242, 232), /*sidebarAccent*/ RGB(255, 204, 0),
      /*brandPrimary*/ RGB(180, 0, 0), /*brandSecondary*/ RGB(50, 50, 50), /*contentBg*/ RGB(255, 255, 255),
      /*sectionHeaderBg*/ RGB(230, 230, 230), /*border*/ RGB(160, 160, 160), /*textPrimary*/ RGB(20, 20, 20),
      /*textMuted*/ RGB(90, 90, 90), /*okGreen*/ RGB(115, 185, 50), /*warnRed*/ RGB(210, 35, 35)}},

    {L"Luna blue (Windows XP)",
     {RGB(225, 234, 248), RGB(235, 242, 252), RGB(0, 102, 204), RGB(0, 78, 152), RGB(0, 120, 215), RGB(255, 255, 255),
      RGB(220, 230, 242), RGB(140, 165, 195), RGB(15, 30, 50), RGB(80, 95, 115), RGB(40, 167, 69),
      RGB(220, 53, 69)}},

    {L"Retro slate (dark)",
     {RGB(45, 48, 52), RGB(38, 41, 45), RGB(0, 180, 120), RGB(0, 210, 140), RGB(220, 220, 220), RGB(28, 30, 34),
      RGB(50, 54, 60), RGB(80, 85, 92), RGB(235, 235, 235), RGB(160, 165, 170), RGB(0, 200, 110), RGB(235, 75, 75)}},
};
const int kThemePresetCount = static_cast<int>(sizeof(kThemePresets) / sizeof(kThemePresets[0]));

const int kCustomThemeIndex = kThemePresetCount;

const AppTheme& PresetTheme(int index) {
    if (index < 0 || index >= kThemePresetCount) index = 0;
    return kThemePresets[index].theme;
}

bool IsDarkColor(COLORREF color) {
    const int luminance = (GetRValue(color) * 299 + GetGValue(color) * 587 + GetBValue(color) * 114) / 1000;
    return luminance < 128;
}
