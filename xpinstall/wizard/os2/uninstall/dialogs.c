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
  ULONG ulFileFound;
  ULONG ulRv = 0;

  UndoDesktopIntegration();
  ulFileFound = GetLogFile(ugUninstall.szLogPath, ugUninstall.szLogFilename, szFileInstallLog, sizeof(szFileInstallLog));
  while(ulFileFound)
  {
    if((silFile = InitSilNodes(szFileInstallLog)) != NULL)
    {
      FileDelete(szFileInstallLog);
      ulRv = Uninstall(silFile);
      DeInitSilNodes(&silFile);
      if(ulRv == WTD_CANCEL)
        break;
    }

    ulFileFound = GetLogFile(ugUninstall.szLogPath, ugUninstall.szLogFilename, szFileInstallLog, sizeof(szFileInstallLog));
  }

  if(ulRv != WTD_CANCEL)
  {
    strcpy(szFileInstallLog, ugUninstall.szLogPath);
    AppendBackSlash(szFileInstallLog, MAX_BUF);
    strcat(szFileInstallLog, ugUninstall.szLogFilename);
    if(FileExists(szFileInstallLog))
    {
      if((silFile = InitSilNodes(szFileInstallLog)) != NULL)
      {
        FileDelete(szFileInstallLog);
        Uninstall(silFile);
        DeInitSilNodes(&silFile);
      }
    }

    /* update Wininit.ini to remove itself at reboot */
    RemoveUninstaller(ugUninstall.szUninstallFilename);
  }
}

