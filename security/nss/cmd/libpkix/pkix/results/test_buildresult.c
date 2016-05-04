/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_buildresult.c
 *
 * Test BuildResult Type
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;

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

static void
testGetValidateResult(
    PKIX_BuildResult *goodObject,
    PKIX_BuildResult *equalObject)
{

    PKIX_ValidateResult *goodValResult = NULL;
    PKIX_ValidateResult *equalValResult = NULL;

    PKIX_TEST_STD_VARS();
    subTest("PKIX_BuildResult_GetValidateResult");

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_BuildResult_GetValidateResult(goodObject, &goodValResult, NULL));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_BuildResult_GetValidateResult(equalObject, &equalValResult, NULL));

    testEqualsHelper((PKIX_PL_Object *)goodValResult,
                     (PKIX_PL_Object *)equalValResult,
                     PKIX_TRUE,
                     plContext);

cleanup:

    PKIX_TEST_DECREF_AC(goodValResult);
    PKIX_TEST_DECREF_AC(equalValResult);

    PKIX_TEST_RETURN();
}

static void
testGetCertChain(
    PKIX_BuildResult *goodObject,
    PKIX_BuildResult *equalObject)
{

    PKIX_List *goodChain = NULL;
    PKIX_List *equalChain = NULL;

    PKIX_TEST_STD_VARS();
    subTest("PKIX_BuildResult_GetCertChain");

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_BuildResult_GetCertChain(goodObject, &goodChain, NULL));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_BuildResult_GetCertChain(equalObject, &equalChain, NULL));

    testEqualsHelper((PKIX_PL_Object *)goodChain,
                     (PKIX_PL_Object *)equalChain,
                     PKIX_TRUE,
                     plContext);

cleanup:

    PKIX_TEST_DECREF_AC(goodChain);
    PKIX_TEST_DECREF_AC(equalChain);

    PKIX_TEST_RETURN();
}

static void
printUsage(char *pName)
{
    printf("\nUSAGE: %s <central-data-dir>\n\n", pName);
}

int
test_buildresult(int argc, char *argv[])
{

    PKIX_BuildResult *goodObject = NULL;
    PKIX_BuildResult *equalObject = NULL;
    PKIX_BuildResult *diffObject = NULL;
    PKIX_UInt32 actualMinorVersion;
    char *dirName = NULL;
    PKIX_UInt32 j = 0;

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

    PKIX_TEST_EXPECT_NO_ERROR(
        PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

    if (argc < 2) {
        printUsage(argv[0]);
        return (0);
    }

    dirName = argv[j + 1];

    subTest("pkix_BuildResult_Create");

    goodObject = createBuildResult(dirName, goodInput, diffInput, goodInput, diffInput, plContext);
    equalObject = createBuildResult(dirName, goodInput, diffInput, goodInput, diffInput, plContext);
    diffObject = createBuildResult(dirName, diffInput, goodInput, diffInput, goodInput, plContext);

    testGetValidateResult(goodObject, equalObject);
    testGetCertChain(goodObject, equalObject);

    PKIX_TEST_EQ_HASH_TOSTR_DUP(goodObject,
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
