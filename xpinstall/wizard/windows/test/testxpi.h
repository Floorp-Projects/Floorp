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

#ifndef _TESTXPI_H_
#define _TESTXPI_H_

#ifdef __cplusplus
#define PR_BEGIN_EXTERN_C       extern "C" {
#define PR_END_EXTERN_C         }
#else
#define PR_BEGIN_EXTERN_C
#define PR_END_EXTERN_C
#endif

typedef unsigned  int PRUint32;
typedef           int PRInt32;

#define PR_EXTERN(type) type

#include "windows.h"
#include "xpi.h"
#include "..\\setup\\xpistub.h"
#include <stdio.h>
#include <io.h>
#include <stdlib.h>
#include <ctype.h>

#define MAX_BUF                         4096

#define ERROR_CODE_HIDE                 0
#define ERROR_CODE_SHOW                 1

/* PP: Parse Path */
#define PP_FILENAME_ONLY                1
#define PP_PATH_ONLY                    2
#define PP_ROOT_ONLY                    3

#define TEST_OK                         0

void            PrintError(LPSTR szMsg, DWORD dwErrorCodeSH, int iExitCode);
void            RemoveQuotes(LPSTR lpszSrc, LPSTR lpszDest, int dwDestSize);
void            RemoveBackSlash(LPSTR szInput);
void            AppendBackSlash(LPSTR szInput, DWORD dwInputSize);
void            ParsePath(LPSTR szInput, LPSTR szOutput, DWORD dwOutputSize, DWORD dwType);
long            FileExists(LPSTR szFile);
int             GetArgC(LPSTR lpszCommandLine);
LPSTR           GetArgV(LPSTR lpszCommandLine, int iIndex, LPSTR lpszDest, int iDestSize);

#endif

