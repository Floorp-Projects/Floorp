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

#ifndef _EXTRA_H_
#define _EXTRA_H_

#include "supersede.h"

BOOL              InitDialogClass(HINSTANCE hInstance, HINSTANCE hSetupRscInst);
BOOL              InitApplication(HINSTANCE hInstance, HINSTANCE hSetupRscInst);
BOOL              InitInstance(HINSTANCE hInstance, DWORD dwCmdShow);
void              PrintError(LPSTR szMsg, DWORD dwErrorCodeSH);
void              FreeMemory(void **vPointer);
void              *NS_GlobalReAlloc(HGLOBAL *hgMemory,
                                    DWORD dwMemoryBufSize,
                                    DWORD dwNewSize);
void              *NS_GlobalAlloc(DWORD dwMaxBuf);
HRESULT           Initialize(HINSTANCE hInstance);
HRESULT           NS_LoadStringAlloc(HANDLE hInstance, DWORD dwID, LPSTR *szStringBuf, DWORD dwStringBuf);
HRESULT           NS_LoadString(HANDLE hInstance, DWORD dwID, LPSTR szStringBuf, DWORD dwStringBuf);
HRESULT           WinSpawn(LPSTR szClientName, LPSTR szParameters, LPSTR szCurrentDir, int iShowCmd, BOOL bWait);
HRESULT           ParseConfigIni(LPSTR lpszCmdLine);
HRESULT           ParseInstallIni();
HRESULT           DecryptString(LPSTR szOutputStr, LPSTR szInputStr);
HRESULT           DecryptVariable(LPSTR szVariable, DWORD dwVariableSize);
void              CollateBackslashes(LPSTR szInputOutputStr);
HRESULT           InitSetupGeneral(void);
HRESULT           InitDlgWelcome(diW *diDialog);
HRESULT           InitDlgLicense(diL *diDialog);
HRESULT           InitDlgQuickLaunch(diQL *diDialog);
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
void              SiCNodeSetAttributes(DWORD dwIndex, DWORD dwAttributes, BOOL bSet, BOOL bIncludeInvisible, DWORD dwACFlag, HWND hwndListBox);
void              SiCNodeSetItemsSelected(DWORD dwSetupType);
char              *SiCNodeGetReferenceName(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag);
char              *SiCNodeGetDescriptionShort(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag);
char              *SiCNodeGetDescriptionLong(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag);
char              *SiCNodeGetReferenceName(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag);
siC               *SiCNodeGetObject(DWORD dwIndex, BOOL bIncludeInvisibleObjs, DWORD dwACFlag);
ULONGLONG         SiCNodeGetInstallSize(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag);
ULONGLONG         SiCNodeGetInstallSizeSystem(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag);
ULONGLONG         SiCNodeGetInstallSizeArchive(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag);
siC               *SiCNodeFind(siC *siComponents, char *szInReferenceName);
void              InitSiComponents(char *szFileIni);
HRESULT           ParseComponentAttributes(char *szBuf, DWORD dwAttributes, BOOL bOverride);
void              InitSiteSelector(char *szFileIni);
void              DeInitSiCDependencies(siCD *siCDDependencies);
BOOL              ResolveDependencies(DWORD dwIndex, HWND hwndListBox);
BOOL              ResolveComponentDependency(siCD *siCDInDependency, HWND hwndListBox);
void              ResolveDependees(LPSTR szToggledDescriptionShort, HWND hwndListBox);
BOOL              ResolveComponentDependee(siCD *siCDInDependee);
void              STSetVisibility(st *stSetupType);
HRESULT           InitSXpcomFile(void);
void              DeInitSXpcomFile(void);
void              DeInitialize(void);
void              DeInitDlgWelcome(diW *diDialog);
void              DeInitDlgLicense(diL *diDialog);
void              DeInitDlgQuickLaunch(diQL *diDialog);
void              DeInitDlgSetupType(diST *diDialog);
void              DeInitDlgSelectComponents(diSC *diDialog);
void              DeInitDlgWindowsIntegration(diWI *diDialog);
void              DeInitDlgProgramFolder(diPF *diDialog);
void              DeInitDlgStartInstall(diSI *diDialog);
void              DeInitDlgSiteSelector(diAS *diDialog);
void              DetermineOSVersionEx(void);
void              DeInitSiComponents(siC **siComponents);
void              DeInitSetupGeneral(void);
HRESULT           ParseSetupIni(void);
HRESULT           GetConfigIni(void);
HRESULT           GetInstallIni(void);
void              CleanTempFiles(void);
void              OutputSetupTitle(HDC hDC);
long              RetrieveArchives(void);
long              RetrieveRedirectFile(void);
void              ParsePath(LPSTR szInput, LPSTR szOutput, DWORD dwOutputSize, BOOL bUseSlash, DWORD dwType);
void              RemoveBackSlash(LPSTR szInput);
void              AppendBackSlash(LPSTR szInput, DWORD dwInputSize);
void              RemoveSlash(LPSTR szInput);
void              AppendSlash(LPSTR szInput, DWORD dwInputSize);
BOOL              DeleteIdiGetConfigIni(void);
BOOL              DeleteIdiGetArchives(void);
BOOL              DeleteIdiGetRedirect(void);
BOOL              DeleteIdiFileIniConfig(void);
BOOL              DeleteIdiFileIniInstall(void);
void              DeleteArchives(DWORD dwDeleteCheck);
BOOL              DeleteIniRedirect(void);
HRESULT           LaunchApps(void);
HRESULT           FileExists(LPSTR szFile);
int               ExtractDirEntries(char* directory,void* vZip);
int               LocateJar(siC *siCObject, LPSTR szPath, int dwPathSize, BOOL bIncludeTempDir);
HRESULT           AddArchiveToIdiFile(siC *siCObject,
                                      char *szSFile,
                                      char *szFileIdiGetArchives);
