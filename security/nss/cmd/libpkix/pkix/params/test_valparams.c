/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_valparams.c
 *
 * Test ValidateParams Type
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;

static void
testDestroy(void *goodObject, void *equalObject, void *diffObject)
{
        PKIX_TEST_STD_VARS();

        subTest("PKIX_ValidateParams_Destroy");

        PKIX_TEST_DECREF_BC(goodObject);
        PKIX_TEST_DECREF_BC(equalObject);
        PKIX_TEST_DECREF_BC(diffObject);

cleanup:

        PKIX_TEST_RETURN();

}

static
void testGetProcParams(
        PKIX_ValidateParams *goodObject,
        PKIX_ValidateParams *equalObject){

        PKIX_ProcessingParams *goodProcParams = NULL;
        PKIX_ProcessingParams *equalProcParams = NULL;

        PKIX_TEST_STD_VARS();
        subTest("PKIX_ValidateParams_GetProcessingParams");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateParams_GetProcessingParams
                (goodObject, &goodProcParams, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateParams_GetProcessingParams
                (equalObject, &equalProcParams, plContext));

        testEqualsHelper
                ((PKIX_PL_Object *)goodProcParams,
                (PKIX_PL_Object *)equalProcParams,
                PKIX_TRUE,
                plContext);

cleanup:

        PKIX_TEST_DECREF_AC(goodProcParams);
        PKIX_TEST_DECREF_AC(equalProcParams);

        PKIX_TEST_RETURN();
}


static
void testGetCertChain(
        PKIX_ValidateParams *goodObject,
        PKIX_ValidateParams *equalObject){

        PKIX_List *goodChain = NULL;
        PKIX_List *equalChain = NULL;

        PKIX_TEST_STD_VARS();
        subTest("PKIX_ValidateParams_GetCertChain");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateParams_GetCertChain
                (goodObject, &goodChain, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateParams_GetCertChain
                (equalObject, &equalChain, plContext));

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

static
void printUsage(char *pName){
        printf("\nUSAGE: %s <central-data-dir>\n\n", pName);
}

int test_valparams(int argc, char *argv[]) {

        PKIX_ValidateParams *goodObject = NULL;
        PKIX_ValidateParams *equalObject = NULL;
        PKIX_ValidateParams *diffObject = NULL;
        PKIX_List *chain = NULL;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;
        char *dirName = NULL;

        char *goodInput = "yassir2yassir";
        char *diffInput = "yassir2bcn";

        char *expectedAscii =
                "[\n"
                "\tProcessing Params: \n"
                "\t********BEGIN PROCESSING PARAMS********\n"
                "\t\t"
                "[\n"
                "\tTrust Anchors: \n"
                "\t********BEGIN LIST OF TRUST ANCHORS********\n"
                "\t\t"
"([\n"
                "\tTrusted CA Name:         "
                "CN=yassir,OU=bcn,OU=east,O=sun,C=us\n"
                "\tTrusted CA PublicKey:    ANSI X9.57 DSA Signature\n"
                "\tInitial Name Constraints:(null)\n"
                "]\n"
                ", [\n"
                "\tTrusted CA Name:         OU=bcn,OU=east,O=sun,C=us\n"
                "\tTrusted CA PublicKey:    ANSI X9.57 DSA Signature\n"
                "\tInitial Name Constraints:(null)\n"
                "]\n"
                ")\n"
                "\t********END LIST OF TRUST ANCHORS********\n"
                "\tDate:    \t\t(null)\n"
                "\tTarget Constraints:    (null)\n"
                "\tInitial Policies:      (null)\n"
                "\tQualifiers Rejected:   FALSE\n"
                "\tCert Stores:           (EMPTY)\n"
                "\tCRL Checking Enabled:  0\n"
                "]\n"
                "\n"
                "\t********END PROCESSING PARAMS********\n"
                "\tChain:    \t\t"
                "([\n"
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
                ", [\n"
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
                ")\n"
                "]\n";

        PKIX_TEST_STD_VARS();

        startTests("ValidateParams");

        PKIX_TEST_EXPECT_NO_ERROR(
            PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

        if (argc < 2){
                printUsage(argv[0]);
                return (0);
        }

        dirName = argv[j+1];

        subTest("PKIX_ValidateParams_Create");
        chain = createCertChain(dirName, diffInput, goodInput, plContext);
        goodObject = createValidateParams
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
        equalObject = createValidateParams
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
        diffObject = createValidateParams
                (dirName,
                diffInput,
                goodInput,
                NULL,
                NULL,
                PKIX_FALSE,
                PKIX_FALSE,
                PKIX_FALSE,
                PKIX_FALSE,
                chain,
                plContext);

        testGetProcParams(goodObject, equalObject);
        testGetCertChain(goodObject, equalObject);

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (goodObject,
                equalObject,
                diffObject,
                NULL, /* expectedAscii, */
                ValidateParams,
                PKIX_FALSE);

        testDestroy(goodObject, equalObject, diffObject);

cleanup:

        PKIX_TEST_DECREF_AC(chain);

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("ValidateParams");

        return (0);
}
