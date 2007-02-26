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
 * test_valresult.c
 *
 * Test ValidateResult Type
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

void *plContext = NULL;

static void
testDestroy(void *goodObject, void *equalObject, void *diffObject)
{
        PKIX_TEST_STD_VARS();

        subTest("PKIX_ValidateResult_Destroy");

        PKIX_TEST_DECREF_BC(goodObject);
        PKIX_TEST_DECREF_BC(equalObject);
        PKIX_TEST_DECREF_BC(diffObject);

cleanup:

        PKIX_TEST_RETURN();

}

void testGetPublicKey(
        PKIX_ValidateResult *goodObject,
        PKIX_ValidateResult *equalObject){

        PKIX_PL_PublicKey *goodPubKey = NULL;
        PKIX_PL_PublicKey *equalPubKey = NULL;

        PKIX_TEST_STD_VARS();
        subTest("PKIX_ValidateResult_GetPublicKey");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateResult_GetPublicKey
                (goodObject, &goodPubKey, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateResult_GetPublicKey
                (equalObject, &equalPubKey, plContext));

        testEqualsHelper
                ((PKIX_PL_Object *)goodPubKey,
                (PKIX_PL_Object *)equalPubKey,
                PKIX_TRUE,
                plContext);

cleanup:

        PKIX_TEST_DECREF_AC(goodPubKey);
        PKIX_TEST_DECREF_AC(equalPubKey);

        PKIX_TEST_RETURN();
}

void testGetTrustAnchor(
        PKIX_ValidateResult *goodObject,
        PKIX_ValidateResult *equalObject){

        PKIX_TrustAnchor *goodAnchor = NULL;
        PKIX_TrustAnchor *equalAnchor = NULL;

        PKIX_TEST_STD_VARS();
        subTest("PKIX_ValidateResult_GetTrustAnchor");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateResult_GetTrustAnchor
                (goodObject, &goodAnchor, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateResult_GetTrustAnchor
                (equalObject, &equalAnchor, plContext));

        testEqualsHelper
                ((PKIX_PL_Object *)goodAnchor,
                (PKIX_PL_Object *)equalAnchor,
                PKIX_TRUE,
                plContext);

cleanup:

        PKIX_TEST_DECREF_AC(goodAnchor);
        PKIX_TEST_DECREF_AC(equalAnchor);

        PKIX_TEST_RETURN();
}

void testGetPolicyTree(
        PKIX_ValidateResult *goodObject,
        PKIX_ValidateResult *equalObject){

        PKIX_PolicyNode *goodTree = NULL;
        PKIX_PolicyNode *equalTree = NULL;

        PKIX_TEST_STD_VARS();
        subTest("PKIX_ValidateResult_GetPolicyTree");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateResult_GetPolicyTree
                (goodObject, &goodTree, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateResult_GetPolicyTree
                (equalObject, &equalTree, plContext));

        if (goodTree) {
                testEqualsHelper
                        ((PKIX_PL_Object *)goodTree,
                        (PKIX_PL_Object *)equalTree,
                        PKIX_TRUE,
                        plContext);
        } else if (equalTree) {
                pkixTestErrorMsg = "Mismatch: NULL and non-NULL Policy Trees";
        }

cleanup:

        PKIX_TEST_DECREF_AC(goodTree);
        PKIX_TEST_DECREF_AC(equalTree);

        PKIX_TEST_RETURN();
}

void printUsage(char *pName){
        printf("\nUSAGE: %s <central-data-dir>\n\n", pName);
}

int main(int argc, char *argv[]) {

        PKIX_ValidateResult *goodObject = NULL;
        PKIX_ValidateResult *equalObject = NULL;
        PKIX_ValidateResult *diffObject = NULL;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;
        PKIX_Boolean useArenas = PKIX_FALSE;

        char *goodInput = "yassir2yassir";
        char *diffInput = "yassir2bcn";
        char *dirName = NULL;

        char *expectedAscii =
                "[\n"
                "\tTrustAnchor: \t\t"
                "[\n"
                "\tTrusted CA Name:         "
                "CN=yassir,OU=bcn,OU=east,O=sun,C=us\n"
                "\tTrusted CA PublicKey:    ANSI X9.57 DSA Signature\n"
                "\tInitial Name Constraints:(null)\n"
                "]\n"
                "\tPubKey:    \t\t"
                "ANSI X9.57 DSA Signature\n"
                "\tPolicyTree:  \t\t(null)\n"
                "]\n";

        PKIX_TEST_STD_VARS();

        startTests("ValidateResult");

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

        subTest("pkix_ValidateResult_Create");

        goodObject = createValidateResult
                (dirName, goodInput, diffInput, plContext);
        equalObject = createValidateResult
                (dirName, goodInput, diffInput, plContext);
        diffObject = createValidateResult
                (dirName, diffInput, goodInput, plContext);

        testGetPublicKey(goodObject, equalObject);
        testGetTrustAnchor(goodObject, equalObject);
        testGetPolicyTree(goodObject, equalObject);

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (goodObject,
                equalObject,
                diffObject,
                expectedAscii,
                ValidateResult,
                PKIX_FALSE);

        testDestroy(goodObject, equalObject, diffObject);

cleanup:

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("ValidateResult");

        return (0);
}
