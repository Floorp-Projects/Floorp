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

#include "nspr.h"
#include "plgetopt.h"

#include <stdio.h>

#define SEM_NAME1 "/tmp/foo.sem"
#define SEM_NAME2 "/tmp/bar.sem"
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
