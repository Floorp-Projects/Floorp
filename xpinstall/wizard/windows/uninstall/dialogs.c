/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code,
 * released March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Sean Su <ssu@netscape.com>
 */

#include "extern.h"
#include "extra.h"
#include "dialogs.h"
#include "ifuncns.h"
#include "parser.h"
#include "rdi.h"

void ParseAllUninstallLogs()
{
  char  szFileInstallLog[MAX_BUF];
  char  szKey[MAX_BUF];
  sil   *silFile;
  DWORD dwFileFound;
  DWORD dwRv = 0;

  UndoDesktopIntegration();
  CleanupMailIntegration();
  dwFileFound = GetLogFile(ugUninstall.szLogPath, ugUninstall.szLogFilename, szFileInstallLog, sizeof(szFileInstallLog));
  while(dwFileFound)
  {
    if((silFile = InitSilNodes(szFileInstallLog)) != NULL)
    {
      FileDelete(szFileInstallLog);
      dwRv = Uninstall(silFile);
      DeInitSilNodes(&silFile);
      if(dwRv == WTD_CANCEL)
        break;
    }

    dwFileFound = GetLogFile(ugUninstall.szLogPath, ugUninstall.szLogFilename, szFileInstallLog, sizeof(szFileInstallLog));
  }

  if(dwRv != WTD_CANCEL)
  {
    lstrcpy(szFileInstallLog, ugUninstall.szLogPath);
    AppendBackSlash(szFileInstallLog, MAX_BUF);
    lstrcat(szFileInstallLog, ugUninstall.szLogFilename);
    if(FileExists(szFileInstallLog))
    {
      if((silFile = InitSilNodes(szFileInstallLog)) != NULL)
      {
        FileDelete(szFileInstallLog);
        Uninstall(silFile);
        DeInitSilNodes(&silFile);
      }
    }

    /* clean up the uninstall windows registry key */
    lstrcpy(szKey, "Software\\Microsoft\\Windows\\CurrentVersion\\uninstall\\");
    lstrcat(szKey, ugUninstall.szUninstallKeyDescription);
    RegDeleteKey(HKEY_LOCAL_MACHINE, szKey);

    /* update Wininit.ini to remove itself at reboot */
    RemoveUninstaller(ugUninstall.szUninstallFilename);
  }
}

LRESULT CALLBACK DlgProcUninstall(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  char  szBuf[MAX_BUF];
  RECT  rDlg;

  switch(msg)
  {
    case WM_INITDIALOG:
      SetWindowText(hDlg, diUninstall.szTitle);
      wsprintf(szBuf, diUninstall.szMessage0, ugUninstall.szDescription);
      SetDlgItemText(hDlg, IDC_MESSAGE0, szBuf);
      GetPrivateProfileString("Dialog Uninstall", "Yes", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      SetDlgItemText(hDlg, IDWIZNEXT, szBuf);
      GetPrivateProfileString("Dialog Uninstall", "No", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      SetDlgItemText(hDlg, IDCANCEL, szBuf);
      SendDlgItemMessage (hDlg, IDC_MESSAGE0, WM_SETFONT, (WPARAM)ugUninstall.definedFont, 0L); 
      SendDlgItemMessage (hDlg, IDWIZNEXT, WM_SETFONT, (WPARAM)ugUninstall.definedFont, 0L); 
      SendDlgItemMessage (hDlg, IDCANCEL, WM_SETFONT, (WPARAM)ugUninstall.definedFont, 0L); 

      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg, HWND_TOP, (dwScreenX/2)-(rDlg.right/2), (dwScreenY/2)-(rDlg.bottom/2), 0, 0, SWP_NOSIZE);

      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDWIZNEXT:
          EnableWindow(GetDlgItem(hDlg, IDWIZNEXT), FALSE);
          EnableWindow(GetDlgItem(hDlg, IDCANCEL), FALSE);
          ParseAllUninstallLogs();
          DestroyWindow(hDlg);
          PostQuitMessage(0);
          break;

        case IDCANCEL:
          DestroyWindow(hDlg);
          PostQuitMessage(0);
          break;

        default:
          break;
      }
      break;
  }
  return(0);
}

