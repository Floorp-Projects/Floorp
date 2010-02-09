/* -*- Mode: C++; c-basic-offset: 2; tab-width: 8; indent-tabs-mode: nil; -*- */
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
 * The Original Code is Fennec Installer for WinCE.
 *
 * The Initial Developer of the Original Code is The Mozilla Foundation.
 *
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Pakhotin <alexp@mozilla.com> (original author)
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

/**
 *
 * nsInstallerDlg.cpp : Mozilla Fennec Installer UI.
 *
 */

#include <windows.h>
#include <projects.h>
#include <aygshell.h>

#include "nsSetupStrings.h"
#include "nsInstaller.h"
#include "ns7zipExtractor.h"
#include "nsInstallerDlg.h"

#define WM_DIALOGCREATED (WM_USER + 1)

const WCHAR c_sInstallPathTemplate[] = L"%s\\%s";
const WCHAR c_sExtractCardPathTemplate[] = L"\\%s%s\\%s";
const WCHAR c_sAppRegKeyTemplate[] = L"Software\\Mozilla\\%s";
const WCHAR c_sFastStartTemplate[] = L"%s\\%sfaststart.exe";

// Message handler for the dialog
BOOL CALLBACK DlgMain(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
  return nsInstallerDlg::GetInstance()->DlgMain(hDlg, message, wParam, lParam);
}

///////////////////////////////////////////////////////////////////////////////
//
// nsInstallerDlg dialog
//
///////////////////////////////////////////////////////////////////////////////

nsInstallerDlg::nsInstallerDlg()
{
  m_hInst = NULL;
  m_hDlg = NULL;
  m_bFastStart = FALSE;
  m_sExtractPath[0] = 0;
  m_sInstallPath[0] = 0;
  m_sProgramFiles[0] = 0;
  m_sErrorMsg[0] = 0;
}

nsInstallerDlg* nsInstallerDlg::GetInstance()
{
  static nsInstallerDlg dlg;
  return &dlg;
}

void nsInstallerDlg::Init(HINSTANCE hInst)
{
  m_hInst = hInst;
}

int nsInstallerDlg::DoModal()
{
  return (int)DialogBox(m_hInst, (LPCTSTR)IDD_MAIN, 0, ::DlgMain);
}

BOOL CALLBACK nsInstallerDlg::DlgMain(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
    case WM_INITDIALOG:
      return OnInitDialog(hDlg, wParam, lParam);

    case WM_DIALOGCREATED:
      return OnDialogCreated(hDlg);

    case WM_COMMAND:
      switch (LOWORD(wParam))
      {
        case IDOK:
        case IDCANCEL:
          EndDialog(hDlg, LOWORD(wParam));
          return TRUE;

        case IDC_BTN_INSTALL:
          if (OnBtnExtract())
            EndDialog(hDlg, IDOK);
          return TRUE;
      }
      break;

    case WM_CLOSE:
      EndDialog(hDlg, message);
      return TRUE;
  }
  return FALSE;
}

