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
#define INCLUDE_URL_VALUE    L"IncludeURL"
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
static wstring              gURLParameter;

// When vertically resizing the dialog, these items should move down
static set<UINT> gAttachedBottom;

// Default set of items for gAttachedBottom
static const UINT kDefaultAttachedBottom[] = {
  IDC_SUBMITREPORTCHECK,
  IDC_VIEWREPORTBUTTON,
  IDC_COMMENTTEXT,
  IDC_INCLUDEURLCHECK,
  IDC_EMAILMECHECK,
  IDC_EMAILTEXT,
  IDC_PROGRESSTEXT,
  IDC_THROBBER,
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

static string FormatLastError()
{
  DWORD err = GetLastError();
  LPWSTR s;
  string message = "Crash report submission failed: ";
  // odds are it's a WinInet error
  HANDLE hInetModule = GetModuleHandle(L"WinInet.dll");
  if(FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                   FORMAT_MESSAGE_FROM_SYSTEM |
                   FORMAT_MESSAGE_FROM_HMODULE,
                   hInetModule,
                   err,
                   0,
                   (LPWSTR)&s,
                   0,
                   NULL) != 0) {
    message += WideToUTF8(s, NULL);
    LocalFree(s);
    // strip off any trailing newlines
    string::size_type n = message.find_last_not_of("\r\n");
    if (n < message.size() - 1) {
      message.erase(n+1);
    }
  }
  else {
    char buf[64];
    sprintf(buf, "Unknown error, error code: 0x%08x", err);
    message += buf;
  }
  return message;
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

  ShowWindow(hwnd, visible ? SW_SHOW : SW_HIDE);
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
    LogMessage("No server URL, not sending report");
  } else {
    google_breakpad::CrashReportSender sender(L"");
    finishedOk = (sender.SendCrashReport(td->sendURL,
                                         td->queryParameters,
                                         td->dumpFile,
                                         &td->serverResponse)
                  == google_breakpad::RESULT_SUCCEEDED);
    if (finishedOk) {
      LogMessage("Crash report submitted successfully");
    }
    else {
      // get an error string and print it to the log
      //XXX: would be nice to get the HTTP status code here, filed:
      // http://code.google.com/p/google-breakpad/issues/detail?id=220
      LogMessage(FormatLastError());
    }
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

  SetBoolKey(gCrashReporterKey.c_str(), INCLUDE_URL_VALUE,
             IsDlgButtonChecked(hwndDlg, IDC_INCLUDEURLCHECK) != 0);
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

  // disable all the form controls
  EnableWindow(GetDlgItem(hwndDlg, IDC_SUBMITREPORTCHECK), false);
  EnableWindow(GetDlgItem(hwndDlg, IDC_VIEWREPORTBUTTON), false);
  EnableWindow(GetDlgItem(hwndDlg, IDC_COMMENTTEXT), false);
  EnableWindow(GetDlgItem(hwndDlg, IDC_INCLUDEURLCHECK), false);
  EnableWindow(GetDlgItem(hwndDlg, IDC_EMAILMECHECK), false);
  EnableWindow(GetDlgItem(hwndDlg, IDC_EMAILTEXT), false);
  EnableWindow(GetDlgItem(hwndDlg, IDC_CLOSEBUTTON), false);
  EnableWindow(GetDlgItem(hwndDlg, IDC_RESTARTBUTTON), false);

  SetDlgItemText(hwndDlg, IDC_PROGRESSTEXT, Str(ST_REPORTDURINGSUBMIT).c_str());
  // start throbber
  // play entire AVI, and loop
  Animate_Play(GetDlgItem(hwndDlg, IDC_THROBBER), 0, -1, -1);
  SetDlgItemVisible(hwndDlg, IDC_THROBBER, true);
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

static void UpdateURL(HWND hwndDlg)
{
  if (IsDlgButtonChecked(hwndDlg, IDC_INCLUDEURLCHECK)) {
    gQueryParameters[L"URL"] = gURLParameter;
  } else {
    gQueryParameters.erase(L"URL");
  }
}

static void UpdateEmail(HWND hwndDlg)
{
  if (IsDlgButtonChecked(hwndDlg, IDC_EMAILMECHECK)) {
    wchar_t email[MAX_EMAIL_LENGTH];
    GetDlgItemText(hwndDlg, IDC_EMAILTEXT, email, sizeof(email));
    gQueryParameters[L"Email"] = email;
    EnableWindow(GetDlgItem(hwndDlg, IDC_EMAILTEXT), true);
  } else {
    gQueryParameters.erase(L"Email");
    EnableWindow(GetDlgItem(hwndDlg, IDC_EMAILTEXT), false);
  }
}

static void UpdateComment(HWND hwndDlg)
{
  wchar_t comment[MAX_COMMENT_LENGTH + 1];
  GetDlgItemText(hwndDlg, IDC_COMMENTTEXT, comment, sizeof(comment));
  if (wcslen(comment) > 0)
    gQueryParameters[L"Comments"] = comment;
  else
    gQueryParameters.erase(L"Comments");
}

/*
 * Dialog procedure for the "view report" dialog.
 */
static BOOL CALLBACK ViewReportDialogProc(HWND hwndDlg, UINT message,
                                          WPARAM wParam, LPARAM lParam)
{
  switch (message) {
  case WM_INITDIALOG: {
    SetWindowText(hwndDlg, Str(ST_VIEWREPORTTITLE).c_str());    
    SetDlgItemText(hwndDlg, IDOK, Str(ST_OK).c_str());
    SendDlgItemMessage(hwndDlg, IDC_VIEWREPORTTEXT,
                       EM_SETTARGETDEVICE, (WPARAM)NULL, 0);
    ShowReportInfo(hwndDlg);
    SetFocus(GetDlgItem(hwndDlg, IDOK));
    return FALSE;
  }

  case WM_COMMAND: {
    if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDOK)
      EndDialog(hwndDlg, 0);
    return FALSE;
  }
  }
  return FALSE;
}

