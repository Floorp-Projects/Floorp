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
 *
 *  Next Generation Apps Version:
 *     Ben Goodger <ben@mozilla.org>
 */

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

typedef HRESULT (_cdecl *XpiInit)(const char *, const char *aLogName, pfnXPIProgress);
typedef HRESULT (_cdecl *XpiInstall)(const char *, const char *, long);
typedef void    (_cdecl *XpiExit)(void);
typedef BOOL    (WINAPI *SetDllPathProc)(const char*);

static XpiInit          pfnXpiInit;
static XpiInstall       pfnXpiInstall;
static XpiExit          pfnXpiExit;
static SetDllPathProc   pfnSetDllPath = NULL;

static long             lFileCounter;
static long             lBarberCounter;
static DWORD            dwCurrentFile;
static DWORD            dwTotalFiles;
char                    szStrProcessingFile[MAX_BUF];
char                    szStrCopyingFile[MAX_BUF];
char                    szStrInstalling[MAX_BUF];
static char             gSavedCwd[MAX_BUF];

static void UpdateArchiveInstallProgress(int aValue);

struct ExtractFilesDlgInfo
{
	HWND	hWndDlg;
	int		nMaxFileBars;	    // maximum number of bars that can be displayed
	int		nMaxArchiveBars;	// maximum number of bars that can be displayed
	int		nFileBars;		    // current number of bars to display
	int		nArchiveBars;		  // current number of bars to display
} dlgInfo;

