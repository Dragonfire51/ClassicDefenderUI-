#pragma once

#include <windows.h>

#include <string>

#include "../AppTheme.h"

// All drawing helpers work in 96 dpi logical units. The main window paints
// through an anisotropic mapping so the whole skin scales with the monitor;
// child controls are positioned with ui::Scale() instead.
namespace ui {

void SetDpi(int dpi);
int Dpi();
int Scale(int logical);

COLORREF Blend(COLORREF base, COLORREF other, int percentOther);
RECT Deflate(const RECT& rc, int amount);

void FillSolid(HDC dc, const RECT& rc, COLORREF color);
void StrokeRect(HDC dc, const RECT& rc, COLORREF color);
void FillRounded(HDC dc, const RECT& rc, int radius, COLORREF fill, COLORREF border);
void LineH(HDC dc, int left, int right, int y, COLORREF color);
void LineV(HDC dc, int x, int top, int bottom, COLORREF color);

HFONT CreateUiFont(int height, int weight, bool italic, const wchar_t* face);

void DrawTextIn(HDC dc, const RECT& rc, const std::wstring& text, HFONT font, COLORREF color, UINT flags);
int MeasureTextWidth(HDC dc, const std::wstring& text, HFONT font);

// Green tick / red cross inside a circle, the core motif of the 2002 status page.
void DrawBadge(HDC dc, int x, int y, int size, bool ok, const AppTheme& theme);

void DrawGroupBox(HDC dc, const RECT& rc, const std::wstring& title, const AppTheme& theme, HFONT titleFont,
                  int headerHeight);

void DrawFeatureRow(HDC dc, const RECT& rc, const std::wstring& label, const std::wstring& value, bool ok,
                    bool selected, const AppTheme& theme, HFONT normal, HFONT bold);

// Classic sunken bar with moving stripes. percent < 0 runs indeterminate, with
// marqueePhase driving the stripe offset.
void DrawProgressBar(HDC dc, const RECT& rc, int percent, int marqueePhase, const AppTheme& theme);

// Bevel button used by the scan page and the toolbar glyphs.
void DrawClassicButton(HDC dc, const RECT& rc, const std::wstring& text, const AppTheme& theme, HFONT font,
                       bool pressed, bool focused, bool defaultButton = false);

enum class ToolGlyph { LiveUpdate, Options, Help };
void DrawToolbarButton(HDC dc, const RECT& rc, ToolGlyph glyph, const std::wstring& label, const AppTheme& theme,
                       HFONT labelFont, bool pressed);

}  // namespace ui
