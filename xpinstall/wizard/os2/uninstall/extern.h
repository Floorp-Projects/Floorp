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
extern HWND             hDlgUninstall;
extern HWND             hDlgMessage;
extern HWND             hWndMain;

extern PSZ              szEGlobalAlloc;
extern PSZ              szEStringLoad;
extern PSZ              szEDllLoad;
extern PSZ              szEStringNull;
extern PSZ              szTempSetupPath;

extern PSZ              szClassName;
extern PSZ              szUninstallDir;
extern PSZ              szTempDir;
extern PSZ              szOSTempDir;
extern PSZ              szFileIniUninstall;
extern PSZ              szFileIniDefaultsInfo;
extern PSZ              gszSharedFilename;

extern ULONG            ulOSType;
extern ULONG            ulScreenX;
extern ULONG            ulScreenY;
extern ULONG            ulDlgFrameX;
extern ULONG            ulDlgFrameY;
extern ULONG            ulTitleBarY;

extern ULONG            gulWhatToDo;

extern uninstallGen     ugUninstall;
extern diU              diUninstall;

#endif
