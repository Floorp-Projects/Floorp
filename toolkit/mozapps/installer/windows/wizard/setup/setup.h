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

#define DEFAULT_SETUP_WINDOW_NAME       "Setup"
/* Class name for the invisible window to be created */
#define CLASS_NAME_SETUP                "MozillaSetup"
#define CLASS_NAME_SETUP_DLG            "MozillaSetupDlg"
#define FILE_INI_SETUP                  "setup.ini"
#define FILE_INI_CONFIG                 "config.ini"
#define FILE_INI_INSTALL                "install.ini"
#define FILE_IDI_GETCONFIGINI           "getconfigini.idi"
#define FILE_IDI_GETARCHIVES            "getarchives.idi"
#define FILE_IDI_GETREDIRECT            "getredirect.idi"
#define FILE_INI_REDIRECT               "redirect.ini"
#define FILE_WGET_LOG                   "wget.log"
#define WIZ_TEMP_DIR                    "ns_temp"
#define FILE_INSTALL_LOG                "install_wizard.log"
#define FILE_INSTALL_STATUS_LOG         "install_status.log"
#define FILE_ALL_JS                     "all-proxy.js"
#define VR_DEFAULT_PRODUCT_NAME         "Mozilla"

#define FORCE_ADD_TO_UNINSTALL_LOG        TRUE
#define DO_NOT_FORCE_ADD_TO_UNINSTALL_LOG FALSE

/* defines that indicate whether something should
 * be logged to the install_wizardX.log or not
 * for uninstallation purposes.
 */
#define ADD_TO_UNINSTALL_LOG            TRUE
#define DO_NOT_ADD_TO_UNINSTALL_LOG     FALSE

/* defines that indeicate whether an install command
 * should have '*dnu*' prepended.  '*dnu*' is parsed
 * by the uninstaller and signals that the specific
 * install command should _not_ be undone.
 */
#define DNU_UNINSTALL                   FALSE
#define DNU_DO_NOT_UNINSTALL            TRUE

#define WINREG_OVERWRITE_KEY            TRUE
#define WINREG_DO_NOT_OVERWRITE_KEY     FALSE
#define WINREG_OVERWRITE_NAME           TRUE
#define WINREG_DO_NOT_OVERWRITE_NAME    FALSE

#define INCLUDE_INVISIBLE_OBJS          TRUE
#define SKIP_INVISIBLE_OBJS             FALSE

#define NO_BANNER_IMAGE                 0x00000000
#define BANNER_IMAGE_DOWNLOAD           0x00000001
#define BANNER_IMAGE_INSTALLING         0x00000002

#define APPPATH_GRE_PATH_SET            0x00000000
#define APPPATH_GRE_PATH_NOT_SET        0x00000001
#define APPPATH_GRE_PATH_ALREADY_SET    0x00000002

#define NEXT_DLG                        1
#define PREV_DLG                        2
#define OTHER_DLG_1                     3

#define MAX_CRC_FAILED_DOWNLOAD_RETRIES 5
#define MAX_FILE_DOWNLOAD_RETRIES       10

#define STATUS_DISABLED                 0
#define STATUS_ENABLED                  1

#define GRE_SETUP_DIR_NAME              "Setup GRE"

/* filename which contains this product setup's exit status */
#define SETUP_EXIT_STATUS_LOG           "%s Setup Exit Status.log"

/* LOCAL GRE defines */
#define GRE_TYPE_NOT_SET                -1
#define GRE_SHARED                      0
#define GRE_LOCAL                       1

/* WS: WinSpawn wait values */
#define WS_DO_NOT_WAIT                  FALSE
#define WS_WAIT                         TRUE

#define MAX_KILL_PROCESS_RETRIES        10

/* CI: Check Instance */
#define CI_FORCE_QUIT_PROCESS           TRUE
#define CI_CLOSE_PROCESS                FALSE

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

/* RA: Restricted Access */
#define RA_IGNORE                       0
#define RA_ONLY_RESTRICTED              1
#define RA_ONLY_NONRESTRICTED           2

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
#define DLG_COMMIT_INSTALL              1
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
#define WIZ_ERROR_PARSING_INTERNAL_STR  1031
#define WIZ_ERROR_REGKEY                1032
#define WIZ_ERROR_INIT                  1033
#define WIZ_ERROR_LOADING_RESOURCE_LIB  1034
#define WIZ_ERROR_CREATE_DIRECTORY      1035

/* E: Errors */
#define E_REBOOT                        999

/* FO: File Operation */
#define FO_OK                           0
#define FO_SUCCESS                      0
#define FO_ERROR_FILE_NOT_FOUND         1
#define FO_ERROR_DESTINATION_CONFLICT   2
#define FO_ERROR_CHANGE_DIR             3
#define FO_ERROR_WRITE                  4
#define FO_ERROR_INCR_EXCEEDS_LIMIT     5

