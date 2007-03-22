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
 *   IBM Corp.
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

#include <os2.h>
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

void            PrintError(PSZ szMsg, ULONG dwErrorCodeSH, int iExitCode);
void            RemoveQuotes(PSZ pszSrc, PSZ pszDest, int DestSize);
void            RemoveBackSlash(PSZ szInput);
void            AppendBackSlash(PSZ szInput, ULONG InputSize);
void            ParsePath(PSZ szInput, PSZ szOutput, ULONG OutputSize, ULONG Type);
long            FileExists(PSZ szFile);
int             GetArgC(PSZ pszCommandLine);
PSZ           GetArgV(PSZ pszCommandLine, int iIndex, PSZ pszDest, int iDestSize);

#endif