MRESULT APIENTRY DlgProcUninstall(HWND hDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  char  szBuf[MAX_BUF];
  SWP   swpDlg;

  switch(msg)
  {
    case WM_INITDLG:
      WinSetPresParam(hDlg, PP_FONTNAMESIZE,
                      strlen(ugUninstall.szDefinedFont)+1, ugUninstall.szDefinedFont);
      WinSetWindowText(hDlg, diUninstall.szTitle);
      sprintf(szBuf, diUninstall.szMessage0, ugUninstall.szDescription);
      WinSetDlgItemText(hDlg, IDC_MESSAGE0, szBuf);
      GetPrivateProfileString("Dialog Uninstall", "Yes", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      WinSetDlgItemText(hDlg, IDWIZNEXT, szBuf);
      GetPrivateProfileString("Dialog Uninstall", "No", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      WinSetDlgItemText(hDlg, DID_CANCEL, szBuf);

      WinQueryWindowPos(hDlg, &swpDlg);
      WinSetWindowPos(hDlg,
                      HWND_TOP,
                      (ulScreenX/2)-(swpDlg.cx/2),
                      (ulScreenY/2)-(swpDlg.cy/2),
                      0,
                      0,
                      SWP_MOVE);
      break;

    case WM_COMMAND:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case IDWIZNEXT:
          WinEnableWindow(WinWindowFromID(hDlg, IDWIZNEXT), FALSE);
          WinEnableWindow(WinWindowFromID(hDlg, DID_CANCEL), FALSE);
          ParseAllUninstallLogs();
          WinDestroyWindow(hDlg);
          WinPostQueueMsg(0, WM_QUIT, 0, 0);
          break;

        case DID_CANCEL:
          WinDestroyWindow(hDlg);
          WinPostQueueMsg(0, WM_QUIT, 0, 0);
          break;

        default:
          break;
      }
      break;
  }
  return(WinDefDlgProc(hDlg, msg, mp1, mp2));
}

MRESULT APIENTRY DlgProcWhatToDo(HWND hDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  char  szBuf[MAX_BUF];
  SWP   swpDlg;

  switch(msg)
  {
    case WM_INITDLG:
      WinSetPresParam(hDlg, PP_FONTNAMESIZE,
                      strlen(ugUninstall.szDefinedFont)+1, ugUninstall.szDefinedFont);
      GetPrivateProfileString("Messages", "DLG_REMOVE_FILE_TITLE", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      WinSetWindowText(hDlg, szBuf);

      if((PSZ)mp2 != NULL)
        WinSetDlgItemText(hDlg, IDC_STATIC_SHARED_FILENAME, (PSZ)mp2);

      GetPrivateProfileString("Dialog Uninstall", "Message1", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      WinSetDlgItemText(hDlg, IDC_MESSAGE0, szBuf);
      GetPrivateProfileString("Dialog Uninstall", "Message2", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      WinSetDlgItemText(hDlg, IDC_MESSAGE1, szBuf);
      GetPrivateProfileString("Dialog Uninstall", "FileName", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      WinSetDlgItemText(hDlg, IDC_STATIC, szBuf);
      GetPrivateProfileString("Dialog Uninstall", "No", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      WinSetDlgItemText(hDlg, ID_NO, szBuf);
      GetPrivateProfileString("Dialog Uninstall", "NoToAll", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      WinSetDlgItemText(hDlg, ID_NO_TO_ALL, szBuf);
      GetPrivateProfileString("Dialog Uninstall", "Yes", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      WinSetDlgItemText(hDlg, ID_YES, szBuf);
      GetPrivateProfileString("Dialog Uninstall", "YesToAll", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      WinSetDlgItemText(hDlg, ID_YES_TO_ALL, szBuf);

      /* Center dialog */
      WinQueryWindowPos(hDlg, &swpDlg);
      WinSetWindowPos(hDlg,
                      HWND_TOP,
                      (ulScreenX/2)-(swpDlg.cx/2),
                      (ulScreenY/2)-(swpDlg.cy/2),
                      0,
                      0,
                      SWP_MOVE);
      break;

    case WM_COMMAND:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case ID_NO:
          WinDismissDlg(hDlg, WTD_NO);
          break;

        case ID_NO_TO_ALL:
          WinDismissDlg(hDlg, WTD_NO_TO_ALL);
          break;

        case ID_YES:
          WinDismissDlg(hDlg, WTD_YES);
          break;

        case ID_YES_TO_ALL:
          WinDismissDlg(hDlg, WTD_YES_TO_ALL);
          break;

        default:
          break;
      }
      break;
  }
  return(WinDefDlgProc(hDlg, msg, mp1, mp2));
}

MRESULT APIENTRY DlgProcMessage(HWND hDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  SWP       swpDlg;
  HWND      hSTMessage = WinWindowFromID(hDlg, IDC_MESSAGE); /* handle to the Static Text message window */
  HPS       hpsSTMessage;
  RECTL     rectlString;
  int       i;

  switch(msg)
  {
    case WM_INITDLG:
      WinSetPresParam(hDlg, PP_FONTNAMESIZE,
                      strlen(ugUninstall.szDefinedFont)+1, ugUninstall.szDefinedFont);
      break;

    case WM_COMMAND:
      switch ( SHORT1FROMMP( mp1 ) )
      {
        case IDC_MESSAGE:
          hpsSTMessage = WinGetPS(hSTMessage);

          rectlString.xLeft = 0;
          rectlString.xRight = 10000;
          rectlString.yTop = 10000;
          rectlString.yBottom = 0;
          WinDrawText(hpsSTMessage, strlen((char*)mp2), (char*)mp2,
                            &rectlString, 0, 0,
                            DT_BOTTOM | DT_QUERYEXTENT | DT_TEXTATTRS | DT_WORDBREAK);
          WinReleasePS(hpsSTMessage);

          WinSetWindowPos(hDlg, HWND_TOP,
                      (ulScreenX/2)-((rectlString.xRight + 55)/2),
                      (ulScreenY/2)-((rectlString.yTop + 50)/2),
                      rectlString.xRight + 55,
                      rectlString.yTop + 50,
                      SWP_SIZE | SWP_MOVE);

          WinQueryWindowPos(hDlg, &swpDlg);

          WinSetWindowPos(hSTMessage,
                         HWND_TOP,
                         ulDlgFrameX,
                         ulDlgFrameY,
                         swpDlg.cx-2*ulDlgFrameX,
                         swpDlg.cy-2*ulDlgFrameY-ulTitleBarY,
                         SWP_SIZE | SWP_MOVE);

          for(i = 0; i < strlen((PSZ)mp2); i++)
          {
            if((((PSZ)mp2)[i] == '\r') || (((PSZ)mp2)[i] == '\n')) 
              ((PSZ)mp2)[i] = ' ';
          }

          WinSetDlgItemText(hDlg, IDC_MESSAGE, (PSZ)mp2);
          return (MRESULT)TRUE;
      }
      break;
  }
  return(WinDefDlgProc(hDlg, msg, mp1, mp2));
}

void ProcessWindowsMessages()
{
  QMSG qmsg;

  while(WinPeekMsg((HAB)0, &qmsg, 0, 0, 0, PM_REMOVE))
  {
    WinDispatchMsg((HAB)0, &qmsg );
  }
}

void ShowMessage(PSZ szMessage, BOOL bShow)
{
  char szBuf[MAX_BUF];

  if(ugUninstall.ulMode != SILENT)
  {
    if((bShow) && (hDlgMessage == NULL))
    {
      memset(szBuf, 0, sizeof(szBuf));
      GetPrivateProfileString("Messages", "MB_MESSAGE_STR", "", szBuf, sizeof(szBuf), szFileIniUninstall);
      hDlgMessage = InstantiateDialog(hWndMain, DLG_MESSAGE, szBuf, DlgProcMessage);
      WinSendMsg(hDlgMessage, WM_COMMAND, IDC_MESSAGE, (MPARAM)szMessage);
    }
    else if(!bShow && hDlgMessage)
    {
      WinDestroyWindow(hDlgMessage);
      hDlgMessage = NULL;
    }
  }
}

HWND InstantiateDialog(HWND hParent, ULONG ulDlgID, PSZ szTitle, PFNWP pfnwpDlgProc)
{
  char szBuf[MAX_BUF];
  HWND hDlg = NULL;
  ATOM atom;

  hDlg = WinLoadDlg(HWND_DESKTOP, hParent, pfnwpDlgProc, 0, ulDlgID, NULL);

  if (hDlg == NULL)
  {
    char szEDialogCreate[MAX_BUF];

    if(GetPrivateProfileString("Messages", "ERROR_DIALOG_CREATE", "", szEDialogCreate, sizeof(szEDialogCreate), szFileIniUninstall))
    {
      sprintf(szBuf, szEDialogCreate, szTitle);
      PrintError(szBuf, ERROR_CODE_SHOW);
    }

    WinPostQueueMsg(NULL, WM_QUIT, 1, 0);
  }

  atom = WinFindAtom(WinQuerySystemAtomTable(), CLASS_NAME);
  WinSetWindowULong(hDlg, QWL_USER, atom);

  return(hDlg);
}
