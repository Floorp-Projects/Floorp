/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Toolkit Crash Reporter
 *
 * The Initial Developer of the Original Code is
 * Ted Mielczarek <ted.mielczarek@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ted Mielczarek <ted.mielczarek@gmail.com>
 *   Dave Camp <dcamp@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif

#include "crashreporter.h"

#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <set>
#include "resource.h"
#include "client/windows/sender/crash_report_sender.h"
#include "common/windows/string_utils-inl.h"

#define CRASH_REPORTER_VALUE L"Enabled"
#define SUBMIT_REPORT_VALUE  L"SubmitReport"
#define EMAIL_ME_VALUE       L"EmailMe"
#define EMAIL_VALUE          L"Email"
#define MAX_EMAIL_LENGTH     1024

#define WM_UPLOADCOMPLETE WM_APP

using std::string;
using std::wstring;
using std::map;
using std::vector;
using std::set;
using std::ios;
using std::ifstream;
using std::ofstream;

using namespace CrashReporter;

typedef struct {
  HWND hDlg;
  wstring dumpFile;
  map<wstring,wstring> queryParameters;
  wstring sendURL;

  wstring serverResponse;
} SendThreadData;

static HANDLE               gThreadHandle;
static SendThreadData       gSendData = { 0, };
static vector<string>       gRestartArgs;
static map<wstring,wstring> gQueryParameters;
static wstring              gCrashReporterKey(L"Software\\Mozilla\\Crash Reporter");

// When vertically resizing the dialog, these items should move down
static set<UINT> gAttachedBottom;

// Default set of items for gAttachedBottom
static const UINT kDefaultAttachedBottom[] = {
  IDC_VIEWREPORTCHECK,
  IDC_VIEWREPORTTEXT,
  IDC_SUBMITCRASHCHECK,
  IDC_EMAILMECHECK,
  IDC_EMAILTEXT,
  IDC_CLOSEBUTTON,
  IDC_RESTARTBUTTON,
};

static wstring UTF8ToWide(const string& utf8, bool *success = 0);
static DWORD WINAPI SendThreadProc(LPVOID param);

static wstring Str(const char* key)
{
  return UTF8ToWide(gStrings[key]);
}

/* === win32 helper functions === */

static void DoInitCommonControls()
{
  INITCOMMONCONTROLSEX ic;
  ic.dwSize = sizeof(INITCOMMONCONTROLSEX);
  ic.dwICC = ICC_PROGRESS_CLASS;
  InitCommonControlsEx(&ic);
  // also get the rich edit control
  LoadLibrary(L"riched20.dll");
}

static bool GetBoolValue(HKEY hRegKey, LPCTSTR valueName, DWORD* value)
{
  DWORD type, dataSize;
  dataSize = sizeof(DWORD);
  if (RegQueryValueEx(hRegKey, valueName, NULL, &type, (LPBYTE)value, &dataSize) == ERROR_SUCCESS
    && type == REG_DWORD)
    return true;

  return false;
}

static bool CheckBoolKey(const wchar_t* key,
                         const wchar_t* valueName,
                         bool* enabled)
{
  *enabled = false;
  bool found = false;
  HKEY hRegKey;
  DWORD val;
  // see if our reg key is set globally
  if (RegOpenKey(HKEY_LOCAL_MACHINE, key, &hRegKey) == ERROR_SUCCESS) {
    if (GetBoolValue(hRegKey, valueName, &val)) {
      *enabled = (val == 1);
      found = true;
    }
    RegCloseKey(hRegKey);
  } else {
    // look for it in user settings
    if (RegOpenKey(HKEY_CURRENT_USER, key, &hRegKey) == ERROR_SUCCESS) {
      if (GetBoolValue(hRegKey, valueName, &val)) {
        *enabled = (val == 1);
        found = true;
      }
      RegCloseKey(hRegKey);
    }
  }

  return found;
}