BOOL nsInstallerDlg::OnInitDialog(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
  // Create a Done button and size it.  
  SHINITDLGINFO shidi;
  shidi.dwMask = SHIDIM_FLAGS;
  shidi.dwFlags = SHIDIF_DONEBUTTON | SHIDIF_SIPDOWN | SHIDIF_SIZEDLGFULLSCREEN | SHIDIF_EMPTYMENU;
  shidi.hDlg = hDlg;
  SHInitDialog(&shidi);

  m_hDlg = hDlg;

  SetWindowText(m_hDlg, Strings.GetString(StrID_WindowCaption));
  SetControlWindowText(IDC_STATIC_TITLE, Strings.GetString(StrID_WindowCaption));
  SetControlWindowText(IDC_STATIC_INSTALL, Strings.GetString(StrID_InstallTo));
  SetControlWindowText(IDC_BTN_INSTALL, Strings.GetString(StrID_Install));
  SetControlWindowText(IDCANCEL, Strings.GetString(StrID_Cancel));

  if (!SHGetSpecialFolderPath(m_hDlg, m_sProgramFiles, CSIDL_PROGRAM_FILES, FALSE))
    wcscpy(m_sProgramFiles, L"\\Program Files");

  _snwprintf(m_sInstallPath, MAX_PATH, c_sInstallPathTemplate,
             m_sProgramFiles, Strings.GetString(StrID_AppShortName));

  SendMessageToControl(IDC_CMB_PATH, CB_ADDSTRING, 0, (LPARAM)(m_sInstallPath));
  FindMemCards();
  SendMessageToControl(IDC_CMB_PATH, CB_SETCURSEL, 0, 0);

  EnableWindow(GetDlgItem(m_hDlg, IDC_BTN_INSTALL), FALSE);

  PostMessage(m_hDlg, WM_DIALOGCREATED, 0, 0);

  return TRUE;
}

void nsInstallerDlg::FindMemCards()
{
  WIN32_FIND_DATA fd;
  HANDLE hFlashCard = FindFirstFlashCard(&fd);
  if (hFlashCard != INVALID_HANDLE_VALUE)
  {
    WCHAR sPath[MAX_PATH];
    do 
    {
      if (wcslen(fd.cFileName) > 0)
      {
        _snwprintf(sPath, MAX_PATH, c_sExtractCardPathTemplate,
          fd.cFileName, m_sProgramFiles, Strings.GetString(StrID_AppShortName));
        SendMessageToControl(IDC_CMB_PATH, CB_ADDSTRING, 0, (LPARAM)(sPath));
      }
    } while(FindNextFlashCard(hFlashCard, &fd) && *fd.cFileName);
  }
}

BOOL nsInstallerDlg::OnDialogCreated(HWND hDlg)
{
  BOOL bUninstallCancelled = FALSE;
  if (RunUninstall(&bUninstallCancelled) && bUninstallCancelled)
  {
    // Cancel installation
    PostMessage(hDlg, WM_CLOSE, 0, 0);
  }
  else
    EnableWindow(GetDlgItem(m_hDlg, IDC_BTN_INSTALL), TRUE);

  return TRUE;
}

BOOL nsInstallerDlg::OnBtnExtract()
{
  BOOL bResult = FALSE;

  EnableWindow(GetDlgItem(m_hDlg, IDC_BTN_INSTALL), FALSE);
  EnableWindow(GetDlgItem(m_hDlg, IDCANCEL), FALSE);

  int nPathIndex = SendMessageToControl(IDC_CMB_PATH, CB_GETCURSEL, 0, 0);
  if (nPathIndex >= 0)
    SendMessageToControl(IDC_CMB_PATH, CB_GETLBTEXT, nPathIndex, (LPARAM)(m_sInstallPath));

  wcscpy(m_sExtractPath, m_sInstallPath);

  // Remove the last directory ("Fennec") from the path as it is already in the archive
  WCHAR *sSlash = wcsrchr(m_sExtractPath, L'\\');
  if (sSlash)
    *sSlash = 0;

  ns7zipExtractor archiveExtractor(g_sExeFullFileName, g_dwExeSize, this);

  SendMessageToControl(IDC_PROGRESS, PBM_SETRANGE, 0, (LPARAM) MAKELPARAM (0, 100)); // progress in percents
  SendMessageToControl(IDC_PROGRESS, PBM_SETPOS, 0, 0);

  int nResult = archiveExtractor.Extract(m_sExtractPath);
  if (nResult == 0)
  {
    Progress(100); // 100%
    bResult = PostExtract();
  }

  EnableWindow(GetDlgItem(m_hDlg, IDC_BTN_INSTALL), TRUE);
  EnableWindow(GetDlgItem(m_hDlg, IDCANCEL), TRUE);

  if (bResult)
  {
    MessageBoxW(m_hDlg, Strings.GetString(StrID_InstalledSuccessfully),
                Strings.GetString(StrID_WindowCaption), MB_OK|MB_ICONINFORMATION);
  }
  else
  {
    WCHAR sMsg[c_nMaxErrorLen];
    if (nResult != 0)
      _snwprintf(sMsg, c_nMaxErrorLen, L"%s %d", Strings.GetString(StrID_ExtractionError), nResult);
    else
      _snwprintf(sMsg, c_nMaxErrorLen, L"%s\n%s", Strings.GetString(StrID_ThereWereErrors), m_sErrorMsg);

    MessageBoxW(m_hDlg, sMsg, Strings.GetString(StrID_WindowCaption), MB_OK|MB_ICONERROR);
  }

  return bResult;
}

