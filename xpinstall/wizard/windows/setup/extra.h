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
void              GetWinReg(HKEY hkRootKey, LPSTR szKey, LPSTR szName, LPSTR szReturnValue, DWORD dwSize);
void              SetWinReg(HKEY hkRootKey, LPSTR szKey, LPSTR szName, DWORD dwType, LPSTR szData, DWORD dwSize);
HRESULT           InitSetupGeneral(void);
HRESULT           InitDlgWelcome(diW *diDialog);
HRESULT           InitDlgLicense(diL *diDialog);
HRESULT           InitDlgSetupType(diST *diDialog);
HRESULT           InitDlgSelectComponents(diSC *diDialog, DWORD dwSM);
HRESULT           InitDlgWindowsIntegration(diWI *diDialog);
HRESULT           InitDlgProgramFolder(diPF *diDialog);
HRESULT           InitDlgStartInstall(diSI *diDialog);
HRESULT           InitSDObject(void);
void              DeInitSDObject(void);
siC               *CreateSiCNode(void);
void              SiCNodeInsert(siC **siCHead, siC *siCTemp);
void              SiCNodeDelete(siC *siCTemp);
siCD              *CreateSiCDepNode(void);
void              SiCDepNodeDelete(siCD *siCDepTemp);
void              SiCDepNodeInsert(siCD **siCDepHead, siCD *siCDepTemp);
HRESULT           SiCNodeGetAttributes(DWORD dwIndex, BOOL bIncludeInvisible);
void              SiCNodeSetAttributes(DWORD dwIndex, DWORD dwAttributes, BOOL bSet, BOOL bIncludeInvisible);
void              SiCNodeSetItemsSelected(DWORD dwItems, DWORD *dwItemsSelected);
char              *SiCNodeGetDescriptionShort(DWORD dwIndex, BOOL bIncludeInvisible);
char              *SiCNodeGetDescriptionLong(DWORD dwIndex, BOOL bIncludeInvisible);
siC               *SiCNodeGetObject(DWORD dwIndex, BOOL bIncludeInvisibleObjs);
ULONGLONG         SiCNodeGetInstallSize(DWORD dwIndex, BOOL bIncludeInvisible);
ULONGLONG         SiCNodeGetInstallSizeSystem(DWORD dwIndex, BOOL bIncludeInvisible);
ULONGLONG         SiCNodeGetInstallSizeArchive(DWORD dwIndex, BOOL bIncludeInvisible);
void              InitSiComponents(char *szFileIni);
HRESULT           ParseComponentAttributes(char *szBuf);
void              InitSiComponents(char *szFileIni);
void              DeInitSiCDependencies(siCD *siCDDependencies);
BOOL              ResolveDependencies(DWORD dwIndex);
BOOL              ResolveComponentDependency(siCD *siCDInDependency);
void              ResolveDependees(LPSTR szToggledDescriptionShort);
BOOL              ResolveComponentDependee(siCD *siCDInDependee);
void              STGetComponents(LPSTR szSection, st *stSetupType, LPSTR szFileIni);
HRESULT           InitSCoreFile(void);
void              DeInitSCoreFile(void);
void              DeInitialize(void);
void              DeInitDlgWelcome(diW *diDialog);
void              DeInitDlgLicense(diL *diDialog);
void              DeInitDlgSetupType(diST *diDialog);
void              DeInitDlgSelectComponents(diSC *diDialog);
void              DeInitDlgWindowsIntegration(diWI *diDialog);
void              DeInitDlgProgramFolder(diPF *diDialog);
void              DeInitDlgStartInstall(diSI *diDialog);
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
HRESULT           RetrieveArchives(void);
/* HRESULT           SmartUpdateJars(void); */
void              ParsePath(LPSTR szInput, LPSTR szOutput, DWORD dwLength, DWORD dwType);
void              RemoveBackSlash(LPSTR szInput);
void              AppendBackSlash(LPSTR szInput, DWORD dwInputSize);
BOOL              DeleteIdiGetConfigIni(void);
BOOL              DeleteIdiGetArchives(void);
BOOL              DeleteIdiFileIniConfig(void);
void              DeleteArchives(void);
HRESULT           LaunchApps(void);
HRESULT           FileExists(LPSTR szFile);
int               ExtractDirEntries(char* directory,void* vZip);
BOOL              LocateJar(siC *siCObject);
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
BOOL              CheckLegacy(void);
int               CompareVersion(verBlock vbVersionOld, verBlock vbVersionNew);
COLORREF          DecryptFontColor(LPSTR szColor);

BOOL              bSDInit;

#endif

