/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

int main(int argc, char **argv)
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