int               SiCNodeGetIndexDS(char *szInDescriptionShort);
int               SiCNodeGetIndexRN(char *szInReferenceName);
void              ViewSiComponentsDependency(char *szBuffer, char *szIndentation, siC *siCNode);
void              ViewSiComponentsDependee(char *szBuffer, char *szIndentation, siC *siCNode);
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
BOOL              CheckLegacy(HWND hDlg);
COLORREF          DecryptFontColor(LPSTR szColor);
ssi               *CreateSsiSiteSelectorNode();
void              SsiSiteSelectorNodeInsert(ssi **ssiHead, ssi *ssiTemp);
void              SsiSiteSelectorNodeDelete(ssi *ssiTemp);
ssi*              SsiGetNode(LPSTR szDescription);
void              UpdateSiteSelector(void);
DWORD             GetAdditionalComponentsCount(void);
DWORD             GetTotalArchivesToDownload();
void              RemoveQuotes(LPSTR lpszSrc, LPSTR lpszDest, int iDestSize);
int               MozCopyStr(LPSTR szSrc, LPSTR szDest, DWORD dwDestBufSize);
LPSTR             GetFirstNonSpace(LPSTR lpszString);
LPSTR             MozStrChar(LPSTR lpszString, char c);
int               GetArgC(LPSTR lpszCommandLine);
LPSTR             GetArgV(LPSTR lpszCommandLine,
                          int iIndex,
                          LPSTR lpszDest,
                          int iDestSize);
DWORD             ParseCommandLine(LPSTR lpszCmdLine);
DWORD             ParseForStartupOptions(LPSTR aCmdLine);
void              SetSetupRunMode(LPSTR szMode);
void              Delay(DWORD dwSeconds);
void              UnsetSetupState(void);
void              SetSetupState(char *szState);
siCD              *InitWinInitNodes(char *szInFile);
void              UpdateWininit(LPSTR szUninstallFilename);
char              *GetSaveInstallerPath(char *szBuf, DWORD dwBufSize);
void              SaveInstallerFiles(void);
void              ResetComponentAttributes(char *szFileIni);
BOOL              IsInList(DWORD dwCurrentItem,
                           DWORD dwItems,
                           DWORD *dwItemsSelected);
int               LocateExistingPath(char *szPath,
                                     char *szExistingPath,
                                     DWORD dwExistingPathSize);
BOOL              ContainsReparseTag(char *szPath,
                                     char *szReparsePath,
                                     DWORD dwReparsePathSize);
BOOL              DeleteInstallLogFile(char *szFile);
int               CRCCheckDownloadedArchives(char *szCorruptedArchiveList,
                                             DWORD dwCorruptedArchivelistSize,
                                             char *szFileIdiGetArchives);
int               CRCCheckArchivesStartup(char *szCorruptedArchiveList,
                                          DWORD dwCorruptedArchiveListSize,
                                          BOOL bIncludeTempPath);
BOOL              ResolveForceUpgrade(siC *siCObject);
void              RestoreInvisibleFlag(siC *siCNode);
void              RestoreAdditionalFlag(siC *siCNode);
void              RestoreEnabledFlag(siC *siCNode);
void              SwapFTPAndHTTP(char *szInUrl, DWORD dwInUrlSize);
void              ClearWinRegUninstallFileDeletion(void);
void              RemoveDelayedDeleteFileEntries(const char *aPathToMatch);
int               AppendToGlobalMessageStream(char *szInfo);
char              *GetOSTypeString(char *szOSType, DWORD dwOSTypeBufSize);
int               UpdateIdiFile(char  *szPartialUrl,
                                DWORD dwPartialUrlBufSize,
                                siC   *siCObject,
                                char  *szSection,
                                char  *szKey,
                                char  *szFileIdiGetArchives);
void              SetDownloadFile(void);
void              UnsetSetupCurrentDownloadFile(void);
void              SetSetupCurrentDownloadFile(char *szCurrentFilename);
char              *GetSetupCurrentDownloadFile(char *szCurrentDownloadFile,
                                       DWORD dwCurrentDownloadFileBufSize);
BOOL              DeleteWGetLog(void);
DWORD             ParseOSType(char *szOSType);
BOOL              ShowAdditionalOptionsDialog(void);
DWORD             GetPreviousUnfinishedState(void);
void              RefreshIcons();
void              NeedToInstallFiles(LPSTR szProdDir);
void              LaunchOneComponent(siC *siCObject, greInfo *aGre);
HRESULT           ProcessXpinstallEngine(void);
void              GetXpinstallPath(char *aPath, int aPathBufSize);
BOOL              GreInstallerNeedsReboot(void);
void              ReplacePrivateProfileStrCR(LPSTR aInputOutputStr);
void              UpdateGREAppInstallerProgress(int percent);
BOOL              IsPathWithinWindir();
void              CleanupOnUpgrade();
BOOL              IsInstallerProductGRE(void);
HRESULT           CleanupOrphanedGREs(void);

#endif /* _EXTRA_H_ */

