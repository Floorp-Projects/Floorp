/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * stress_test.c
 *
 * Creates and deletes many objects
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;

int
stress_test(int argc, char *argv[])
{

    PKIX_UInt32 i, k, length, hashcode;
    PKIX_UInt32 size = 17576;
    char temp[4];
    PKIX_Boolean result;
    PKIX_PL_String *strings[17576], *tempString;
    PKIX_PL_String *utf16strings[17576];
    PKIX_PL_ByteArray *byteArrays[17576];
    void *dest;
    PKIX_PL_HashTable *ht = NULL;
    PKIX_UInt32 actualMinorVersion;
    PKIX_UInt32 j = 0;

    PKIX_TEST_STD_VARS();

    startTests("Stress Test");

    PKIX_TEST_EXPECT_NO_ERROR(
        PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

    /* ---------------------------- */
    subTest("Create every three letter String");

    for (i = 0; i < 26; i++)
        for (j = 0; j < 26; j++)
            for (k = 0; k < 26; k++) {
                temp[0] = (char)('a' + i);
                temp[1] = (char)('a' + j);
                temp[2] = (char)('a' + k);
                temp[3] = 0;
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create(PKIX_ESCASCII, temp, 3,
                                                                &strings[26 * (i * 26 + j) + k],
                                                                plContext));
            }

    /* ---------------------------- */
    subTest("Create a bytearray from each string's UTF-16 encoding");
    for (i = 0; i < size; i++) {
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_GetEncoded(strings[i],
                                                            PKIX_UTF16,
                                                            &dest,
                                                            &length,
                                                            plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_ByteArray_Create(dest, length, &byteArrays[i], plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(dest, plContext));
    }

    /* ---------------------------- */
    subTest("Create a copy string from each bytearray");
    for (i = 0; i < size; i++) {
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create(PKIX_UTF16, *(void **)byteArrays[i], 6,
                                                        &utf16strings[i], plContext));
    }

    /* ---------------------------- */
    subTest("Compare each original string with the copy");
    for (i = 0; i < size; i++) {
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Equals((PKIX_PL_Object *)strings[i],
                                                        (PKIX_PL_Object *)utf16strings[i],
                                                        &result,
                                                        plContext));
        if (result == 0)
            testError("Strings do not match");
    }

    /* ---------------------------- */
    subTest("Put each string into a Hashtable");
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Create(size /
                                                           2,
                                                       0, &ht, plContext));

    for (i = 0; i < size; i++) {
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Hashcode((PKIX_PL_Object *)strings[i],
                                                          &hashcode,
                                                          plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Add(ht,
                                                        (void *)&hashcode,
                                                        (void *)strings[i],
                                                        plContext));
    }

    /* ---------------------------- */
    subTest("Compare each copy string with the hashtable entries ");
    for (i = 0; i < size; i++) {
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Hashcode((PKIX_PL_Object *)utf16strings[i],
                                                          &hashcode,
                                                          plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Lookup(ht,
                                                           (void *)&hashcode,
                                                           (PKIX_PL_Object **)&tempString,
                                                           plContext));

        if (tempString == NULL)
            testError("String not found in hashtable");
        else {
            PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Equals((PKIX_PL_Object *)tempString,
                                                            (PKIX_PL_Object *)utf16strings[i],
                                                            &result,
                                                            plContext));
            if (result == 0)
                testError("Strings do not match");
            PKIX_TEST_DECREF_BC(tempString);
        }
    }

cleanup:

    /* ---------------------------- */
    subTest("Destroy All Objects");

    PKIX_TEST_DECREF_AC(ht);

    for (i = 0; i < size; i++) {
        PKIX_TEST_DECREF_AC(strings[i]);
        PKIX_TEST_DECREF_AC(utf16strings[i]);
        PKIX_TEST_DECREF_AC(byteArrays[i]);
    }

    PKIX_Shutdown(plContext);

    PKIX_TEST_RETURN();

    endTests("Stress Test");
    return (0);
}
