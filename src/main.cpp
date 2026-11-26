#include <windows.h>

#include <commctrl.h>
#include <objbase.h>

#include "Resource.h"
#include "Settings.h"
#include "ui/MainWindow.h"

#ifdef _MSC_VER
// Common Controls v6 gives the threat list a modern list view; the rest of the
// window is drawn with plain GDI.
#pragma comment(linker,                                                                                        \
                "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' " \
                "processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

namespace {

void EnableDpiAwareness() {
    // DPI_AWARENESS_CONTEXT_SYSTEM_AWARE (-2); resolved at runtime so the binary
    // still starts on Windows versions without SetProcessDpiAwarenessContext.
    HANDLE systemAware = reinterpret_cast<HANDLE>(static_cast<INT_PTR>(-2));

    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        using SetContextFn = BOOL(WINAPI*)(HANDLE);
        auto setContext = reinterpret_cast<SetContextFn>(
            reinterpret_cast<void*>(GetProcAddress(user32, "SetProcessDpiAwarenessContext")));
        if (setContext && setContext(systemAware)) return;
    }
    SetProcessDPIAware();
}

// --layout WxH resizes the window once, so the reflowing layout can be captured
// at any size without a window manager (used by the docs screenshots).
bool HasLayoutFlag(int& width, int& height) {
    const wchar_t* commandLine = GetCommandLineW();
    if (commandLine == nullptr) return false;
    const wchar_t* flag = wcsstr(commandLine, L"--layout");
    if (flag == nullptr) return false;
    int parsedWidth = 0;
    int parsedHeight = 0;
    if (swscanf_s(flag + 8, L" %dx%d", &parsedWidth, &parsedHeight) != 2) return false;
    if (parsedWidth < 320 || parsedHeight < 200) return false;
    width = parsedWidth;
    height = parsedHeight;
    return true;
}

bool HasDemoFlag() {
    const wchar_t* commandLine = GetCommandLineW();
    return commandLine != nullptr && wcsstr(commandLine, L"--demo") != nullptr;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    EnableDpiAwareness();

    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 1;

    // Called from the UI thread, before any WMI proxy exists, so the workers'
    // later attempt simply reports RPC_E_TOO_LATE.
    CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr,
                         EOAC_NONE, nullptr);

    INITCOMMONCONTROLSEX commonControls = {};
    commonControls.dwSize = sizeof(commonControls);
    commonControls.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&commonControls);

    AppSettings settings;
    settings::Load(settings);

    int layoutWidth = 0;
    int layoutHeight = 0;
    const bool hasLayout = HasLayoutFlag(layoutWidth, layoutHeight);

    MainWindow window;
    if (!window.Create(instance, settings, HasDemoFlag())) {
        CoUninitialize();
        return 1;
    }

    ShowWindow(window.Handle(), showCommand);
    if (hasLayout)
        SetWindowPos(window.Handle(), nullptr, 0, 0, layoutWidth, layoutHeight, SWP_NOMOVE | SWP_NOZORDER);
    UpdateWindow(window.Handle());

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    CoUninitialize();
    return static_cast<int>(message.wParam);
}
