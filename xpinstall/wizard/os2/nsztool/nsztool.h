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

#ifndef _TESTXPI_H_
#define _TESTXPI_H_

#define MAX_BUF                         4096

#define ERROR_CODE_HIDE                 0
#define ERROR_CODE_SHOW                 1

/* PP: Parse Path */
#define PP_FILENAME_ONLY                1
#define PP_PATH_ONLY                    2
#define PP_ROOT_ONLY                    3

void            PrintError(PSZ szMsg, ULONG dwErrorCodeSH);
void            RemoveQuotes(PSZ lpszSrc, PSZ lpszDest, int dwDestSize);
void            RemoveBackSlash(PSZ szInput);
void            AppendBackSlash(PSZ szInput, ULONG dwInputSize);
void            ParsePath(PSZ szInput, PSZ szOutput, ULONG dwOutputSize, ULONG dwType);
APIRET            FileExists(PSZ szFile);
int             GetArgC(PSZ lpszCommandLine);
PSZ           GetArgV(PSZ lpszCommandLine, int iIndex, PSZ lpszDest, int iDestSize);
void            AddFile(LHANDLE hExe, PSZ lpszFile);
BOOL APIENTRY   ExtractFilesProc(LHANDLE hModule, PSZ lpszType, PSZ lpszName, LONG lParam);

#endif

