/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1999-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
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
