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

#ifndef _SETUP_H_
#define _SETUP_H_

#ifdef __cplusplus
#define PR_BEGIN_EXTERN_C       extern "C" {
#define PR_END_EXTERN_C         }
#else /* __cplusplus */
#define PR_BEGIN_EXTERN_C
#define PR_END_EXTERN_C
#endif /* __cplusplus */

#define PR_EXTERN(type) type

typedef unsigned int PRUint32;
typedef int PRInt32;

#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <direct.h>
#include <tchar.h>
#include <commctrl.h>
#include "setuprsc.h"
#include "resource.h"
#include "zipfile.h"

#define CLASS_NAME_SETUP                "Setup"
#define CLASS_NAME_SETUP_DLG            "MozillaSetupDlg"
#define FILE_INI_SETUP                  "setup.ini"
#define FILE_INI_CONFIG                 "config.ini"
#define FILE_IDI_GETCONFIGINI           "getconfigini.idi"
#define FILE_IDI_GETARCHIVES            "getarchives.idi"
#define FILE_IDI_GETREDIRECT            "getredirect.idi"
#define FILE_INI_REDIRECT               "redirect.ini"
#define WIZ_TEMP_DIR                    "ns_temp"
#define FILE_INSTALL_LOG                "install_wizard.log"
#define FILE_INSTALL_STATUS_LOG         "install_status.log"
#define FILE_ALL_JS                     "all-proxy.js"
#define VR_DEFAULT_PRODUCT_NAME         "Mozilla"

#define MAX_CRC_FAILED_DOWNLOAD_RETRIES 3
#define MAX_FILE_DOWNLOAD_RETRIES       3

#define BAR_MARGIN                      1
#define BAR_SPACING                     0
#define BAR_WIDTH                       6
#define BAR_LIBXPNET_MARGIN             1
#define BAR_LIBXPNET_SPACING            0
#define BAR_LIBXPNET_WIDTH              1

/* W: When for install status logging */
#define W_START                         0
#define W_END                           1

/* W: When for crc check failed logging */
#define W_STARTUP                       0
#define W_DOWNLOAD                      1

/* UP: Use Protocol */
#define UP_FTP                          0
#define UP_HTTP                         1

/* LIS: Log Install Status */
#define LIS_SUCCESS                     0
#define LIS_FAILURE                     1

/* UG: Upgrade */
#define UG_NONE                         0
#define UG_DELETE                       1
#define UG_IGNORE                       2
#define UG_GOBACK                       3

/* AP: Archive Path */
#define AP_NOT_FOUND                    0
#define AP_TEMP_PATH                    1
#define AP_SETUP_PATH                   2
#define AP_ALTERNATE_PATH               3

/* PP: Parse Path */
#define PP_FILENAME_ONLY                1
#define PP_PATH_ONLY                    2
#define PP_ROOT_ONLY                    3
#define PP_EXTENSION_ONLY               4

/* DA: Delete Archive */
#define DA_ONLY_IF_IN_ARCHIVES_LST      1
#define DA_ONLY_IF_NOT_IN_ARCHIVES_LST  2
#define DA_IGNORE_ARCHIVES_LST          3

/* T: Timing */
#define T_PRE_DOWNLOAD                  1
#define T_POST_DOWNLOAD                 2
#define T_PRE_XPCOM                     3
#define T_POST_XPCOM                    4
#define T_PRE_SMARTUPDATE               5
#define T_POST_SMARTUPDATE              6
#define T_PRE_LAUNCHAPP                 7
#define T_POST_LAUNCHAPP                8
#define T_DEPEND_REBOOT                 9
#define T_PRE_ARCHIVE                   10
#define T_POST_ARCHIVE                  11

#define MAX_BUF                         2048
#define MAX_BUF_TINY                    256
#define MAX_BUF_SMALL                   512
#define MAX_BUF_MEDIUM                  1024
#define MAX_BUF_LARGE                   MAX_BUF
#define MAX_BUF_XLARGE                  4096
#define MAX_ITOA                        46
#define MAX_INI_SK                      128

