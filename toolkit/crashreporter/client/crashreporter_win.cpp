/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif

#include "crashreporter.h"

#include <windows.h>
#include <versionhelpers.h>
#include <commctrl.h>
#include <richedit.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <math.h>
#include <set>
#include <algorithm>
#include "resource.h"
#include "windows/sender/crash_report_sender.h"
#include "common/windows/string_utils-inl.h"

#define CRASH_REPORTER_VALUE L"Enabled"
#define SUBMIT_REPORT_VALUE  L"SubmitCrashReport"
#define SUBMIT_REPORT_OLD    L"SubmitReport"
#define INCLUDE_URL_VALUE    L"IncludeURL"
#define EMAIL_ME_VALUE       L"EmailMe"
#define EMAIL_VALUE          L"Email"
#define MAX_EMAIL_LENGTH     1024

#define SENDURL_ORIGINAL L"https://crash-reports.mozilla.com/submit"
#define SENDURL_XPSP2 L"https://crash-reports-xpsp2.mozilla.com/submit"

#define WM_UPLOADCOMPLETE WM_APP

// Thanks, Windows.h :(
#undef min
#undef max

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
  map<wstring,wstring> queryParameters;
  map<wstring,wstring> files;
  wstring sendURL;

  wstring serverResponse;
} SendThreadData;

/*
 * Per http://msdn2.microsoft.com/en-us/library/ms645398(VS.85).aspx
 * "The DLGTEMPLATEEX structure is not defined in any standard header file.
 * The structure definition is provided here to explain the format of an
 * extended template for a dialog box.
*/
typedef struct {
    WORD dlgVer;
    WORD signature;
    DWORD helpID;
    DWORD exStyle;
  // There's more to this struct, but it has weird variable-length
  // members, and I only actually need to touch exStyle on an existing
  // instance, so I've omitted the rest.
} DLGTEMPLATEEX;

static HANDLE               gThreadHandle;
static SendThreadData       gSendData = { 0, };
static vector<string>       gRestartArgs;
static map<wstring,wstring> gQueryParameters;
static wstring              gCrashReporterKey(L"Software\\Mozilla\\Crash Reporter");
static wstring              gURLParameter;
static int                  gCheckboxPadding = 6;
static bool                 gRTLlayout = false;

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
  LoadLibrary(L"Msftedit.dll");
}

static bool GetBoolValue(HKEY hRegKey, LPCTSTR valueName, DWORD* value)
{
  DWORD type, dataSize;
  dataSize = sizeof(DWORD);
  if (RegQueryValueEx(hRegKey, valueName, nullptr,
                      &type, (LPBYTE)value, &dataSize) == ERROR_SUCCESS &&
      type == REG_DWORD)
    return true;

  return false;
}

// Removes a value from HKEY_LOCAL_MACHINE and HKEY_CURRENT_USER, if it exists.
static void RemoveUnusedValues(const wchar_t* key, LPCTSTR valueName)
{
  HKEY hRegKey;

  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, KEY_SET_VALUE, &hRegKey)
      == ERROR_SUCCESS) {
    RegDeleteValue(hRegKey, valueName);
    RegCloseKey(hRegKey);
  }

  if (RegOpenKeyEx(HKEY_CURRENT_USER, key, 0, KEY_SET_VALUE, &hRegKey)
      == ERROR_SUCCESS) {
    RegDeleteValue(hRegKey, valueName);
    RegCloseKey(hRegKey);
  }
}

static bool CheckBoolKey(const wchar_t* key,
                         const wchar_t* valueName,
                         bool* enabled)
{
  /*
   * NOTE! This code needs to stay in sync with the preference checking
   *       code in in nsExceptionHandler.cpp.
   */
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
  /*
   * NOTE! This code needs to stay in sync with the preference setting
   *       code in in nsExceptionHandler.cpp.
   */
  HKEY hRegKey;

  // remove the old value from the registry if it exists
  RemoveUnusedValues(key, SUBMIT_REPORT_OLD);

  if (RegCreateKey(HKEY_CURRENT_USER, key, &hRegKey) == ERROR_SUCCESS) {
    DWORD data = (enabled ? 1 : 0);
    RegSetValueEx(hRegKey, value, 0, REG_DWORD, (LPBYTE)&data, sizeof(data));
    RegCloseKey(hRegKey);
  }
}

