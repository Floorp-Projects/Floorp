/*
 * test_httpcertstore.c
 *
 * Test Httpcertstore Type
 *
 * Copyright 2004-2005 Sun Microsystems, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistribution of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistribution in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * Neither the name of Sun Microsystems, Inc. or the names of contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * This software is provided "AS IS," without a warranty of any kind. ALL
 * EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES, INCLUDING
 * ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
 * OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED. SUN MICROSYSTEMS, INC. ("SUN")
 * AND ITS LICENSORS SHALL NOT BE LIABLE FOR ANY DAMAGES SUFFERED BY LICENSEE
 * AS A RESULT OF USING, MODIFYING OR DISTRIBUTING THIS SOFTWARE OR ITS
 * DERIVATIVES. IN NO EVENT WILL SUN OR ITS LICENSORS BE LIABLE FOR ANY LOST
 * REVENUE, PROFIT OR DATA, OR FOR DIRECT, INDIRECT, SPECIAL, CONSEQUENTIAL,
 * INCIDENTAL OR PUNITIVE DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE THEORY
 * OF LIABILITY, ARISING OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE,
 * EVEN IF SUN HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 *
 * You acknowledge that this software is not designed or intended for use in
 * the design, construction, operation or maintenance of any nuclear facility.
 */

#include "testutil.h"
#include "testutil_nss.h"
#include "pkix_pl_common.h"

static void *plContext = NULL;

static void
printUsage(char *testname)
{
    char *fmt =
        "USAGE: %s [-arenas] certDir certName\n";
    printf(fmt, "test_httpcertstore");
}

/* Functional tests for Socket public functions */
static void
do_other_work(void)
{ /* while waiting for nonblocking I/O to complete */
    (void)PR_Sleep(2 * 60);
}

PKIX_Error *
PKIX_PL_HttpCertStore_Create(
    PKIX_PL_HttpClient *client, /* if NULL, use default Client */
    PKIX_PL_GeneralName *location,
    PKIX_CertStore **pCertStore,
    void *plContext);

PKIX_Error *
pkix_pl_HttpCertStore_CreateWithAsciiName(
    PKIX_PL_HttpClient *client, /* if NULL, use default Client */
    char *location,
    PKIX_CertStore **pCertStore,
    void *plContext);

static PKIX_Error *
getLocation(
    PKIX_PL_Cert *certWithAia,
    PKIX_PL_GeneralName **pLocation,
    void *plContext)
{
    PKIX_List *aiaList = NULL;
    PKIX_UInt32 size = 0;
    PKIX_PL_InfoAccess *aia = NULL;
    PKIX_UInt32 iaType = PKIX_INFOACCESS_LOCATION_UNKNOWN;
    PKIX_PL_GeneralName *location = NULL;

    PKIX_TEST_STD_VARS();

    subTest("Getting Authority Info Access");

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetAuthorityInfoAccess(certWithAia, &aiaList, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength(aiaList, &size, plContext));

    if (size != 1) {
        pkixTestErrorMsg = "unexpected number of AIA";
        goto cleanup;
    }

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem(aiaList, 0, (PKIX_PL_Object **)&aia, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_InfoAccess_GetLocationType(aia, &iaType, plContext));

    if (iaType != PKIX_INFOACCESS_LOCATION_HTTP) {
        pkixTestErrorMsg = "unexpected location type in AIA";
        goto cleanup;
    }

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_InfoAccess_GetLocation(aia, &location, plContext));

    *pLocation = location;

cleanup:
    PKIX_TEST_DECREF_AC(aiaList);
    PKIX_TEST_DECREF_AC(aia);

    PKIX_TEST_RETURN();

    return (NULL);
}

