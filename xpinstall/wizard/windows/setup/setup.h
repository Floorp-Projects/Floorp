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

#ifndef _SETUP_H_
#define _SETUP_H_

#ifdef __cplusplus
#define PR_BEGIN_EXTERN_C       extern "C" {
#define PR_END_EXTERN_C         }
#else
#define PR_BEGIN_EXTERN_C
#define PR_END_EXTERN_C
#endif

#define PR_EXTERN(type) type

typedef unsigned int PRUint32;
typedef int PRInt32;

#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <afxres.h>
#include <direct.h>
#include <tchar.h>
#include <commctrl.h>
#include "setuprsc\\setuprsc.h"
#include "resource.h"
#include "sdinst.h"
#include "zipfile.h"

#define CLASS_NAME                      "Setup"
#define FILE_INI_SETUP                  "setup.ini"
#define FILE_INI_CONFIG                 "config.ini"
#define FILE_IDI_GETCONFIGINI           "getconfigini.idi"
#define FILE_IDI_GETARCHIVES            "getarchives.idi"

/* PP: Parse Path */
#define PP_FILENAME_ONLY                1
#define PP_PATH_ONLY                    2
#define PP_ROOT_ONLY                    3

/* T: Timing */
#define T_PRE_DOWNLOAD                  1
#define T_POST_DOWNLOAD                 2
#define T_PRE_CORE                      3
#define T_POST_CORE                     4
#define T_PRE_SMARTUPDATE               5
#define T_POST_SMARTUPDATE              6
#define T_PRE_LAUNCHAPP                 7
#define T_POST_LAUNCHAPP                8

#define MAX_BUF                         4096
#define ERROR_CODE_HIDE                 0
#define ERROR_CODE_SHOW                 1
#define DLG_NONE                        0
#define CX_CHECKBOX                     13
#define CY_CHECKBOX                     13

/* WIZ: WIZARD defines */
#define WIZ_OK                          0
#define WIZ_MEMORY_ALLOC_FAILED         1

/* FO: File Operation */
#define FO_OK                           0
#define FO_SUCCESS                      0
#define FO_ERROR_FILE_NOT_FOUND         1
#define FO_ERROR_DESTINATION_CONFLICT   2
#define FO_ERROR_CHANGE_DIR             3

#define NORMAL                          0
#define SILENT                          1
#define AUTO                            2

/* ST: Setup Type */
#define ST_RADIO0                       0
#define ST_RADIO1                       1
#define ST_RADIO2                       2
#define ST_RADIO3                       3

/* SM: Setup Type Mode */
#define SM_SINGLE                       0
#define SM_MULTI                        1

/* SIC: Setup Info Component*/
#define SIC_SELECTED                    1
#define SIC_INVISIBLE                   2
#define SIC_LAUNCHAPP                   4
#define SIC_DOWNLOAD_REQUIRED           8

/* OS: Operating System */
#define OS_WIN95_DEBUTE                 1
#define OS_WIN95                        2
#define OS_WIN98                        4
#define OS_NT3                          8
#define OS_NT4                         16

/* DSR: Disk Space Required */
#define DSR_DESTINATION                 0
#define DSR_SYSTEM                      1

typedef HRESULT (_cdecl *SDI_NETINSTALL) (LPSDISTRUCT);

typedef struct dlgSetup
{
  DWORD   dwDlgID;
  WNDPROC fDlgProc;
  LPSTR   szTitle;
} diS;

typedef struct dlgWelcome
{
  BOOL  bShowDialog;
  LPSTR szTitle;
  LPSTR szMessage0;
  LPSTR szMessage1;
  LPSTR szMessage2;
} diW;

typedef struct dlgLicense
{
  BOOL  bShowDialog;
  LPSTR szTitle;
  LPSTR szLicenseFilename;
  LPSTR szMessage0;
  LPSTR szMessage1;
} diL;

typedef struct stStruct
{
  BOOL  bVisible;
  DWORD dwItems;
  DWORD dwItemsSelected[MAX_BUF];
  LPSTR szDescriptionShort;
  LPSTR szDescriptionLong;
} st;

typedef struct dlgSetupType
{
  BOOL  bShowDialog;
  LPSTR szTitle;
  LPSTR szMessage0;
  st    stSetupType0;
  st    stSetupType1;
  st    stSetupType2;
  st    stSetupType3;
} diST;

typedef struct dlgSelectComponents
{
  BOOL  bShowDialog;
  DWORD bShowDialogSM;
  LPSTR szTitle;
  LPSTR szMessage0;
} diSC;

typedef struct wiCBstruct
{
  BOOL  bEnabled;
  BOOL  bCheckBoxState;
  LPSTR szDescription;
  LPSTR szArchive;
} wiCBs;

typedef struct dlgWindowsIntegration
{
  BOOL  bShowDialog;
  LPSTR szTitle;
  LPSTR szMessage0;
  LPSTR szMessage1;
  wiCBs wiCB0;
  wiCBs wiCB1;
  wiCBs wiCB2;
  wiCBs wiCB3;
} diWI;

typedef struct dlgProgramFolder
{
  BOOL  bShowDialog;
  LPSTR szTitle;
  LPSTR szMessage0;
} diPF;

typedef struct dlgStartInstall
{
  BOOL  bShowDialog;
  LPSTR szTitle;
  LPSTR szMessage0;
} diSI;

typedef struct dlgReboot
{
  DWORD dwShowDialog;
  LPSTR szTitle;
} diR;

typedef struct setupStruct
{
  DWORD dwMode;
  DWORD dwCustomType;
  LPSTR szPath;
  LPSTR szProductName;
  LPSTR szProgramFolderName;
  LPSTR szProgramFolderPath;
  LPSTR szAlternateArchiveSearchPath;
} setupGen;

typedef struct sinfoSmartDownload
{
  LPSTR szCoreFile;
  LPSTR szCoreDir;
  LPSTR szNoAds;
  LPSTR szSilent;
  LPSTR szExecution;
  LPSTR szConfirmInstall;
  LPSTR szExtractMsg;
  LPSTR szExe;
  LPSTR szExeParam;
  LPSTR szCoreFilePath;
} siSD;

typedef struct sinfoCoreFile
{
  LPSTR szSource;
  LPSTR szDestination;
  BOOL  bCleanup;
  LPSTR szMessage;
} siCF;

typedef struct sinfoComponentDep siCD;
struct sinfoComponentDep
{
  LPSTR szDescriptionShort;
  siCD  *Next;
  siCD  *Prev;
};

typedef struct sinfoComponent siC;
struct sinfoComponent
{
  ULONGLONG       ullInstallSize;
  ULONGLONG       ullInstallSizeSystem;
  DWORD           dwAttributes;
  LPSTR           szArchiveName;
  LPSTR           szDescriptionShort;
  LPSTR           szDescriptionLong;
  LPSTR           szParameter;
  siCD            *siCDDependencies;
  siC             *Next;
  siC             *Prev;
};

#endif
