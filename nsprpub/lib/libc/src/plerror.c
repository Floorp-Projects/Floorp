/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
