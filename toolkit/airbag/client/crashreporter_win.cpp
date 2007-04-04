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

#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <shellapi.h>
#include "resource.h"
#include "client/windows/sender/crash_report_sender.h"
#include <fstream>

#define CRASH_REPORTER_KEY L"Software\\Mozilla\\Crash Reporter"
#define CRASH_REPORTER_VALUE L"Enabled"

#define WM_UPLOADCOMPLETE WM_APP

using std::string;
using std::wstring;
using std::map;
using std::ifstream;

bool ReadConfig();
BOOL CALLBACK EnableDialogProc(HWND hwndDlg, UINT message, WPARAM wParam,
                               LPARAM lParam);
BOOL CALLBACK SendDialogProc(HWND hwndDlg, UINT message, WPARAM wParam,
                             LPARAM lParam);
HANDLE CreateSendThread(HWND hDlg, LPCTSTR dumpFile);
bool CheckCrashReporterEnabled(bool* enabled);
void SetCrashReporterEnabled(bool enabled);
bool SendCrashReport(wstring dumpFile,
                     const map<wstring,wstring> *query_parameters);
DWORD WINAPI SendThreadProc(LPVOID param);

typedef struct {
  HWND hDlg;
  wstring dumpFile;
  const map<wstring,wstring> *query_parameters;
} SENDTHREADDATA;

TCHAR sendURL[2048] = L"\0";
bool  deleteDump = true;

// Sort of a hack to get l10n
enum {
  ST_OK,
  ST_CANCEL,
  ST_CRASHREPORTERTITLE,
  ST_CRASHREPORTERDESCRIPTION,
  ST_RADIOENABLE,
  ST_RADIODISABLE,
  ST_SENDTITLE,
  ST_SUBMITSUCCESS,
  ST_SUBMITFAILED,
  NUM_STRINGS
};

LPCTSTR stringNames[] = {
  L"Ok",
  L"Cancel",
  L"CrashReporterTitle",
  L"CrashReporterDescription",
  L"RadioEnable",
  L"RadioDisable",
  L"SendTitle",
  L"SubmitSuccess",
  L"SubmitFailed"
};

LPTSTR strings[NUM_STRINGS];

const wchar_t* kExtraDataExtension = L".extra";

void DoInitCommonControls()
{
	INITCOMMONCONTROLSEX ic;
	ic.dwSize = sizeof(INITCOMMONCONTROLSEX);
	ic.dwICC = ICC_PROGRESS_CLASS;
	InitCommonControlsEx(&ic);
  // also get the rich edit control
  LoadLibrary(L"riched20.dll");
}

bool LoadStrings(LPCTSTR fileName)
{
  for (int i=ST_OK; i<NUM_STRINGS; i++) {
    strings[i] = new TCHAR[1024];
    GetPrivateProfileString(L"Strings", stringNames[i], L"", strings[i], 1024, fileName);
    if (stringNames[i][0] == '\0')
      return false;
    //XXX should probably check for strings > 1024...
  }
  return true;
}

bool ReadConfig()
{
  TCHAR fileName[MAX_PATH];

  if (GetModuleFileName(NULL, fileName, MAX_PATH)) {
    // get crashreporter ini
    LPTSTR s = wcsrchr(fileName, '.');
    if (s) {
      wcscpy(s, L".ini");

      GetPrivateProfileString(L"Settings", L"URL", L"", sendURL, 2048, fileName);

      TCHAR tmp[16];
      GetPrivateProfileString(L"Settings", L"Delete", L"1", tmp, 16, fileName);
      deleteDump = _wtoi(tmp) > 0;

      return LoadStrings(fileName);
    }
  }
  return false;
}

BOOL CALLBACK EnableDialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
  switch (message) {
  case WM_INITDIALOG:
    SetWindowText(hwndDlg, strings[ST_CRASHREPORTERTITLE]);
    SetDlgItemText(hwndDlg, IDOK, strings[ST_OK]);
    SetDlgItemText(hwndDlg, IDC_RADIOENABLE, strings[ST_RADIOENABLE]);
    SetDlgItemText(hwndDlg, IDC_RADIODISABLE, strings[ST_RADIODISABLE]);
    SetDlgItemText(hwndDlg, IDC_DESCRIPTIONTEXT, strings[ST_CRASHREPORTERDESCRIPTION]);
    SendDlgItemMessage(hwndDlg, IDC_DESCRIPTIONTEXT, EM_SETTARGETDEVICE, (WPARAM)NULL, 0);
    SetFocus(GetDlgItem(hwndDlg, IDC_RADIOENABLE));
    CheckRadioButton(hwndDlg, IDC_RADIOENABLE, IDC_RADIODISABLE, lParam ? IDC_RADIOENABLE : IDC_RADIODISABLE);
    return FALSE;

  case WM_COMMAND:
    if (LOWORD(wParam) == IDOK && HIWORD(wParam) == BN_CLICKED)
      {
        UINT enableChecked = IsDlgButtonChecked(hwndDlg, IDC_RADIOENABLE);
        EndDialog(hwndDlg, (enableChecked > 0) ? 1 : 0);
      }
    return FALSE;

  default: 
    return FALSE; 
  } 
}

