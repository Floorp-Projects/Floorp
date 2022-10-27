/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_bigint.c
 *
 * Tests BigInt Types
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;

static void
createBigInt(
    PKIX_PL_BigInt **bigInts,
    char *bigIntAscii,
    PKIX_Boolean errorHandling)
{
    PKIX_PL_String *bigIntString = NULL;

    PKIX_TEST_STD_VARS();

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create(PKIX_ESCASCII,
                                                    bigIntAscii,
                                                    PL_strlen(bigIntAscii),
                                                    &bigIntString,
                                                    plContext));

    if (errorHandling) {
        PKIX_TEST_EXPECT_ERROR(PKIX_PL_BigInt_Create(bigIntString,
                                                     bigInts,
                                                     plContext));
    } else {
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_BigInt_Create(bigIntString,
                                                        bigInts,
                                                        plContext));
    }

cleanup:

    PKIX_TEST_DECREF_AC(bigIntString);

    PKIX_TEST_RETURN();
}

static void
testToString(
    PKIX_PL_BigInt *bigInt,
    char *expAscii)
{
    PKIX_PL_String *bigIntString = NULL;
    char *temp = NULL;

    PKIX_TEST_STD_VARS();

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString((PKIX_PL_Object *)bigInt,
                                                      &bigIntString, plContext));

    temp = PKIX_String2ASCII(bigIntString, plContext);
    if (temp == plContext) {
        testError("PKIX_String2Ascii failed");
        goto cleanup;
    }

    if (PL_strcmp(temp, expAscii) != 0) {
        (void)printf("\tBigInt ToString: %s %s\n", temp, expAscii);
        testError("Output string does not match source");
    }

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));

cleanup:

    PKIX_TEST_DECREF_AC(bigIntString);

    PKIX_TEST_RETURN();
}

static void
testCompare(
    PKIX_PL_BigInt *firstBigInt,
    PKIX_PL_BigInt *secondBigInt,
    PKIX_Int32 *cmpResult)
{
    PKIX_TEST_STD_VARS();

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Compare((PKIX_PL_Object *)firstBigInt,
                                                     (PKIX_PL_Object *)secondBigInt,
                                                     cmpResult, plContext));
cleanup:

    PKIX_TEST_RETURN();
}

static void
testDestroy(
    PKIX_PL_BigInt *bigInt)
{
    PKIX_TEST_STD_VARS();

    PKIX_TEST_DECREF_BC(bigInt);

cleanup:

    PKIX_TEST_RETURN();
}

int
test_bigint(int argc, char *argv[])
{

    PKIX_UInt32 size = 4, badSize = 3, i = 0;
    PKIX_PL_BigInt *testBigInt[4] = { NULL };
    PKIX_Int32 cmpResult;
    PKIX_UInt32 actualMinorVersion;
    PKIX_UInt32 j = 0;

    char *bigIntValue[4] = {
        "03",
        "ff",
        "1010101010101010101010101010101010101010",
        "1010101010101010101010101010101010101010",
    };

    char *badValue[3] = { "00ff", "fff", "-ff" };

    PKIX_TEST_STD_VARS();

    startTests("BigInts");

    PKIX_TEST_EXPECT_NO_ERROR(
        PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

    for (i = 0; i < badSize; i++) {
        subTest("PKIX_PL_BigInt_Create <error_handling>");
        createBigInt(&testBigInt[i], badValue[i], PKIX_TRUE);
    }

    for (i = 0; i < size; i++) {
        subTest("PKIX_PL_BigInt_Create");
        createBigInt(&testBigInt[i], bigIntValue[i], PKIX_FALSE);
    }

    PKIX_TEST_EQ_HASH_TOSTR_DUP(testBigInt[2],
                                testBigInt[3],
                                testBigInt[1],
                                bigIntValue[2],
                                BigInt,
                                PKIX_TRUE);

    for (i = 0; i < size; i++) {
        subTest("PKIX_PL_BigInt_ToString");
        testToString(testBigInt[i], bigIntValue[i]);
    }

    subTest("PKIX_PL_BigInt_Compare <gt>");
    testCompare(testBigInt[2], testBigInt[1], &cmpResult);
    if (cmpResult <= 0) {
        testError("Invalid Result from String Compare");
    }

    subTest("PKIX_PL_BigInt_Compare <lt>");
    testCompare(testBigInt[1], testBigInt[2], &cmpResult);
    if (cmpResult >= 0) {
        testError("Invalid Result from String Compare");
    }

    subTest("PKIX_PL_BigInt_Compare <eq>");
    testCompare(testBigInt[2], testBigInt[3], &cmpResult);
    if (cmpResult != 0) {
        testError("Invalid Result from String Compare");
    }

    for (i = 0; i < size; i++) {
        subTest("PKIX_PL_BigInt_Destroy");
        testDestroy(testBigInt[i]);
    }

cleanup:

    PKIX_Shutdown(plContext);

    endTests("BigInt");

    return (0);
}
