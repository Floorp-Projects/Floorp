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

#include "extern.h"
#include "dialogs.h"
#include "extra.h"
#include "xpistub.h"
#include "xpi.h"
#include "xperr.h"
#include "logging.h"
#include "ifuncns.h"

#define BDIR_RIGHT 1
#define BDIR_LEFT  2

typedef HRESULT (_Optlink *XpiInit)(const char *, const char *aLogName, pfnXPIProgress);
typedef HRESULT (_Optlink *XpiInstall)(const char *, const char *, long);
typedef void    (_Optlink *XpiExit)(void);

static XpiInit          pfnXpiInit;
static XpiInstall       pfnXpiInstall;
static XpiExit          pfnXpiExit;

static DWORD            dwCurrentArchive;
static DWORD            dwTotalArchives;
char                    szStrProcessingFile[MAX_BUF];
char                    szStrCopyingFile[MAX_BUF];
char                    szStrInstalling[MAX_BUF];

static void UpdateGaugeArchiveProgressBar(unsigned value);

struct ExtractFilesDlgInfo
{
	HWND	hWndDlg;
	int		nMaxFileBars;	    // maximum number of bars that can be displayed
	int		nMaxArchiveBars;	// maximum number of bars that can be displayed
	int		nArchiveBars;		  // current number of bars to display
} dlgInfo;

HRESULT InitializeXPIStub()
{
  char szBuf[MAX_BUF];
  char szXPIStubFile[MAX_BUF];
  char szEDosQueryProcAddr[MAX_BUF];
  APIRET rc;

  hXPIStubInst = NULL;

  if(!GetPrivateProfileString("Messages", "ERROR_DOSQUERYPROCADDR", "", szEDosQueryProcAddr, sizeof(szEDosQueryProcAddr), szFileIniInstall))
    return(1);

  /* change current directory to where xpistub.dll */
  /* change current directory to where xpistub.dll */
  strcpy(szBuf, siCFXpcomFile.szDestination);
  AppendBackSlash(szBuf, sizeof(szBuf));
  strcat(szBuf, "bin");
  chdir(szBuf);

  /* Set LIBPATHSTRICT */
  DosSetExtLIBPATH("T", LIBPATHSTRICT);

  /* Add it to LIBPATH */
  DosSetExtLIBPATH(szBuf, BEGIN_LIBPATH);

  /* build full path to xpistub.dll */
  strcpy(szXPIStubFile, szBuf);
  AppendBackSlash(szXPIStubFile, sizeof(szXPIStubFile));
  strcat(szXPIStubFile, "xpistub.dll");

  if(FileExists(szXPIStubFile) == FALSE)
    return(2);

  /* load xpistub.dll */
  if (DosLoadModule(NULL, 0, szXPIStubFile, &hXPIStubInst) != NO_ERROR)
  {
    sprintf(szBuf, szEDllLoad, szXPIStubFile);
    PrintError(szBuf, ERROR_CODE_SHOW);
    return(1);
  }
  if(DosQueryProcAddr(hXPIStubInst, 0, "_XPI_Init", &pfnXpiInit) != NO_ERROR)
  {
    sprintf(szBuf, szEDosQueryProcAddr, "_XPI_Init");
    PrintError(szBuf, ERROR_CODE_SHOW);
    return(1);
  }
  if(DosQueryProcAddr(hXPIStubInst, 0, "_XPI_Install", &pfnXpiInstall) != NO_ERROR)
  {
    sprintf(szBuf, szEDosQueryProcAddr, "_XPI_Install");
    PrintError(szBuf, ERROR_CODE_SHOW);
    return(1);
  }
  if(DosQueryProcAddr(hXPIStubInst, 0, "_XPI_Exit", &pfnXpiExit) != NO_ERROR)
  {
    sprintf(szBuf, szEDosQueryProcAddr, "_XPI_Exit");
    PrintError(szBuf, ERROR_CODE_SHOW);
    return(1);
  }

  return(0);
}

