#include "OptionsDialog.h"

#include <commdlg.h>

#include <memory>

#include "../Async.h"
#include "../Resource.h"
#include "FolderPicker.h"
#include "Paint.h"

namespace {

struct ExclusionResult {
    std::vector<std::wstring> paths;
    std::wstring error;
};

struct PreferenceResult {
    std::wstring message;
    bool success = false;
    bool reloadExclusions = false;
};

struct State {
    HWND owner = nullptr;
    AppSettings settings;
    DefenderStatus status;
    std::vector<std::wstring> exclusions;
    bool busy = false;
};

State* GetState(HWND dialog) {
    return reinterpret_cast<State*>(GetWindowLongPtrW(dialog, DWLP_USER));
}

void SetStatus(HWND dialog, const std::wstring& text) {
    HWND label = GetDlgItem(dialog, IDC_OPT_STATE);
    if (!label) return;
    SetWindowTextW(label, text.c_str());
    InvalidateRect(label, nullptr, TRUE);
}

void CenterOverOwner(HWND dialog, HWND owner) {
    if (!owner) return;
    RECT ownerRect = {};
    RECT dialogRect = {};
    GetWindowRect(owner, &ownerRect);
    GetWindowRect(dialog, &dialogRect);
    const int width = dialogRect.right - dialogRect.left;
    const int height = dialogRect.bottom - dialogRect.top;
    const int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - width) / 2;
    const int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - height) / 2;
    SetWindowPos(dialog, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void PushPreview(const State& state) {
    if (!state.owner) return;
    auto* theme = new AppTheme(state.settings.theme);
    if (!PostMessageW(state.owner, WM_APP_THEME_PREVIEW, 0, reinterpret_cast<LPARAM>(theme))) delete theme;
}

void SaveAndNotify(HWND dialog, State& state) {
    settings::Save(state.settings);
    if (state.owner) PostMessageW(state.owner, WM_APP_SETTINGS_CHANGED, 0, 0);
    SetStatus(dialog, L"Preferences saved to HKEY_CURRENT_USER\\Software\\ClassicDefenderUI.");
}

void RefreshPresetCombo(HWND dialog, const State& state) {
    HWND combo = GetDlgItem(dialog, IDC_OPT_PRESET);
    if (!combo) return;
    const int index = state.settings.presetIndex;
    SendMessageW(combo, CB_SETCURSEL, index < 0 ? kCustomThemeIndex : index, 0);
}

void RefreshColorList(HWND dialog) {
    HWND list = GetDlgItem(dialog, IDC_OPT_COLOR_LIST);
    if (list) InvalidateRect(list, nullptr, TRUE);
}

void RefreshExclusionList(HWND dialog, const State& state) {
    HWND list = GetDlgItem(dialog, IDC_OPT_EXCL_LIST);
    if (!list) return;
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    for (const std::wstring& path : state.exclusions) SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(path.c_str()));
    if (state.exclusions.empty()) SetStatus(dialog, L"No folder exclusions are configured.");
}

void LoadExclusions(HWND dialog) {
    SetStatus(dialog, L"Reading folder exclusions from MSFT_MpPreference...");
    PostAsync<ExclusionResult>(dialog, WM_APP_EXCLUSIONS_READY, 0, [] {
        ComApartment apartment;
        DefenderClient client;
        ExclusionResult result;
        if (!client.QueryExclusions(result.paths)) result.error = client.LastError();
        return result;
    });
}

void ReportFailure(HWND dialog, const std::wstring& message) {
    SetStatus(dialog, message);
    MessageBoxW(dialog, message.c_str(), L"Windows Defender", MB_OK | MB_ICONWARNING);
}