static void SetBoolKey(const wchar_t* key, const wchar_t* value, bool enabled)
{
  HKEY hRegKey;
  if (RegCreateKey(HKEY_CURRENT_USER, key, &hRegKey) == ERROR_SUCCESS) {
    DWORD data = (enabled ? 1 : 0);
    RegSetValueEx(hRegKey, value, 0, REG_DWORD, (LPBYTE)&data, sizeof(data));
    RegCloseKey(hRegKey);
  }
}

static bool GetStringValue(HKEY hRegKey, LPCTSTR valueName, wstring& value)
{
  DWORD type, dataSize;
  wchar_t buf[2048];
  dataSize = sizeof(buf);
  if (RegQueryValueEx(hRegKey, valueName, NULL, &type, (LPBYTE)buf, &dataSize) == ERROR_SUCCESS
      && type == REG_SZ) {
    value = buf;
    return true;
  }

  return false;
}

static bool GetStringKey(const wchar_t* key,
                         const wchar_t* valueName,
                         wstring& value)
{
  value = L"";
  bool found = false;
  HKEY hRegKey;
  // see if our reg key is set globally
  if (RegOpenKey(HKEY_LOCAL_MACHINE, key, &hRegKey) == ERROR_SUCCESS) {
    if (GetStringValue(hRegKey, valueName, value)) {
      found = true;
    }
    RegCloseKey(hRegKey);
  } else {
    // look for it in user settings
    if (RegOpenKey(HKEY_CURRENT_USER, key, &hRegKey) == ERROR_SUCCESS) {
      if (GetStringValue(hRegKey, valueName, value)) {
        found = true;
      }
      RegCloseKey(hRegKey);
    }
  }

  return found;
}

static void SetStringKey(const wchar_t* key,
                         const wchar_t* valueName,
                         const wstring& value)
{
  HKEY hRegKey;
  if (RegCreateKey(HKEY_CURRENT_USER, key, &hRegKey) == ERROR_SUCCESS) {
    RegSetValueEx(hRegKey, valueName, 0, REG_SZ,
                  (LPBYTE)value.c_str(),
                  (value.length() + 1) * sizeof(wchar_t));
    RegCloseKey(hRegKey);
  }
}

// Gets the position of a window relative to another window's client area
static void GetRelativeRect(HWND hwnd, HWND hwndParent, RECT* r)
{
  GetWindowRect(hwnd, r);
  ScreenToClient(hwndParent, (POINT*)&(r->left));
  ScreenToClient(hwndParent, (POINT*)&(r->right));
}

static void SetDlgItemVisible(HWND hwndDlg, UINT item, bool visible)
{
  HWND hwnd = GetDlgItem(hwndDlg, item);
  LONG style = GetWindowLong(hwnd, GWL_STYLE);
  if (visible)
    style |= WS_VISIBLE;
  else
    style &= ~WS_VISIBLE;

  SetWindowLong(hwnd, GWL_STYLE, style);
}

static void SetDlgItemDisabled(HWND hwndDlg, UINT item, bool disabled)
{
  HWND hwnd = GetDlgItem(hwndDlg, item);
  LONG style = GetWindowLong(hwnd, GWL_STYLE);
  if (!disabled)
    style |= WS_DISABLED;
  else
    style &= ~WS_DISABLED;

  SetWindowLong(hwnd, GWL_STYLE, style);
}

/* === Crash Reporting Dialog === */

static void StretchDialog(HWND hwndDlg, int ydiff)
{
  RECT r;
  GetWindowRect(hwndDlg, &r);
  r.bottom += ydiff;
  MoveWindow(hwndDlg, r.left, r.top,
             r.right - r.left, r.bottom - r.top, TRUE);
}

static void ReflowDialog(HWND hwndDlg, int ydiff)
{
  // Move items attached to the bottom down/up by as much as
  // the window resize
  for (set<UINT>::const_iterator item = gAttachedBottom.begin();
       item != gAttachedBottom.end();
       item++) {
    RECT r;
    HWND hwnd = GetDlgItem(hwndDlg, *item);
    GetRelativeRect(hwnd, hwndDlg, &r);
    r.top += ydiff;
    r.bottom += ydiff;
    MoveWindow(hwnd, r.left, r.top,
               r.right - r.left, r.bottom - r.top, TRUE);
  }
}

