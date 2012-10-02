/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_mutex3.c
 *
 * Tests multi-threaded functionality of Mutex
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static PKIX_PL_Mutex *mutex;
static void *plContext = NULL;

static void t1(/* ARGSUSED */ void* arg) {
        PKIX_Error *errorResult;

        (void) printf("t1 acquiring lock...\n");
        errorResult = PKIX_PL_Mutex_Lock(mutex, plContext);
        if (errorResult) testError("PKIX_PL_Mutex_Lock failed");

        (void) printf("t1 sleeplng for 3 seconds\n");
        PR_Sleep(PR_SecondsToInterval(3));
        (void) printf("t1 releasing lock...\n");

        errorResult = PKIX_PL_Mutex_Unlock(mutex, plContext);
        if (errorResult) testError("PKIX_PL_Mutex_Unlock failed");

        (void) printf("t1 exiting...\n");
}

static void t2(/* ARGSUSED */ void* arg) {
        PKIX_Error *errorResult;

        (void) printf("t2 acquiring lock...\n");
        errorResult = PKIX_PL_Mutex_Lock(mutex, plContext);
        if (errorResult) testError("PKIX_PL_Mutex_Lock failed");

        (void) printf("t2 releasing lock...\n");
        errorResult = PKIX_PL_Mutex_Unlock(mutex, plContext);
        if (errorResult) testError("PKIX_PL_Mutex_Unlock failed");

        (void) printf("t2 exiting...\n");
}

int test_mutex3(int argc, char *argv[]) {
        PRThread *thread, *thread2;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;

        PKIX_TEST_STD_VARS();

        startTests("Mutex and Threads");

        PKIX_TEST_EXPECT_NO_ERROR(
            PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

        subTest("Mutex Creation");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Mutex_Create(&mutex, plContext));

        subTest("Starting thread");
        thread = PR_CreateThread(PR_USER_THREAD,
                                t1,
                                NULL,
                                PR_PRIORITY_NORMAL,
                                PR_LOCAL_THREAD,
                                PR_JOINABLE_THREAD,
                                0);

        thread2 = PR_CreateThread(PR_USER_THREAD,
                                t2,
                                NULL,
                                PR_PRIORITY_NORMAL,
                                PR_LOCAL_THREAD,
                                PR_JOINABLE_THREAD,
                                0);

        PR_JoinThread(thread2);
        PR_JoinThread(thread);

cleanup:

        PKIX_TEST_DECREF_AC(mutex);

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("Mutex and Threads");

        return (0);
}
