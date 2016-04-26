/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_customcrlchecker.c
 *
 * Test Custom CRL Checking
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

#define PKIX_TEST_MAX_CERTS     10
#define PKIX_TEST_COLLECTIONCERTSTORE_NUM_CRLS 5

static void *plContext = NULL;
char *dirName = NULL; /* also used in callback */

static
void printUsage1(char *pName){
        printf("\nUSAGE: %s test-purpose [ENE|EE] ", pName);
        printf("cert [certs].\n");
}

static
void printUsageMax(PKIX_UInt32 numCerts){
        printf("\nUSAGE ERROR: number of certs %d exceed maximum %d\n",
                numCerts, PKIX_TEST_MAX_CERTS);
}

static PKIX_Error *
getCRLCallback(
        PKIX_CertStore *store,
        PKIX_CRLSelector *crlSelector,
        void **pNBIOContext,
        PKIX_List **pCrlList,
        void *plContext)
{
        char *crlFileNames[] = {"chem.crl",
                                "phys.crl",
                                "prof.crl",
                                "sci.crl",
                                "test.crl",
                                0 };
        PKIX_PL_CRL *crl = NULL;
        PKIX_List *crlList = NULL;
        PKIX_UInt32 i = 0;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&crlList, plContext));

        while (crlFileNames[i]) {

                crl = createCRL(dirName, crlFileNames[i++], plContext);

                if (crl != NULL) {

                        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                                (crlList, (PKIX_PL_Object *)crl, plContext));

                        PKIX_TEST_DECREF_BC(crl);
                }
        }

        *pCrlList = crlList;

cleanup:

        PKIX_TEST_RETURN();

        return (0); /* this function is called by libpkix */

}

static PKIX_Error *
getCRLContinue(
        PKIX_CertStore *store,
        PKIX_CRLSelector *crlSelector,
        void **pNBIOContext,
        PKIX_List **pCrlList,
        void *plContext)
{
        return (NULL);
}

static PKIX_Error *
getCertCallback(
        PKIX_CertStore *store,
        PKIX_CertSelector *certSelector,
        void **pNBIOContext,
        PKIX_List **pCerts,
        void *plContext)
{
        return (NULL);
}

static PKIX_Error *
getCertContinue(
        PKIX_CertStore *store,
        PKIX_CertSelector *certSelector,
        void **pNBIOContext,
        PKIX_List **pCerts,
        void *plContext)
{
        return (NULL);
}

static PKIX_Error *
testCRLSelectorMatchCallback(
        PKIX_CRLSelector *selector,
        PKIX_PL_CRL *crl,
        void *plContext)
{
        PKIX_ComCRLSelParams *comCrlSelParams = NULL;
        PKIX_List *issuerList = NULL;
        PKIX_PL_X500Name *issuer = NULL;
        PKIX_PL_X500Name *crlIssuer = NULL;
        PKIX_UInt32 numIssuers = 0;
        PKIX_UInt32 i = 0;
        PKIX_Boolean result = PKIX_FALSE;
        PKIX_Error *error = NULL;
        char *errorText = "Not an error, CRL Select mismatch";

        PKIX_TEST_STD_VARS();

        subTest("Custom_Selector_MatchCallback");

        if (selector != NULL) {
                PKIX_TEST_EXPECT_NO_ERROR
                        (PKIX_CRLSelector_GetCommonCRLSelectorParams
                        (selector, &comCrlSelParams, plContext));
        }

        if (crl != NULL) {
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CRL_GetIssuer
                        (crl, &crlIssuer, plContext));
        }

        if (comCrlSelParams != NULL) {
                PKIX_TEST_EXPECT_NO_ERROR
                        (PKIX_ComCRLSelParams_GetIssuerNames
                        (comCrlSelParams, &issuerList, plContext));
        }

        if (issuerList != NULL) {

                PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength
                    (issuerList, &numIssuers, plContext));

                for (i = 0; i < numIssuers; i++){

                        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                                    (issuerList,
                                    i, (PKIX_PL_Object **)&issuer,
                                    plContext));

                        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Equals
                                    ((PKIX_PL_Object *)crlIssuer,
                                    (PKIX_PL_Object *)issuer,
                                    &result,
                                    plContext));

                        if (result != PKIX_TRUE) {
                                break;
                        }

                        if (i == numIssuers-1) {

                                PKIX_TEST_EXPECT_NO_ERROR
                                        (PKIX_Error_Create
                                        (0,
                                        NULL,
                                        NULL,
                                        PKIX_TESTNOTANERRORCRLSELECTMISMATCH,
                                        &error,
                                        plContext));

                                PKIX_TEST_DECREF_AC(issuer);
                                issuer = NULL;
                                break;
                        }

                        PKIX_TEST_DECREF_AC(issuer);

                }
        }