HRESULT InitializeXPIStub(char *xpinstallPath)
{
  char szBuf[MAX_BUF];
  char szXPIStubFile[MAX_BUF];
  char szEGetProcAddress[MAX_BUF];
  HANDLE hKernel;

  hXPIStubInst = NULL;
  GetCurrentDirectory(sizeof(gSavedCwd), gSavedCwd);

  if(!GetPrivateProfileString("Messages", "ERROR_GETPROCADDRESS", "", szEGetProcAddress, sizeof(szEGetProcAddress), szFileIniInstall))
    return(1);

  /* change current directory to where xpistub.dll */
  SetCurrentDirectory(xpinstallPath);

  /* Windows XP SP1 changed DLL search path strategy, setting current dir */
  /* is no longer sufficient. Use SetDLLDirectory() if available */
  if ((hKernel = LoadLibrary("kernel32.dll")) != NULL)
  {
    pfnSetDllPath = (SetDllPathProc)GetProcAddress(hKernel, "SetDllDirectoryA");
    if (pfnSetDllPath)
      pfnSetDllPath(xpinstallPath);
  }

  /* build full path to xpistub.dll */
  lstrcpy(szXPIStubFile, xpinstallPath);
  AppendBackSlash(szXPIStubFile, sizeof(szXPIStubFile));
  lstrcat(szXPIStubFile, "xpistub.dll");

  if(FileExists(szXPIStubFile) == FALSE)
    return(2);

  /* load xpistub.dll */
  if((hXPIStubInst = LoadLibraryEx(szXPIStubFile, NULL, LOAD_WITH_ALTERED_SEARCH_PATH)) == NULL)
  {
    wsprintf(szBuf, szEDllLoad, szXPIStubFile);
    PrintError(szBuf, ERROR_CODE_SHOW);
    return(1);
  }
  if(((FARPROC)pfnXpiInit = GetProcAddress(hXPIStubInst, "XPI_Init")) == NULL)
  {
    wsprintf(szBuf, szEGetProcAddress, "XPI_Init");
    PrintError(szBuf, ERROR_CODE_SHOW);
    return(1);
  }
  if(((FARPROC)pfnXpiInstall = GetProcAddress(hXPIStubInst, "XPI_Install")) == NULL)
  {
    wsprintf(szBuf, szEGetProcAddress, "XPI_Install");
    PrintError(szBuf, ERROR_CODE_SHOW);
    return(1);
  }
  if(((FARPROC)pfnXpiExit = GetProcAddress(hXPIStubInst, "XPI_Exit")) == NULL)
  {
    wsprintf(szBuf, szEGetProcAddress, "XPI_Exit");
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
    FreeLibrary(hXPIStubInst);

  chdir(szSetupDir);
  if (pfnSetDllPath)
    pfnSetDllPath(NULL);

  SetCurrentDirectory(gSavedCwd);
  return(0);
}

void GetTotalArchivesToInstall(void)
{
  DWORD i = 0;
  siC *siCObject = NULL;

  dwTotalFiles = 0;
  
  siCObject = SiCNodeGetObject(i, TRUE, AC_ALL);
  while (siCObject) {
    if ((siCObject->dwAttributes & SIC_SELECTED) && 
        !(siCObject->dwAttributes & SIC_LAUNCHAPP))
      dwTotalFiles += siCObject->iFileCount;

    siCObject = SiCNodeGetObject(++i, TRUE, AC_ALL);
  }
}

char *GetErrorString(DWORD dwError, char *szErrorString, DWORD dwErrorStringSize)
{
  int  i = 0;
  char szErrorNumber[MAX_BUF];

  ZeroMemory(szErrorString, dwErrorStringSize);
  itoa(dwError, szErrorNumber, 10);

  /* map the error value to a string */
  while(TRUE)
  {
    if(*XpErrorList[i] == '\0')
      break;

    if(lstrcmpi(szErrorNumber, XpErrorList[i]) == 0)
    {
      if(*XpErrorList[i + 1] != '\0')
        lstrcpy(szErrorString, XpErrorList[i + 1]);

      break;
    }

    ++i;
  }

  return(szErrorString);
}

HRESULT SmartUpdateJars(HWND aWizardPanel)
{
  DWORD     i = 0;
  siC       *siCObject = NULL;
  HRESULT   hrResult;
  char      szBuf[MAX_BUF];
  char      szEXpiInstall[MAX_BUF];
  char      szArchive[MAX_BUF];
  char      szMsgSmartUpdateStart[MAX_BUF];
  char      szDlgExtractingTitle[MAX_BUF];
  char      xpinstallPath[MAX_BUF];
  char      xpiArgs[MAX_BUF];

  // Save the handle to the dialog window so the installer procedures
  // can send messages to it. 
  dlgInfo.hWndDlg = aWizardPanel;

  if (!GetPrivateProfileString("Messages", "DLG_EXTRACTING_TITLE", 
                               "", szDlgExtractingTitle, 
                               sizeof(szDlgExtractingTitle), 
                               szFileIniInstall))
    return 1;

  if (!GetPrivateProfileString("Messages", "STR_PROCESSINGFILE", 
                               "", szStrProcessingFile, 
                               sizeof(szStrProcessingFile), 
                               szFileIniInstall) ||
      !GetPrivateProfileString("Messages", "STR_INSTALLING", 
                               "", szStrInstalling, 
                               sizeof(szStrInstalling), 
                               szFileIniInstall) ||
      !GetPrivateProfileString("Messages", "STR_COPYINGFILE", 
                               "", szStrCopyingFile, 
                               sizeof(szStrCopyingFile), 
                               szFileIniInstall))
    exit(1);

  GetXpinstallPath(xpinstallPath, sizeof(xpinstallPath));
  if(InitializeXPIStub(xpinstallPath) == WIZ_OK)
  {
    LogISXPInstall(W_START);
    lstrcpy(szBuf, sgProduct.szPath);
    if(*sgProduct.szSubPath != '\0')
    {
      AppendBackSlash(szBuf, sizeof(szBuf));
      lstrcat(szBuf, sgProduct.szSubPath);
    }
    hrResult = pfnXpiInit(szBuf, FILE_INSTALL_LOG, cbXPIProgress);

    SetDlgItemText(dlgInfo.hWndDlg, IDC_STATUS0, szMsgSmartUpdateStart);

    GetTotalArchivesToInstall();

    SendDlgItemMessage(dlgInfo.hWndDlg, IDC_PROGRESS_ARCHIVE, PBM_SETRANGE, 
                       0, MAKELPARAM(0, dwTotalFiles));

    dwCurrentFile = 0;
    siCObject = SiCNodeGetObject(i, TRUE, AC_ALL);
    while (siCObject) {
      if(siCObject->dwAttributes & SIC_SELECTED)
        /* Since the archive is selected, we need to process the file ops here */
         ProcessFileOps(T_PRE_ARCHIVE, siCObject->szReferenceName);

      /* launch smartupdate engine for earch jar to be installed */
      if((siCObject->dwAttributes & SIC_SELECTED)   &&
        !(siCObject->dwAttributes & SIC_LAUNCHAPP) &&
        !(siCObject->dwAttributes & SIC_DOWNLOAD_ONLY))
      {
        lFileCounter      = 0;
        lBarberCounter    = 0;
			  dlgInfo.nFileBars = 0;

        // We need to send this message otherwise the progress bars will paint
        // over the completion page for some unknown reason! -ben
        SendMessage(aWizardPanel, WM_PAINT, 0, 0);

        lstrcpy(szArchive, sgProduct.szAlternateArchiveSearchPath);
        AppendBackSlash(szArchive, sizeof(szArchive));
        lstrcat(szArchive, siCObject->szArchiveName);
        if((*sgProduct.szAlternateArchiveSearchPath == '\0') || (!FileExists(szArchive)))
        {
          lstrcpy(szArchive, szSetupDir);
          AppendBackSlash(szArchive, sizeof(szArchive));
          lstrcat(szArchive, siCObject->szArchiveName);
          if(!FileExists(szArchive))
          {
            lstrcpy(szArchive, szTempDir);
            AppendBackSlash(szArchive, sizeof(szArchive));
            lstrcat(szArchive, siCObject->szArchiveName);
            if(!FileExists(szArchive))
            {
              char szEFileNotFound[MAX_BUF];

              if(GetPrivateProfileString("Messages", "ERROR_FILE_NOT_FOUND", "", szEFileNotFound, sizeof(szEFileNotFound), szFileIniInstall))
              {
                wsprintf(szBuf, szEFileNotFound, szArchive);
                PrintError(szBuf, ERROR_CODE_HIDE);
              }
              return(1);
            }
          }
        }

        wsprintf(szBuf, szStrInstalling, siCObject->szDescriptionShort);
        SetDlgItemText(dlgInfo.hWndDlg, IDC_STATUS0, szBuf);
        LogISXPInstallComponent(siCObject->szDescriptionShort);

        /* XXX fix: we need to better support passing arguments to .xpi files.
         * This is a temporary hack to get greType passed to browser.xpi so that
         * it won't delete GRE files if GRE is installed in the mozilla dir.
         *
         * What should be done is have the arguments be described in each
         * component's section in config.ini and have it passed thru here. */
        *xpiArgs = '\0';
        if(lstrcmpi(siCObject->szArchiveName, "gre.xpi") == 0)
          MozCopyStr(sgProduct.szRegPath, xpiArgs, sizeof(xpiArgs));
        else if((lstrcmpi(siCObject->szArchiveName, "browser.xpi") == 0) &&
                (sgProduct.greType == GRE_LOCAL))
          /* passing -greShared to browser.xpi will tell it to cleanup GRE files
           * from it's directory if they exist. */
          MozCopyStr("-greLocal", xpiArgs, sizeof(xpiArgs));

        hrResult = pfnXpiInstall(szArchive, xpiArgs, 0xFFFF);
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
            wsprintf(szBuf, "%s - %s: %d %s", szEXpiInstall, siCObject->szDescriptionShort, hrResult, szErrorString);
            PrintError(szBuf, ERROR_CODE_HIDE);
          }

          /* break out of the siCObject while loop */
          break;
        }

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

      siCObject = SiCNodeGetObject(++i, TRUE, AC_ALL);
    } /* while(siCObject) */

    //report 100% progress status for successful installs
    UpdateGREInstallProgress(100);
    LogMSXPInstallStatus(NULL, hrResult);
    pfnXpiExit();
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
  char szFilename[MAX_BUF];
  char szStrProcessingFileBuf[MAX_BUF];
  char szStrCopyingFileBuf[MAX_BUF];

  if(sgProduct.mode != SILENT)
  {
    ParsePath((char *)msg, szFilename, sizeof(szFilename), FALSE, PP_FILENAME_ONLY);

    dlgInfo.nFileBars = 0;
    
    // XXXben this is a really nasty hack. We shouldn't be parsing the msg value
    // to find out what sort of progress notification this is. Really, the installer
    // listener system needs to be rearchitected to send out a message type (int) 
    // and a message value, and the listeners can synthesize a display message if
    // they want to. As it stands, the strings that are used here are NOT localizable
    // (hard coded over in mozilla/xpinstall/src), which blows dead goats through
    // straws. 
    if ((!strncmp(msg, "Installing: ", 12) ||
         !strncmp(msg, "Replacing: ", 11)) 
         && (max != 0 && val <= max)) {
      ++dwCurrentFile;

      UpdateArchiveInstallProgress((int)dwCurrentFile);
      UpdateGREInstallProgress((int)dwCurrentFile);
    }

    ProcessWindowsMessages();
  }
}

void cbXPIFinal(const char *URL, PRInt32 finalStatus)
{
}



// Update the Archive Progress Bar to the specified percentage. 
static void UpdateArchiveInstallProgress(int aValue)
{
  if (sgProduct.mode != SILENT) {
    SendDlgItemMessage(dlgInfo.hWndDlg, IDC_PROGRESS_ARCHIVE, PBM_SETPOS, 
                       (WPARAM)aValue, 0);
    Sleep(5);
  }
}

