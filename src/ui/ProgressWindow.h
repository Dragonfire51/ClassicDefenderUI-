#pragma once

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "../AppTheme.h"
#include "../Progress.h"

namespace ui {

// Modeless, themed window that follows a ProgressTracker. One layout serves the
// scan and the definition update: both show a striped classic bar plus a list of
// label/value rows, only the rows differ.
class ProgressWindow {
public:
    ProgressWindow() = default;
    ~ProgressWindow();
    ProgressWindow(const ProgressWindow&) = delete;
    ProgressWindow& operator=(const ProgressWindow&) = delete;

    bool Create(HWND owner, HINSTANCE instance, std::shared_ptr<ProgressTracker> tracker, const AppTheme& theme);
    HWND Handle() const { return hwnd_; }
    void ApplyTheme(const AppTheme& theme);

private:
    static LRESULT CALLBACK WndProcStatic(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(UINT message, WPARAM wParam, LPARAM lParam);

    bool OnCreate();
    void OnPaint(HDC target);
    void Paint(HDC dc);
    void OnDrawItem(const DRAWITEMSTRUCT& item);
    void SyncButton();

    struct Stat {
        std::wstring label;
        std::wstring value;
    };
    std::vector<Stat> BuildStats(const ProgressSnapshot& snapshot) const;
    std::wstring Elapsed() const;

    HWND hwnd_ = nullptr;
    HWND owner_ = nullptr;
    HINSTANCE instance_ = nullptr;
    HWND closeButton_ = nullptr;
    std::shared_ptr<ProgressTracker> tracker_;
    AppTheme theme_{};
    HFONT fontTitle_ = nullptr;
    HFONT fontBold_ = nullptr;
    HFONT fontNormal_ = nullptr;
    HFONT fontSmall_ = nullptr;
    HFONT fontBrand_ = nullptr;
    ULONGLONG startTick_ = 0;
    int marqueePhase_ = 0;
    unsigned long long lastHash_ = 0;
};

}  // namespace ui
