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

#ifndef _PARSER_H_
#define _PARSER_H_

sil         *InitSilNodes(char *szFileIni);
void        DeInitSilNodes(sil **silHead);
ULONG       ParseCommandLine(int argc, char *argv[]);
HRESULT     FileExists(PSZ szFile);
ULONG       Uninstall(sil* silFile);
void        ParseForFile(PSZ szString, PSZ szKey, PSZ szFile, ULONG ulShortFilenameBufSize);
void        ParseForCopyFile(PSZ szString, PSZ szKeyStr, PSZ szFile, ULONG ulShortFilenameBufSize);
void        ParseForWinRegInfo(PSZ szString, PSZ szKeyStr, PSZ szRootKey, ULONG ulRootKeyBufSize, PSZ szKey, ULONG ulKeyBufSize, PSZ szName, ULONG ulNameBufSize);
ULONG       GetLogFile(PSZ szTargetPath, PSZ szInFilename, PSZ szOutBuf, ULONG ulOutBufSize);
void        RemoveUninstaller(PSZ szUninstallFilename);
ULONG       DecrementSharedFileCounter(char *file);
BOOL        DeleteOrDelayUntilReboot(PSZ szFile);
BOOL        UnregisterServer(char *file);
int         GetSharedFileCount(char *file);
BOOL        DetermineUnRegisterServer(sil *silInstallLogHead, PSZ szFile);

#endif

