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

#if !defined(XP_UNIX)

int main()
{
    /* This test applies to Unix only. */
    return 0;
}

#else /* XP_UNIX */

#include "nspr.h"

#include <stdio.h>
#include <unistd.h>
#include <errno.h>

int main()
{
    int pipefd[2];
    int rv;
    char c = '\0';

    /* This initializes NSPR. */
    PR_SetError(0, 0);

    if (pipe(pipefd) == -1) {
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
    printf("write to broken pipe failed with EPIPE, as expected\n");
    printf("PASSED\n");
    return 0;
}

#endif /* XP_UNIX */
