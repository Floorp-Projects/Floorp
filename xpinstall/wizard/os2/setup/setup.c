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
 *     IBM Corp.  
 */

#include "setup.h"
#include "extra.h"
#include "dialogs.h"
#include "ifuncns.h"

/* global variables */
HOBJECT       hInst;
HOBJECT       hSetupRscInst;
HOBJECT       hSDInst;
HOBJECT       hXPIStubInst;

HBITMAP         hbmpBoxChecked;
HBITMAP         hbmpBoxCheckedDisabled;
HBITMAP         hbmpBoxUnChecked;

LHANDLE          hAccelTable;

HWND            hDlgCurrent;
HWND            hDlgMessage;
HWND            hWndMain;

PSZ           szEGlobalAlloc;
PSZ           szEStringLoad;
PSZ           szEDllLoad;
PSZ           szEStringNull;
PSZ           szTempSetupPath;

PSZ           szSetupDir;
PSZ           szTempDir;
PSZ           szOSTempDir;
PSZ           szFileIniConfig;
PSZ           szFileIniInstall;

PSZ           szSiteSelectorDescription;

ULONG           dwWizardState;
ULONG           dwSetupType;
ULONG           dwScreenX;
ULONG           dwScreenY;

ULONG           dwTempSetupType;
ULONG           gdwUpgradeValue;
ULONG           gdwSiteSelectorStatus;

BOOL            bSDUserCanceled;
BOOL            bIdiArchivesExists;
BOOL            bCreateDestinationDir;
BOOL            bReboot;
BOOL            gbILUseTemp;
BOOL            gbPreviousUnfinishedDownload;
BOOL            gbIgnoreRunAppX;
BOOL            gbIgnoreProgramFolderX;
BOOL            gbRestrictedAccess;

setupGen        sgProduct;
diS             diSetup;
diW             diWelcome;
diL             diLicense;
diST            diSetupType;
diSC            diSelectComponents;
diSC            diSelectAdditionalComponents;
diWI            diWindowsIntegration;
diPF            diProgramFolder;
diDO            diDownloadOptions;
diAS            diAdvancedSettings;
diSI            diStartInstall;
diD             diDownload;
diR             diReboot;
siSD            siSDObject;
siCF            siCFXpcomFile;
siC             *siComponents;
ssi             *ssiSiteSelector;
installGui      sgInstallGui;
sems            gErrorMessageStream;
sysinfo         gSystemInfo;
dsN             *gdsnComponentDSRequirement = NULL;

/* do not add setup.exe to the list because we figure out the filename
 * by calling GetModuleFileName() */
char *SetupFileList[] = {"setuprsc.dll",
                         "config.ini",
                         "setup.ini",
                         "installer.ini",
                         ""};

int APIENTRY WinMain(HOBJECT hInstance, HOBJECT hPrevInstance, PSZ lpszCmdLine, int nCmdShow)
{
  /***********************************************************************/
  /* LHANDLE hInstance;       handle for this instance                    */
  /* LHANDLE hPrevInstance;   handle for possible previous instances      */
  /* PSZ  lpszCmdLine;     long pointer to exec command line           */
  /* int    nCmdShow;        Show code for main window display           */
  /***********************************************************************/

  QMSG   msg;
  char  szBuf[MAX_BUF];
  int   iRv = WIZ_OK;
  HWND  hwndFW;
  HINI  hiniInstall;

  if(!hPrevInstance)
  {
    /* Allow only one instance of setup to run.
     * Detect a previous instance of setup, bring it to the 
     * foreground, and quit current instance */
    if((hwndFW = FindWindow(CLASS_NAME_SETUP_DLG, NULL)) != NULL)
    {
      ShowWindow(hwndFW, SW_RESTORE);
      SetForegroundWindow(hwndFW);
      iRv = WIZ_SETUP_ALREADY_RUNNING;
      PostQuitMessage(1);
    }
    else if(Initialize(hInstance))
      PostQuitMessage(1);
    else if(!InitApplication(hInstance, hSetupRscInst))
    {
      char szEFailed[MAX_BUF];
      hiniInstall = PrfOpenProfile((HAB)0, szFileIniInstall);
      if(PrfQueryProfileString(hiniInstall, "Messages", "ERROR_FAILED", "", szEFailed, sizeof(szEFailed)))
      {
        sprintf(szBuf, szEFailed, "InitApplication().");
        PrintError(szBuf, ERROR_CODE_SHOW);
      }
      PrfCloseProfile(hiniInstall);
      PostQuitMessage(1);
    }
    else if(!InitInstance(hInstance, nCmdShow))
    {
      char szEFailed[MAX_BUF];
      hiniInstall = PrfOpenProfile((HAB)0, szFileIniInstall);
      if(PrfQueryProfileString(hiniInstall, "Messages", "ERROR_FAILED", "", szEFailed, sizeof(szEFailed)))
      {
        sprintf(szBuf, szEFailed, "InitInstance().");
        PrintError(szBuf, ERROR_CODE_SHOW);
      }
      PrfCloseProfile(hiniInstall);
      PostQuitMessage(1);
    }
    else if(GetInstallIni())
    {
      PostQuitMessage(1);
    }
    else if(ParseInstallIni())
    {
      PostQuitMessage(1);
    }
    else if(GetConfigIni())
    {
      PostQuitMessage(1);
    }
    else if(ParseConfigIni(lpszCmdLine))
    {
      PostQuitMessage(1);
    }
    else
    {
      DlgSequenceNext();
    }
  }

  while(WinGetMsg((HAB)0, &msg, NULL, 0, 0))
  {
    if((!IsDialogMessage(hDlgCurrent, &msg)) && (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)))
    {
       WinDispatchMsg((HAB)0, &msg);
    }
  }

  if(iRv != WIZ_SETUP_ALREADY_RUNNING)
    /* Do clean up before exiting from the application */
    DeInitialize();

  return(msg.wParam);
} /*  End of WinMain */