static DWORD WINAPI SendThreadProc(LPVOID param)
{
  bool finishedOk;
  SendThreadData* td = (SendThreadData*)param;

  if (td->sendURL.empty()) {
    finishedOk = false;
  } else {
    google_breakpad::CrashReportSender sender(L"");
    finishedOk = (sender.SendCrashReport(td->sendURL,
                                         td->queryParameters,
                                         td->dumpFile,
                                         &td->serverResponse)
                  == google_breakpad::RESULT_SUCCEEDED);
  }

  PostMessage(td->hDlg, WM_UPLOADCOMPLETE, finishedOk ? 1 : 0, 0);

  return 0;
}

static void EndCrashReporterDialog(HWND hwndDlg, int code)
{
  // Save the current values to the registry
  wchar_t email[MAX_EMAIL_LENGTH];
  GetDlgItemText(hwndDlg, IDC_EMAILTEXT, email, sizeof(email));
  SetStringKey(gCrashReporterKey.c_str(), EMAIL_VALUE, email);

  SetBoolKey(gCrashReporterKey.c_str(), EMAIL_ME_VALUE,
             IsDlgButtonChecked(hwndDlg, IDC_EMAILMECHECK) != 0);
  SetBoolKey(gCrashReporterKey.c_str(), SUBMIT_REPORT_VALUE,
             IsDlgButtonChecked(hwndDlg, IDC_SUBMITREPORTCHECK) != 0);

  EndDialog(hwndDlg, code);
}

static void MaybeSendReport(HWND hwndDlg)
{
  if (!IsDlgButtonChecked(hwndDlg, IDC_SUBMITREPORTCHECK)) {
    EndCrashReporterDialog(hwndDlg, 0);
    return;
  }

  gThreadHandle = NULL;
  gSendData.hDlg = hwndDlg;
  gSendData.queryParameters = gQueryParameters;

  gThreadHandle = CreateThread(NULL, 0, SendThreadProc, &gSendData, 0, NULL);
}

static void RestartApplication()
{
  wstring cmdLine;

  for (unsigned int i = 0; i < gRestartArgs.size(); i++) {
    cmdLine += L"\"" + UTF8ToWide(gRestartArgs[i]) + L"\" ";
  }

  STARTUPINFO si;
  PROCESS_INFORMATION pi;

  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_SHOWNORMAL;
  ZeroMemory(&pi, sizeof(pi));

  if (CreateProcess(NULL, (LPWSTR)cmdLine.c_str(), NULL, NULL, FALSE, 0,
                    NULL, NULL, &si, &pi)) {
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  }
}

static void ShowReportInfo(HWND hwndDlg)
{
  wstring description;

  for (map<wstring,wstring>::const_iterator i = gQueryParameters.begin();
       i != gQueryParameters.end();
       i++) {
    description += i->first;
    description += L": ";
    description += i->second;
    description += L"\n";
  }

  description += L"\n";
  description += Str(ST_EXTRAREPORTINFO);

  SetDlgItemText(hwndDlg, IDC_VIEWREPORTTEXT, description.c_str());
}

static void ShowHideReport(HWND hwndDlg)
{
  // When resizing the dialog to show the report, these items should
  // stay put
  gAttachedBottom.erase(IDC_VIEWREPORTCHECK);
  gAttachedBottom.erase(IDC_VIEWREPORTTEXT);

  RECT r;
  HWND hwnd = GetDlgItem(hwndDlg, IDC_VIEWREPORTTEXT);

  GetWindowRect(hwnd, &r);
  int diff = (r.bottom - r.top) + 10;
  if (IsDlgButtonChecked(hwndDlg, IDC_VIEWREPORTCHECK)) {
    SetDlgItemVisible(hwndDlg, IDC_VIEWREPORTTEXT, true);
  } else {
    SetDlgItemVisible(hwndDlg, IDC_VIEWREPORTTEXT, false);
    diff = -diff;
  }

  StretchDialog(hwndDlg, diff);

  // set these back to normal
  gAttachedBottom.insert(IDC_VIEWREPORTCHECK);
  gAttachedBottom.insert(IDC_VIEWREPORTTEXT);
}

