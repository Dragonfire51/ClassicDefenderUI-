#include "ProgressWindow.h"

#include <algorithm>
#include <functional>

#include "../Resource.h"
#include "Paint.h"

namespace ui {
namespace {

constexpr int kWidth = 420;
constexpr int kHeight = 300;

constexpr int kCloseButton = 1201;

RECT MakeRect(int left, int top, int right, int bottom) {
    RECT rc = {left, top, right, bottom};
    return rc;
}

std::wstring FormatCount(long long value) {
    std::wstring digits = std::to_wstring(value);
    std::wstring out;
    int count = 0;
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
        if (count != 0 && count % 3 == 0) out.push_back(L',');
        out.push_back(*it);
        ++count;
    }
    std::reverse(out.begin(), out.end());
    return out;
}

std::wstring FormatBytes(double bytes) {
    const wchar_t* units[] = {L"B", L"KB", L"MB", L"GB"};
    int unit = 0;
    while (bytes >= 1024.0 && unit < 3) {
        bytes /= 1024.0;
        ++unit;
    }
    wchar_t number[32] = {};
    swprintf(number, 32, L"%.1f", bytes);
    return std::wstring(number) + L" " + units[unit];
}

unsigned long long SnapshotHash(const ProgressSnapshot& snapshot) {
    unsigned long long hash = static_cast<unsigned long long>(snapshot.phase);
    const long long values[] = {snapshot.filesChecked, snapshot.filesTotal, snapshot.systemItems, snapshot.memoryItems,
                                snapshot.startupItems, snapshot.bytes, snapshot.percent, snapshot.threats};
    for (long long value : values) hash = hash * 1315423911ull + static_cast<unsigned long long>(value);
    hash ^= std::hash<std::wstring>{}(snapshot.title + snapshot.subtitle + snapshot.current + snapshot.message +
                                      snapshot.signatures);
    return hash;
}

}  // namespace

ProgressWindow::~ProgressWindow() {
    if (hwnd_ && IsWindow(hwnd_)) DestroyWindow(hwnd_);
}

bool ProgressWindow::Create(HWND owner, HINSTANCE instance, std::shared_ptr<ProgressTracker> tracker,
                            const AppTheme& theme) {
    owner_ = owner;
    instance_ = instance;
    tracker_ = std::move(tracker);
    theme_ = theme;

    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = &ProgressWindow::WndProcStatic;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = L"ClassicDefenderProgress";
    RegisterClassExW(&windowClass);

    const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    RECT frame = {0, 0, ui::Scale(kWidth), ui::Scale(kHeight)};
    AdjustWindowRectEx(&frame, style, FALSE, WS_EX_DLGMODALFRAME);
    const int width = frame.right - frame.left;
    const int height = frame.bottom - frame.top;

    RECT ownerRect = {};
    GetWindowRect(owner, &ownerRect);
    const int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - width) / 2;
    const int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - height) / 2;

    hwnd_ = CreateWindowExW(WS_EX_DLGMODALFRAME, windowClass.lpszClassName, L"Windows Defender",
                            style | WS_VISIBLE, x, y, width, height, owner, nullptr, instance, this);
    if (!hwnd_) return false;
    UpdateWindow(hwnd_);
    return true;
}

