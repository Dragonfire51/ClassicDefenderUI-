#include "Paint.h"

#include <algorithm>

namespace ui {
namespace {

int g_dpi = 96;

void DrawHLineImpl(HDC dc, int x1, int x2, int y, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, x1, y, nullptr);
    LineTo(dc, x2, y);
    SelectObject(dc, old);
    DeleteObject(pen);
}

void DrawVLineImpl(HDC dc, int x, int y1, int y2, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, x, y1, nullptr);
    LineTo(dc, x, y2);
    SelectObject(dc, old);
    DeleteObject(pen);
}

void DrawArrowDown(HDC dc, int cx, int top, int length, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HBRUSH brush = CreateSolidBrush(color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, brush);

    MoveToEx(dc, cx, top, nullptr);
    LineTo(dc, cx, top + length);
    POINT head[3] = {{cx - 4, top + length - 4}, {cx + 5, top + length - 4}, {cx, top + length + 2}};
    Polygon(dc, head, 3);

    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void DrawToolbarGlyph(HDC dc, const RECT& rc, ToolGlyph glyph, const AppTheme& theme) {
    const COLORREF frame = Blend(theme.textPrimary, theme.topBarBg, 20);
    const COLORREF accent = theme.brandPrimary;

    switch (glyph) {
        case ToolGlyph::LiveUpdate: {
            // Monitor with a download arrow: the classic LiveUpdate motif.
            RECT screen = {rc.left + 1, rc.top + 1, rc.right - 5, rc.bottom - 5};
            StrokeRect(dc, screen, frame);
            RECT inner = Deflate(screen, 1);
            FillSolid(dc, inner, Blend(theme.topBarBg, RGB(255, 255, 255), 55));
            DrawArrowDown(dc, inner.left + 6, inner.top + 2, 6, accent);
            DrawHLineImpl(dc, screen.left + 3, screen.right - 3, screen.bottom + 3, frame);
            break;
        }
        case ToolGlyph::Options: {
            // Slider panel.
            RECT panel = {rc.left + 1, rc.top + 1, rc.right - 3, rc.bottom - 3};
            FillSolid(dc, panel, Blend(theme.topBarBg, RGB(255, 255, 255), 55));
            StrokeRect(dc, panel, frame);
            DrawHLineImpl(dc, panel.left, panel.right - 1, panel.top + 4, frame);
            const int midY = panel.top + 10;
            DrawHLineImpl(dc, panel.left + 3, panel.right - 3, midY, frame);
            RECT knob = {panel.left + 7, midY - 3, panel.left + 12, midY + 3};
            FillSolid(dc, knob, accent);
            StrokeRect(dc, knob, frame);
            break;
        }
        case ToolGlyph::Help: {
            HPEN pen = CreatePen(PS_SOLID, 1, frame);
            HGDIOBJ oldPen = SelectObject(dc, pen);
            HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
            Ellipse(dc, rc.left, rc.top, rc.right, rc.bottom);
            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
            DeleteObject(pen);

            HFONT font = CreateUiFont(15, FW_BOLD, false, L"Tahoma");
            DrawTextIn(dc, rc, L"?", font, accent, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            DeleteObject(font);
            break;
        }
    }
}

}  // namespace

void SetDpi(int dpi) {
    g_dpi = dpi > 0 ? dpi : 96;
}

int Dpi() {
    return g_dpi;
}

int Scale(int logical) {
    return MulDiv(logical, g_dpi, 96);
}

COLORREF Blend(COLORREF base, COLORREF other, int percentOther) {
    const int percentBase = 100 - std::clamp(percentOther, 0, 100);
    const int p = std::clamp(percentOther, 0, 100);
    return RGB((GetRValue(base) * percentBase + GetRValue(other) * p) / 100,
               (GetGValue(base) * percentBase + GetGValue(other) * p) / 100,
               (GetBValue(base) * percentBase + GetBValue(other) * p) / 100);
}

RECT Deflate(const RECT& rc, int amount) {
    RECT out = {rc.left + amount, rc.top + amount, rc.right - amount, rc.bottom - amount};
    return out;
}

void FillSolid(HDC dc, const RECT& rc, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rc, brush);
    DeleteObject(brush);
}

