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

#ifndef _EXTRA_H_
#define _EXTRA_H_

typedef __int64 ULARGE_INTEGER;

BOOL              InitDialogClass(HOBJECT hInstance, HOBJECT hSetupRscInst);
BOOL              InitApplication(HOBJECT hInstance, HOBJECT hSetupRscInst);
BOOL              InitInstance(HOBJECT hInstance, ULONG dwCmdShow);
void              PrintError(PSZ szMsg, ULONG dwErrorCodeSH);
void              FreeMemory(void **vPointer);
void              *NS_GlobalReAlloc(HPOINTER hgMemory,
                                    ULONG dwMemoryBufSize,
                                    ULONG dwNewSize);
void              *NS_GlobalAlloc(ULONG dwMaxBuf);
APIRET           Initialize(HOBJECT hInstance);
APIRET           NS_LoadStringAlloc(LHANDLE hInstance, ULONG dwID, PSZ *szStringBuf, ULONG dwStringBuf);
APIRET           NS_LoadString(LHANDLE hInstance, ULONG dwID, PSZ szStringBuf, ULONG dwStringBuf);
APIRET           WinSpawn(PSZ szClientName, PSZ szParameters, PSZ szCurrentDir, int iShowCmd, BOOL bWait);
APIRET           ParseConfigIni(PSZ lpszCmdLine);
APIRET           ParseInstallIni();
APIRET           DecryptString(PSZ szOutputStr, PSZ szInputStr);
APIRET           DecryptVariable(PSZ szVariable, ULONG dwVariableSize);
APIRET           InitSetupGeneral(void);
APIRET           InitDlgWelcome(diW *diDialog);
APIRET           InitDlgLicense(diL *diDialog);
APIRET           InitDlgSetupType(diST *diDialog);
APIRET           InitDlgSelectComponents(diSC *diDialog, ULONG dwSM);
APIRET           InitDlgWindowsIntegration(diWI *diDialog);
APIRET           InitDlgProgramFolder(diPF *diDialog);
APIRET           InitDlgStartInstall(diSI *diDialog);
APIRET           InitDlgSiteSelector(diAS *diDialog);
APIRET           InitSDObject(void);
void              DeInitSDObject(void);
siC               *CreateSiCNode(void);
void              SiCNodeInsert(siC **siCHead, siC *siCTemp);
void              SiCNodeDelete(siC *siCTemp);
siCD              *CreateSiCDepNode(void);
void              SiCDepNodeDelete(siCD *siCDepTemp);
void              SiCDepNodeInsert(siCD **siCDepHead, siCD *siCDepTemp);
APIRET          SiCNodeGetAttributes(ULONG dwIndex, BOOL bIncludeInvisible, ULONG dwACFlag);
void              SiCNodeSetAttributes(ULONG dwIndex, ULONG dwAttributes, BOOL bSet, BOOL bIncludeInvisible, ULONG dwACFlag);
void              SiCNodeSetItemsSelected(ULONG dwSetupType);
char              *SiCNodeGetReferenceName(ULONG dwIndex, BOOL bIncludeInvisible, ULONG dwACFlag);
char              *SiCNodeGetDescriptionShort(ULONG dwIndex, BOOL bIncludeInvisible, ULONG dwACFlag);
char              *SiCNodeGetDescriptionLong(ULONG dwIndex, BOOL bIncludeInvisible, ULONG dwACFlag);
char              *SiCNodeGetReferenceName(ULONG dwIndex, BOOL bIncludeInvisible, ULONG dwACFlag);
siC               *SiCNodeGetObject(ULONG dwIndex, BOOL bIncludeInvisibleObjs, ULONG dwACFlag);
ULONGLONG         SiCNodeGetInstallSize(ULONG dwIndex, BOOL bIncludeInvisible, ULONG dwACFlag);
ULONGLONG         SiCNodeGetInstallSizeSystem(ULONG dwIndex, BOOL bIncludeInvisible, ULONG dwACFlag);
ULONGLONG         SiCNodeGetInstallSizeArchive(ULONG dwIndex, BOOL bIncludeInvisible, ULONG dwACFlag);
siC               *SiCNodeFind(siC *siComponents, char *szInReferenceName);
void              InitSiComponents(char *szFileIni);
APIRET           ParseComponentAttributes(char *szBuf);
void              InitSiteSelector(char *szFileIni);
void              DeInitSiCDependencies(siCD *siCDDependencies);
BOOL              ResolveDependencies(ULONG dwIndex);
BOOL              ResolveComponentDependency(siCD *siCDInDependency);
void              ResolveDependees(PSZ szToggledDescriptionShort);
BOOL              ResolveComponentDependee(siCD *siCDInDependee);
void              STSetVisibility(st *stSetupType);
APIRET           InitSXpcomFile(void);
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
void              DetermineOSVersionEx(void);
void              DeInitSiComponents(siC **siComponents);
void              DeInitSetupGeneral(void);
APIRET           ParseSetupIni(void);
APIRET           GetConfigIni(void);
APIRET           GetInstallIni(void);
void              CleanTempFiles(void);
void              OutputSetupTitle(HDC hDC);
long              RetrieveArchives(void);
long              RetrieveRedirectFile(void);
void              ParsePath(PSZ szInput, PSZ szOutput, ULONG dwOutputSize, BOOL bUseSlash, ULONG dwType);
void              RemoveBackSlash(PSZ szInput);
void              AppendBackSlash(PSZ szInput, ULONG dwInputSize);
void              RemoveSlash(PSZ szInput);
void              AppendSlash(PSZ szInput, ULONG dwInputSize);
BOOL              DeleteIdiGetConfigIni(void);
BOOL              DeleteIdiGetArchives(void);
BOOL              DeleteIdiGetRedirect(void);
BOOL              DeleteIdiFileIniConfig(void);
BOOL              DeleteIdiFileIniInstall(void);
void              DeleteArchives(ULONG dwDeleteCheck);
BOOL              DeleteIniRedirect(void);
APIRET           LaunchApps(void);
APIRET           FileExists(PSZ szFile);
int               ExtractDirEntries(char* directory,void* vZip);
int               LocateJar(siC *siCObject, PSZ szPath, int dwPathSize, BOOL bIncludeTempDir);
APIRET           AddArchiveToIdiFile(siC *siCObject,
                                      char *szSFile,
                                      char *szFileIdiGetArchives);
