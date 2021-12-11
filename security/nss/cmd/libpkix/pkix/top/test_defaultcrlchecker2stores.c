/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_defaultcrlchecker2stores.c
 *
 * Test Default CRL with multiple CertStore Checking
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

#define PKIX_TEST_MAX_CERTS 10

static void *plContext = NULL;

static void
printUsage1(char *pName)
{
    printf("\nUSAGE: %s test-purpose [ENE|EE] ", pName);
    printf("crl-directory cert [certs].\n");
}

static void
printUsageMax(PKIX_UInt32 numCerts)
{
    printf("\nUSAGE ERROR: number of certs %d exceed maximum %d\n",
           numCerts, PKIX_TEST_MAX_CERTS);
}

static PKIX_Error *
getCertCallback(
    PKIX_CertStore *store,
    PKIX_CertSelector *certSelector,
    PKIX_List **pCerts,
    void *plContext)
{
    return (NULL);
}

static PKIX_Error *
testDefaultMultipleCertStores(PKIX_ValidateParams *valParams,
                              char *crlDir1,
                              char *crlDir2)
{
    PKIX_PL_String *dirString1 = NULL;
    PKIX_PL_String *dirString2 = NULL;
    PKIX_CertStore *certStore1 = NULL;
    PKIX_CertStore *certStore2 = NULL;
    PKIX_List *certStoreList = NULL;
    PKIX_ProcessingParams *procParams = NULL;

    PKIX_TEST_STD_VARS();

    subTest("PKIX_PL_CollectionCertStore_Create");

    /* Create CollectionCertStore */

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create(PKIX_ESCASCII,
                                                    crlDir1,
                                                    0,
                                                    &dirString1,
                                                    plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CollectionCertStore_Create(dirString1,
                                                                 &certStore1,
                                                                 plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create(PKIX_ESCASCII,
                                                    crlDir2,
                                                    0,
                                                    &dirString2,
                                                    plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CollectionCertStore_Create(dirString2,
                                                                 &certStore2,
                                                                 plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateParams_GetProcessingParams(valParams, &procParams, plContext));

    /* Add multiple CollectionCertStores */

    subTest("PKIX_ProcessingParams_SetCertStores");
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&certStoreList, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem(certStoreList, (PKIX_PL_Object *)certStore1, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetCertStores(procParams, certStoreList, plContext));

    subTest("PKIX_ProcessingParams_AddCertStore");
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_AddCertStore(procParams, certStore2, plContext));

    subTest("PKIX_ProcessingParams_SetRevocationEnabled");
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetRevocationEnabled(procParams, PKIX_TRUE, plContext));

cleanup:

    PKIX_TEST_DECREF_AC(dirString1);
    PKIX_TEST_DECREF_AC(dirString2);
    PKIX_TEST_DECREF_AC(certStore1);
    PKIX_TEST_DECREF_AC(certStore2);
    PKIX_TEST_DECREF_AC(certStoreList);
    PKIX_TEST_DECREF_AC(procParams);

    PKIX_TEST_RETURN();

    return (0);
}

/*
 * Validate Certificate Chain with Certificate Revocation List
 *      Certificate Chain is build based on input certs' sequence.
 *      CRL is fetched from the directory specified in CollectionCertStore.
 *      while CollectionCertStore is linked in CertStore Object which then
 *      linked in ProcessParam. During validation, CRLChecker will invoke
 *      the crlCallback (this test uses PKIX_PL_CollectionCertStore_GetCRL)
 *      to get CRL data for revocation check.
 *      This test gets CRL's from two CertStores, each has a valid CRL
 *      required for revocation check to pass.
 */

int
test_defaultcrlchecker2stores(int argc, char *argv[])
{

    PKIX_List *chain = NULL;
    PKIX_ValidateParams *valParams = NULL;
    PKIX_ValidateResult *valResult = NULL;
    PKIX_UInt32 actualMinorVersion;
    char *certNames[PKIX_TEST_MAX_CERTS];
    PKIX_PL_Cert *certs[PKIX_TEST_MAX_CERTS];
    PKIX_VerifyNode *verifyTree = NULL;
    PKIX_PL_String *verifyString = NULL;
    PKIX_UInt32 chainLength = 0;
    PKIX_UInt32 i = 0;
    PKIX_UInt32 j = 0;
    PKIX_Boolean testValid = PKIX_TRUE;
    char *dirName = NULL;
    char *anchorName = NULL;

    PKIX_TEST_STD_VARS();

    if (argc < 6) {
        printUsage1(argv[0]);
        return (0);
    }

    startTests("CRL Checker");

    PKIX_TEST_EXPECT_NO_ERROR(
        PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

    /* ENE = expect no error; EE = expect error */
    if (PORT_Strcmp(argv[2 + j], "ENE") == 0) {
        testValid = PKIX_TRUE;
    } else if (PORT_Strcmp(argv[2 + j], "EE") == 0) {
        testValid = PKIX_FALSE;
    } else {
        printUsage1(argv[0]);
        return (0);
    }

    chainLength = (argc - j) - 7;
    if (chainLength > PKIX_TEST_MAX_CERTS) {
        printUsageMax(chainLength);
    }

    for (i = 0; i < chainLength; i++) {

        certNames[i] = argv[(7 + j) + i];
        certs[i] = NULL;
    }

    subTest(argv[1 + j]);

    subTest("Default-CRL-Checker");

    subTest("Default-CRL-Checker - Create Cert Chain");

    dirName = argv[3 + j];

    chain = createCertChainPlus(dirName, certNames, certs, chainLength, plContext);

    subTest("Default-CRL-Checker - Create Params");

    anchorName = argv[6 + j];

    valParams = createValidateParams(dirName,
                                     anchorName,
                                     NULL,
                                     NULL,
                                     NULL,
                                     PKIX_FALSE,
                                     PKIX_FALSE,
                                     PKIX_FALSE,
                                     PKIX_FALSE,
                                     chain,
                                     plContext);

    subTest("Multiple-CertStores");

    testDefaultMultipleCertStores(valParams, argv[4 + j], argv[5 + j]);

    subTest("Default-CRL-Checker - Validate Chain");

    if (testValid == PKIX_TRUE) {
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateChain(valParams, &valResult, &verifyTree, plContext));
    } else {
        PKIX_TEST_EXPECT_ERROR(PKIX_ValidateChain(valParams, &valResult, &verifyTree, plContext));
    }

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString((PKIX_PL_Object *)verifyTree, &verifyString, plContext));
    (void)printf("verifyTree is\n%s\n", verifyString->escAsciiString);

cleanup:

    PKIX_TEST_DECREF_AC(verifyString);
    PKIX_TEST_DECREF_AC(verifyTree);

    PKIX_TEST_DECREF_AC(valParams);
    PKIX_TEST_DECREF_AC(valResult);
    PKIX_TEST_DECREF_AC(chain);

    PKIX_Shutdown(plContext);

    PKIX_TEST_RETURN();

    endTests("CRL Checker");

    return (0);
}
