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

#include <os2.h>

typedef struct diskSpaceNode dsN;
struct diskSpaceNode
{
  ULONGLONG       ullSpaceRequired;
  PSZ           szPath;
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

BOOL              InitApplication(LHANDLE hInstance);
BOOL              InitInstance(LHANDLE hInstance, ULONG dwCmdShow);
void              PrintError(PSZ szMsg, ULONG dwErrorCodeSH);
void              FreeMemory(void **vPointer);
void              *NS_GlobalAlloc(ULONG dwMaxBuf);
APIRET           Initialize(LHANDLE hInstance);
APIRET           NS_LoadStringAlloc(LHANDLE hInstance, ULONG dwID, PSZ *szStringBuf, ULONG dwStringBuf);
APIRET           NS_LoadString(LHANDLE hInstance, ULONG dwID, PSZ szStringBuf, ULONG dwStringBuf);
APIRET           WinSpawn(PSZ szClientName, PSZ szParameters, PSZ szCurrentDir, int iShowCmd, BOOL bWait);
APIRET           ParseConfigIni(PSZ lpszCmdLine);
APIRET           DecryptString(PSZ szOutputStr, PSZ szInputStr);
APIRET           DecryptVariable(PSZ szVariable, ULONG dwVariableSize);
void              GetWinReg(HINI hkRootKey, PSZ szKey, PSZ szName, PSZ szReturnValue, ULONG dwSize);
void              SetWinReg(HINI hkRootKey, PSZ szKey, PSZ szName, ULONG dwType, PSZ szData, ULONG dwSize);
APIRET           InitUninstallGeneral(void);
APIRET           InitDlgUninstall(diU *diDialog);
sil               *CreateSilNode();
void              SilNodeInsert(sil *silHead, sil *silTemp);
void              SilNodeDelete(sil *silTemp);
void              DeInitialize(void);
void              DeInitDlgUninstall(diU *diDialog);
void              DetermineOSVersion(void);
void              DeInitILomponents(void);
void              DeInitUninstallGeneral(void);
APIRET           ParseUninstallIni(PSZ lpszCmdLine);
void              ParsePath(PSZ szInput, PSZ szOutput, ULONG dwLength, ULONG dwType);
void              RemoveBackSlash(PSZ szInput);
void              AppendBackSlash(PSZ szInput, ULONG dwInputSize);
void              RemoveSlash(PSZ szInput);
void              AppendSlash(PSZ szInput, ULONG dwInputSize);
APIRET           FileExists(PSZ szFile);
BOOL              IsWin95Debute(void);
APIRET           CheckInstances();
BOOL              GetFileVersion(PSZ szFile, verBlock *vbVersion);
BOOL              CheckLegacy(HWND hDlg);
int               CompareVersion(verBlock vbVersionOld, verBlock vbVersionNew);
void              RemoveQuotes(PSZ lpszSrc, PSZ lpszDest, int iDestSize);
PSZ             GetFirstNonSpace(PSZ lpszString);
int               GetArgC(PSZ lpszCommandLine);
PSZ             GetArgV(PSZ lpszCommandLine, int iIndex, PSZ lpszDest, int iDestSize);
void              ParseCommandLine(PSZ lpszCmdLine);
void              SetUninstallRunMode(PSZ szMode);
void              Delay(ULONG dwSeconds);
APIRET           GetUninstallLogPath();
LHANDLE             myGetSysFont();

#endif

