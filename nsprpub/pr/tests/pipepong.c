/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * File: pipepong.c
 *
 * Description:
 * This test runs in conjunction with the pipeping test.
 * The pipeping test creates two pipes and redirects the
 * stdin and stdout of this test to the pipes.  Then the
 * pipeping test writes "ping" to this test and this test
 * writes "pong" back.  Note that this test does not depend
 * on NSPR at all.  To run this pair of tests, just invoke
 * pipeping.
 *
 * Tested areas: process creation, pipes, file descriptor
 * inheritance, standard I/O redirection.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NUM_ITERATIONS 10

int main(int argc, char **argv)
{
    char buf[1024];
    size_t nBytes;
    int idx;

    for (idx = 0; idx < NUM_ITERATIONS; idx++) {
        memset(buf, 0, sizeof(buf));
        nBytes = fread(buf, 1, 5, stdin);
        fprintf(stderr, "pong process: received \"%s\"\n", buf);
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
        fprintf(stderr, "pong process: sending \"%s\"\n", buf);
        nBytes = fwrite(buf, 1, 5, stdout);
        if (nBytes != 5) {
            fprintf(stderr, "pong process: fwrite failed\n");
            exit(1);
        }
        fflush(stdout);
    }

    return 0;
}
