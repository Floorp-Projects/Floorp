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
 *     IBM Corp. 
 */
#define INCL_WINSYS

#include <os2.h>
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
  ULONG dwFileFound;
  ULONG dwRv = 0;

  dwFileFound = GetLogFile(ugUninstall.szLogPath, ugUninstall.szLogFilename, szFileInstallLog, sizeof(szFileInstallLog));
  while(dwFileFound)
  {
    if((silFile = InitSilNodes(szFileInstallLog)) != NULL)
    {
      UndoDesktopIntegration();
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

MRESULT EXPENTRY DlgProcUninstall(HWND hDlg, UINT msg, MPARAM wParam, LONG lParam)
{
   PSZ pszFontNameSize;
   ULONG ulFontNameSizeLength;
   char  szBuf[MAX_BUF];
   RECTL  rDlg;
   
   pszFontNameSize = myGetSysFont();
   ulFontNameSizeLength = sizeof(pszFontNameSize) +1;

  switch(msg)
  {
    case WM_INITDLG:
      SetWindowText(hDlg, diUninstall.szTitle);
      wsprintf(szBuf, diUninstall.szMessage0, ugUninstall.szDescription);
      SetDlgItemText(hDlg, IDC_MESSAGE0, szBuf);
      GetPrivateProfileString("Dialog Uninstall", "Uninstall", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      SetDlgItemText(hDlg, IDWIZNEXT, szBuf);
      GetPrivateProfileString("Dialog Uninstall", "Cancel", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      SetDlgItemText(hDlg, MBID_CANCEL, szBuf);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_MESSAGE0), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDWIZNEXT), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, MBID_CANCEL), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
 

      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg, HWND_TOP, (dwScreenX/2)-(rDlg.xRight/2), (dwScreenY/2)-(rDlg.yBottom/2), 0, 0, SWP_NOSIZE);

      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDWIZNEXT:
          EnableWindow(GetDlgItem(hDlg, IDWIZNEXT), FALSE);
          EnableWindow(GetDlgItem(hDlg, MBID_CANCEL), FALSE);
          ParseAllUninstallLogs();
          DestroyWindow(hDlg);
          PostQuitMessage(0);
          break;

        case MBID_CANCEL:
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

MRESULT EXPENTRY DlgProcWhatToDo(HWND hDlg, UINT msg, MPARAM wParam, LONG lParam)
{
   PSZ pszFontNameSize;
   ULONG ulFontNameSizeLength;
   char  szBuf[MAX_BUF];
   RECTL  rDlg;
   
   pszFontNameSize = myGetSysFont();
   ulFontNameSizeLength = sizeof(pszFontNameSize) +1;

  switch(msg)
  {
    case WM_INITDLG:
      GetPrivateProfileString("Messages", "DLG_REMOVE_FILE_TITLE", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      SetWindowText(hDlg, szBuf);

      if((PSZ)lParam != NULL)
        SetDlgItemText(hDlg, IDC_STATIC_SHARED_FILENAME, (PSZ)lParam);

      GetPrivateProfileString("Dialog Uninstall", "Message1", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      SetDlgItemText(hDlg, IDC_MESSAGE0, szBuf);
      GetPrivateProfileString("Dialog Uninstall", "Message2", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      SetDlgItemText(hDlg, IDC_MESSAGE1, szBuf);
      GetPrivateProfileString("Dialog Uninstall", "FileName", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      SetDlgItemText(hDlg, IDC_STATIC, szBuf);
      GetPrivateProfileString("Dialog Uninstall", "No", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      SetDlgItemText(hDlg, MBID_NO, szBuf);
      GetPrivateProfileString("Dialog Uninstall", "NoToAll", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      SetDlgItemText(hDlg, ID_NO_TO_ALL, szBuf);
      GetPrivateProfileString("Dialog Uninstall", "Yes", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      SetDlgItemText(hDlg, MBID_YES, szBuf);
      GetPrivateProfileString("Dialog Uninstall", "YesToAll", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      SetDlgItemText(hDlg, ID_YES_TO_ALL, szBuf);
       
      WinSetPresParam(WinWindowFromID(hDlg, IDC_MESSAGE0), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_MESSAGE1), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, IDC_STATIC_SHARED_FILENAME), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, MBID_NO), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, ID_NO_TO_ALL), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, MBID_YES), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
      WinSetPresParam(WinWindowFromID(hDlg, ID_YES_TO_ALL), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);
     
      if(GetClientRect(hDlg, &rDlg))
        SetWindowPos(hDlg, HWND_TOP, (dwScreenX/2)-(rDlg.xRight/2), (dwScreenY/2)-(rDlg.yBottom/2), 0, 0, SWP_NOSIZE);

      break;

    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case MBID_NO:
          EndDialog(hDlg, WTD_NO);
          break;

        case ID_NO_TO_ALL:
          EndDialog(hDlg, WTD_NO_TO_ALL);
          break;

        case MBID_YES:
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

MRESULT EXPENTRY DlgProcMessage(HWND hDlg, UINT msg, MPARAM wParam, LONG lParam)
{
  RECTL      rDlg;
  HWND      hSTMessage = GetDlgItem(hDlg, IDC_MESSAGE); 
  HDC       hdcSTMessage;
  SIZEL      sizeString;
  LOGFONT   logFont;
  HFONT     hfontTmp;
  HFONT     hfontOld;
  int       i;
  PSZ pszFontNameSize;
  ULONG ulFontNameSizeLength;
    
   pszFontNameSize = myGetSysFont();
   ulFontNameSizeLength = sizeof(pszFontNameSize) +1;

  switch(msg)
  {
    case WM_INITDLG:
        WinSetPresParam(WinWindowFromID(hDlg, IDC_MESSAGE), PP_FONTNAMESIZE, ulFontNameSizeLength, pszFontNameSize);          
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

          GetTextExtentPoint32(hdcSTMessage, (PSZ)lParam, lstrlen((PSZ)lParam), &sizeString);
          SelectObject(hdcSTMessage, hfontOld);
          DeleteObject(hfontTmp);
          ReleaseDC(hSTMessage, hdcSTMessage);

          SetWindowPos(hDlg, HWND_TOP,
                      (dwScreenX/2)-((sizeString.cx + 40)/2), (dwScreenY/2)-((sizeString.cy + 40)/2),
                      sizeString.cx + 40, sizeString.cy + 40,
                      SWP_SHOW);

          if(GetClientRect(hDlg, &rDlg))
            SetWindowPos(hSTMessage,
                         HWND_TOP,
                         rDlg.xLeft,
                         rDlg.yTop,
                         rDlg.xRight,
                         rDlg.yBottom,
                         SWP_SHOW);

          for(i = 0; i < lstrlen((PSZ)lParam); i++)
          {
            if((((PSZ)lParam)[i] == '\r') || (((PSZ)lParam)[i] == '\n')) 
              ((PSZ)lParam)[i] = ' ';
          }

          SetDlgItemText(hDlg, IDC_MESSAGE, (PSZ)lParam);
          break;
      }
      break;
  }
  return(0);
}

void ProcessWindowsMessages()
{
  QMSG msg;

  while(PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

void ShowMessage(PSZ szMessage, BOOL bShow)
{
  char szBuf[MAX_BUF];

  if(ugUninstall.dwMode != SILENT)
  {
    if((bShow) && (hDlgMessage == NULL))
    {
      ZeroMemory(szBuf, sizeof(szBuf));
      GetPrivateProfileString("Messages", "MB_MESSAGE_STR", "", szBuf, sizeof(szBuf), szFileIniUninstall);
       hDlgMessage =InstantiateDialog(hWndMain, DLG_MESSAGE, szBuf, DlgProcMessage);
      SendMessage(hDlgMessage, WM_COMMAND, IDC_MESSAGE, (MPARAM)szMessage);
    }
    else if(!bShow && hDlgMessage)
    {
      DestroyWindow(hDlgMessage);
      hDlgMessage = NULL;
    }
  }
}

HWND InstantiateDialog(HWND hParent, ULONG dwDlgID, PSZ szTitle, PFNWP wpDlgProc)
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

PSZ myGetSysFont()
{
  PSZ szFontNameSize[MAXNAMEL];
  PrfQueryProfileString(HINI_USER,"PM_SYSTEMFONTS", "IconText", "9.WarpSans", szFontNameSize, MAXNAMEL);
  return szFontNameSize;
}