bool GetRegValue(HKEY hRegKey, LPCTSTR valueName, DWORD* value)
{
	DWORD type, dataSize;
	dataSize = sizeof(DWORD);
	if (RegQueryValueEx(hRegKey, valueName, NULL, &type, (LPBYTE)value, &dataSize) == ERROR_SUCCESS
		&& type == REG_DWORD)
		return true;

	return false;
}

bool CheckCrashReporterEnabled(bool* enabled)
{
  *enabled = false;
  bool found = false;
  HKEY hRegKey;
  DWORD val;
  // see if our reg key is set globally
  if (RegOpenKey(HKEY_LOCAL_MACHINE, CRASH_REPORTER_KEY, &hRegKey) == ERROR_SUCCESS)
    {
      if (GetRegValue(hRegKey, CRASH_REPORTER_VALUE, &val))
        {
          *enabled = (val == 1);
          found = true;
        }
      RegCloseKey(hRegKey);
    }
  else
    {
      // look for it in user settings
      if (RegOpenKey(HKEY_CURRENT_USER, CRASH_REPORTER_KEY, &hRegKey) == ERROR_SUCCESS)	
        {
          if (GetRegValue(hRegKey, CRASH_REPORTER_VALUE, &val))
            {
              *enabled = (val == 1);
              found = true;
            }
          RegCloseKey(hRegKey);
        }
    }

  // didn't find our reg key, ask user
  return found;
}

void SetCrashReporterEnabled(bool enabled)
{
  HKEY hRegKey;
  if (RegCreateKey(HKEY_CURRENT_USER, CRASH_REPORTER_KEY, &hRegKey) == ERROR_SUCCESS) {
    DWORD data = (enabled ? 1 : 0);
    RegSetValueEx(hRegKey, CRASH_REPORTER_VALUE, 0, REG_DWORD, (LPBYTE)&data, sizeof(data));
    RegCloseKey(hRegKey);
  }
}

BOOL CALLBACK SendDialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
  static bool finishedOk = false;
  static HANDLE hThread = NULL;

  switch (message) {
  case WM_INITDIALOG:
    {
      //init strings
      SetWindowText(hwndDlg, strings[ST_SENDTITLE]);
      SetDlgItemText(hwndDlg, IDCANCEL, strings[ST_CANCEL]);
      // init progressmeter
      SendDlgItemMessage(hwndDlg, IDC_PROGRESS, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
      SendDlgItemMessage(hwndDlg, IDC_PROGRESS, PBM_SETPOS, 0, 0);
      // now create a thread to actually do the sending
      SENDTHREADDATA* td = (SENDTHREADDATA*)lParam;
      td->hDlg = hwndDlg;
      CreateThread(NULL, 0, SendThreadProc, td, 0, NULL);
    }
    return TRUE;

  case WM_UPLOADCOMPLETE:
    WaitForSingleObject(hThread, INFINITE);
    finishedOk = (wParam == 1);
    if (finishedOk) {
      SendDlgItemMessage(hwndDlg, IDC_PROGRESS, PBM_SETPOS, 100, 0);
      MessageBox(hwndDlg, strings[ST_SUBMITSUCCESS], strings[ST_CRASHREPORTERTITLE], MB_OK | MB_ICONINFORMATION);
    }
    else {
      MessageBox(hwndDlg, strings[ST_SUBMITFAILED], strings[ST_CRASHREPORTERTITLE], MB_OK | MB_ICONERROR);
    }
    EndDialog(hwndDlg, finishedOk ? 1 : 0);
    return TRUE;

  case WM_COMMAND:
    if (LOWORD(wParam) == IDCANCEL && HIWORD(wParam) == BN_CLICKED) {
      EndDialog(hwndDlg, finishedOk ? 1 : 0);
    }
    return TRUE;

  default:
    return FALSE; 
  } 
}

bool SendCrashReport(wstring dumpFile,
                     const map<wstring,wstring> *query_parameters)
{
  SENDTHREADDATA td;
  td.hDlg = NULL;
  td.dumpFile = dumpFile;
  td.query_parameters = query_parameters;

  int res = (int)DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_SENDDIALOG), NULL,
                                (DLGPROC)SendDialogProc, (LPARAM)&td);

  return (res >= 0);
}