// Return the number of bytes this string will take encoded
// in UTF-8
static inline int BytesInUTF8(wchar_t* str)
{
  // Just count size of buffer for UTF-8, minus one
  // (we don't need to count the null terminator)
  return WideCharToMultiByte(CP_UTF8, 0, str, -1, NULL, 0, NULL, NULL) - 1;
}

// Calculate the length of the text in this edit control (in bytes,
// in the UTF-8 encoding) after replacing the current selection
// with |insert|.
static int NewTextLength(HWND hwndEdit, wchar_t* insert)
{
  wchar_t current[MAX_COMMENT_LENGTH + 1];

  GetWindowText(hwndEdit, current, MAX_COMMENT_LENGTH + 1);
  DWORD selStart, selEnd;
  SendMessage(hwndEdit, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);

  int selectionLength = 0;
  if (selEnd - selStart > 0) {
    wchar_t selection[MAX_COMMENT_LENGTH + 1];
    google_breakpad::WindowsStringUtils::safe_wcsncpy(selection,
                                                      MAX_COMMENT_LENGTH + 1,
                                                      current + selStart,
                                                      selEnd - selStart);
    selection[selEnd - selStart] = '\0';
    selectionLength = BytesInUTF8(selection);
  }

  // current string length + replacement text length
  // - replaced selection length
  return BytesInUTF8(current) + BytesInUTF8(insert) - selectionLength;
}

