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
 * Portions created by the Initial Developer are Copyright (C) 2000
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
 * This test exercises the code that allocates and frees the syspoll_list
 * array of PRThread in the pthreads version.  This test is intended to be
 * run under Purify to verify that there is no memory leak.
 */

#include "nspr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define POLL_DESC_COUNT 256  /* This should be greater than the
                              * STACK_POLL_DESC_COUNT macro in
                              * ptio.c to cause syspoll_list to
                              * be created. */

static PRPollDesc pd[POLL_DESC_COUNT];

static void Test(void)
{
    int i;
    PRInt32 rv;
    PRIntervalTime timeout;

    timeout = PR_MillisecondsToInterval(10);
    /* cause syspoll_list to grow */
    for (i = 1; i <= POLL_DESC_COUNT; i++) {
        rv = PR_Poll(pd, i, timeout);
        if (rv != 0) {
            fprintf(stderr,
                "PR_Poll should time out but returns %d (%d, %d)\n",
                rv, PR_GetError(), PR_GetOSError());
            exit(1);
        }
    }
    /* syspoll_list should be large enough for all these */
    for (i = POLL_DESC_COUNT; i >= 1; i--) {
        rv = PR_Poll(pd, i, timeout);
        if (rv != 0) {
            fprintf(stderr, "PR_Poll should time out but returns %d\n", rv);
            exit(1);
        }
    }
}

static void ThreadFunc(void *arg)
{
    Test();
}

int main(int argc, char **argv)
{
    int i;
    PRThread *thread;
    PRFileDesc *sock;
    PRNetAddr addr;

    memset(&addr, 0, sizeof(addr));
    addr.inet.family = PR_AF_INET;
    addr.inet.port = PR_htons(0);
    addr.inet.ip = PR_htonl(PR_INADDR_ANY);
    for (i = 0; i < POLL_DESC_COUNT; i++) {
        sock = PR_NewTCPSocket();
        if (sock == NULL) {
            fprintf(stderr, "PR_NewTCPSocket failed\n");
            exit(1);
        }
        if (PR_Bind(sock, &addr) == PR_FAILURE) {
            fprintf(stderr, "PR_Bind failed\n");
            exit(1);
        }
        if (PR_Listen(sock, 5) == PR_FAILURE) {
            fprintf(stderr, "PR_Listen failed\n");
            exit(1);
        }
    
        pd[i].fd = sock;
        pd[i].in_flags = PR_POLL_READ;
    }

    /* first run the test on the primordial thread */
    Test();

    /* then run the test on all three kinds of threads */
    thread = PR_CreateThread(PR_USER_THREAD, ThreadFunc, NULL,
            PR_PRIORITY_NORMAL, PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);
    if (NULL == thread) {
        fprintf(stderr, "PR_CreateThread failed\n");
        exit(1);
    }
    if (PR_JoinThread(thread) == PR_FAILURE) {
        fprintf(stderr, "PR_JoinThread failed\n");
        exit(1);
    }
    thread = PR_CreateThread(PR_USER_THREAD, ThreadFunc, NULL,
            PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0);
    if (NULL == thread) {
        fprintf(stderr, "PR_CreateThread failed\n");
        exit(1);
    }
    if (PR_JoinThread(thread) == PR_FAILURE) {
        fprintf(stderr, "PR_JoinThread failed\n");
        exit(1);
    }
    thread = PR_CreateThread(PR_USER_THREAD, ThreadFunc, NULL,
            PR_PRIORITY_NORMAL, PR_GLOBAL_BOUND_THREAD, PR_JOINABLE_THREAD, 0);
    if (NULL == thread) {
        fprintf(stderr, "PR_CreateThread failed\n");
        exit(1);
    }
    if (PR_JoinThread(thread) == PR_FAILURE) {
        fprintf(stderr, "PR_JoinThread failed\n");
        exit(1);
    }
    for (i = 0; i < POLL_DESC_COUNT; i++) {
        if (PR_Close(pd[i].fd) == PR_FAILURE) {
            fprintf(stderr, "PR_Close failed\n");
            exit(1);
        }
    }
    PR_Cleanup();
    printf("PASS\n");
    return 0;
}
