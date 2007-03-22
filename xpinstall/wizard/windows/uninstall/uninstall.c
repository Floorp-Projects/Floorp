/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Sean Su <ssu@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

BOOL            gbAllowMultipleInstalls = FALSE;

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
  int   iRv = WIZ_OK;
  HWND  hwndFW;

  if(!hPrevInstance)
  {
    hInst = GetModuleHandle(NULL);
    if(InitUninstallGeneral())
      PostQuitMessage(1);
    else if(ParseCommandLine(lpszCmdLine))
      PostQuitMessage(1);
    else if((hwndFW = FindWindow(CLASS_NAME_UNINSTALL_DLG, NULL)) != NULL && !gbAllowMultipleInstalls)
    {
    /* Allow only one instance of setup to run.
     * Detect a previous instance of setup, bring it to the 
     * foreground, and quit current instance */

      ShowWindow(hwndFW, SW_RESTORE);
      SetForegroundWindow(hwndFW);
      iRv = WIZ_SETUP_ALREADY_RUNNING;
      PostQuitMessage(1);
    }
    else if(Initialize(hInst))
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
    else if(ParseUninstallIni())
    {
      PostQuitMessage(1);
    }
    else if(ugUninstall.bUninstallFiles == TRUE)
    {
      if(diUninstall.bShowDialog == TRUE)
        hDlgUninstall = InstantiateDialog(hWndMain, DLG_UNINSTALL, diUninstall.szTitle, DlgProcUninstall);
      // Assumes that SHOWICONS, HIDEICONS, and SETDEFAULT never show dialogs
      else if((ugUninstall.mode == SHOWICONS) || (ugUninstall.mode == HIDEICONS))
        ParseDefaultsInfo();
      else if(ugUninstall.mode == SETDEFAULT)
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

  /* garbage collection */
  DeInitUninstallGeneral();
  if(iRv != WIZ_SETUP_ALREADY_RUNNING)
    /* Do clean up before exiting from the application */
    DeInitialize();

  return(msg.wParam);
} /*  End of WinMain */

