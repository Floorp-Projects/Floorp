/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_buildchain_uchecker.c
 *
 * Test BuildChain User Checker function
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;
static PKIX_UInt32 numUserCheckerCalled = 0;

static void
printUsage(void)
{
    (void)printf("\nUSAGE:\ttest_buildchain_uchecker [ENE|EE] "
                 "[-|[F]<userOID>] "
                 "<trustedCert> <targetCert> <certStoreDirectory>\n\n");
    (void)printf("Builds a chain of certificates between "
                 "<trustedCert> and <targetCert>\n"
                 "using the certs and CRLs in <certStoreDirectory>.\n"
                 "If <userOID> is not an empty string, its value is used as\n"
                 "user defined checker's critical extension OID.\n"
                 "A - for <userOID> is no OID and F is for supportingForward.\n"
                 "If ENE is specified, then an Error is Not Expected.\n"
                 "If EE is specified, an Error is Expected.\n");
}

static PKIX_Error *
testUserChecker(
    PKIX_CertChainChecker *checker,
    PKIX_PL_Cert *cert,
    PKIX_List *unresExtOIDs,
    void **pNBIOContext,
    void *plContext)
{
    numUserCheckerCalled++;
    return (0);
}

int
test_buildchain_uchecker(int argc, char *argv[])
{
    PKIX_BuildResult *buildResult = NULL;
    PKIX_ComCertSelParams *certSelParams = NULL;
    PKIX_CertSelector *certSelector = NULL;
    PKIX_TrustAnchor *anchor = NULL;
    PKIX_List *anchors = NULL;
    PKIX_List *certs = NULL;
    PKIX_PL_Cert *cert = NULL;
    PKIX_ProcessingParams *procParams = NULL;
    PKIX_CertChainChecker *checker = NULL;
    char *dirName = NULL;
    PKIX_PL_String *dirNameString = NULL;
    PKIX_PL_Cert *trustedCert = NULL;
    PKIX_PL_Cert *targetCert = NULL;
    PKIX_UInt32 numCerts = 0;
    PKIX_UInt32 i = 0;
    PKIX_UInt32 j = 0;
    PKIX_UInt32 k = 0;
    PKIX_UInt32 chainLength = 0;
    PKIX_CertStore *certStore = NULL;
    PKIX_List *certStores = NULL;
    char *asciiResult = NULL;
    PKIX_Boolean result;
    PKIX_Boolean testValid = PKIX_TRUE;
    PKIX_Boolean supportForward = PKIX_FALSE;
    PKIX_List *expectedCerts = NULL;
    PKIX_List *userOIDs = NULL;
    PKIX_PL_OID *oid = NULL;
    PKIX_PL_Cert *dirCert = NULL;
    PKIX_PL_String *actualCertsString = NULL;
    PKIX_PL_String *expectedCertsString = NULL;
    char *actualCertsAscii = NULL;
    char *expectedCertsAscii = NULL;
    char *oidString = NULL;
    void *buildState = NULL;  /* needed by pkix_build for non-blocking I/O */
    void *nbioContext = NULL; /* needed by pkix_build for non-blocking I/O */

    PKIX_TEST_STD_VARS();

    if (argc < 5) {
        printUsage();
        return (0);
    }

    startTests("BuildChain_UserChecker");

    PKIX_TEST_EXPECT_NO_ERROR(
        PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

    /* ENE = expect no error; EE = expect error */
    if (PORT_Strcmp(argv[2 + j], "ENE") == 0) {
        testValid = PKIX_TRUE;
    } else if (PORT_Strcmp(argv[2 + j], "EE") == 0) {
        testValid = PKIX_FALSE;
    } else {
        printUsage();
        return (0);
    }

    /* OID specified at argv[3+j] */

    if (*argv[3 + j] != '-') {

        if (*argv[3 + j] == 'F') {
            supportForward = PKIX_TRUE;
            oidString = argv[3 + j] + 1;
        } else {
            oidString = argv[3 + j];
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&userOIDs, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_OID_Create(oidString, &oid, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem(userOIDs, (PKIX_PL_Object *)oid, plContext));
        PKIX_TEST_DECREF_BC(oid);
    }

    subTest(argv[1 + j]);

    dirName = argv[4 + j];

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&expectedCerts, plContext));

    chainLength = argc - j - 5;

    for (k = 0; k < chainLength; k++) {

        dirCert = createCert(dirName, argv[5 + k + j], plContext);

        if (k == (chainLength - 1)) {
            PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_IncRef((PKIX_PL_Object *)dirCert, plContext));
            trustedCert = dirCert;
        } else {

            PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem(expectedCerts,
                                                           (PKIX_PL_Object *)dirCert,
                                                           plContext));

            if (k == 0) {
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_IncRef((PKIX_PL_Object *)dirCert,
                                                                plContext));
                targetCert = dirCert;
            }
        }

        PKIX_TEST_DECREF_BC(dirCert);
    }

    /* create processing params with list of trust anchors */

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_TrustAnchor_CreateWithCert(trustedCert, &anchor, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&anchors, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem(anchors, (PKIX_PL_Object *)anchor, plContext));
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_Create(anchors, &procParams, plContext));

    /* create CertSelector with target certificate in params */

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_Create(&certSelParams, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetCertificate(certSelParams, targetCert, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_Create(NULL, NULL, &certSelector, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_SetCommonCertSelectorParams(certSelector, certSelParams, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetTargetCertConstraints(procParams, certSelector, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertChainChecker_Create(testUserChecker,
                                                           supportForward,
                                                           PKIX_FALSE,
                                                           userOIDs,
                                                           NULL,
                                                           &checker,
                                                           plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_AddCertChainChecker(procParams, checker, plContext));

    /* create CertStores */

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create(PKIX_ESCASCII,
                                                    dirName,
                                                    0,
                                                    &dirNameString,
                                                    plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CollectionCertStore_Create(dirNameString, &certStore, plContext));

#if 0
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Pk11CertStore_Create
                                    (&certStore, plContext));
#endif

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&certStores, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem(certStores, (PKIX_PL_Object *)certStore, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetCertStores(procParams, certStores, plContext));

    /* build cert chain using processing params and return buildResult */

    pkixTestErrorResult = PKIX_BuildChain(procParams,
                                          &nbioContext,
                                          &buildState,
                                          &buildResult,
                                          NULL,
                                          plContext);

    if (testValid == PKIX_TRUE) { /* ENE */
        if (pkixTestErrorResult) {
            (void)printf("UNEXPECTED RESULT RECEIVED!\n");
        } else {
            (void)printf("EXPECTED RESULT RECEIVED!\n");
            PKIX_TEST_DECREF_BC(pkixTestErrorResult);
        }
    } else { /* EE */
        if (pkixTestErrorResult) {
            (void)printf("EXPECTED RESULT RECEIVED!\n");
            PKIX_TEST_DECREF_BC(pkixTestErrorResult);
        } else {
            testError("UNEXPECTED RESULT RECEIVED");
        }
    }

    if (buildResult) {

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_BuildResult_GetCertChain(buildResult, &certs, NULL));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength(certs, &numCerts, plContext));

        printf("\n");

        for (i = 0; i < numCerts; i++) {
            PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem(certs,
                                                        i,
                                                        (PKIX_PL_Object **)&cert,
                                                        plContext));

            asciiResult = PKIX_Cert2ASCII(cert);

            printf("CERT[%d]:\n%s\n", i, asciiResult);

            PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(asciiResult, plContext));
            asciiResult = NULL;

            PKIX_TEST_DECREF_BC(cert);
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Equals((PKIX_PL_Object *)certs,
                                                        (PKIX_PL_Object *)expectedCerts,
                                                        &result,
                                                        plContext));

        if (!result) {
            testError("BUILT CERTCHAIN IS "
                      "NOT THE ONE THAT WAS EXPECTED");

            PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString((PKIX_PL_Object *)certs,
                                                              &actualCertsString,
                                                              plContext));

            actualCertsAscii = PKIX_String2ASCII(actualCertsString, plContext);
            if (actualCertsAscii == NULL) {
                pkixTestErrorMsg = "PKIX_String2ASCII Failed";
                goto cleanup;
            }

            PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString((PKIX_PL_Object *)expectedCerts,
                                                              &expectedCertsString,
                                                              plContext));

            expectedCertsAscii = PKIX_String2ASCII(expectedCertsString, plContext);
            if (expectedCertsAscii == NULL) {
                pkixTestErrorMsg = "PKIX_String2ASCII Failed";
                goto cleanup;
            }

            (void)printf("Actual value:\t%s\n", actualCertsAscii);
            (void)printf("Expected value:\t%s\n",
                         expectedCertsAscii);

            if (chainLength - 1 != numUserCheckerCalled) {
                pkixTestErrorMsg =
                    "PKIX user defined checker not called";
            }

            goto cleanup;
        }
    }

