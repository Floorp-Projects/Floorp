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
 * test_oid.c
 *
 * Test OID Types
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

void *plContext = NULL;

void
createOID(
        PKIX_PL_OID **testOID,
        char *oidAscii,
        PKIX_Boolean errorHandling)
{
        PKIX_TEST_STD_VARS();

        if (errorHandling){
                PKIX_TEST_EXPECT_ERROR
                        (PKIX_PL_OID_Create(oidAscii, testOID, plContext));
        } else {
                PKIX_TEST_EXPECT_NO_ERROR
                        (PKIX_PL_OID_Create(oidAscii, testOID, plContext));
        }


cleanup:

        PKIX_TEST_RETURN();
}

void
testToString(
        PKIX_PL_OID *oid,
        char *expAscii)
{
        PKIX_PL_String *oidString = NULL;
        char *temp = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString
                                    ((PKIX_PL_Object*)oid,
                                    &oidString, plContext));

        temp = PKIX_String2ASCII(oidString, plContext);
        if (temp == NULL){
                testError("PKIX_String2Ascii failed");
                goto cleanup;
        }

        if (PL_strcmp(temp, expAscii) != 0) {
                (void) printf("\tOid ToString: %s %s\n", temp, expAscii);
                testError("Output string does not match source");
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));

cleanup:

        PKIX_TEST_DECREF_AC(oidString);

        PKIX_TEST_RETURN();
}

void
testCompare(
            PKIX_PL_OID *oid0,
            PKIX_PL_OID *oid1,
            PKIX_PL_OID *oid2,
            PKIX_PL_OID *oid3)
{
        PKIX_Int32 cmpResult;
        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Compare((PKIX_PL_Object*)oid0,
                                            (PKIX_PL_Object*)oid1,
                                            &cmpResult, plContext));
        if (cmpResult <= 0) testError("Invalid Result from OID Compare");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Compare((PKIX_PL_Object*)oid1,
                                        (PKIX_PL_Object*)oid0,
                                        &cmpResult, plContext));
        if (cmpResult >= 0) testError("Invalid Result from OID Compare");


        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Compare((PKIX_PL_Object*)oid1,
                                        (PKIX_PL_Object*)oid2,
                                        &cmpResult, plContext));
        if (cmpResult >= 0) testError("Invalid Result from OID Compare");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Compare((PKIX_PL_Object*)oid2,
                                        (PKIX_PL_Object*)oid1,
                                        &cmpResult, plContext));
        if (cmpResult <= 0) testError("Invalid Result from OID Compare");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Compare((PKIX_PL_Object*)oid1,
                                        (PKIX_PL_Object*)oid3,
                                        &cmpResult, plContext));
        if (cmpResult != 0) testError("Invalid Result from OID Compare");

cleanup:

        PKIX_TEST_RETURN();
}

void
testDestroy(
        PKIX_PL_OID *oid)
{
        PKIX_TEST_STD_VARS();

        PKIX_TEST_DECREF_BC(oid);

cleanup:

        PKIX_TEST_RETURN();
}


int main(int argc, char *argv[]) {

        PKIX_PL_OID *testOID[6] = {NULL};
        PKIX_PL_OID *badTestOID = NULL;
        PKIX_UInt32 i, size = 6;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;
        PKIX_Boolean useArenas = PKIX_FALSE;

        char* validOID[6] = {
                "2.11.22222.33333",
                "1.2.3.004.5.6.7",
                "2.11.22222.33333",
                "1.2.3.4.5.6.7",
                "1.2.3",
                "2.39.3"
        };

        char* expected[6] = {
                "2.11.22222.33333",
                "1.2.3.4.5.6.7",
                "2.11.22222.33333",
                "1.2.3.4.5.6.7",
                "1.2.3",
                "2.39.3"
        };

        char *badOID[11] = {
                "1.2.4294967299",
                "this. is. a. bad. oid",
                "00a1000.002b",
                "100.-5.10",
                "1.2..3",
                ".1.2.3",
                "1.2.3.",
                "00010.1.2.3",
                "1.000041.2.3",
                "000000000000000000000000000000000000000010.3.2",
                "1"
        };

        PKIX_TEST_STD_VARS();

        startTests("OIDs");

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                                    (PKIX_TRUE, /* nssInitNeeded */
                                    useArenas,
                                    PKIX_MAJOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    &actualMinorVersion,
                                    &plContext));

        for (i = 0; i < size; i++) {
                subTest("PKIX_PL_OID_Create");
                createOID(&testOID[i], validOID[i], PKIX_FALSE);
        }

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (testOID[0],
                testOID[2],
                testOID[1],
                NULL,
                OID,
                PKIX_FALSE);

        for (i = 0; i < size; i++) {
                subTest("PKIX_PL_OID_ToString");
                testToString(testOID[i], expected[i]);
        }

        subTest("PKIX_PL_OID_Compare");
        testCompare(testOID[0], testOID[1], testOID[2], testOID[3]);

        for (i = 0; i < size; i++) {
                subTest("PKIX_PL_OID_Destroy");
                testDestroy(testOID[i]);
        }

        for (i = 0; i < 11; i++) {
                subTest("PKIX_PL_OID Error Handling");
                createOID(&badTestOID, badOID[i], PKIX_TRUE);
        }

cleanup:

        PKIX_Shutdown(plContext);

        endTests("OIDs");

        return (0);

}
