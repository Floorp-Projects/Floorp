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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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

int main()
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