/* Mode of Setup to run in */
#define NOT_SET                         -1
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
#define SIC_UNCOMPRESS                  0x00000400
#define SIC_SUPERSEDE                   0x00000800
#define SIC_MAIN_COMPONENT              0x00001000

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
#define OS_NT50                         0x00000100
#define OS_NT51                         0x00000200

/* DSR: Disk Space Required */
#define DSR_DESTINATION                 0
#define DSR_SYSTEM                      1
#define DSR_TEMP                        2
#define DSR_DOWNLOAD_SIZE               3

/* SS: Site Selector */
#define SS_HIDE                         0
#define SS_SHOW                         1

/* PUS: Previous Unfinished State */
#define PUS_NONE                         0
#define PUS_DOWNLOAD                     1
#define PUS_UNPACK_XPCOM                 2
#define PUS_INSTALL_XPI                  3
#define SETUP_STATE_DOWNLOAD             "downloading"
#define SETUP_STATE_UNPACK_XPCOM         "unpacking xpcom"
#define SETUP_STATE_INSTALL_XPI          "installing xpi"
#define SETUP_STATE_REMOVING_PREV_INST   "removing previous installation"


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
  LPSTR szMessageWelcome;
  LPSTR szMessage0;
  LPSTR szMessage1;
  LPSTR szMessage2;
  LPSTR szMessage3;
} diW;

typedef struct dlgLicense
{
  BOOL  bShowDialog;
  LPSTR szTitle;
  LPSTR szSubTitle;
  LPSTR szLicenseFilename;
  LPSTR szMessage0;
  LPSTR szRadioAccept;
  LPSTR szRadioDecline;
} diL;

typedef struct dlgQuickLaunch
{
  BOOL  bShowDialog;
  LPSTR szTitle;
  LPSTR szMessage0;
  LPSTR szMessage1;
  LPSTR szMessage2;
  BOOL  bTurboMode;
  BOOL  bTurboModeEnabled;
} diQL;


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
  LPSTR szSubTitle;
  LPSTR szMessage0;
  st    stSetupType0;
  st    stSetupType1;
} diST;

typedef struct dlgSelectComponents
{
  BOOL  bShowDialog;
  DWORD bShowDialogSM;
  LPSTR szTitle;
  LPSTR szSubTitle;
  LPSTR szMessage0;
} diSC;

typedef struct dlgSelectInstallPath
{
  BOOL bShowDialog;
  LPSTR szTitle;
  LPSTR szSubTitle;
  LPSTR szMessage0;
} diSIP;

typedef struct dlgUpgrade
{
  BOOL bShowDialog;
  BOOL bShowInEasyInstall;
  LPSTR szTitle;
  LPSTR szSubTitle;
  LPSTR szMessageCleanup;
  LPSTR szCheckboxSafeInstall;
  LPSTR szSafeInstallInfo;
  LPSTR szUnsafeInstallInfo;
  LPSTR szNoSafeUpgradeWindir;
} diU;

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
  LPSTR szSubTitle;
  LPSTR szMessage0;
  LPSTR szRegistryKey;
  wiCBs wiCB0;
  wiCBs wiCB1;
  wiCBs wiCB2;
} diWI;

typedef struct dlgProgramFolder
{
  BOOL  bShowDialog;
  LPSTR szTitle;
  LPSTR szMessage0;
} diPF;

typedef struct dlgAdditionalOptions
{
  BOOL  bShowDialog;
  LPSTR szTitle;
  LPSTR szMessage0;
  LPSTR szMessage1;
  BOOL  bSaveInstaller;
  BOOL  bRecaptureHomepage;
  BOOL  bShowHomepageOption;
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
  LPSTR szSubTitle;
  LPSTR szMessageInstall;
  LPSTR szMessageDownload;
  LPSTR szMessage0;
} diSI;

typedef struct dlgDownloading
{
  BOOL  bShowDialog;
  LPSTR szTitle;
  LPSTR szSubTitle;
  LPSTR szBlurb;
  LPSTR szFileNameKey;
  LPSTR szTimeRemainingKey;
} diDL;

typedef struct dlgInstalling
{
  BOOL bShowDialog;
  LPSTR szTitle;
  LPSTR szSubTitle;
  LPSTR szBlurb;
  LPSTR szStatusFile;
  LPSTR szStatusComponent;
} diI;

