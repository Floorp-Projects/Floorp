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

#ifndef _EXTRA_H_
#define _EXTRA_H_

typedef struct diskSpaceNode dsN;
struct diskSpaceNode
{
  ULONGLONG       ullSpaceRequired;
  LPSTR           szPath;
  LPSTR           szVDSPath;
  dsN             *Next;
  dsN             *Prev;
};

typedef struct structVer
{
  ULONGLONG ullMajor;
  ULONGLONG ullMinor;
  ULONGLONG ullRelease;
  ULONGLONG ullBuild;
} verBlock;

BOOL              InitDialogClass(HINSTANCE hInstance, HINSTANCE hSetupRscInst);
BOOL              InitApplication(HINSTANCE hInstance, HINSTANCE hSetupRscInst);
BOOL              InitInstance(HINSTANCE hInstance, DWORD dwCmdShow);
void              PrintError(LPSTR szMsg, DWORD dwErrorCodeSH);
void              FreeMemory(void **vPointer);
void              *NS_GlobalAlloc(DWORD dwMaxBuf);
HRESULT           Initialize(HINSTANCE hInstance);
HRESULT           NS_LoadStringAlloc(HANDLE hInstance, DWORD dwID, LPSTR *szStringBuf, DWORD dwStringBuf);
HRESULT           NS_LoadString(HANDLE hInstance, DWORD dwID, LPSTR szStringBuf, DWORD dwStringBuf);
HRESULT           WinSpawn(LPSTR szClientName, LPSTR szParameters, LPSTR szCurrentDir, int iShowCmd, BOOL bWait);
HRESULT           ParseConfigIni(LPSTR lpszCmdLine);
HRESULT           DecryptString(LPSTR szOutputStr, LPSTR szInputStr);
HRESULT           DecryptVariable(LPSTR szVariable, DWORD dwVariableSize);
HRESULT           InitSetupGeneral(void);
HRESULT           InitDlgWelcome(diW *diDialog);
HRESULT           InitDlgLicense(diL *diDialog);
HRESULT           InitDlgSetupType(diST *diDialog);
HRESULT           InitDlgSelectComponents(diSC *diDialog, DWORD dwSM);
HRESULT           InitDlgWindowsIntegration(diWI *diDialog);
HRESULT           InitDlgProgramFolder(diPF *diDialog);
HRESULT           InitDlgStartInstall(diSI *diDialog);
HRESULT           InitDlgSiteSelector(diAS *diDialog);
HRESULT           InitSDObject(void);
void              DeInitSDObject(void);
siC               *CreateSiCNode(void);
void              SiCNodeInsert(siC **siCHead, siC *siCTemp);
void              SiCNodeDelete(siC *siCTemp);
siCD              *CreateSiCDepNode(void);
void              SiCDepNodeDelete(siCD *siCDepTemp);
void              SiCDepNodeInsert(siCD **siCDepHead, siCD *siCDepTemp);
HRESULT           SiCNodeGetAttributes(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag);
void              SiCNodeSetAttributes(DWORD dwIndex, DWORD dwAttributes, BOOL bSet, BOOL bIncludeInvisible, DWORD dwACFlag);
void              SiCNodeSetItemsSelected(DWORD dwItems, DWORD *dwItemsSelected);
char              *SiCNodeGetDescriptionShort(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag);
char              *SiCNodeGetDescriptionLong(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag);
siC               *SiCNodeGetObject(DWORD dwIndex, BOOL bIncludeInvisibleObjs, DWORD dwACFlag);
ULONGLONG         SiCNodeGetInstallSize(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag);
ULONGLONG         SiCNodeGetInstallSizeSystem(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag);
ULONGLONG         SiCNodeGetInstallSizeArchive(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag);
void              InitSiComponents(char *szFileIni);
HRESULT           ParseComponentAttributes(char *szBuf);
void              InitSiteSelector(char *szFileIni);
void              DeInitSiCDependencies(siCD *siCDDependencies);
BOOL              ResolveDependencies(DWORD dwIndex);
BOOL              ResolveComponentDependency(siCD *siCDInDependency);
void              ResolveDependees(LPSTR szToggledDescriptionShort);
BOOL              ResolveComponentDependee(siCD *siCDInDependee);
void              STGetComponents(LPSTR szSection, st *stSetupType, LPSTR szFileIni);
HRESULT           InitSXpcomFile(void);
void              DeInitSXpcomFile(void);
void              DeInitialize(void);
void              DeInitDlgWelcome(diW *diDialog);
void              DeInitDlgLicense(diL *diDialog);
void              DeInitDlgSetupType(diST *diDialog);
void              DeInitDlgSelectComponents(diSC *diDialog);
void              DeInitDlgWindowsIntegration(diWI *diDialog);
void              DeInitDlgProgramFolder(diPF *diDialog);
void              DeInitDlgStartInstall(diSI *diDialog);
void              DeInitDlgSiteSelector(diAS *diDialog);
void              DetermineOSVersion(void);
void              DeInitSiComponents(void);
void              DeInitSetupGeneral(void);
HRESULT           InitializeSmartDownload(void);
HRESULT           DeInitializeSmartDownload(void);
HRESULT           ParseSetupIni(void);
HRESULT           GetConfigIni(void);
void              CleanTempFiles(void);
void              OutputSetupTitle(HDC hDC);
HRESULT           SdArchives(LPSTR szFileIdi, LPSTR szDownloadDir);
long              RetrieveArchives(void);
long              RetrieveRedirectFile(void);
/* HRESULT           SmartUpdateJars(void); */
void              ParsePath(LPSTR szInput, LPSTR szOutput, DWORD dwLength, DWORD dwType);
void              RemoveBackSlash(LPSTR szInput);
void              AppendBackSlash(LPSTR szInput, DWORD dwInputSize);
void              RemoveSlash(LPSTR szInput);
void              AppendSlash(LPSTR szInput, DWORD dwInputSize);
BOOL              DeleteIdiGetConfigIni(void);
BOOL              DeleteIdiGetArchives(void);
BOOL              DeleteIdiGetRedirect(void);
BOOL              DeleteIdiFileIniConfig(void);
void              DeleteArchives(void);
BOOL              DeleteIniRedirect(void);
HRESULT           LaunchApps(void);
HRESULT           FileExists(LPSTR szFile);
int               ExtractDirEntries(char* directory,void* vZip);
BOOL              LocateJar(siC *siCObject, LPSTR szPath, DWORD dwPathSize, BOOL bIncludeTempDir);
HRESULT           AddArchiveToIdiFile(siC *siCObject, char *szSComponent, char *szSFile, char *szFileIdiGetArchives);
int               SiCNodeGetIndexDS(char *szInDescriptionShort);
void              ViewSiComponents(void);
BOOL              IsWin95Debute(void);
ULONGLONG         GetDiskSpaceRequired(DWORD dwType);
ULONGLONG         GetDiskSpaceAvailable(LPSTR szPath);
HRESULT           VerifyDiskSpace(void);
HRESULT           ErrorMsgDiskSpace(ULONGLONG ullDSAvailable, ULONGLONG ullDSRequired, LPSTR szPath, BOOL bCrutialMsg);
void              SetCustomType(void);
void              GetAlternateArchiveSearchPath(LPSTR lpszCmdLine);
BOOL              NeedReboot(void);
BOOL              LocatePreviousPath(LPSTR szMainSectionName, LPSTR szPath, DWORD dwPathSize);
BOOL              LocatePathNscpReg(LPSTR szSection, LPSTR szPath, DWORD dwPathSize);
BOOL              LocatePathWinReg(LPSTR szSection, LPSTR szPath, DWORD dwPathSize);
BOOL              LocatePath(LPSTR szSection, LPSTR szPath, DWORD dwPathSize);
int               VR_GetPath(char *component_path, unsigned long sizebuf, char *buf);
dsN               *CreateDSNode();
void              DsNodeInsert(dsN **dsNHead, dsN *dsNTemp);
void              DsNodeDelete(dsN **dsNTemp);
void              DeInitDSNode(dsN **dsnComponentDSRequirement);
void              UpdatePathDiskSpaceRequired(LPSTR szPath, ULONGLONG ullInstallSize, dsN **dsnComponentDSRequirement);
HRESULT           InitComponentDiskSpaceInfo(dsN **dsnComponentDSRequirement);
HRESULT           CheckInstances();
long              RandomSelect(void);
void              TranslateVersionStr(LPSTR szVersion, verBlock *vbVersion);
BOOL              GetFileVersion(LPSTR szFile, verBlock *vbVersion);
BOOL              CheckLegacy(HWND hDlg);
int               CompareVersion(verBlock vbVersionOld, verBlock vbVersionNew);
COLORREF          DecryptFontColor(LPSTR szColor);
ssi               *CreateSsiSiteSelectorNode();
void              SsiSiteSelectorNodeInsert(ssi **ssiHead, ssi *ssiTemp);
void              SsiSiteSelectorNodeDelete(ssi *ssiTemp);
ssi*              SsiGetNode(LPSTR szDescription);
void              UpdateSiteSelector(void);
DWORD             GetAdditionalComponentsCount(void);
DWORD             GetTotalArchivesToDownload();
void              RemoveQuotes(LPSTR lpszSrc, LPSTR lpszDest, int iDestSize);
LPSTR             GetFirstNonSpace(LPSTR lpszString);
int               GetArgC(LPSTR lpszCommandLine);
LPSTR             GetArgV(LPSTR lpszCommandLine, int iIndex, LPSTR lpszDest, int iDestSize);
void              ParseCommandLine(LPSTR lpszCmdLine);
void              SetSetupRunMode(LPSTR szMode);
void              Delay(DWORD dwSeconds);
siCD              *InitWinInitNodes(char *szInFile);
void              UpdateWininit(LPSTR szUninstallFilename);
char              *GetSaveInstallerPath(char *szBuf, DWORD dwBufSize);
void              SaveInstallerFiles(void);
void              ResetComponentAttributes(char *szFileIni);
BOOL              IsInList(DWORD dwCurrentItem, DWORD dwItems, DWORD *dwItemsSelected);
int               LocateExistingPath(char *szPath, char *szExistingPath, DWORD dwExistingPathSize);
BOOL              ContainsReparseTag(char *szPath, char *szReparsePath, DWORD dwReparsePathSize);
BOOL              DeleteInstallLogFile();

BOOL              bSDInit;

#endif

