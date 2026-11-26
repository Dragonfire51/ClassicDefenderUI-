// Console harness for the theme/settings layer. It runs on Windows (or Wine):
//   theme_test.exe
// Exits non-zero when a check fails. The registry values it writes live under
// HKCU\Software\ClassicDefenderUI and are restored to the default preset.

#include <windows.h>

#include <cstdio>

#include "AppTheme.h"
#include "Settings.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* label) {
    std::printf("%s  %s\n", condition ? "PASS" : "FAIL", label);
    if (!condition) ++g_failures;
}

}  // namespace

int main() {
    Check(kThemeFieldCount == static_cast<int>(ThemeField::Count), "field table describes every ThemeField");

    bool uniqueOffsets = true;
    bool uniqueKeys = true;
    for (int outer = 0; outer < kThemeFieldCount; ++outer) {
        for (int inner = outer + 1; inner < kThemeFieldCount; ++inner) {
            if (kThemeFields[outer].offset == kThemeFields[inner].offset) uniqueOffsets = false;
            if (wcscmp(kThemeFields[outer].key, kThemeFields[inner].key) == 0) uniqueKeys = false;
        }
    }
    Check(uniqueOffsets, "every field maps to its own struct member");
    Check(uniqueKeys, "every field has its own registry value name");
    Check(kThemePresetCount >= 3, "at least three presets ship");
    Check(ThemeFieldColor(kThemePresets[0].theme, ThemeField::WarnRed) == RGB(210, 35, 35),
          "colour lookup resolves the right offset");
    Check(ThemeFieldColor(kThemePresets[0].theme, ThemeField::ContentBg) == RGB(255, 255, 255),
          "colour lookup is field specific");
    Check(IsDarkColor(RGB(10, 10, 10)) && !IsDarkColor(RGB(240, 240, 240)), "dark colour detection");
    Check(PresetTheme(999).topBarBg == kThemePresets[0].theme.topBarBg, "out of range preset clamps to the first");
    Check(PresetTheme(-5).brandPrimary == kThemePresets[0].theme.brandPrimary, "negative preset clamps to the first");

    AppSettings original = AppSettings{};
    original.theme = kThemePresets[1].theme;
    original.presetIndex = 1;
    original.customColors[3] = RGB(1, 2, 3);
    original.customColors[15] = RGB(250, 251, 252);
    original.lastTab = 2;
    Check(settings::Save(original), "settings saved");

    AppSettings loaded;
    Check(settings::Load(loaded), "settings loaded");
    Check(loaded.presetIndex == 1, "preset index roundtrip");
    Check(loaded.lastTab == 2, "last tab roundtrip");
    Check(loaded.customColors[3] == RGB(1, 2, 3) && loaded.customColors[15] == RGB(250, 251, 252),
          "custom colour palette roundtrip");

    bool everyColour = true;
    for (int index = 0; index < kThemeFieldCount; ++index) {
        const ThemeField field = kThemeFields[index].field;
        if (ThemeFieldColor(loaded.theme, field) != ThemeFieldColor(original.theme, field)) everyColour = false;
    }
    Check(everyColour, "every theme colour roundtrips through the registry");

    AppSettings tweak;
    settings::Load(tweak);
    const COLORREF untouchedTopBar = ThemeFieldColor(tweak.theme, ThemeField::TopBarBg);
    ThemeFieldRef(tweak.theme, ThemeField::BrandPrimary) = RGB(9, 9, 9);
    tweak.presetIndex = kCustomThemeIndex;
    Check(settings::Save(tweak), "hand edited theme saved");

    AppSettings reloaded;
    settings::Load(reloaded);
    Check(ThemeFieldColor(reloaded.theme, ThemeField::BrandPrimary) == RGB(9, 9, 9),
          "single colour override survives a reload");
    Check(ThemeFieldColor(reloaded.theme, ThemeField::TopBarBg) == untouchedTopBar,
          "untouched colours keep their stored value");
    Check(reloaded.presetIndex == kCustomThemeIndex, "custom preset index roundtrip");

    AppSettings defaults;
    defaults.theme = PresetTheme(0);
    Check(settings::Save(defaults), "defaults restored");

    std::printf("%d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
