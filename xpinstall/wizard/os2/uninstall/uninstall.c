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
HWND            hDlgUninstall;
HWND            hDlgMessage;
HWND            hWndMain;

PSZ             szEGlobalAlloc;
PSZ             szEStringLoad;
PSZ             szEDllLoad;
PSZ             szEStringNull;
PSZ             szTempSetupPath;

PSZ             szClassName;
PSZ             szUninstallDir;
PSZ             szTempDir;
PSZ             szOSTempDir;
PSZ             szFileIniUninstall;
PSZ             szFileIniDefaultsInfo;
PSZ             gszSharedFilename;

ULONG           ulOSType;
ULONG           ulScreenX;
ULONG           ulScreenY;
ULONG           ulDlgFrameX;
ULONG           ulDlgFrameY;
ULONG           ulTitleBarY;

ULONG           gulWhatToDo;

uninstallGen    ugUninstall;
diU             diUninstall;

main(int argc, char *argv[], char *envp[])
{
  HAB hab;
  HMQ hmq;
  QMSG qmsg;
  char  szBuf[MAX_BUF];
  ATOM atom;

  hab = WinInitialize( 0 );
  hmq = WinCreateMsgQueue( hab, 0 );

  atom = WinAddAtom(WinQuerySystemAtomTable(), CLASS_NAME);

  if(Initialize(0, argv[0]))
  {
    WinPostQueueMsg(0, WM_QUIT, 1, 0);
  }
  else if(!InitApplication(0))
  {
    char szEFailed[MAX_BUF];

    if(NS_LoadString(0, IDS_ERROR_FAILED, szEFailed, MAX_BUF) == WIZ_OK)
    {
      sprintf(szBuf, szEFailed, "InitApplication().");
      PrintError(szBuf, ERROR_CODE_SHOW);
    }
    WinPostQueueMsg(0, WM_QUIT, 1, 0);
  }
  else if(ParseUninstallIni(argc, argv))
  {
    WinPostQueueMsg(0, WM_QUIT, 1, 0);
  }
  else if(ugUninstall.bUninstallFiles == TRUE)
  {
    if(diUninstall.bShowDialog == TRUE)
      hDlgUninstall = InstantiateDialog(hWndMain, DLG_UNINSTALL, diUninstall.szTitle, DlgProcUninstall);
    else
      ParseAllUninstallLogs();
  }

  if((ugUninstall.bUninstallFiles == TRUE) && (diUninstall.bShowDialog == TRUE))
  {
    while ( WinGetMsg( hab, &qmsg, NULLHANDLE, 0, 0 ) )
      WinDispatchMsg( hab, &qmsg );
  }

  /* Do clean up before exiting from the application */
  DeInitialize();

  WinDeleteAtom(WinQuerySystemAtomTable(), atom);

  WinDestroyMsgQueue( hmq );
  WinTerminate( hab ); 

}

