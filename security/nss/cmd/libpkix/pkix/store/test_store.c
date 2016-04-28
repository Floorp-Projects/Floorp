/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_certstore.c
 *
 * Test CertStore Type
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;

static PKIX_Error *
testCRLCallback(
    PKIX_CertStore *store,
    PKIX_CRLSelector *selector,
    void **pNBIOContext,
    PKIX_List **pCrls, /* list of PKIX_PL_Crl */
    void *plContext)
{
    return (0);
}

static PKIX_Error *
testCRLContinue(
    PKIX_CertStore *store,
    PKIX_CRLSelector *selector,
    void **pNBIOContext,
    PKIX_List **pCrls, /* list of PKIX_PL_Crl */
    void *plContext)
{
    return (0);
}

static PKIX_Error *
testCertCallback(
    PKIX_CertStore *store,
    PKIX_CertSelector *selector,
    void **pNBIOContext,
    PKIX_List **pCerts, /* list of PKIX_PL_Cert */
    void *plContext)
{
    return (0);
}

static PKIX_Error *
testCertContinue(
    PKIX_CertStore *store,
    PKIX_CertSelector *selector,
    void **pNBIOContext,
    PKIX_List **pCerts, /* list of PKIX_PL_Cert */
    void *plContext)
{
    return (0);
}

static char *
catDirName(char *platform, char *dir, void *plContext)
{
    char *pathName = NULL;
    PKIX_UInt32 dirLen;
    PKIX_UInt32 platformLen;

    PKIX_TEST_STD_VARS();

    dirLen = PL_strlen(dir);
    platformLen = PL_strlen(platform);

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Malloc(platformLen +
                                                 dirLen +
                                                 2,
                                             (void **)&pathName, plContext));

    PL_strcpy(pathName, platform);
    PL_strcat(pathName, "/");
    PL_strcat(pathName, dir);

cleanup:

    PKIX_TEST_RETURN();

    return (pathName);
}

static void
testCertStore(char *crlDir)
{
    PKIX_PL_String *dirString = NULL;
    PKIX_CertStore *certStore = NULL;
    PKIX_PL_Object *getCertStoreContext = NULL;
    PKIX_CertStore_CertCallback certCallback = NULL;
    PKIX_CertStore_CRLCallback crlCallback = NULL;

    PKIX_TEST_STD_VARS();

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create(PKIX_ESCASCII,
                                                    crlDir,
                                                    0,
                                                    &dirString,
                                                    plContext));

    subTest("PKIX_CertStore_Create");
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertStore_Create(testCertCallback,
                                                    testCRLCallback,
                                                    testCertContinue,
                                                    testCRLContinue,
                                                    NULL, /* trustCallback */
                                                    (PKIX_PL_Object *)dirString,
                                                    PKIX_TRUE, /* cacheFlag */
                                                    PKIX_TRUE, /* local */
                                                    &certStore,
                                                    plContext));

    subTest("PKIX_CertStore_GetCertCallback");
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertStore_GetCertCallback(certStore, &certCallback, plContext));

    if (certCallback != testCertCallback) {
        testError("PKIX_CertStore_GetCertCallback unexpected mismatch");
    }

    subTest("PKIX_CertStore_GetCRLCallback");
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertStore_GetCRLCallback(certStore, &crlCallback, plContext));

    if (crlCallback != testCRLCallback) {
        testError("PKIX_CertStore_GetCRLCallback unexpected mismatch");
    }

    subTest("PKIX_CertStore_GetCertStoreContext");
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertStore_GetCertStoreContext(certStore, &getCertStoreContext, plContext));

    if ((PKIX_PL_Object *)dirString != getCertStoreContext) {
        testError("PKIX_CertStore_GetCertStoreContext unexpected mismatch");
    }

cleanup:

    PKIX_TEST_DECREF_AC(dirString);
    PKIX_TEST_DECREF_AC(certStore);
    PKIX_TEST_DECREF_AC(getCertStoreContext);

    PKIX_TEST_RETURN();
}

static void
printUsage(char *pName)
{
    printf("\nUSAGE: %s testName <data-dir> <platform-dir>\n\n", pName);
}

/* Functional tests for CertStore public functions */

int
test_store(int argc, char *argv[])
{

    char *platformDir = NULL;
    char *dataDir = NULL;
    char *combinedDir = NULL;
    PKIX_UInt32 actualMinorVersion;
    PKIX_UInt32 j = 0;

    PKIX_TEST_STD_VARS();

    PKIX_TEST_EXPECT_NO_ERROR(
        PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

    if (argc < (3 + j)) {
        printUsage(argv[0]);
        return (0);
    }

    startTests(argv[1 + j]);

    dataDir = argv[2 + j];
    platformDir = argv[3 + j];
    combinedDir = catDirName(platformDir, dataDir, plContext);

    testCertStore(combinedDir);

cleanup:

    pkixTestErrorResult = PKIX_PL_Free(combinedDir, plContext);

    PKIX_Shutdown(plContext);

    PKIX_TEST_RETURN();

    endTests("CertStore");

    return (0);
}
