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

#ifndef _IFUNCNS_H_
#define _IFUNCNS_H_

APIRET     TimingCheck(ULONG dwTiming, PSZ szSection, PSZ szFile);
APIRET     FileUncompress(PSZ szFrom, PSZ szTo);
APIRET     ProcessXpcomFile(void);
APIRET     CleanupXpcomFile(void);
APIRET     ProcessUncompressFile(ULONG dwTiming, char *szSectionPrefix);
APIRET     FileMove(PSZ szFrom, PSZ szTo);
APIRET     ProcessMoveFile(ULONG dwTiming, char *szSectionPrefix);
APIRET     FileCopy(PSZ szFrom, PSZ szTo, BOOL bFailIfExists, BOOL bDnu);
APIRET     ProcessCopyFile(ULONG dwTiming, char *szSectionPrefix);
APIRET     ProcessCreateDirectory(ULONG dwTiming, char *szSectionPrefix);
APIRET     FileDelete(PSZ szDestination);
APIRET     ProcessDeleteFile(ULONG dwTiming, char *szSectionPrefix);
APIRET     DirectoryRemove(PSZ szDestination, BOOL bRemoveSubdirs);
APIRET     ProcessRemoveDirectory(ULONG dwTiming, char *szSectionPrefix);
APIRET     ProcessRunApp(ULONG dwTiming, char *szSectionPrefix);
APIRET     ProcessWinReg(ULONG dwTiming, char *szSectionPrefix);
APIRET     CreateALink(PSZ lpszPathObj,
                        PSZ lpszPathLink,
                        PSZ lpszDesc,
                        PSZ lpszWorkingPath,
                        PSZ lpszArgs,
                        PSZ lpszIconFullPath,
                        int iIcon);
APIRET     ProcessProgramFolder(ULONG dwTiming, char *szSectionPrefix);
APIRET     ProcessProgramFolderShowCmd(void);
APIRET     CreateDirectoriesAll(char* szPath, BOOL bLogForUninstall);
void        ProcessFileOps(ULONG dwTiming, char *szSectionPrefix);
void        DeleteWinRegValue(HINI hkRootKey, PSZ szKey, PSZ szName);
void        DeleteWinRegKey(HINI hkRootKey, PSZ szKey, BOOL bAbsoluteDelete);
ULONG       GetWinReg(HINI hkRootKey, PSZ szKey, PSZ szName, PSZ szReturnValue, ULONG dwSize);
void        SetWinReg(HINI hkRootKey,
                      PSZ szKey,
                      BOOL bOverwriteKey,
                      PSZ szName,
                      BOOL bOverwriteName,
                      ULONG dwType,
                      PBYTE lpbData,
                      ULONG dwSize,
                      BOOL bLogForUninstall,
                      BOOL bDnu);
HINI        ParseRootKey(PSZ szRootKey);
char        *ParseRootKeyString(HINI hkKey,
                                PSZ szRootKey,
                                ULONG dwRootKeyBufSize);
BOOL        ParseRegType(PSZ szType, ULONG *dwType);
BOOL        WinRegKeyExists(HINI hkRootKey, PSZ szKey);
BOOL        WinRegNameExists(HINI hkRootKey, PSZ szKey, PSZ szName);
APIRET     FileCopySequential(PSZ szSourcePath, PSZ szDestPath, PSZ szFilename);
APIRET     ProcessCopyFileSequential(ULONG dwTiming, char *szSectionPrefix);
void        UpdateInstallLog(PSZ szKey, PSZ szString, BOOL bDnu);
void        UpdateInstallStatusLog(PSZ szString);
int         RegisterDll32(char *File);
APIRET     FileSelfRegister(PSZ szFilename, PSZ szDestination);
APIRET     ProcessSelfRegisterFile(ULONG dwTiming, char *szSectionPrefix);
void        UpdateJSProxyInfo(void);
int         VerifyArchive(PSZ szArchive);
APIRET     ProcessSetVersionRegistry(ULONG dwTiming, char *szSectionPrefix);
char        *BuildNumberedString(ULONG dwIndex, char *szInputStringPrefix, char *szInputString, char *szOutBuf, ULONG dwOutBufSize);
void        GetUserAgentShort(char *szUserAgent, char *szOutUAShort, ULONG dwOutUAShortSize);
void        CleanupPreviousVersionRegKeys(void);
ULONG       ParseRestrictedAccessKey(PSZ szKey);

#endif /* _IFUNCNS_H_ */

