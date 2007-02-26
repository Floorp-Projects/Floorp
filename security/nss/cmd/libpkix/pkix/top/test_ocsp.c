/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Sun Microsystems
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
/*
 * test_ocspchecker.c
 *
 * Test OcspChecker function
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

void *plContext = NULL;

void printUsage(void){
        (void) printf("\nUSAGE:\nOcspChecker TestName [ENE|EE] "
                    "<certStoreDirectory> <trustedCert> <targetCert>\n\n");
        (void) printf
                ("Validates a chain of certificates between "
                "<trustedCert> and <targetCert>\n"
                "using the certs and CRLs in <certStoreDirectory>. "
                "If ENE is specified,\n"
                "then an Error is Not Expected. "
                "If EE is specified, an Error is Expected.\n");
}

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

PKIX_Error *
testDefaultCertStore(PKIX_ValidateParams *valParams, char *crlDir)
{
        PKIX_PL_String *dirString = NULL;
        PKIX_CertStore *certStore = NULL;
        PKIX_ProcessingParams *procParams = NULL;
        PKIX_PL_Date *validity = NULL; 
        PKIX_List *revCheckers = NULL;
        PKIX_RevocationChecker *revChecker = NULL;
	PKIX_PL_Object *revCheckerContext = NULL;
        PKIX_OcspChecker *ocspChecker = NULL;

        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_CollectionCertStoreContext_Create");

        /* Create CollectionCertStore */

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                (PKIX_ESCASCII, crlDir, 0, &dirString, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CollectionCertStore_Create
                (dirString, &certStore, plContext));

        /* Create CertStore */

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateParams_GetProcessingParams
                (valParams, &procParams, plContext));

        subTest("PKIX_ProcessingParams_AddCertStore");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_AddCertStore
                (procParams, certStore, plContext));

        subTest("PKIX_ProcessingParams_SetRevocationEnabled");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetRevocationEnabled
                (procParams, PKIX_FALSE, plContext));

        /* create current Date */
        PKIX_TEST_EXPECT_NO_ERROR(pkix_pl_Date_CreateFromPRTime
                (PR_Now(), &validity, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&revCheckers, plContext));

        /* create revChecker */
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_OcspChecker_Initialize
                (validity,
                NULL,        /* pwArg */
                NULL,        /* Use default responder */
                &revChecker,
                plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_RevocationChecker_GetRevCheckerContext
                (revChecker, &revCheckerContext, plContext));

        /* Check that this object is a ocsp checker */
        PKIX_TEST_EXPECT_NO_ERROR(pkix_CheckType
                    (revCheckerContext, PKIX_OCSPCHECKER_TYPE, plContext));

        ocspChecker = (PKIX_OcspChecker *)revCheckerContext;

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_OcspChecker_SetVerifyFcn
                (ocspChecker,
                PKIX_PL_OcspResponse_UseBuildChain,
                plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (revCheckers, (PKIX_PL_Object *)revChecker, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetRevocationCheckers
                (procParams, revCheckers, plContext));

cleanup:

        PKIX_TEST_DECREF_AC(dirString);
        PKIX_TEST_DECREF_AC(procParams);
        PKIX_TEST_DECREF_AC(certStore);
        PKIX_TEST_DECREF_AC(revCheckers);
        PKIX_TEST_DECREF_AC(revChecker);
        PKIX_TEST_DECREF_AC(ocspChecker);
        PKIX_TEST_DECREF_AC(validity);

        PKIX_TEST_RETURN();

        return (0);
}

