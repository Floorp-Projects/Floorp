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

#include <windows.h>

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

BOOL              InitApplication(HINSTANCE hInstance);
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
void              SetWinRegNumValue(HKEY hkRootKey, LPSTR szKey, LPSTR szName, DWORD dwData);
HRESULT           InitUninstallGeneral(void);
HRESULT           InitDlgUninstall(diU *diDialog);
sil               *CreateSilNode();
void              SilNodeInsert(sil *silHead, sil *silTemp);
void              SilNodeDelete(sil *silTemp);
void              DeInitialize(void);
void              DeInitDlgUninstall(diU *diDialog);
void              DetermineOSVersion(void);
void              DeInitILomponents(void);
void              DeInitUninstallGeneral(void);
HRESULT           ParseUninstallIni();
void              ParsePath(LPSTR szInput, LPSTR szOutput, DWORD dwLength, DWORD dwType);
void              RemoveBackSlash(LPSTR szInput);
void              AppendBackSlash(LPSTR szInput, DWORD dwInputSize);
void              RemoveSlash(LPSTR szInput);
void              AppendSlash(LPSTR szInput, DWORD dwInputSize);
HRESULT           FileExists(LPSTR szFile);
BOOL              IsWin95Debute(void);
HRESULT           CheckInstances();
BOOL              GetFileVersion(LPSTR szFile, verBlock *vbVersion);
BOOL              CheckLegacy(HWND hDlg);
int               CompareVersion(verBlock vbVersionOld, verBlock vbVersionNew);
void              RemoveQuotes(LPSTR lpszSrc, LPSTR lpszDest, int iDestSize);
int               MozCopyStr(LPSTR szSrc, LPSTR szDest, DWORD dwDestBufSize);
LPSTR             GetFirstSpace(LPSTR lpszString);
LPSTR             GetFirstNonSpace(LPSTR lpszString);
LPSTR             MozStrChar(LPSTR lpszString, char c);
int               GetArgC(LPSTR lpszCommandLine);
LPSTR             GetArgV(LPSTR lpszCommandLine, int iIndex, LPSTR lpszDest, int iDestSize);
DWORD             ParseCommandLine(LPSTR lpszCmdLine);
void              SetUninstallRunMode(LPSTR szMode);
void              Delay(DWORD dwSeconds);
HRESULT           GetAppPath();
DWORD             CleanupAppList();
DWORD             ProcessAppItem(HKEY hkRoot, LPSTR szKeyAppList, LPSTR szAppID);
void              RemovePathToExeXX(HKEY hkRootKey, LPSTR szKey, DWORD dwIndex, const DWORD dwUpperLimit);
HRESULT           GetUninstallLogPath();
BOOL              WinRegNameExists(HKEY hkRootKey, LPSTR szKey, LPSTR szName);
void              DeleteWinRegValue(HKEY hkRootKey, LPSTR szKey, LPSTR szName);
void              ReplacePrivateProfileStrCR(LPSTR aInputOutputStr);
BOOL              IsPathWithinWindir(char *aTargetPath);
void              VerifyAndDeleteInstallationFolder(void);

#endif /* _EXTRA_H_ */