HRESULT DeInitializeXPIStub()
{
  pfnXpiInit    = NULL;
  pfnXpiInstall = NULL;
  pfnXpiExit    = NULL;

  if(hXPIStubInst)
    DosFreeModule(hXPIStubInst);

  chdir(szSetupDir);
  return(0);
}

void GetTotalArchivesToInstall(void)
{
  DWORD     dwIndex0;
  siC       *siCObject = NULL;

  dwIndex0        = 0;
  dwTotalArchives = 0;
  siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  while(siCObject)
  {
    if((siCObject->dwAttributes & SIC_SELECTED) && !(siCObject->dwAttributes & SIC_LAUNCHAPP))
      ++dwTotalArchives;

    ++dwIndex0;
    siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  }
}

char *GetErrorString(DWORD dwError, char *szErrorString, DWORD dwErrorStringSize)
{
  int  i = 0;
  char szErrorNumber[MAX_BUF];

  memset(szErrorString, 0, dwErrorStringSize);
  _itoa(dwError, szErrorNumber, 10);

  /* map the error value to a string */
  while(TRUE)
  {
    if(*XpErrorList[i] == '\0')
      break;

    if(stricmp(szErrorNumber, XpErrorList[i]) == 0)
    {
      if(*XpErrorList[i + 1] != '\0')
        strcpy(szErrorString, XpErrorList[i + 1]);

      break;
    }

    ++i;
  }

  return(szErrorString);
}

