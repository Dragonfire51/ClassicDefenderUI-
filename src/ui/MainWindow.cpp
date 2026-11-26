#include "MainWindow.h"

#include <commctrl.h>
#include <objbase.h>
#include <shellapi.h>
#include <windowsx.h>

#include <algorithm>
#include <memory>

#include "../Async.h"
#include "../Resource.h"
#include "Version.h"
#include "FolderPicker.h"
#include "OptionsDialog.h"
#include "Paint.h"
#include "ReportsView.h"

namespace {

// The 2002 layout is designed on a fixed 660x452 client area. Painting goes
// through an anisotropic mapping, child controls use ui::Scale().
constexpr int kClientWidth = 660;
constexpr int kClientHeight = 452;
constexpr int kHeaderHeight = 58;
constexpr int kSidebarWidth = 178;
constexpr int kPanelLeft = 190;
constexpr int kPanelTop = 64;
constexpr int kPanelRight = 648;
constexpr int kPanelBottom = 400;
constexpr int kPageLeft = kPanelLeft + 16;
constexpr int kPageRight = kPanelRight - 12;
constexpr int kTitleTop = 74;
constexpr int kRowHeight = 27;
constexpr int kGroup1Top = 112;
constexpr int kGroup1Bottom = 243;
constexpr int kGroup2Top = 254;
constexpr int kGroup2Bottom = 360;
constexpr int kDetailsLeft = 506;
constexpr int kNavTop = 108;
constexpr int kNavHeight = 28;
constexpr int kNavStep = 32;

RECT MakeRect(int left, int top, int right, int bottom) {
    RECT rc = {left, top, right, bottom};
    return rc;
}

std::wstring OnOff(bool value) {
    return value ? L"On" : L"Off";
}

std::wstring WindowText(HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(control, text.data(), length + 1);
    text.resize(static_cast<size_t>(length));
    return text;
}

DefenderStatus DemoStatus() {
    DefenderStatus status;
    status.wmiConnected = true;
    status.antivirusEnabled = true;
    status.realTimeProtection = true;
    status.behaviorMonitoring = true;
    status.cloudProtection = true;
    status.signatureVersion = L"1.403.2195.0";
    status.engineVersion = L"1.1.24080.9";
    status.serviceVersion = L"4.18.24080.9";
    status.lastQuickScan = L"02/12/2003";
    status.lastFullScan = L"02/12/2003";
    status.lastSignatureUpdate = L"26/11/2003";
    return status;
}

std::vector<ThreatRecord> DemoThreats() {
    ThreatRecord trojan;
    trojan.threatId = 2147723442;
    trojan.name = L"Trojan:Win32/Emotet";
    trojan.severityId = 5;
    trojan.statusId = 3;
    trojan.detected = L"24/11/2003";
    trojan.resources = L"file:_C:\\Users\\Public\\update.exe";
    trojan.actionSucceeded = true;

    ThreatRecord pup;
    pup.threatId = 2147723001;
    pup.name = L"PUA:Win32/Bundler";
    pup.severityId = 2;
    pup.statusId = 4;
    pup.detected = L"19/11/2003";
    pup.resources = L"D:\\Downloads\\setup_toolbar.exe";
    pup.actionSucceeded = true;

    return {trojan, pup};
}

void DrawNavItem(HDC dc, const RECT& rc, const AppTheme& theme, HFONT normal, HFONT bold, const std::wstring& text,
                 bool selected) {
    ui::FillSolid(dc, rc, theme.sidebarBg);

    if (selected) {
        RECT box = MakeRect(rc.left + 2, rc.top + 1, rc.right - 14, rc.bottom - 1);
        ui::FillSolid(dc, box, theme.contentBg);
        ui::StrokeRect(dc, box, theme.border);

        const int midY = (box.top + box.bottom) / 2;
        POINT triangle[3] = {{box.right + 10, midY - 6}, {box.right + 10, midY + 6}, {box.right + 2, midY}};
        HBRUSH brush = CreateSolidBrush(theme.brandPrimary);
        HPEN pen = CreatePen(PS_SOLID, 1, theme.brandPrimary);
        HGDIOBJ oldBrush = SelectObject(dc, brush);
        HGDIOBJ oldPen = SelectObject(dc, pen);
        Polygon(dc, triangle, 3);
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(brush);
        DeleteObject(pen);
    }

    RECT textRect = MakeRect(rc.left + 8, rc.top, rc.right - 16, rc.bottom);
    ui::DrawTextIn(dc, textRect, text, selected ? bold : normal,
                   selected ? theme.brandPrimary : theme.textPrimary,
                   DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

ui::ToolGlyph GlyphForCommand(int id) {
    switch (id) {
        case IDC_BTN_OPTIONS:
            return ui::ToolGlyph::Options;
        case IDC_BTN_HELP:
            return ui::ToolGlyph::Help;
        default:
            return ui::ToolGlyph::LiveUpdate;
    }
}

}  // namespace

MainWindow::MainWindow() = default;
MainWindow::~MainWindow() = default;

bool MainWindow::Create(HINSTANCE instance, const AppSettings& settings, bool demoMode) {
    instance_ = instance;
    settings_ = settings;
    demo_ = demoMode;

    HDC screen = GetDC(nullptr);
    ui::SetDpi(screen ? GetDeviceCaps(screen, LOGPIXELSX) : 96);
    if (screen) ReleaseDC(nullptr, screen);

    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = &MainWindow::WndProcStatic;
    windowClass.hInstance = instance;
    windowClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APPICON));
    windowClass.hIconSm = windowClass.hIcon;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = L"ClassicDefenderWindow";
    if (!RegisterClassExW(&windowClass)) return false;

