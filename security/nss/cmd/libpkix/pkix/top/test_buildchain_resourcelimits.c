/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_buildchain_resourcelimits.c
 *
 * Test BuildChain function with constraints on resources
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

#define PKIX_TESTUSERCHECKER_TYPE (PKIX_NUMTYPES+30)

static void *plContext = NULL;
static PKIX_Boolean usebind = PKIX_FALSE;
static PKIX_Boolean useLDAP = PKIX_FALSE;
static char buf[PR_NETDB_BUF_SIZE];
static char *serverName = NULL;

static void printUsage(void) {
    (void) printf("\nUSAGE:\ttest_buildchain_resourcelimits [-arenas] "
        "[usebind] servername[:port]\\\n\t\t<testName> [ENE|EE]"
        " <certStoreDirectory>\\\n\t\t<targetCert>"
        " <intermediate Certs...> <trustedCert>\n\n");
    (void) printf
        ("Builds a chain of certificates from <targetCert> to <trustedCert>\n"
        "using the certs and CRLs in <certStoreDirectory>. "
        "servername[:port] gives\n"
        "the address of an LDAP server. If port is not"
        " specified, port 389 is used.\n\"-\" means no LDAP server.\n\n"
        "If ENE is specified, then an Error is Not Expected.\n"
        "EE indicates an Error is Expected.\n");
}

