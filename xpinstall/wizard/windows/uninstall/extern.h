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

#ifndef _EXTERN_H_
#define _EXTERN_H_

#include "uninstall.h"

/* external global variables */
extern HINSTANCE        hInst;
extern HANDLE           hAccelTable;

extern HWND             hDlgUninstall;
extern HWND             hDlgMessage;
extern HWND             hWndMain;

extern LPSTR            szEGlobalAlloc;
extern LPSTR            szEStringLoad;
extern LPSTR            szEDllLoad;
extern LPSTR            szEStringNull;
extern LPSTR            szTempSetupPath;

extern LPSTR            szClassName;
extern LPSTR            szUninstallDir;
extern LPSTR            szTempDir;
extern LPSTR            szOSTempDir;
extern LPSTR            szFileIniUninstall;
extern LPSTR            szFileIniDefaultsInfo;
extern LPSTR            gszSharedFilename;

extern ULONG            ulOSType;
extern DWORD            dwScreenX;
extern DWORD            dwScreenY;

extern DWORD            gdwWhatToDo;

extern BOOL             gbAllowMultipleInstalls;

extern uninstallGen     ugUninstall;
extern diU              diUninstall;

#endif
