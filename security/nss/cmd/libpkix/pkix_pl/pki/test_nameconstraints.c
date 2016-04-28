/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_nameconstraints.c
 *
 * Test CERT Name Constraints Type
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;

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
testNameConstraints(char *dataDir)
{
    char *goodPname = "nameConstraintsDN5CACert.crt";
    PKIX_PL_Cert *goodCert = NULL;
    PKIX_PL_CertNameConstraints *goodNC = NULL;
    char *expectedAscii =
        "[\n"
        "\t\tPermitted Name:  (OU=permittedSubtree1,"
        "O=Test Certificates,C=US)\n"
        "\t\tExcluded Name:   (OU=excludedSubtree1,"
        "OU=permittedSubtree1,O=Test Certificates,C=US)\n"
        "\t]\n";

    PKIX_TEST_STD_VARS();

    subTest("PKIX_PL_CertNameConstraints");

    goodCert = createCert(dataDir, goodPname, plContext);

    subTest("PKIX_PL_Cert_GetNameConstraints");

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetNameConstraints(goodCert, &goodNC, plContext));

    testToStringHelper((PKIX_PL_Object *)goodNC, expectedAscii, plContext);

cleanup:

    PKIX_TEST_DECREF_AC(goodNC);
    PKIX_TEST_DECREF_AC(goodCert);

    PKIX_TEST_RETURN();
}

static void
printUsage(void)
{
    (void)printf("\nUSAGE:\ttest_nameconstraints <test-purpose>"
                 " <data-dir> <platform-prefix>\n\n");
}

/* Functional tests for CRL public functions */

int
test_nameconstraints(int argc, char *argv[])
{
    PKIX_UInt32 actualMinorVersion;
    PKIX_UInt32 j = 0;
    char *platformDir = NULL;
    char *dataDir = NULL;
    char *combinedDir = NULL;

    /* Note XXX serialnumber and reasoncode need debug */

    PKIX_TEST_STD_VARS();

    startTests("NameConstraints");

    PKIX_TEST_EXPECT_NO_ERROR(
        PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

    if (argc < 3 + j) {
        printUsage();
        return (0);
    }

    dataDir = argv[2 + j];
    platformDir = argv[3 + j];
    combinedDir = catDirName(platformDir, dataDir, plContext);

    testNameConstraints(combinedDir);

cleanup:

    pkixTestErrorResult = PKIX_PL_Free(combinedDir, plContext);

    PKIX_Shutdown(plContext);

    PKIX_TEST_RETURN();

    endTests("NameConstraints");

    return (0);
}
