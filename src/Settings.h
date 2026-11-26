#pragma once

#include "AppTheme.h"

#include <array>

// Everything the UI remembers between runs. Defender keeps its own settings
// (exclusions, protection toggles); this struct only holds skin and view state.
struct AppSettings {
    AppTheme theme;
    int presetIndex = 0;
    std::array<COLORREF, 16> customColors{};
    int lastTab = 0;
};

namespace settings {

// Both functions are safe to call with a root other than HKEY_CURRENT_USER,
// which is what the console test harness does.
bool Load(AppSettings& out, HKEY root = HKEY_CURRENT_USER);
bool Save(const AppSettings& in, HKEY root = HKEY_CURRENT_USER);

}  // namespace settings
