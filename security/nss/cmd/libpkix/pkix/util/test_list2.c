/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_list2.c
 *
 * Performs an in-place sort on a list
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;

int test_list2(int argc, char *argv[]) {

        PKIX_List *list;
        char *temp;
        PKIX_UInt32 i = 0;
        PKIX_UInt32 j = 0;
        PKIX_Int32 cmpResult;
        PKIX_PL_OID *testOID;
        PKIX_PL_String *testString;
        PKIX_PL_Object *obj, *obj2;
        PKIX_UInt32 size = 10;
        char *testOIDString[10] = {
                "2.9.999.1.20",
                "1.2.3.4.5.6.7",
                "0.1",
                "1.2.3.5",
                "0.39",
                "1.2.3.4.7",
                "1.2.3.4.6",
                "0.39.1",
                "1.2.3.4.5",
                "0.39.1.300"
        };
        PKIX_UInt32 actualMinorVersion;

        PKIX_TEST_STD_VARS();

        startTests("List Sorting");

        PKIX_TEST_EXPECT_NO_ERROR(
            PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

        subTest("Creating Unsorted Lists");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&list, plContext));
        for (i = 0; i < size; i++) {
                /* Create a new OID object */
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_OID_Create(
                                                testOIDString[i],
                                                &testOID,
                                                plContext));
                /* Insert it into the list */
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                            (list, (PKIX_PL_Object*)testOID, plContext));
                /* Decref the string object */
                PKIX_TEST_DECREF_BC(testOID);
        }

        subTest("Outputting Unsorted List");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString((PKIX_PL_Object*)list,
                                            &testString,
                                            plContext));
        temp = PKIX_String2ASCII(testString, plContext);
        if (temp){
                (void) printf("%s \n", temp);
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }
        PKIX_TEST_DECREF_BC(testString);

        subTest("Performing Bubble Sort");

        for (i = 0; i < size; i++)
                for (j = 9; j > i; j--) {
                        PKIX_TEST_EXPECT_NO_ERROR
                                (PKIX_List_GetItem(list, j, &obj, plContext));
                        PKIX_TEST_EXPECT_NO_ERROR
                                (PKIX_List_GetItem
                                (list, j-1, &obj2, plContext));

                        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Compare
                                    (obj, obj2, &cmpResult, plContext));
                        if (cmpResult < 0) {
                                /* Exchange the items */
                                PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_SetItem
                                            (list, j, obj2, plContext));
                                PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_SetItem
                                            (list, j-1, obj, plContext));
                        }
                        /* DecRef objects */
                        PKIX_TEST_DECREF_BC(obj);
                        PKIX_TEST_DECREF_BC(obj2);
                }

        subTest("Outputting Sorted List");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString((PKIX_PL_Object*)list,
                                            &testString,
                                            plContext));
        temp = PKIX_String2ASCII(testString, plContext);
        if (temp){
                (void) printf("%s \n", temp);
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }

cleanup:

        PKIX_TEST_DECREF_AC(testString);
        PKIX_TEST_DECREF_AC(list);

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("List Sorting");

        return (0);
}
