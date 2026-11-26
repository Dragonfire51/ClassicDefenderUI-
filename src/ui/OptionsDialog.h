#pragma once

#include <windows.h>

#include "../DefenderWmi.h"
#include "../Settings.h"

class OptionsDialog {
public:
    // Modal. The dialog edits a copy of the settings, previews every colour
    // change on the owner window and persists on OK/Apply.
    static void Show(HWND owner, HINSTANCE instance, const AppSettings& settings, const DefenderStatus& status);
};
