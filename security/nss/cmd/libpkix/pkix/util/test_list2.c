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
 * test_list2.c
 *
 * Performs an in-place sort on a list
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

void *plContext = NULL;

int main(int argc, char *argv[]) {

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
        PKIX_Boolean useArenas = PKIX_FALSE;
        PKIX_UInt32 actualMinorVersion;

        PKIX_TEST_STD_VARS();

        startTests("List Sorting");

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                                    (PKIX_TRUE, /* nssInitNeeded */
                                    useArenas,
                                    PKIX_MAJOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    &actualMinorVersion,
                                    &plContext));

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