LRESULT CALLBACK DlgProcWhatToDo(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  char  szBuf[MAX_BUF];
  RECT  rDlg;

  switch(msg)
  {
    case WM_INITDIALOG:
      GetPrivateProfileString("Messages", "DLG_REMOVE_FILE_TITLE", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      SetWindowText(hDlg, szBuf);

      if((LPSTR)lParam != NULL)
        SetDlgItemText(hDlg, IDC_STATIC_SHARED_FILENAME, (LPSTR)lParam);

      GetPrivateProfileString("Dialog Uninstall", "Message1", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      SetDlgItemText(hDlg, IDC_MESSAGE0, szBuf);
      GetPrivateProfileString("Dialog Uninstall", "Message2", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      SetDlgItemText(hDlg, IDC_MESSAGE1, szBuf);
      GetPrivateProfileString("Dialog Uninstall", "FileName", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      SetDlgItemText(hDlg, IDC_STATIC, szBuf);
      GetPrivateProfileString("Dialog Uninstall", "No", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      SetDlgItemText(hDlg, ID_NO, szBuf);
      GetPrivateProfileString("Dialog Uninstall", "NoToAll", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      SetDlgItemText(hDlg, ID_NO_TO_ALL, szBuf);
      GetPrivateProfileString("Dialog Uninstall", "Yes", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      SetDlgItemText(hDlg, ID_YES, szBuf);
      GetPrivateProfileString("Dialog Uninstall", "YesToAll", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      SetDlgItemText(hDlg, ID_YES_TO_ALL, szBuf);

      SendDlgItemMessage (hDlg, IDC_MESSAGE0, WM_SETFONT, (WPARAM)ugUninstall.definedFont, 0L); 
      SendDlgItemMessage (hDlg, IDC_MESSAGE1, WM_SETFONT, (WPARAM)ugUninstall.definedFont, 0L); 
      SendDlgItemMessage (hDlg, IDC_STATIC, WM_SETFONT, (WPARAM)ugUninstall.definedFont, 0L); 
      SendDlgItemMessage (hDlg, IDC_STATIC_SHARED_FILENAME, WM_SETFONT, (WPARAM)ugUninstall.definedFont, 0L); 
      SendDlgItemMessage (hDlg, ID_NO, WM_SETFONT, (WPARAM)ugUninstall.definedFont, 0L); 
      SendDlgItemMessage (hDlg, ID_NO_TO_ALL, WM_SETFONT, (WPARAM)ugUninstall.definedFont, 0L); 
      SendDlgItemMessage (hDlg, ID_YES, WM_SETFONT, (WPARAM)ugUninstall.definedFont, 0L); 
      SendDlgItemMessage (hDlg, ID_YES_TO_ALL, WM_SETFONT, (WPARAM)ugUninstall.definedFont, 0L); 

      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg, HWND_TOP, (dwScreenX/2)-(rDlg.right/2), (dwScreenY/2)-(rDlg.bottom/2), 0, 0, SWP_NOSIZE);

      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case ID_NO:
          EndDialog(hDlg, WTD_NO);
          break;

        case ID_NO_TO_ALL:
          EndDialog(hDlg, WTD_NO_TO_ALL);
          break;

        case ID_YES:
          EndDialog(hDlg, WTD_YES);
          break;

        case ID_YES_TO_ALL:
          EndDialog(hDlg, WTD_YES_TO_ALL);
          break;

        default:
          break;
      }
      break;
  }
  return(0);
}

LRESULT CALLBACK DlgProcMessage(HWND hDlg, UINT msg, WPARAM wParam, LONG lParam)
{
  RECT      rDlg;
  HWND      hSTMessage = GetDlgItem(hDlg, IDC_MESSAGE); /* handle to the Static Text message window */
  HDC       hdcSTMessage;
  SIZE      sizeString;
  LOGFONT   logFont;
  HFONT     hfontTmp;
  HFONT     hfontOld;
  int       i;

  switch(msg)
  {
    case WM_INITDIALOG:
          SendDlgItemMessage (hDlg, IDC_MESSAGE, WM_SETFONT, (WPARAM)ugUninstall.definedFont, 0L); 
      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDC_MESSAGE:
          hdcSTMessage = GetWindowDC(hSTMessage);

          SystemParametersInfo(SPI_GETICONTITLELOGFONT,
                               sizeof(logFont),
                               (PVOID)&logFont,
                               0);
          hfontTmp = CreateFontIndirect(&logFont);

          if(hfontTmp)
            hfontOld = SelectObject(hdcSTMessage, hfontTmp);

          GetTextExtentPoint32(hdcSTMessage, (LPSTR)lParam, lstrlen((LPSTR)lParam), &sizeString);
          SelectObject(hdcSTMessage, hfontOld);
          DeleteObject(hfontTmp);
          ReleaseDC(hSTMessage, hdcSTMessage);

          SetWindowPos(hDlg, HWND_TOP,
                      (dwScreenX/2)-((sizeString.cx + 40)/2), (dwScreenY/2)-((sizeString.cy + 40)/2),
                      sizeString.cx + 40, sizeString.cy + 40,
                      SWP_SHOWWINDOW);

          if(GetClientRect(hDlg, &rDlg))
            SetWindowPos(hSTMessage,
                         HWND_TOP,
                         rDlg.left,
                         rDlg.top,
                         rDlg.right,
                         rDlg.bottom,
                         SWP_SHOWWINDOW);

          for(i = 0; i < lstrlen((LPSTR)lParam); i++)
          {
            if((((LPSTR)lParam)[i] == '\r') || (((LPSTR)lParam)[i] == '\n')) 
              ((LPSTR)lParam)[i] = ' ';
          }

          SetDlgItemText(hDlg, IDC_MESSAGE, (LPSTR)lParam);
          break;
      }
      break;
  }
  return(0);
}

void ProcessWindowsMessages()
{
  MSG msg;

  while(PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

void ShowMessage(LPSTR szMessage, BOOL bShow)
{
  char szBuf[MAX_BUF];

  if(ugUninstall.dwMode != SILENT)
  {
    if((bShow) && (hDlgMessage == NULL))
    {
      ZeroMemory(szBuf, sizeof(szBuf));
      GetPrivateProfileString("Messages", "MB_MESSAGE_STR", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      hDlgMessage = InstantiateDialog(hWndMain, DLG_MESSAGE, szBuf, DlgProcMessage);
      SendMessage(hDlgMessage, WM_COMMAND, IDC_MESSAGE, (LPARAM)szMessage);
    }
    else if(!bShow && hDlgMessage)
    {
      DestroyWindow(hDlgMessage);
      hDlgMessage = NULL;
    }
  }
}

HWND InstantiateDialog(HWND hParent, DWORD dwDlgID, LPSTR szTitle, WNDPROC wpDlgProc)
{
  char szBuf[MAX_BUF];
  HWND hDlg = NULL;

  if((hDlg = CreateDialog(hInst, MAKEINTRESOURCE(dwDlgID), hParent, wpDlgProc)) == NULL)
  {
    char szEDialogCreate[MAX_BUF];

    if(GetPrivateProfileString("Messages", "ERROR_DIALOG_CREATE", "", szEDialogCreate, sizeof(szEDialogCreate), szFileIniUninstall))
    {
      wsprintf(szBuf, szEDialogCreate, szTitle);
      PrintError(szBuf, ERROR_CODE_SHOW);
    }

    PostQuitMessage(1);
  }

  return(hDlg);
}