void StrokeRect(HDC dc, const RECT& rc, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void FillRounded(HDC dc, const RECT& rc, int radius, COLORREF fill, COLORREF border) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void LineH(HDC dc, int left, int right, int y, COLORREF color) {
    DrawHLineImpl(dc, left, right, y, color);
}

void LineV(HDC dc, int x, int top, int bottom, COLORREF color) {
    DrawVLineImpl(dc, x, top, bottom, color);
}

HFONT CreateUiFont(int height, int weight, bool italic, const wchar_t* face) {
    return CreateFontW(height, 0, 0, 0, weight, italic ? TRUE : FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
}

void DrawTextIn(HDC dc, const RECT& rc, const std::wstring& text, HFONT font, COLORREF color, UINT flags) {
    if (text.empty()) return;
    HGDIOBJ oldFont = font ? SelectObject(dc, font) : nullptr;
    const int oldMode = SetBkMode(dc, TRANSPARENT);
    const COLORREF oldColor = SetTextColor(dc, color);

    RECT target = rc;
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &target, flags);

    SetTextColor(dc, oldColor);
    SetBkMode(dc, oldMode);
    if (oldFont) SelectObject(dc, oldFont);
}

int MeasureTextWidth(HDC dc, const std::wstring& text, HFONT font) {
    if (!font) return 0;
    HGDIOBJ oldFont = SelectObject(dc, font);
    SIZE size = {};
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
    SelectObject(dc, oldFont);
    return size.cx;
}

void DrawBadge(HDC dc, int x, int y, int size, bool ok, const AppTheme& theme) {
    const COLORREF fill = ok ? theme.okGreen : theme.warnRed;
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN rim = CreatePen(PS_SOLID, 1, Blend(fill, RGB(0, 0, 0), 45));
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, rim);
    Ellipse(dc, x, y, x + size, y + size);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(rim);
    DeleteObject(brush);

    HPEN mark = CreatePen(PS_SOLID, std::max(1, size / 8), RGB(255, 255, 255));
    HGDIOBJ savedPen = SelectObject(dc, mark);
    if (ok) {
        MoveToEx(dc, x + size * 26 / 100, y + size * 52 / 100, nullptr);
        LineTo(dc, x + size * 44 / 100, y + size * 70 / 100);
        LineTo(dc, x + size * 76 / 100, y + size * 30 / 100);
    } else {
        MoveToEx(dc, x + size * 32 / 100, y + size * 32 / 100, nullptr);
        LineTo(dc, x + size * 68 / 100, y + size * 68 / 100);
        MoveToEx(dc, x + size * 68 / 100, y + size * 32 / 100, nullptr);
        LineTo(dc, x + size * 32 / 100, y + size * 68 / 100);
    }
    SelectObject(dc, savedPen);
    DeleteObject(mark);
}

