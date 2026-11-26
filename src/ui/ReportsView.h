#pragma once

#include <windows.h>

#include <vector>

#include "../AppTheme.h"
#include "../DefenderWmi.h"

// SysListView32 showing MSFT_MpThreatDetection rows with themed custom draw.
class ReportsView {
public:
    void Create(HWND parent, HINSTANCE instance);
    void Destroy();

    HWND Handle() const { return list_; }
    HWND RefreshButton() const { return refresh_; }

    void Layout(const RECT& deviceRect);
    void SetThreats(const std::vector<ThreatRecord>& threats);
    void ApplyTheme(const AppTheme& theme);

    // Handles NM_CUSTOMDRAW coming from the list view and returns the CDRF_* code.
    LRESULT OnCustomDraw(NMHDR* header);

private:
    static LRESULT CALLBACK HeaderProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    HWND list_ = nullptr;
    HWND refresh_ = nullptr;
    HWND header_ = nullptr;
    WNDPROC headerProc_ = nullptr;
    AppTheme theme_{};
    std::vector<ThreatRecord> threats_;
};
