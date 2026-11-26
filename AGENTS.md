# ClassicDefenderUI — agent notes

Win32 (C++17, GDI, no UWP and no .NET) front-end for the Microsoft Defender WMI
provider, drawn to look like Norton AntiVirus 2002. Resizable/maximizable window
whose 2002 layout reflows from a 660x452 design size, owner-draw navigation,
live-switchable colour themes.

## Build

```bat
:: Windows / MSVC
cmake -B build -A x64
cmake --build build --config Release
```

```sh
# Cross-compile from Linux (MinGW-w64)
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw-w64-x86_64.cmake -DBUILD_TESTING_TOOLS=ON
cmake --build build
```

`BUILD_TESTING_TOOLS=ON` adds `theme_test.exe`, a console harness that drives the
real registry code (`ctest --test-dir build`, or run it under Wine).

`ClassicDefenderUI.exe --demo` runs the whole UI on sample data (no WMI) — use it
for layout work and screenshots.

## Code map

| File | Responsibility |
| --- | --- |
| `src/DefenderWmi.*` | COM/WMI client for `ROOT\Microsoft\Windows\Defender` (`MSFT_MpComputerStatus`, `MSFT_MpScan`, `MSFT_MpSignature`, `MSFT_MpThreat*`, `MSFT_MpPreference`) |
| `src/AppTheme.*` | The 12 `ThemeField` colours, presets, field ↔ registry-name table |
| `src/Settings.*` | `HKCU\Software\ClassicDefenderUI` persistence (theme, custom palette, last tab) |
| `src/ui/Paint.*` | GDI primitives in 96 dpi logical units |
| `src/ui/MainWindow.*` | Window, pages, async plumbing, owner-draw buttons |
| `src/ui/OptionsDialog.*` | Modal options: presets, `ChooseColorW`, exclusions, protection toggles |
| `src/ui/ReportsView.*` | `SysListView32` with themed custom draw (Reports and Quarantine) |
| `src/ui/ProgressWindow.*` | Themed modeless progress window for scans and updates |
| `src/Progress.h` | Thread-safe `ProgressSnapshot` shared by the workers and the window |
| `src/ScanScope.*` | Scan scope walk (files), services, process and startup counters, definition folder size |
| `src/ui/FolderPicker.*` | `IFileOpenDialog` folder picker |
| `src/Async.h` | `PostAsync<Result>()`: worker → heap payload → `WM_APP_*` |

## Non-obvious rules

- **Never call WMI from the UI thread.** Each worker creates a `ComApartment` and
  its own `DefenderClient`; results travel as heap payloads posted to the window
  and are deleted by the receiver, so no worker ever touches `MainWindow`.
- **Two coordinate systems.** The window paints through an anisotropic mapping
  (`SetWindowExtEx(660,452)` / `SetViewportExtEx(client)`), so paint code uses
  plain 96 dpi units and text scales with the monitor. Child controls live in
  device pixels and must be placed with `ui::Scale()`.
- **Theme table is offset-based.** `kThemeFields` maps each `ThemeField` to its
  registry value name and `offsetof(AppTheme, …)`; a `static_assert` keeps the
  table length equal to `ThemeField::Count`. Add a colour in both places.
- **Elevation and the manifest.** MSVC gets
  `/MANIFESTUAC:level='requireAdministrator' uiAccess='false'` from CMake; MinGW
  embeds `src/app.manifest` via `src/manifest.rc` (resource `1/24`). Both carry
  the Common Controls v6 dependency and `dpiAware=true`.
- **Tamper Protection** makes the WMI writes fail with `WBEM_E_ACCESS_DENIED`
  (`0x80041003`); `DefenderClient::LastError()` maps the known WBEM codes to
  actionable text and every caller surfaces it instead of failing silently.