    if (demo_) {
        status_ = DemoStatus();
        threats_ = DemoThreats();
    }

    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    RECT frame = {0, 0, ui::Scale(kClientWidth), ui::Scale(kClientHeight)};
    AdjustWindowRect(&frame, style, FALSE);
    const int width = frame.right - frame.left;
    const int height = frame.bottom - frame.top;

    RECT workArea = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const int x = workArea.left + ((workArea.right - workArea.left) - width) / 2;
    const int y = workArea.top + ((workArea.bottom - workArea.top) - height) / 2;

    hwnd_ = CreateWindowExW(0, windowClass.lpszClassName, L"Windows Defender AntiVirus 2002 Edition", style, x, y,
                            width, height, nullptr, nullptr, instance, this);
    return hwnd_ != nullptr;
}

LRESULT CALLBACK MainWindow::WndProcStatic(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    MainWindow* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<MainWindow*>(create->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    if (!self) return DefWindowProcW(hwnd, message, wParam, lParam);
    return self->WndProc(message, wParam, lParam);
}

const AppTheme& MainWindow::Theme() const {
    return previewing_ ? preview_ : settings_.theme;
}

RECT MainWindow::PanelRect() const {
    return MakeRect(kPanelLeft, kPanelTop, kPanelRight, kPanelBottom);
}

RECT MainWindow::RowRect(int index) const {
    const int column = index < 4 ? index : index - 4;
    const int top = (index < 4 ? kGroup1Top : kGroup2Top) + 21;
    const int left = kPageLeft + 1;
    const int right = kDetailsLeft - 16;
    return MakeRect(left, top + column * kRowHeight, right, top + (column + 1) * kRowHeight);
}

LRESULT MainWindow::WndProc(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
            return OnCreate() ? 0 : -1;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
            OnPaint();
            return 0;

        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED || HIWORD(wParam) == 0) OnCommand(LOWORD(wParam));
            return 0;

        case WM_DRAWITEM:
            OnDrawItem(*reinterpret_cast<DRAWITEMSTRUCT*>(lParam));
            return TRUE;

        case WM_NOTIFY:
            return OnNotify(*reinterpret_cast<NMHDR*>(lParam));

        case WM_LBUTTONDOWN: {
            RECT client = {};
            GetClientRect(hwnd_, &client);
            const int logicalX = MulDiv(GET_X_LPARAM(lParam), kClientWidth, std::max(1L, client.right));
            const int logicalY = MulDiv(GET_Y_LPARAM(lParam), kClientHeight, std::max(1L, client.bottom));
            OnLeftButtonDown(logicalX, logicalY);
            return 0;
        }

        case WM_TIMER:
            if (!demo_ && !busy_) {
                RefreshStatus();
                if (page_ == Page::Reports) RefreshThreats();
            }
            return 0;

        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            RECT frame = {0, 0, ui::Scale(kClientWidth), ui::Scale(kClientHeight)};
            AdjustWindowRect(&frame, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
            info->ptMinTrackSize.x = frame.right - frame.left;
            info->ptMinTrackSize.y = frame.bottom - frame.top;
            info->ptMaxTrackSize = info->ptMinTrackSize;
            return 0;
        }

        case WM_APP_STATUS_READY: {
            std::unique_ptr<StatusResult> result(reinterpret_cast<StatusResult*>(lParam));
            status_ = result->status;
            statusError_ = result->error;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }

        case WM_APP_THREATS_READY: {
            std::unique_ptr<ThreatsResult> result(reinterpret_cast<ThreatsResult*>(lParam));
            threats_ = std::move(result->threats);
            threatsError_ = result->error;
            if (reports_) reports_->SetThreats(threats_);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }

        case WM_APP_OPERATION_DONE: {
            std::unique_ptr<OperationResult> result(reinterpret_cast<OperationResult*>(lParam));
            SetBusy(false, std::wstring());
            operationText_ = result->message;
            operationFailed_ = !result->success;
            RefreshStatus();
            if (page_ == Page::Reports) RefreshThreats();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }

        case WM_APP_THEME_PREVIEW: {
            std::unique_ptr<AppTheme> theme(reinterpret_cast<AppTheme*>(lParam));
            ShowThemePreview(*theme);
            return 0;
        }

        case WM_APP_PREVIEW_END:
            EndThemePreview();
            return 0;

        case WM_APP_SETTINGS_CHANGED:
            ReloadSettings();
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd_, 1);
            settings::Save(settings_);
            if (reports_) reports_->Destroy();
            DeleteObject(fontNormal_);
            DeleteObject(fontBold_);
            DeleteObject(fontTitle_);
            DeleteObject(fontBrand_);
            DeleteObject(fontSmall_);
            PostQuitMessage(0);
            return 0;

        default:
            break;
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

