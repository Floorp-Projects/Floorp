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

#include "setup.h"
#include "extra.h"
#include "dialogs.h"
#include "ifuncns.h"

/* global variables */
HINSTANCE       hSetupRscInst;
HINSTANCE       hXPIStubInst;

HWND            hDlgCurrent;
HWND            hDlgMessage;
HWND            hWndMain;

LPSTR           szEGlobalAlloc;
LPSTR           szEStringLoad;
LPSTR           szEDllLoad;
LPSTR           szEStringNull;
LPSTR           szTempSetupPath;

LPSTR           szSetupDir;
LPSTR           szTempDir;
LPSTR           szOSTempDir;
LPSTR           szFileIniConfig;
LPSTR           szFileIniInstall;

LPSTR           szSiteSelectorDescription;

DWORD           ulWizardState;
ULONG           ulSetupType;
LONG            lScreenX;
LONG            lScreenY;

DWORD           ulTempSetupType;
DWORD           gulUpgradeValue;
DWORD           gulSiteSelectorStatus;

BOOL            bSDUserCanceled;
BOOL            bIdiArchivesExists;
BOOL            bCreateDestinationDir;
BOOL            bReboot;
BOOL            gbILUseTemp;
BOOL            gbPreviousUnfinishedDownload;
BOOL            gbPreviousUnfinishedInstallXpi;
BOOL            gbIgnoreRunAppX;
BOOL            gbIgnoreProgramFolderX;
BOOL            gbDownloadTriggered;

setupGen        sgProduct;
diS             diSetup;
diW             diWelcome;
diL             diLicense;
diQL            diQuickLaunch;
diST            diSetupType;
diSC            diSelectComponents;
diSC            diSelectAdditionalComponents;
diOI            diOS2Integration;
diPF            diProgramFolder;
diDO            diAdditionalOptions;
diAS            diAdvancedSettings;
diSI            diStartInstall;
diD             diDownload;
diR             diReboot;
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
                         "install.ini",
                         "license.txt",
                         ""};

int main(int argc, char *argv[], char *envp[])
{
  HAB hab;
  HMQ hmq;
  QMSG qmsg;
  char  szBuf[MAX_BUF];
  int   iRv = WIZ_OK;
  HWND  hwndFW;
  int rc = 0;
  ATOM atom;

  hab = WinInitialize( 0 );
  hmq = WinCreateMsgQueue( hab, 0 );

  atom = WinAddAtom(WinQuerySystemAtomTable(), CLASS_NAME_SETUP_DLG);

  /* Allow only one instance of setup to run.
   * Detect a previous instance of setup, bring it to the 
   * foreground, and quit current instance */

  /* Iterate over top level windows searching for one of the required class
   * and a matching title.
   */
  if((hwndFW = FindWindow(CLASS_NAME_SETUP_DLG)) != NULL)
  {
    WinSetActiveWindow(HWND_DESKTOP, hwndFW);
    iRv = WIZ_SETUP_ALREADY_RUNNING;
    WinPostQueueMsg(0, WM_QUIT, 1, 0);
  }
  else if(Initialize(0, argv[0]))
    WinPostQueueMsg(0, WM_QUIT, 1, 0);
  else if(!InitApplication())
  {
    char szEFailed[MAX_BUF];

    if(GetPrivateProfileString("Messages", "ERROR_FAILED", "", szEFailed, sizeof(szEFailed), szFileIniInstall))
    {
      sprintf(szBuf, szEFailed, "InitApplication().");
      PrintError(szBuf, ERROR_CODE_SHOW);
    }
    WinPostQueueMsg(0, WM_QUIT, 1, 0);
  }
  else if(!InitInstance())
  {
    char szEFailed[MAX_BUF];

    if(GetPrivateProfileString("Messages", "ERROR_FAILED", "", szEFailed, sizeof(szEFailed), szFileIniInstall))
    {
      sprintf(szBuf, szEFailed, "InitInstance().");
      PrintError(szBuf, ERROR_CODE_SHOW);
    }
    WinPostQueueMsg(0, WM_QUIT, 1, 0);
  }
  else if(GetInstallIni())
  {
    WinPostQueueMsg(0, WM_QUIT, 1, 0);
  }
  else if(ParseInstallIni())
  {
    WinPostQueueMsg(0, WM_QUIT, 1, 0);
  }
  else if(GetConfigIni())
  {
    WinPostQueueMsg(0, WM_QUIT, 1, 0);
  }
  else if(ParseConfigIni(argc, argv))
  {
    WinPostQueueMsg(0, WM_QUIT, 1, 0);
  }
  else
  {
    DlgSequence(NEXT_DLG);
  }

  while ( WinGetMsg( hab, &qmsg, NULLHANDLE, 0, 0 ) )
    WinDispatchMsg( hab, &qmsg );

  if(iRv != WIZ_SETUP_ALREADY_RUNNING)
    /* Do clean up before exiting from the application */
    DeInitialize();

  WinDeleteAtom(WinQuerySystemAtomTable(), atom);

  WinDestroyMsgQueue( hmq );
  WinTerminate( hab ); 

  /* Unset LIBPATHSTRICT just in case it didn't get unset elsewhere */
  DosSetExtLIBPATH("F", LIBPATHSTRICT);
}

