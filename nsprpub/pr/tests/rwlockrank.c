/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * RWLock rank tests
 */

#include "nspr.h"
#include "plgetopt.h"

static int _debug_on;
static PRRWLock *rwlock0;
static PRRWLock *rwlock1;
static PRRWLock *rwlock2;

static void rwtest(void *args)
{
    PR_RWLock_Rlock(rwlock1);
    PR_RWLock_Unlock(rwlock1);

    PR_RWLock_Rlock(rwlock1);
    PR_RWLock_Unlock(rwlock1);

    // Test correct lock rank.
    PR_RWLock_Rlock(rwlock1);
    PR_RWLock_Rlock(rwlock2);
    PR_RWLock_Unlock(rwlock2);
    PR_RWLock_Unlock(rwlock1);

    PR_RWLock_Rlock(rwlock1);
    PR_RWLock_Rlock(rwlock2);
    PR_RWLock_Unlock(rwlock1);
    PR_RWLock_Unlock(rwlock2);

    PR_RWLock_Rlock(rwlock1);
    PR_RWLock_Rlock(rwlock0);
    PR_RWLock_Rlock(rwlock2);
    PR_RWLock_Unlock(rwlock2);
    PR_RWLock_Unlock(rwlock0);
    PR_RWLock_Unlock(rwlock1);

#if 0
    // Test incorrect lock rank.
    PR_RWLock_Rlock(rwlock2);
    PR_RWLock_Rlock(rwlock1);
    PR_RWLock_Unlock(rwlock1);
    PR_RWLock_Unlock(rwlock2);

    PR_RWLock_Rlock(rwlock2);
    PR_RWLock_Rlock(rwlock0);
    PR_RWLock_Rlock(rwlock1);
    PR_RWLock_Unlock(rwlock1);
    PR_RWLock_Unlock(rwlock0);
    PR_RWLock_Unlock(rwlock2);
#endif
}

int main(int argc, char **argv)
{
    PRStatus rc;
    PRThread *thread;
    PLOptStatus os;
    PLOptState *opt = PL_CreateOptState(argc, argv, "d");

    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt))) {
        if (PL_OPT_BAD == os) continue;
        switch (opt->option) {
        case 'd':  /* debug mode */
            _debug_on = 1;
            break;
         default:
            break;
        }
    }
    PL_DestroyOptState(opt);

    rwlock0 = PR_NewRWLock(PR_RWLOCK_RANK_NONE, "Lock 0");
    if (rwlock0 == NULL) {
        fprintf(stderr, "PR_NewRWLock failed - error %d\n",
                (int)PR_GetError());
        return 1;
    }
    rwlock1 = PR_NewRWLock(1, "Lock 1");
    if (rwlock1 == NULL) {
        fprintf(stderr, "PR_NewRWLock failed - error %d\n",
                (int)PR_GetError());
        return 1;
    }
    rwlock2 = PR_NewRWLock(2, "Lock 2");
    if (rwlock2 == NULL) {
        fprintf(stderr, "PR_NewRWLock failed - error %d\n",
                (int)PR_GetError());
        return 1;
    }

    thread = PR_CreateThread(PR_USER_THREAD, rwtest, NULL, PR_PRIORITY_NORMAL,
                             PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0);
    if (thread == NULL) {
        fprintf(stderr, "PR_CreateThread failed - error %d\n",
                (int)PR_GetError());
        PR_ProcessExit(2);
    }
    if (_debug_on) {
        printf("%s: created thread = %p\n", argv[0], thread);
    }

    rc = PR_JoinThread(thread);
    PR_ASSERT(rc == PR_SUCCESS);

    PR_DestroyRWLock(rwlock0);
    rwlock0 = NULL;
    PR_DestroyRWLock(rwlock1);
    rwlock1 = NULL;
    PR_DestroyRWLock(rwlock2);
    rwlock2 = NULL;

    printf("PASS\n");
    return 0;
}
