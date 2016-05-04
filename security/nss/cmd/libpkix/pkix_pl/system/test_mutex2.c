/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_mutex2.c
 *
 * Tests multi-threaded functionality of Mutex
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static int box1 = 0, box2 = 0, box3 = 0;
static PKIX_PL_Mutex *mutex;
static PRCondVar *cv;
static void *plContext = NULL;

static void
consumer(/* ARGSUSED */ void *arg)
{
    PRStatus status = PR_SUCCESS;
    PKIX_Error *errorResult;
    int i = 0;
    for (i = 0; i < 5; i++) {
        (void)PKIX_PL_Mutex_Lock(mutex, plContext);
        while (((box1 == 0) ||
                (box2 == 0) ||
                (box3 == 0)) &&
               (status == PR_SUCCESS))
            status = PR_WaitCondVar(cv, PR_INTERVAL_NO_TIMEOUT);

        (void)printf("\tConsumer got Box1 = %d ", box1);
        box1 = 0;
        (void)printf("Box2 = %d ", box2);
        box2 = 0;
        (void)printf("Box3 = %d\n", box3);
        box3 = 0;

        status = PR_NotifyAllCondVar(cv);
        if (status == PR_FAILURE)
            (void)printf("Consumer error while notifying condvar\n");
        errorResult = PKIX_PL_Mutex_Unlock(mutex, plContext);
        if (errorResult)
            testError("PKIX_PL_Mutex_Unlock failed");
    }
    (void)printf("Consumer exiting...\n");
}

static void
producer(void *arg)
{
    PRStatus status = PR_SUCCESS;
    int value = *(int *)arg;
    int i = 0;
    int *box;
    PKIX_Error *errorResult;
    if (value == 10)
        box = &box1;
    else if (value == 20)
        box = &box2;
    else if (value == 30)
        box = &box3;

    for (i = 0; i < 5; i++) {
        (void)PKIX_PL_Mutex_Lock(mutex, plContext);
        while ((*box != 0) && (status == PR_SUCCESS))
            status = PR_WaitCondVar(cv, PR_INTERVAL_NO_TIMEOUT);

        *box = i + 1;
        (void)printf("\tProducer %d put value: %d\n", value, *box);

        status = PR_NotifyAllCondVar(cv);
        if (status == PR_FAILURE)
            (void)printf("Producer %d error while notifying condvar\n",
                         value);
        errorResult = PKIX_PL_Mutex_Unlock(mutex, plContext);
        if (errorResult)
            testError("PKIX_PL_Mutex_Unlock failed");
    }
}

int
test_mutex2(int argc, char *argv[])
{

    PRThread *consThread, *prodThread, *prodThread2, *prodThread3;
    int x = 10, y = 20, z = 30;
    PKIX_UInt32 actualMinorVersion;
    PKIX_UInt32 j = 0;

    PKIX_TEST_STD_VARS();

    startTests("Mutex and Threads");

    PKIX_TEST_EXPECT_NO_ERROR(
        PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

    (void)printf("Attempting to create new mutex...\n");
    subTest("Mutex Creation");
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Mutex_Create(&mutex, plContext));

    cv = PR_NewCondVar(*(PRLock **)mutex);

    subTest("Starting consumer thread");
    consThread = PR_CreateThread(PR_USER_THREAD,
                                 consumer,
                                 NULL,
                                 PR_PRIORITY_NORMAL,
                                 PR_LOCAL_THREAD,
                                 PR_JOINABLE_THREAD,
                                 0);

    subTest("Starting producer thread 1");
    prodThread = PR_CreateThread(PR_USER_THREAD,
                                 producer,
                                 &x,
                                 PR_PRIORITY_NORMAL,
                                 PR_LOCAL_THREAD,
                                 PR_JOINABLE_THREAD,
                                 0);

    subTest("Starting producer thread 2");
    prodThread2 = PR_CreateThread(PR_USER_THREAD,
                                  producer,
                                  &y,
                                  PR_PRIORITY_NORMAL,
                                  PR_LOCAL_THREAD,
                                  PR_JOINABLE_THREAD,
                                  0);

    subTest("Starting producer thread 3");
    prodThread3 = PR_CreateThread(PR_USER_THREAD,
                                  producer,
                                  &z,
                                  PR_PRIORITY_NORMAL,
                                  PR_LOCAL_THREAD,
                                  PR_JOINABLE_THREAD,
                                  0);

    PR_JoinThread(consThread);

    (void)PR_DestroyCondVar(cv);
    PKIX_TEST_DECREF_BC(mutex);

    /*
         * Note: we should also be freeing each thread's stack, but we
         * don't have access to the prodThread->stack variable (since
         * it is not exported). As a result, we have 120 bytes of memory
         * leakage.
         */

    PR_Free(prodThread);
    PR_Free(prodThread2);
    PR_Free(prodThread3);

cleanup:

    PKIX_Shutdown(plContext);

    PKIX_TEST_RETURN();

    endTests("Mutex and Threads");

    return (0);
}
