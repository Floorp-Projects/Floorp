/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_validatechain_NB.c
 *
 * Test ValidateChain (nonblocking I/O) function
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;

static
void printUsage(void){
        (void) printf("\nUSAGE:\ntest_validateChain_NB TestName [ENE|EE] "
                    "<certStoreDirectory> <trustedCert> <targetCert>\n\n");
        (void) printf
                ("Validates a chain of certificates between "
                "<trustedCert> and <targetCert>\n"
                "using the certs and CRLs in <certStoreDirectory>. "
                "If ENE is specified,\n"
                "then an Error is Not Expected. "
                "If EE is specified, an Error is Expected.\n");
}

static
char *createFullPathName(
        char *dirName,
        char *certFile,
        void *plContext)
{
        PKIX_UInt32 certFileLen;
        PKIX_UInt32 dirNameLen;
        char *certPathName = NULL;

        PKIX_TEST_STD_VARS();

        certFileLen = PL_strlen(certFile);
        dirNameLen = PL_strlen(dirName);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Malloc
                (dirNameLen + certFileLen + 2,
                (void **)&certPathName,
                plContext));

        PL_strcpy(certPathName, dirName);
        PL_strcat(certPathName, "/");
        PL_strcat(certPathName, certFile);
        printf("certPathName = %s\n", certPathName);

cleanup:

        PKIX_TEST_RETURN();

        return (certPathName);
}

static PKIX_Error *
testSetupCertStore(PKIX_ValidateParams *valParams, char *ldapName)
{
        PKIX_PL_String *dirString = NULL;
        PKIX_CertStore *certStore = NULL;
        PKIX_ProcessingParams *procParams = NULL;
        PKIX_PL_LdapDefaultClient *ldapClient = NULL;

        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_CollectionCertStoreContext_Create");

        /* Create LDAPCertStore */
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_LdapDefaultClient_CreateByName
                (ldapName,
                0,      /* timeout */
                NULL,   /* bindPtr */
                &ldapClient,
                plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_LdapCertStore_Create
                ((PKIX_PL_LdapClient *)ldapClient,
                &certStore,
                plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateParams_GetProcessingParams
                (valParams, &procParams, plContext));

        subTest("PKIX_ProcessingParams_AddCertStore");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_AddCertStore
                (procParams, certStore, plContext));

        subTest("PKIX_ProcessingParams_SetRevocationEnabled");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetRevocationEnabled
                (procParams, PKIX_TRUE, plContext));

cleanup:

        PKIX_TEST_DECREF_AC(dirString);
        PKIX_TEST_DECREF_AC(procParams);
        PKIX_TEST_DECREF_AC(certStore);
        PKIX_TEST_DECREF_AC(ldapClient);

        PKIX_TEST_RETURN();

        return (0);
}

static char *levels[] = {
        "None", "Fatal Error", "Error", "Warning", "Debug", "Trace"
};

static PKIX_Error *loggerCallback(
        PKIX_Logger *logger,
        PKIX_PL_String *message,
        PKIX_UInt32 logLevel,
        PKIX_ERRORCLASS logComponent,
        void *plContext)
{
#define resultSize 150
        char *msg = NULL;
        char result[resultSize];

        PKIX_TEST_STD_VARS();

        msg = PKIX_String2ASCII(message, plContext);
        PR_snprintf(result, resultSize,
            "Logging %s (%s): %s",
	    levels[logLevel],
	    PKIX_ERRORCLASSNAMES[logComponent],
	    msg);
        subTest(result);

cleanup:
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(msg, plContext));
        PKIX_TEST_RETURN();
}

