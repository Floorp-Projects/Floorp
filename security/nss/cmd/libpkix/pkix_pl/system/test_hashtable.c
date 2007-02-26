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
 * test_hashtable.c
 *
 * Tests Hashtables
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

void *plContext = NULL;

void
createHashTables(
        PKIX_PL_HashTable **ht,
        PKIX_PL_HashTable **ht2,
        PKIX_PL_HashTable **ht3,
        PKIX_PL_HashTable **ht4)
{
        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Create
                (1, 0, ht, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Create
                (5, 0, ht2, plContext));

        /* at most two entries per bucket */
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Create
                (1, 2, ht4, plContext));

        *ht3 = *ht;
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Object_IncRef((PKIX_PL_Object*)*ht3, plContext));

cleanup:
        PKIX_TEST_RETURN();
}

void
testAdd(
        PKIX_PL_HashTable *ht,
        PKIX_PL_HashTable *ht2,
        PKIX_PL_String **testString,
        PKIX_PL_String **testString2,
        PKIX_PL_String **testString3,
        PKIX_PL_OID **testOID)
{
        char* dummyString = "test string 1";
        char* dummyString2 = "test string 2";
        char* dummyString3 = "test string 3";
        char* dummyOID = "2.11.22222.33333";

        PKIX_TEST_STD_VARS();

        /* Make some dummy objects */
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_String_Create(
                                PKIX_ESCASCII,
                                dummyString,
                                PL_strlen(dummyString),
                                testString,
                                plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_String_Create(
                                PKIX_ESCASCII,
                                dummyString2,
                                PL_strlen(dummyString2),
                                testString2,
                                plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_String_Create(
                                PKIX_ESCASCII,
                                dummyString3,
                                PL_strlen(dummyString3),
                                testString3,
                                plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_OID_Create(dummyOID, testOID, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Add
                (ht,
                (PKIX_PL_Object *)*testString,
                (PKIX_PL_Object *)*testString2,
                plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Add
                (ht2,
                (PKIX_PL_Object *)*testString,
                (PKIX_PL_Object *)*testString2,
                plContext));


        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Add
                (ht,
                (PKIX_PL_Object *)*testString2,
                (PKIX_PL_Object *)*testString,
                plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Add
                (ht2,
                (PKIX_PL_Object *)*testString2,
                (PKIX_PL_Object *)*testString,
                plContext));



        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Add
                (ht,
                (PKIX_PL_Object *)*testOID,
                (PKIX_PL_Object *)*testOID,
                plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Add
                (ht2,
                (PKIX_PL_Object *)*testOID,
                (PKIX_PL_Object *)*testOID,
                plContext));

cleanup:
        PKIX_TEST_RETURN();
}

void
testAddFIFO(
        PKIX_PL_HashTable *ht,
        PKIX_PL_String **testString,
        PKIX_PL_String **testString2,
        PKIX_PL_String **testString3)
{
        PKIX_PL_String *targetString = NULL;
        PKIX_Boolean cmpResult;

        PKIX_TEST_STD_VARS();

        /*
         * ht is created as one bucket, two entries per bucket. Since we add
         * three items to the ht, we expect the first one to be deleted.
         */
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Add
                (ht,
                (PKIX_PL_Object *)*testString,
                (PKIX_PL_Object *)*testString,
                plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Add
                (ht,
                (PKIX_PL_Object *)*testString2,
                (PKIX_PL_Object *)*testString2,
                plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Add
                (ht,
                (PKIX_PL_Object *)*testString3,
                (PKIX_PL_Object *)*testString3,
                plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Lookup
                                    (ht,
                                    (PKIX_PL_Object *)*testString,
                                    (PKIX_PL_Object**)&targetString,
                                    plContext));
        if (targetString != NULL) {
                testError("HashTable_Lookup retrieved a supposed deleted item");
                PKIX_TEST_DECREF_BC(targetString);
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Lookup
                                    (ht,
                                    (PKIX_PL_Object *)*testString3,
                                    (PKIX_PL_Object**)&targetString,
                                    plContext));

         PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Object_Equals(
                                    (PKIX_PL_Object *)targetString,
                                    (PKIX_PL_Object *)*testString3,
                                    &cmpResult,
                                    plContext));
        if (cmpResult != PKIX_TRUE){
                testError("HashTable_Lookup failed");
        }
        PKIX_TEST_DECREF_BC(targetString);

cleanup:
        PKIX_TEST_RETURN();
}

void
testLookup(
        PKIX_PL_HashTable *ht,
        PKIX_PL_HashTable *ht2,
        PKIX_PL_String *testString,
        PKIX_PL_String *testString2,
        PKIX_PL_String *testString3,
        PKIX_PL_OID *testOID)
{
        PKIX_PL_String *targetString = NULL;
        PKIX_PL_String *targetOID = NULL;
        PKIX_Boolean cmpResult;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Lookup
                                    (ht,
                                    (PKIX_PL_Object *)testString,
                                    (PKIX_PL_Object**)&targetString,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Object_Equals(
                                    (PKIX_PL_Object *)targetString,
                                    (PKIX_PL_Object *)testString2,
                                    &cmpResult,
                                    plContext));
        if (cmpResult != PKIX_TRUE){
                testError("HashTable_Lookup failed");
        }
        PKIX_TEST_DECREF_BC(targetString);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Lookup
                (ht2,
                (PKIX_PL_Object *)testString,
                (PKIX_PL_Object**)&targetString,
                plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Object_Equals(
                                    (PKIX_PL_Object *)targetString,
                                    (PKIX_PL_Object *)testString2,
                                    &cmpResult,
                                    plContext));
        if (cmpResult != PKIX_TRUE){
                testError("HashTable_Lookup failed");
        }
        PKIX_TEST_DECREF_BC(targetString);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Lookup
                (ht2,
                (PKIX_PL_Object *)testString2,
                (PKIX_PL_Object**)&targetString,
                plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Object_Equals(
                                    (PKIX_PL_Object *)targetString,
                                    (PKIX_PL_Object *)testString,
                                    &cmpResult,
                                    plContext));
        if (cmpResult != PKIX_TRUE){
                testError("HashTable_Lookup failed");
        }
        PKIX_TEST_DECREF_BC(targetString);


        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Lookup
                (ht,
                (PKIX_PL_Object *)testOID,
                (PKIX_PL_Object**)&targetOID,
                plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString
                ((PKIX_PL_Object*)targetOID, &targetString, plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Object_Equals(
                                    (PKIX_PL_Object *)targetOID,
                                    (PKIX_PL_Object *)testOID,
                                    &cmpResult,
                                    plContext));
        if (cmpResult != PKIX_TRUE){
                testError("HashTable_Lookup failed");
        }
        PKIX_TEST_DECREF_BC(targetString);
        PKIX_TEST_DECREF_BC(targetOID);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Lookup
                (ht2,
                (PKIX_PL_Object *)testOID,
                (PKIX_PL_Object**)&targetOID,
                plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString
                ((PKIX_PL_Object*)targetOID, &targetString, plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Object_Equals(
                                    (PKIX_PL_Object *)targetOID,
                                    (PKIX_PL_Object *)testOID,
                                    &cmpResult,
                                    plContext));
        if (cmpResult != PKIX_TRUE){
                testError("HashTable_Lookup failed");
        }
        PKIX_TEST_DECREF_BC(targetString);
        PKIX_TEST_DECREF_BC(targetOID);

        (void) printf("Looking up item not in HashTable.\n");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Lookup
                (ht,
                (PKIX_PL_Object *)testString3,
                (PKIX_PL_Object**)&targetString,
                plContext));
        if (targetString == NULL)
                (void) printf("\tCorrectly returned NULL.\n");
        else
                testError("Hashtable did not return NULL value as expected");


cleanup:
        PKIX_TEST_RETURN();
}

void
testRemove(
        PKIX_PL_HashTable *ht,
        PKIX_PL_HashTable *ht2,
        PKIX_PL_String *testString,
        PKIX_PL_String *testString2,
        PKIX_PL_OID *testOID)
{

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Remove
                                (ht,
                                (PKIX_PL_Object *)testString,
                                plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Remove
                                (ht,
                                (PKIX_PL_Object *)testOID,
                                plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Remove
                                (ht2,
                                (PKIX_PL_Object *)testString2,
                                plContext));

        PKIX_TEST_EXPECT_ERROR(PKIX_PL_HashTable_Remove
                            (ht,
                            (PKIX_PL_Object *)testString,
                            plContext));

cleanup:
        PKIX_TEST_RETURN();
}

void
testDestroy(
        PKIX_PL_HashTable *ht,
        PKIX_PL_HashTable *ht2,
        PKIX_PL_HashTable *ht3,
        PKIX_PL_HashTable *ht4)
{
        PKIX_TEST_STD_VARS();

        PKIX_TEST_DECREF_BC(ht);
        PKIX_TEST_DECREF_BC(ht2);
        PKIX_TEST_DECREF_BC(ht3);
        PKIX_TEST_DECREF_BC(ht4);

cleanup:
        PKIX_TEST_RETURN();
}




int main(int argc, char *argv[]) {

        PKIX_PL_HashTable *ht, *ht2, *ht3, *ht4;
        PKIX_PL_String *testString, *testString2, *testString3;
        PKIX_PL_OID *testOID;
        PKIX_UInt32 actualMinorVersion;
        PKIX_Boolean useArenas = PKIX_FALSE;
        PKIX_UInt32 j = 0;

        PKIX_TEST_STD_VARS();

        startTests("HashTables");

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                                    (PKIX_TRUE, /* nssInitNeeded */
                                    useArenas,
                                    PKIX_MAJOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    &actualMinorVersion,
                                    &plContext));

        subTest("PKIX_PL_HashTable_Create");
        createHashTables(&ht, &ht2, &ht3, &ht4);

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (ht,
                ht3,
                ht2,
                NULL,
                HashTable,
                PKIX_FALSE);

        subTest("PKIX_PL_HashTable_Add");
        testAdd(ht, ht2, &testString, &testString2, &testString3, &testOID);

        subTest("PKIX_PL_HashTable_ADD - with Bucket Size limit");
        testAddFIFO(ht4, &testString, &testString2, &testString3);

        subTest("PKIX_PL_HashTable_Lookup");
        testLookup(ht, ht2, testString, testString2, testString3, testOID);

        subTest("PKIX_PL_HashTable_Remove");
        testRemove(ht, ht2, testString, testString2, testOID);

        PKIX_TEST_DECREF_BC(testString);
        PKIX_TEST_DECREF_BC(testString2);
        PKIX_TEST_DECREF_BC(testString3);
        PKIX_TEST_DECREF_BC(testOID);

        subTest("PKIX_PL_HashTable_Destroy");
        testDestroy(ht, ht2, ht3, ht4);

cleanup:

        PKIX_Shutdown(plContext);

        endTests("BigInt");

        return (0);

}
