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
HRESULT     ProcessCreateCustomFiles(DWORD dwTiming);
HRESULT     FileDelete(LPSTR szDestination);
HRESULT     ProcessDeleteFile(DWORD dwTiming, char *szSectionPrefix);
HRESULT     DirectoryRemove(LPSTR szDestination, BOOL bRemoveSubdirs);
HRESULT     ProcessRemoveDirectory(DWORD dwTiming, char *szSectionPrefix);
HRESULT     ProcessRunApp(DWORD dwTiming, char *szSectionPrefix);
HRESULT     ProcessOS2INI(ULONG ulTiming, char *szSectionPrefix);
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
HRESULT     FileCopySequential(LPSTR szSourcePath, LPSTR szDestPath, LPSTR szFilename);
HRESULT     ProcessCopyFileSequential(DWORD dwTiming, char *szSectionPrefix);
void        UpdateInstallLog(PSZ szKey, PSZ szString, BOOL bDnu);
void        UpdateInstallStatusLog(PSZ szString);
void        UpdateJSProxyInfo(void);
int         VerifyArchive(LPSTR szArchive);
HRESULT     ProcessSetVersionRegistry(DWORD dwTiming, char *szSectionPrefix);
char        *BuildNumberedString(DWORD dwIndex, char *szInputStringPrefix, char *szInputString, char *szOutBuf, DWORD dwOutBufSize);
void        GetUserAgentShort(char *szUserAgent, char *szOutUAShort, DWORD dwOutUAShortSize);
void        CleanupPreviousVersionINIKeys(void);
HRESULT     CleanupArgsRegistry();
void        ProcessFileOpsForSelectedComponents(DWORD dwTiming);
void        ProcessFileOpsForAll(DWORD dwTiming);

#endif /* _IFUNCNS_H_ */

