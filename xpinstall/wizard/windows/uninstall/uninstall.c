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

#include "uninstall.h"
#include "extra.h"
#include "dialogs.h"
#include "ifuncns.h"

/* global variables */
HINSTANCE       hInst;

HANDLE          hAccelTable;

HWND            hDlgUninstall;
HWND            hDlgMessage;
HWND            hWndMain;

LPSTR           szEGlobalAlloc;
LPSTR           szEStringLoad;
LPSTR           szEDllLoad;
LPSTR           szEStringNull;
LPSTR           szTempSetupPath;

LPSTR           szClassName;
LPSTR           szUninstallDir;
LPSTR           szTempDir;
LPSTR           szOSTempDir;
LPSTR           szFileIniUninstall;
LPSTR           szFileIniDefaultsInfo;
LPSTR           gszSharedFilename;

ULONG           ulOSType;
DWORD           dwScreenX;
DWORD           dwScreenY;

DWORD           gdwWhatToDo;

uninstallGen    ugUninstall;
diU             diUninstall;

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
  /***********************************************************************/
  /* HANDLE hInstance;       handle for this instance                    */
  /* HANDLE hPrevInstance;   handle for possible previous instances      */
  /* LPSTR  lpszCmdLine;     long pointer to exec command line           */
  /* int    nCmdShow;        Show code for main window display           */
  /***********************************************************************/

  MSG   msg;
  char  szBuf[MAX_BUF];

  if(!hPrevInstance)
  {
    hInst = GetModuleHandle(NULL);
    if(Initialize(hInst))
    {
      PostQuitMessage(1);
    }
    else if(!InitApplication(hInst))
    {
      char szEFailed[MAX_BUF];

      if(NS_LoadString(hInst, IDS_ERROR_FAILED, szEFailed, MAX_BUF) == WIZ_OK)
      {
        wsprintf(szBuf, szEFailed, "InitApplication().");
        PrintError(szBuf, ERROR_CODE_SHOW);
      }
      PostQuitMessage(1);
    }
    else if(ParseUninstallIni(lpszCmdLine))
    {
      PostQuitMessage(1);
    }
    else if(ugUninstall.bUninstallFiles == TRUE)
    {
      if(diUninstall.bShowDialog == TRUE)
        hDlgUninstall = InstantiateDialog(hWndMain, DLG_UNINSTALL, diUninstall.szTitle, DlgProcUninstall);
      // Assumes that SHOWICONS, HIDEICONS, and SETDEFAULT never show dialogs
      else if((ugUninstall.dwMode == SHOWICONS) || (ugUninstall.dwMode == HIDEICONS))
        ParseDefaultsInfo();
      else if(ugUninstall.dwMode == SETDEFAULT)
        SetDefault();
      else
        ParseAllUninstallLogs();
    }
  }

  if((ugUninstall.bUninstallFiles == TRUE) && (diUninstall.bShowDialog == TRUE))
  {
    while(GetMessage(&msg, NULL, 0, 0))
    {
      if((!IsDialogMessage(hDlgUninstall, &msg)) && (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)))
      {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }
  }

  /* Do clean up before exiting from the application */
  DeInitialize();

  return(msg.wParam);
} /*  End of WinMain */

