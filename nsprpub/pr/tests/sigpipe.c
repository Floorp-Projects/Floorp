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
 *************************************************************************
 *
 * Test: sigpipe.c
 *
 *     Test the SIGPIPE handler in NSPR.  This test applies to Unix only.
 *
 *************************************************************************
 */

#if !defined(XP_UNIX) && !defined(XP_OS2)

int main(void)
{
    /* This test applies to Unix and OS/2 (emx build). */
    return 0;
}

#else /* XP_UNIX && OS/2 */

#include "nspr.h"

#ifdef XP_OS2
#define INCL_DOSQUEUES
#define INCL_DOSERRORS
#include <os2.h>
#endif

#include <stdio.h>
#ifdef XP_OS2_VACPP
#define EPIPE EBADF  /* IBM's write() doesn't return EPIPE */
#include <io.h>
#else
#include <unistd.h>
#endif
#include <errno.h>

static void Test(void *arg)
{
#ifdef XP_OS2
    HFILE pipefd[2];
#else
    int pipefd[2];
#endif
    int rv;
    char c = '\0';

#ifdef XP_OS2
    if (DosCreatePipe(&pipefd[0], &pipefd[1], 4096) != 0) {
#else
    if (pipe(pipefd) == -1) {
#endif
        fprintf(stderr, "cannot create pipe: %d\n", errno);
        exit(1);
    }
    close(pipefd[0]);

    rv = write(pipefd[1], &c, 1);
    if (rv != -1) {
        fprintf(stderr, "write to broken pipe should have failed with EPIPE but returned %d\n", rv);
        exit(1);
    }
    if (errno != EPIPE) {
        fprintf(stderr, "write to broken pipe failed but with wrong errno: %d\n", errno);
        exit(1);
    }
    close(pipefd[1]);
    printf("write to broken pipe failed with EPIPE, as expected\n");
}

int main(void)
{
    PRThread *thread;

    /* This initializes NSPR. */
    PR_SetError(0, 0);

    thread = PR_CreateThread(PR_USER_THREAD, Test, NULL, PR_PRIORITY_NORMAL,
            PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0);
    if (thread == NULL) {
        fprintf(stderr, "PR_CreateThread failed\n");
        exit(1);
    }
    if (PR_JoinThread(thread) == PR_FAILURE) {
        fprintf(stderr, "PR_JoinThread failed\n");
        exit(1);
    }
    Test(NULL);

    printf("PASSED\n");
    return 0;
}

#endif /* XP_UNIX */
