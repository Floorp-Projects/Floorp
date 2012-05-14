/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nspr.h"
#include "plgetopt.h"

#include <stdio.h>

#ifdef SYMBIAN
#define SHM_NAME "c:\\data\\counter"
#define SEM_NAME1 "c:\\data\\foo.sem"
#define SEM_NAME2 "c:\\data\\bar.sem"
#else
#define SHM_NAME "/tmp/counter"
#define SEM_NAME1 "/tmp/foo.sem"
#define SEM_NAME2 "/tmp/bar.sem"
#endif
#define ITERATIONS 1000

static PRBool debug_mode = PR_FALSE;
static PRIntn iterations = ITERATIONS;
static PRSem *sem1, *sem2;

static void Help(void)
{
    fprintf(stderr, "semapong test program usage:\n");
    fprintf(stderr, "\t-d           debug mode         (FALSE)\n");
    fprintf(stderr, "\t-c <count>   loop count         (%d)\n", ITERATIONS);
    fprintf(stderr, "\t-h           this message\n");
}  /* Help */

int main(int argc, char **argv)
{
    PRIntn i;
    PRSharedMemory *shm;
    PRIntn *counter_addr;
    PLOptStatus os;
    PLOptState *opt = PL_CreateOptState(argc, argv, "dc:h");

    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt))) {
        if (PL_OPT_BAD == os) continue;
        switch (opt->option) {
            case 'd':  /* debug mode */
                debug_mode = PR_TRUE;
                break;
            case 'c':  /* loop count */
                iterations = atoi(opt->value);
                break;
            case 'h':
            default:
                Help();
                return 2;
        }
    }
    PL_DestroyOptState(opt);

    shm = PR_OpenSharedMemory(SHM_NAME, sizeof(*counter_addr), 0, 0666);
    if (NULL == shm) {
        fprintf(stderr, "PR_OpenSharedMemory failed (%d, %d)\n",
                PR_GetError(), PR_GetOSError());
        exit(1);
    }
    sem1 = PR_OpenSemaphore(SEM_NAME1, 0, 0, 0);
    if (NULL == sem1) {
        fprintf(stderr, "PR_OpenSemaphore failed (%d, %d)\n",
                PR_GetError(), PR_GetOSError());
        exit(1);
    }
    sem2 = PR_OpenSemaphore(SEM_NAME2, 0, 0, 0);
    if (NULL == sem2) {
        fprintf(stderr, "PR_OpenSemaphore failed (%d, %d)\n",
                PR_GetError(), PR_GetOSError());
        exit(1);
    }

    counter_addr = PR_AttachSharedMemory(shm, 0);
    if (NULL == counter_addr) {
        fprintf(stderr, "PR_AttachSharedMemory failed\n");
        exit(1);
    }

    /*
     * Process 2 waits on semaphore 2 and posts to semaphore 1.
     */
    for (i = 0; i < iterations; i++) {
        if (PR_WaitSemaphore(sem2) == PR_FAILURE) {
            fprintf(stderr, "PR_WaitSemaphore failed\n");
            exit(1);
        }
        if (*counter_addr == 2*i+1) {
            if (debug_mode) printf("process 2: counter = %d\n", *counter_addr);
        } else {
            fprintf(stderr, "process 2: counter should be %d but is %d\n",
                    2*i+1, *counter_addr);
            exit(1);
        }
        (*counter_addr)++;
        if (PR_PostSemaphore(sem1) == PR_FAILURE) {
            fprintf(stderr, "PR_PostSemaphore failed\n");
            exit(1);
        }
    }
    if (PR_DetachSharedMemory(shm, counter_addr) == PR_FAILURE) {
        fprintf(stderr, "PR_DetachSharedMemory failed\n");
        exit(1);
    }
    if (PR_CloseSharedMemory(shm) == PR_FAILURE) {
        fprintf(stderr, "PR_CloseSharedMemory failed\n");
        exit(1);
    }
    if (PR_CloseSemaphore(sem1) == PR_FAILURE) {
        fprintf(stderr, "PR_CloseSemaphore failed\n");
    }
    if (PR_CloseSemaphore(sem2) == PR_FAILURE) {
        fprintf(stderr, "PR_CloseSemaphore failed\n");
    }
    printf("PASS\n");
    return 0;
}