int               SiCNodeGetIndexDS(char *szInDescriptionShort);
int               SiCNodeGetIndexRN(char *szInReferenceName);
void              ViewSiComponentsDependency(char *szBuffer, char *szIndentation, siC *siCNode);
void              ViewSiComponentsDependee(char *szBuffer, char *szIndentation, siC *siCNode);
void              ViewSiComponents(void);
BOOL              IsWin95Debute(void);
ULONGLONG         GetDiskSpaceRequired(ULONG dwType);
ULONGLONG         GetDiskSpaceAvailable(PSZ szPath);
APIRET           VerifyDiskSpace(void);
APIRET           ErrorMsgDiskSpace(ULONGLONG ullDSAvailable, ULONGLONG ullDSRequired, PSZ szPath, BOOL bCrutialMsg);
void              SetCustomType(void);
void              GetAlternateArchiveSearchPath(PSZ lpszCmdLine);
BOOL              NeedReboot(void);
BOOL              LocatePreviousPath(PSZ szMainSectionName, PSZ szPath, ULONG dwPathSize);
BOOL              LocatePathNscpReg(PSZ szSection, PSZ szPath, ULONG dwPathSize);
BOOL              LocatePathWinReg(PSZ szSection, PSZ szPath, ULONG dwPathSize);
BOOL              LocatePath(PSZ szSection, PSZ szPath, ULONG dwPathSize);
int               VR_GetPath(char *component_path, unsigned long sizebuf, char *buf);
dsN               *CreateDSNode();
void              DsNodeInsert(dsN **dsNHead, dsN *dsNTemp);
void              DsNodeDelete(dsN **dsNTemp);
void              DeInitDSNode(dsN **dsnComponentDSRequirement);
void              UpdatePathDiskSpaceRequired(PSZ szPath, ULONGLONG ullInstallSize, dsN **dsnComponentDSRequirement);
APIRET           InitComponentDiskSpaceInfo(dsN **dsnComponentDSRequirement);
APIRET           CheckInstances();
long              RandomSelect(void);
BOOL              CheckLegacy(HWND hDlg);
COLOR          DecryptFontColor(PSZ szColor);
ssi               *CreateSsiSiteSelectorNode();
void              SsiSiteSelectorNodeInsert(ssi **ssiHead, ssi *ssiTemp);
void              SsiSiteSelectorNodeDelete(ssi *ssiTemp);
ssi*              SsiGetNode(PSZ szDescription);
void              UpdateSiteSelector(void);
ULONG             GetAdditionalComponentsCount(void);
ULONG             GetTotalArchivesToDownload();
void              RemoveQuotes(PSZ lpszSrc, PSZ lpszDest, int iDestSize);
PSZ             GetFirstNonSpace(PSZ lpszString);
int               GetArgC(PSZ lpszCommandLine);
PSZ             GetArgV(PSZ lpszCommandLine,
                          int iIndex,
                          PSZ lpszDest,
                          int iDestSize);
