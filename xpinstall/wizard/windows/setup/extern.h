/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code,
 * released March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Sean Su <ssu@netscape.com>
 */

#ifndef _EXTERN_H_
#define _EXTERN_H_

#include "setup.h"

/* external global variables */
extern HINSTANCE        hInst;
extern HINSTANCE        hSetupRscInst;
extern HINSTANCE        hSDInst;
extern HINSTANCE        hXPIStubInst;

extern HBITMAP          hbmpBoxChecked;
extern HBITMAP          hbmpBoxUnChecked;

extern HANDLE           hAccelTable;

extern HWND             hDlg;
extern HWND             hDlgMessage;
extern HWND             hWndMain;

extern SDI_NETINSTALL   pfnNetInstall;

extern LPSTR            szEGlobalAlloc;
extern LPSTR            szEStringLoad;
extern LPSTR            szEDllLoad;
extern LPSTR            szEStringNull;
extern LPSTR            szTempSetupPath;

extern LPSTR            szClassName;
extern LPSTR            szSetupDir;
extern LPSTR            szTempDir;
extern LPSTR            szFileIniConfig;

extern DWORD            dwWizardState;
extern DWORD            dwSetupType;
extern DWORD            dwOSType;
extern DWORD            dwScreenX;
extern DWORD            dwScreenY;

extern DWORD            dwTempSetupType;

extern BOOL             bSDUserCanceled;
extern BOOL             bIdiArchivesExists;
extern BOOL             bCreateDestinationDir;

extern setupGen         sgProduct;
extern diS              diSetup;
extern diW              diWelcome;
extern diL              diLicense;
extern diST             diSetupType;
extern diSC             diSelectComponents;
extern diWI             diWindowsIntegration;
extern diPF             diProgramFolder;
extern diSI             diStartInstall;
extern diR              diReboot;
extern siSD             siSDObject;
extern siCF             siCFCoreFile;
extern siC              *siComponents;

#endif