int
test_httpcertstore(int argc, char *argv[])
{

    PKIX_UInt32 i = 0;
    PKIX_UInt32 numCerts = 0;
    PKIX_UInt32 numCrls = 0;
    int j = 0;
    PKIX_UInt32 actualMinorVersion;
    PKIX_UInt32 length = 0;

    char *certName = NULL;
    char *certDir = NULL;
    PKIX_PL_Cert *cmdLineCert = NULL;
    PKIX_PL_Cert *cert = NULL;
    PKIX_CertSelector *certSelector = NULL;
    PKIX_CertStore *certStore = NULL;
    PKIX_CertStore *crlStore = NULL;
    PKIX_PL_GeneralName *location = NULL;
    PKIX_CertStore_CertCallback getCerts = NULL;
    PKIX_List *certs = NULL;
    char *asciiResult = NULL;
    void *nbio = NULL;

    PKIX_PL_CRL *crl = NULL;
    PKIX_CRLSelector *crlSelector = NULL;
    char *crlLocation = "http://betty.nist.gov/pathdiscoverytestsuite/CRL"
                        "files/BasicHTTPURIPeer2CACRL.crl";
    PKIX_CertStore_CRLCallback getCrls = NULL;
    PKIX_List *crls = NULL;
    PKIX_PL_String *crlString = NULL;

    PKIX_TEST_STD_VARS();

    startTests("HttpCertStore");

    PKIX_TEST_EXPECT_NO_ERROR(
        PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

    if (argc != (j + 3)) {
        printUsage(argv[0]);
        pkixTestErrorMsg = "Missing command line argument.";
        goto cleanup;
    }

    certDir = argv[++j];
    certName = argv[++j];

    cmdLineCert = createCert(certDir, certName, plContext);
    if (cmdLineCert == NULL) {
        pkixTestErrorMsg = "Unable to create Cert";
        goto cleanup;
    }

    /* muster arguments to create HttpCertStore */
    PKIX_TEST_EXPECT_NO_ERROR(getLocation(cmdLineCert, &location, plContext));

    if (location == NULL) {
        pkixTestErrorMsg = "Give me a cert with an HTTP URI!";
        goto cleanup;
    }

    /* create HttpCertStore */
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HttpCertStore_Create(NULL, location, &certStore, plContext));

    /* get the GetCerts callback */
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertStore_GetCertCallback(certStore, &getCerts, plContext));

    /* create a CertSelector */
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_Create(NULL, NULL, &certSelector, plContext));

    /* Get the certs */
    PKIX_TEST_EXPECT_NO_ERROR(getCerts(certStore, certSelector, &nbio, &certs, plContext));

    while (nbio != NULL) {
        /* poll for a completion */

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertStore_CertContinue(certStore, certSelector, &nbio, &certs, plContext));
    }

    if (certs) {

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength(certs, &numCerts, plContext));

        if (numCerts == 0) {
            printf("HttpCertStore returned an empty Cert list\n");
            goto cleanup;
        }

        for (i = 0; i < numCerts; i++) {
            PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem(certs,
                                                        i,
                                                        (PKIX_PL_Object **)&cert,
                                                        plContext));

            asciiResult = PKIX_Cert2ASCII(cert);

            printf("CERT[%d]:\n%s\n", i, asciiResult);

            /* PKIX_Cert2ASCII used PKIX_PL_Malloc(...,,NULL) */
            PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(asciiResult, NULL));
            asciiResult = NULL;

            PKIX_TEST_DECREF_BC(cert);
        }
    } else {
        printf("HttpCertStore returned a NULL Cert list\n");
    }

    /* create HttpCertStore */
    PKIX_TEST_EXPECT_NO_ERROR(pkix_pl_HttpCertStore_CreateWithAsciiName(NULL, crlLocation, &crlStore, plContext));

    /* get the GetCrls callback */
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertStore_GetCRLCallback(crlStore, &getCrls, plContext));

    /* create a CrlSelector */
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_CRLSelector_Create(NULL, NULL, &crlSelector, plContext));

    /* Get the crls */
    PKIX_TEST_EXPECT_NO_ERROR(getCrls(crlStore, crlSelector, &nbio, &crls, plContext));

    while (nbio != NULL) {
        /* poll for a completion */

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertStore_CrlContinue(crlStore, crlSelector, &nbio, &crls, plContext));
    }

    if (crls) {

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength(crls, &numCrls, plContext));

        if (numCrls == 0) {
            printf("HttpCertStore returned an empty CRL list\n");
            goto cleanup;
        }

        for (i = 0; i < numCrls; i++) {
            PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem(crls,
                                                        i,
                                                        (PKIX_PL_Object **)&crl,
                                                        plContext));

            PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString(
                (PKIX_PL_Object *)crl,
                &crlString,
                plContext));

            PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_GetEncoded(crlString,
                                                                PKIX_ESCASCII,
                                                                (void **)&asciiResult,
                                                                &length,
                                                                plContext));

            printf("CRL[%d]:\n%s\n", i, asciiResult);

            PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(asciiResult, plContext));
            PKIX_TEST_DECREF_BC(crlString);
            PKIX_TEST_DECREF_BC(crl);
        }
    } else {
        printf("HttpCertStore returned a NULL CRL list\n");
    }

cleanup:

    PKIX_TEST_DECREF_AC(cert);
    PKIX_TEST_DECREF_AC(cmdLineCert);
    PKIX_TEST_DECREF_AC(certStore);
    PKIX_TEST_DECREF_AC(crlStore);
    PKIX_TEST_DECREF_AC(location);
    PKIX_TEST_DECREF_AC(certs);
    PKIX_TEST_DECREF_AC(crl);
    PKIX_TEST_DECREF_AC(crlString);
    PKIX_TEST_DECREF_AC(crls);

    PKIX_TEST_RETURN();

    endTests("HttpDefaultClient");

    return (0);
}