DWORD WINAPI SendThreadProc(LPVOID param)
{
  bool finishedOk;
  SENDTHREADDATA* td = (SENDTHREADDATA*)param;

  wstring url(sendURL);
  wstring result;

  if (url.empty()) {
    finishedOk = false;
  }
  else {
    finishedOk = (google_breakpad::CrashReportSender
      ::SendCrashReport(url,
                        *(td->query_parameters),
                        td->dumpFile,
                        &result)
                  == google_breakpad::RESULT_SUCCEEDED);
  }
  PostMessage(td->hDlg, WM_UPLOADCOMPLETE, finishedOk ? 1 : 0, 0);

  return 0;
}

//XXX: change this to use Breakpad's string conversion functions
// when we update to latest SVN
bool ConvertString(const char* utf8_string, wstring& ucs2_string)
{
  wchar_t *buffer = NULL;
  int buffer_size = MultiByteToWideChar(CP_ACP, 0, utf8_string,
                                        -1, NULL, 0);
  if(buffer_size == 0)
    return false;

  buffer = new wchar_t[buffer_size];
  if(buffer == NULL)
    return false;
  
  MultiByteToWideChar(CP_ACP, 0, utf8_string,
                      -1, buffer, buffer_size);
  ucs2_string = buffer;
  delete [] buffer;
  return true;
}

void ReadExtraData(const wstring& filename,
                   map<wstring, wstring>& query_parameters)
{
#if _MSC_VER >= 1400  // MSVC 2005/8
  ifstream file;
  file.open(filename.c_str(), std::ios::in);
#else  // _MSC_VER >= 1400
  ifstream file(_wfopen(filename.c_str(), L"rb"));
#endif  // _MSC_VER >= 1400
  if (file.is_open()) {
    do {
      string in_line;
      std::getline(file, in_line);
      if (!in_line.empty()) {
        int pos = in_line.find('=');
        if (pos >= 0) {
          wstring key, value;
          ConvertString(in_line.substr(0, pos).c_str(), key);
          ConvertString(in_line.substr(pos + 1,
                                       in_line.length() - pos - 1).c_str(),
                        value);
          query_parameters[key] = value;
        }
      }
    } while(!file.eof());
    file.close();
  }
}

wstring GetExtraDataFilename(const wstring& dumpfile)
{
  wstring filename(dumpfile);
  int dot = filename.rfind('.');
  if (dot < 0)
    return L"";

  filename.replace(dot, filename.length() - dot, kExtraDataExtension);
  return filename;
}

int main(int argc, char **argv)
{
  map<wstring,wstring> query_parameters;

	DoInitCommonControls();
  if (!ReadConfig()) {
    MessageBox(NULL, L"Missing crashreporter.ini file", L"Crash Reporter Error", MB_OK | MB_ICONSTOP);
    return 0;
  }

  wstring dumpfile;
  bool enabled = false;

  if (argc > 1) {
    if (!ConvertString(argv[1], dumpfile))
      return 0;
  }

  if (dumpfile.empty()) {
    // no dump file specified, just ask about enabling
    if (!CheckCrashReporterEnabled(&enabled))
      enabled = true;

    enabled = (1 == DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_ENABLEDIALOG), NULL, (DLGPROC)EnableDialogProc, (LPARAM)enabled));
    SetCrashReporterEnabled(enabled);
  }
  else {
    wstring extrafile = GetExtraDataFilename(dumpfile);
    if (!extrafile.empty())
      ReadExtraData(extrafile, query_parameters);

    if (!CheckCrashReporterEnabled(&enabled)) {
      //ask user if crash reporter should be enabled
      enabled = (1 == DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_ENABLEDIALOG), NULL, (DLGPROC)EnableDialogProc, (LPARAM)true));
      SetCrashReporterEnabled(enabled);
    }
    // if enabled, send crash report
    if (enabled) {
      if (SendCrashReport(dumpfile, &query_parameters) && deleteDump) {
        DeleteFile(dumpfile.c_str());
        if (!extrafile.empty())
          DeleteFile(extrafile.c_str());
      }
      //TODO: show details?
    }
  }
}

#if defined(XP_WIN) && !defined(__GNUC__)
// We need WinMain in order to not be a console app.  This function is unused
// if we are a console application.
int WINAPI WinMain( HINSTANCE, HINSTANCE, LPSTR args, int )
{
  // Do the real work.
  return main(__argc, __argv);
}
#endif