LRESULT CALLBACK ProgressWindow::WndProcStatic(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    ProgressWindow* self = reinterpret_cast<ProgressWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<ProgressWindow*>(create->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    if (!self) return DefWindowProcW(hwnd, message, wParam, lParam);
    return self->WndProc(message, wParam, lParam);
}

LRESULT ProgressWindow::WndProc(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
            return OnCreate() ? 0 : -1;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT paint = {};
            HDC target = BeginPaint(hwnd_, &paint);
            if (target) {
                OnPaint(target);
                EndPaint(hwnd_, &paint);
            }
            return 0;
        }

        case WM_TIMER: {
            ++marqueePhase_;
            const ProgressSnapshot snapshot = tracker_ ? tracker_->Get() : ProgressSnapshot{};
            const unsigned long long hash = SnapshotHash(snapshot);
            if (hash != lastHash_ || !IsFinished(snapshot)) {
                lastHash_ = hash;
                SyncButton();
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return 0;
        }

        case WM_DRAWITEM:
            OnDrawItem(*reinterpret_cast<DRAWITEMSTRUCT*>(lParam));
            return TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == kCloseButton || LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                const ProgressSnapshot snapshot = tracker_ ? tracker_->Get() : ProgressSnapshot{};
                if (IsFinished(snapshot)) DestroyWindow(hwnd_);
            }
            return 0;

        case WM_CLOSE: {
            // The engine call cannot be interrupted, so the window stays until it ends.
            const ProgressSnapshot snapshot = tracker_ ? tracker_->Get() : ProgressSnapshot{};
            if (IsFinished(snapshot)) DestroyWindow(hwnd_);
            return 0;
        }

        case WM_DESTROY:
            KillTimer(hwnd_, 1);
            DeleteObject(fontTitle_);
            DeleteObject(fontBold_);
            DeleteObject(fontNormal_);
            DeleteObject(fontSmall_);
            DeleteObject(fontBrand_);
            fontTitle_ = fontBold_ = fontNormal_ = fontSmall_ = fontBrand_ = nullptr;
            hwnd_ = nullptr;
            return 0;

        default:
            break;
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

bool ProgressWindow::OnCreate() {
    fontTitle_ = ui::CreateUiFont(-19, FW_BOLD, false, L"Arial");
    fontBold_ = ui::CreateUiFont(-11, FW_BOLD, false, L"Tahoma");
    fontNormal_ = ui::CreateUiFont(-11, FW_NORMAL, false, L"Tahoma");
    fontSmall_ = ui::CreateUiFont(-9, FW_NORMAL, false, L"Tahoma");
    fontBrand_ = ui::CreateUiFont(-12, FW_BOLD, true, L"Arial");

    closeButton_ = CreateWindowExW(0, L"BUTTON", L"Close",
                                   WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP | WS_DISABLED, 0, 0, 0, 0,
                                   hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCloseButton)), instance_,
                                   nullptr);
    if (closeButton_) SendMessageW(closeButton_, WM_SETFONT, reinterpret_cast<WPARAM>(fontNormal_), TRUE);
    MoveWindow(closeButton_, ui::Scale(kWidth - 86), ui::Scale(kHeight - 34), ui::Scale(72), ui::Scale(23), FALSE);

    startTick_ = GetTickCount64();
    SetTimer(hwnd_, 1, 150, nullptr);
    return true;
}

void ProgressWindow::ApplyTheme(const AppTheme& theme) {
    theme_ = theme;
    if (hwnd_) InvalidateRect(hwnd_, nullptr, TRUE);
}

void ProgressWindow::SyncButton() {
    if (!closeButton_) return;
    const ProgressSnapshot snapshot = tracker_ ? tracker_->Get() : ProgressSnapshot{};
    EnableWindow(closeButton_, IsFinished(snapshot) ? TRUE : FALSE);
}

void ProgressWindow::OnDrawItem(const DRAWITEMSTRUCT& item) {
    if (item.hDC == nullptr || static_cast<int>(item.CtlID) != kCloseButton) return;
    ui::DrawClassicButton(item.hDC, item.rcItem, L"Close", theme_, fontNormal_,
                          (item.itemState & ODS_SELECTED) != 0, (item.itemState & ODS_FOCUS) != 0);
}

void ProgressWindow::OnPaint(HDC target) {
    RECT client = {};
    GetClientRect(hwnd_, &client);
    if (client.right <= 0 || client.bottom <= 0) return;

    HDC memory = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, client.right, client.bottom);
    HGDIOBJ oldBitmap = SelectObject(memory, bitmap);

    SetMapMode(memory, MM_ANISOTROPIC);
    SetWindowExtEx(memory, kWidth, kHeight, nullptr);
    SetViewportExtEx(memory, client.right, client.bottom, nullptr);
    SetBkMode(memory, TRANSPARENT);

    Paint(memory);

    SetMapMode(memory, MM_TEXT);
    BitBlt(target, 0, 0, client.right, client.bottom, memory, 0, 0, SRCCOPY);

    SelectObject(memory, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memory);
}

