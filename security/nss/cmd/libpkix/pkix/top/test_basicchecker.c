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
 * test_basicchecker.c
 *
 * Test Basic Checking
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

void *plContext = NULL;

void testPass(char *dirName, char *goodInput, char *diffInput, char *dateAscii){

        PKIX_List *chain = NULL;
        PKIX_ValidateParams *valParams = NULL;
        PKIX_ValidateResult *valResult = NULL;
	PKIX_VerifyNode *verifyTree = NULL;
	PKIX_PL_String *verifyString = NULL;

        PKIX_TEST_STD_VARS();

        subTest("Basic-Common-Fields <pass>");
        /*
         * Tests the Expiration, NameChaining, and Signature Checkers
         */

        chain = createCertChain(dirName, goodInput, diffInput, plContext);

        valParams = createValidateParams
                (dirName,
                goodInput,
                diffInput,
                dateAscii,
                NULL,
                PKIX_FALSE,
                PKIX_FALSE,
                PKIX_FALSE,
                PKIX_FALSE,
                chain,
                plContext);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateChain
                (valParams, &valResult, &verifyTree, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString
                ((PKIX_PL_Object*)verifyTree, &verifyString, plContext));
        (void) printf("verifyTree is\n%s\n", verifyString->escAsciiString);

cleanup:

        PKIX_TEST_DECREF_AC(verifyString);
        PKIX_TEST_DECREF_AC(verifyTree);
        PKIX_TEST_DECREF_AC(chain);
        PKIX_TEST_DECREF_AC(valParams);
        PKIX_TEST_DECREF_AC(valResult);

        PKIX_TEST_RETURN();
}

void testNameChainingFail(
        char *dirName,
        char *goodInput,
        char *diffInput,
        char *dateAscii)
{
        PKIX_List *chain = NULL;
        PKIX_ValidateParams *valParams = NULL;
        PKIX_ValidateResult *valResult = NULL;
	PKIX_VerifyNode *verifyTree = NULL;
	PKIX_PL_String *verifyString = NULL;

        PKIX_TEST_STD_VARS();

        subTest("NameChaining <fail>");

        chain = createCertChain(dirName, diffInput, goodInput, plContext);

        valParams = createValidateParams
                (dirName,
                goodInput,
                diffInput,
                dateAscii,
                NULL,
                PKIX_FALSE,
                PKIX_FALSE,
                PKIX_FALSE,
                PKIX_FALSE,
                chain,
                plContext);

        PKIX_TEST_EXPECT_ERROR(PKIX_ValidateChain
                (valParams, &valResult, &verifyTree, plContext));

cleanup:

        PKIX_TEST_DECREF_AC(verifyString);
        PKIX_TEST_DECREF_AC(verifyTree);
        PKIX_TEST_DECREF_AC(chain);
        PKIX_TEST_DECREF_AC(valParams);
        PKIX_TEST_DECREF_AC(valResult);

        PKIX_TEST_RETURN();
}

void testDateFail(char *dirName, char *goodInput, char *diffInput){

        PKIX_List *chain = NULL;
        PKIX_ValidateParams *valParams = NULL;
        PKIX_ValidateResult *valResult = NULL;

        PKIX_TEST_STD_VARS();

        chain = createCertChain(dirName, goodInput, diffInput, plContext);

        subTest("Expiration <fail>");
        valParams = createValidateParams
                (dirName,
                goodInput,
                diffInput,
                NULL,
                NULL,
                PKIX_FALSE,
                PKIX_FALSE,
                PKIX_FALSE,
                PKIX_FALSE,
                chain,
                plContext);

        PKIX_TEST_EXPECT_ERROR(PKIX_ValidateChain
                                (valParams, &valResult, NULL, plContext));

cleanup:

        PKIX_TEST_DECREF_AC(chain);
        PKIX_TEST_DECREF_AC(valParams);
        PKIX_TEST_DECREF_AC(valResult);

        PKIX_TEST_RETURN();
}

void testSignatureFail(
       char *dirName,
       char *goodInput,
       char *diffInput,
       char *dateAscii)
{
        PKIX_List *chain = NULL;
        PKIX_ValidateParams *valParams = NULL;
        PKIX_ValidateResult *valResult = NULL;

        PKIX_TEST_STD_VARS();

        subTest("Signature <fail>");

        chain = createCertChain(dirName, diffInput, goodInput, plContext);

        valParams = createValidateParams
                (dirName,
                goodInput,
                diffInput,
                dateAscii,
                NULL,
                PKIX_FALSE,
                PKIX_FALSE,
                PKIX_FALSE,
                PKIX_FALSE,
                chain,
                plContext);

        PKIX_TEST_EXPECT_ERROR(PKIX_ValidateChain
                                (valParams, &valResult, NULL, plContext));

cleanup:

        PKIX_TEST_DECREF_AC(chain);
        PKIX_TEST_DECREF_AC(valParams);
        PKIX_TEST_DECREF_AC(valResult);

        PKIX_TEST_RETURN();
}

void printUsage(char *pName){
        printf("\nUSAGE: %s <central-data-dir>\n\n", pName);
}

int main(int argc, char *argv[]) {

        char *goodInput = "yassir2yassir";
        char *diffInput = "yassir2bcn";
        char *dateAscii = "991201000000Z";
        char *dirName = NULL;
        PKIX_UInt32 j = 0;
        PKIX_Boolean useArenas = PKIX_FALSE;
        PKIX_UInt32 actualMinorVersion;

        PKIX_TEST_STD_VARS();

        startTests("SignatureChecker");

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                                    (PKIX_TRUE, /* nssInitNeeded */
                                    useArenas,
                                    PKIX_MAJOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    &actualMinorVersion,
                                    &plContext));

        if (argc < 2){
                printUsage(argv[0]);
                return (0);
        }

        dirName = argv[j+1];

        /* The NameChaining, Expiration, and Signature Checkers all pass */
        testPass(dirName, goodInput, diffInput, dateAscii);

        /* Individual Checkers fail */
        testNameChainingFail(dirName, goodInput, diffInput, dateAscii);
        testDateFail(dirName, goodInput, diffInput);

        /*
         * XXX
         * since the signature check is done last, we need to create
         * certs whose name chaining passes, but their signatures fail;
         * we currently don't have any such certs.
         */
        /* testSignatureFail(goodInput, diffInput, dateAscii); */


cleanup:

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("SignatureChecker");

        return (0);
}