LRESULT nsInstallerDlg::SendMessageToControl(int nCtlID, UINT Msg, WPARAM wParam /*= 0*/, LPARAM lParam /*= 0*/)
{
  return SendMessage(GetDlgItem(m_hDlg, nCtlID), Msg, wParam, lParam);
}

void nsInstallerDlg::SetControlWindowText(int nCtlID, const WCHAR *sText)
{
  SetWindowText(GetDlgItem(m_hDlg, nCtlID), sText);
}

///////////////////////////////////////////////////////////////////////////////
//
// ZipProgress member override
//
///////////////////////////////////////////////////////////////////////////////
void nsInstallerDlg::Progress(int n)
{
  SendMessageToControl(IDC_PROGRESS, PBM_SETPOS, (WPARAM)n, 0);
}

//////////////////////////////////////////////////////////////////////////
//
// PostExtract - additional step after extraction
//
//////////////////////////////////////////////////////////////////////////

#define RUN_AND_CHECK(fn) \
  if (!fn()) { \
    bResult = FALSE; \
    AddErrorMsg(L ## #fn ## L" failed"); \
  }

BOOL nsInstallerDlg::PostExtract()
{
  BOOL bResult = TRUE;

  m_bFastStart = FastStartFileExists();

  RUN_AND_CHECK(CreateShortcut)

  RUN_AND_CHECK(StoreInstallPath)

  MoveSetupStrings();

  RUN_AND_CHECK(SilentFirstRun)

  RUN_AND_CHECK(RunSetupCab)

  return bResult;
}

BOOL nsInstallerDlg::StoreInstallPath()
{
  HKEY hKey;
  WCHAR sRegFennecKey[MAX_PATH];
  _snwprintf(sRegFennecKey, MAX_PATH, c_sAppRegKeyTemplate, Strings.GetString(StrID_AppShortName));

  // Store the installation path - to be used by the uninstaller
  LONG result = RegCreateKeyEx(HKEY_LOCAL_MACHINE, sRegFennecKey, 0, REG_NONE,
                               REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL);
  if (result == ERROR_SUCCESS)
  {
    result = RegSetValueEx(hKey, L"Path", 0, REG_SZ, (BYTE* const)m_sInstallPath, (wcslen(m_sInstallPath)+1)*sizeof(WCHAR));
    RegCloseKey(hKey);
  }

  return (result == ERROR_SUCCESS);
}

// Creates shortcuts for Fennec and (optionally) FastStart service.
// Note: The shortcut names have to be in sync with DeleteShortcut in Uninstaller.cpp
BOOL nsInstallerDlg::CreateShortcut()
{
  BOOL result = FALSE;

  WCHAR sFennecPath[MAX_PATH];
  _snwprintf(sFennecPath, MAX_PATH, L"\"%s\\%s.exe\"", m_sInstallPath, Strings.GetString(StrID_AppShortName));

  WCHAR sProgramsPath[MAX_PATH];
  if (SHGetSpecialFolderPath(m_hDlg, sProgramsPath, CSIDL_PROGRAMS, FALSE))
  {
    WCHAR sShortcutPath[MAX_PATH];
    _snwprintf(sShortcutPath, MAX_PATH, L"%s\\%s.lnk", sProgramsPath, Strings.GetString(StrID_AppShortName));

    // Delete the old shortcut if it exists
    if(SetFileAttributes(sShortcutPath, FILE_ATTRIBUTE_NORMAL))
      DeleteFile(sShortcutPath);

    result = SHCreateShortcut(sShortcutPath, sFennecPath);
  }

  if (m_bFastStart)
  {
    WCHAR sFastStartPath[MAX_PATH];
    _snwprintf(sFastStartPath, MAX_PATH, L"\"%s\\%sfaststart.exe\"", m_sInstallPath, Strings.GetString(StrID_AppShortName));

    WCHAR sStartupPath[MAX_PATH];
    if (SHGetSpecialFolderPath(m_hDlg, sStartupPath, CSIDL_STARTUP, FALSE))
    {
      WCHAR sStartupShortcutPath[MAX_PATH];
      _snwprintf(sStartupShortcutPath, MAX_PATH, L"%s\\%sFastStart.lnk", sStartupPath, Strings.GetString(StrID_AppShortName));

      if(SetFileAttributes(sStartupShortcutPath, FILE_ATTRIBUTE_NORMAL))
        DeleteFile(sStartupShortcutPath);

      result = SHCreateShortcut(sStartupShortcutPath, sFastStartPath) && result;
    }
  }

  return result;
}

BOOL nsInstallerDlg::MoveSetupStrings()
{
  WCHAR sExtractedStringsFile[MAX_PATH];
  _snwprintf(sExtractedStringsFile, MAX_PATH, L"%s\\%s", m_sExtractPath, c_sStringsFile);

  WCHAR sNewStringsFile[MAX_PATH];
  _snwprintf(sNewStringsFile, MAX_PATH, L"%s\\%s", m_sInstallPath, c_sStringsFile);

  return MoveFile(sExtractedStringsFile, sNewStringsFile);
}

BOOL nsInstallerDlg::SilentFirstRun()
{
  SetControlWindowText(IDC_STATUS_TEXT, Strings.GetString(StrID_CreatingUserProfile));
  UpdateWindow(m_hDlg); // make sure the text is drawn

  WCHAR sCmdLine[MAX_PATH];
  WCHAR *sParams = NULL;
  if (m_bFastStart)
  {
    // Run fast start exe instead - it will create the profile and stay in the background
    _snwprintf(sCmdLine, MAX_PATH, c_sFastStartTemplate, m_sInstallPath, Strings.GetString(StrID_AppShortName));
  }
  else
  {
    _snwprintf(sCmdLine, MAX_PATH, L"%s\\%s.exe", m_sInstallPath, Strings.GetString(StrID_AppShortName));
    sParams = L"-silent -nosplash";
  }
  PROCESS_INFORMATION pi;
  BOOL bResult = CreateProcess(sCmdLine, sParams,
                               NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi);
  if (bResult)
  {
    // Wait for it to finish (since the system is likely to be busy
    // while it launches anyways). The process may never terminate
    // if FastStart is enabled, so don't wait longer than 10 seconds.
    WaitForSingleObject(pi.hProcess, 10000);
  }

  if (m_bFastStart)
  {
    // When the FastStart is enabled, Fennec is being loaded in the background,
    // user cannot launch it with a shortcut, and the system is busy at the moment,
    // so we'll just wait here.

    // Class name: appName + "MessageWindow"
    WCHAR sClassName[MAX_PATH];
    _snwprintf(sClassName, MAX_PATH, L"%s%s", Strings.GetString(StrID_AppShortName), L"MessageWindow");

    // Wait until the hidden window gets created or for some timeout (~10s seems to be reasonable)
    HWND handle = NULL;
    for (int i = 0; i < 20 && !handle; i++)
    {
      handle = ::FindWindowW(sClassName, NULL);
      Sleep(500);
    }
  }

  SetWindowText(GetDlgItem(m_hDlg, IDC_STATUS_TEXT), L"");
  return bResult;
}

BOOL nsInstallerDlg::RunSetupCab()
{
  BOOL bResult = FALSE;

  WCHAR sCabPath[MAX_PATH];
  WCHAR sParams[MAX_PATH + 20];
  _snwprintf(sCabPath, MAX_PATH, L"%s\\setup.cab", m_sInstallPath);

  // Run "wceload.exe /noui Fennec.cab"
  _snwprintf(sParams, MAX_PATH, L"/noui \"%s\"", sCabPath);
  PROCESS_INFORMATION pi;
  bResult = CreateProcess(L"\\Windows\\wceload.exe",
                          sParams, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi);
  if (bResult)
  {
    // Wait for it to finish
    WaitForSingleObject(pi.hProcess, INFINITE);
  }

  return bResult;
}

void nsInstallerDlg::AddErrorMsg(WCHAR* sErr)
{
  WCHAR sMsg[c_nMaxErrorLen];
  _snwprintf(sMsg, c_nMaxErrorLen, L"%s. LastError = %d\n", sErr, GetLastError());
  wcsncat(m_sErrorMsg, sMsg, c_nMaxErrorLen - wcslen(m_sErrorMsg));
}

//////////////////////////////////////////////////////////////////////////
//
// Uninstall previous installation
//
//////////////////////////////////////////////////////////////////////////

BOOL nsInstallerDlg::GetInstallPath(WCHAR *sPath)
{
  HKEY hKey;
  WCHAR sRegFennecKey[MAX_PATH];
  _snwprintf(sRegFennecKey, MAX_PATH, c_sAppRegKeyTemplate, Strings.GetString(StrID_AppShortName));

  LONG result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, sRegFennecKey, 0, KEY_ALL_ACCESS, &hKey);
  if (result == ERROR_SUCCESS)
  {
    DWORD dwType = NULL;
    DWORD dwCount = MAX_PATH * sizeof(WCHAR);
    result = RegQueryValueEx(hKey, L"Path", NULL, &dwType, (LPBYTE)sPath, &dwCount);

    RegCloseKey(hKey);
  }

  return (result == ERROR_SUCCESS);
}

