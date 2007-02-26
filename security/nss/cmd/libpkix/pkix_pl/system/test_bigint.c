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
 * test_bigint.c
 *
 * Tests BigInt Types
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

void *plContext = NULL;

void
createBigInt(
        PKIX_PL_BigInt **bigInts,
        char *bigIntAscii,
        PKIX_Boolean errorHandling)
{
        PKIX_PL_String *bigIntString = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                                    (PKIX_ESCASCII,
                                    bigIntAscii,
                                    PL_strlen(bigIntAscii),
                                    &bigIntString,
                                    plContext));

        if (errorHandling){
                PKIX_TEST_EXPECT_ERROR(PKIX_PL_BigInt_Create
                                            (bigIntString,
                                            bigInts,
                                            plContext));
        } else {
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_BigInt_Create
                                            (bigIntString,
                                            bigInts,
                                            plContext));
        }

cleanup:

        PKIX_TEST_DECREF_AC(bigIntString);

        PKIX_TEST_RETURN();
}

void
testToString(
        PKIX_PL_BigInt *bigInt,
        char *expAscii)
{
        PKIX_PL_String *bigIntString = NULL;
        char *temp = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString
                                    ((PKIX_PL_Object*)bigInt,
                                    &bigIntString, plContext));

        temp = PKIX_String2ASCII(bigIntString, plContext);
        if (temp == plContext){
                testError("PKIX_String2Ascii failed");
                goto cleanup;
        }

        if (PL_strcmp(temp, expAscii) != 0) {
                (void) printf("\tBigInt ToString: %s %s\n", temp, expAscii);
                testError("Output string does not match source");
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));

cleanup:

        PKIX_TEST_DECREF_AC(bigIntString);

        PKIX_TEST_RETURN();
}

void
testCompare(
        PKIX_PL_BigInt *firstBigInt,
        PKIX_PL_BigInt *secondBigInt,
        PKIX_Int32 *cmpResult)
{
        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Compare
                                    ((PKIX_PL_Object*)firstBigInt,
                                    (PKIX_PL_Object*)secondBigInt,
                                    cmpResult, plContext));
cleanup:

        PKIX_TEST_RETURN();
}

void
testDestroy(
        PKIX_PL_BigInt *bigInt)
{
        PKIX_TEST_STD_VARS();

        PKIX_TEST_DECREF_BC(bigInt);

cleanup:

        PKIX_TEST_RETURN();
}

int main(int argc, char *argv[]) {

        PKIX_UInt32 size = 4, badSize = 3, i = 0;
        PKIX_PL_BigInt *testBigInt[4] = {NULL};
        PKIX_Int32 cmpResult;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;
        PKIX_Boolean useArenas = PKIX_FALSE;

        char *bigIntValue[4] =
        {
                "03",
                "ff",
                "1010101010101010101010101010101010101010",
                "1010101010101010101010101010101010101010",
        };

        char *badValue[3] = {"00ff", "fff", "-ff"};

        PKIX_TEST_STD_VARS();

        startTests("BigInts");

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                                    (PKIX_TRUE, /* nssInitNeeded */
                                    useArenas,
                                    PKIX_MAJOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    &actualMinorVersion,
                                    &plContext));

        for (i = 0; i < badSize; i++) {
                subTest("PKIX_PL_BigInt_Create <error_handling>");
                createBigInt(&testBigInt[i], badValue[i], PKIX_TRUE);
        }

        for (i = 0; i < size; i++) {
                subTest("PKIX_PL_BigInt_Create");
                createBigInt(&testBigInt[i], bigIntValue[i], PKIX_FALSE);
        }

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (testBigInt[2],
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
        if (cmpResult <= 0){
                testError("Invalid Result from String Compare");
        }

        subTest("PKIX_PL_BigInt_Compare <lt>");
        testCompare(testBigInt[1], testBigInt[2], &cmpResult);
        if (cmpResult >= 0){
                testError("Invalid Result from String Compare");
        }

        subTest("PKIX_PL_BigInt_Compare <eq>");
        testCompare(testBigInt[2], testBigInt[3], &cmpResult);
        if (cmpResult != 0){
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
