#pragma once

#include <windows.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../AppTheme.h"
#include "../DefenderWmi.h"
#include "../Settings.h"

class ReportsView;
class ProgressTracker;

namespace ui {
class ProgressWindow;
}

struct StatusResult {
    DefenderStatus status;
    std::wstring error;
};

struct ThreatsResult {
    std::vector<ThreatRecord> threats;
    std::wstring error;
};

struct OperationResult {
    std::wstring message;
    bool success = false;
};

enum class Page { Status = 0, Scan = 1, Reports = 2, Quarantine = 3 };

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool Create(HINSTANCE instance, const AppSettings& settings, bool demoMode);
    HWND Handle() const { return hwnd_; }

    void ShowThemePreview(const AppTheme& theme);
    void EndThemePreview();
    void ReloadSettings();

private:
    static LRESULT CALLBACK WndProcStatic(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(UINT message, WPARAM wParam, LPARAM lParam);

    struct StatusRow {
        std::wstring label;
        std::wstring value;
        std::wstring details;
        bool ok = true;
    };

    const AppTheme& Theme() const;
    std::vector<StatusRow> BuildRows() const;

    bool OnCreate();
    void OnPaint();
    void CreateControls();
    void LayoutControls();
    void ApplyThemeToChildren();
    void ShowPage(Page page);
    void SetBusy(bool busy, const std::wstring& pending);

    void UpdateMetrics();

    void OnCommand(int id);
    void OnDrawItem(const DRAWITEMSTRUCT& item);
    LRESULT OnNotify(const NMHDR& header);
    void OnLeftButtonDown(int x, int y);

    void PaintQuarantinePage(HDC dc);
    void SetQuarantineFromThreats();
    void QuarantineAction(int command);

    void OpenOptions();
    void ShowAbout();
    void StartCustomScan();
    void StartScanOperation(long scanType, const std::wstring& path);
    void StartUpdateOperation();
    void StartOperation(const std::wstring& pending, std::function<OperationResult(DefenderClient&)> work);
    void RefreshStatus();
    void RefreshThreats();

    void Paint(HDC dc);
    void PaintHeader(HDC dc);
    void PaintSidebar(HDC dc);
    void PaintSweep(HDC dc);
    void PaintFooter(HDC dc);
    void PaintStatusPage(HDC dc);
    void PaintScanPage(HDC dc);
    void PaintReportsPage(HDC dc);
    void PaintDetails(HDC dc, const RECT& rc);

    RECT RowRect(int index) const;
    RECT PanelRect() const;

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    bool demo_ = false;

    AppSettings settings_;
    AppTheme preview_{};
    bool previewing_ = false;

    DefenderStatus status_;
    std::wstring statusError_;
    std::vector<ThreatRecord> threats_;
    std::wstring threatsError_;

    int clientWidth_ = 0;
    int clientHeight_ = 0;
    int panelRight_ = 0;
    int panelBottom_ = 0;
    int pageRight_ = 0;
    int group2Bottom_ = 0;
    int detailsLeft_ = 0;

    std::shared_ptr<ProgressTracker> tracker_;
    std::unique_ptr<ui::ProgressWindow> progress_;

    Page page_ = Page::Status;
    int selectedRow_ = 0;
    std::wstring operationText_;
    bool operationFailed_ = false;
    bool busy_ = false;

    HFONT fontNormal_ = nullptr;
    HFONT fontBold_ = nullptr;
    HFONT fontTitle_ = nullptr;
    HFONT fontBrand_ = nullptr;
    HFONT fontSmall_ = nullptr;

    std::vector<ThreatRecord> quarantineRecords_;
    std::unique_ptr<ReportsView> reports_;
    std::unique_ptr<ReportsView> quarantine_;
    HWND navButtons_[4] = {nullptr, nullptr, nullptr, nullptr};
    HWND toolButtons_[3] = {nullptr, nullptr, nullptr};
    HWND scanButtons_[3] = {nullptr, nullptr, nullptr};
    HWND quarantineButtons_[4] = {nullptr, nullptr, nullptr, nullptr};
};