void ModifyExclusionAsync(HWND dialog, State& state, const std::wstring& path, bool add) {
    if (state.busy) return;
    state.busy = true;
    EnableWindow(GetDlgItem(dialog, IDC_OPT_EXCL_ADD), FALSE);
    EnableWindow(GetDlgItem(dialog, IDC_OPT_EXCL_REMOVE), FALSE);
    SetStatus(dialog, add ? L"Adding the exclusion..." : L"Removing the exclusion...");

    PostAsync<PreferenceResult>(dialog, WM_APP_OPERATION_DONE, 0, [path, add] {
        ComApartment apartment;
        DefenderClient client;
        PreferenceResult result;
        if (add) {
            result.success = client.AddExclusion(path);
            result.message = result.success ? L"Folder added to the Defender exclusions."
                                           : L"Could not add the folder - " + client.LastError();
        } else {
            result.success = client.RemoveExclusion(path);
            result.message = result.success ? L"Folder removed from the Defender exclusions."
                                           : L"Could not remove the folder - " + client.LastError();
        }
        result.reloadExclusions = true;
        return result;
    });
}

void SetProtectionAsync(HWND dialog, State& state, bool realTime, bool enable) {
    if (state.busy) return;
    state.busy = true;
    EnableWindow(GetDlgItem(dialog, IDC_OPT_RTP), FALSE);
    EnableWindow(GetDlgItem(dialog, IDC_OPT_CLOUD), FALSE);
    SetStatus(dialog, L"Writing the preference to Windows Defender...");

    PostAsync<PreferenceResult>(dialog, WM_APP_OPERATION_DONE, 0, [realTime, enable] {
        ComApartment apartment;
        DefenderClient client;
        PreferenceResult result;
        result.success = realTime ? client.SetRealTimeProtection(enable) : client.SetCloudProtection(enable);
        result.message = result.success ? L"Preference applied." : L"Windows Defender refused the change - " + client.LastError();
        return result;
    });
}