#define ERROR_CODE_HIDE                 0
#define ERROR_CODE_SHOW                 1
#define DLG_NONE                        0
#define CX_CHECKBOX                     13
#define CY_CHECKBOX                     13

/* WIZ: WIZARD defines */
#define WIZ_OK                          0
#define WIZ_ERROR_UNDEFINED             1024
#define WIZ_MEMORY_ALLOC_FAILED         1025
#define WIZ_OUT_OF_MEMORY               WIZ_MEMORY_ALLOC_FAILED
#define WIZ_ARCHIVES_MISSING            1026
#define WIZ_CRC_PASS                    WIZ_OK
#define WIZ_CRC_FAIL                    1028
#define WIZ_SETUP_ALREADY_RUNNING       1029
#define WIZ_TOO_MANY_NETWORK_ERRORS     1030

/* E: Errors */
#define E_REBOOT                        999

/* FO: File Operation */
#define FO_OK                           0
#define FO_SUCCESS                      0
#define FO_ERROR_FILE_NOT_FOUND         1
#define FO_ERROR_DESTINATION_CONFLICT   2
#define FO_ERROR_CHANGE_DIR             3

/* Mode of Setup to run in */
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
#define SIC_SELECTED                    0x00000001
#define SIC_INVISIBLE                   0x00000002
#define SIC_LAUNCHAPP                   0x00000004
#define SIC_DOWNLOAD_REQUIRED           0x00000008
#define SIC_DOWNLOAD_ONLY               0x00000010
#define SIC_ADDITIONAL                  0x00000020
#define SIC_DISABLED                    0x00000040
#define SIC_FORCE_UPGRADE               0x00000080
#define SIC_IGNORE_DOWNLOAD_ERROR       0x00000100
#define SIC_IGNORE_XPINSTALL_ERROR      0x00000200

/* AC: Additional Components */
#define AC_NONE                         0
#define AC_COMPONENTS                   1
#define AC_ADDITIONAL_COMPONENTS        2
#define AC_ALL                          3

/* OS: Operating System */
#define OS_WIN9x                        0x00000001
#define OS_WIN95_DEBUTE                 0x00000002
#define OS_WIN95                        0x00000004
#define OS_WIN98                        0x00000008
#define OS_NT                           0x00000010
#define OS_NT3                          0x00000020
#define OS_NT4                          0x00000040
#define OS_NT5                          0x00000080

/* DSR: Disk Space Required */
#define DSR_DESTINATION                 0
#define DSR_SYSTEM                      1
#define DSR_TEMP                        2
#define DSR_DOWNLOAD_SIZE               3

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
  DWORD dwCItems;
  DWORD dwCItemsSelected[MAX_BUF]; /* components */
  LPSTR szDescriptionShort;
  LPSTR szDescriptionLong;
} st;

