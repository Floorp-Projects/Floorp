/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_crl.c
 *
 * Test CRL Type
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;

static void
createCRLs(
    char *dataDir,
    char *goodInput,
    char *diffInput,
    PKIX_PL_CRL **goodObject,
    PKIX_PL_CRL **equalObject,
    PKIX_PL_CRL **diffObject)
{
    PKIX_TEST_STD_VARS();

    subTest("PKIX_PL_CRL_Create <goodObject>");
    *goodObject = createCRL(dataDir, goodInput, plContext);

    subTest("PKIX_PL_CRL_Create <equalObject>");
    *equalObject = createCRL(dataDir, goodInput, plContext);

    subTest("PKIX_PL_CRL_Create <diffObject>");
    *diffObject = createCRL(dataDir, diffInput, plContext);

    PKIX_TEST_RETURN();
}

static void
testGetCRLEntryForSerialNumber(
    PKIX_PL_CRL *goodObject)
{
    PKIX_PL_BigInt *bigInt;
    PKIX_PL_String *bigIntString = NULL;
    PKIX_PL_CRLEntry *crlEntry = NULL;
    PKIX_PL_String *crlEntryString = NULL;
    char *snAscii = "3039";
    char *expectedAscii =
        "\n\t[\n"
        "\tSerialNumber:    3039\n"
        "\tReasonCode:      257\n"
        "\tRevocationDate:  Fri Jan 07, 2005\n"
        /*      "\tRevocationDate:  Fri Jan 07 15:09:10 2005\n" */
        "\tCritExtOIDs:     (EMPTY)\n"
        "\t]\n\t";

    PKIX_TEST_STD_VARS();

    subTest("PKIX_PL_CRL_GetCRLEntryForSerialNumber");

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create(
        PKIX_ESCASCII,
        snAscii,
        PL_strlen(snAscii),
        &bigIntString,
        plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_BigInt_Create(
        bigIntString,
        &bigInt,
        plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CRL_GetCRLEntryForSerialNumber(
        goodObject, bigInt, &crlEntry, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString(
        (PKIX_PL_Object *)crlEntry,
        &crlEntryString,
        plContext));

    testToStringHelper((PKIX_PL_Object *)crlEntryString,
                       expectedAscii, plContext);

cleanup:

    PKIX_TEST_DECREF_AC(bigIntString);
    PKIX_TEST_DECREF_AC(bigInt);
    PKIX_TEST_DECREF_AC(crlEntryString);
    PKIX_TEST_DECREF_AC(crlEntry);
    PKIX_TEST_RETURN();
}

static void
testGetIssuer(
    PKIX_PL_CRL *goodObject,
    PKIX_PL_CRL *equalObject,
    PKIX_PL_CRL *diffObject)
{
    PKIX_PL_X500Name *goodIssuer = NULL;
    PKIX_PL_X500Name *equalIssuer = NULL;
    PKIX_PL_X500Name *diffIssuer = NULL;
    char *expectedAscii = "CN=hanfeiyu,O=sun,C=us";

    PKIX_TEST_STD_VARS();

    subTest("PKIX_PL_CRL_GetIssuer");

    PKIX_TEST_EXPECT_NO_ERROR(
        PKIX_PL_CRL_GetIssuer(goodObject, &goodIssuer, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(
        PKIX_PL_CRL_GetIssuer(equalObject, &equalIssuer, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(
        PKIX_PL_CRL_GetIssuer(diffObject, &diffIssuer, plContext));

    PKIX_TEST_EQ_HASH_TOSTR_DUP(goodIssuer,
                                equalIssuer,
                                diffIssuer,
                                expectedAscii,
                                X500Name,
                                PKIX_TRUE);

cleanup:

    PKIX_TEST_DECREF_AC(goodIssuer);
    PKIX_TEST_DECREF_AC(equalIssuer);
    PKIX_TEST_DECREF_AC(diffIssuer);

    PKIX_TEST_RETURN();
}

static void
testCritExtensionsAbsent(PKIX_PL_CRL *crl)
{
    PKIX_List *oidList = NULL;
    PKIX_UInt32 numOids = 0;

    PKIX_TEST_STD_VARS();

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CRL_GetCriticalExtensionOIDs(crl, &oidList, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength(oidList, &numOids, plContext));
    if (numOids != 0) {
        pkixTestErrorMsg = "unexpected mismatch";
    }

cleanup:

    PKIX_TEST_DECREF_AC(oidList);

    PKIX_TEST_RETURN();
}

static void
testGetCriticalExtensionOIDs(PKIX_PL_CRL *goodObject)
{
    subTest("PKIX_PL_CRL_GetCriticalExtensionOIDs "
            "<0 element>");
    testCritExtensionsAbsent(goodObject);
}

static void
testVerifySignature(char *dataCentralDir, PKIX_PL_CRL *crl)
{
    PKIX_PL_Cert *firstCert = NULL;
    PKIX_PL_Cert *secondCert = NULL;
    PKIX_PL_PublicKey *firstPubKey = NULL;
    PKIX_PL_PublicKey *secondPubKey = NULL;

    PKIX_TEST_STD_VARS();

    subTest("PKIX_PL_Cert_Create <hanfeiyu2hanfeiyu>");
    firstCert = createCert(dataCentralDir, "hanfeiyu2hanfeiyu", plContext);

    subTest("PKIX_PL_Cert_Create <hy2hy-bc0>");
    secondCert = createCert(dataCentralDir, "hy2hy-bc0", plContext);

    subTest("PKIX_PL_Cert_GetSubjectPublicKey <hanfeiyu2hanfeiyu>");
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubjectPublicKey(firstCert, &firstPubKey, plContext));

    subTest("PKIX_PL_Cert_GetSubjectPublicKey <hanfei2hanfei>");
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubjectPublicKey(secondCert, &secondPubKey, plContext));

    subTest("PKIX_PL_CRL_VerifySignature <positive>");
    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CRL_VerifySignature(crl, firstPubKey, plContext));

    subTest("PKIX_PL_CRL_VerifySignature <negative>");
    PKIX_TEST_EXPECT_ERROR(PKIX_PL_CRL_VerifySignature(crl, secondPubKey, plContext));

cleanup:

    PKIX_TEST_DECREF_AC(firstCert);
    PKIX_TEST_DECREF_AC(secondCert);
    PKIX_TEST_DECREF_AC(firstPubKey);
    PKIX_TEST_DECREF_AC(secondPubKey);

    PKIX_TEST_RETURN();
}

static void
printUsage(void)
{
    (void)printf("\nUSAGE:\ttest_crl <test-purpose> <data-central-dir>\n\n");
}

/* Functional tests for CRL public functions */

int
test_crl(int argc, char *argv[])
{
    PKIX_PL_CRL *goodObject = NULL;
    PKIX_PL_CRL *equalObject = NULL;
    PKIX_PL_CRL *diffObject = NULL;
    PKIX_UInt32 actualMinorVersion;
    PKIX_UInt32 j = 0;

    char *dataCentralDir = NULL;
    char *goodInput = "crlgood.crl";
    char *diffInput = "crldiff.crl";
    char *expectedAscii =
        "[\n"
        "\tVersion:         v2\n"
        "\tIssuer:          CN=hanfeiyu,O=sun,C=us\n"
        "\tUpdate:   [Last: Fri Jan 07, 2005\n"
        /*      "\tUpdate:   [Last: Fri Jan 07 15:09:10 2005\n" */
        "\t           Next: Sat Jan 07, 2006]\n"
        /*      "\t           Next: Sat Jan 07 15:09:10 2006]\n" */
        "\tSignatureAlgId:  1.2.840.10040.4.3\n"
        "\tCRL Number     : (null)\n"
        "\n\tEntry List:      (\n"
        "\t[\n"
        "\tSerialNumber:    010932\n"
        "\tReasonCode:      260\n"
        "\tRevocationDate:  Fri Jan 07, 2005\n"
        /*      "\tRevocationDate:  Fri Jan 07 15:09:10 2005\n" */
        "\tCritExtOIDs:     (EMPTY)\n"
        "\t]\n\t"
        ", "
        "\n\t[\n"
        "\tSerialNumber:    3039\n"
        "\tReasonCode:      257\n"
        "\tRevocationDate:  Fri Jan 07, 2005\n"
        /*      "\tRevocationDate:  Fri Jan 07 15:09:10 2005\n" */
        "\tCritExtOIDs:     (EMPTY)\n"
        "\t]\n\t"
        ")"
        "\n\n"
        "\tCritExtOIDs:     (EMPTY)\n"
        "]\n";
    /* Note XXX serialnumber and reasoncode need debug */

    PKIX_TEST_STD_VARS();

    startTests("CRL");

    PKIX_TEST_EXPECT_NO_ERROR(
        PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

    if (argc < 3 + j) {
        printUsage();
        return (0);
    }

    dataCentralDir = argv[2 + j];

    createCRLs(dataCentralDir,
               goodInput,
               diffInput,
               &goodObject,
               &equalObject,
               &diffObject);

    PKIX_TEST_EQ_HASH_TOSTR_DUP(goodObject,
                                equalObject,
                                diffObject,
                                expectedAscii,
                                CRL,
                                PKIX_TRUE);

    testGetIssuer(goodObject, equalObject, diffObject);

    testGetCriticalExtensionOIDs(goodObject);

    testGetCRLEntryForSerialNumber(goodObject);

    testVerifySignature(dataCentralDir, goodObject);

cleanup:

    PKIX_TEST_DECREF_AC(goodObject);
    PKIX_TEST_DECREF_AC(equalObject);
    PKIX_TEST_DECREF_AC(diffObject);

    PKIX_Shutdown(plContext);

    PKIX_TEST_RETURN();

    endTests("CRL");

    return (0);
}