HRESULT SmartUpdateJars()
{
  DWORD     dwIndex0;
  siC       *siCObject = NULL;
  HRESULT   hrResult;
  char      szBuf[MAX_BUF];
  char      szEXpiInstall[MAX_BUF];
  char      szArchive[MAX_BUF];
  char      szMsgSmartUpdateStart[MAX_BUF];
  char      szDlgExtractingTitle[MAX_BUF];

  if(!GetPrivateProfileString("Messages", "MSG_SMARTUPDATE_START", "", szMsgSmartUpdateStart, sizeof(szMsgSmartUpdateStart), szFileIniInstall))
    return(1);
  if(!GetPrivateProfileString("Messages", "DLG_EXTRACTING_TITLE", "", szDlgExtractingTitle, sizeof(szDlgExtractingTitle), szFileIniInstall))
    return(1);
  if(!GetPrivateProfileString("Messages", "STR_PROCESSINGFILE", "", szStrProcessingFile, sizeof(szStrProcessingFile), szFileIniInstall))
    exit(1);
  if(!GetPrivateProfileString("Messages", "STR_INSTALLING", "", szStrInstalling, sizeof(szStrInstalling), szFileIniInstall))
    exit(1);
  if(!GetPrivateProfileString("Messages", "STR_COPYINGFILE", "", szStrCopyingFile, sizeof(szStrCopyingFile), szFileIniInstall))
    exit(1);

  ShowMessage(szMsgSmartUpdateStart, TRUE);
  if(InitializeXPIStub() == WIZ_OK)
  {
    LogISXPInstall(W_START);
    strcpy(szBuf, sgProduct.szPath);
    if(*sgProduct.szSubPath != '\0')
    {
      AppendBackSlash(szBuf, sizeof(szBuf));
      strcat(szBuf, sgProduct.szSubPath);
    }
    hrResult = pfnXpiInit(szBuf, FILE_INSTALL_LOG, cbXPIProgress);

    /* Unset LIBPATHSTRICT */
    DosSetExtLIBPATH("F", LIBPATHSTRICT);

    ShowMessage(szMsgSmartUpdateStart, FALSE);
    InitProgressDlg();
    GetTotalArchivesToInstall();
    WinSetWindowText(dlgInfo.hWndDlg, szDlgExtractingTitle);

    dwIndex0          = 0;
    dwCurrentArchive  = 0;
    siCObject         = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
    while(siCObject)
    {
      if(siCObject->dwAttributes & SIC_SELECTED)
        /* Since the archive is selected, we need to process the file ops here */
         ProcessFileOps(T_PRE_ARCHIVE, siCObject->szReferenceName);

      /* launch smartupdate engine for earch jar to be installed */
      if((siCObject->dwAttributes & SIC_SELECTED)   &&
        !(siCObject->dwAttributes & SIC_LAUNCHAPP) &&
        !(siCObject->dwAttributes & SIC_DOWNLOAD_ONLY))
      {
        strcpy(szArchive, sgProduct.szAlternateArchiveSearchPath);
        AppendBackSlash(szArchive, sizeof(szArchive));
        strcat(szArchive, siCObject->szArchiveName);
        if((*sgProduct.szAlternateArchiveSearchPath == '\0') || (!FileExists(szArchive)))
        {
          strcpy(szArchive, szSetupDir);
          AppendBackSlash(szArchive, sizeof(szArchive));
          strcat(szArchive, siCObject->szArchiveName);
          if(!FileExists(szArchive))
          {
            strcpy(szArchive, szTempDir);
            AppendBackSlash(szArchive, sizeof(szArchive));
            strcat(szArchive, siCObject->szArchiveName);
            if(!FileExists(szArchive))
            {
              char szEFileNotFound[MAX_BUF];

              if(GetPrivateProfileString("Messages", "ERROR_FILE_NOT_FOUND", "", szEFileNotFound, sizeof(szEFileNotFound), szFileIniInstall))
              {
                sprintf(szBuf, szEFileNotFound, szArchive);
                PrintError(szBuf, ERROR_CODE_HIDE);
              }
              return(1);
            }
          }
        }

        if(dwCurrentArchive == 0)
        {
          ++dwCurrentArchive;
          UpdateGaugeArchiveProgressBar((unsigned)(((double)(dwCurrentArchive)/(double)dwTotalArchives)*(double)100));
        }

        sprintf(szBuf, szStrInstalling, siCObject->szDescriptionShort);
        WinSetDlgItemText(dlgInfo.hWndDlg, IDC_STATUS0, szBuf);
        LogISXPInstallComponent(siCObject->szDescriptionShort);

        hrResult = pfnXpiInstall(szArchive, "", 0xFFFF);
        if(hrResult == E_REBOOT)
          bReboot = TRUE;
        else if((hrResult != WIZ_OK) &&
               !(siCObject->dwAttributes & SIC_IGNORE_XPINSTALL_ERROR))
        {
          LogMSXPInstallStatus(siCObject->szArchiveName, hrResult);
          LogISXPInstallComponentResult(hrResult);
          if(GetPrivateProfileString("Messages", "ERROR_XPI_INSTALL", "", szEXpiInstall, sizeof(szEXpiInstall), szFileIniInstall))
          {
            char szErrorString[MAX_BUF];

            GetErrorString(hrResult, szErrorString, sizeof(szErrorString));
            sprintf(szBuf, "%s - %s: %d %s", szEXpiInstall, siCObject->szDescriptionShort, hrResult, szErrorString);
            PrintError(szBuf, ERROR_CODE_HIDE);
          }

          /* break out of the siCObject while loop */
          break;
        }

        ++dwCurrentArchive;
        UpdateGaugeArchiveProgressBar((unsigned)(((double)(dwCurrentArchive)/(double)dwTotalArchives)*(double)100));
        ProcessWindowsMessages();
        LogISXPInstallComponentResult(hrResult);

        if((hrResult != WIZ_OK) &&
          (siCObject->dwAttributes & SIC_IGNORE_XPINSTALL_ERROR))
          /* reset the result to WIZ_OK if there was an error and the
           * component's attributes contains SIC_IGNORE_XPINSTALL_ERROR.
           * This should be done after LogISXPInstallComponentResult()
           * because we still should log the error value. */
          hrResult = WIZ_OK;
      }

      if(siCObject->dwAttributes & SIC_SELECTED)
        /* Since the archive is selected, we need to do the file ops here */
         ProcessFileOps(T_POST_ARCHIVE, siCObject->szReferenceName);

      ++dwIndex0;
      siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
    } /* while(siCObject) */

    LogMSXPInstallStatus(NULL, hrResult);
    pfnXpiExit();
    if(sgProduct.ulMode != SILENT)
      WinDestroyWindow(dlgInfo.hWndDlg);
  }
  else
  {
    ShowMessage(szMsgSmartUpdateStart, FALSE);
  }

  DeInitializeXPIStub();
  LogISXPInstall(W_END);

  return(hrResult);
}