static bool GetStringValue(HKEY hRegKey, LPCTSTR valueName, wstring& value)
{
  DWORD type, dataSize;
  wchar_t buf[2048] = {};
  dataSize = sizeof(buf) - 1;
  if (RegQueryValueEx(hRegKey, valueName, nullptr,
                     &type, (LPBYTE)buf, &dataSize) == ERROR_SUCCESS &&
      type == REG_SZ) {
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
                   nullptr) != 0) {
    message += WideToUTF8(s, nullptr);
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

#define TS_DRAW 2
#define BP_CHECKBOX  3

typedef  HANDLE (WINAPI*OpenThemeDataPtr)(HWND hwnd, LPCWSTR pszClassList);
typedef  HRESULT (WINAPI*CloseThemeDataPtr)(HANDLE hTheme);
typedef  HRESULT (WINAPI*GetThemePartSizePtr)(HANDLE hTheme, HDC hdc, int iPartId,
                                              int iStateId, RECT* prc, int ts,
                                              SIZE* psz);
typedef HRESULT (WINAPI*GetThemeContentRectPtr)(HANDLE hTheme, HDC hdc, int iPartId,
                                                int iStateId, const RECT* pRect,
                                                RECT* pContentRect);


static void GetThemeSizes(HWND hwnd)
{
  HMODULE themeDLL = LoadLibrary(L"uxtheme.dll");

  if (!themeDLL)
    return;

  OpenThemeDataPtr openTheme =
    (OpenThemeDataPtr)GetProcAddress(themeDLL, "OpenThemeData");
  CloseThemeDataPtr closeTheme =
    (CloseThemeDataPtr)GetProcAddress(themeDLL, "CloseThemeData");
  GetThemePartSizePtr getThemePartSize =
    (GetThemePartSizePtr)GetProcAddress(themeDLL, "GetThemePartSize");

  if (!openTheme || !closeTheme || !getThemePartSize) {
    FreeLibrary(themeDLL);
    return;
  }

  HANDLE buttonTheme = openTheme(hwnd, L"Button");
  if (!buttonTheme) {
    FreeLibrary(themeDLL);
    return;
  }
  HDC hdc = GetDC(hwnd);
  SIZE s;
  getThemePartSize(buttonTheme, hdc, BP_CHECKBOX, 0, nullptr, TS_DRAW, &s);
  gCheckboxPadding = s.cx;
  closeTheme(buttonTheme);
  FreeLibrary(themeDLL);
}

// Gets the position of a window relative to another window's client area
static void GetRelativeRect(HWND hwnd, HWND hwndParent, RECT* r)
{
  GetWindowRect(hwnd, r);
  MapWindowPoints(nullptr, hwndParent, (POINT*)r, 2);
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
                                         td->files,
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

  if (gAutoSubmit) {
    // Ordinarily this is done on the main thread in CrashReporterDialogProc,
    // for auto submit we don't run that and it should be safe to finish up
    // here as is done on other platforms.
    SendCompleted(finishedOk, WideToUTF8(gSendData.serverResponse));
  } else {
    PostMessage(td->hDlg, WM_UPLOADCOMPLETE, finishedOk ? 1 : 0, 0);
  }

  return 0;
}

static void EndCrashReporterDialog(HWND hwndDlg, int code)
{
  // Save the current values to the registry
  wchar_t email[MAX_EMAIL_LENGTH];
  GetDlgItemTextW(hwndDlg, IDC_EMAILTEXT, email,
                  sizeof(email) / sizeof(email[0]));
  SetStringKey(gCrashReporterKey.c_str(), EMAIL_VALUE, email);

  SetBoolKey(gCrashReporterKey.c_str(), INCLUDE_URL_VALUE,
             IsDlgButtonChecked(hwndDlg, IDC_INCLUDEURLCHECK) != 0);
  SetBoolKey(gCrashReporterKey.c_str(), EMAIL_ME_VALUE,
             IsDlgButtonChecked(hwndDlg, IDC_EMAILMECHECK) != 0);
  SetBoolKey(gCrashReporterKey.c_str(), SUBMIT_REPORT_VALUE,
             IsDlgButtonChecked(hwndDlg, IDC_SUBMITREPORTCHECK) != 0);

  EndDialog(hwndDlg, code);
}

static void MaybeResizeProgressText(HWND hwndDlg)
{
  HWND hwndProgress = GetDlgItem(hwndDlg, IDC_PROGRESSTEXT);
  HDC hdc = GetDC(hwndProgress);
  HFONT hfont = (HFONT)SendMessage(hwndProgress, WM_GETFONT, 0, 0);
  if (hfont)
    SelectObject(hdc, hfont);
  SIZE size;
  RECT rect;
  GetRelativeRect(hwndProgress, hwndDlg, &rect);

  wchar_t text[1024];
  GetWindowText(hwndProgress, text, 1024);

  if (!GetTextExtentPoint32(hdc, text, wcslen(text), &size))
    return;

  if (size.cx < (rect.right - rect.left))
    return;

  // Figure out how much we need to resize things vertically
  // This is sort of a fudge, but it should be good enough.
  int wantedHeight = size.cy *
    (int)ceil((float)size.cx / (float)(rect.right - rect.left));
  int diff = wantedHeight - (rect.bottom - rect.top);
  if (diff <= 0)
    return;

  MoveWindow(hwndProgress, rect.left, rect.top,
             rect.right - rect.left,
             wantedHeight,
             TRUE);

  gAttachedBottom.clear();
  gAttachedBottom.insert(IDC_CLOSEBUTTON);
  gAttachedBottom.insert(IDC_RESTARTBUTTON);

  StretchDialog(hwndDlg, diff);

  for (int i = 0; i < sizeof(kDefaultAttachedBottom) / sizeof(UINT); i++) {
    gAttachedBottom.insert(kDefaultAttachedBottom[i]);
  }
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
  MaybeResizeProgressText(hwndDlg);
  // start throbber
  // play entire AVI, and loop
  Animate_Play(GetDlgItem(hwndDlg, IDC_THROBBER), 0, -1, -1);
  SetDlgItemVisible(hwndDlg, IDC_THROBBER, true);
  gThreadHandle = nullptr;
  gSendData.hDlg = hwndDlg;
  gSendData.queryParameters = gQueryParameters;

  gThreadHandle = CreateThread(nullptr, 0, SendThreadProc, &gSendData, 0,
                               nullptr);
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

  if (CreateProcess(nullptr, (LPWSTR)cmdLine.c_str(), nullptr, nullptr, FALSE,
                    0, nullptr, nullptr, &si, &pi)) {
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
    GetDlgItemTextW(hwndDlg, IDC_EMAILTEXT, email,
                    sizeof(email) / sizeof(email[0]));
    gQueryParameters[L"Email"] = email;
    if (IsDlgButtonChecked(hwndDlg, IDC_SUBMITREPORTCHECK))
      EnableWindow(GetDlgItem(hwndDlg, IDC_EMAILTEXT), true);
  } else {
    gQueryParameters.erase(L"Email");
    EnableWindow(GetDlgItem(hwndDlg, IDC_EMAILTEXT), false);
  }
}

static void UpdateComment(HWND hwndDlg)
{
  wchar_t comment[MAX_COMMENT_LENGTH + 1];
  GetDlgItemTextW(hwndDlg, IDC_COMMENTTEXT, comment,
                  sizeof(comment) / sizeof(comment[0]));
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
                       EM_SETTARGETDEVICE, (WPARAM)nullptr, 0);
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
  return WideCharToMultiByte(CP_UTF8, 0, str, -1,
                             nullptr, 0, nullptr, nullptr) - 1;
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
  static WNDPROC super = nullptr;

  if (super == nullptr)
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
    UINT format = DT_EDITCONTROL | DT_NOPREFIX | DT_WORDBREAK | DT_INTERNAL;
    if (gRTLlayout)
      format |= DT_RIGHT;
    if (txt)
      DrawText(hdc, txt, wcslen(txt), &r, format);
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

// Resize a control to fit this text
static int ResizeControl(HWND hwndButton, RECT& rect, wstring text,
                         bool shiftLeft, int userDefinedPadding)
{
  HDC hdc = GetDC(hwndButton);
  HFONT hfont = (HFONT)SendMessage(hwndButton, WM_GETFONT, 0, 0);
  if (hfont)
    SelectObject(hdc, hfont);
  SIZE size, oldSize;
  int sizeDiff = 0;

  wchar_t oldText[1024];
  GetWindowText(hwndButton, oldText, 1024);

  if (GetTextExtentPoint32(hdc, text.c_str(), text.length(), &size)
      // default text on the button
      && GetTextExtentPoint32(hdc, oldText, wcslen(oldText), &oldSize)) {
    /*
     Expand control widths to accomidate wider text strings. For most
     controls (including buttons) the text padding is defined by the
     dialog's rc file. Some controls (such as checkboxes) have padding
     that extends to the end of the dialog, in which case we ignore the
     rc padding and rely on a user defined value passed in through
     userDefinedPadding.
    */
    int textIncrease = size.cx - oldSize.cx;
    if (textIncrease < 0)
      return 0;
    int existingTextPadding;
    if (userDefinedPadding == 0)
      existingTextPadding = (rect.right - rect.left) - oldSize.cx;
    else
      existingTextPadding = userDefinedPadding;
    sizeDiff = textIncrease + existingTextPadding;

    if (shiftLeft) {
      // shift left by the amount the button should grow
      rect.left -= sizeDiff;
    }
    else {
      // grow right instead
      rect.right += sizeDiff;
    }
    MoveWindow(hwndButton, rect.left, rect.top,
               rect.right - rect.left,
               rect.bottom - rect.top,
               TRUE);
  }
  return sizeDiff;
}

// The window was resized horizontally, so widen some of our
// controls to make use of the space
static void StretchControlsToFit(HWND hwndDlg)
{
  int controls[] = {
    IDC_DESCRIPTIONTEXT,
    IDC_SUBMITREPORTCHECK,
    IDC_COMMENTTEXT,
    IDC_INCLUDEURLCHECK,
    IDC_EMAILMECHECK,
    IDC_EMAILTEXT,
    IDC_PROGRESSTEXT
  };

  RECT dlgRect;
  GetClientRect(hwndDlg, &dlgRect);

  for (int i=0; i<sizeof(controls)/sizeof(controls[0]); i++) {
    RECT r;
    HWND hwndControl = GetDlgItem(hwndDlg, controls[i]);
    GetRelativeRect(hwndControl, hwndDlg, &r);
    // 6 pixel spacing on the right
    if (r.right + 6 != dlgRect.right) {
      r.right = dlgRect.right - 6;
      MoveWindow(hwndControl, r.left, r.top,
                 r.right - r.left,
                 r.bottom - r.top,
                 TRUE);
    }
  }
}

static void SubmitReportChecked(HWND hwndDlg)
{
  bool enabled = (IsDlgButtonChecked(hwndDlg, IDC_SUBMITREPORTCHECK) != 0);
  EnableWindow(GetDlgItem(hwndDlg, IDC_VIEWREPORTBUTTON), enabled);
  EnableWindow(GetDlgItem(hwndDlg, IDC_COMMENTTEXT), enabled);
  EnableWindow(GetDlgItem(hwndDlg, IDC_INCLUDEURLCHECK), enabled);
  EnableWindow(GetDlgItem(hwndDlg, IDC_EMAILMECHECK), enabled);
  EnableWindow(GetDlgItem(hwndDlg, IDC_EMAILTEXT),
               enabled && (IsDlgButtonChecked(hwndDlg, IDC_EMAILMECHECK)
                           != 0));
  SetDlgItemVisible(hwndDlg, IDC_PROGRESSTEXT, enabled);
}

static INT_PTR DialogBoxParamMaybeRTL(UINT idd, HWND hwndParent,
                                      DLGPROC dlgProc, LPARAM param)
{
  INT_PTR rv = 0;
  if (gRTLlayout) {
    // We need to toggle the WS_EX_LAYOUTRTL style flag on the dialog
    // template.
    HRSRC hDialogRC = FindResource(nullptr, MAKEINTRESOURCE(idd),
                                   RT_DIALOG);
    HGLOBAL  hDlgTemplate = LoadResource(nullptr, hDialogRC);
    DLGTEMPLATEEX* pDlgTemplate = (DLGTEMPLATEEX*)LockResource(hDlgTemplate);
    unsigned long sizeDlg = SizeofResource(nullptr, hDialogRC);
    HGLOBAL hMyDlgTemplate = GlobalAlloc(GPTR, sizeDlg);
     DLGTEMPLATEEX* pMyDlgTemplate =
      (DLGTEMPLATEEX*)GlobalLock(hMyDlgTemplate);
    memcpy(pMyDlgTemplate, pDlgTemplate, sizeDlg);

    pMyDlgTemplate->exStyle |= WS_EX_LAYOUTRTL;

    rv = DialogBoxIndirectParam(nullptr, (LPCDLGTEMPLATE)pMyDlgTemplate,
                                hwndParent, dlgProc, param);
    GlobalUnlock(hMyDlgTemplate);
    GlobalFree(hMyDlgTemplate);
  }
  else {
    rv = DialogBoxParam(nullptr, MAKEINTRESOURCE(idd), hwndParent,
                        dlgProc, param);
  }

  return rv;
}


static BOOL CALLBACK CrashReporterDialogProc(HWND hwndDlg, UINT message,
                                             WPARAM wParam, LPARAM lParam)
{
  static int sHeight = 0;

  bool success;
  bool enabled;

  switch (message) {
  case WM_INITDIALOG: {
    GetThemeSizes(hwndDlg);
    RECT r;
    GetClientRect(hwndDlg, &r);
    sHeight = r.bottom - r.top;

    SetWindowText(hwndDlg, Str(ST_CRASHREPORTERTITLE).c_str());
    HICON hIcon = LoadIcon(GetModuleHandle(nullptr),
                           MAKEINTRESOURCE(IDI_MAINICON));
    SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

    // resize the "View Report" button based on the string length
    RECT rect;
    HWND hwnd = GetDlgItem(hwndDlg, IDC_VIEWREPORTBUTTON);
    GetRelativeRect(hwnd, hwndDlg, &rect);
    ResizeControl(hwnd, rect, Str(ST_VIEWREPORT), false, 0);
    SetDlgItemText(hwndDlg, IDC_VIEWREPORTBUTTON, Str(ST_VIEWREPORT).c_str());

    hwnd = GetDlgItem(hwndDlg, IDC_SUBMITREPORTCHECK);
    GetRelativeRect(hwnd, hwndDlg, &rect);
    long maxdiff = ResizeControl(hwnd, rect, Str(ST_CHECKSUBMIT), false,
                                gCheckboxPadding);
    SetDlgItemText(hwndDlg, IDC_SUBMITREPORTCHECK,
                   Str(ST_CHECKSUBMIT).c_str());

    if (!CheckBoolKey(gCrashReporterKey.c_str(),
                      SUBMIT_REPORT_VALUE, &enabled))
      enabled = true;

    CheckDlgButton(hwndDlg, IDC_SUBMITREPORTCHECK, enabled ? BST_CHECKED
                                                           : BST_UNCHECKED);
    SubmitReportChecked(hwndDlg);

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

    hwnd = GetDlgItem(hwndDlg, IDC_INCLUDEURLCHECK);
    GetRelativeRect(hwnd, hwndDlg, &rect);
    long diff = ResizeControl(hwnd, rect, Str(ST_CHECKURL), false,
                             gCheckboxPadding);
    maxdiff = std::max(diff, maxdiff);
    SetDlgItemText(hwndDlg, IDC_INCLUDEURLCHECK, Str(ST_CHECKURL).c_str());

    // want this on by default
    if (CheckBoolKey(gCrashReporterKey.c_str(), INCLUDE_URL_VALUE, &enabled) &&
        !enabled) {
      CheckDlgButton(hwndDlg, IDC_INCLUDEURLCHECK, BST_UNCHECKED);
    } else {
      CheckDlgButton(hwndDlg, IDC_INCLUDEURLCHECK, BST_CHECKED);
    }

    hwnd = GetDlgItem(hwndDlg, IDC_EMAILMECHECK);
    GetRelativeRect(hwnd, hwndDlg, &rect);
    diff = ResizeControl(hwnd, rect, Str(ST_CHECKEMAIL), false,
                         gCheckboxPadding);
    maxdiff = std::max(diff, maxdiff);
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

    RECT closeRect;
    HWND hwndClose = GetDlgItem(hwndDlg, IDC_CLOSEBUTTON);
    GetRelativeRect(hwndClose, hwndDlg, &closeRect);

    RECT restartRect;
    HWND hwndRestart = GetDlgItem(hwndDlg, IDC_RESTARTBUTTON);
    GetRelativeRect(hwndRestart, hwndDlg, &restartRect);

    // set the close button text and shift the buttons around
    // since the size may need to change
    int sizeDiff = ResizeControl(hwndClose, closeRect, Str(ST_QUIT),
                                 true, 0);
    restartRect.left -= sizeDiff;
    restartRect.right -= sizeDiff;
    SetDlgItemText(hwndDlg, IDC_CLOSEBUTTON, Str(ST_QUIT).c_str());

    if (gRestartArgs.size() > 0) {
      // Resize restart button to fit text
      ResizeControl(hwndRestart, restartRect, Str(ST_RESTART), true, 0);
      SetDlgItemText(hwndDlg, IDC_RESTARTBUTTON, Str(ST_RESTART).c_str());
    } else {
      // No restart arguments, so just hide the restart button
      SetDlgItemVisible(hwndDlg, IDC_RESTARTBUTTON, false);
    }
    // See if we need to widen the window
    // Leave 6 pixels on either side + 6 pixels between the buttons
    int neededSize = closeRect.right - closeRect.left +
      restartRect.right - restartRect.left + 6 * 3;
    GetClientRect(hwndDlg, &r);
    // We may already have resized one of the checkboxes above
    maxdiff = std::max(maxdiff, neededSize - (r.right - r.left));

    if (maxdiff > 0) {
      // widen window
      GetWindowRect(hwndDlg, &r);
      r.right += maxdiff;
      MoveWindow(hwndDlg, r.left, r.top,
                 r.right - r.left, r.bottom - r.top, TRUE);
      // shift both buttons right
      if (restartRect.left + maxdiff < 6)
        maxdiff += 6;
      closeRect.left += maxdiff;
      closeRect.right += maxdiff;
      restartRect.left += maxdiff;
      restartRect.right += maxdiff;
      MoveWindow(hwndClose, closeRect.left, closeRect.top,
                 closeRect.right - closeRect.left,
                 closeRect.bottom - closeRect.top,
                 TRUE);
      StretchControlsToFit(hwndDlg);
    }
    // need to move the restart button regardless
    MoveWindow(hwndRestart, restartRect.left, restartRect.top,
               restartRect.right - restartRect.left,
               restartRect.bottom - restartRect.top,
               TRUE);

    // Resize the description text last, in case the window was resized
    // before this.
    SendDlgItemMessage(hwndDlg, IDC_DESCRIPTIONTEXT,
                       EM_SETEVENTMASK, (WPARAM)nullptr,
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
    // Force redraw.
    SendDlgItemMessage(hwndDlg, IDC_DESCRIPTIONTEXT,
                       EM_SETTARGETDEVICE, (WPARAM)nullptr, 0);
    // Force resize.
    SendDlgItemMessage(hwndDlg, IDC_DESCRIPTIONTEXT,
                       EM_REQUESTRESIZE, 0, 0);

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

    MaybeResizeProgressText(hwndDlg);

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
    InvalidateRect(hwndDlg, nullptr, TRUE);
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
        DialogBoxParamMaybeRTL(IDD_VIEWREPORTDIALOG, hwndDlg,
                       (DLGPROC)ViewReportDialogProc, 0);
        break;
      case IDC_SUBMITREPORTCHECK:
        SubmitReportChecked(hwndDlg);
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
    MaybeResizeProgressText(hwndDlg);
    // close dialog after 5 seconds
    SetTimer(hwndDlg, 0, 5000, nullptr);
    //
    return TRUE;
  }

  case WM_LBUTTONDOWN: {
    HWND hwndEmail = GetDlgItem(hwndDlg, IDC_EMAILTEXT);
    POINT p = { LOWORD(lParam), HIWORD(lParam) };
    // if the email edit control is clicked, enable it,
    // check the email checkbox, and focus the email edit control
    if (ChildWindowFromPoint(hwndDlg, p) == hwndEmail &&
        IsWindowEnabled(GetDlgItem(hwndDlg, IDC_RESTARTBUTTON)) &&
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
  wchar_t* buffer = nullptr;
  int buffer_size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                                        -1, nullptr, 0);
  if(buffer_size == 0) {
    if (success)
      *success = false;
    return L"";
  }

  buffer = new wchar_t[buffer_size];
  if(buffer == nullptr) {
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

static string WideToMBCP(const wstring& wide,
                         unsigned int cp,
                         bool* success = nullptr)
{
  char* buffer = nullptr;
  int buffer_size = WideCharToMultiByte(cp, 0, wide.c_str(),
                                        -1, nullptr, 0, nullptr, nullptr);
  if(buffer_size == 0) {
    if (success)
      *success = false;
    return "";
  }

  buffer = new char[buffer_size];
  if(buffer == nullptr) {
    if (success)
      *success = false;
    return "";
  }

  WideCharToMultiByte(cp, 0, wide.c_str(),
                      -1, buffer, buffer_size, nullptr, nullptr);
  string mb = buffer;
  delete [] buffer;

  if (success)
    *success = true;

  return mb;
}

string WideToUTF8(const wstring& wide, bool* success)
{
  return WideToMBCP(wide, CP_UTF8, success);
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
  MessageBox(nullptr, Str(ST_CRASHREPORTERDEFAULT).c_str(),
             L"Crash Reporter",
             MB_OK | MB_ICONSTOP);
}

static bool CanUseMainCrashReportServer()
{
  // Any NT from 6.0 and above is fine.
  if (IsWindowsVersionOrGreater(6, 0, 0)) {
    return true;
  }

  // On NT 5 servers, we need Server 2003 SP2.
  if (IsWindowsServer()) {
    return IsWindowsVersionOrGreater(5, 2, 2);
  }

  // Otherwise we have an NT 5 client.
  // We need exactly XP SP3 (version 5.1 SP3 but not version 5.2).
  return (IsWindowsVersionOrGreater(5, 1, 3) &&
         !IsWindowsVersionOrGreater(5, 2, 0));
}

bool UIShowCrashUI(const StringTable& files,
                   const StringTable& queryParameters,
                   const string& sendURL,
                   const vector<string>& restartArgs)
{
  gSendData.hDlg = nullptr;
  gSendData.sendURL = UTF8ToWide(sendURL);

  // Older Windows don't support the crash report server's crypto.
  // This is a hack to use an alternate server.
  if (!CanUseMainCrashReportServer() &&
      gSendData.sendURL.find(SENDURL_ORIGINAL) == 0) {
    gSendData.sendURL.replace(0, ARRAYSIZE(SENDURL_ORIGINAL) - 1,
                              SENDURL_XPSP2);
  }

  for (StringTable::const_iterator i = files.begin();
       i != files.end();
       i++) {
    gSendData.files[UTF8ToWide(i->first)] = UTF8ToWide(i->second);
  }

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

  if (gStrings.find("isRTL") != gStrings.end() &&
      gStrings["isRTL"] == "yes")
    gRTLlayout = true;

  if (gAutoSubmit) {
    gSendData.queryParameters = gQueryParameters;

    gThreadHandle = CreateThread(nullptr, 0, SendThreadProc, &gSendData, 0,
                                 nullptr);
    WaitForSingleObject(gThreadHandle, INFINITE);
    // SendCompleted was called from SendThreadProc
    return true;
  }

  return 1 == DialogBoxParamMaybeRTL(IDD_SENDDIALOG, nullptr,
                                     (DLGPROC)CrashReporterDialogProc, 0);
}

void UIError_impl(const string& message)
{
  wstring title = Str(ST_CRASHREPORTERTITLE);
  if (title.empty())
    title = L"Crash Reporter Error";

  MessageBox(nullptr, UTF8ToWide(message).c_str(), title.c_str(),
             MB_OK | MB_ICONSTOP);
}

bool UIGetIniPath(string& path)
{
  wchar_t fileName[MAX_PATH];
  if (GetModuleFileName(nullptr, fileName, MAX_PATH)) {
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
  wchar_t path[MAX_PATH] = {};
  HRESULT hRes = SHGetFolderPath(nullptr,
                                 CSIDL_APPDATA,
                                 nullptr,
                                 0,
                                 path);
  if (FAILED(hRes)) {
    // This provides a fallback for getting the path to APPDATA by querying the
    // registry when the call to SHGetFolderPath is unable to provide this path
    // (Bug 513958).
    HKEY key;
    DWORD type, dwRes;
    DWORD size = sizeof(path) - 1;
    dwRes = ::RegOpenKeyExW(HKEY_CURRENT_USER,
                            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders",
                            0,
                            KEY_READ,
                            &key);
    if (dwRes != ERROR_SUCCESS)
      return false;

    dwRes = RegQueryValueExW(key,
                             L"AppData",
                             nullptr,
                             &type,
                             (LPBYTE)&path,
                             &size);
    ::RegCloseKey(key);
    // The call to RegQueryValueExW must succeed, the type must be REG_SZ, the
    // buffer size must not equal 0, and the buffer size be a multiple of 2.
    if (dwRes != ERROR_SUCCESS || type != REG_SZ || size == 0 || size % 2 != 0)
        return false;
  }

  if (!vendor.empty()) {
    PathAppend(path, UTF8ToWide(vendor).c_str());
  }
  PathAppend(path, UTF8ToWide(product).c_str());
  PathAppend(path, L"Crash Reports");
  settings_path = WideToUTF8(path);
  return true;
}

bool UIEnsurePathExists(const string& path)
{
  if (CreateDirectory(UTF8ToWide(path).c_str(), nullptr) == 0) {
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

ifstream* UIOpenRead(const string& filename, bool binary)
{
  // adapted from breakpad's src/common/windows/http_upload.cc
  std::ios_base::openmode mode = ios::in;

  if (binary) {
    mode = mode | ios::binary;
  }

#if defined(_MSC_VER)
  ifstream* file = new ifstream();
  file->open(UTF8ToWide(filename).c_str(), mode);
#else   // GCC
  ifstream* file = new ifstream(WideToMBCP(UTF8ToWide(filename), CP_ACP).c_str(),
                                mode);
#endif  // _MSC_VER

  return file;
}

ofstream* UIOpenWrite(const string& filename,
                      bool append, // append=false
                      bool binary) // binary=false
{
  // adapted from breakpad's src/common/windows/http_upload.cc
  std::ios_base::openmode mode = ios::out;
  if (append) {
    mode = mode | ios::app;
  }
  if (binary) {
    mode = mode | ios::binary;
  }

#if defined(_MSC_VER)
  ofstream* file = new ofstream();
  file->open(UTF8ToWide(filename).c_str(), mode);
#else   // GCC
  ofstream* file = new ofstream(WideToMBCP(UTF8ToWide(filename), CP_ACP).c_str(),
                                mode);
#endif  // _MSC_VER

  return file;
}

struct FileData
{
  FILETIME timestamp;
  wstring path;
};

static bool CompareFDTime(const FileData& fd1, const FileData& fd2)
{
  return CompareFileTime(&fd1.timestamp, &fd2.timestamp) > 0;
}

void UIPruneSavedDumps(const std::string& directory)
{
  wstring wdirectory = UTF8ToWide(directory);

  WIN32_FIND_DATA fdata;
  wstring findpath = wdirectory + L"\\*.dmp";
  HANDLE dirlist = FindFirstFile(findpath.c_str(), &fdata);
  if (dirlist == INVALID_HANDLE_VALUE)
    return;

  vector<FileData> dumpfiles;

  for (BOOL ok = true; ok; ok = FindNextFile(dirlist, &fdata)) {
    FileData fd = {fdata.ftLastWriteTime, wdirectory + L"\\" + fdata.cFileName};
    dumpfiles.push_back(fd);
  }

  sort(dumpfiles.begin(), dumpfiles.end(), CompareFDTime);

  while (dumpfiles.size() > kSaveCount) {
    // get the path of the oldest file
    wstring path = (--dumpfiles.end())->path;
    DeleteFile(path.c_str());

    // s/.dmp/.extra/
    path.replace(path.size() - 4, 4, L".extra");
    DeleteFile(path.c_str());

    dumpfiles.pop_back();
  }
  FindClose(dirlist);
}

bool UIRunProgram(const string& exename,
                  const std::vector<std::string>& args,
                  bool wait)
{
  wstring cmdLine = L"\"" + UTF8ToWide(exename) + L"\" ";

  for (auto arg : args) {
    cmdLine += L"\"" + UTF8ToWide(arg) + L"\" ";
  }

  STARTUPINFO si = {};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi = {};

  if (!CreateProcess(/* lpApplicationName */ nullptr,
                     (LPWSTR)cmdLine.c_str(),
                     /* lpProcessAttributes */ nullptr,
                     /* lpThreadAttributes */ nullptr,
                     /* bInheritHandles */ false,
                     NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW,
                     /* lpEnvironment */ nullptr,
                     /* lpCurrentDirectory */ nullptr,
                     &si, &pi)) {
    return false;
  }

  if (wait) {
    WaitForSingleObject(pi.hProcess, INFINITE);
  }

  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  return true;
}

string
UIGetEnv(const string name)
{
  const wchar_t *var = _wgetenv(UTF8ToWide(name).c_str());
  if (var && *var) {
    return WideToUTF8(var);
  }

  return "";
}
