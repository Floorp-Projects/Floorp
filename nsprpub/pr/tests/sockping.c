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
 * File: sockping.c
 *
 * Description:
 * This test runs in conjunction with the sockpong test.
 * This test creates a socket pair and passes one socket
 * to the sockpong test.  Then this test writes "ping" to
 * to the sockpong test and the sockpong test writes "pong"
 * back.  To run this pair of tests, just invoke sockping.
 *
 * Tested areas: process creation, socket pairs, file
 * descriptor inheritance.
 */

#include "prerror.h"
#include "prio.h"
#include "prproces.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NUM_ITERATIONS 10

static char *child_argv[] = { "sockpong", NULL };

int main()
{
    PRFileDesc *sock[2];
    PRStatus status;
    PRProcess *process;
    PRProcessAttr *attr;
    char buf[1024];
    PRInt32 nBytes;
    PRInt32 exitCode;
    int idx;

    status = PR_NewTCPSocketPair(sock);
    if (status == PR_FAILURE) {
        fprintf(stderr, "PR_NewTCPSocketPair failed\n");
        exit(1);
    }

    status = PR_SetFDInheritable(sock[1], PR_TRUE);
    if (status == PR_FAILURE) {
        fprintf(stderr, "PR_SetFDInheritable failed: (%d, %d)\n",
                PR_GetError(), PR_GetOSError());
        exit(1);
    }

    attr = PR_NewProcessAttr();
    if (attr == NULL) {
        fprintf(stderr, "PR_NewProcessAttr failed\n");
        exit(1);
    }

    status = PR_ProcessAttrSetInheritableFD(attr, sock[1], "SOCKET");
    if (status == PR_FAILURE) {
        fprintf(stderr, "PR_ProcessAttrSetInheritableFD failed\n");
        exit(1);
    }

    process = PR_CreateProcess(child_argv[0], child_argv, NULL, attr);
    if (process == NULL) {
        fprintf(stderr, "PR_CreateProcess failed\n");
        exit(1);
    }
    PR_DestroyProcessAttr(attr);
    status = PR_Close(sock[1]);
    if (status == PR_FAILURE) {
        fprintf(stderr, "PR_Close failed\n");
        exit(1);
    }

    for (idx = 0; idx < NUM_ITERATIONS; idx++) {
        strcpy(buf, "ping");
        printf("ping process: sending \"%s\"\n", buf);
        nBytes = PR_Write(sock[0], buf, 5);
        if (nBytes == -1) {
            fprintf(stderr, "PR_Write failed: (%d, %d)\n",
                    PR_GetError(), PR_GetOSError());
            exit(1);
        }
        memset(buf, 0, sizeof(buf));
        nBytes = PR_Read(sock[0], buf, sizeof(buf));
        if (nBytes == -1) {
            fprintf(stderr, "PR_Read failed\n");
            exit(1);
        }
        printf("ping process: received \"%s\"\n", buf);
        if (nBytes != 5) {
            fprintf(stderr, "ping process: expected 5 bytes but got %d bytes\n",
                    nBytes);
            exit(1);
        }
        if (strcmp(buf, "pong") != 0) {
            fprintf(stderr, "ping process: expected \"pong\" but got \"%s\"\n",
                    buf);
            exit(1);
        }
    }

    status = PR_Close(sock[0]);
    if (status == PR_FAILURE) {
        fprintf(stderr, "PR_Close failed\n");
        exit(1);
    }
    status = PR_WaitProcess(process, &exitCode);
    if (status == PR_FAILURE) {
        fprintf(stderr, "PR_WaitProcess failed\n");
        exit(1);
    }
    if (exitCode == 0) {
        printf("PASS\n");
        return 0;
    } else {
        printf("FAIL\n");
        return 1;
    }
}
