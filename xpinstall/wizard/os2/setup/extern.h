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

#include "setup.h"

/* external global variables */
extern HINSTANCE        hSetupRscInst;
extern HINSTANCE        hXPIStubInst;

extern HWND             hDlgCurrent;
extern HWND             hDlgMessage;
extern HWND             hWndMain;

extern LPSTR            szEGlobalAlloc;
extern LPSTR            szEStringLoad;
extern LPSTR            szEDllLoad;
extern LPSTR            szEStringNull;
extern LPSTR            szTempSetupPath;

extern LPSTR            szSetupDir;
extern LPSTR            szTempDir;
extern LPSTR            szOSTempDir;
extern LPSTR            szFileIniConfig;
extern LPSTR            szFileIniInstall;

extern LPSTR            szSiteSelectorDescription;

extern ULONG            ulWizardState;
extern ULONG            ulSetupType;

extern ULONG            ulTempSetupType;
extern ULONG            gulUpgradeValue;
extern ULONG            gulSiteSelectorStatus;

extern BOOL             bSDUserCanceled;
extern BOOL             bIdiArchivesExists;
extern BOOL             bCreateDestinationDir;
extern BOOL             bReboot;
extern BOOL             gbILUseTemp;
extern BOOL             gbPreviousUnfinishedDownload;
extern BOOL             gbIgnoreRunAppX;
extern BOOL             gbIgnoreProgramFolderX;
extern BOOL             gbDownloadTriggered;

extern setupGen         sgProduct;
extern diS              diSetup;
extern diW              diWelcome;
extern diQL             diQuickLaunch;
extern diL              diLicense;
extern diST             diSetupType;
extern diSC             diSelectComponents;
extern diSC             diSelectAdditionalComponents;
extern diOI             diOS2Integration;
extern diPF             diProgramFolder;
extern diDO             diAdditionalOptions;
extern diAS             diAdvancedSettings;
extern diSI             diStartInstall;
extern diD              diDownload;
extern diR              diReboot;
extern siCF             siCFXpcomFile;
extern siC              *siComponents;
extern ssi              *ssiSiteSelector;
extern char             *SetupFileList[];
extern installGui       sgInstallGui;
extern sems             gErrorMessageStream;
extern sysinfo          gSystemInfo;
extern dsN              *gdsnComponentDSRequirement;

#endif /* _EXTERN_H */

