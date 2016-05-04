/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_valresult.c
 *
 * Test ValidateResult Type
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;

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

static void
testGetPublicKey(
    PKIX_ValidateResult *goodObject,
    PKIX_ValidateResult *equalObject)
{

    PKIX_PL_PublicKey *goodPubKey = NULL;
    PKIX_PL_PublicKey *equalPubKey = NULL;

    PKIX_TEST_STD_VARS();
    subTest("PKIX_ValidateResult_GetPublicKey");

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateResult_GetPublicKey(goodObject, &goodPubKey, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateResult_GetPublicKey(equalObject, &equalPubKey, plContext));

    testEqualsHelper((PKIX_PL_Object *)goodPubKey,
                     (PKIX_PL_Object *)equalPubKey,
                     PKIX_TRUE,
                     plContext);

cleanup:

    PKIX_TEST_DECREF_AC(goodPubKey);
    PKIX_TEST_DECREF_AC(equalPubKey);

    PKIX_TEST_RETURN();
}

static void
testGetTrustAnchor(
    PKIX_ValidateResult *goodObject,
    PKIX_ValidateResult *equalObject)
{

    PKIX_TrustAnchor *goodAnchor = NULL;
    PKIX_TrustAnchor *equalAnchor = NULL;

    PKIX_TEST_STD_VARS();
    subTest("PKIX_ValidateResult_GetTrustAnchor");

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateResult_GetTrustAnchor(goodObject, &goodAnchor, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateResult_GetTrustAnchor(equalObject, &equalAnchor, plContext));

    testEqualsHelper((PKIX_PL_Object *)goodAnchor,
                     (PKIX_PL_Object *)equalAnchor,
                     PKIX_TRUE,
                     plContext);

cleanup:

    PKIX_TEST_DECREF_AC(goodAnchor);
    PKIX_TEST_DECREF_AC(equalAnchor);

    PKIX_TEST_RETURN();
}

static void
testGetPolicyTree(
    PKIX_ValidateResult *goodObject,
    PKIX_ValidateResult *equalObject)
{

    PKIX_PolicyNode *goodTree = NULL;
    PKIX_PolicyNode *equalTree = NULL;

    PKIX_TEST_STD_VARS();
    subTest("PKIX_ValidateResult_GetPolicyTree");

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateResult_GetPolicyTree(goodObject, &goodTree, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateResult_GetPolicyTree(equalObject, &equalTree, plContext));

    if (goodTree) {
        testEqualsHelper((PKIX_PL_Object *)goodTree,
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

static void
printUsage(char *pName)
{
    printf("\nUSAGE: %s <central-data-dir>\n\n", pName);
}

int
test_valresult(int argc, char *argv[])
{

    PKIX_ValidateResult *goodObject = NULL;
    PKIX_ValidateResult *equalObject = NULL;
    PKIX_ValidateResult *diffObject = NULL;
    PKIX_UInt32 actualMinorVersion;
    PKIX_UInt32 j = 0;

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

    PKIX_TEST_EXPECT_NO_ERROR(
        PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

    if (argc < 2) {
        printUsage(argv[0]);
        return (0);
    }

    dirName = argv[j + 1];

    subTest("pkix_ValidateResult_Create");

    goodObject = createValidateResult(dirName, goodInput, diffInput, plContext);
    equalObject = createValidateResult(dirName, goodInput, diffInput, plContext);
    diffObject = createValidateResult(dirName, diffInput, goodInput, plContext);

    testGetPublicKey(goodObject, equalObject);
    testGetTrustAnchor(goodObject, equalObject);
    testGetPolicyTree(goodObject, equalObject);

    PKIX_TEST_EQ_HASH_TOSTR_DUP(goodObject,
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