cleanup:

        PKIX_TEST_DECREF_AC(comCrlSelParams);
        PKIX_TEST_DECREF_AC(crlIssuer);
        PKIX_TEST_DECREF_AC(issuer);
        PKIX_TEST_DECREF_AC(issuerList);

        PKIX_TEST_RETURN();

        return (error);

}

static PKIX_Error *
testAddIssuerName(PKIX_ComCRLSelParams *comCrlSelParams, char *issuerName)
{
        PKIX_PL_String *issuerString = NULL;
        PKIX_PL_X500Name *issuer = NULL;
        PKIX_UInt32 length = 0;

        PKIX_TEST_STD_VARS();

        subTest("PKIX_ComCRLSelParams_AddIssuerName");

        length = PL_strlen(issuerName);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                                    (PKIX_UTF8,
                                    issuerName,
                                    length,
                                    &issuerString,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_X500Name_Create(issuerString,
                                    &issuer,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCRLSelParams_AddIssuerName
                                    (comCrlSelParams, issuer, plContext));

cleanup:

        PKIX_TEST_DECREF_AC(issuerString);
        PKIX_TEST_DECREF_AC(issuer);

        PKIX_TEST_RETURN();

        return (0);
}

static PKIX_Error *
testCustomCertStore(PKIX_ValidateParams *valParams)
{
        PKIX_CertStore_CRLCallback crlCallback;
        PKIX_CertStore *certStore = NULL;
        PKIX_ProcessingParams *procParams = NULL;
        char *issuerName1 = "cn=science,o=mit,c=us";
        char *issuerName2 = "cn=physics,o=mit,c=us";
        char *issuerName3 = "cn=prof noall,o=mit,c=us";
        char *issuerName4 = "cn=testing CRL,o=test,c=us";
        PKIX_ComCRLSelParams *comCrlSelParams = NULL;
        PKIX_CRLSelector *crlSelector = NULL;
        PKIX_List *crlList = NULL;
        PKIX_UInt32 numCrl = 0;
        void *nbioContext = NULL;

        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_CollectionCertStore_Create");

        /* Create CRLSelector, link in CollectionCertStore */

        subTest("PKIX_ComCRLSelParams_AddIssuerNames");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCRLSelParams_Create
                                    (&comCrlSelParams, plContext));


        testAddIssuerName(comCrlSelParams, issuerName1);
        testAddIssuerName(comCrlSelParams, issuerName2);
        testAddIssuerName(comCrlSelParams, issuerName3);
        testAddIssuerName(comCrlSelParams, issuerName4);


        subTest("PKIX_CRLSelector_SetCommonCRLSelectorParams");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CRLSelector_Create
                                    (testCRLSelectorMatchCallback,
                                    NULL,
                                    &crlSelector,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CRLSelector_SetCommonCRLSelectorParams
                                    (crlSelector, comCrlSelParams, plContext));

        /* Create CertStore, link in CRLSelector */

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateParams_GetProcessingParams
                                    (valParams, &procParams, plContext));

        subTest("PKIX_CertStore_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertStore_Create
                                    (getCertCallback,
                                    getCRLCallback,
                                    getCertContinue,
                                    getCRLContinue,
                                    NULL,       /* trustCallback */
                                    (PKIX_PL_Object *)crlSelector, /* fake */
                                    PKIX_FALSE, /* cacheFlag */
                                    PKIX_TRUE,  /* localFlag */
                                    &certStore,
                                    plContext));


        subTest("PKIX_ProcessingParams_AddCertStore");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_AddCertStore
                                    (procParams, certStore, plContext));

        subTest("PKIX_ProcessingParams_SetRevocationEnabled");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetRevocationEnabled
                                    (procParams, PKIX_TRUE, plContext));

        subTest("PKIX_CertStore_GetCRLCallback");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertStore_GetCRLCallback
                                    (certStore,
                                    &crlCallback,
                                    NULL));

        subTest("Getting CRL by CRL Callback");
        PKIX_TEST_EXPECT_NO_ERROR(crlCallback
                                    (certStore,
                                    crlSelector,
                                    &nbioContext,
                                    &crlList,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength
                                    (crlList,
                                    &numCrl,
                                    plContext));

        if (numCrl != PKIX_TEST_COLLECTIONCERTSTORE_NUM_CRLS) {
                pkixTestErrorMsg = "unexpected CRL number mismatch";
        }

cleanup:

        PKIX_TEST_DECREF_AC(crlList);
        PKIX_TEST_DECREF_AC(comCrlSelParams);
        PKIX_TEST_DECREF_AC(crlSelector);
        PKIX_TEST_DECREF_AC(procParams);
        PKIX_TEST_DECREF_AC(certStore);

        PKIX_TEST_RETURN();

        return (0);
}

