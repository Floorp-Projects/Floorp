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
 * test_mutex3.c
 *
 * Tests multi-threaded functionality of Mutex
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static PKIX_PL_Mutex *mutex;
void *plContext = NULL;

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

int main(int argc, char *argv[]) {
        PRThread *thread, *thread2;
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
