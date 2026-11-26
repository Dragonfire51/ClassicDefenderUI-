#pragma once

// Application private messages. Payloads are heap objects owned by the receiver.
#define WM_APP_STATUS_READY (WM_APP + 1)
#define WM_APP_THREATS_READY (WM_APP + 2)
#define WM_APP_OPERATION_DONE (WM_APP + 3)
#define WM_APP_THEME_PREVIEW (WM_APP + 4)
#define WM_APP_PREVIEW_END (WM_APP + 5)
#define WM_APP_SETTINGS_CHANGED (WM_APP + 6)
#define WM_APP_EXCLUSIONS_BUSY (WM_APP + 7)
#define WM_APP_EXCLUSIONS_READY (WM_APP + 8)

// Top bar commands.
#define IDC_BTN_LIVEUPDATE 1001
#define IDC_BTN_OPTIONS 1002
#define IDC_BTN_HELP 1003

// Sidebar navigation.
#define IDC_NAV_STATUS 1101
#define IDC_NAV_SCAN 1102
#define IDC_NAV_REPORTS 1103
#define IDC_NAV_QUARANTINE 1104

// Per-tab controls.
#define IDC_SCAN_QUICK 1201
#define IDC_SCAN_FULL 1202
#define IDC_SCAN_CUSTOM 1203
#define IDC_REPORTS_LIST 1211
#define IDC_REPORTS_REFRESH 1212
#define IDC_QUAR_LIST 1221
#define IDC_QUAR_REMOVE 1222
#define IDC_QUAR_RESTORE 1223
#define IDC_QUAR_EXCLUDE 1224
#define IDC_QUAR_REANALYZE 1225

// Options dialog template and controls.
#define IDD_OPTIONS 2000
#define IDC_OPT_PRESET 2001
#define IDC_OPT_COLOR_LIST 2002
#define IDC_OPT_CHANGE_COLOR 2003
#define IDC_OPT_RESET 2004
#define IDC_OPT_EXCL_LIST 2005
#define IDC_OPT_EXCL_ADD 2006
#define IDC_OPT_EXCL_REMOVE 2007
#define IDC_OPT_RTP 2008
#define IDC_OPT_CLOUD 2009
#define IDC_OPT_HINT 2010
#define ID_OPT_APPLY 2011
#define IDC_OPT_STATE 2012

#define IDI_APPICON 3001
