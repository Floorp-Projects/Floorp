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

#ifndef _IFUNCNS_H_
#define _IFUNCNS_H_

HRESULT     TimingCheck(DWORD dwTiming, LPSTR szSection, LPSTR szFile);
HRESULT     FileUncompress(LPSTR szFrom, LPSTR szTo);
HRESULT     ProcessXpcomFile(void);
HRESULT     CleanupXpcomFile(void);
HRESULT     ProcessUncompressFile(DWORD dwTiming, char *szSectionPrefix);
HRESULT     FileMove(LPSTR szFrom, LPSTR szTo);
HRESULT     ProcessMoveFile(DWORD dwTiming, char *szSectionPrefix);
HRESULT     FileCopy(LPSTR szFrom, LPSTR szTo, BOOL bFailIfExists, BOOL bDnu);
HRESULT     ProcessCopyFile(DWORD dwTiming, char *szSectionPrefix);
HRESULT     ProcessCreateDirectory(DWORD dwTiming, char *szSectionPrefix);
HRESULT     FileDelete(LPSTR szDestination);
HRESULT     ProcessDeleteFile(DWORD dwTiming, char *szSectionPrefix);
HRESULT     DirectoryRemove(LPSTR szDestination, BOOL bRemoveSubdirs);
HRESULT     ProcessRemoveDirectory(DWORD dwTiming, char *szSectionPrefix);
HRESULT     ProcessRunApp(DWORD dwTiming, char *szSectionPrefix);
HRESULT     ProcessWinReg(DWORD dwTiming, char *szSectionPrefix);
HRESULT     CreateALink(LPSTR lpszPathObj,
                        LPSTR lpszPathLink,
                        LPSTR lpszDesc,
                        LPSTR lpszWorkingPath,
                        LPSTR lpszArgs,
                        LPSTR lpszIconFullPath,
                        int iIcon);
HRESULT     ProcessProgramFolder(DWORD dwTiming, char *szSectionPrefix);
HRESULT     ProcessProgramFolderShowCmd(void);
HRESULT     CreateDirectoriesAll(char* szPath, BOOL bLogForUninstall);
void        ProcessFileOps(DWORD dwTiming, char *szSectionPrefix);
void        DeleteWinRegValue(HKEY hkRootKey, LPSTR szKey, LPSTR szName);
void        DeleteWinRegKey(HKEY hkRootKey, LPSTR szKey, BOOL bAbsoluteDelete);
DWORD       GetWinReg(HKEY hkRootKey, LPSTR szKey, LPSTR szName, LPSTR szReturnValue, DWORD dwSize);
void        SetWinReg(HKEY hkRootKey,
                      LPSTR szKey,
                      BOOL bOverwriteKey,
                      LPSTR szName,
                      BOOL bOverwriteName,
                      DWORD dwType,
                      LPBYTE lpbData,
                      DWORD dwSize,
                      BOOL bLogForUninstall,
                      BOOL bDnu);
HKEY        ParseRootKey(LPSTR szRootKey);
char        *ParseRootKeyString(HKEY hkKey,
                                LPSTR szRootKey,
                                DWORD dwRootKeyBufSize);
BOOL        ParseRegType(LPSTR szType, DWORD *dwType);
BOOL        WinRegKeyExists(HKEY hkRootKey, LPSTR szKey);
BOOL        WinRegNameExists(HKEY hkRootKey, LPSTR szKey, LPSTR szName);
HRESULT     FileCopySequential(LPSTR szSourcePath, LPSTR szDestPath, LPSTR szFilename);
HRESULT     ProcessCopyFileSequential(DWORD dwTiming, char *szSectionPrefix);
void        UpdateInstallLog(LPSTR szKey, LPSTR szString, BOOL bDnu);
void        UpdateInstallStatusLog(LPSTR szString);
int         RegisterDll32(char *File);
HRESULT     FileSelfRegister(LPSTR szFilename, LPSTR szDestination);
HRESULT     ProcessSelfRegisterFile(DWORD dwTiming, char *szSectionPrefix);
void        UpdateJSProxyInfo(void);
int         VerifyArchive(LPSTR szArchive);
HRESULT     ProcessSetVersionRegistry(DWORD dwTiming, char *szSectionPrefix);
char        *BuildNumberedString(DWORD dwIndex, char *szInputStringPrefix, char *szInputString, char *szOutBuf, DWORD dwOutBufSize);
void        GetUserAgentShort(char *szUserAgent, char *szOutUAShort, DWORD dwOutUAShortSize);
void        CleanupPreviousVersionRegKeys(void);
DWORD       ParseRestrictedAccessKey(LPSTR szKey);
void        AppendWinReg(HKEY hkRootKey,
                      LPSTR szKey,
                      LPSTR szName,
                      DWORD dwType,
                      LPBYTE lpbData,
                      BYTE delimiter,
                      DWORD dwSize,
                      BOOL bLogForUninstall,
                      BOOL bDnu);
HRESULT     CleanupArgsRegistry();
void        ProcessFileOpsForSelectedComponents(DWORD dwTiming);
void        ProcessFileOpsForAll(DWORD dwTiming);

#endif /* _IFUNCNS_H_ */

