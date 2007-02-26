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
 * test_rwlock.c
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static PKIX_PL_RWLock *rwlock = NULL, *rwlock2 = NULL, *rwlock3 = NULL;
static PRThread *thread = NULL, *thread2 = NULL, *thread3 = NULL;
void *plContext = NULL;

static void reader(void) {
        PKIX_Error *errorResult;

        errorResult = PKIX_PL_AcquireReaderLock(rwlock, NULL);
        if (errorResult) testError("PKIX_PL_AcquireReaderLock failed");

        (void) printf("\t[Thread #1 Read Lock #1.]\n");
        (void) printf("\t[Thread #1 Sleeplng for 1 seconds.]\n");
        PR_Sleep(PR_SecondsToInterval(1));
        PKIX_PL_ReleaseReaderLock(rwlock, NULL);
        if (errorResult) testError("PKIX_PL_ReleaseReaderLock failed");
        (void) printf("\t[Thread #1 Read UNLock #1.]\n");
}


static void writer(void) {
        PKIX_Error *errorResult;
        /* This thread should stick here until lock 1 is released */
        PKIX_PL_AcquireWriterLock(rwlock, NULL);
        if (errorResult) testError("PKIX_PL_AcquireWriterLock failed");

        (void) printf("\t[Thread #2 Write Lock #1.]\n");

        PKIX_PL_AcquireWriterLock(rwlock2, NULL);
        if (errorResult) testError("PKIX_PL_AcquireWriterLock failed");
        (void) printf("\t[Thread #2 Write Lock #2.]\n");

        (void) printf("\t[Thread #2 Sleeplng for 1 seconds.]\n");
        PR_Sleep(PR_SecondsToInterval(1));

        PKIX_PL_ReleaseWriterLock(rwlock2, NULL);
        if (errorResult) testError("PKIX_PL_ReleaseWriterLock failed");
        (void) printf("\t[Thread #2 Write UNLock #2.]\n");

        (void) printf("\t[Thread #2 Sleeplng for 1 seconds.]\n");
        PR_Sleep(PR_SecondsToInterval(1));

        PKIX_PL_ReleaseWriterLock(rwlock, NULL);
        if (errorResult) testError("PKIX_PL_ReleaseWriterLock failed");
        (void) printf("\t[Thread #2 Write UNLock #1.]\n");

        PR_JoinThread(thread3);
}

static void reader2(void) {
        PKIX_Error *errorResult;
        /* Reader 2 should yield here until the writer is done */

        PKIX_PL_AcquireReaderLock(rwlock2, NULL);
        if (errorResult) testError("PKIX_PL_AcquireReaderLock failed");

        (void) printf("\t[Thread #3 Read Lock #2.]\n");

        PKIX_PL_AcquireReaderLock(rwlock3, NULL);
        if (errorResult) testError("PKIX_PL_AcquireReaderLock failed");
        (void) printf("\t[Thread #3 Read Lock #3.]\n");

        (void) printf("\t[Thread #3 Sleeplng for 1 seconds.]\n");
        PR_Sleep(PR_SecondsToInterval(1));

        PKIX_PL_ReleaseReaderLock(rwlock3, NULL);
        if (errorResult) testError("PKIX_PL_ReleaseReaderLock failed");
        (void) printf("\t[Thread #3 Read UNLock #3.]\n");

        (void) printf("\t[Thread #3 Sleeplng for 1 seconds.]\n");
        PR_Sleep(PR_SecondsToInterval(1));

        PKIX_PL_ReleaseReaderLock(rwlock2, NULL);
        if (errorResult) testError("PKIX_PL_ReleaseReaderLock failed");
        (void) printf("\t[Thread #3 Read UNLock #2.]\n");
}



int main() {
        PKIX_PL_String* outputString = NULL;
        PKIX_UInt32 j = 0;
        PKIX_Boolean useArenas = PKIX_FALSE;
        PKIX_Boolean bool;
        PKIX_UInt32 actualMinorVersion;

        PKIX_TEST_STD_VARS();
        startTests("RWLocks");

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                                    (PKIX_TRUE, /* nssInitNeeded */
                                    useArenas,
                                    PKIX_MAJOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    &actualMinorVersion,
                                    &plContext));

        (void) printf("Attempting to create new rwlock...\n");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_RWLock_Create(&rwlock, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_RWLock_Create(&rwlock2, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_RWLock_Create(&rwlock3, plContext));

        /* Test toString functionality */
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString
                ((PKIX_PL_Object*)rwlock, &outputString, plContext));

        (void) printf("Testing RWLock toString: %s\n",
                PKIX_String2ASCII(outputString));

        PKIX_TEST_DECREF_BC(outputString);

        /* Call Equals on two different objects */
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Equals
                ((PKIX_PL_Object*)rwlock,
                (PKIX_PL_Object*)rwlock2,
                &bool,
                plContext));

        (void) printf("Testing RWLock Equals: %d (should be 0)\n", bool);

        if (bool != 0)
                testError("Error in RWLock_Equals");

        /* Call Equals on two equal objects */
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Equals((PKIX_PL_Object*)rwlock,
                            (PKIX_PL_Object*)rwlock, &bool, plContext));

        (void) printf("Testing RWLock Equals: %d (should be 1)\n", bool);
        if (bool != 1)
                testError("Error in RWLock_Equals");

        subTest("Multi-Thread Read/Write Lock Testing");

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_AcquireReaderLock(rwlock, plContext));
        (void) printf("\t[Main Thread Read Lock #1.]\n");

        thread = PR_CreateThread(PR_USER_THREAD,
                            reader,
                            NULL,
                            PR_PRIORITY_NORMAL,
                            PR_LOCAL_THREAD,
                            PR_JOINABLE_THREAD,
                            0);

        thread2 = PR_CreateThread(PR_USER_THREAD,
                            writer,
                            NULL,
                            PR_PRIORITY_NORMAL,
                            PR_LOCAL_THREAD,
                            PR_JOINABLE_THREAD,
                            0);

        thread3 = PR_CreateThread(PR_USER_THREAD,
                            reader2,
                            NULL,
                            PR_PRIORITY_NORMAL,
                            PR_LOCAL_THREAD,
                            PR_JOINABLE_THREAD,
                            0);

        PR_JoinThread(thread);
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_ReleaseReaderLock
                (rwlock, plContext));
        (void) printf("\t[Main Thread Read Unlock #1.]\n");

        PR_JoinThread(thread2);



cleanup:

        /* Test destructor */
        subTest("Testing destructor...");
        PKIX_TEST_DECREF_AC(rwlock);
        PKIX_TEST_DECREF_AC(rwlock2);
        PKIX_TEST_DECREF_AC(rwlock3);

        pkixTestTempResult = PKIX_Shutdown(plContext);
        if (pkixTestTempResult) pkixTestErrorResult = pkixTestTempResult;

        PKIX_TEST_RETURN();

        endTests("RWLocks");

        return (0);
}