BOOL nsInstallerDlg::RunUninstall(BOOL *pbCancelled)
{
  BOOL bResult = FALSE;
  WCHAR sUninstallPath[MAX_PATH];
  if (GetInstallPath(sUninstallPath))
  {
    if (wcslen(sUninstallPath) > 0 && sUninstallPath[wcslen(sUninstallPath)-1] != '\\')
      wcscat(sUninstallPath, L"\\");

    WCHAR sParam[MAX_PATH+10];
    _snwprintf(sParam, MAX_PATH+9, L"[remove] %s", sUninstallPath);

    wcscat(sUninstallPath, L"uninstall.exe");

    PROCESS_INFORMATION pi;
    bResult = CreateProcess(sUninstallPath, sParam,
                            NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi);
    if (bResult)
    {
      // Wait for it to finish
      WaitForSingleObject(pi.hProcess, INFINITE);

      if (pbCancelled != NULL)
      {
        DWORD dwExitCode = 0;
        GetExitCodeProcess(pi.hProcess, &dwExitCode);
        *pbCancelled = (dwExitCode == IDCANCEL);
      }
    }
  }
  return bResult;
}

//////////////////////////////////////////////////////////////////////////
//
// Helper functions
//
//////////////////////////////////////////////////////////////////////////

BOOL nsInstallerDlg::FastStartFileExists()
{
  WCHAR sFastStartPath[MAX_PATH];
  _snwprintf(sFastStartPath, MAX_PATH, c_sFastStartTemplate, m_sInstallPath, Strings.GetString(StrID_AppShortName));
  // Check if file exists
  return (GetFileAttributes(sFastStartPath) != INVALID_FILE_ATTRIBUTES);
}
