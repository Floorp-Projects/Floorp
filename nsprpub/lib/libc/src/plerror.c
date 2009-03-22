/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*
** File:plerror.c
** Description: Simple routine to print translate the calling thread's
**  error numbers and print them to "syserr".
*/

#include "plerror.h"

#include "prprf.h"
#include "prerror.h"

PR_IMPLEMENT(void) PL_FPrintError(PRFileDesc *fd, const char *msg)
{
PRErrorCode error = PR_GetError();
PRInt32 oserror = PR_GetOSError();
const char *name = PR_ErrorToName(error);

	if (NULL != msg) PR_fprintf(fd, "%s: ", msg);
    if (NULL == name)
        PR_fprintf(
			fd, " (%d)OUT OF RANGE, oserror = %d\n", error, oserror);
    else
        PR_fprintf(
            fd, "%s(%d), oserror = %d\n",
            name, error, oserror);
}  /* PL_FPrintError */

PR_IMPLEMENT(void) PL_PrintError(const char *msg)
{
	static PRFileDesc *fd = NULL;
	if (NULL == fd) fd = PR_GetSpecialFD(PR_StandardError);
	PL_FPrintError(fd, msg);
}  /* PL_PrintError */

/* plerror.c */