static
void testLogErrors(
	PKIX_ERRORCLASS module,
	PKIX_UInt32 loggingLevel,
        PKIX_List *loggers,
	void *plContext)
{
        PKIX_Logger *logger = NULL;
        PKIX_PL_String *component = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Logger_Create
                (loggerCallback, NULL, &logger, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Logger_SetLoggingComponent
                (logger, module, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Logger_SetMaxLoggingLevel
                (logger, loggingLevel, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (loggers, (PKIX_PL_Object *) logger, plContext));

cleanup:
        PKIX_TEST_DECREF_AC(logger);
        PKIX_TEST_DECREF_AC(component);

        PKIX_TEST_RETURN();
}

int test_validatechain_NB(int argc, char *argv[]){

        PKIX_ValidateParams *valParams = NULL;
        PKIX_ValidateResult *valResult = NULL;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;
        PKIX_UInt32 k = 0;
        PKIX_UInt32 chainLength = 0;
        PKIX_Boolean testValid = PKIX_TRUE;
        PKIX_List *chainCerts = NULL;
        PKIX_PL_Cert *dirCert = NULL;
        char *dirCertName = NULL;
        char *anchorCertName = NULL;
        char *dirName = NULL;
        PKIX_UInt32 certIndex = 0;
        PKIX_UInt32 anchorIndex = 0;
        PKIX_UInt32 checkerIndex = 0;
        PKIX_Boolean revChecking = PKIX_FALSE;
        PKIX_List *checkers = NULL;
        PRPollDesc *pollDesc = NULL;
        PRErrorCode errorCode = 0;
        PKIX_PL_Socket *socket = NULL;
        char *ldapName = NULL;
	PKIX_VerifyNode *verifyTree = NULL;
	PKIX_PL_String *verifyString = NULL;

        PKIX_List *loggers = NULL;
        PKIX_Logger *logger = NULL;
        char *logging = NULL;
        PKIX_PL_String *component = NULL;

        PKIX_TEST_STD_VARS();

        if (argc < 5) {
                printUsage();
                return (0);
        }

        startTests("ValidateChain_NB");

        PKIX_TEST_EXPECT_NO_ERROR(
            PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

        /* ENE = expect no error; EE = expect error */
        if (PORT_Strcmp(argv[2+j], "ENE") == 0) {
                testValid = PKIX_TRUE;
        } else if (PORT_Strcmp(argv[2+j], "EE") == 0) {
                testValid = PKIX_FALSE;
        } else {
                printUsage();
                return (0);
        }

        subTest(argv[1+j]);

        dirName = argv[3+j];

        chainLength = argc - j - 5;

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&chainCerts, plContext));

        for (k = 0; k < chainLength; k++){

                dirCert = createCert(dirName, argv[5+k+j], plContext);

                PKIX_TEST_EXPECT_NO_ERROR
                        (PKIX_List_AppendItem
                        (chainCerts, (PKIX_PL_Object *)dirCert, plContext));

                PKIX_TEST_DECREF_BC(dirCert);
        }

        valParams = createValidateParams
                (dirName,
                argv[4+j],
                NULL,
                NULL,
                NULL,
                PKIX_FALSE,
                PKIX_FALSE,
                PKIX_FALSE,
                PKIX_FALSE,
                chainCerts,
                plContext);

        ldapName = PR_GetEnv("LDAP");
        /* Is LDAP set in the environment? */
        if ((ldapName == NULL) || (*ldapName == '\0')) {
                testError("LDAP not set in environment");
                goto cleanup;
        }

        pkixTestErrorResult = pkix_pl_Socket_CreateByName
                (PKIX_FALSE,       /* isServer */
                PR_SecondsToInterval(30), /* try 30 secs for connect */
                ldapName,
                &errorCode,
                &socket,
                plContext);

        if (pkixTestErrorResult != NULL) {
                PKIX_PL_Object_DecRef
                        ((PKIX_PL_Object *)pkixTestErrorResult, plContext);
                pkixTestErrorResult = NULL;
                testError("Unable to connect to LDAP Server");
                goto cleanup;
        }

        PKIX_TEST_DECREF_BC(socket);

        testSetupCertStore(valParams, ldapName);

        logging = PR_GetEnv("LOGGING");
        /* Is LOGGING set in the environment? */
        if ((logging != NULL) && (*logging != '\0')) {

                PKIX_TEST_EXPECT_NO_ERROR
                        (PKIX_List_Create(&loggers, plContext));

		testLogErrors
			(PKIX_VALIDATE_ERROR, 2, loggers, plContext);
		testLogErrors
			(PKIX_CERTCHAINCHECKER_ERROR, 2, loggers, plContext);
		testLogErrors
			(PKIX_LDAPDEFAULTCLIENT_ERROR, 2, loggers, plContext);
		testLogErrors
			(PKIX_CERTSTORE_ERROR, 2, loggers, plContext);

                PKIX_TEST_EXPECT_NO_ERROR(PKIX_SetLoggers(loggers, plContext));

        }

        pkixTestErrorResult = PKIX_ValidateChain_NB
                (valParams,
                &certIndex,
                &anchorIndex,
                &checkerIndex,
                &revChecking,
                &checkers,
                (void **)&pollDesc,
                &valResult,
		&verifyTree,
                plContext);

        while (pollDesc != NULL) {

                if (PR_Poll(pollDesc, 1, 0) < 0) {
                        testError("PR_Poll failed");
                }

                pkixTestErrorResult = PKIX_ValidateChain_NB
                        (valParams,
                        &certIndex,
                        &anchorIndex,
                        &checkerIndex,
                        &revChecking,
                        &checkers,
                        (void **)&pollDesc,
                        &valResult,
			&verifyTree,
                        plContext);
        }

        if (pkixTestErrorResult) {
                if (testValid == PKIX_FALSE) { /* EE */
                        (void) printf("EXPECTED ERROR RECEIVED!\n");
                } else { /* ENE */
                        testError("UNEXPECTED ERROR RECEIVED");
                }
                PKIX_TEST_DECREF_BC(pkixTestErrorResult);
        } else {

	        if (testValid == PKIX_TRUE) { /* ENE */
        	        (void) printf("EXPECTED NON-ERROR RECEIVED!\n");
	        } else { /* EE */
        	        (void) printf("UNEXPECTED NON-ERROR RECEIVED!\n");
	        }
        }

cleanup:

	if (verifyTree) {
	        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString
        	    ((PKIX_PL_Object*)verifyTree, &verifyString, plContext));
	        (void) printf("verifyTree is\n%s\n",
		    verifyString->escAsciiString);
	}

        PKIX_TEST_DECREF_AC(verifyString);
        PKIX_TEST_DECREF_AC(verifyTree);
        PKIX_TEST_DECREF_AC(checkers);
        PKIX_TEST_DECREF_AC(chainCerts);
        PKIX_TEST_DECREF_AC(valParams);
        PKIX_TEST_DECREF_AC(valResult);

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("ValidateChain_NB");

        return (0);
}