/*
 * Validate Certificate Chain with Certificate Revocation List
 *      Certificate Chain is built based on input certs' sequence.
 *      CRL is fetched from the directory specified in CollectionCertStore.
 *      while CollectionCertStore is linked in CertStore Object which then
 *      linked in ProcessParam. During validation, CRLChecker will invoke
 *      the crlCallback (this test uses PKIX_PL_CollectionCertStore_GetCRL)
 *      to get CRL data for revocation check.
 *      This test set criteria in CRLSelector which is linked in
 *      CommonCRLSelectorParam. When CRL data is fetched into cache for
 *      revocation check, CRL's are filtered based on the criteria set.
 */

int test_customcrlchecker(int argc, char *argv[]){

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
        char *anchorName = NULL;

        PKIX_TEST_STD_VARS();

        if (argc < 5) {
                printUsage1(argv[0]);
                return (0);
        }

        startTests("CRL Checker");

        PKIX_TEST_EXPECT_NO_ERROR(
            PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

        /* ENE = expect no error; EE = expect error */
        if (PORT_Strcmp(argv[2+j], "ENE") == 0) {
                testValid = PKIX_TRUE;
        } else if (PORT_Strcmp(argv[2+j], "EE") == 0) {
                testValid = PKIX_FALSE;
        } else {
                printUsage1(argv[0]);
                return (0);
        }

        chainLength = (argc - j) - 5;
        if (chainLength > PKIX_TEST_MAX_CERTS) {
                printUsageMax(chainLength);
        }

        for (i = 0; i < chainLength; i++) {

                certNames[i] = argv[(5 + j) +i];
                certs[i] = NULL;
        }

        dirName = argv[3+j];

        subTest(argv[1+j]);

        subTest("Custom-CRL-Checker - Create Cert Chain");

        chain = createCertChainPlus
                (dirName, certNames, certs, chainLength, plContext);

        subTest("Custom-CRL-Checker - Create Params");

        anchorName = argv[4+j];

        valParams = createValidateParams
                (dirName,
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

        subTest("Custom-CRL-Checker - Set Processing Params for CertStore");

        testCustomCertStore(valParams);

        subTest("Custom-CRL-Checker - Validate Chain");

        if (testValid == PKIX_TRUE) {
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateChain
                        (valParams, &valResult, &verifyTree, plContext));
        } else {
                PKIX_TEST_EXPECT_ERROR(PKIX_ValidateChain
                        (valParams, &valResult, &verifyTree, plContext));
        }

cleanup:

        PKIX_TEST_DECREF_AC(verifyString);
        PKIX_TEST_DECREF_AC(verifyTree);
        PKIX_TEST_DECREF_AC(chain);
        PKIX_TEST_DECREF_AC(valParams);
        PKIX_TEST_DECREF_AC(valResult);

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("CRL Checker");

        return (0);
}