typedef struct dlgInstallSuccessful
{
  BOOL bShowDialog;
  LPSTR szTitle;
  LPSTR szMessageHeader;
  LPSTR szMessage0;
  LPSTR szMessage1;
  LPSTR szResetHomepage;
  LPSTR szRegistryKey;
  LPSTR szLaunchApp;
  BOOL bResetHomepageChecked;
  BOOL bLaunchAppChecked;
} diIS;

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
  int       mode;
  int       greType;
  DWORD     dwCustomType;
  DWORD     dwNumberOfComponents;
  LPSTR     szPath;
  LPSTR     szSubPath;
  LPSTR     szProgramName;
  LPSTR     szCompanyName;
  LPSTR     szProductName;
  LPSTR     szProductNameInternal;
  LPSTR     szProductNamePrevious;
  LPSTR     szUninstallFilename;
  LPSTR     szUserAgent;
  LPSTR     szProgramFolderName;
  LPSTR     szProgramFolderPath;
  LPSTR     szAlternateArchiveSearchPath;
  LPSTR     szParentProcessFilename;
  BOOL      bLockPath;
  BOOL      bSharedInst;
  BOOL      bInstallFiles;
  BOOL      checkCleanupOnUpgrade;
  BOOL      doCleanupOnUpgrade;
  LPSTR     szAppID;
  LPSTR     szAppPath;
  LPSTR     szRegPath;
  BOOL      greCleanupOrphans;
  char      greCleanupOrphansMessage[MAX_BUF];
  char      greID[MAX_BUF];
  char      grePrivateKey[MAX_BUF];
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
  BOOL        bStatus;
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
  LPSTR           szArchiveNameUncompressed;
  LPSTR           szArchivePath;
  LPSTR           szDestinationPath;
  LPSTR           szDescriptionShort;
  LPSTR           szDescriptionLong;
  LPSTR           szParameter;
  LPSTR           szReferenceName;
  BOOL            bForceUpgrade;
  BOOL            bSupersede;
  int             iNetRetries;
  int             iCRCRetries;
  int             iNetTimeOuts;
  int             iFileCount;
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

typedef struct dlgInstall
{
	HFONT systemFont;
	HFONT definedFont;
  HFONT welcomeTitleFont;
  char szFontName[MAX_BUF];
  char szFontSize[MAX_BUF];
  char szCharSet[MAX_BUF];
  char szOk_[MAX_BUF];
  char szOk[MAX_BUF];
  char szCancel_[MAX_BUF];
  char szCancel[MAX_BUF];
  char szNext_[MAX_BUF];
  char szBack_[MAX_BUF];
  char szIgnore_[MAX_BUF];
  char szProxyMessage[MAX_BUF];
  char szProxyButton[MAX_BUF];
  char szProxySettings_[MAX_BUF];
  char szProxySettings[MAX_BUF];
  char szServer[MAX_BUF];
  char szPort[MAX_BUF];
  char szUserId[MAX_BUF];
  char szPassword[MAX_BUF];
  char szSelectDirectory[MAX_BUF];
  char szDirectories_[MAX_BUF];
  char szDrives_[MAX_BUF];
  char szStatus[MAX_BUF];
  char szFile[MAX_BUF];
  char szUrl[MAX_BUF];
  char szTo[MAX_BUF];
  char szAccept_[MAX_BUF];
  char szDecline_[MAX_BUF];
  char szProgramFolder_[MAX_BUF];
  char szExistingFolder_[MAX_BUF];
  char szSetupMessage[MAX_BUF];
  char szRestart[MAX_BUF];
  char szYesRestart[MAX_BUF];
  char szNoRestart[MAX_BUF];
  char szAdditionalComponents_[MAX_BUF];
  char szDescription[MAX_BUF];
  char szTotalDownloadSize[MAX_BUF];
  char szSpaceAvailable[MAX_BUF];
  char szComponents_[MAX_BUF];
  char szBrowseInfo[MAX_BUF];
  char szDestinationDirectory[MAX_BUF];
  char szBrowse_[MAX_BUF];
  char szDownloadSize[MAX_BUF];
  char szCurrentSettings[MAX_BUF];
  char szInstallFolder[MAX_BUF];
  char szPrimCompNoOthers[MAX_BUF];
  char szPrimCompOthers[MAX_BUF];
  char szAddtlCompWrapper[MAX_BUF];
  char szInstall_[MAX_BUF];
  char szDelete_[MAX_BUF];
  char szContinue_[MAX_BUF];
  char szSkip_[MAX_BUF];
  char szExtracting[MAX_BUF];
  char szReadme_[MAX_BUF];
  char szPause_[MAX_BUF];
  char szResume_[MAX_BUF];
  char szChecked[MAX_BUF];
  char szUnchecked[MAX_BUF];
} installGui;

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
  DWORD lastWindowPosCenterX;
  DWORD lastWindowPosCenterY;
  BOOL  bScreenReader;
  BOOL  bRefreshIcons;
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

