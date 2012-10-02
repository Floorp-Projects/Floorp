/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_monitorlock.c
 *
 * Tests basic MonitorLock object functionality. No multi-threading.
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;

static
void createMonitorLockes(
        PKIX_PL_MonitorLock **monitorLock,
        PKIX_PL_MonitorLock **monitorLock2,
        PKIX_PL_MonitorLock **monitorLock3)
{
        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_MonitorLock_Create
                (monitorLock, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_MonitorLock_Create
                (monitorLock2, plContext));

        *monitorLock3 = *monitorLock;
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_IncRef
                ((PKIX_PL_Object*)*monitorLock3, plContext));

cleanup:
        PKIX_TEST_RETURN();
}

static
void testLock(PKIX_PL_MonitorLock *monitorLock)
{
        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_MonitorLock_Enter
                (monitorLock, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_MonitorLock_Enter
                (monitorLock, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_MonitorLock_Exit
                (monitorLock, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_MonitorLock_Exit
                (monitorLock, plContext));

cleanup:
        PKIX_TEST_RETURN();
}

static
void testDestroy(
        PKIX_PL_MonitorLock *monitorLock,
        PKIX_PL_MonitorLock *monitorLock2,
        PKIX_PL_MonitorLock *monitorLock3)
{
        PKIX_TEST_STD_VARS();

        PKIX_TEST_DECREF_BC(monitorLock);
        PKIX_TEST_DECREF_BC(monitorLock2);
        PKIX_TEST_DECREF_BC(monitorLock3);

cleanup:
        PKIX_TEST_RETURN();
}

int test_monitorlock(int argc, char *argv[]) {

        PKIX_PL_MonitorLock *monitorLock, *monitorLock2, *monitorLock3;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;

        PKIX_TEST_STD_VARS();

        startTests("MonitorLocks");

        PKIX_TEST_EXPECT_NO_ERROR(
            PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

        subTest("PKIX_PL_MonitorLock_Create");
        createMonitorLockes(&monitorLock, &monitorLock2, &monitorLock3);

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (monitorLock,
                monitorLock3,
                monitorLock2,
                NULL,
                MonitorLock,
                PKIX_FALSE);

        subTest("PKIX_PL_MonitorLock_Lock/Unlock");
        testLock(monitorLock);

        subTest("PKIX_PL_MonitorLock_Destroy");
        testDestroy(monitorLock, monitorLock2, monitorLock3);

cleanup:

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("MonitorLockes");

        return (0);

}