int main(int argc, char *argv[]){

        PKIX_ValidateParams *valParams = NULL;
        PKIX_ProcessingParams *procParams = NULL;
        PKIX_ComCertSelParams *certSelParams = NULL;
        PKIX_CertSelector *certSelector = NULL;
        PKIX_ValidateResult *valResult = NULL;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;
        PKIX_UInt32 k = 0;
        PKIX_UInt32 chainLength = 0;
        PKIX_Boolean testValid = PKIX_TRUE;
        PKIX_Boolean useArenas = PKIX_FALSE;
        PKIX_List *chainCerts = NULL;
	PKIX_VerifyNode *verifyTree = NULL;
	PKIX_PL_String *verifyString = NULL;
        PKIX_PL_Cert *dirCert = NULL;
        PKIX_PL_Cert *trustedCert = NULL;
        PKIX_PL_Cert *targetCert = NULL;
        PKIX_TrustAnchor *anchor = NULL;
        PKIX_List *anchors = NULL;
        char *dirCertName = NULL;
        char *anchorCertName = NULL;
        char *dirName = NULL;
        char *databaseDir = NULL;

        PKIX_TEST_STD_VARS();

        if (argc < 5) {
                printUsage();
                return (0);
        }

        startTests("OcspChecker");

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        databaseDir = argv[3+j];

        /* This must precede the call to PKIX_Initialize! */
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize_SetConfigDir
            (PKIX_STORE_TYPE_PK11, databaseDir, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                (PKIX_TRUE, /* nssInitNeeded */
                useArenas,
                PKIX_MAJOR_VERSION,
                PKIX_MINOR_VERSION,
                PKIX_MINOR_VERSION,
                &actualMinorVersion,
                &plContext));

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

        dirName = databaseDir;

        chainLength = argc - j - 5;

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&chainCerts, plContext));

        for (k = 0; k < chainLength; k++) {

                dirCert = createCert(dirName, argv[5+k+j], plContext);

                if (k == 0) {
                        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_IncRef
                                ((PKIX_PL_Object *)dirCert, plContext));
                        targetCert = dirCert;
                }

                PKIX_TEST_EXPECT_NO_ERROR
                        (PKIX_List_AppendItem
                        (chainCerts, (PKIX_PL_Object *)dirCert, plContext));

                PKIX_TEST_DECREF_BC(dirCert);
        }

        /* create processing params with list of trust anchors */

        anchorCertName = argv[4+j];
        trustedCert = createCert(dirName, anchorCertName, plContext);

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

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateParams_Create
                (procParams, chainCerts, &valParams, plContext));

        testDefaultCertStore(valParams, dirName);

        pkixTestErrorResult = PKIX_ValidateChain
                (valParams, &valResult, &verifyTree, plContext);


        if (pkixTestErrorResult) {
                if (testValid == PKIX_FALSE) { /* EE */
                        (void) printf("EXPECTED ERROR RECEIVED!\n");
                } else { /* ENE */
                        testError("UNEXPECTED ERROR RECEIVED");
                }
                PKIX_TEST_DECREF_BC(pkixTestErrorResult);
        } else {
	        if (testValid == PKIX_TRUE) { /* ENE */
        	        (void) printf("EXPECTED SUCCESSFUL VALIDATION!\n");
	        } else { /* EE */
        	        (void) printf("UNEXPECTED SUCCESSFUL VALIDATION!\n");
	        }
	}

        subTest("Displaying VerifyTree");

	if (verifyTree == NULL) {
                (void) printf("VerifyTree is NULL\n");
	} else {
	        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString
        	    ((PKIX_PL_Object *)verifyTree, &verifyString, plContext));
                (void) printf("verifyTree is\n%s\n",
                    verifyString->escAsciiString);
                PKIX_TEST_DECREF_BC(verifyString);
                PKIX_TEST_DECREF_BC(verifyTree);
	}

cleanup:

        PKIX_TEST_DECREF_AC(valParams);
        PKIX_TEST_DECREF_AC(procParams);
        PKIX_TEST_DECREF_AC(certSelParams);
        PKIX_TEST_DECREF_AC(certSelector);
        PKIX_TEST_DECREF_AC(chainCerts);
        PKIX_TEST_DECREF_AC(anchors);
        PKIX_TEST_DECREF_AC(anchor);
        PKIX_TEST_DECREF_AC(trustedCert);
        PKIX_TEST_DECREF_AC(targetCert);
        PKIX_TEST_DECREF_AC(valResult);

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("OcspChecker");

        return (0);
}