static PKIX_Error *
createLdapCertStore(
        char *hostname,
        PRIntervalTime timeout,
        PKIX_CertStore **pLdapCertStore,
        void* plContext)
{
        PRIntn backlog = 0;

        char *bindname = "";
        char *auth = "";

        LDAPBindAPI bindAPI;
        LDAPBindAPI *bindPtr = NULL;
        PKIX_PL_LdapDefaultClient *ldapClient = NULL;
        PKIX_CertStore *ldapCertStore = NULL;

        PKIX_TEST_STD_VARS();

        if (usebind) {
                bindPtr = &bindAPI;
                bindAPI.selector = SIMPLE_AUTH;
                bindAPI.chooser.simple.bindName = bindname;
                bindAPI.chooser.simple.authentication = auth;
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_LdapDefaultClient_CreateByName
                (hostname, timeout, bindPtr, &ldapClient, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_LdapCertStore_Create
                ((PKIX_PL_LdapClient *)ldapClient, &ldapCertStore, plContext));

        *pLdapCertStore = ldapCertStore;
cleanup:

        PKIX_TEST_DECREF_AC(ldapClient);

        PKIX_TEST_RETURN();

        return (pkixTestErrorResult);

}

static void Test_BuildResult(
        PKIX_ProcessingParams *procParams,
        PKIX_Boolean testValid,
        PKIX_List *expectedCerts,
        void *plContext)
{
        PKIX_PL_Cert *cert = NULL;
        PKIX_List *certs = NULL;
        PKIX_PL_String *actualCertsString = NULL;
        PKIX_PL_String *expectedCertsString = NULL;
        PKIX_BuildResult *buildResult = NULL;
        PKIX_Boolean result;
        PKIX_Boolean supportForward = PKIX_FALSE;
        PKIX_UInt32 numCerts, i;
        char *asciiResult = NULL;
        char *actualCertsAscii = NULL;
        char *expectedCertsAscii = NULL;
        void *state = NULL;
        PRPollDesc *pollDesc = NULL;

        PKIX_TEST_STD_VARS();

        pkixTestErrorResult = PKIX_BuildChain
                (procParams,
                (void **)&pollDesc,
                &state,
                &buildResult,
                NULL,
                plContext);

        while (pollDesc != NULL) {

                if (PR_Poll(pollDesc, 1, 0) < 0) {
                        testError("PR_Poll failed");
                }

                pkixTestErrorResult = PKIX_BuildChain
                        (procParams,
                        (void **)&pollDesc,
                        &state,
                        &buildResult,
                        NULL,
                        plContext);
        }

        if (pkixTestErrorResult) {
                if (testValid == PKIX_FALSE) { /* EE */
                        (void) printf("EXPECTED ERROR RECEIVED!\n");
                } else { /* ENE */
                        testError("UNEXPECTED ERROR RECEIVED!\n");
                }
                PKIX_TEST_DECREF_BC(pkixTestErrorResult);
                goto cleanup;
        }

        if (testValid == PKIX_TRUE) { /* ENE */
                (void) printf("EXPECTED NON-ERROR RECEIVED!\n");
        } else { /* EE */
                testError("UNEXPECTED NON-ERROR RECEIVED!\n");
        }

        if (buildResult){

                PKIX_TEST_EXPECT_NO_ERROR
                        (PKIX_BuildResult_GetCertChain
                        (buildResult, &certs, NULL));

                PKIX_TEST_EXPECT_NO_ERROR
                        (PKIX_List_GetLength(certs, &numCerts, plContext));

                printf("\n");

                for (i = 0; i < numCerts; i++){
                        PKIX_TEST_EXPECT_NO_ERROR
                                (PKIX_List_GetItem
                                (certs,
                                i,
                                (PKIX_PL_Object**)&cert,
                                plContext));

                        asciiResult = PKIX_Cert2ASCII(cert);

                        printf("CERT[%d]:\n%s\n", i, asciiResult);

                        /* PKIX_Cert2ASCII used PKIX_PL_Malloc(...,,NULL) */
                        PKIX_TEST_EXPECT_NO_ERROR
                                (PKIX_PL_Free(asciiResult, NULL));
                        asciiResult = NULL;

                        PKIX_TEST_DECREF_BC(cert);
                }

                PKIX_TEST_EXPECT_NO_ERROR
                        (PKIX_PL_Object_Equals
                        ((PKIX_PL_Object*)certs,
                        (PKIX_PL_Object*)expectedCerts,
                        &result,
                        plContext));

                if (!result){
                        testError("BUILT CERTCHAIN IS "
                                    "NOT THE ONE THAT WAS EXPECTED");

                        PKIX_TEST_EXPECT_NO_ERROR
                                (PKIX_PL_Object_ToString
                                ((PKIX_PL_Object *)certs,
                                &actualCertsString,
                                plContext));

                        actualCertsAscii = PKIX_String2ASCII
                                (actualCertsString, plContext);
                        if (actualCertsAscii == NULL){
                                pkixTestErrorMsg = "PKIX_String2ASCII Failed";
                                goto cleanup;
                        }

                        PKIX_TEST_EXPECT_NO_ERROR
                                (PKIX_PL_Object_ToString
                                ((PKIX_PL_Object *)expectedCerts,
                                &expectedCertsString,
                                plContext));

                        expectedCertsAscii = PKIX_String2ASCII
                                (expectedCertsString, plContext);
                        if (expectedCertsAscii == NULL){
                                pkixTestErrorMsg = "PKIX_String2ASCII Failed";
                                goto cleanup;
                        }

                        (void) printf("Actual value:\t%s\n", actualCertsAscii);
                        (void) printf("Expected value:\t%s\n",
                                        expectedCertsAscii);
                }

        }

cleanup:

        PKIX_PL_Free(asciiResult, NULL);
        PKIX_PL_Free(actualCertsAscii, plContext);
        PKIX_PL_Free(expectedCertsAscii, plContext);
        PKIX_TEST_DECREF_AC(state);
        PKIX_TEST_DECREF_AC(buildResult);
        PKIX_TEST_DECREF_AC(certs);
        PKIX_TEST_DECREF_AC(cert);
        PKIX_TEST_DECREF_AC(actualCertsString);
        PKIX_TEST_DECREF_AC(expectedCertsString);

        PKIX_TEST_RETURN();

}

int test_buildchain_resourcelimits(int argc, char *argv[])
{
        PKIX_ComCertSelParams *certSelParams = NULL;
        PKIX_CertSelector *certSelector = NULL;
        PKIX_TrustAnchor *anchor = NULL;
        PKIX_List *anchors = NULL;
        PKIX_ProcessingParams *procParams = NULL;
        PKIX_CertChainChecker *checker = NULL;
        PKIX_ResourceLimits *resourceLimits = NULL;
        char *dirName = NULL;
        PKIX_PL_String *dirNameString = NULL;
        PKIX_PL_Cert *trustedCert = NULL;
        PKIX_PL_Cert *targetCert = NULL;
        PKIX_PL_Cert *dirCert = NULL;
        PKIX_UInt32 actualMinorVersion = 0;
        PKIX_UInt32 j = 0;
        PKIX_UInt32 k = 0;
        PKIX_CertStore *ldapCertStore = NULL;
        PRIntervalTime timeout = 0; /* 0 for non-blocking */
        PKIX_CertStore *certStore = NULL;
        PKIX_List *certStores = NULL;
        PKIX_List *expectedCerts = NULL;
        PKIX_Boolean testValid = PKIX_FALSE;
        PKIX_Boolean usebind = PKIX_FALSE;
        PKIX_Boolean useLDAP = PKIX_FALSE;

        PKIX_TEST_STD_VARS();

        if (argc < 5){
                printUsage();
                return (0);
        }

        startTests("BuildChain_ResourceLimits");

        PKIX_TEST_EXPECT_NO_ERROR(
            PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

        /*
         * arguments:
         * [optional] -arenas
         * [optional] usebind
         *            servername or servername:port ( - for no server)
         *            testname
         *            EE or ENE
         *            cert directory
         *            target cert (end entity)
         *            intermediate certs
         *            trust anchor
         */

        /* optional argument "usebind" for Ldap CertStore */
        if (argv[j + 1]) {
                if (PORT_Strcmp(argv[j + 1], "usebind") == 0) {
                        usebind = PKIX_TRUE;
                        j++;
                }
        }

        if (PORT_Strcmp(argv[++j], "-") == 0) {
                useLDAP = PKIX_FALSE;
        } else {
                serverName = argv[j];
        }

        subTest(argv[++j]);

        /* ENE = expect no error; EE = expect error */
        if (PORT_Strcmp(argv[++j], "ENE") == 0) {
                testValid = PKIX_TRUE;
        } else if (PORT_Strcmp(argv[j], "EE") == 0) {
                testValid = PKIX_FALSE;
        } else {
                printUsage();
                return (0);
        }

        dirName = argv[++j];

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&expectedCerts, plContext));

        for (k = ++j; k < argc; k++) {

                dirCert = createCert(dirName, argv[k], plContext);

                if (k == (argc - 1)) {
                        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_IncRef
                                ((PKIX_PL_Object *)dirCert, plContext));
                        trustedCert = dirCert;
                } else {

                        PKIX_TEST_EXPECT_NO_ERROR
                                (PKIX_List_AppendItem
                                (expectedCerts,
                                (PKIX_PL_Object *)dirCert,
                                plContext));

                        if (k == j) {
                                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_IncRef
                                        ((PKIX_PL_Object *)dirCert, plContext));
                                targetCert = dirCert;
                        }
                }

                PKIX_TEST_DECREF_BC(dirCert);
        }

        /* create processing params with list of trust anchors */

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_TrustAnchor_CreateWithCert
                                    (trustedCert, &anchor, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&anchors, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_AppendItem
                (anchors, (PKIX_PL_Object *)anchor, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_Create
                                    (anchors, &procParams, plContext));

        /* create CertSelector with target certificate in params */

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ComCertSelParams_Create(&certSelParams, plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ComCertSelParams_SetCertificate
                (certSelParams, targetCert, plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_Create
                (NULL, NULL, &certSelector, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_SetCommonCertSelectorParams
                                (certSelector, certSelParams, plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ProcessingParams_SetTargetCertConstraints
                (procParams, certSelector, plContext));

        /* create CertStores */

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                                    (PKIX_ESCASCII,
                                    dirName,
                                    0,
                                    &dirNameString,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CollectionCertStore_Create
                                    (dirNameString, &certStore, plContext));

#if 0
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Pk11CertStore_Create
                                    (&certStore, plContext));
#endif


        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&certStores, plContext));

        if (useLDAP == PKIX_TRUE) {
                PKIX_TEST_EXPECT_NO_ERROR(createLdapCertStore
                        (serverName, timeout, &ldapCertStore, plContext));

                PKIX_TEST_EXPECT_NO_ERROR
                        (PKIX_List_AppendItem
                        (certStores,
                        (PKIX_PL_Object *)ldapCertStore,
                        plContext));
        }

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_AppendItem
                (certStores, (PKIX_PL_Object *)certStore, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetCertStores
                                    (procParams, certStores, plContext));

        /* set resource limits */

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_Create
                (&resourceLimits, plContext));

        /* need longer time when running dbx for memory leak checking */
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxTime
                (resourceLimits, 60, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxFanout
                (resourceLimits, 2, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxDepth
                (resourceLimits, 2, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetResourceLimits
                                    (procParams, resourceLimits, plContext));

        /* build cert chain using processing params and return buildResult */

        subTest("Testing ResourceLimits MaxFanout & MaxDepth - <pass>");
        Test_BuildResult
                (procParams,
                testValid,
                expectedCerts,
                plContext);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxFanout
                (resourceLimits, 1, plContext));

        subTest("Testing ResourceLimits MaxFanout - <fail>");
        Test_BuildResult
                (procParams,
                PKIX_FALSE,
                expectedCerts,
                plContext);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxFanout
                (resourceLimits, 2, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxDepth
                (resourceLimits, 1, plContext));

        subTest("Testing ResourceLimits MaxDepth - <fail>");
        Test_BuildResult
                (procParams,
                PKIX_FALSE,
                expectedCerts,
                plContext);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxFanout
                (resourceLimits, 0, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxDepth
                (resourceLimits, 0, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxTime
                (resourceLimits, 0, plContext));

        subTest("Testing ResourceLimits No checking - <pass>");
        Test_BuildResult
                (procParams,
                testValid,
                expectedCerts,
                plContext);

cleanup:

        PKIX_TEST_DECREF_AC(expectedCerts);
        PKIX_TEST_DECREF_AC(procParams);
        PKIX_TEST_DECREF_AC(procParams);
        PKIX_TEST_DECREF_AC(certStores);
        PKIX_TEST_DECREF_AC(certStore);
        PKIX_TEST_DECREF_AC(ldapCertStore);
        PKIX_TEST_DECREF_AC(dirNameString);
        PKIX_TEST_DECREF_AC(trustedCert);
        PKIX_TEST_DECREF_AC(targetCert);
        PKIX_TEST_DECREF_AC(anchors);
        PKIX_TEST_DECREF_AC(anchor);
        PKIX_TEST_DECREF_AC(certSelParams);
        PKIX_TEST_DECREF_AC(certSelector);
        PKIX_TEST_DECREF_AC(checker);
        PKIX_TEST_DECREF_AC(resourceLimits);

        PKIX_TEST_RETURN();

        PKIX_Shutdown(plContext);

        endTests("BuildChain_UserChecker");

        return (0);

}
