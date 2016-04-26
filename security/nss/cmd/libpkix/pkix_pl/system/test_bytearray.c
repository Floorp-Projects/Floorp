/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_bytearray.c
 *
 * Tests ByteArray types.
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;

static void
createByteArray(
        PKIX_PL_ByteArray **byteArray,
        char *bytes,
        PKIX_UInt32 length)
{
        PKIX_TEST_STD_VARS();
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_ByteArray_Create
                                    ((void*)bytes,
                                    length,
                                    byteArray,
                                    plContext));
cleanup:
        PKIX_TEST_RETURN();
}


static void
testZeroLength(void)
{
        PKIX_PL_ByteArray *byteArray = NULL;
        void *array = NULL;
        PKIX_UInt32 length = 2;

        PKIX_TEST_STD_VARS();

        createByteArray(&byteArray, NULL, 0);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_ByteArray_GetLength
                                    (byteArray, &length, plContext));
        if (length != 0){
                testError("Length should be zero");
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_ByteArray_GetPointer
                                    (byteArray, &array, plContext));
        if (array){
                testError("Array should be NULL");
        }

        testToStringHelper((PKIX_PL_Object *)byteArray, "[]", plContext);

cleanup:

        PKIX_TEST_DECREF_AC(byteArray);

        PKIX_TEST_RETURN();
}


static void
testToString(
        PKIX_PL_ByteArray *byteArray,
        char *expAscii)
{
        PKIX_PL_String *string = NULL;
        char *temp = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString
                                    ((PKIX_PL_Object*)byteArray,
                                    &string, plContext));

        temp = PKIX_String2ASCII(string, plContext);
        if (temp == NULL){
                testError("PKIX_String2Ascii failed");
                goto cleanup;
        }

        if (PL_strcmp(temp, expAscii) != 0) {
                (void) printf("\tByteArray ToString: %s %s\n", temp, expAscii);
                testError("Output string does not match source");
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));

cleanup:

        PKIX_TEST_DECREF_AC(string);

        PKIX_TEST_RETURN();
}

static void
testGetLength(
        PKIX_PL_ByteArray *byteArray,
        PKIX_UInt32 expLength)
{
        PKIX_UInt32 arrayLength;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_ByteArray_GetLength
                                    (byteArray, &arrayLength, plContext));

        if (arrayLength != expLength){
                (void) printf("\tByteArray GetLength: %d %d\n",
                            arrayLength, expLength);
                testError("Incorrect Array Length returned");
        }


cleanup:
        PKIX_TEST_RETURN();
}

static void
testGetPointer(
        PKIX_PL_ByteArray *byteArray,
        char *expBytes,
        PKIX_UInt32 arrayLength)
{
        char *temp = NULL;
        PKIX_UInt32 j;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_ByteArray_GetPointer
                                    (byteArray, (void **)&temp, plContext));

        for (j = 0; j < arrayLength; j++) {
                if (temp[j] != expBytes[j]){
                        testError("Incorrect Byte Array Contents");
                }
        }

cleanup:

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        PKIX_TEST_RETURN();
}

void
testDestroy(
        PKIX_PL_ByteArray *byteArray)
{
        PKIX_TEST_STD_VARS();

        PKIX_TEST_DECREF_BC(byteArray);

cleanup:

        PKIX_TEST_RETURN();
}

int test_bytearray(int argc, char *argv[]) {

        PKIX_PL_ByteArray *testByteArray[4];

        PKIX_UInt32 i, size = 4;
        PKIX_UInt32 lengths[4] = {5, 6, 1, 5};
        char dArray0[5] = {1, 2, 3, 4, 5};
        unsigned char dArray1[6] = {127, 128, 129, 254, 255, 0};
        char dArray2[1] = {100};
        char dArray3[5] = {1, 2, 3, 4, 5};

        char *expected[4] = {
                "[001, 002, 003, 004, 005]",
                "[127, 128, 129, 254, 255, 000]",
                "[100]",
                "[001, 002, 003, 004, 005]"
        };

        char *dummyArray[4];
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;

        PKIX_TEST_STD_VARS();

        dummyArray[0] = dArray0;
        dummyArray[1] = (char*)dArray1;
        dummyArray[2] = dArray2;
        dummyArray[3] = dArray3;

        startTests("ByteArrays");

        PKIX_TEST_EXPECT_NO_ERROR(
            PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

        subTest ("PKIX_PL_ByteArray_Create <zero length>");
        testZeroLength();

        for (i = 0; i < size; i++) {
                subTest("PKIX_PL_ByteArray_Create");
                createByteArray(&testByteArray[i], dummyArray[i], lengths[i]);
        }

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (testByteArray[0],
                testByteArray[3],
                testByteArray[1],
                "[001, 002, 003, 004, 005]",
                ByteArray,
                PKIX_TRUE);

        for (i = 0; i < size; i++) {
                subTest("PKIX_PL_ByteArray_ToString");
                testToString(testByteArray[i], expected[i]);
        }

        for (i = 0; i < size; i++) {
                subTest("PKIX_PL_ByteArray_GetLength");
                testGetLength(testByteArray[i], lengths[i]);
        }

        for (i = 0; i < size; i++) {
                subTest("PKIX_PL_ByteArray_GetPointer");
                testGetPointer(testByteArray[i], dummyArray[i], lengths[i]);
        }


        for (i = 0; i < size; i++) {
                subTest("PKIX_PL_ByteArray_Destroy");
                testDestroy(testByteArray[i]);
        }

cleanup:

        PKIX_Shutdown(plContext);

        endTests("ByteArray");

        return (0);

}
