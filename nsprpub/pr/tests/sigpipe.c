/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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

int main(void)
{
#ifdef XP_OS2
    HFILE pipefd[2];
#else
    int pipefd[2];
#endif
    int rv;
    char c = '\0';

    /* This initializes NSPR. */
    PR_SetError(0, 0);

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
    printf("PASSED\n");
    return 0;
}

#endif /* XP_UNIX */