cleanup:
    PKIX_PL_Free(asciiResult, plContext);
    PKIX_PL_Free(actualCertsAscii, plContext);
    PKIX_PL_Free(expectedCertsAscii, plContext);

    PKIX_TEST_DECREF_AC(actualCertsString);
    PKIX_TEST_DECREF_AC(expectedCertsString);
    PKIX_TEST_DECREF_AC(expectedCerts);
    PKIX_TEST_DECREF_AC(certs);
    PKIX_TEST_DECREF_AC(cert);
    PKIX_TEST_DECREF_AC(certStore);
    PKIX_TEST_DECREF_AC(certStores);
    PKIX_TEST_DECREF_AC(dirNameString);
    PKIX_TEST_DECREF_AC(trustedCert);
    PKIX_TEST_DECREF_AC(targetCert);
    PKIX_TEST_DECREF_AC(anchor);
    PKIX_TEST_DECREF_AC(anchors);
    PKIX_TEST_DECREF_AC(procParams);
    PKIX_TEST_DECREF_AC(certSelParams);
    PKIX_TEST_DECREF_AC(certSelector);
    PKIX_TEST_DECREF_AC(buildResult);
    PKIX_TEST_DECREF_AC(procParams);
    PKIX_TEST_DECREF_AC(userOIDs);
    PKIX_TEST_DECREF_AC(checker);

    PKIX_TEST_RETURN();

    PKIX_Shutdown(plContext);

    endTests("BuildChain_UserChecker");

    return (0);
}