static void UpdateEmail(HWND hwndDlg)
{
  if (IsDlgButtonChecked(hwndDlg, IDC_EMAILMECHECK)) {
    wchar_t email[MAX_EMAIL_LENGTH];
    GetDlgItemText(hwndDlg, IDC_EMAILTEXT, email, sizeof(email));
    gQueryParameters[L"Email"] = email;
  } else {
    gQueryParameters.erase(L"Email");
  }
}

static BOOL CALLBACK CrashReporterDialogProc(HWND hwndDlg, UINT message,
                                             WPARAM wParam, LPARAM lParam)
{
  static int sHeight = 0;

  bool success;
  bool enabled;

  switch (message) {
  case WM_INITDIALOG: {
    RECT r;
    GetClientRect(hwndDlg, &r);
    sHeight = r.bottom - r.top;

    SetWindowText(hwndDlg, Str(ST_CRASHREPORTERTITLE).c_str());

    SendDlgItemMessage(hwndDlg, IDC_DESCRIPTIONTEXT,
                       EM_SETEVENTMASK, (WPARAM)NULL,
                       ENM_REQUESTRESIZE);
    wstring description = Str(ST_CRASHREPORTERHEADER);
    description += L"\n\n";
    description += Str(ST_CRASHREPORTERDESCRIPTION);
    SetDlgItemText(hwndDlg, IDC_DESCRIPTIONTEXT, description.c_str());

    // Make the title bold.
    CHARFORMAT fmt = { 0, };
    fmt.cbSize = sizeof(fmt);
    fmt.dwMask = CFM_BOLD;
    fmt.dwEffects = CFE_BOLD;
    SendDlgItemMessage(hwndDlg, IDC_DESCRIPTIONTEXT, EM_SETSEL,
                       0, Str(ST_CRASHREPORTERHEADER).length());
    SendDlgItemMessage(hwndDlg, IDC_DESCRIPTIONTEXT, EM_SETCHARFORMAT,
                       SCF_SELECTION, (LPARAM)&fmt);
    SendDlgItemMessage(hwndDlg, IDC_DESCRIPTIONTEXT, EM_SETSEL, 0, 0);
    SendDlgItemMessage(hwndDlg, IDC_DESCRIPTIONTEXT,
                       EM_SETTARGETDEVICE, (WPARAM)NULL, 0);

    SetDlgItemText(hwndDlg, IDC_VIEWREPORTCHECK, Str(ST_VIEWREPORT).c_str());
    SendDlgItemMessage(hwndDlg, IDC_VIEWREPORTTEXT,
                       EM_SETTARGETDEVICE, (WPARAM)NULL, 0);


    SetDlgItemText(hwndDlg, IDC_SUBMITREPORTCHECK,
                   Str(ST_CHECKSUBMIT).c_str());
    if (CheckBoolKey(gCrashReporterKey.c_str(),
                     SUBMIT_REPORT_VALUE, &enabled) &&
        !enabled) {
      CheckDlgButton(hwndDlg, IDC_SUBMITREPORTCHECK, BST_UNCHECKED);
      EnableWindow(GetDlgItem(hwndDlg, IDC_EMAILMECHECK), enabled);
      EnableWindow(GetDlgItem(hwndDlg, IDC_EMAILTEXT), enabled);
    } else {
      CheckDlgButton(hwndDlg, IDC_SUBMITREPORTCHECK, BST_CHECKED);
    }

    SetDlgItemText(hwndDlg, IDC_EMAILMECHECK, Str(ST_CHECKEMAIL).c_str());
    if (CheckBoolKey(gCrashReporterKey.c_str(), EMAIL_ME_VALUE, &enabled) &&
        enabled) {
      CheckDlgButton(hwndDlg, IDC_EMAILMECHECK, BST_CHECKED);
    } else {
      CheckDlgButton(hwndDlg, IDC_EMAILMECHECK, BST_UNCHECKED);
    }

    wstring email;
    if (GetStringKey(gCrashReporterKey.c_str(), EMAIL_VALUE, email)) {
      SetDlgItemText(hwndDlg, IDC_EMAILTEXT, email.c_str());
    }

    SetDlgItemText(hwndDlg, IDC_CLOSEBUTTON, Str(ST_CLOSE).c_str());

    if (gRestartArgs.size() > 0) {
      SetDlgItemText(hwndDlg, IDC_RESTARTBUTTON, Str(ST_RESTART).c_str());
    } else {
      // No restart arguments, move the close button over to the side
      // and hide the restart button
      SetDlgItemVisible(hwndDlg, IDC_RESTARTBUTTON, false);

      RECT closeRect;
      HWND hwndClose = GetDlgItem(hwndDlg, IDC_CLOSEBUTTON);
      GetRelativeRect(hwndClose, hwndDlg, &closeRect);

      RECT restartRect;
      HWND hwndRestart = GetDlgItem(hwndDlg, IDC_RESTARTBUTTON);
      GetRelativeRect(hwndRestart, hwndDlg, &restartRect);

      int size = closeRect.right - closeRect.left;
      closeRect.right = restartRect.right;
      closeRect.left = closeRect.right - size;

      MoveWindow(hwndClose, closeRect.left, closeRect.top,
                 closeRect.right - closeRect.left,
                 closeRect.bottom - closeRect.top,
                 TRUE);
    }
    UpdateEmail(hwndDlg);
    ShowReportInfo(hwndDlg);

    SetFocus(GetDlgItem(hwndDlg, IDC_EMAILTEXT));
    return FALSE;
  }
  case WM_SIZE: {
    ReflowDialog(hwndDlg, HIWORD(lParam) - sHeight);
    sHeight = HIWORD(lParam);
    InvalidateRect(hwndDlg, NULL, TRUE);
    return FALSE;
  }
  case WM_NOTIFY: {
    NMHDR* notification = reinterpret_cast<NMHDR*>(lParam);
    if (notification->code == EN_REQUESTRESIZE) {
      // Resizing the rich edit control to fit the description text.
      REQRESIZE* reqresize = reinterpret_cast<REQRESIZE*>(lParam);
      RECT newSize = reqresize->rc;
      RECT oldSize;
      GetRelativeRect(notification->hwndFrom, hwndDlg, &oldSize);

      // resize the text box as requested
      MoveWindow(notification->hwndFrom, newSize.left, newSize.top,
                 newSize.right - newSize.left, newSize.bottom - newSize.top,
                 TRUE);

      // Resize the dialog to fit (the WM_SIZE handler will move the controls)
      StretchDialog(hwndDlg, newSize.bottom - oldSize.bottom);
    }
    return FALSE;
  }
  case WM_COMMAND: {
    if (HIWORD(wParam) == BN_CLICKED) {
      switch(LOWORD(wParam)) {
      case IDC_VIEWREPORTCHECK:
        ShowHideReport(hwndDlg);
        break;
      case IDC_SUBMITREPORTCHECK:
        enabled = (IsDlgButtonChecked(hwndDlg, IDC_SUBMITREPORTCHECK) != 0);
        EnableWindow(GetDlgItem(hwndDlg, IDC_EMAILMECHECK), enabled);
        EnableWindow(GetDlgItem(hwndDlg, IDC_EMAILTEXT), enabled);
        break;
      case IDC_EMAILMECHECK:
        UpdateEmail(hwndDlg);
        ShowReportInfo(hwndDlg);
        break;
      case IDC_CLOSEBUTTON:
        // Hide the dialog after "closing", but leave it around to coordinate
        // with the upload thread
        ShowWindow(hwndDlg, SW_HIDE);
        MaybeSendReport(hwndDlg);
        break;
      case IDC_RESTARTBUTTON:
        // Hide the dialog after "closing", but leave it around to coordinate
        // with the upload thread
        ShowWindow(hwndDlg, SW_HIDE);
        RestartApplication();
        MaybeSendReport(hwndDlg);
        break;
      }
    } else if (HIWORD(wParam) == EN_CHANGE) {
      switch(LOWORD(wParam)) {
      case IDC_EMAILTEXT:
        wchar_t email[MAX_EMAIL_LENGTH];
        if (GetDlgItemText(hwndDlg, IDC_EMAILTEXT, email, sizeof(email)) > 0)
          CheckDlgButton(hwndDlg, IDC_EMAILMECHECK, BST_CHECKED);
        else
          CheckDlgButton(hwndDlg, IDC_EMAILMECHECK, BST_UNCHECKED);
        UpdateEmail(hwndDlg);
        ShowReportInfo(hwndDlg);
      }
    }

    return FALSE;
  }
  case WM_UPLOADCOMPLETE: {
    WaitForSingleObject(gThreadHandle, INFINITE);
    success = (wParam == 1);
    SendCompleted(success, WideToUTF8(gSendData.serverResponse));
    if (!success) {
      MessageBox(hwndDlg,
                 Str(ST_SUBMITFAILED).c_str(),
                 Str(ST_CRASHREPORTERTITLE).c_str(),
                 MB_OK | MB_ICONERROR);
    }
    EndCrashReporterDialog(hwndDlg, success ? 1 : 0);
    return TRUE;
  }
  }
  return FALSE;
}

