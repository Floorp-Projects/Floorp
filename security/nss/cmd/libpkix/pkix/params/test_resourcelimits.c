/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_resourcelimits.c
 *
 * Test ResourceLimits Type
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;

static void
testDestroy(void *goodObject, void *equalObject, void *diffObject)
{
    PKIX_TEST_STD_VARS();

    subTest("PKIX_ResourceLimits_Destroy");

    PKIX_TEST_DECREF_BC(goodObject);
    PKIX_TEST_DECREF_BC(equalObject);
    PKIX_TEST_DECREF_BC(diffObject);

cleanup:

    PKIX_TEST_RETURN();
}

int
test_resourcelimits(int argc, char *argv[])
{

    PKIX_ResourceLimits *goodObject = NULL;
    PKIX_ResourceLimits *equalObject = NULL;
    PKIX_ResourceLimits *diffObject = NULL;
    PKIX_UInt32 maxTime = 0;
    PKIX_UInt32 maxFanout = 0;
    PKIX_UInt32 maxDepth = 0;
    PKIX_UInt32 actualMinorVersion;
    PKIX_UInt32 j = 0;
    char *expectedAscii =
        "[\n"
        "\tMaxTime:           		10\n"
        "\tMaxFanout:         		5\n"
        "\tMaxDepth:         		5\n"
        "]\n";

    PKIX_TEST_STD_VARS();

    startTests("ResourceLimits");

    PKIX_TEST_EXPECT_NO_ERROR(
        PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

    subTest("PKIX_ResourceLimits_Create");
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_Create(&goodObject, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_Create(&diffObject, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_Create(&equalObject, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxTime(goodObject, 10, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_GetMaxTime(goodObject, &maxTime, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxTime(equalObject, maxTime, plContext));
    maxTime++;
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxTime(diffObject, maxTime, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxFanout(goodObject, 5, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_GetMaxFanout(goodObject, &maxFanout, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxFanout(equalObject, maxFanout, plContext));
    maxFanout++;
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxFanout(diffObject, maxFanout, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxDepth(goodObject, 5, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_GetMaxDepth(goodObject, &maxDepth, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxDepth(equalObject, maxDepth, plContext));
    maxDepth++;
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxDepth(diffObject, maxDepth, plContext));

    PKIX_TEST_EQ_HASH_TOSTR_DUP(goodObject,
                                equalObject,
                                diffObject,
                                expectedAscii,
                                ResourceLimits,
                                PKIX_FALSE);

    testDestroy(goodObject, equalObject, diffObject);

cleanup:

    PKIX_Shutdown(plContext);

    PKIX_TEST_RETURN();

    endTests("ResourceLimits");

    return (0);
}
