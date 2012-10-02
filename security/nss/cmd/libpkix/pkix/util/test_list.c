/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_list.c
 *
 * Tests List Objects
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;

static void
createLists(PKIX_List **list, PKIX_List **list2)
{
        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(list, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(list2, plContext));

cleanup:

        PKIX_TEST_RETURN();
}

static void
testReverseList(void)
{
        PKIX_List *firstList = NULL;
        PKIX_List *reverseList = NULL;
        PKIX_UInt32 length, i;
        char *testItemString = "one";
        char *testItemString2 = "two";
        PKIX_PL_String *testItem = NULL;
        PKIX_PL_String *testItem2 = NULL;
        PKIX_PL_Object *retrievedItem1 = NULL;
        PKIX_PL_Object *retrievedItem2 = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&firstList, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_ReverseList
                                    (firstList, &reverseList, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength
                                    (reverseList, &length, plContext));
        if (length != 0){
                testError("Incorrect Length returned");
        }

        PKIX_TEST_DECREF_BC(reverseList);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                                    (PKIX_ESCASCII,
                                    testItemString,
                                    0,
                                    &testItem,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                                    (PKIX_ESCASCII,
                                    testItemString2,
                                    0,
                                    &testItem2,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                                    (firstList,
                                    (PKIX_PL_Object*)testItem,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_ReverseList
                                    (firstList, &reverseList, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength
                                    (reverseList, &length, plContext));
        if (length != 1){
                testError("Incorrect Length returned");
        }

        PKIX_TEST_DECREF_BC(reverseList);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                                    (firstList,
                                    (PKIX_PL_Object*)testItem2,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                                    (firstList,
                                    (PKIX_PL_Object*)testItem,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                                    (firstList,
                                    (PKIX_PL_Object*)testItem2,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_ReverseList
                                    (firstList, &reverseList, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength
                                    (reverseList, &length, plContext));
        if (length != 4){
                testError("Incorrect Length returned");
        }

        for (i = 0; i < length; i++){
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                                            (firstList,
                                            i,
                                            &retrievedItem1,
                                            plContext));

                PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                                            (reverseList,
                                            (length - 1) - i,
                                            &retrievedItem2,
                                            plContext));

                testEqualsHelper
                        (retrievedItem1, retrievedItem2, PKIX_TRUE, plContext);

                PKIX_TEST_DECREF_BC(retrievedItem1);
                PKIX_TEST_DECREF_BC(retrievedItem2);

        }

cleanup:

        PKIX_TEST_DECREF_AC(firstList);
        PKIX_TEST_DECREF_AC(reverseList);

        PKIX_TEST_DECREF_AC(testItem);
        PKIX_TEST_DECREF_AC(testItem2);

        PKIX_TEST_DECREF_AC(retrievedItem1);
        PKIX_TEST_DECREF_AC(retrievedItem2);

        PKIX_TEST_RETURN();
}

static void
testZeroLengthList(PKIX_List *list)
{
        PKIX_UInt32 length;
        PKIX_Boolean empty;
        char *testItemString = "hello";
        PKIX_PL_String *testItem = NULL;
        PKIX_PL_String *retrievedItem = NULL;
        PKIX_List *diffList = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&diffList, plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_GetLength(list, &length, plContext));

        if (length != 0){
                testError("Incorrect Length returned");
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_IsEmpty(list, &empty, plContext));
        if (!empty){
                testError("Incorrect result for PKIX_List_IsEmpty");
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                                    (PKIX_ESCASCII,
                                    testItemString,
                                    0,
                                    &testItem,
                                    plContext));

        PKIX_TEST_EXPECT_ERROR(PKIX_List_InsertItem
                            (list, 0, (PKIX_PL_Object *)testItem, plContext));

        PKIX_TEST_EXPECT_ERROR(PKIX_List_SetItem
                            (list, 0, (PKIX_PL_Object *)testItem, plContext));

        PKIX_TEST_EXPECT_ERROR(PKIX_List_GetItem
                            (list,
                            0,
                            (PKIX_PL_Object **)&retrievedItem,
                            plContext));

        PKIX_TEST_EXPECT_ERROR(PKIX_List_DeleteItem(list, 0, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                                    (diffList,
                                    (PKIX_PL_Object*)testItem,
                                    plContext));

        testDuplicateHelper((PKIX_PL_Object *)diffList, plContext);

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (list, list, diffList, "(EMPTY)", List, PKIX_TRUE);

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_GetLength(diffList, &length, plContext));
        if (length != 1){
                testError("Incorrect Length returned");
        }

        PKIX_TEST_EXPECT_ERROR(PKIX_List_DeleteItem(list, 1, plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_DeleteItem(diffList, 0, plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_GetLength(diffList, &length, plContext));
        if (length != 0){
                testError("Incorrect Length returned");
        }

cleanup:

        PKIX_TEST_DECREF_AC(testItem);
        PKIX_TEST_DECREF_AC(diffList);
        PKIX_TEST_RETURN();
}

static void
testGetLength(PKIX_List *list)
{
        PKIX_UInt32 length;
        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_GetLength(list, &length, plContext));

        if (length != 3){
                testError("Incorrect Length returned");
        }

cleanup:

        PKIX_TEST_RETURN();
}

static void
testGetSetItem(
        PKIX_List *list,
        char *testItemString,
        char *testItemString2,
        char *testItemString3,
        PKIX_PL_String **testItem,
        PKIX_PL_String **testItem2,
        PKIX_PL_String **testItem3)
{
        PKIX_PL_Object *tempItem = NULL;
        char *temp = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                                        (PKIX_ESCASCII,
                                        testItemString,
                                        PL_strlen(testItemString),
                                        testItem,
                                        plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                                        (PKIX_ESCASCII,
                                        testItemString2,
                                        PL_strlen(testItemString2),
                                        testItem2,
                                        plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                                        (PKIX_ESCASCII,
                                        testItemString3,
                                        PL_strlen(testItemString3),
                                        testItem3,
                                        plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_AppendItem
                (list, (PKIX_PL_Object*)*testItem, plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_AppendItem
                (list, (PKIX_PL_Object*)*testItem, plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_AppendItem
                (list, (PKIX_PL_Object*)*testItem, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_SetItem
                    (list, 0, (PKIX_PL_Object*)*testItem, plContext));


        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_SetItem
                    (list, 1, (PKIX_PL_Object*)*testItem2, plContext));


        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_SetItem
                    (list, 2, (PKIX_PL_Object*)*testItem3, plContext));


        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_GetItem(list, 0, &tempItem, plContext));

        temp = PKIX_String2ASCII((PKIX_PL_String*)tempItem, plContext);
        if (temp){
                if (PL_strcmp(testItemString, temp) != 0)
                testError("GetItem from list is incorrect");
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }

        PKIX_TEST_DECREF_BC(tempItem);

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_GetItem(list, 1, &tempItem, plContext));

        temp = PKIX_String2ASCII((PKIX_PL_String*)tempItem, plContext);
        if (temp){
                if (PL_strcmp(testItemString2, temp) != 0)
                        testError("GetItem from list is incorrect");
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }
        PKIX_TEST_DECREF_BC(tempItem);

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_GetItem(list, 2, &tempItem, plContext));

        temp = PKIX_String2ASCII((PKIX_PL_String*)tempItem, plContext);
        if (temp){
                if (PL_strcmp(testItemString3, temp) != 0)
                        testError("GetItem from list is incorrect");
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }
        PKIX_TEST_DECREF_BC(tempItem);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_SetItem
                    (list, 0, (PKIX_PL_Object*)*testItem3, plContext));
        temp = PKIX_String2ASCII(*testItem3, plContext);
        if (temp){
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_GetItem(list, 0, &tempItem, plContext));

        temp = PKIX_String2ASCII((PKIX_PL_String*)tempItem, plContext);
        if (temp){
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }

        temp = PKIX_String2ASCII((PKIX_PL_String*)tempItem, plContext);
        if (temp){
                if (PL_strcmp(testItemString3, temp) != 0)
                        testError("GetItem from list is incorrect");
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }
        PKIX_TEST_DECREF_BC(tempItem);


cleanup:

        PKIX_TEST_RETURN();
}

static void
testInsertItem(
        PKIX_List *list,
        PKIX_PL_String *testItem,
        char *testItemString)
{
        PKIX_PL_Object *tempItem = NULL;
        PKIX_PL_String *outputString = NULL;
        char *temp = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_InsertItem
                (list, 0, (PKIX_PL_Object*)testItem, plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_GetItem(list, 0, &tempItem, plContext));

        temp = PKIX_String2ASCII((PKIX_PL_String*)tempItem, plContext);
        if (temp){
                if (PL_strcmp(testItemString, temp) != 0)
                        testError("GetItem from list is incorrect");
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }
        PKIX_TEST_DECREF_BC(tempItem);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString((PKIX_PL_Object*)list,
                                        &outputString,
                                        plContext));

        temp = PKIX_String2ASCII(outputString, plContext);
        if (temp){
                if (PL_strcmp("(a, c, b, c)", temp) != 0)
                        testError("List toString is Incorrect");
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }

        PKIX_TEST_DECREF_BC(outputString);

cleanup:

        PKIX_TEST_RETURN();
}

static void
testAppendItem(PKIX_List *list, PKIX_PL_String *testItem)
{
        PKIX_UInt32 length2;
        PKIX_PL_String *outputString = NULL;
        char *temp = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_GetLength(list, &length2, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem(list,
                                        (PKIX_PL_Object*)testItem,
                                        plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString((PKIX_PL_Object*)list,
                                        &outputString,
                                        plContext));

        temp = PKIX_String2ASCII(outputString, plContext);
        if (temp){
                if (PL_strcmp("(a, c, b, c, a)", temp) != 0)
                        testError("List toString is Incorrect");
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }

        PKIX_TEST_DECREF_BC(outputString);

cleanup:

        PKIX_TEST_RETURN();
}

static void
testNestedLists(
        PKIX_List *list,
        PKIX_List *list2,
        PKIX_PL_String *testItem,
        PKIX_PL_String *testItem2)
{
        PKIX_PL_String *outputString = NULL;
        char *temp = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_AppendItem
                (list2, (PKIX_PL_Object*)testItem, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem(list2,
                                        (PKIX_PL_Object*)NULL,
                                        plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem(list2,
                                        (PKIX_PL_Object*)testItem,
                                        plContext));


        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Object_ToString((PKIX_PL_Object*)list2,
                                        &outputString,
                                        plContext));

        temp = PKIX_String2ASCII(outputString, plContext);
        if (temp){
                if (PL_strcmp("(a, (null), a)", temp) != 0)
                        testError("List toString is Incorrect");
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }
        PKIX_TEST_DECREF_BC(outputString);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_InsertItem(list, 1,
                                        (PKIX_PL_Object*)list2,
                                        plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString((PKIX_PL_Object*)list,
                                        &outputString,
                                        plContext));

        temp = PKIX_String2ASCII(outputString, plContext);
        if (temp){
                if (PL_strcmp("(a, (a, (null), a), c, b, c, a)", temp) != 0)
                        testError("List toString is Incorrect");
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }
        PKIX_TEST_DECREF_BC(outputString);

cleanup:

        PKIX_TEST_RETURN();
}

static void
testDeleteItem(
        PKIX_List *list,
        PKIX_List *list2,
        PKIX_PL_String *testItem2,
        PKIX_PL_String *testItem3)
{
        PKIX_PL_String *outputString = NULL;
        char *temp = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_DeleteItem(list, 5, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString
                                    ((PKIX_PL_Object*)list,
                                    &outputString,
                                    plContext));

        temp = PKIX_String2ASCII(outputString, plContext);
        if (temp){
                if (PL_strcmp("(a, (a, (null), a), c, b, c)", temp) != 0)
                        testError("List toString is Incorrect");
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }
        PKIX_TEST_DECREF_BC(outputString);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_DeleteItem(list, 1, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString
                                    ((PKIX_PL_Object*)list,
                                    &outputString,
                                    plContext));

        temp = PKIX_String2ASCII(outputString, plContext);
        if (temp){
                if (PL_strcmp("(a, c, b, c)", temp) != 0)
                        testError("List toString is Incorrect");
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }
        PKIX_TEST_DECREF_BC(outputString);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_DeleteItem(list, 0, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString((PKIX_PL_Object*)list,
                                        &outputString,
                                        plContext));

        temp = PKIX_String2ASCII(outputString, plContext);
        if (temp){
                if (PL_strcmp("(c, b, c)", temp) != 0)
                        testError("List toString is Incorrect");
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }
        PKIX_TEST_DECREF_BC(outputString);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_DeleteItem(list2, 1, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Object_ToString((PKIX_PL_Object*)list2,
                                        &outputString,
                                        plContext));
        temp = PKIX_String2ASCII(outputString, plContext);
        if (temp){
                if (PL_strcmp("(a, a)", temp) != 0)
                        testError("List toString is Incorrect");
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }
        PKIX_TEST_DECREF_BC(outputString);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                    (list2,
                    (PKIX_PL_Object*)testItem2,
                    plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Object_ToString((PKIX_PL_Object*)list2,
                                        &outputString,
                                        plContext));

        temp = PKIX_String2ASCII(outputString, plContext);
        if (temp){
                if (PL_strcmp("(a, a, b)", temp) != 0)
                        testError("List toString is Incorrect");
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }
        PKIX_TEST_DECREF_BC(outputString);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_DeleteItem(list2, 2, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Object_ToString((PKIX_PL_Object*)list2,
                                        &outputString,
                                        plContext));

        temp = PKIX_String2ASCII(outputString, plContext);
        if (temp){
                if (PL_strcmp("(a, a)", temp) != 0)
                        testError("List toString is Incorrect");
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }
        PKIX_TEST_DECREF_BC(outputString);


        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                    (list2,
                    (PKIX_PL_Object*)testItem3,
                    plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Object_ToString((PKIX_PL_Object*)list2,
                                        &outputString,
                                        plContext));
        temp = PKIX_String2ASCII(outputString, plContext);
        if (temp){
                if (PL_strcmp("(a, a, c)", temp) != 0)
                        testError("List toString is Incorrect");
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }
        PKIX_TEST_DECREF_BC(outputString);


        PKIX_TEST_DECREF_BC(list2);

cleanup:

        PKIX_TEST_RETURN();
}

#if testContainsFunction
/* This test requires pkix_List_Contains to be in nss.def */
static void
testContains(void)
{

        PKIX_List *list;
        PKIX_PL_String *testItem, *testItem2, *testItem3, *testItem4;
        char *testItemString = "a";
        char *testItemString2 = "b";
        char *testItemString3 = "c";
        char *testItemString4 = "d";
        PKIX_Boolean found = PKIX_FALSE;

        PKIX_TEST_STD_VARS();
        subTest("pkix_ListContains");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                (PKIX_ESCASCII,
                testItemString,
                PL_strlen(testItemString),
                &testItem,
                plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                (PKIX_ESCASCII,
                testItemString2,
                PL_strlen(testItemString2),
                &testItem2,
                plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                (PKIX_ESCASCII,
                testItemString3,
                PL_strlen(testItemString3),
                &testItem3,
                plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                (PKIX_ESCASCII,
                testItemString4,
                PL_strlen(testItemString4),
                &testItem4,
                plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&list, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (list, (PKIX_PL_Object*)testItem, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (list, (PKIX_PL_Object*)testItem2, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (list, (PKIX_PL_Object*)testItem3, plContext));

        subTest("pkix_List_Contains <object missing>");

        PKIX_TEST_EXPECT_NO_ERROR(pkix_List_Contains
                (list, (PKIX_PL_Object *)testItem4, &found, plContext));

        if (found){
                testError("Contains found item that wasn't there!");
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (list, (PKIX_PL_Object*)testItem4, plContext));

        subTest("pkix_List_Contains <object present>");

        PKIX_TEST_EXPECT_NO_ERROR(pkix_List_Contains
                (list, (PKIX_PL_Object *)testItem4, &found, plContext));

        if (!found){
                testError("Contains missed item that was present!");
        }

        PKIX_TEST_DECREF_BC(list);
        PKIX_TEST_DECREF_BC(testItem);
        PKIX_TEST_DECREF_BC(testItem2);
        PKIX_TEST_DECREF_BC(testItem3);
        PKIX_TEST_DECREF_BC(testItem4);

cleanup:

        PKIX_TEST_RETURN();
}
#endif

static void
testErrorHandling(void)
{
        PKIX_List *emptylist = NULL;
        PKIX_List *list = NULL;
        PKIX_PL_Object *tempItem = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&list, plContext));

        PKIX_TEST_EXPECT_ERROR
                (PKIX_List_GetItem(list, 4, &tempItem, plContext));

        PKIX_TEST_EXPECT_ERROR(PKIX_List_GetItem(list, 1, NULL, plContext));
        PKIX_TEST_EXPECT_ERROR(PKIX_List_SetItem(list, 4, tempItem, plContext));
        PKIX_TEST_EXPECT_ERROR(PKIX_List_SetItem(NULL, 1, tempItem, plContext));
        PKIX_TEST_EXPECT_ERROR
                (PKIX_List_InsertItem(list, 4, tempItem, plContext));

        PKIX_TEST_EXPECT_ERROR
                (PKIX_List_InsertItem(NULL, 1, tempItem, plContext));

        PKIX_TEST_EXPECT_ERROR(PKIX_List_AppendItem(NULL, tempItem, plContext));
        PKIX_TEST_EXPECT_ERROR(PKIX_List_DeleteItem(list, 5, plContext));
        PKIX_TEST_EXPECT_ERROR(PKIX_List_DeleteItem(NULL, 1, plContext));
        PKIX_TEST_EXPECT_ERROR(PKIX_List_GetLength(list, NULL, plContext));

        PKIX_TEST_DECREF_BC(list);
        PKIX_TEST_DECREF_BC(emptylist);

cleanup:

        PKIX_TEST_RETURN();
}

static void
testDestroy(PKIX_List *list)
{
        PKIX_TEST_STD_VARS();

        PKIX_TEST_DECREF_BC(list);

cleanup:

        PKIX_TEST_RETURN();
}

int test_list(int argc, char *argv[]) {

        PKIX_List *list, *list2;
        PKIX_PL_String *testItem, *testItem2, *testItem3;
        char *testItemString = "a";
        char *testItemString2 = "b";
        char *testItemString3 = "c";
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;

        PKIX_TEST_STD_VARS();

        startTests("Lists");

        PKIX_TEST_EXPECT_NO_ERROR(
            PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

        subTest("PKIX_List_Create");
        createLists(&list, &list2);

        subTest("pkix_List_ReverseList");
        testReverseList();

        subTest("Zero-length List");
        testZeroLengthList(list);

        subTest("PKIX_List_Get/SetItem");
        testGetSetItem
                (list,
                testItemString,
                testItemString2,
                testItemString3,
                &testItem,
                &testItem2,
                &testItem3);

        subTest("PKIX_List_GetLength");
        testGetLength(list);

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (list,
                list,
                list2,
                "(c, b, c)",
                List,
                PKIX_TRUE);

        subTest("PKIX_List_InsertItem");
        testInsertItem(list, testItem, testItemString);

        subTest("PKIX_List_AppendItem");
        testAppendItem(list, testItem);

        subTest("Nested Lists");
        testNestedLists(list, list2, testItem, testItem2);

        subTest("PKIX_List_DeleteItem");
        testDeleteItem(list, list2, testItem2, testItem3);

        PKIX_TEST_DECREF_BC(testItem);
        PKIX_TEST_DECREF_BC(testItem2);
        PKIX_TEST_DECREF_BC(testItem3);

#if testContainsFunction
/* This test requires pkix_List_Contains to be in nss.def */
        testContains();
#endif

        subTest("PKIX_List Error Handling");
        testErrorHandling();

        subTest("PKIX_List_Destroy");
        testDestroy(list);

cleanup:

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("Lists");

        return (0);

}
