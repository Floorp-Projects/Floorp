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
 * Copyright (C) 2000 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * File: pipepong2.c
 *
 * Description:
 * This test runs in conjunction with the pipeping2 test.
 * The pipeping2 test creates two pipes and passes two
 * pipe fd's to this test.  Then the pipeping2 test writes
 * "ping" to this test and this test writes "pong" back.
 * To run this pair of tests, just invoke pipeping2.
 *
 * Tested areas: process creation, pipes, file descriptor
 * inheritance.
 */

#include "prerror.h"
#include "prio.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NUM_ITERATIONS 10

int main()
{
    PRFileDesc *pipe_read, *pipe_write;
    PRStatus status;
    char buf[1024];
    PRInt32 nBytes;
    int idx;

    pipe_read = PR_GetInheritedFD("PIPE_READ");
    if (pipe_read == NULL) {
        fprintf(stderr, "PR_GetInheritedFD failed\n");
        exit(1);
    }
    status = PR_SetFDInheritable(pipe_read, PR_FALSE);
    if (status == PR_FAILURE) {
        fprintf(stderr, "PR_SetFDInheritable failed\n");
        exit(1);
    }
    pipe_write = PR_GetInheritedFD("PIPE_WRITE");
    if (pipe_write == NULL) {
        fprintf(stderr, "PR_GetInheritedFD failed\n");
        exit(1);
    }
    status = PR_SetFDInheritable(pipe_write, PR_FALSE);
    if (status == PR_FAILURE) {
        fprintf(stderr, "PR_SetFDInheritable failed\n");
        exit(1);
    }

    for (idx = 0; idx < NUM_ITERATIONS; idx++) {
        memset(buf, 0, sizeof(buf));
        nBytes = PR_Read(pipe_read, buf, sizeof(buf));
        if (nBytes == -1) {
            fprintf(stderr, "PR_Read failed: (%d, %d)\n",
                    PR_GetError(), PR_GetOSError());
            exit(1);
        }
        printf("pong process: received \"%s\"\n", buf);
        if (nBytes != 5) {
            fprintf(stderr, "pong process: expected 5 bytes but got %d bytes\n",
                    nBytes);
            exit(1);
        }
        if (strcmp(buf, "ping") != 0) {
            fprintf(stderr, "pong process: expected \"ping\" but got \"%s\"\n",
                    buf);
            exit(1);
        }

        strcpy(buf, "pong");
        printf("pong process: sending \"%s\"\n", buf);
        nBytes = PR_Write(pipe_write, buf, 5);
        if (nBytes == -1) {
            fprintf(stderr, "PR_Write failed\n");
            exit(1);
        }
    }

    status = PR_Close(pipe_read);
    if (status == PR_FAILURE) {
        fprintf(stderr, "PR_Close failed\n");
        exit(1);
    }
    status = PR_Close(pipe_write);
    if (status == PR_FAILURE) {
        fprintf(stderr, "PR_Close failed\n");
        exit(1);
    }
    return 0;
}
