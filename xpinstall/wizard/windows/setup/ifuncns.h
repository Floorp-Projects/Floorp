/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code,
 * released March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Sean Su <ssu@netscape.com>
 */

#ifndef _IFUNCNS_H_
#define _IFUNCNS_H_

HRESULT     TimingCheck(DWORD dwTiming, LPSTR szSection, LPSTR szFile);
HRESULT     FileUncompress(LPSTR szFrom, LPSTR szTo);
HRESULT     ProcessCoreFile(void);
HRESULT     CleanupCoreFile(void);
HRESULT     ProcessUncompressFile(DWORD dwTiming);
HRESULT     FileMove(LPSTR szFrom, LPSTR szTo);
HRESULT     ProcessMoveFile(DWORD dwTiming);
HRESULT     FileCopy(LPSTR szFrom, LPSTR szTo, BOOL bFailIfExists);
HRESULT     ProcessCopyFile(DWORD dwTiming);
HRESULT     ProcessCreateDirectory(DWORD dwTiming);
HRESULT     FileDelete(LPSTR szDestination);
HRESULT     ProcessDeleteFile(DWORD dwTiming);
HRESULT     DirectoryRemove(LPSTR szDestination, BOOL bRemoveSubdirs);
HRESULT     ProcessRemoveDirectory(DWORD dwTiming);
HRESULT     ProcessRunApp(DWORD dwTiming);
HRESULT     CreateALink(LPSTR lpszPathObj,
                        LPSTR lpszPathLink,
                        LPSTR lpszDesc,
                        LPSTR lpszWorkingPath,
                        LPSTR lpszArgs,
                        LPSTR lpszIconFullPath,
                        int iIcon);
HRESULT     ProcessProgramFolder(DWORD dwTiming);
HRESULT     ProcessProgramFolderShowCmd(void);
HRESULT     CreateDirectoriesAll(char* szPath);
void        ProcessFileOps(DWORD dwTiming);

#endif
