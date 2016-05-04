/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_ekuchecker.c
 *
 * Test Extend Key Usage Checker
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
    printf("[E]oid[,oid]* <data-dir> cert [certs].\n");
}

static void
printUsageMax(PKIX_UInt32 numCerts)
{
    printf("\nUSAGE ERROR: number of certs %d exceed maximum %d\n",
           numCerts, PKIX_TEST_MAX_CERTS);
}

static PKIX_Error *
testCertSelectorMatchCallback(
    PKIX_CertSelector *selector,
    PKIX_PL_Cert *cert,
    PKIX_Boolean *pResult,
    void *plContext)
{
    *pResult = PKIX_TRUE;

    return (0);
}

static PKIX_Error *
testEkuSetup(
    PKIX_ValidateParams *valParams,
    char *ekuOidString,
    PKIX_Boolean *only4EE)
{
    PKIX_ProcessingParams *procParams = NULL;
    PKIX_List *ekuList = NULL;
    PKIX_PL_OID *ekuOid = NULL;
    PKIX_ComCertSelParams *selParams = NULL;
    PKIX_CertSelector *certSelector = NULL;
    PKIX_Boolean last_token = PKIX_FALSE;
    PKIX_UInt32 i, tokeni;

    PKIX_TEST_STD_VARS();

    subTest("PKIX_ValidateParams_GetProcessingParams");
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateParams_GetProcessingParams(valParams, &procParams, plContext));

    /* Get extended key usage OID(s) from command line, separated by ","  */

    if (ekuOidString[0] == '"') {
        /* erase doble quotes, if any */
        i = 1;
        while (ekuOidString[i] != '"' && ekuOidString[i] != '\0') {
            ekuOidString[i - 1] = ekuOidString[i];
            i++;
        }
        ekuOidString[i - 1] = '\0';
    }

    if (ekuOidString[0] == '\0') {
        ekuList = NULL;
    } else {

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&ekuList, plContext));

        /* if OID string start with E, only check for last cert */
        if (ekuOidString[0] == 'E') {
            *only4EE = PKIX_TRUE;
            tokeni = 2;
            i = 1;
        } else {
            *only4EE = PKIX_FALSE;
            tokeni = 1;
            i = 0;
        }

        while (last_token != PKIX_TRUE) {
            while (ekuOidString[tokeni] != ',' &&
                   ekuOidString[tokeni] != '\0') {
                tokeni++;
            }
            if (ekuOidString[tokeni] == '\0') {
                last_token = PKIX_TRUE;
            } else {
                ekuOidString[tokeni] = '\0';
                tokeni++;
            }

            PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_OID_Create(&ekuOidString[i], &ekuOid, plContext));

            PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem(ekuList, (PKIX_PL_Object *)ekuOid, plContext));

            PKIX_TEST_DECREF_BC(ekuOid);
            i = tokeni;
        }
    }

    /* Set extended key usage link to processing params */

    subTest("PKIX_ComCertSelParams_Create");
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_Create(&selParams, plContext));

    subTest("PKIX_ComCertSelParams_SetExtendedKeyUsage");
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetExtendedKeyUsage(selParams, ekuList, plContext));

    subTest("PKIX_CertSelector_Create");
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_Create(testCertSelectorMatchCallback,
                                                       NULL,
                                                       &certSelector,
                                                       plContext));

    subTest("PKIX_CertSelector_SetCommonCertSelectorParams");
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_SetCommonCertSelectorParams(certSelector, selParams, plContext));

    subTest("PKIX_ProcessingParams_SetTargetCertConstraints");
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetTargetCertConstraints(procParams, certSelector, plContext));

cleanup:

    PKIX_TEST_DECREF_AC(selParams);
    PKIX_TEST_DECREF_AC(certSelector);
    PKIX_TEST_DECREF_AC(procParams);
    PKIX_TEST_DECREF_AC(ekuOid);
    PKIX_TEST_DECREF_AC(ekuList);

    PKIX_TEST_RETURN();

    return (0);
}

static PKIX_Error *
testEkuChecker(
    PKIX_ValidateParams *valParams,
    PKIX_Boolean only4EE)
{
    PKIX_ProcessingParams *procParams = NULL;

    PKIX_TEST_STD_VARS();

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateParams_GetProcessingParams(valParams, &procParams, plContext));

    subTest("PKIX_ProcessingParams_SetRevocationEnabled - disable");
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetRevocationEnabled(procParams, PKIX_FALSE, plContext));

    if (only4EE == PKIX_FALSE) {
        subTest("PKIX_PL_EkuChecker_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_EkuChecker_Create(procParams, plContext));
    }

cleanup:

    PKIX_TEST_DECREF_AC(procParams);

    PKIX_TEST_RETURN();

    return (0);
}

int
test_ekuchecker(int argc, char *argv[])
{
    PKIX_List *chain = NULL;
    PKIX_ValidateParams *valParams = NULL;
    PKIX_ValidateResult *valResult = NULL;
    PKIX_UInt32 actualMinorVersion;
    char *certNames[PKIX_TEST_MAX_CERTS];
    char *dirName = NULL;
    PKIX_PL_Cert *certs[PKIX_TEST_MAX_CERTS];
    PKIX_UInt32 chainLength = 0;
    PKIX_UInt32 i = 0;
    PKIX_UInt32 j = 0;
    PKIX_Boolean testValid = PKIX_FALSE;
    PKIX_Boolean only4EE = PKIX_FALSE;

    PKIX_TEST_STD_VARS();

    if (argc < 5) {
        printUsage1(argv[0]);
        return (0);
    }

    startTests("EKU Checker");

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

    dirName = argv[4 + j];

    chainLength = (argc - j) - 6;
    if (chainLength > PKIX_TEST_MAX_CERTS) {
        printUsageMax(chainLength);
    }

    for (i = 0; i < chainLength; i++) {

        certNames[i] = argv[6 + i + j];
        certs[i] = NULL;
    }

    subTest(argv[1 + j]);

    subTest("Extended-Key-Usage-Checker");

    subTest("Extended-Key-Usage-Checker - Create Cert Chain");

    chain = createCertChainPlus(dirName, certNames, certs, chainLength, plContext);

    subTest("Extended-Key-Usage-Checker - Create Params");

    valParams = createValidateParams(dirName,
                                     argv[5 +
                                          j],
                                     NULL,
                                     NULL,
                                     NULL,
                                     PKIX_FALSE,
                                     PKIX_FALSE,
                                     PKIX_FALSE,
                                     PKIX_FALSE,
                                     chain,
                                     plContext);

    subTest("Default CertStore");

    testEkuSetup(valParams, argv[3 + j], &only4EE);

    testEkuChecker(valParams, only4EE);

    subTest("Extended-Key-Usage-Checker - Validate Chain");

    if (testValid == PKIX_TRUE) {
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateChain(valParams, &valResult, NULL, plContext));
    } else {
        PKIX_TEST_EXPECT_ERROR(PKIX_ValidateChain(valParams, &valResult, NULL, plContext));
    }

cleanup:

    PKIX_TEST_DECREF_AC(chain);
    PKIX_TEST_DECREF_AC(valParams);
    PKIX_TEST_DECREF_AC(valResult);

    PKIX_Shutdown(plContext);

    PKIX_TEST_RETURN();

    endTests("EKU Checker");

    return (0);
}