static wstring UTF8ToWide(const string& utf8, bool *success)
{
  wchar_t* buffer = NULL;
  int buffer_size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                                        -1, NULL, 0);
  if(buffer_size == 0) {
    if (success)
      *success = false;
    return L"";
  }

  buffer = new wchar_t[buffer_size];
  if(buffer == NULL) {
    if (success)
      *success = false;
    return L"";
  }

  MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                      -1, buffer, buffer_size);
  wstring str = buffer;
  delete [] buffer;

  if (success)
    *success = true;

  return str;
}

string WideToUTF8(const wstring& wide, bool* success)
{
  char* buffer = NULL;
  int buffer_size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                                        -1, NULL, 0, NULL, NULL);
  if(buffer_size == 0) {
    if (success)
      *success = false;
    return "";
  }

  buffer = new char[buffer_size];
  if(buffer == NULL) {
    if (success)
      *success = false;
    return "";
  }

  WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                      -1, buffer, buffer_size, NULL, NULL);
  string utf8 = buffer;
  delete [] buffer;

  if (success)
    *success = true;

  return utf8;
}

/* === Crashreporter UI Functions === */

bool UIInit()
{
  for (int i = 0; i < sizeof(kDefaultAttachedBottom) / sizeof(UINT); i++) {
    gAttachedBottom.insert(kDefaultAttachedBottom[i]);
  }

  DoInitCommonControls();
  return true;
}

