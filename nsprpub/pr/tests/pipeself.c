/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * File: pipeself.c
 *
 * Description:
 * This test has two threads communicating with each other using
 * two unidirectional pipes.  The primordial thread is the ping
 * thread and the other thread is the pong thread.  The ping
 * thread writes "ping" to the pong thread and the pong thread
 * writes "pong" back.
 */

#include "prio.h"
#include "prerror.h"
#include "prthread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_ITERATIONS 10

static PRFileDesc *ping_in, *ping_out;
static PRFileDesc *pong_in, *pong_out;

static void PongThreadFunc(void *arg)
{
    char buf[1024];
    int idx;
    PRInt32 nBytes;
    PRStatus status;

    for (idx = 0; idx < NUM_ITERATIONS; idx++) {
        memset(buf, 0, sizeof(buf));
        nBytes = PR_Read(pong_in, buf, sizeof(buf));
        if (nBytes == -1) {
            fprintf(stderr, "PR_Read failed\n");
            exit(1);
        }
        printf("pong thread: received \"%s\"\n", buf);
        if (nBytes != 5) {
            fprintf(stderr, "pong thread: expected 5 bytes but got %d bytes\n",
                    nBytes);
            exit(1);
        }
        if (strcmp(buf, "ping") != 0) {
            fprintf(stderr, "pong thread: expected \"ping\" but got \"%s\"\n",
                    buf);
            exit(1);
        }
        strcpy(buf, "pong");
        printf("pong thread: sending \"%s\"\n", buf);
        nBytes = PR_Write(pong_out, buf, 5);
        if (nBytes == -1) {
            fprintf(stderr, "PR_Write failed: (%d, %d)\n", PR_GetError(),
                    PR_GetOSError());
            exit(1);
        }
    }

    status = PR_Close(pong_in);
    if (status == PR_FAILURE) {
        fprintf(stderr, "PR_Close failed\n");
        exit(1);
    }
    status = PR_Close(pong_out);
    if (status == PR_FAILURE) {
        fprintf(stderr, "PR_Close failed\n");
        exit(1);
    }
}

int main(int argc, char **argv)
{
    PRStatus status;
    PRThread *pongThread;
    char buf[1024];
    PRInt32 nBytes;
    int idx;

    status = PR_CreatePipe(&ping_in, &pong_out);
    if (status == PR_FAILURE) {
        fprintf(stderr, "PR_CreatePipe failed\n");
        exit(1);
    }
    status = PR_CreatePipe(&pong_in, &ping_out);
    if (status == PR_FAILURE) {
        fprintf(stderr, "PR_CreatePipe failed\n");
        exit(1);
    }

    pongThread = PR_CreateThread(PR_USER_THREAD, PongThreadFunc, NULL,
            PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0);
    if (pongThread == NULL) {
        fprintf(stderr, "PR_CreateThread failed\n");
        exit(1);
    }

    for (idx = 0; idx < NUM_ITERATIONS; idx++) {
        strcpy(buf, "ping");
        printf("ping thread: sending \"%s\"\n", buf);
        nBytes = PR_Write(ping_out, buf, 5);
        if (nBytes == -1) {
            fprintf(stderr, "PR_Write failed: (%d, %d)\n", PR_GetError(),
                    PR_GetOSError());
            exit(1);
        }
        memset(buf, 0, sizeof(buf));
        nBytes = PR_Read(ping_in, buf, sizeof(buf));
        if (nBytes == -1) {
            fprintf(stderr, "PR_Read failed\n");
            exit(1);
        }
        printf("ping thread: received \"%s\"\n", buf);
        if (nBytes != 5) {
            fprintf(stderr, "ping thread: expected 5 bytes but got %d bytes\n",
                    nBytes);
            exit(1);
        }
        if (strcmp(buf, "pong") != 0) {
            fprintf(stderr, "ping thread: expected \"pong\" but got \"%s\"\n",
                    buf);
            exit(1);
        }
    }

    status = PR_Close(ping_in);
    if (status == PR_FAILURE) {
        fprintf(stderr, "PR_Close failed\n");
        exit(1);
    }
    status = PR_Close(ping_out);
    if (status == PR_FAILURE) {
        fprintf(stderr, "PR_Close failed\n");
        exit(1);
    }
    status = PR_JoinThread(pongThread);
    if (status == PR_FAILURE) {
        fprintf(stderr, "PR_JoinThread failed\n");
        exit(1);
    }

#if defined(XP_UNIX) && !defined(SYMBIAN)
	/*
	 * Test PR_Available for pipes
	 */
    status = PR_CreatePipe(&ping_in, &ping_out);
    if (status == PR_FAILURE) {
        fprintf(stderr, "PR_CreatePipe failed\n");
        exit(1);
    }
	nBytes = PR_Write(ping_out, buf, 250);
	if (nBytes == -1) {
		fprintf(stderr, "PR_Write failed: (%d, %d)\n", PR_GetError(),
				PR_GetOSError());
		exit(1);
	}
	nBytes = PR_Available(ping_in);
	if (nBytes < 0) {
		fprintf(stderr, "PR_Available failed: (%d, %d)\n", PR_GetError(),
				PR_GetOSError());
		exit(1);
	} else if (nBytes != 250) {
		fprintf(stderr, "PR_Available: expected 250 bytes but got %d bytes\n",
				nBytes);
		exit(1);
	}
	printf("PR_Available: expected %d, got %d bytes\n",250, nBytes);
	/* read some data */
	nBytes = PR_Read(ping_in, buf, 7);
	if (nBytes == -1) {
		fprintf(stderr, "PR_Read failed\n");
		exit(1);
	}
	/* check available data */
	nBytes = PR_Available(ping_in);
	if (nBytes < 0) {
		fprintf(stderr, "PR_Available failed: (%d, %d)\n", PR_GetError(),
				PR_GetOSError());
		exit(1);
	} else if (nBytes != (250 - 7)) {
		fprintf(stderr, "PR_Available: expected 243 bytes but got %d bytes\n",
				nBytes);
		exit(1);
	}
	printf("PR_Available: expected %d, got %d bytes\n",243, nBytes);
	/* read all data */
	nBytes = PR_Read(ping_in, buf, sizeof(buf));
	if (nBytes == -1) {
		fprintf(stderr, "PR_Read failed\n");
		exit(1);
	} else if (nBytes != 243) {
		fprintf(stderr, "PR_Read failed: expected %d, got %d bytes\n",
							243, nBytes);
		exit(1);
	}
	/* check available data */
	nBytes = PR_Available(ping_in);
	if (nBytes < 0) {
		fprintf(stderr, "PR_Available failed: (%d, %d)\n", PR_GetError(),
				PR_GetOSError());
		exit(1);
	} else if (nBytes != 0) {
		fprintf(stderr, "PR_Available: expected 0 bytes but got %d bytes\n",
				nBytes);
		exit(1);
	}
	printf("PR_Available: expected %d, got %d bytes\n", 0, nBytes);

    status = PR_Close(ping_in);
    if (status == PR_FAILURE) {
        fprintf(stderr, "PR_Close failed\n");
        exit(1);
    }
    status = PR_Close(ping_out);
    if (status == PR_FAILURE) {
        fprintf(stderr, "PR_Close failed\n");
        exit(1);
    }
#endif /* XP_UNIX */

    printf("PASS\n");
    return 0;
}
