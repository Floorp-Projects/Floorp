/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    /* This test applies to Unix and OS/2. */
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
#include <unistd.h>
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

int main(int argc, char **argv)
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