void UIShutdown()
{
}

void UIShowDefaultUI()
{
  MessageBox(NULL, Str(ST_CRASHREPORTERDEFAULT).c_str(),
             L"Crash Reporter",
             MB_OK | MB_ICONSTOP);
}

void UIShowCrashUI(const string& dumpFile,
                   const StringTable& queryParameters,
                   const string& sendURL,
                   const vector<string>& restartArgs)
{
  gSendData.hDlg = NULL;
  gSendData.dumpFile = UTF8ToWide(dumpFile);
  gSendData.sendURL = UTF8ToWide(sendURL);

  for (StringTable::const_iterator i = queryParameters.begin();
       i != queryParameters.end();
       i++) {
    gQueryParameters[UTF8ToWide(i->first)] = UTF8ToWide(i->second);
  }

  if (gQueryParameters.find(L"Vendor") != gQueryParameters.end()) {
    gCrashReporterKey = L"Software\\";
    if (!gQueryParameters[L"Vendor"].empty()) {
      gCrashReporterKey += gQueryParameters[L"Vendor"] + L"\\";
    }
    gCrashReporterKey += gQueryParameters[L"Name"] + L"\\Crash Reporter";
  }

  gRestartArgs = restartArgs;

  DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_SENDDIALOG), NULL,
                 (DLGPROC)CrashReporterDialogProc, 0);
}

