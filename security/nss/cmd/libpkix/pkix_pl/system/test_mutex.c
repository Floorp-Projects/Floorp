/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_mutex.c
 *
 * Tests basic mutex object functionality. No multi-threading.
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;

static
void createMutexes(
        PKIX_PL_Mutex **mutex,
        PKIX_PL_Mutex **mutex2,
        PKIX_PL_Mutex **mutex3)
{
        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Mutex_Create(mutex, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Mutex_Create(mutex2, plContext));

        *mutex3 = *mutex;
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Object_IncRef((PKIX_PL_Object*)*mutex3, plContext));

cleanup:
        PKIX_TEST_RETURN();
}

static
void testLock(PKIX_PL_Mutex *mutex)
{
        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Mutex_Lock(mutex, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Mutex_Unlock(mutex, plContext));

cleanup:
        PKIX_TEST_RETURN();
}

static
void testDestroy(
        PKIX_PL_Mutex *mutex,
        PKIX_PL_Mutex *mutex2,
        PKIX_PL_Mutex *mutex3)
{
        PKIX_TEST_STD_VARS();

        PKIX_TEST_DECREF_BC(mutex);
        PKIX_TEST_DECREF_BC(mutex2);
        PKIX_TEST_DECREF_BC(mutex3);

cleanup:
        PKIX_TEST_RETURN();
}

int test_mutex(int argc, char *argv[]) {

        PKIX_PL_Mutex *mutex, *mutex2, *mutex3;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;

        PKIX_TEST_STD_VARS();

        startTests("Mutexes");

        PKIX_TEST_EXPECT_NO_ERROR(
            PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

        subTest("PKIX_PL_Mutex_Create");
        createMutexes(&mutex, &mutex2, &mutex3);

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (mutex,
                mutex3,
                mutex2,
                NULL,
                Mutex,
                PKIX_FALSE);

        subTest("PKIX_PL_Mutex_Lock/Unlock");
        testLock(mutex);

        subTest("PKIX_PL_Mutex_Destroy");
        testDestroy(mutex, mutex2, mutex3);

cleanup:

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("Mutexes");

        return (0);

}
