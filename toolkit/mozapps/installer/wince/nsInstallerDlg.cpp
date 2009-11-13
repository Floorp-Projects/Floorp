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
#include <DeviceResolutionAware.h>
#define SHELL_AYGSHELL

#include "nsSetupStrings.h"
#include "nsInstaller.h"
#include "ns7zipExtractor.h"
#include "nsInstallerDlg.h"

const WCHAR c_sInstallPathTemplate[] = L"%s\\%s";
const WCHAR c_sExtractCardPathTemplate[] = L"\\%s%s\\%s";

// Message handler for the dialog
INT_PTR CALLBACK DlgMain(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
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

INT_PTR CALLBACK nsInstallerDlg::DlgMain(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
    case WM_INITDIALOG:
      return OnInitDialog(hDlg, wParam, lParam);

    case WM_COMMAND:
      switch (LOWORD(wParam))
      {
        case IDOK:
          {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
          }

        case IDCANCEL:
          {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)FALSE;
          }

        case IDC_BTN_INSTALL:
          {
            if (OnBtnExtract())
            {
              EndDialog(hDlg, IDOK);
              return (INT_PTR)TRUE;
            }
            else
              return (INT_PTR)FALSE;
          }
        }
        break;

    case WM_CLOSE:
      EndDialog(hDlg, message);
      return (INT_PTR)TRUE;

  }
  return (INT_PTR)FALSE;
}

BOOL nsInstallerDlg::OnInitDialog(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
#ifdef SHELL_AYGSHELL
  {
    // Create a Done button and size it.  
    SHINITDLGINFO shidi;
    shidi.dwMask = SHIDIM_FLAGS;
    shidi.dwFlags = SHIDIF_DONEBUTTON | SHIDIF_SIPDOWN | SHIDIF_SIZEDLGFULLSCREEN | SHIDIF_EMPTYMENU;
    shidi.hDlg = hDlg;
    SHInitDialog(&shidi);
  }
#endif // SHELL_AYGSHELL

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

  SendMessageToControl(IDC_CMB_PATH, CB_SETCURSEL, 0, 0 );

  return (INT_PTR)TRUE;
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

BOOL nsInstallerDlg::OnBtnExtract()
{
  BOOL bResult = FALSE;

  int nPathIndex = SendMessageToControl(IDC_CMB_PATH, CB_GETCURSEL, 0, 0);
  if (nPathIndex >=0 )
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
  return SendMessage( GetDlgItem(m_hDlg, nCtlID), Msg, wParam, lParam);
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
  SendMessageToControl( IDC_PROGRESS, PBM_SETPOS, (WPARAM)n, 0);
}

//////////////////////////////////////////////////////////////////////////
//
// PostExtract - additional step after extraction
//
//////////////////////////////////////////////////////////////////////////
BOOL nsInstallerDlg::PostExtract()
{
  BOOL bResult = TRUE;

  if (!CreateShortcut())
  {
    bResult = FALSE;
    AddErrorMsg(L"CreateShortcut failed");
  }

  if (!StoreInstallPath())
  {
    bResult = FALSE;
    AddErrorMsg(L"StoreInstallPath failed");
  }

  MoveSetupStrings();

  if (!SilentFirstRun())
  {
    bResult = FALSE;
    AddErrorMsg(L"SilentFirstRun failed");
  }

  return bResult;
}

BOOL nsInstallerDlg::StoreInstallPath()
{
  HKEY hKey;
  WCHAR sRegFennecKey[MAX_PATH];
  _snwprintf(sRegFennecKey, MAX_PATH, L"Software\\%s", Strings.GetString(StrID_AppShortName));

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

BOOL nsInstallerDlg::CreateShortcut()
{
  WCHAR sFennecPath[MAX_PATH];
  _snwprintf(sFennecPath, MAX_PATH, L"\"%s\\%s.exe\"", m_sInstallPath, Strings.GetString(StrID_AppShortName));

  WCHAR sProgramsPath[MAX_PATH];
  if (!SHGetSpecialFolderPath(m_hDlg, sProgramsPath, CSIDL_PROGRAMS, FALSE))
    wcscpy(sProgramsPath, L"\\Windows\\Start Menu\\Programs");

  WCHAR sShortcutPath[MAX_PATH];
  _snwprintf(sShortcutPath, MAX_PATH, L"%s\\%s.lnk", sProgramsPath, Strings.GetString(StrID_AppShortName));

  return SHCreateShortcut(sShortcutPath, sFennecPath);
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
  _snwprintf(sCmdLine, MAX_PATH, L"%s\\%s.exe", m_sInstallPath, Strings.GetString(StrID_AppShortName));
  PROCESS_INFORMATION pi;
  BOOL bResult = CreateProcess(sCmdLine, L"-silent -nosplash",
                               NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi);
  if (bResult)
  {
    // Wait for it to finish
    WaitForSingleObject(pi.hProcess, INFINITE);
  }
  SetWindowText(GetDlgItem(m_hDlg, IDC_STATUS_TEXT), L"");
  return bResult;
}

void nsInstallerDlg::AddErrorMsg(WCHAR* sErr)
{
  WCHAR sMsg[c_nMaxErrorLen];
  _snwprintf(sMsg, c_nMaxErrorLen, L"%s. LastError = %d\n", sErr, GetLastError());
  wcsncat(m_sErrorMsg, sMsg, c_nMaxErrorLen - wcslen(m_sErrorMsg));
}