void DrawGroupBox(HDC dc, const RECT& rc, const std::wstring& title, const AppTheme& theme, HFONT titleFont,
                  int headerHeight) {
    FillSolid(dc, rc, theme.contentBg);
    StrokeRect(dc, rc, theme.border);

    RECT header = {rc.left + 1, rc.top + 1, rc.right - 1, rc.top + 1 + headerHeight};
    FillSolid(dc, header, theme.sectionHeaderBg);
    DrawHLineImpl(dc, header.left, header.right - 1, header.bottom, theme.border);

    RECT text = Deflate(header, 1);
    text.left += 9;
    DrawTextIn(dc, text, title, titleFont, theme.textPrimary, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void DrawFeatureRow(HDC dc, const RECT& rc, const std::wstring& label, const std::wstring& value, bool ok,
                    bool selected, const AppTheme& theme, HFONT normal, HFONT bold) {
    if (selected) FillSolid(dc, rc, Blend(theme.contentBg, theme.sectionHeaderBg, 45));

    const int badgeSize = 16;
    DrawBadge(dc, rc.left + 10, rc.top + (rc.bottom - rc.top - badgeSize) / 2, badgeSize, ok, theme);

    RECT labelRect = {rc.left + 36, rc.top, rc.right - 130, rc.bottom};
    DrawTextIn(dc, labelRect, label, normal, theme.textPrimary,
               DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT valueRect = {rc.right - 124, rc.top, rc.right - 14, rc.bottom};
    DrawTextIn(dc, valueRect, value, ok ? normal : bold, ok ? theme.textPrimary : theme.warnRed,
               DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    DrawHLineImpl(dc, rc.left + 8, rc.right - 8, rc.bottom - 1, Blend(theme.contentBg, theme.border, 35));
}

void DrawClassicButton(HDC dc, const RECT& rc, const std::wstring& text, const AppTheme& theme, HFONT font,
                       bool pressed, bool focused, bool defaultButton) {
    const COLORREF face = theme.sectionHeaderBg;
    const COLORREF light = Blend(face, RGB(255, 255, 255), 85);
    const COLORREF shadow = Blend(face, RGB(0, 0, 0), 30);
    const COLORREF frame = Blend(face, theme.textPrimary, 45);

    FillSolid(dc, rc, face);
    StrokeRect(dc, rc, defaultButton ? theme.brandPrimary : frame);

    const RECT inner = Deflate(rc, 1);
    const COLORREF topLeft = pressed ? shadow : light;
    const COLORREF bottomRight = pressed ? light : shadow;
    DrawHLineImpl(dc, inner.left, inner.right - 1, inner.top, topLeft);
    DrawVLineImpl(dc, inner.left, inner.top, inner.bottom - 1, topLeft);
    DrawHLineImpl(dc, inner.left, inner.right - 1, inner.bottom - 1, bottomRight);
    DrawVLineImpl(dc, inner.right - 1, inner.top, inner.bottom - 1, bottomRight);

    RECT textRect = inner;
    if (pressed) OffsetRect(&textRect, 1, 1);
    DrawTextIn(dc, textRect, text, font, theme.textPrimary, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (focused) {
        RECT focus = Deflate(rc, 4);
        DrawFocusRect(dc, &focus);
    }
}

void DrawToolbarButton(HDC dc, const RECT& rc, ToolGlyph glyph, const std::wstring& label, const AppTheme& theme,
                       HFONT labelFont, bool pressed) {
    // The button sits on the header, so it has to repaint its own background.
    ui::FillSolid(dc, rc, theme.topBarBg);
    if (pressed) {
        FillSolid(dc, rc, Blend(theme.topBarBg, RGB(0, 0, 0), 10));
        StrokeRect(dc, rc, Blend(theme.topBarBg, RGB(0, 0, 0), 45));
    }

    RECT content = rc;
    if (pressed) OffsetRect(&content, 1, 1);

    const int glyphWidth = 24;
    const int centre = content.left + (content.right - content.left) / 2;
    RECT glyphRect = {centre - glyphWidth / 2, content.top + 2, centre + glyphWidth / 2, content.top + 24};
    DrawToolbarGlyph(dc, glyphRect, glyph, theme);

    RECT labelRect = {content.left, content.top + 25, content.right, content.bottom};
    DrawTextIn(dc, labelRect, label, labelFont, theme.textPrimary, DT_CENTER | DT_TOP | DT_SINGLELINE);
}

}  // namespace ui

namespace ui {
namespace {

RECT RectOf(int left, int top, int right, int bottom) {
    RECT rc = {left, top, right, bottom};
    return rc;
}

}  // namespace

void DrawProgressBar(HDC dc, const RECT& rc, int percent, int marqueePhase, const AppTheme& theme) {
    const RECT inner = Deflate(rc, 1);

    FillSolid(dc, rc, theme.border);
    FillSolid(dc, inner, Blend(theme.contentBg, RGB(0, 0, 0), 8));
    LineH(dc, inner.left, inner.right, inner.top, Blend(theme.contentBg, RGB(0, 0, 0), 22));
    LineV(dc, inner.left, inner.top, inner.bottom, Blend(theme.contentBg, RGB(0, 0, 0), 22));

    const int left = inner.left + 1;
    const int right = inner.right - 1;
    const int top = inner.top + 1;
    const int bottom = inner.bottom - 1;
    if (right <= left) return;

    const int fillRight = percent < 0 ? right : left + MulDiv(right - left, std::clamp(percent, 0, 100), 100);
    const COLORREF face = theme.brandPrimary;
    const COLORREF lightFace = Blend(face, RGB(255, 255, 255), 35);
    const COLORREF shade = Blend(face, RGB(0, 0, 0), 28);

    if (fillRight > left) {
        const int middle = top + (bottom - top) / 2;
        FillSolid(dc, RectOf(left, top, fillRight, middle), lightFace);
        FillSolid(dc, RectOf(left, middle, fillRight, bottom), face);
        // Segment grooves, the detail that gives the bar its period look.
        for (int x = left + 9; x < fillRight; x += 9) LineV(dc, x, top, bottom, shade);
    }

    if (percent < 0) {
        const int span = std::max(1, bottom - top);
        const int step = 18;
        const int offset = (marqueePhase * 2) % step;
        const COLORREF stripe = Blend(theme.brandPrimary, theme.contentBg, 55);
        HBRUSH brush = CreateSolidBrush(stripe);
        HPEN pen = CreatePen(PS_SOLID, 1, stripe);
        HGDIOBJ oldBrush = SelectObject(dc, brush);
        HGDIOBJ oldPen = SelectObject(dc, pen);
        for (int x = left + offset - step; x < right; x += step) {
            POINT points[3] = {{x, top}, {x + span / 2, top + span / 2}, {x, bottom}};
            Polygon(dc, points, 3);
        }
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(pen);
        DeleteObject(brush);
    }
}

}  // namespace ui
