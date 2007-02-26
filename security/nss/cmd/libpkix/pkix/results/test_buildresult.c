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
 * test_buildresult.c
 *
 * Test BuildResult Type
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

void *plContext = NULL;

static void
testDestroy(void *goodObject, void *equalObject, void *diffObject)
{
        PKIX_TEST_STD_VARS();

        subTest("PKIX_BuildResult_Destroy");

        PKIX_TEST_DECREF_BC(goodObject);
        PKIX_TEST_DECREF_BC(equalObject);
        PKIX_TEST_DECREF_BC(diffObject);

cleanup:

        PKIX_TEST_RETURN();

}

void testGetValidateResult(
        PKIX_BuildResult *goodObject,
        PKIX_BuildResult *equalObject){

        PKIX_ValidateResult *goodValResult = NULL;
        PKIX_ValidateResult *equalValResult = NULL;

        PKIX_TEST_STD_VARS();
        subTest("PKIX_BuildResult_GetValidateResult");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_BuildResult_GetValidateResult
                (goodObject, &goodValResult, NULL));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_BuildResult_GetValidateResult
                (equalObject, &equalValResult, NULL));

        testEqualsHelper
                ((PKIX_PL_Object *)goodValResult,
                (PKIX_PL_Object *)equalValResult,
                PKIX_TRUE,
                plContext);

cleanup:

        PKIX_TEST_DECREF_AC(goodValResult);
        PKIX_TEST_DECREF_AC(equalValResult);

        PKIX_TEST_RETURN();
}

void testGetCertChain(
        PKIX_BuildResult *goodObject,
        PKIX_BuildResult *equalObject){

        PKIX_List *goodChain = NULL;
        PKIX_List *equalChain = NULL;

        PKIX_TEST_STD_VARS();
        subTest("PKIX_BuildResult_GetCertChain");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_BuildResult_GetCertChain
                (goodObject, &goodChain, NULL));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_BuildResult_GetCertChain
                (equalObject, &equalChain, NULL));

        testEqualsHelper
                ((PKIX_PL_Object *)goodChain,
                (PKIX_PL_Object *)equalChain,
                PKIX_TRUE,
                plContext);

cleanup:

        PKIX_TEST_DECREF_AC(goodChain);
        PKIX_TEST_DECREF_AC(equalChain);

        PKIX_TEST_RETURN();
}

void printUsage(char *pName){
        printf("\nUSAGE: %s <central-data-dir>\n\n", pName);
}

int main(int argc, char *argv[]) {

        PKIX_BuildResult *goodObject = NULL;
        PKIX_BuildResult *equalObject = NULL;
        PKIX_BuildResult *diffObject = NULL;
        PKIX_UInt32 actualMinorVersion;
        char *dirName = NULL;
	PKIX_UInt32 j = 0;
	PKIX_Boolean useArenas = PKIX_FALSE;

        char *goodInput = "yassir2yassir";
        char *diffInput = "yassir2bcn";

        char *expectedAscii =
                "[\n"
                "\tValidateResult: \t\t"
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
                "]\n"
                "\tCertChain:    \t\t("
                "[\n"
                "\tVersion:         v3\n"
                "\tSerialNumber:    37bc65af\n"
                "\tIssuer:          CN=yassir,OU=bcn,OU=east,O=sun,C=us\n"
                "\tSubject:         CN=yassir,OU=bcn,OU=east,O=sun,C=us\n"
                "\tValidity: [From: Thu Aug 19 16:14:39 1999\n"
                "\t           To:   Fri Aug 18 16:14:39 2000]\n"
                "\tSubjectAltNames: (null)\n"
                "\tAuthorityKeyId:  (null)\n"
                "\tSubjectKeyId:    (null)\n"
                "\tSubjPubKeyAlgId: ANSI X9.57 DSA Signature\n"
                "\tCritExtOIDs:     (2.5.29.15, 2.5.29.19)\n"
                "\tExtKeyUsages:    (null)\n"
                "\tBasicConstraint: CA(0)\n"
                "\tCertPolicyInfo:  (null)\n"
                "\tPolicyMappings:  (null)\n"
                "\tExplicitPolicy:  -1\n"
                "\tInhibitMapping:  -1\n"
                "\tInhibitAnyPolicy:-1\n"
                "\tNameConstraints: (null)\n"
                "]\n"
                ", [\n"
                "\tVersion:         v3\n"
                "\tSerialNumber:    37bc66ec\n"
                "\tIssuer:          CN=yassir,OU=bcn,OU=east,O=sun,C=us\n"
                "\tSubject:         OU=bcn,OU=east,O=sun,C=us\n"
                "\tValidity: [From: Thu Aug 19 16:19:56 1999\n"
                "\t           To:   Fri Aug 18 16:19:56 2000]\n"
                "\tSubjectAltNames: (null)\n"
                "\tAuthorityKeyId:  (null)\n"
                "\tSubjectKeyId:    (null)\n"
                "\tSubjPubKeyAlgId: ANSI X9.57 DSA Signature\n"
                "\tCritExtOIDs:     (2.5.29.15, 2.5.29.19)\n"
                "\tExtKeyUsages:    (null)\n"
                "\tBasicConstraint: CA(0)\n"
                "\tCertPolicyInfo:  (null)\n"
                "\tPolicyMappings:  (null)\n"
                "\tExplicitPolicy:  -1\n"
                "\tInhibitMapping:  -1\n"
                "\tInhibitAnyPolicy:-1\n"
                "\tNameConstraints: (null)\n"
                "]\n"
                ")\n"
                "]\n";

        PKIX_TEST_STD_VARS();

        startTests("BuildResult");

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

        subTest("pkix_BuildResult_Create");

        goodObject = createBuildResult
            (dirName, goodInput, diffInput, goodInput, diffInput, plContext);
        equalObject = createBuildResult
            (dirName, goodInput, diffInput, goodInput, diffInput, plContext);
        diffObject = createBuildResult
            (dirName, diffInput, goodInput, diffInput, goodInput, plContext);

        testGetValidateResult(goodObject, equalObject);
        testGetCertChain(goodObject, equalObject);

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (goodObject,
                equalObject,
                diffObject,
                NULL, /* expectedAscii, */
                BuildResult,
                PKIX_FALSE);

        testDestroy(goodObject, equalObject, diffObject);

cleanup:

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("BuildResult");

        return (0);
}