bool MainWindow::OnCreate() {
    fontNormal_ = ui::CreateUiFont(-11, FW_NORMAL, false, L"Tahoma");
    fontBold_ = ui::CreateUiFont(-11, FW_BOLD, false, L"Tahoma");
    fontTitle_ = ui::CreateUiFont(-21, FW_BOLD, false, L"Arial");
    fontBrand_ = ui::CreateUiFont(-19, FW_BOLD, true, L"Arial");
    fontSmall_ = ui::CreateUiFont(-10, FW_NORMAL, false, L"Tahoma");

    CreateControls();
    LayoutControls();
    ApplyThemeToChildren();

    SetTimer(hwnd_, 1, 30000, nullptr);

    ShowPage(static_cast<Page>(std::clamp(settings_.lastTab, 0, 2)));
    if (demo_) {
        if (reports_) reports_->SetThreats(threats_);
    } else {
        RefreshStatus();
    }
    return true;
}

void MainWindow::CreateControls() {
    const DWORD navStyle = WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP;
    const auto id = [](int value) { return reinterpret_cast<HMENU>(static_cast<INT_PTR>(value)); };

    const wchar_t* navLabels[3] = {L"Status", L"Scan for Viruses", L"Reports"};
    const int navIds[3] = {IDC_NAV_STATUS, IDC_NAV_SCAN, IDC_NAV_REPORTS};
    for (int index = 0; index < 3; ++index)
        navButtons_[index] = CreateWindowExW(0, L"BUTTON", navLabels[index], navStyle, 0, 0, 0, 0, hwnd_,
                                             id(navIds[index]), instance_, nullptr);

    const wchar_t* toolLabels[3] = {L"LiveUpdate", L"Options", L"Help"};
    const int toolIds[3] = {IDC_BTN_LIVEUPDATE, IDC_BTN_OPTIONS, IDC_BTN_HELP};
    for (int index = 0; index < 3; ++index)
        toolButtons_[index] = CreateWindowExW(0, L"BUTTON", toolLabels[index], navStyle, 0, 0, 0, 0, hwnd_,
                                              id(toolIds[index]), instance_, nullptr);

    const wchar_t* scanLabels[3] = {L"Run a quick scan", L"Run a full scan", L"Scan a folder..."};
    const int scanIds[3] = {IDC_SCAN_QUICK, IDC_SCAN_FULL, IDC_SCAN_CUSTOM};
    for (int index = 0; index < 3; ++index)
        scanButtons_[index] = CreateWindowExW(0, L"BUTTON", scanLabels[index], navStyle, 0, 0, 0, 0, hwnd_,
                                              id(scanIds[index]), instance_, nullptr);

    reports_ = std::make_unique<ReportsView>();
    reports_->Create(hwnd_, instance_);
}

void MainWindow::LayoutControls() {
    const auto place = [](HWND control, int x, int y, int width, int height) {
        if (control) MoveWindow(control, ui::Scale(x), ui::Scale(y), ui::Scale(width), ui::Scale(height), TRUE);
    };

    for (int index = 0; index < 3; ++index) place(navButtons_[index], 8, kNavTop + index * kNavStep, 166, kNavHeight);

    place(toolButtons_[0], 250, 6, 86, 46);
    place(toolButtons_[1], 342, 6, 80, 46);
    place(toolButtons_[2], 552, 6, 92, 46);

    place(scanButtons_[0], kPageLeft, 132, 260, 34);
    place(scanButtons_[1], kPageLeft, 174, 260, 34);
    place(scanButtons_[2], kPageLeft, 216, 260, 34);

    if (reports_) {
        RECT list = MakeRect(ui::Scale(kPageLeft), ui::Scale(124), ui::Scale(kPageRight), ui::Scale(348));
        reports_->Layout(list);
        place(reports_->RefreshButton(), kPageRight - 80, 70, 80, 24);
    }
}