void ProgressWindow::Paint(HDC dc) {
    const ProgressSnapshot snapshot = tracker_ ? tracker_->Get() : ProgressSnapshot{};

    ui::FillSolid(dc, MakeRect(0, 0, kWidth, kHeight), theme_.contentBg);

    ui::FillSolid(dc, MakeRect(0, 0, kWidth, 28), theme_.topBarBg);
    ui::FillSolid(dc, MakeRect(0, 0, kWidth, 2), theme_.brandPrimary);
    ui::LineH(dc, 0, kWidth, 28, theme_.border);
    ui::DrawTextIn(dc, MakeRect(12, 6, kWidth - 12, 26), L"Windows Defender AntiVirus", fontBrand_, theme_.textMuted,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    ui::DrawTextIn(dc, MakeRect(14, 34, kWidth - 14, 60), snapshot.title, fontTitle_, theme_.textPrimary,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    ui::DrawTextIn(dc, MakeRect(14, 60, kWidth - 14, 78), snapshot.subtitle, fontSmall_, theme_.textMuted,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    const int barPercent =
        snapshot.phase == ProgressPhase::Done ? 100 : (snapshot.phase == ProgressPhase::Failed ? 0 : snapshot.percent);
    ui::DrawProgressBar(dc, MakeRect(14, 78, kWidth - 14, 100), barPercent, marqueePhase_, theme_);

    int top = 106;
    for (const Stat& stat : BuildStats(snapshot)) {
        ui::DrawTextIn(dc, MakeRect(16, top, 112, top + 17), stat.label, fontNormal_, theme_.textMuted,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        // Right aligned by measured width: DrawText's DT_RIGHT kept overflowing
        // in Wine's font substitution, and this stays exact everywhere.
        const int valueWidth = ui::MeasureTextWidth(dc, stat.value, fontNormal_);
        const int valueLeft = std::max(112, kWidth - 30 - valueWidth);
        ui::DrawTextIn(dc, MakeRect(valueLeft, top, kWidth - 24, top + 17), stat.value, fontNormal_,
                       theme_.textPrimary, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        ui::LineH(dc, 16, kWidth - 16, top + 17, ui::Blend(theme_.contentBg, theme_.sectionHeaderBg, 55));
        top += 17;
    }

    ui::LineH(dc, 14, kWidth - 14, 208, theme_.border);

    if (IsFinished(snapshot)) {
        const COLORREF color = snapshot.phase == ProgressPhase::Failed ? theme_.warnRed : theme_.brandPrimary;
        ui::DrawTextIn(dc, MakeRect(16, 216, kWidth - 16, kHeight - 46), snapshot.message, fontBold_, color,
                       DT_LEFT | DT_TOP | DT_WORDBREAK);
    } else {
        const std::wstring nowLabel = snapshot.kind == ProgressKind::Scan ? L"Now checking" : L"Status";
        ui::DrawTextIn(dc, MakeRect(16, 214, kWidth - 16, 230), nowLabel, fontBold_, theme_.textMuted,
                       DT_LEFT | DT_SINGLELINE);
        ui::DrawTextIn(dc, MakeRect(16, 230, kWidth - 16, 246), snapshot.current, fontSmall_, theme_.textMuted,
                       DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        const std::wstring footnote =
            snapshot.kind == ProgressKind::Scan
                ? L"Counters come from this window's own walk of the scan scope: Defender reports only start "
                  L"and finish, and cannot be interrupted from here."
                : L"Speed is measured from the growth of the Defender definition folder.";
        ui::DrawTextIn(dc, MakeRect(16, 250, kWidth - 100, kHeight - 10), footnote, fontSmall_, theme_.textMuted,
                       DT_LEFT | DT_TOP | DT_WORDBREAK);
    }
}

std::vector<ProgressWindow::Stat> ProgressWindow::BuildStats(const ProgressSnapshot& snapshot) const {
    std::vector<Stat> stats;
    if (snapshot.kind == ProgressKind::Scan) {
        std::wstring files = FormatCount(snapshot.filesChecked);
        if (snapshot.filesTotal > 0) files += L" / " + FormatCount(snapshot.filesTotal);
        stats.push_back({L"Files examined", files});
        stats.push_back({L"System items", FormatCount(snapshot.systemItems)});
        stats.push_back({L"Memory (processes)", FormatCount(snapshot.memoryItems)});
        stats.push_back({L"Startup items", FormatCount(snapshot.startupItems)});
        if (snapshot.threats > 0) stats.push_back({L"Threats found", FormatCount(snapshot.threats)});
    } else {
        stats.push_back({L"Downloaded", snapshot.bytes > 0 ? FormatBytes(static_cast<double>(snapshot.bytes)) : L"-"});
        stats.push_back({L"Speed", snapshot.bytesPerSecond >= 1.0 ? FormatBytes(snapshot.bytesPerSecond) + L"/s" : L"-"});
        stats.push_back({L"Signatures", snapshot.signatures.empty() ? L"-" : snapshot.signatures});
        if (snapshot.filesChecked > 0) stats.push_back({L"Files refreshed", FormatCount(snapshot.filesChecked)});
    }
    stats.push_back({L"Elapsed", Elapsed()});
    return stats;
}

std::wstring ProgressWindow::Elapsed() const {
    const unsigned long long seconds = (GetTickCount64() - startTick_) / 1000;
    wchar_t buffer[32] = {};
    swprintf(buffer, 32, L"%02llu:%02llu", seconds / 60, seconds % 60);
    return buffer;
}

}  // namespace ui