typedef struct dlgSetupType
{
  BOOL  bShowDialog;
  LPSTR szTitle;
  LPSTR szMessage0;
  LPSTR szReadmeFilename;
  LPSTR szReadmeApp;
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

typedef struct dlgDownloadOptions
{
  BOOL  bShowDialog;
  LPSTR szTitle;
  LPSTR szMessage0;
  LPSTR szMessage1;
  BOOL  bSaveInstaller;
  DWORD dwUseProtocol;
  BOOL  bUseProtocolSettings;
  BOOL  bShowProtocols;
} diDO;

typedef struct dlgAdvancedSettings
{
  BOOL  bShowDialog;
  LPSTR szTitle;
  LPSTR szMessage0;
  LPSTR szProxyServer;
  LPSTR szProxyPort;
  LPSTR szProxyUser;
  LPSTR szProxyPasswd;
} diAS;

typedef struct dlgStartInstall
{
  BOOL  bShowDialog;
  LPSTR szTitle;
  LPSTR szMessageInstall;
  LPSTR szMessageDownload;
} diSI;

typedef struct dlgDownload
{
  BOOL  bShowDialog;
  LPSTR szTitle;
  LPSTR szMessageDownload0;
  LPSTR szMessageRetry0;
} diD;

typedef struct dlgReboot
{
  DWORD dwShowDialog;
  LPSTR szTitle;
} diR;

typedef struct setupStruct
{
  DWORD     dwMode;
  DWORD     dwCustomType;
  DWORD     dwNumberOfComponents;
  LPSTR     szPath;
  LPSTR     szSubPath;
  LPSTR     szProgramName;
  LPSTR     szCompanyName;
  LPSTR     szProductName;
  LPSTR     szUninstallFilename;
  LPSTR     szUserAgent;
  LPSTR     szProgramFolderName;
  LPSTR     szProgramFolderPath;
  LPSTR     szAlternateArchiveSearchPath;
  LPSTR     szParentProcessFilename;
  BOOL      bLockPath;
} setupGen;

typedef struct sinfoSmartDownload
{
  LPSTR szXpcomFile;
  LPSTR szXpcomDir;
  LPSTR szNoAds;
  LPSTR szSilent;
  LPSTR szExecution;
  LPSTR szConfirmInstall;
  LPSTR szExtractMsg;
  LPSTR szExe;
  LPSTR szExeParam;
  LPSTR szXpcomFilePath;
} siSD;

typedef struct sinfoXpcomFile
{
  LPSTR       szSource;
  LPSTR       szDestination;
  LPSTR       szMessage;
  BOOL        bCleanup;
  ULONGLONG   ullInstallSize;
} siCF;

typedef struct sinfoComponentDep siCD;
struct sinfoComponentDep
{
  LPSTR szDescriptionShort;
  LPSTR szReferenceName;
  siCD  *Next;
  siCD  *Prev;
};

typedef struct sinfoComponent siC;
struct sinfoComponent
{
  ULONGLONG       ullInstallSize;
  ULONGLONG       ullInstallSizeSystem;
  ULONGLONG       ullInstallSizeArchive;
  long            lRandomInstallPercentage;
  long            lRandomInstallValue;
  DWORD           dwAttributes;
  LPSTR           szArchiveName;
  LPSTR           szArchivePath;
  LPSTR           szDestinationPath;
  LPSTR           szDescriptionShort;
  LPSTR           szDescriptionLong;
  LPSTR           szParameter;
  LPSTR           szReferenceName;
  BOOL            bForceUpgrade;
  int             iNetRetries;
  int             iCRCRetries;
  siCD            *siCDDependencies;
  siCD            *siCDDependees;
  siC             *Next;
  siC             *Prev;
};

typedef struct ssInfo ssi;
struct ssInfo
{
  LPSTR szDescription;
  LPSTR szDomain;
  LPSTR szIdentifier;
  ssi   *Next;
  ssi   *Prev;
};

/* structure message stream */
typedef struct sEMsgStream sems;
struct sEMsgStream
{
  char   szURL[MAX_BUF];
  char   szConfirmationMessage[MAX_BUF];
  char   *szMessage;
  DWORD  dwMessageBufSize;
  BOOL   bEnabled;
  BOOL   bSendMessage;
  BOOL   bShowConfirmation;
};

/* structure system info*/
typedef struct sSysInfo sysinfo;
struct sSysInfo
{
  DWORD dwOSType;
  DWORD dwMajorVersion;
  DWORD dwMinorVersion;
  DWORD dwBuildNumber;
  char  szExtraString[MAX_BUF];
  DWORD dwMemoryTotalPhysical;
  DWORD dwMemoryAvailablePhysical;
  DWORD dwScreenX;
  DWORD dwScreenY;
};

typedef struct diskSpaceNode dsN;
struct diskSpaceNode
{
  ULONGLONG       ullSpaceRequired;
  LPSTR           szPath;
  LPSTR           szVDSPath;
  dsN             *Next;
  dsN             *Prev;
};

#endif /* _SETUP_H */

