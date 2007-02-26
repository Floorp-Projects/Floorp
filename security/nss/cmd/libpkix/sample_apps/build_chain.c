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
 * buildChain.c
 *
 * Tests Cert Chain Building
 *
 */

#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include "pkix_pl_generalname.h"
#include "pkix_pl_cert.h"
#include "pkix.h"
#include "testutil.h"
#include "prlong.h"
#include "plstr.h"
#include "prthread.h"
#include "nspr.h"
#include "prtypes.h"
#include "prtime.h"
#include "pk11func.h"
#include "secasn1.h"
#include "cert.h"
#include "cryptohi.h"
#include "secoid.h"
#include "certdb.h"
#include "secitem.h"
#include "keythi.h"
#include "nss.h"

void *plContext = NULL;

void printUsage(void){
        (void) printf("\nUSAGE:\tbuildChain "
                        "<trustedCert> <targetCert> <certStoreDirectory>\n\n");
        (void) printf
                ("Builds a chain of certificates between "
                "<trustedCert> and <targetCert>\n"
                "using the certs and CRLs in <certStoreDirectory>.\n");
}

PKIX_PL_Cert *
createCert(char *inFileName)
{
        PKIX_PL_ByteArray *byteArray = NULL;
        void *buf = NULL;
        PRFileDesc *inFile = NULL;
        PKIX_UInt32 len;
        SECItem certDER;
        SECStatus rv;
        /* default: NULL cert (failure case) */
        PKIX_PL_Cert *cert = NULL;

        PKIX_TEST_STD_VARS();

        certDER.data = NULL;

        inFile = PR_Open(inFileName, PR_RDONLY, 0);

        if (!inFile){
                pkixTestErrorMsg = "Unable to open cert file";
                goto cleanup;
        } else {
                rv = SECU_ReadDERFromFile(&certDER, inFile, PR_FALSE);
                if (!rv){
                        buf = (void *)certDER.data;
                        len = certDER.len;

                        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_ByteArray_Create
                                        (buf, len, &byteArray, plContext));

                        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_Create
                                        (byteArray, &cert, plContext));

                        SECITEM_FreeItem(&certDER, PR_FALSE);
                } else {
                        pkixTestErrorMsg = "Unable to read DER from cert file";
                        goto cleanup;
                }
        }

cleanup:

        if (inFile){
                PR_Close(inFile);
        }

        if (PKIX_TEST_ERROR_RECEIVED){
                SECITEM_FreeItem(&certDER, PR_FALSE);
        }

        PKIX_TEST_DECREF_AC(byteArray);

        PKIX_TEST_RETURN();

        return (cert);
}

int main(int argc, char *argv[])
{
        PKIX_BuildResult *buildResult = NULL;
        PKIX_ComCertSelParams *certSelParams = NULL;
        PKIX_CertSelector *certSelector = NULL;
        PKIX_TrustAnchor *anchor = NULL;
        PKIX_List *anchors = NULL;
        PKIX_List *certs = NULL;
        PKIX_PL_Cert *cert = NULL;
        PKIX_ProcessingParams *procParams = NULL;
        char *trustedCertFile = NULL;
        char *targetCertFile = NULL;
        char *storeDirAscii = NULL;
        PKIX_PL_String *storeDirString = NULL;
        PKIX_PL_Cert *trustedCert = NULL;
        PKIX_PL_Cert *targetCert = NULL;
        PKIX_UInt32 actualMinorVersion, numCerts, i;
        PKIX_UInt32 j = 0;
        PKIX_CertStore *certStore = NULL;
        PKIX_List *certStores = NULL;
        char * asciiResult = NULL;
        PKIX_Boolean useArenas = PKIX_FALSE;
        void *buildState = NULL; /* needed by pkix_build for non-blocking I/O */
        void *nbioContext = NULL;

        PKIX_TEST_STD_VARS();

        if (argc < 4){
                printUsage();
                return (0);
        }

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                                    (PKIX_TRUE, /* nssInitNeeded */
                                    useArenas,
                                    PKIX_MAJOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    &actualMinorVersion,
                                    &plContext));

        /* create processing params with list of trust anchors */
        trustedCertFile = argv[j+1];
        trustedCert = createCert(trustedCertFile);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_TrustAnchor_CreateWithCert
                                    (trustedCert, &anchor, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&anchors, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                            (anchors, (PKIX_PL_Object *)anchor, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_Create
                            (anchors, &procParams, plContext));


        /* create CertSelector with target certificate in params */
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ComCertSelParams_Create(&certSelParams, plContext));

        targetCertFile = argv[j+2];
        targetCert = createCert(targetCertFile);

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ComCertSelParams_SetCertificate
                (certSelParams, targetCert, plContext));

        PKIX_TEST_EXPECT_NO_ERROR
            (PKIX_CertSelector_Create(NULL, NULL, &certSelector, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_SetCommonCertSelectorParams
                                (certSelector, certSelParams, plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_ProcessingParams_SetTargetCertConstraints
                (procParams, certSelector, plContext));

        /* create CertStores */

        storeDirAscii = argv[j+3];

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                (PKIX_ESCASCII, storeDirAscii, 0, &storeDirString, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CollectionCertStore_Create
                (storeDirString, &certStore, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&certStores, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (certStores, (PKIX_PL_Object *)certStore, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetCertStores
                (procParams, certStores, plContext));

        /* build cert chain using processing params and return buildResult */

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_BuildChain
                (procParams,
                &nbioContext,
                &buildState,
                &buildResult,
                NULL,
                plContext));

        /*
         * As long as we use only CertStores with blocking I/O, we can omit
         * checking for completion with nbioContext.
         */

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_BuildResult_GetCertChain(buildResult, &certs, plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_GetLength(certs, &numCerts, plContext));

        printf("\n");

        for (i = 0; i < numCerts; i++){
                PKIX_TEST_EXPECT_NO_ERROR
                        (PKIX_List_GetItem
                        (certs, i, (PKIX_PL_Object**)&cert, plContext));

                asciiResult = PKIX_Cert2ASCII(cert);

                printf("CERT[%d]:\n%s\n", i, asciiResult);

                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(asciiResult, plContext));
                asciiResult = NULL;

                PKIX_TEST_DECREF_BC(cert);
        }

cleanup:

        if (PKIX_TEST_ERROR_RECEIVED){
                (void) printf("FAILED TO BUILD CHAIN\n");
        } else {
                (void) printf("SUCCESSFULLY BUILT CHAIN\n");
        }

        PKIX_PL_Free(asciiResult, plContext);

        PKIX_TEST_DECREF_AC(certs);
        PKIX_TEST_DECREF_AC(cert);
        PKIX_TEST_DECREF_AC(certStore);
        PKIX_TEST_DECREF_AC(certStores);
        PKIX_TEST_DECREF_AC(storeDirString);
        PKIX_TEST_DECREF_AC(trustedCert);
        PKIX_TEST_DECREF_AC(targetCert);
        PKIX_TEST_DECREF_AC(anchor);
        PKIX_TEST_DECREF_AC(anchors);
        PKIX_TEST_DECREF_AC(procParams);
        PKIX_TEST_DECREF_AC(certSelParams);
        PKIX_TEST_DECREF_AC(certSelector);
        PKIX_TEST_DECREF_AC(buildResult);

        PKIX_TEST_RETURN();

        PKIX_Shutdown(plContext);

        return (0);

}