void MainWindow::ApplyThemeToChildren() {
    const AppTheme& theme = Theme();
    if (reports_) reports_->ApplyTheme(theme);

    for (HWND button : navButtons_)
        if (button) InvalidateRect(button, nullptr, TRUE);
    for (HWND button : toolButtons_)
        if (button) InvalidateRect(button, nullptr, TRUE);
    for (HWND button : scanButtons_)
        if (button && IsWindowVisible(button)) InvalidateRect(button, nullptr, TRUE);

    RedrawWindow(hwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void MainWindow::ShowThemePreview(const AppTheme& theme) {
    preview_ = theme;
    previewing_ = true;
    ApplyThemeToChildren();
}

void MainWindow::EndThemePreview() {
    previewing_ = false;
    ApplyThemeToChildren();
}

void MainWindow::ReloadSettings() {
    settings::Load(settings_);
    previewing_ = false;
    ApplyThemeToChildren();
}

void MainWindow::ShowPage(Page page) {
    page_ = page;
    settings_.lastTab = static_cast<int>(page);

    const BOOL scanning = page == Page::Scan;
    const BOOL reporting = page == Page::Reports;
    for (HWND button : scanButtons_) ShowWindow(button, scanning ? SW_SHOW : SW_HIDE);
    if (reports_) {
        ShowWindow(reports_->Handle(), reporting ? SW_SHOW : SW_HIDE);
        ShowWindow(reports_->RefreshButton(), reporting ? SW_SHOW : SW_HIDE);
    }

    if (reporting && threats_.empty() && threatsError_.empty() && !demo_) RefreshThreats();

    // Owner-draw navigation depends on the active page, so it must repaint too.
    for (HWND button : navButtons_)
        if (button) InvalidateRect(button, nullptr, TRUE);

    SetFocus(hwnd_);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void MainWindow::OnCommand(int id) {
    switch (id) {
        case IDC_NAV_STATUS:
            ShowPage(Page::Status);
            break;
        case IDC_NAV_SCAN:
            ShowPage(Page::Scan);
            break;
        case IDC_NAV_REPORTS:
            ShowPage(Page::Reports);
            break;
        case IDC_BTN_LIVEUPDATE:
            StartUpdateOperation();
            break;
        case IDC_BTN_OPTIONS:
            OpenOptions();
            break;
        case IDC_BTN_HELP:
            ShowAbout();
            break;
        case IDC_SCAN_QUICK:
            StartScanOperation(1, std::wstring());
            break;
        case IDC_SCAN_FULL:
            StartScanOperation(2, std::wstring());
            break;
        case IDC_SCAN_CUSTOM:
            StartCustomScan();
            break;
        case IDC_REPORTS_REFRESH:
            RefreshThreats();
            break;
        default:
            break;
    }
}

void MainWindow::OnDrawItem(const DRAWITEMSTRUCT& item) {
    if (item.hDC == nullptr) return;

    const int id = static_cast<int>(item.CtlID);
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool focused = (item.itemState & ODS_FOCUS) != 0;
    const std::wstring text = WindowText(item.hwndItem);
    const AppTheme& theme = Theme();

    if (id >= IDC_NAV_STATUS && id <= IDC_NAV_REPORTS) {
        DrawNavItem(item.hDC, item.rcItem, theme, fontNormal_, fontBold_, text,
                    static_cast<int>(id - IDC_NAV_STATUS) == static_cast<int>(page_));
        return;
    }

    if (id == IDC_BTN_LIVEUPDATE || id == IDC_BTN_OPTIONS || id == IDC_BTN_HELP) {
        ui::DrawToolbarButton(item.hDC, item.rcItem, GlyphForCommand(id), text, theme, fontSmall_, pressed);
        return;
    }

    ui::DrawClassicButton(item.hDC, item.rcItem, text, theme, fontNormal_, pressed, focused);
}

LRESULT MainWindow::OnNotify(const NMHDR& header) {
    if (reports_ && header.hwndFrom == reports_->Handle() && header.code == NM_CUSTOMDRAW)
        return reports_->OnCustomDraw(const_cast<NMHDR*>(&header));
    return 0;
}

void MainWindow::OnLeftButtonDown(int x, int y) {
    if (page_ != Page::Status) return;
    const std::vector<StatusRow> rows = BuildRows();
    for (int index = 0; index < static_cast<int>(rows.size()); ++index) {
        const RECT rc = RowRect(index);
        if (x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom) {
            selectedRow_ = index;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
    }
}

void MainWindow::SetBusy(bool busy, const std::wstring& pending) {
    busy_ = busy;
    if (busy) {
        operationText_ = pending;
        operationFailed_ = false;
    }
    const BOOL enabled = busy ? FALSE : TRUE;
    for (HWND button : toolButtons_) EnableWindow(button, enabled);
    for (HWND button : scanButtons_) EnableWindow(button, enabled);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::StartOperation(const std::wstring& pending, std::function<OperationResult(DefenderClient&)> work) {
    if (busy_) return;
    SetBusy(true, pending);

    const HWND target = hwnd_;
    std::thread([target, work = std::move(work)]() {
        ComApartment apartment;
        DefenderClient client;
        OperationResult* result = new OperationResult(work(client));
        if (result->message.empty()) result->message = result->success ? L"Done." : client.LastError();
        if (!PostMessageW(target, WM_APP_OPERATION_DONE, 0, reinterpret_cast<LPARAM>(result))) delete result;
    }).detach();
}

void MainWindow::StartUpdateOperation() {
    if (demo_) {
        StartOperation(L"Updating virus definitions...", [](DefenderClient&) {
            Sleep(700);
            return OperationResult{L"Virus definitions are up to date (demo mode).", true};
        });
        return;
    }

    StartOperation(L"Updating virus definitions...", [](DefenderClient& client) {
        if (client.UpdateSignatures())
            return OperationResult{L"Windows Defender accepted the signature update request.", true};
        return OperationResult{L"Update failed - " + client.LastError(), false};
    });
}

void MainWindow::StartScanOperation(long scanType, const std::wstring& path) {
    const std::wstring pending = scanType == 1   ? L"Starting a quick scan..."
                                 : scanType == 2 ? L"Starting a full scan..."
                                                 : L"Starting a custom scan...";
    if (demo_) {
        StartOperation(pending, [](DefenderClient&) {
            Sleep(900);
            return OperationResult{L"Scan finished - no threats found (demo mode).", true};
        });
        return;
    }

    StartOperation(pending, [scanType, path](DefenderClient& client) {
        if (client.StartScan(scanType, path)) return OperationResult{L"Scan started in the Defender service.", true};
        return OperationResult{L"Cannot start the scan - " + client.LastError(), false};
    });
}

void MainWindow::StartCustomScan() {
    std::wstring folder;
    if (!ui::PickFolder(hwnd_, folder)) return;
    StartScanOperation(3, folder);
}

void MainWindow::OpenOptions() {
    OptionsDialog::Show(hwnd_, instance_, settings_, status_);
}

void MainWindow::ShowAbout() {
    const std::wstring text =
        L"Windows Defender AntiVirus 2002 Edition\n"
        L"Classic Defender UI " APP_VERSION_WIDE L"\n\n"
        L"A classic Win32 front-end for the Microsoft Defender WMI provider "
        L"(ROOT\\Microsoft\\Windows\\Defender).\n\n"
        L"Status, scanning, signature updates, threat reports and folder exclusions are read from "
        L"and written to the real engine. Colour themes are stored per user under "
        L"HKEY_CURRENT_USER\\Software\\ClassicDefenderUI.\n\n"
        L"Run elevated: the provider rejects most write operations otherwise.";
    MessageBoxW(hwnd_, text.c_str(), L"About this program", MB_OK | MB_ICONINFORMATION);
}

void MainWindow::RefreshStatus() {
    if (demo_ || !hwnd_) return;
    PostAsync<StatusResult>(hwnd_, WM_APP_STATUS_READY, 0, [] {
        ComApartment apartment;
        DefenderClient client;
        StatusResult result;
        if (!client.QueryStatus(result.status)) result.error = client.LastError();
        return result;
    });
}

void MainWindow::RefreshThreats() {
    if (!hwnd_) return;
    if (demo_) {
        if (reports_) reports_->SetThreats(threats_);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }
    PostAsync<ThreatsResult>(hwnd_, WM_APP_THREATS_READY, 0, [] {
        ComApartment apartment;
        DefenderClient client;
        ThreatsResult result;
        if (!client.QueryThreats(result.threats)) result.error = client.LastError();
        return result;
    });
}

std::vector<MainWindow::StatusRow> MainWindow::BuildRows() const {
    const DefenderStatus& status = status_;
    const bool connected = status.wmiConnected;

    std::vector<StatusRow> rows;
    rows.push_back({L"Auto-Protect", OnOff(status.realTimeProtection),
                    L"Auto-Protect examines files and programs the moment they are opened. If it is off, turn "
                    L"it back on in Windows Security > Virus & threat protection.",
                    status.realTimeProtection});
    rows.push_back({L"Incoming Email", OnOff(status.cloudProtection),
                    L"Maps to the engine's IOAV protection flag: mail attachments and downloaded files are "
                    L"scanned before they reach the disk.",
                    status.cloudProtection});
    rows.push_back({L"Script Blocking", OnOff(status.behaviorMonitoring),
                    L"Behaviour monitoring stops suspicious actions instead of waiting for a signature match.",
                    status.behaviorMonitoring});
    rows.push_back({L"Full System Scan", status.lastFullScan,
                    L"Date of the last completed full scan. Use Scan for Viruses to start a new one.",
                    connected && status.lastFullScan != L"Never"});
    rows.push_back({L"Virus Definitions", status.lastSignatureUpdate,
                    L"Date the virus definitions were last updated. LiveUpdate asks Microsoft Update for the "
                    L"newest package.",
                    connected && status.lastSignatureUpdate != L"Unknown"});
    rows.push_back({L"Service Version", status.serviceVersion,
                    L"Platform service version reported by MSFT_MpComputerStatus.AMServiceVersion. Engine: " +
                        status.engineVersion,
                    connected});
    rows.push_back({L"Automatic LiveUpdate", L"On",
                    L"Defender refreshes its definitions on its own schedule; LiveUpdate forces an immediate "
                    L"check.",
                    connected});

    if (!connected) {
        for (StatusRow& row : rows) {
            row.value = L"Unknown";
            row.ok = false;
        }
    }
    return rows;
}

void MainWindow::OnPaint() {
    PAINTSTRUCT paint = {};
    HDC target = BeginPaint(hwnd_, &paint);
    if (!target) return;

    RECT client = {};
    GetClientRect(hwnd_, &client);
    const int width = client.right;
    const int height = client.bottom;

    if (width > 0 && height > 0) {
        HDC memory = CreateCompatibleDC(target);
        HBITMAP bitmap = CreateCompatibleBitmap(target, width, height);
        HGDIOBJ oldBitmap = SelectObject(memory, bitmap);

        SetMapMode(memory, MM_ANISOTROPIC);
        SetWindowExtEx(memory, kClientWidth, kClientHeight, nullptr);
        SetViewportExtEx(memory, width, height, nullptr);
        SetBkMode(memory, TRANSPARENT);

        Paint(memory);

        SetMapMode(memory, MM_TEXT);
        BitBlt(target, 0, 0, width, height, memory, 0, 0, SRCCOPY);

        SelectObject(memory, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(memory);
    }

    EndPaint(hwnd_, &paint);
}

void MainWindow::Paint(HDC dc) {
    const AppTheme& theme = Theme();

    ui::FillSolid(dc, MakeRect(0, 0, kClientWidth, kClientHeight), theme.contentBg);
    PaintSidebar(dc);
    PaintSweep(dc);
    PaintHeader(dc);
    ui::FillRounded(dc, PanelRect(), 12, theme.contentBg, theme.border);

    switch (page_) {
        case Page::Status:
            PaintStatusPage(dc);
            break;
        case Page::Scan:
            PaintScanPage(dc);
            break;
        case Page::Reports:
            PaintReportsPage(dc);
            break;
    }

    if (!operationText_.empty()) {
        const int top = page_ == Page::Status ? kGroup2Bottom + 6 : kPanelBottom - 34;
        RECT line = MakeRect(kPageLeft, top, kPageRight, top + 22);
        ui::DrawTextIn(dc, line, operationText_, fontBold_, operationFailed_ ? theme.warnRed : theme.brandPrimary,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    PaintFooter(dc);
}

void MainWindow::PaintHeader(HDC dc) {
    const AppTheme& theme = Theme();
    ui::FillSolid(dc, MakeRect(0, 0, kClientWidth, kHeaderHeight), theme.topBarBg);
    ui::FillSolid(dc, MakeRect(0, 0, kClientWidth, 2), theme.brandPrimary);
    ui::LineH(dc, 0, kClientWidth, kHeaderHeight - 2, ui::Blend(theme.topBarBg, RGB(255, 255, 255), 70));
    ui::LineH(dc, 0, kClientWidth, kHeaderHeight - 1, theme.border);

    const int logoLeft = 14;
    const int logoTop = 12;
    const int logoSize = 32;
    HBRUSH outer = CreateSolidBrush(theme.brandPrimary);
    HPEN rim = CreatePen(PS_SOLID, 1, ui::Blend(theme.brandPrimary, RGB(0, 0, 0), 45));
    HGDIOBJ oldBrush = SelectObject(dc, outer);
    HGDIOBJ oldPen = SelectObject(dc, rim);
    Ellipse(dc, logoLeft, logoTop, logoLeft + logoSize, logoTop + logoSize);
    HBRUSH inner = CreateSolidBrush(theme.sidebarAccent);
    SelectObject(dc, inner);
    Ellipse(dc, logoLeft + 7, logoTop + 3, logoLeft + logoSize - 3, logoTop + logoSize - 7);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(inner);
    DeleteObject(rim);
    DeleteObject(outer);

    const std::wstring first = L"Windows";
    const std::wstring second = L" Defender";
    const int textLeft = logoLeft + logoSize + 8;
    RECT firstRect = MakeRect(textLeft, 10, textLeft + 240, 34);
    ui::DrawTextIn(dc, firstRect, first, fontBrand_, theme.textPrimary, DT_LEFT | DT_TOP | DT_SINGLELINE);
    const int firstWidth = ui::MeasureTextWidth(dc, first, fontBrand_);
    RECT secondRect = MakeRect(textLeft + firstWidth, 10, textLeft + firstWidth + 260, 34);
    ui::DrawTextIn(dc, secondRect, second, fontBrand_, theme.brandPrimary, DT_LEFT | DT_TOP | DT_SINGLELINE);

    RECT tagline = MakeRect(textLeft + 2, 34, textLeft + 320, 50);
    ui::DrawTextIn(dc, tagline, L"classic console for Defender", fontSmall_, theme.textMuted,
                   DT_LEFT | DT_TOP | DT_SINGLELINE);
}

void MainWindow::PaintSidebar(HDC dc) {
    const AppTheme& theme = Theme();
    ui::FillSolid(dc, MakeRect(0, kHeaderHeight, kSidebarWidth, kClientHeight), theme.sidebarBg);
    ui::LineV(dc, kSidebarWidth, kHeaderHeight, kClientHeight, theme.border);

    RECT plate = MakeRect(8, 64, kSidebarWidth - 8, 94);
    ui::FillRounded(dc, plate, 3, theme.contentBg, theme.border);
    RECT text = plate;
    text.left += 10;
    ui::DrawTextIn(dc, text, L"Windows Defender", fontBold_, theme.brandPrimary,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void MainWindow::PaintSweep(HDC dc) {
    // Filled quarter ellipse: its arc crosses the left edge near y=344 and the
    // bottom edge near x=346, which is the yellow swoosh of the original skin.
    const AppTheme& theme = Theme();
    const int saved = SaveDC(dc);

    HRGN clip = CreateRectRgn(0, kHeaderHeight + 4, kClientWidth, kClientHeight);
    SelectClipRgn(dc, clip);

    HBRUSH brush = CreateSolidBrush(theme.sidebarAccent);
    HPEN pen = CreatePen(PS_SOLID, 1, ui::Blend(theme.sidebarAccent, RGB(0, 0, 0), 32));
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    Ellipse(dc, -360, 340, 480, 1180);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);

    RestoreDC(dc, saved);
    DeleteObject(clip);
}

void MainWindow::PaintFooter(HDC dc) {
    const AppTheme& theme = Theme();
    const std::wstring first = L"Windows";
    const std::wstring second = L" Defender";
    const std::wstring year = L" 2002";

    const int widthFirst = ui::MeasureTextWidth(dc, first, fontBrand_);
    const int widthSecond = ui::MeasureTextWidth(dc, second, fontBrand_);
    const int widthYear = ui::MeasureTextWidth(dc, year, fontBrand_);

    const int rightEdge = kPanelRight - 4;
    const int top = kClientHeight - 36;
    const int xYear = rightEdge - widthYear;
    const int xSecond = xYear - widthSecond;
    const int xFirst = xSecond - widthFirst;

    RECT yearRect = MakeRect(xYear, top, rightEdge, top + 26);
    ui::DrawTextIn(dc, yearRect, year, fontBrand_, theme.textMuted, DT_RIGHT | DT_TOP | DT_SINGLELINE);
    RECT secondRect = MakeRect(xSecond, top, xYear, top + 26);
    ui::DrawTextIn(dc, secondRect, second, fontBrand_, theme.brandPrimary, DT_RIGHT | DT_TOP | DT_SINGLELINE);
    RECT firstRect = MakeRect(xFirst, top, xSecond, top + 26);
    ui::DrawTextIn(dc, firstRect, first, fontBrand_, theme.textPrimary, DT_RIGHT | DT_TOP | DT_SINGLELINE);
}

void MainWindow::PaintStatusPage(HDC dc) {
    const AppTheme& theme = Theme();
    const std::vector<StatusRow> rows = BuildRows();
    const bool healthy = status_.wmiConnected && status_.realTimeProtection && status_.antivirusEnabled;

    const std::wstring title = healthy ? L"System Status: OK" : L"System Status: Attention Required";
    RECT titleRect = MakeRect(kPageLeft, kTitleTop, kPageRight, kTitleTop + 26);
    ui::DrawTextIn(dc, titleRect, title, fontTitle_, theme.textPrimary, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    const int titleWidth = ui::MeasureTextWidth(dc, title, fontTitle_);
    ui::DrawBadge(dc, kPageLeft + titleWidth + 12, kTitleTop + 4, 20, healthy, theme);

    if (!status_.wmiConnected && !statusError_.empty()) {
        RECT error = MakeRect(kPageLeft, kTitleTop + 28, kPageRight, kTitleTop + 42);
        ui::DrawTextIn(dc, error, statusError_, fontSmall_, theme.warnRed,
                       DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    const RECT group1 = MakeRect(kPageLeft, kGroup1Top, kDetailsLeft - 16, kGroup1Bottom);
    ui::DrawGroupBox(dc, group1, L"Security Scanning Features", theme, fontBold_, 21);
    const RECT group2 = MakeRect(kPageLeft, kGroup2Top, kDetailsLeft - 16, kGroup2Bottom);
    ui::DrawGroupBox(dc, group2, L"Virus Definition Service", theme, fontBold_, 21);

    for (int index = 0; index < static_cast<int>(rows.size()); ++index) {
        const RECT row = RowRect(index);
        ui::DrawFeatureRow(dc, row, rows[index].label, rows[index].value, rows[index].ok, index == selectedRow_,
                           theme, fontNormal_, fontBold_);
    }

    PaintDetails(dc, MakeRect(kDetailsLeft, kGroup1Top, kPageRight, kGroup2Bottom));
}

void MainWindow::PaintDetails(HDC dc, const RECT& rc) {
    const AppTheme& theme = Theme();
    const std::vector<StatusRow> rows = BuildRows();

    ui::FillRounded(dc, rc, 3, theme.contentBg, theme.border);
    RECT header = MakeRect(rc.left + 1, rc.top + 1, rc.right - 1, rc.top + 24);
    ui::FillSolid(dc, header, ui::Blend(theme.contentBg, theme.sectionHeaderBg, 40));
    ui::LineH(dc, header.left, header.right - 1, header.bottom, theme.border);
    ui::DrawTextIn(dc, header, L"Item Details", fontBold_, theme.textPrimary, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT body = MakeRect(rc.left + 10, header.bottom + 8, rc.right - 10, rc.bottom - 8);

    if (!status_.wmiConnected) {
        std::wstring message =
            L"The items marked in red need your attention.\n\nWindows Defender's WMI provider could not be "
            L"reached, so the status is unknown.";
        if (!statusError_.empty()) message += L"\n\n" + statusError_;
        ui::DrawTextIn(dc, body, message, fontSmall_, theme.textMuted, DT_LEFT | DT_TOP | DT_WORDBREAK);
        return;
    }

    const int selected = std::clamp(selectedRow_, 0, static_cast<int>(rows.size()) - 1);
    std::wstring message = rows[selected].label + L"\n\n" + rows[selected].details;
    message += L"\n\nClick the item at the left to read more about it.";
    ui::DrawTextIn(dc, body, message, fontSmall_, theme.textMuted, DT_LEFT | DT_TOP | DT_WORDBREAK);
}

void MainWindow::PaintScanPage(HDC dc) {
    const AppTheme& theme = Theme();

    RECT titleRect = MakeRect(kPageLeft, kTitleTop, kPageRight, kTitleTop + 26);
    ui::DrawTextIn(dc, titleRect, L"Scan for Viruses", fontTitle_, theme.textPrimary,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT subtitle = MakeRect(kPageLeft, 102, kPageRight, 118);
    ui::DrawTextIn(dc, subtitle, L"Choose how thoroughly the Defender engine should check this computer.",
                   fontSmall_, theme.textMuted, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

    const wchar_t* descriptions[3] = {
        L"Running processes, memory and the folders malware likes to hide in.",
        L"Every file on every fixed drive. It can run for an hour or more.",
        L"Any folder or drive you pick, scanned with the same engine.",
    };
    for (int index = 0; index < 3; ++index) {
        RECT note = MakeRect(476, 132 + index * 42, kPageRight, 132 + index * 42 + 34);
        ui::DrawTextIn(dc, note, descriptions[index], fontSmall_, theme.textMuted, DT_LEFT | DT_TOP | DT_WORDBREAK);
    }

    const int infoTop = 268;
    RECT line1 = MakeRect(kPageLeft, infoTop, kPageRight, infoTop + 16);
    ui::DrawTextIn(dc, line1, L"Last quick scan: " + status_.lastQuickScan, fontNormal_, theme.textPrimary,
                   DT_LEFT | DT_SINGLELINE);
    RECT line2 = MakeRect(kPageLeft, infoTop + 18, kPageRight, infoTop + 34);
    ui::DrawTextIn(dc, line2, L"Last full scan: " + status_.lastFullScan, fontNormal_, theme.textPrimary,
                   DT_LEFT | DT_SINGLELINE);
    RECT line3 = MakeRect(kPageLeft, infoTop + 36, kPageRight, infoTop + 52);
    ui::DrawTextIn(dc, line3, L"Virus definitions: " + status_.signatureVersion + L" (" + status_.lastSignatureUpdate + L")",
                   fontNormal_, theme.textPrimary, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void MainWindow::PaintReportsPage(HDC dc) {
    const AppTheme& theme = Theme();

    RECT titleRect = MakeRect(kPageLeft, kTitleTop, kPageRight - 90, kTitleTop + 26);
    ui::DrawTextIn(dc, titleRect, L"Activity & Threat Reports", fontTitle_, theme.textPrimary,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT subtitle = MakeRect(kPageLeft, 102, kPageRight, 118);
    ui::DrawTextIn(dc, subtitle, L"MSFT_MpThreatDetection joined with the MSFT_MpThreat catalogue.",
                   fontSmall_, theme.textMuted, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT footer = MakeRect(kPageLeft, 352, kPageRight, 376);
    if (!threatsError_.empty()) {
        ui::DrawTextIn(dc, footer, threatsError_, fontSmall_, theme.warnRed,
                       DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    } else if (threats_.empty()) {
        ui::DrawTextIn(dc, footer, L"No threat detections reported by Windows Defender.", fontSmall_,
                       theme.textMuted, DT_LEFT | DT_SINGLELINE);
    } else {
        const std::wstring count = L"Detections: " + std::to_wstring(threats_.size()) +
                                   L"   -   Windows Security > Protection history holds the full log.";
        ui::DrawTextIn(dc, footer, count, fontSmall_, theme.textMuted, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
}
