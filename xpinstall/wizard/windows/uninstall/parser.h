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

#define KEY_INSTALLING                  "installing: "
#define KEY_REPLACING                   "replacing: "
#define KEY_STORE_REG_STRING            "store registry value string: "
#define KEY_STORE_REG_NUMBER            "store registry value number: "
#define KEY_CREATE_REG_KEY              "create registry key: "
#define KEY_COPY_FILE                   "copy file: "
#define KEY_MOVE_FILE                   "move file: "
#define KEY_RENAME_FILE                 "rename file: "
#define KEY_CREATE_FOLDER               "create folder: "
#define KEY_RENAME_DIR                  "rename dir: "
#define KEY_WINDOWS_SHORTCUT            "windows shortcut: "

sil         *InitSilNodes(char *szFileIni);
void        DeInitSilNodes(sil **silHead);
void        ParseCommandLine(LPSTR lpszCmdLine);
HRESULT     FileExists(LPSTR szFile);
void        Uninstall(sil* silFile);
void        ParseForFile(LPSTR szString, LPSTR szKey, LPSTR szShortFilename, DWORD dwShortFilenameBufSize);
void        ParseForWinRegInfo(LPSTR szString, LPSTR szKeyStr, LPSTR szRootKey, DWORD dwRootKeyBufSize, LPSTR szKey, DWORD dwKeyBufSize, LPSTR szName, DWORD dwNameBufSize);
void        DeleteWinRegKey(HKEY hkRootKey, LPSTR szKey);
void        DeleteWinRegValue(HKEY hkRootKey, LPSTR szKey, LPSTR szName);
DWORD       GetLogFile(LPSTR szTargetPath, LPSTR szInFilename, LPSTR szOutBuf, DWORD dwOutBufSize);
void        RemoveUninstaller(LPSTR szUninstallFilename);

#endif