void DrawColorItem(const DRAWITEMSTRUCT& item, const State& state) {
    if (item.itemID == static_cast<UINT>(-1)) return;
    const int index = static_cast<int>(item.itemID);
    if (index < 0 || index >= kThemeFieldCount) return;

    HDC dc = item.hDC;
    const bool selected = (item.itemState & ODS_SELECTED) != 0;
    const COLORREF background = GetSysColor(selected ? COLOR_HIGHLIGHT : COLOR_WINDOW);
    const COLORREF foreground = GetSysColor(selected ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT);

    ui::FillSolid(dc, item.rcItem, background);

    RECT swatch = item.rcItem;
    swatch.left += 4;
    swatch.right = swatch.left + ui::Scale(22);
    swatch.top += 2;
    swatch.bottom -= 2;
    ui::FillSolid(dc, swatch, ThemeFieldColor(state.settings.theme, kThemeFields[index].field));
    ui::StrokeRect(dc, swatch, RGB(90, 90, 90));

    RECT text = item.rcItem;
    text.left = swatch.right + ui::Scale(8);
    text.right -= ui::Scale(4);
    HFONT font = reinterpret_cast<HFONT>(SendMessageW(item.hwndItem, WM_GETFONT, 0, 0));
    ui::DrawTextIn(dc, text, kThemeFields[index].label, font, foreground,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (selected) DrawFocusRect(dc, &item.rcItem);
}

void ChooseFieldColor(HWND dialog, State& state) {
    HWND list = GetDlgItem(dialog, IDC_OPT_COLOR_LIST);
    if (!list) return;
    const int index = static_cast<int>(SendMessageW(list, LB_GETCURSEL, 0, 0));
    if (index < 0 || index >= kThemeFieldCount) {
        SetStatus(dialog, L"Select a colour entry first.");
        return;
    }

    const ThemeField field = kThemeFields[index].field;
    CHOOSECOLORW chooser = {};
    chooser.lStructSize = sizeof(chooser);
    chooser.hwndOwner = dialog;
    chooser.rgbResult = ThemeFieldColor(state.settings.theme, field);
    chooser.lpCustColors = state.settings.customColors.data();
    chooser.Flags = CC_FULLOPEN | CC_RGBINIT | CC_ANYCOLOR;

    if (!ChooseColorW(&chooser)) return;

    ThemeFieldRef(state.settings.theme, field) = chooser.rgbResult;
    state.settings.presetIndex = kCustomThemeIndex;
    RefreshPresetCombo(dialog, state);
    RefreshColorList(dialog);
    PushPreview(state);
    SetStatus(dialog, L"Colour updated - press OK or Apply to keep it.");
}

INT_PTR CALLBACK DialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
    State* state = GetState(dialog);

    switch (message) {
        case WM_INITDIALOG: {
            state = reinterpret_cast<State*>(lParam);
            SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(state));

            HWND combo = GetDlgItem(dialog, IDC_OPT_PRESET);
            for (int index = 0; index < kThemePresetCount; ++index)
                SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kThemePresets[index].name));
            SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Custom"));
            RefreshPresetCombo(dialog, *state);

            HWND list = GetDlgItem(dialog, IDC_OPT_COLOR_LIST);
            SendMessageW(list, LB_SETITEMHEIGHT, 0, static_cast<LPARAM>(ui::Scale(18)));
            for (int index = 0; index < kThemeFieldCount; ++index)
                SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kThemeFields[index].label));
            SendMessageW(list, LB_SETCURSEL, 0, 0);

            CheckDlgButton(dialog, IDC_OPT_RTP, state->status.realTimeProtection ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(dialog, IDC_OPT_CLOUD, state->status.cloudProtection ? BST_CHECKED : BST_UNCHECKED);
            if (!state->status.wmiConnected) {
                EnableWindow(GetDlgItem(dialog, IDC_OPT_RTP), FALSE);
                EnableWindow(GetDlgItem(dialog, IDC_OPT_CLOUD), FALSE);
                SetStatus(dialog, L"Defender WMI is unreachable: protection toggles and exclusions are read-only.");
            }

            CenterOverOwner(dialog, state->owner);
            LoadExclusions(dialog);
            return TRUE;
        }

        case WM_DRAWITEM: {
            auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (!state || !item || item->CtlID != IDC_OPT_COLOR_LIST) return FALSE;
            DrawColorItem(*item, *state);
            return TRUE;
        }

        case WM_APP_EXCLUSIONS_READY: {
            std::unique_ptr<ExclusionResult> result(reinterpret_cast<ExclusionResult*>(lParam));
            if (!state) return TRUE;
            state->busy = false;
            EnableWindow(GetDlgItem(dialog, IDC_OPT_EXCL_ADD), TRUE);
            EnableWindow(GetDlgItem(dialog, IDC_OPT_EXCL_REMOVE), TRUE);
            if (!result->error.empty()) {
                SetStatus(dialog, result->error);
                return TRUE;
            }
            state->exclusions = result->paths;
            RefreshExclusionList(dialog, *state);
            if (!state->exclusions.empty()) {
                const std::wstring text =
                    L"ExclusionPath lists " + std::to_wstring(state->exclusions.size()) + L" folders.";
                SetStatus(dialog, text);
            }
            return TRUE;
        }

        case WM_APP_OPERATION_DONE: {
            std::unique_ptr<PreferenceResult> result(reinterpret_cast<PreferenceResult*>(lParam));
            if (!state) return TRUE;
            state->busy = false;
            EnableWindow(GetDlgItem(dialog, IDC_OPT_EXCL_ADD), TRUE);
            EnableWindow(GetDlgItem(dialog, IDC_OPT_EXCL_REMOVE), TRUE);
            if (state->status.wmiConnected) {
                EnableWindow(GetDlgItem(dialog, IDC_OPT_RTP), TRUE);
                EnableWindow(GetDlgItem(dialog, IDC_OPT_CLOUD), TRUE);
            }
            if (result->success) {
                SetStatus(dialog, result->message);
                if (result->reloadExclusions) LoadExclusions(dialog);
            } else {
                ReportFailure(dialog, result->message);
                CheckDlgButton(dialog, IDC_OPT_RTP, state->status.realTimeProtection ? BST_CHECKED : BST_UNCHECKED);
                CheckDlgButton(dialog, IDC_OPT_CLOUD, state->status.cloudProtection ? BST_CHECKED : BST_UNCHECKED);
            }
            return TRUE;
        }

        case WM_COMMAND: {
            if (!state) return FALSE;
            const int id = LOWORD(wParam);
            const int notification = HIWORD(wParam);

            switch (id) {
                case IDC_OPT_PRESET:
                    if (notification == CBN_SELCHANGE) {
                        const int chosen = static_cast<int>(SendMessageW(GetDlgItem(dialog, IDC_OPT_PRESET),
                                                                        CB_GETCURSEL, 0, 0));
                        if (chosen >= 0 && chosen < kThemePresetCount) {
                            state->settings.theme = PresetTheme(chosen);
                            state->settings.presetIndex = chosen;
                            RefreshColorList(dialog);
                            PushPreview(*state);
                        }
                    }
                    return TRUE;

                case IDC_OPT_CHANGE_COLOR:
                    ChooseFieldColor(dialog, *state);
                    return TRUE;

                case IDC_OPT_RESET: {
                    int chosen = static_cast<int>(SendMessageW(GetDlgItem(dialog, IDC_OPT_PRESET), CB_GETCURSEL, 0, 0));
                    if (chosen < 0 || chosen >= kThemePresetCount) chosen = 0;
                    state->settings.theme = PresetTheme(chosen);
                    state->settings.presetIndex = chosen;
                    RefreshPresetCombo(dialog, *state);
                    RefreshColorList(dialog);
                    PushPreview(*state);
                    SetStatus(dialog, L"Preset colours restored.");
                    return TRUE;
                }

                case IDC_OPT_EXCL_ADD: {
                    std::wstring folder;
                    if (!ui::PickFolder(dialog, folder)) return TRUE;
                    ModifyExclusionAsync(dialog, *state, folder, true);
                    return TRUE;
                }

                case IDC_OPT_EXCL_REMOVE: {
                    HWND list = GetDlgItem(dialog, IDC_OPT_EXCL_LIST);
                    const int index = static_cast<int>(SendMessageW(list, LB_GETCURSEL, 0, 0));
                    if (index < 0) {
                        SetStatus(dialog, L"Select a folder in the list first.");
                        return TRUE;
                    }
                    const int length = static_cast<int>(SendMessageW(list, LB_GETTEXTLEN, index, 0));
                    std::wstring path(static_cast<size_t>(length) + 1, L'\0');
                    SendMessageW(list, LB_GETTEXT, index, reinterpret_cast<LPARAM>(path.data()));
                    path.resize(static_cast<size_t>(length));
                    ModifyExclusionAsync(dialog, *state, path, false);
                    return TRUE;
                }

                case IDC_OPT_RTP:
                    if (notification == BN_CLICKED) {
                        const bool enable = IsDlgButtonChecked(dialog, IDC_OPT_RTP) == BST_CHECKED;
                        SetProtectionAsync(dialog, *state, true, enable);
                    }
                    return TRUE;

                case IDC_OPT_CLOUD:
                    if (notification == BN_CLICKED) {
                        const bool enable = IsDlgButtonChecked(dialog, IDC_OPT_CLOUD) == BST_CHECKED;
                        SetProtectionAsync(dialog, *state, false, enable);
                    }
                    return TRUE;

                case ID_OPT_APPLY:
                    SaveAndNotify(dialog, *state);
                    return TRUE;

                case IDOK:
                    SaveAndNotify(dialog, *state);
                    EndDialog(dialog, IDOK);
                    return TRUE;

                case IDCANCEL:
                    EndDialog(dialog, IDCANCEL);
                    return TRUE;

                default:
                    return FALSE;
            }
        }

        default:
            return FALSE;
    }
}

}  // namespace

void OptionsDialog::Show(HWND owner, HINSTANCE instance, const AppSettings& settings, const DefenderStatus& status) {
    State state;
    state.owner = owner;
    state.settings = settings;
    state.status = status;

    DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_OPTIONS), owner, DialogProc, reinterpret_cast<LPARAM>(&state));

    if (owner) PostMessageW(owner, WM_APP_PREVIEW_END, 0, 0);
}
