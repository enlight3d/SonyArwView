// Settings.cpp - the SonyArwView settings window.
//
// Shown when SonyArwOpen.exe is launched with NO file argument (i.e. from the
// Start menu). It lets the user (1) see whether SonyArwView is the .arw default
// and jump to the Settings page to fix it, and (2) choose whether opening an .arw
// shows the preview in Windows Photos or hands the ORIGINAL file to their own
// viewer. The viewer choice is stored via Config.h (the same file the open
// handler reads). Plain Win32 dialog -- no extra dependencies.

#include "Config.h"
#include "resource.h"

#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <string>

namespace {

// Is SonyArwView the current default app for .arw? We read the per-user
// UserChoice ProgId, then that ProgId's open AppUserModelID, and check it belongs
// to our package.
bool IsDefaultForArw() {
    wchar_t progId[256] = {};
    DWORD cb = sizeof(progId);
    if (RegGetValueW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.arw\\UserChoice",
            L"ProgId", RRF_RT_REG_SZ, nullptr, progId, &cb) != ERROR_SUCCESS) {
        return false;
    }
    const std::wstring key = std::wstring(L"Software\\Classes\\") + progId + L"\\shell\\open";
    wchar_t aumid[256] = {};
    cb = sizeof(aumid);
    if (RegGetValueW(HKEY_CURRENT_USER, key.c_str(), L"AppUserModelID",
                     RRF_RT_REG_SZ, nullptr, aumid, &cb) != ERROR_SUCCESS) {
        return false;
    }
    return _wcsnicmp(aumid, L"SonyArwView_", 12) == 0;
}

void UpdateEnableState(HWND dlg) {
    const BOOL useViewer = (IsDlgButtonChecked(dlg, IDC_RADIO_VIEWER) == BST_CHECKED);
    EnableWindow(GetDlgItem(dlg, IDC_VIEWERPATH), useViewer);
    EnableWindow(GetDlgItem(dlg, IDC_BROWSE), useViewer);
}

void OnBrowse(HWND dlg) {
    wchar_t file[MAX_PATH] = {};
    GetDlgItemTextW(dlg, IDC_VIEWERPATH, file, MAX_PATH);
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = dlg;
    ofn.lpstrFilter = L"Programs\0*.exe\0All files\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Choose the app to open .arw files with";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (GetOpenFileNameW(&ofn)) {
        SetDlgItemTextW(dlg, IDC_VIEWERPATH, file);
        CheckRadioButton(dlg, IDC_RADIO_PHOTOS, IDC_RADIO_VIEWER, IDC_RADIO_VIEWER);
        UpdateEnableState(dlg);
    }
}

void OnInit(HWND dlg) {
    SetDlgItemTextW(dlg, IDC_STATUS,
        IsDefaultForArw()
            ? L"\x2713  SonyArwView is your default for .arw."
            : L"\x26A0  SonyArwView is not the default for .arw yet.");

    const std::wstring viewer = cfg::ReadViewerRaw();
    if (!viewer.empty()) {
        CheckRadioButton(dlg, IDC_RADIO_PHOTOS, IDC_RADIO_VIEWER, IDC_RADIO_VIEWER);
        SetDlgItemTextW(dlg, IDC_VIEWERPATH, viewer.c_str());
    } else {
        CheckRadioButton(dlg, IDC_RADIO_PHOTOS, IDC_RADIO_VIEWER, IDC_RADIO_PHOTOS);
    }
    UpdateEnableState(dlg);
}

// Returns true if the dialog should close.
bool OnSave(HWND dlg) {
    if (IsDlgButtonChecked(dlg, IDC_RADIO_VIEWER) == BST_CHECKED) {
        wchar_t path[MAX_PATH] = {};
        GetDlgItemTextW(dlg, IDC_VIEWERPATH, path, MAX_PATH);
        if (path[0] == L'\0') {
            MessageBoxW(dlg,
                L"Pick a viewer with Browse, or choose \"Show the preview in Windows Photos\".",
                L"SonyArwView", MB_OK | MB_ICONINFORMATION);
            return false;
        }
        cfg::WriteViewer(path);
    } else {
        cfg::ClearViewer();
    }
    return true;
}

INT_PTR CALLBACK DlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    (void)lp;
    switch (msg) {
    case WM_INITDIALOG:
        OnInit(dlg);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_RADIO_PHOTOS:
        case IDC_RADIO_VIEWER:
            UpdateEnableState(dlg);
            return TRUE;
        case IDC_BROWSE:
            OnBrowse(dlg);
            return TRUE;
        case IDC_SETDEFAULT:
            ShellExecuteW(dlg, L"open", L"ms-settings:defaultapps", nullptr, nullptr, SW_SHOWNORMAL);
            return TRUE;
        case IDOK:
            if (OnSave(dlg)) EndDialog(dlg, IDOK);
            return TRUE;
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        }
        return FALSE;
    case WM_CLOSE:
        EndDialog(dlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

} // namespace

void ShowSettingsDialog() {
    DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_SETTINGS),
                    nullptr, DlgProc, 0);
}