ULONG             ParseCommandLine(PSZ lpszCmdLine);
void              SetSetupRunMode(PSZ szMode);
void              Delay(ULONG dwSeconds);
siCD              *InitWinInitNodes(char *szInFile);
void              UpdateWininit(PSZ szUninstallFilename);
char              *GetSaveInstallerPath(char *szBuf, ULONG dwBufSize);
void              SaveInstallerFiles(void);
void              ResetComponentAttributes(char *szFileIni);
BOOL              IsInList(ULONG dwCurrentItem,
                           ULONG dwItems,
                           ULONG *dwItemsSelected);
int               LocateExistingPath(char *szPath,
                                     char *szExistingPath,
                                     ULONG dwExistingPathSize);
BOOL              ContainsReparseTag(char *szPath,
                                     char *szReparsePath,
                                     ULONG dwReparsePathSize);
BOOL              DeleteInstallLogFile(char *szFile);
int               CRCCheckDownloadedArchives(char *szCorruptedArchiveList,
                                             ULONG dwCorruptedArchivelistSize,
                                             char *szFileIdiGetArchives);
int               CRCCheckArchivesStartup(char *szCorruptedArchiveList,
                                          ULONG dwCorruptedArchiveListSize,
                                          BOOL bIncludeTempPath);
BOOL              ResolveForceUpgrade(siC *siCObject);
void              SwapFTPAndHTTP(char *szInUrl, ULONG dwInUrlSize);
void              ClearWinRegUninstallFileDeletion(void);
int               AppendToGlobalMessageStream(char *szInfo);
char              *GetOSTypeString(char *szOSType, ULONG dwOSTypeBufSize);
int               UpdateIdiFile(char  *szPartialUrl,
                                ULONG dwPartialUrlBufSize,
                                siC   *siCObject,
                                char  *szSection,
                                char  *szKey,
                                char  *szFileIdiGetArchives);
void              SetDownloadFile(void);
void              UnsetSetupCurrentDownloadFile(void);
void              SetSetupCurrentDownloadFile(char *szCurrentFilename);
char              *GetSetupCurrentDownloadFile(char *szCurrentDownloadFile,
                                       ULONG dwCurrentDownloadFileBufSize);
BOOL              DeleteWGetLog(void);

#endif /* _EXTRA_H_ */

