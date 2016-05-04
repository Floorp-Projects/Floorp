/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_basicchecker.c
 *
 * Test Basic Checking
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;

static void
testPass(char *dirName, char *goodInput, char *diffInput, char *dateAscii)
{

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

    valParams = createValidateParams(dirName,
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

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateChain(valParams, &valResult, &verifyTree, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString((PKIX_PL_Object *)verifyTree, &verifyString, plContext));
    (void)printf("verifyTree is\n%s\n", verifyString->escAsciiString);

cleanup:

    PKIX_TEST_DECREF_AC(verifyString);
    PKIX_TEST_DECREF_AC(verifyTree);
    PKIX_TEST_DECREF_AC(chain);
    PKIX_TEST_DECREF_AC(valParams);
    PKIX_TEST_DECREF_AC(valResult);

    PKIX_TEST_RETURN();
}

static void
testNameChainingFail(
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

    valParams = createValidateParams(dirName,
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

    PKIX_TEST_EXPECT_ERROR(PKIX_ValidateChain(valParams, &valResult, &verifyTree, plContext));

cleanup:

    PKIX_TEST_DECREF_AC(verifyString);
    PKIX_TEST_DECREF_AC(verifyTree);
    PKIX_TEST_DECREF_AC(chain);
    PKIX_TEST_DECREF_AC(valParams);
    PKIX_TEST_DECREF_AC(valResult);

    PKIX_TEST_RETURN();
}

static void
testDateFail(char *dirName, char *goodInput, char *diffInput)
{

    PKIX_List *chain = NULL;
    PKIX_ValidateParams *valParams = NULL;
    PKIX_ValidateResult *valResult = NULL;

    PKIX_TEST_STD_VARS();

    chain = createCertChain(dirName, goodInput, diffInput, plContext);

    subTest("Expiration <fail>");
    valParams = createValidateParams(dirName,
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

    PKIX_TEST_EXPECT_ERROR(PKIX_ValidateChain(valParams, &valResult, NULL, plContext));

cleanup:

    PKIX_TEST_DECREF_AC(chain);
    PKIX_TEST_DECREF_AC(valParams);
    PKIX_TEST_DECREF_AC(valResult);

    PKIX_TEST_RETURN();
}

static void
testSignatureFail(
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

    valParams = createValidateParams(dirName,
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

    PKIX_TEST_EXPECT_ERROR(PKIX_ValidateChain(valParams, &valResult, NULL, plContext));

cleanup:

    PKIX_TEST_DECREF_AC(chain);
    PKIX_TEST_DECREF_AC(valParams);
    PKIX_TEST_DECREF_AC(valResult);

    PKIX_TEST_RETURN();
}

static void
printUsage(char *pName)
{
    printf("\nUSAGE: %s <central-data-dir>\n\n", pName);
}

int
test_basicchecker(int argc, char *argv[])
{

    char *goodInput = "yassir2yassir";
    char *diffInput = "yassir2bcn";
    char *dateAscii = "991201000000Z";
    char *dirName = NULL;
    PKIX_UInt32 j = 0;
    PKIX_UInt32 actualMinorVersion;

    PKIX_TEST_STD_VARS();

    startTests("SignatureChecker");

    PKIX_TEST_EXPECT_NO_ERROR(
        PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

    if (argc < 2) {
        printUsage(argv[0]);
        return (0);
    }

    dirName = argv[j + 1];

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
