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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Sun Microsystems
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
void *plContext = NULL;

static void consumer(/* ARGSUSED */ void* arg) {
        PRStatus status = PR_SUCCESS;
        PKIX_Error *errorResult;
        int i = 0;
        for (i = 0; i < 5; i++) {
                (void) PKIX_PL_Mutex_Lock(mutex, plContext);
                while (((box1 == 0) ||
                        (box2 == 0) ||
                        (box3 == 0)) &&
                        (status == PR_SUCCESS))
                        status = PR_WaitCondVar(cv, PR_INTERVAL_NO_TIMEOUT);

                (void) printf("\tConsumer got Box1 = %d ", box1);
                box1 = 0;
                (void) printf("Box2 = %d ", box2);
                box2 = 0;
                (void) printf("Box3 = %d\n", box3);
                box3 = 0;

                status = PR_NotifyAllCondVar(cv);
                if (status == PR_FAILURE)
                        (void) printf
                                ("Consumer error while notifying condvar\n");
                errorResult = PKIX_PL_Mutex_Unlock(mutex, plContext);
                if (errorResult) testError("PKIX_PL_Mutex_Unlock failed");
        }
        (void) printf("Consumer exiting...\n");
}

static void producer(void* arg) {
        PRStatus status = PR_SUCCESS;
        int value = *(int*)arg;
        int i = 0;
        int *box;
        PKIX_Error *errorResult;
        if (value == 10) box = &box1;
        else if (value == 20) box = &box2;
        else if (value == 30) box = &box3;

        for (i = 0; i < 5; i++) {
                (void) PKIX_PL_Mutex_Lock(mutex, plContext);
                while ((*box != 0) && (status == PR_SUCCESS))
                        status = PR_WaitCondVar(cv, PR_INTERVAL_NO_TIMEOUT);

                *box = i+1;
                (void) printf
                        ("\tProducer %d put value: %d\n", value, *box);

                status = PR_NotifyAllCondVar(cv);
                if (status == PR_FAILURE)
                        (void) printf
                                ("Producer %d error while notifying condvar\n",
                                value);
                errorResult = PKIX_PL_Mutex_Unlock(mutex, plContext);
                if (errorResult) testError("PKIX_PL_Mutex_Unlock failed");
        }
}

int main(int argc, char *argv[]) {

        PRThread *consThread, *prodThread, *prodThread2, *prodThread3;
        int x = 10, y = 20, z = 30;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;
        PKIX_Boolean useArenas = PKIX_FALSE;

        PKIX_TEST_STD_VARS();

        startTests("Mutex and Threads");

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                                    (PKIX_TRUE, /* nssInitNeeded */
                                    useArenas,
                                    PKIX_MAJOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    &actualMinorVersion,
                                    &plContext));

        (void) printf("Attempting to create new mutex...\n");
        subTest("Mutex Creation");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Mutex_Create(&mutex, plContext));

        cv = PR_NewCondVar(*(PRLock **) mutex);

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

        (void) PR_DestroyCondVar(cv);
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