// Window procedure for subclassing edit controls
static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                         LPARAM lParam)
{
  static WNDPROC super = NULL;

  if (super == NULL)
    super = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_USERDATA);

  switch (uMsg) {
  case WM_PAINT: {
    HDC hdc;
    PAINTSTRUCT ps;
    RECT r;
    wchar_t windowText[1024];

    GetWindowText(hwnd, windowText, 1024);
    // if the control contains text or is focused, draw it normally
    if (GetFocus() == hwnd || windowText[0] != '\0')
      return CallWindowProc(super, hwnd, uMsg, wParam, lParam);
    
    GetClientRect(hwnd, &r);
    hdc = BeginPaint(hwnd, &ps);
    FillRect(hdc, &r, GetSysColorBrush(IsWindowEnabled(hwnd)
                                       ? COLOR_WINDOW : COLOR_BTNFACE));
    SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));
    SelectObject(hdc, (HFONT)GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(hdc, TRANSPARENT);
    wchar_t* txt = (wchar_t*)GetProp(hwnd, L"PROP_GRAYTEXT");
    // Get the actual edit control rect
    CallWindowProc(super, hwnd, EM_GETRECT, 0, (LPARAM)&r);
    if (txt)
      DrawText(hdc, txt, wcslen(txt), &r,
               DT_EDITCONTROL | DT_NOPREFIX | DT_WORDBREAK | DT_INTERNAL);
    EndPaint(hwnd, &ps);
    return 0;
  }

    // We handle WM_CHAR and WM_PASTE to limit the comment box to 500
    // bytes in UTF-8.
  case WM_CHAR: {
    // Leave accelerator keys and non-printing chars (except LF) alone
    if (wParam & (1<<24) || wParam & (1<<29) ||
        (wParam < ' ' && wParam != '\n'))
      break;
  
    wchar_t ch[2] = { (wchar_t)wParam, 0 };
    if (NewTextLength(hwnd, ch) > MAX_COMMENT_LENGTH)
      return 0;

    break;
  }

  case WM_PASTE: {
    if (IsClipboardFormatAvailable(CF_UNICODETEXT) &&
        OpenClipboard(hwnd)) {
      HGLOBAL hg = GetClipboardData(CF_UNICODETEXT); 
      wchar_t* pastedText = (wchar_t*)GlobalLock(hg);
      int newSize = 0;

      if (pastedText)
        newSize = NewTextLength(hwnd, pastedText);

      GlobalUnlock(hg);
      CloseClipboard();

      if (newSize > MAX_COMMENT_LENGTH)
        return 0;
    }
    break;
  }

  case WM_SETFOCUS:
  case WM_KILLFOCUS: {
    RECT r;
    GetClientRect(hwnd, &r);
    InvalidateRect(hwnd, &r, TRUE);
    break;
  }

  case WM_DESTROY: {
    // cleanup our property
    HGLOBAL hData = RemoveProp(hwnd, L"PROP_GRAYTEXT");
    if (hData)
      GlobalFree(hData);
  }
  }

  return CallWindowProc(super, hwnd, uMsg, wParam, lParam);
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
    HICON hIcon = LoadIcon(GetModuleHandle(NULL),
                           MAKEINTRESOURCE(IDI_MAINICON));
    SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

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

    // resize the "View Report" button based on the string length
    RECT viewRect;
    HWND hwndView = GetDlgItem(hwndDlg, IDC_VIEWREPORTBUTTON);
    GetRelativeRect(hwndView, hwndDlg, &viewRect);
    HDC hdc = GetDC(hwndView);
    const wstring viewButtonText = Str(ST_VIEWREPORT);
    wchar_t oldViewButtonText[1024];

    GetDlgItemText(hwndDlg, IDC_VIEWREPORTBUTTON, oldViewButtonText, 1024);
    SIZE size, oldSize;

    if (GetTextExtentPoint32(hdc, viewButtonText.c_str(),
                             viewButtonText.length(), &size) &&
        // default text on the button
        GetTextExtentPoint32(hdc, oldViewButtonText,
                             wcslen(oldViewButtonText), &oldSize)) {
      // grow right right by the amount it changed
      int sizeDiff = size.cx - oldSize.cx;

      // don't bother shrinking the button
      if (sizeDiff > 0) {
        viewRect.right += sizeDiff;
        MoveWindow(hwndView, viewRect.left, viewRect.top,
                   viewRect.right - viewRect.left,
                   viewRect.bottom - viewRect.top,
                   TRUE);
      }
    }
    SetDlgItemText(hwndDlg, IDC_VIEWREPORTBUTTON, viewButtonText.c_str());


    SetDlgItemText(hwndDlg, IDC_SUBMITREPORTCHECK,
                   Str(ST_CHECKSUBMIT).c_str());
    if (CheckBoolKey(gCrashReporterKey.c_str(),
                     SUBMIT_REPORT_VALUE, &enabled) &&
        !enabled) {
      CheckDlgButton(hwndDlg, IDC_SUBMITREPORTCHECK, BST_UNCHECKED);
      EnableWindow(GetDlgItem(hwndDlg, IDC_VIEWREPORTBUTTON), enabled);
      EnableWindow(GetDlgItem(hwndDlg, IDC_COMMENTTEXT), enabled);
      EnableWindow(GetDlgItem(hwndDlg, IDC_INCLUDEURLCHECK), enabled);
      EnableWindow(GetDlgItem(hwndDlg, IDC_EMAILMECHECK), enabled);
      EnableWindow(GetDlgItem(hwndDlg, IDC_EMAILTEXT), enabled);
      SetDlgItemVisible(hwndDlg, IDC_PROGRESSTEXT, enabled);
    } else {
      CheckDlgButton(hwndDlg, IDC_SUBMITREPORTCHECK, BST_CHECKED);
    }


    HWND hwndComment = GetDlgItem(hwndDlg, IDC_COMMENTTEXT);
    WNDPROC OldWndProc = (WNDPROC)SetWindowLongPtr(hwndComment,
                                                   GWLP_WNDPROC,
                                                   (LONG_PTR)EditSubclassProc);

    // Subclass comment edit control to get placeholder text
    SetWindowLongPtr(hwndComment, GWLP_USERDATA, (LONG_PTR)OldWndProc);
    wstring commentGrayText = Str(ST_COMMENTGRAYTEXT);
    wchar_t* hMem = (wchar_t*)GlobalAlloc(GPTR, (commentGrayText.length() + 1)*sizeof(wchar_t));
    wcscpy(hMem, commentGrayText.c_str());
    SetProp(hwndComment, L"PROP_GRAYTEXT", hMem);

    SetDlgItemText(hwndDlg, IDC_INCLUDEURLCHECK, Str(ST_CHECKURL).c_str());
    // want this on by default
    if (CheckBoolKey(gCrashReporterKey.c_str(), INCLUDE_URL_VALUE, &enabled) &&
        !enabled) {
      CheckDlgButton(hwndDlg, IDC_INCLUDEURLCHECK, BST_UNCHECKED);
    } else {
      CheckDlgButton(hwndDlg, IDC_INCLUDEURLCHECK, BST_CHECKED);
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

    // Subclass email edit control to get placeholder text
    HWND hwndEmail = GetDlgItem(hwndDlg, IDC_EMAILTEXT);
    OldWndProc = (WNDPROC)SetWindowLongPtr(hwndEmail,
                                           GWLP_WNDPROC,
                                           (LONG_PTR)EditSubclassProc);
    SetWindowLongPtr(hwndEmail, GWLP_USERDATA, (LONG_PTR)OldWndProc);
    wstring emailGrayText = Str(ST_EMAILGRAYTEXT);
    hMem = (wchar_t*)GlobalAlloc(GPTR, (emailGrayText.length() + 1)*sizeof(wchar_t));
    wcscpy(hMem, emailGrayText.c_str());
    SetProp(hwndEmail, L"PROP_GRAYTEXT", hMem);

    SetDlgItemText(hwndDlg, IDC_PROGRESSTEXT, Str(ST_REPORTPRESUBMIT).c_str());
    SetDlgItemText(hwndDlg, IDC_CLOSEBUTTON, Str(ST_QUIT).c_str());

    RECT closeRect;
    HWND hwndClose = GetDlgItem(hwndDlg, IDC_CLOSEBUTTON);
    GetRelativeRect(hwndClose, hwndDlg, &closeRect);

    RECT restartRect;
    HWND hwndRestart = GetDlgItem(hwndDlg, IDC_RESTARTBUTTON);
    GetRelativeRect(hwndRestart, hwndDlg, &restartRect);

    if (gRestartArgs.size() > 0) {
      // set the restart button text and shift the buttons around
      // since the size may need to change
      hdc = GetDC(hwndRestart);
      const wstring restartButtonText = Str(ST_RESTART);
      if (GetTextExtentPoint32(hdc, restartButtonText.c_str(),
                               restartButtonText.length(), &size)) {
        // shift left by the amount the button should grow
        int sizeDiff = size.cx - (restartRect.right - restartRect.left);
        restartRect.left -= sizeDiff;
        closeRect.left -= sizeDiff;
        closeRect.right -= sizeDiff;
        MoveWindow(hwndRestart, restartRect.left, restartRect.top,
                   restartRect.right - restartRect.left,
                   restartRect.bottom - restartRect.top,
                   TRUE);
      }
      SetDlgItemText(hwndDlg, IDC_RESTARTBUTTON, restartButtonText.c_str());
    } else {
      // No restart arguments, move the close button over to the side
      // and hide the restart button
      SetDlgItemVisible(hwndDlg, IDC_RESTARTBUTTON, false);

      int size = closeRect.right - closeRect.left;
      closeRect.right = restartRect.right;
      closeRect.left = closeRect.right - size;
    }
    // need to move the close button regardless
    MoveWindow(hwndClose, closeRect.left, closeRect.top,
               closeRect.right - closeRect.left,
               closeRect.bottom - closeRect.top,
               TRUE);

    // if no URL was given, hide the URL checkbox
    if (gQueryParameters.find(L"URL") == gQueryParameters.end()) {
      RECT urlCheckRect, emailCheckRect;
      GetWindowRect(GetDlgItem(hwndDlg, IDC_INCLUDEURLCHECK), &urlCheckRect);
      GetWindowRect(GetDlgItem(hwndDlg, IDC_EMAILMECHECK), &emailCheckRect);

      SetDlgItemVisible(hwndDlg, IDC_INCLUDEURLCHECK, false);

      gAttachedBottom.erase(IDC_VIEWREPORTBUTTON);
      gAttachedBottom.erase(IDC_SUBMITREPORTCHECK);
      gAttachedBottom.erase(IDC_COMMENTTEXT);

      StretchDialog(hwndDlg, urlCheckRect.top - emailCheckRect.top);

      gAttachedBottom.insert(IDC_VIEWREPORTBUTTON);
      gAttachedBottom.insert(IDC_SUBMITREPORTCHECK);
      gAttachedBottom.insert(IDC_COMMENTTEXT);
    }

    // Open the AVI resource for the throbber
    Animate_Open(GetDlgItem(hwndDlg, IDC_THROBBER),
                 MAKEINTRESOURCE(IDR_THROBBER));

    UpdateURL(hwndDlg);
    UpdateEmail(hwndDlg);

    SetFocus(GetDlgItem(hwndDlg, IDC_SUBMITREPORTCHECK));
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
      case IDC_VIEWREPORTBUTTON:
        DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_VIEWREPORTDIALOG), hwndDlg,
                       (DLGPROC)ViewReportDialogProc, 0);
        break;
      case IDC_SUBMITREPORTCHECK:
        enabled = (IsDlgButtonChecked(hwndDlg, IDC_SUBMITREPORTCHECK) != 0);
        EnableWindow(GetDlgItem(hwndDlg, IDC_VIEWREPORTBUTTON), enabled);
        EnableWindow(GetDlgItem(hwndDlg, IDC_COMMENTTEXT), enabled);
        EnableWindow(GetDlgItem(hwndDlg, IDC_INCLUDEURLCHECK), enabled);
        EnableWindow(GetDlgItem(hwndDlg, IDC_EMAILMECHECK), enabled);
        EnableWindow(GetDlgItem(hwndDlg, IDC_EMAILTEXT),
                     enabled && (IsDlgButtonChecked(hwndDlg, IDC_EMAILMECHECK)
                                 != 0));
        SetDlgItemVisible(hwndDlg, IDC_PROGRESSTEXT, enabled);
        break;
      case IDC_INCLUDEURLCHECK:
        UpdateURL(hwndDlg);
        break;
      case IDC_EMAILMECHECK:
        UpdateEmail(hwndDlg);
        break;
      case IDC_CLOSEBUTTON:
        MaybeSendReport(hwndDlg);
        break;
      case IDC_RESTARTBUTTON:
        RestartApplication();
        MaybeSendReport(hwndDlg);
        break;
      }
    } else if (HIWORD(wParam) == EN_CHANGE) {
      switch(LOWORD(wParam)) {
      case IDC_EMAILTEXT:
        UpdateEmail(hwndDlg);
        break;
      case IDC_COMMENTTEXT:
        UpdateComment(hwndDlg);
      }
    }

    return FALSE;
  }
  case WM_UPLOADCOMPLETE: {
    WaitForSingleObject(gThreadHandle, INFINITE);
    success = (wParam == 1);
    SendCompleted(success, WideToUTF8(gSendData.serverResponse));
    // hide throbber
    Animate_Stop(GetDlgItem(hwndDlg, IDC_THROBBER));
    SetDlgItemVisible(hwndDlg, IDC_THROBBER, false);

    SetDlgItemText(hwndDlg, IDC_PROGRESSTEXT,
                   success ?
                   Str(ST_REPORTSUBMITSUCCESS).c_str() :
                   Str(ST_SUBMITFAILED).c_str());
    // close dialog after 5 seconds
    SetTimer(hwndDlg, 0, 5000, NULL);
    //
    return TRUE;
  }

  case WM_LBUTTONDOWN: {
    HWND hwndEmail = GetDlgItem(hwndDlg, IDC_EMAILTEXT);
    POINT p = { LOWORD(lParam), HIWORD(lParam) };
    // if the email edit control is clicked, enable it,
    // check the email checkbox, and focus the email edit control
    if (ChildWindowFromPoint(hwndDlg, p) == hwndEmail &&
        !IsWindowEnabled(hwndEmail) &&
        IsDlgButtonChecked(hwndDlg, IDC_SUBMITREPORTCHECK) != 0) {
      CheckDlgButton(hwndDlg, IDC_EMAILMECHECK, BST_CHECKED);
      UpdateEmail(hwndDlg);
      SetFocus(hwndEmail);
    }
    break;
  }

  case WM_TIMER: {
    // The "1" gets used down in UIShowCrashUI to indicate that we at least
    // tried to send the report.
    EndCrashReporterDialog(hwndDlg, 1);
    return FALSE;
  }

  case WM_CLOSE: {
    EndCrashReporterDialog(hwndDlg, 0);
    return FALSE;
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

bool UIShowCrashUI(const string& dumpFile,
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
    gCrashReporterKey += gQueryParameters[L"ProductName"] + L"\\Crash Reporter";
  }

  if (gQueryParameters.find(L"URL") != gQueryParameters.end())
    gURLParameter = gQueryParameters[L"URL"];

  gRestartArgs = restartArgs;

  return DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_SENDDIALOG), NULL,
                        (DLGPROC)CrashReporterDialogProc, 0) == 1;
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

ofstream* UIOpenWrite(const string& filename, bool append) // append=false
{
  // adapted from breakpad's src/common/windows/http_upload.cc

  // The "open" method on pre-MSVC8 ifstream implementations doesn't accept a
  // wchar_t* filename, so use _wfopen directly in that case.  For VC8 and
  // later, _wfopen has been deprecated in favor of _wfopen_s, which does
  // not exist in earlier versions, so let the ifstream open the file itself.
#if _MSC_VER >= 1400  // MSVC 2005/8
  ofstream* file = new ofstream();
  file->open(UTF8ToWide(filename).c_str(), append ? ios::out | ios::app
                                                  : ios::out);
#else  // _MSC_VER >= 1400
  ofstream* file = new ofstream(_wfopen(UTF8ToWide(filename).c_str(),
                                        append ? L"a" : L"w"));
#endif  // _MSC_VER >= 1400

  return file;
}
