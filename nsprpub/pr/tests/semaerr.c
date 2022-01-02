/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nspr.h"
#include "plgetopt.h"

#include <stdio.h>

#ifdef DEBUG
#define SEM_D "D"
#else
#define SEM_D
#endif
#ifdef IS_64
#define SEM_64 "64"
#else
#define SEM_64
#endif

#define NO_SUCH_SEM_NAME "/tmp/nosuchsem.sem" SEM_D SEM_64
#define SEM_NAME1 "/tmp/foo.sem" SEM_D SEM_64
#define EXE_NAME "semaerr1"
#define SEM_MODE  0666

static PRBool debug_mode = PR_FALSE;

static void Help(void)
{
    fprintf(stderr, "semaerr test program usage:\n");
    fprintf(stderr, "\t-d           debug mode         (FALSE)\n");
    fprintf(stderr, "\t-h           this message\n");
}  /* Help */

int main(int argc, char **argv)
{
    PLOptStatus os;
    PLOptState *opt = PL_CreateOptState(argc, argv, "dh");
    PRSem *sem;
    char *child_argv[32];
    char **child_arg;
    PRProcess *proc;
    PRInt32 exit_code;

    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt))) {
        if (PL_OPT_BAD == os) {
            continue;
        }
        switch (opt->option) {
            case 'd':  /* debug mode */
                debug_mode = PR_TRUE;
                break;
            case 'h':
            default:
                Help();
                return 2;
        }
    }
    PL_DestroyOptState(opt);

    /*
     * Open a nonexistent semaphore without the PR_SEM_CREATE
     * flag should fail with PR_FILE_NOT_FOUND_ERROR.
     */
    (void) PR_DeleteSemaphore(NO_SUCH_SEM_NAME);
    sem = PR_OpenSemaphore(NO_SUCH_SEM_NAME, 0, 0, 0);
    if (NULL != sem) {
        fprintf(stderr, "Opening nonexistent semaphore %s "
                "without the PR_SEM_CREATE flag should fail "
                "but succeeded\n", NO_SUCH_SEM_NAME);
        exit(1);
    }
    if (PR_GetError() != PR_FILE_NOT_FOUND_ERROR) {
        fprintf(stderr, "Expected error is %d but got (%d, %d)\n",
                PR_FILE_NOT_FOUND_ERROR, PR_GetError(), PR_GetOSError());
        exit(1);
    }

    /*
     * Create a semaphore and let the another process
     * try PR_SEM_CREATE and PR_SEM_CREATE|PR_SEM_EXCL.
     */
    if (PR_DeleteSemaphore(SEM_NAME1) == PR_SUCCESS) {
        fprintf(stderr, "warning: deleted semaphore %s from previous "
                "run of the test\n", SEM_NAME1);
    }
    sem = PR_OpenSemaphore(SEM_NAME1, PR_SEM_CREATE, SEM_MODE, 0);
    if (sem == NULL) {
        fprintf(stderr, "PR_OpenSemaphore failed (%d, %d)\n",
                PR_GetError(), PR_GetOSError());
        exit(1);
    }
    child_arg = child_argv;
    *child_arg++ = EXE_NAME;
    if (debug_mode) {
        *child_arg++ = "-d";
    }
    *child_arg = NULL;
    proc = PR_CreateProcess(child_argv[0], child_argv, NULL, NULL);
    if (proc == NULL) {
        fprintf(stderr, "PR_CreateProcess failed\n");
        exit(1);
    }
    if (PR_WaitProcess(proc, &exit_code) == PR_FAILURE) {
        fprintf(stderr, "PR_WaitProcess failed\n");
        exit(1);
    }
    if (exit_code != 0) {
        fprintf(stderr, "process semaerr1 failed\n");
        exit(1);
    }
    if (PR_CloseSemaphore(sem) == PR_FAILURE) {
        fprintf(stderr, "PR_CloseSemaphore failed\n");
        exit(1);
    }
    if (PR_DeleteSemaphore(SEM_NAME1) == PR_FAILURE) {
        fprintf(stderr, "PR_DeleteSemaphore failed\n");
        exit(1);
    }

    printf("PASS\n");
    return 0;
}
