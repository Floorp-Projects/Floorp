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

#ifndef _PARSER_H_
#define _PARSER_H_

sil         *InitSilNodes(char *szFileIni);
void        DeInitSilNodes(sil **silHead);
void        ParseCommandLine(PSZ lpszCmdLine);
APIRET     FileExists(PSZ szFile);
ULONG       Uninstall(sil* silFile);
void        ParseForFile(PSZ szString, PSZ szKey, PSZ szFile, ULONG dwShortFilenameBufSize);
void        ParseForCopyFile(PSZ szString, PSZ szKeyStr, PSZ szFile, ULONG dwShortFilenameBufSize);
void        ParseForWinRegInfo(PSZ szString, PSZ szKeyStr, PSZ szRootKey, ULONG dwRootKeyBufSize, PSZ szKey, ULONG dwKeyBufSize, PSZ szName, ULONG dwNameBufSize);
void        DeleteWinRegKey(HINI hkRootKey, PSZ szKey, BOOL bAbsoluteDelete);
void        DeleteWinRegValue(HINI hkRootKey, PSZ szKey, PSZ szName);
ULONG       GetLogFile(PSZ szTargetPath, PSZ szInFilename, PSZ szOutBuf, ULONG dwOutBufSize);
void        RemoveUninstaller(PSZ szUninstallFilename);
ULONG       DecrementSharedFileCounter(char *file);
BOOL        DeleteOrDelayUntilReboot(PSZ szFile);
BOOL        UnregisterServer(char *file);
int         GetSharedFileCount(char *file);
BOOL        DetermineUnRegisterServer(sil *silInstallLogHead, PSZ szFile);

#endif