void UIError_impl(const string& message)
{
  wstring title = Str(ST_CRASHREPORTERTITLE);
  if (title.empty())
    title = L"Crash Reporter Error";

  MessageBox(NULL, UTF8ToWide(message).c_str(), title.c_str(),
             MB_OK | MB_ICONSTOP);
}

bool UIGetIniPath(string& path)
{
  wchar_t fileName[MAX_PATH];
  if (GetModuleFileName(NULL, fileName, MAX_PATH)) {
    // get crashreporter ini
    wchar_t* s = wcsrchr(fileName, '.');
    if (s) {
      wcscpy(s, L".ini");
      path = WideToUTF8(fileName);
      return true;
    }
  }

  return false;
}

bool UIGetSettingsPath(const string& vendor,
                       const string& product,
                       string& settings_path)
{
  wchar_t path[MAX_PATH];
  if(SUCCEEDED(SHGetFolderPath(NULL,
                               CSIDL_APPDATA,
                               NULL,
                               0,
                               path)))  {
    if (!vendor.empty()) {
      PathAppend(path, UTF8ToWide(vendor).c_str());
    }
    PathAppend(path, UTF8ToWide(product).c_str());
    PathAppend(path, L"Crash Reports");
    settings_path = WideToUTF8(path);
    return true;
  }
  return false;
}

bool UIEnsurePathExists(const string& path)
{
  if (CreateDirectory(UTF8ToWide(path).c_str(), NULL) == 0) {
    if (GetLastError() != ERROR_ALREADY_EXISTS)
      return false;
  }

  return true;
}

bool UIFileExists(const string& path)
{
  DWORD attrs = GetFileAttributes(UTF8ToWide(path).c_str());
  return (attrs != INVALID_FILE_ATTRIBUTES);
}

bool UIMoveFile(const string& oldfile, const string& newfile)
{
  if (oldfile == newfile)
    return true;

  return MoveFile(UTF8ToWide(oldfile).c_str(), UTF8ToWide(newfile).c_str())
    == TRUE;
}

bool UIDeleteFile(const string& oldfile)
{
  return DeleteFile(UTF8ToWide(oldfile).c_str()) == TRUE;
}

ifstream* UIOpenRead(const string& filename)
{
  // adapted from breakpad's src/common/windows/http_upload.cc

  // The "open" method on pre-MSVC8 ifstream implementations doesn't accept a
  // wchar_t* filename, so use _wfopen directly in that case.  For VC8 and
  // later, _wfopen has been deprecated in favor of _wfopen_s, which does
  // not exist in earlier versions, so let the ifstream open the file itself.
#if _MSC_VER >= 1400  // MSVC 2005/8
  ifstream* file = new ifstream();
  file->open(UTF8ToWide(filename).c_str(), ios::in);
#else  // _MSC_VER >= 1400
  ifstream* file = new ifstream(_wfopen(UTF8ToWide(filename).c_str(), L"r"));
#endif  // _MSC_VER >= 1400

  return file;
}

ofstream* UIOpenWrite(const string& filename)
{
  // adapted from breakpad's src/common/windows/http_upload.cc

  // The "open" method on pre-MSVC8 ifstream implementations doesn't accept a
  // wchar_t* filename, so use _wfopen directly in that case.  For VC8 and
  // later, _wfopen has been deprecated in favor of _wfopen_s, which does
  // not exist in earlier versions, so let the ifstream open the file itself.
#if _MSC_VER >= 1400  // MSVC 2005/8
  ofstream* file = new ofstream();
  file->open(UTF8ToWide(filename).c_str(), ios::out);
#else  // _MSC_VER >= 1400
  ofstream* file = new ofstream(_wfopen(UTF8ToWide(filename).c_str(), L"w"));
#endif  // _MSC_VER >= 1400

  return file;
}