void cbXPIStart(const char *URL, const char *UIName)
{
}

void cbXPIProgress(const char* msg, PRInt32 val, PRInt32 max)
{
  char szBuf[MAX_BUF];

  if(sgProduct.ulMode != SILENT)
  {
    TruncateString(WinWindowFromID(dlgInfo.hWndDlg, IDC_STATUS3), msg, szBuf, sizeof(szBuf));
    WinSetDlgItemText(dlgInfo.hWndDlg, IDC_STATUS3, szBuf);
  }

  ProcessWindowsMessages();
}

void cbXPIFinal(const char *URL, PRInt32 finalStatus)
{
}



/////////////////////////////////////////////////////////////////////////////
// Progress bar

// Centers the specified window over the desktop. Assumes the window is
// smaller both horizontally and vertically than the desktop
static void
CenterWindow(HWND hWndDlg)
{
  SWP swpDlg;

  WinQueryWindowPos(hWndDlg, &swpDlg);
  WinSetWindowPos(hWndDlg,
                  0,
                  (WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN)/2)-(swpDlg.cx/2),
                  (WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN)/2)-(swpDlg.cy/2),
                  0,
                  0,
                  SWP_MOVE);
}

// Window proc for dialog
MRESULT APIENTRY
ProgressDlgProc(HWND hWndDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch (msg)
  {
    case WM_INITDLG:
      AdjustDialogSize(hWndDlg);
      WinSetPresParam(hWndDlg, PP_FONTNAMESIZE,
                      strlen(sgInstallGui.szDefinedFont)+1, sgInstallGui.szDefinedFont);
      WinSendMsg(WinWindowFromID(hWndDlg, IDC_GAUGE_ARCHIVE), SLM_SETSLIDERINFO,
                               MPFROM2SHORT(SMA_SHAFTDIMENSIONS, 0),
                               (MPARAM)20);
//      DisableSystemMenuItems(hWndDlg, TRUE);
      CenterWindow(hWndDlg);
      break;
   case WM_CLOSE:
   case WM_COMMAND:
      return (MRESULT)TRUE;
  }
  return WinDefDlgProc(hWndDlg, msg, mp1, mp2);
}

// This routine will update the Archive Gauge progress bar to the specified percentage
// (value between 0 and 100)
static void
UpdateGaugeArchiveProgressBar(unsigned value)
{
  if(sgProduct.ulMode != SILENT) {
    WinSendMsg(WinWindowFromID(dlgInfo.hWndDlg, IDC_GAUGE_ARCHIVE), SLM_SETSLIDERINFO,
                               MPFROM2SHORT(SMA_SLIDERARMPOSITION, SMA_INCREMENTVALUE),
                               (MPARAM)(value-1));
  }
}

void InitProgressDlg()
{
  if(sgProduct.ulMode != SILENT)
  {
    dlgInfo.hWndDlg = WinLoadDlg(HWND_DESKTOP, hWndMain, ProgressDlgProc, hSetupRscInst, DLG_EXTRACTING, NULL);
    WinShowWindow(dlgInfo.hWndDlg, TRUE);
  }
}

