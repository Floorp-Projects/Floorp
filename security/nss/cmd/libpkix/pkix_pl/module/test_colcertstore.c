/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_colcertstore.c
 *
 * Test CollectionCertStore Type
 *
 */

#include "testutil.h"

#include "testutil_nss.h"

/* When CRL IDP is supported, change NUM_CRLS to 9 */
#define PKIX_TEST_COLLECTIONCERTSTORE_NUM_CRLS 4
#define PKIX_TEST_COLLECTIONCERTSTORE_NUM_CERTS 15

static void *plContext = NULL;

static PKIX_Error *
testCRLSelectorMatchCallback(
        PKIX_CRLSelector *selector,
        PKIX_PL_CRL *crl,
	PKIX_Boolean *pMatch,
        void *plContext)
{
	*pMatch = PKIX_TRUE;

        return (0);
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
getCertCallback(
        PKIX_CertStore *store,
        PKIX_CertSelector *certSelector,
        PKIX_List **pCerts,
        void *plContext)
{
        return (0);
}

static char *catDirName(char *platform, char *dir, void *plContext)
{
        char *pathName = NULL;
        PKIX_UInt32 dirLen;
        PKIX_UInt32 platformLen;

        PKIX_TEST_STD_VARS();

        dirLen = PL_strlen(dir);
        platformLen = PL_strlen(platform);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Malloc
                (platformLen + dirLen + 2, (void **)&pathName, plContext));

        PL_strcpy(pathName, platform);
        PL_strcat(pathName, "/");
        PL_strcat(pathName, dir);

cleanup:

        PKIX_TEST_RETURN();

        return (pathName);
}

static 
void testGetCRL(char *crlDir)
{
        PKIX_PL_String *dirString = NULL;
        PKIX_CertStore_CRLCallback crlCallback;
        PKIX_CertStore *certStore = NULL;
        PKIX_CRLSelector *crlSelector = NULL;
        PKIX_List *crlList = NULL;
        PKIX_UInt32 numCrl = 0;
	void *nbioContext = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                                    (PKIX_ESCASCII,
                                    crlDir,
                                    0,
                                    &dirString,
                                    plContext));

        subTest("PKIX_PL_CollectionCertStore_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CollectionCertStore_Create
                                    (dirString,
                                    &certStore,
                                    plContext));

        subTest("PKIX_CRLSelector_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CRLSelector_Create
                                    (testCRLSelectorMatchCallback,
                                    NULL,
                                    &crlSelector,
                                    plContext));

        subTest("PKIX_CertStore_GetCRLCallback");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertStore_GetCRLCallback
                                    (certStore, &crlCallback, NULL));

        subTest("Getting data from CRL Callback");
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

        PKIX_TEST_DECREF_AC(dirString);
        PKIX_TEST_DECREF_AC(crlList);
        PKIX_TEST_DECREF_AC(crlSelector);
        PKIX_TEST_DECREF_AC(certStore);

        PKIX_TEST_RETURN();
}

static 
void testGetCert(char *certDir)
{
        PKIX_PL_String *dirString = NULL;
        PKIX_CertStore_CertCallback certCallback;
        PKIX_CertStore *certStore = NULL;
        PKIX_CertSelector *certSelector = NULL;
        PKIX_List *certList = NULL;
        PKIX_UInt32 numCert = 0;
	void *nbioContext = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                                    (PKIX_ESCASCII,
                                    certDir,
                                    0,
                                    &dirString,
                                    plContext));

        subTest("PKIX_PL_CollectionCertStore_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CollectionCertStore_Create
                                    (dirString,
                                    &certStore,
                                    plContext));

        subTest("PKIX_CertSelector_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_Create
                                    (testCertSelectorMatchCallback,
                                    NULL,
                                    &certSelector,
                                    plContext));

        subTest("PKIX_CertStore_GetCertCallback");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertStore_GetCertCallback
                                    (certStore, &certCallback, NULL));

        subTest("Getting data from Cert Callback");
        PKIX_TEST_EXPECT_NO_ERROR(certCallback
                                    (certStore,
                                    certSelector,
                                    &nbioContext,
                                    &certList,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength
                                    (certList,
                                    &numCert,
                                    plContext));

        if (numCert != PKIX_TEST_COLLECTIONCERTSTORE_NUM_CERTS) {
                pkixTestErrorMsg = "unexpected Cert number mismatch";
        }

cleanup:

        PKIX_TEST_DECREF_AC(dirString);
        PKIX_TEST_DECREF_AC(certList);
        PKIX_TEST_DECREF_AC(certSelector);
        PKIX_TEST_DECREF_AC(certStore);

        PKIX_TEST_RETURN();
}

static void printUsage(char *pName){
        printf("\nUSAGE: %s test-purpose <data-dir> <platform-dir>\n\n", pName);
}

/* Functional tests for CollectionCertStore public functions */

int test_colcertstore(int argc, char *argv[]) {

        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;
        char *platformDir = NULL;
        char *dataDir = NULL;
        char *combinedDir = NULL;

        PKIX_TEST_STD_VARS();

        startTests("CollectionCertStore");

        PKIX_TEST_EXPECT_NO_ERROR(
            PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

        if (argc < (3 + j)) {
                printUsage(argv[0]);
                return (0);
        }

        dataDir = argv[2 + j];
        platformDir = argv[3 + j];
	combinedDir = catDirName(platformDir, dataDir, plContext);

        testGetCRL(combinedDir);
        testGetCert(combinedDir);

cleanup:

        pkixTestErrorResult = PKIX_PL_Free(combinedDir, plContext);

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("CollectionCertStore");

        return (0);
}
