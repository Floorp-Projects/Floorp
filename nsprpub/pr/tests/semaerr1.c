/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nspr.h"
#include "plgetopt.h"

#include <stdio.h>

#ifdef SYMBIAN
#define SEM_NAME1 "c:\\data\\foo.sem"
#define SEM_NAME2 "c:\\data\\bar.sem"
#else
#define SEM_NAME1 "/tmp/foo.sem"
#define SEM_NAME2 "/tmp/bar.sem"
#endif
#define SEM_MODE  0666

static PRBool debug_mode = PR_FALSE;

static void Help(void)
{
    fprintf(stderr, "semaerr1 test program usage:\n");
    fprintf(stderr, "\t-d           debug mode         (FALSE)\n");
    fprintf(stderr, "\t-h           this message\n");
}  /* Help */

int main(int argc, char **argv)
{
    PLOptStatus os;
    PLOptState *opt = PL_CreateOptState(argc, argv, "dh");
    PRSem *sem;

    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt))) {
        if (PL_OPT_BAD == os) continue;
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
     * PR_SEM_CREATE|PR_SEM_EXCL should be able to
     * create a nonexistent semaphore.
     */
    (void) PR_DeleteSemaphore(SEM_NAME2);
    sem = PR_OpenSemaphore(SEM_NAME2, PR_SEM_CREATE|PR_SEM_EXCL, SEM_MODE, 0);
    if (sem == NULL) {
        fprintf(stderr, "PR_OpenSemaphore failed\n");
        exit(1);
    }
    if (PR_CloseSemaphore(sem) == PR_FAILURE) {
        fprintf(stderr, "PR_CloseSemaphore failed\n");
        exit(1);
    }
    if (PR_DeleteSemaphore(SEM_NAME2) == PR_FAILURE) {
        fprintf(stderr, "PR_DeleteSemaphore failed\n");
        exit(1);
    }
    
    /*
     * Opening an existing semaphore with PR_SEM_CREATE|PR_SEM_EXCL.
     * should fail with PR_FILE_EXISTS_ERROR.
     */
    sem = PR_OpenSemaphore(SEM_NAME1, PR_SEM_CREATE|PR_SEM_EXCL, SEM_MODE, 0);
    if (sem != NULL) {
        fprintf(stderr, "PR_OpenSemaphore should fail but succeeded\n");
        exit(1);
    }
    if (PR_GetError() != PR_FILE_EXISTS_ERROR) {
        fprintf(stderr, "Expect %d but got %d\n", PR_FILE_EXISTS_ERROR,
                PR_GetError());
        exit(1);
    }

    /*
     * Try again, with just PR_SEM_CREATE.  This should succeed.
     */
    sem = PR_OpenSemaphore(SEM_NAME1, PR_SEM_CREATE, SEM_MODE, 0);
    if (sem == NULL) {
        fprintf(stderr, "PR_OpenSemaphore failed (%d, %d)\n",
                PR_GetError(), PR_GetOSError());
        exit(1);
    }
    if (PR_CloseSemaphore(sem) == PR_FAILURE) {
        fprintf(stderr, "PR_CloseSemaphore failed\n");
        exit(1);
    }

    sem = PR_OpenSemaphore(SEM_NAME2, PR_SEM_CREATE|PR_SEM_EXCL, SEM_MODE, 0);
    if (sem == NULL) {
        fprintf(stderr, "PR_OpenSemaphore failed (%d, %d)\n",
                PR_GetError(), PR_GetOSError());
        exit(1);
    }
    if (PR_CloseSemaphore(sem) == PR_FAILURE) {
        fprintf(stderr, "PR_CloseSemaphore failed\n");
        exit(1);
    }

    printf("PASS\n");
    return 0;
}
