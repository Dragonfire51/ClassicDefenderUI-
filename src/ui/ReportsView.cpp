#include "ReportsView.h"

#include <commctrl.h>

#include <algorithm>

#include "../Resource.h"
#include "Paint.h"

namespace {

constexpr int kColumns = 4;
const wchar_t* kColumnTitles[kColumns] = {L"Threat", L"Severity", L"Detected", L"Status"};
const int kColumnWidths[kColumns] = {162, 74, 86, 84};

void SetSubItemText(HWND list, int row, int column, const std::wstring& text) {
    ListView_SetItemText(list, row, column, const_cast<wchar_t*>(text.c_str()));
}

}  // namespace

void ReportsView::Create(HWND parent, HINSTANCE instance) {
    list_ = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                            WS_CHILD | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 0, 0, 0, 0,
                            parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_REPORTS_LIST)), instance,
                            nullptr);
    if (!list_) return;

    ListView_SetExtendedListViewStyle(list_, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

    for (int column = 0; column < kColumns; ++column) {
        LVCOLUMNW info = {};
        info.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;
        info.pszText = const_cast<wchar_t*>(kColumnTitles[column]);
        info.cx = ui::Scale(kColumnWidths[column]);
        info.iSubItem = column;
        info.fmt = column == 0 ? LVCFMT_LEFT : LVCFMT_LEFT;
        ListView_InsertColumn(list_, column, &info);
    }

    refresh_ = CreateWindowExW(0, L"BUTTON", L"Refresh", WS_CHILD | BS_OWNERDRAW | WS_TABSTOP, 0, 0, 0, 0, parent,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_REPORTS_REFRESH)), instance,
                               nullptr);

    header_ = ListView_GetHeader(list_);
    if (header_) {
        headerProc_ = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(header_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&ReportsView::HeaderProc)));
        SetWindowLongPtrW(header_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    }
}

void ReportsView::Destroy() {
    if (header_ && headerProc_) {
        SetWindowLongPtrW(header_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(headerProc_));
        headerProc_ = nullptr;
    }
    if (refresh_) DestroyWindow(refresh_);
    if (list_) DestroyWindow(list_);
    refresh_ = nullptr;
    list_ = nullptr;
    header_ = nullptr;
}

void ReportsView::Layout(const RECT& deviceRect) {
    if (!list_) return;
    MoveWindow(list_, deviceRect.left, deviceRect.top, deviceRect.right - deviceRect.left,
               deviceRect.bottom - deviceRect.top, TRUE);
}

void ReportsView::SetThreats(const std::vector<ThreatRecord>& threats) {
    threats_ = threats;
    if (!list_) return;

    SendMessageW(list_, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(list_);

    for (size_t index = 0; index < threats_.size(); ++index) {
        const ThreatRecord& threat = threats_[index];

        LVITEMW item = {};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = static_cast<int>(index);
        item.pszText = const_cast<wchar_t*>(threat.name.c_str());
        item.lParam = static_cast<LPARAM>(index);
        const int row = ListView_InsertItem(list_, &item);
        if (row < 0) continue;

        SetSubItemText(list_, row, 1, SeverityName(threat.severityId));
        SetSubItemText(list_, row, 2, threat.detected.empty() ? L"-" : threat.detected);
        SetSubItemText(list_, row, 3, ThreatStatusName(threat.statusId));
    }

    SendMessageW(list_, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(list_, nullptr, TRUE);
}

int ReportsView::Selected() const {
    if (!list_) return -1;
    return ListView_GetNextItem(list_, -1, LVNI_SELECTED);
}

void ReportsView::ApplyTheme(const AppTheme& theme) {
    theme_ = theme;
    if (!list_) return;
    ListView_SetBkColor(list_, theme.contentBg);
    ListView_SetTextBkColor(list_, theme.contentBg);
    ListView_SetTextColor(list_, theme.textPrimary);
    InvalidateRect(list_, nullptr, TRUE);
}

LRESULT ReportsView::OnCustomDraw(NMHDR* header) {
    auto* draw = reinterpret_cast<NMLVCUSTOMDRAW*>(header);
    switch (draw->nmcd.dwDrawStage) {
        case CDDS_PREPAINT:
            return CDRF_NOTIFYITEMDRAW;
        case CDDS_ITEMPREPAINT:
        case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {
            const size_t index = static_cast<size_t>(draw->nmcd.lItemlParam);
            const bool severe = index < threats_.size() && threats_[index].severityId >= 4;
            draw->clrText = severe ? theme_.warnRed : theme_.textPrimary;
            draw->clrTextBk = theme_.contentBg;
            return CDRF_NEWFONT;
        }
        default:
            return CDRF_DODEFAULT;
    }
}

LRESULT CALLBACK ReportsView::HeaderProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<ReportsView*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (self && message == WM_NOTIFY) {
        auto* notification = reinterpret_cast<NMHDR*>(lParam);
        if (notification && notification->code == NM_CUSTOMDRAW) {
            auto* draw = reinterpret_cast<NMCUSTOMDRAW*>(notification);
            if (draw->dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
            if (draw->dwDrawStage == CDDS_ITEMPREPAINT) {
                SetBkColor(draw->hdc, self->theme_.sectionHeaderBg);
                SetTextColor(draw->hdc, self->theme_.textPrimary);
                return CDRF_NEWFONT;
            }
        }
    }

    if (self && self->headerProc_) return CallWindowProcW(self->headerProc_, hwnd, message, wParam, lParam);
    return DefWindowProcW(hwnd, message, wParam, lParam);
}
