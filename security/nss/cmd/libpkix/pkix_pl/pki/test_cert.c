/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_cert.c
 *
 * Test Cert Type
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static PKIX_PL_Cert *altNameNoneCert = NULL;
static PKIX_PL_Cert *altNameOtherCert = NULL;
static PKIX_PL_Cert *altNameOtherCert_diff = NULL;
static PKIX_PL_Cert *altNameRfc822Cert = NULL;
static PKIX_PL_Cert *altNameRfc822Cert_diff = NULL;
static PKIX_PL_Cert *altNameDnsCert = NULL;
static PKIX_PL_Cert *altNameDnsCert_diff = NULL;
static PKIX_PL_Cert *altNameX400Cert = NULL;
static PKIX_PL_Cert *altNameX400Cert_diff = NULL;
static PKIX_PL_Cert *altNameDnCert = NULL;
static PKIX_PL_Cert *altNameDnCert_diff = NULL;
static PKIX_PL_Cert *altNameEdiCert = NULL;
static PKIX_PL_Cert *altNameEdiCert_diff = NULL;
static PKIX_PL_Cert *altNameUriCert = NULL;
static PKIX_PL_Cert *altNameUriCert_diff = NULL;
static PKIX_PL_Cert *altNameIpCert = NULL;
static PKIX_PL_Cert *altNameIpCert_diff = NULL;
static PKIX_PL_Cert *altNameOidCert = NULL;
static PKIX_PL_Cert *altNameOidCert_diff = NULL;
static PKIX_PL_Cert *altNameMultipleCert = NULL;

static void *plContext = NULL;

static void createCerts(
        char *dataCentralDir,
        char *goodInput,
        char *diffInput,
        PKIX_PL_Cert **goodObject,
        PKIX_PL_Cert **equalObject,
        PKIX_PL_Cert **diffObject)
{
        subTest("PKIX_PL_Cert_Create <goodObject>");
        *goodObject = createCert(dataCentralDir, goodInput, plContext);

        subTest("PKIX_PL_Cert_Create <equalObject>");
        *equalObject = createCert(dataCentralDir, goodInput, plContext);

        subTest("PKIX_PL_Cert_Create <diffObject>");
        *diffObject = createCert(dataCentralDir, diffInput, plContext);
}


static void
createCertsWithSubjectAltNames(char *dataCentralDir)
{
        subTest("PKIX_PL_Cert_Create <altNameDNS>");
        altNameDnsCert = createCert
                (dataCentralDir, "generalName/altNameDnsCert", plContext);

        subTest("PKIX_PL_Cert_Create <altNameDNS_diff>");
        altNameDnsCert_diff = createCert
                (dataCentralDir, "generalName/altNameDnsCert_diff", plContext);


        subTest("PKIX_PL_Cert_Create <altNameRFC822>");
        altNameRfc822Cert = createCert
                (dataCentralDir, "generalName/altNameRfc822Cert", plContext);

        subTest("PKIX_PL_Cert_Create <altNameRFC822_diff>");
        altNameRfc822Cert_diff = createCert
                (dataCentralDir, "generalName/altNameRfc822Cert_diff", plContext);

        subTest("PKIX_PL_Cert_Create <altNameX400Cert>");
        altNameX400Cert = createCert
                (dataCentralDir, "generalName/altNameX400Cert", plContext);

        subTest("PKIX_PL_Cert_Create <altNameX400_diff>");
        altNameX400Cert_diff = createCert
                (dataCentralDir, "generalName/altNameX400Cert_diff", plContext);

        subTest("PKIX_PL_Cert_Create <altNameDN>");
        altNameDnCert = createCert
                (dataCentralDir, "generalName/altNameDnCert", plContext);

        subTest("PKIX_PL_Cert_Create <altNameDN_diff>");
        altNameDnCert_diff = createCert
                (dataCentralDir, "generalName/altNameDnCert_diff", plContext);

        subTest("PKIX_PL_Cert_Create <altNameEdiCert>");
        altNameEdiCert = createCert
                (dataCentralDir, "generalName/altNameEdiCert", plContext);

        subTest("PKIX_PL_Cert_Create <altNameEdi_diff>");
        altNameEdiCert_diff = createCert
                (dataCentralDir, "generalName/altNameEdiCert_diff", plContext);

        subTest("PKIX_PL_Cert_Create <altNameURI>");
        altNameUriCert = createCert
                (dataCentralDir, "generalName/altNameUriCert", plContext);

        subTest("PKIX_PL_Cert_Create <altNameURI_diff>");
        altNameUriCert_diff = createCert
                (dataCentralDir, "generalName/altNameUriCert_diff", plContext);

        subTest("PKIX_PL_Cert_Create <altNameIP>");
        altNameIpCert = createCert
                (dataCentralDir, "generalName/altNameIpCert", plContext);

        subTest("PKIX_PL_Cert_Create <altNameIP_diff>");
        altNameIpCert_diff = createCert
                (dataCentralDir, "generalName/altNameIpCert_diff", plContext);

        subTest("PKIX_PL_Cert_Create <altNameOID>");
        altNameOidCert = createCert
                (dataCentralDir, "generalName/altNameOidCert", plContext);

        subTest("PKIX_PL_Cert_Create <altNameOID_diff>");
        altNameOidCert_diff = createCert
                (dataCentralDir, "generalName/altNameOidCert_diff", plContext);

        subTest("PKIX_PL_Cert_Create <altNameOther>");
        altNameOtherCert = createCert
                (dataCentralDir, "generalName/altNameOtherCert", plContext);

        subTest("PKIX_PL_Cert_Create <altNameOther_diff>");
        altNameOtherCert_diff = createCert
                (dataCentralDir, "generalName/altNameOtherCert_diff", plContext);

        subTest("PKIX_PL_Cert_Create <altNameNone>");
        altNameNoneCert = createCert
                (dataCentralDir, "generalName/altNameNoneCert", plContext);

        subTest("PKIX_PL_Cert_Create <altNameMultiple>");
        altNameMultipleCert = createCert
                (dataCentralDir, "generalName/altNameRfc822DnsCert", plContext);
}

static void testGetVersion(
        PKIX_PL_Cert *goodObject)
{
        PKIX_UInt32 goodVersion;

        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_Cert_GetVersion");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetVersion
                (goodObject, &goodVersion, plContext));

        if (goodVersion != 2){
                testError("unexpected mismatch");
                (void) printf("Actual value:\t%d\n", goodVersion);
                (void) printf("Expected value:\t2\n");
                goto cleanup;
        }

cleanup:

        PKIX_TEST_RETURN();
}

static void testGetSerialNumber(
        PKIX_PL_Cert *goodObject,
        PKIX_PL_Cert *equalObject,
        PKIX_PL_Cert *diffObject)
{
        PKIX_PL_BigInt *goodSN = NULL;
        PKIX_PL_BigInt *equalSN = NULL;
        PKIX_PL_BigInt *diffSN = NULL;
        char *expectedAscii = "37bc66ec";

        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_Cert_GetSerialNumber");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSerialNumber
                (goodObject, &goodSN, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSerialNumber
                (equalObject, &equalSN, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSerialNumber
                (diffObject, &diffSN, plContext));

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (goodSN, equalSN, diffSN, expectedAscii, BigInt, PKIX_TRUE);

cleanup:

        PKIX_TEST_DECREF_AC(goodSN);
        PKIX_TEST_DECREF_AC(equalSN);
        PKIX_TEST_DECREF_AC(diffSN);

        PKIX_TEST_RETURN();
}


static void testGetSubject(
        PKIX_PL_Cert *goodObject,
        PKIX_PL_Cert *equalObject,
        PKIX_PL_Cert *diffObject)
{
        PKIX_PL_X500Name *goodSubject = NULL;
        PKIX_PL_X500Name *equalSubject = NULL;
        PKIX_PL_X500Name *diffSubject = NULL;
        char *expectedAscii = "OU=bcn,OU=east,O=sun,C=us";

        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_Cert_GetSubject");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubject
                (goodObject, &goodSubject, plContext));

        if (!goodSubject){
                testError("Certificate Subject should not be NULL");
                goto cleanup;
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubject
                (equalObject, &equalSubject, plContext));

        if (!equalSubject){
                testError("Certificate Subject should not be NULL");
                goto cleanup;
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubject
                (diffObject, &diffSubject, plContext));

        if (!diffSubject){
                testError("Certificate Subject should not be NULL");
                goto cleanup;
        }

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (goodSubject,
                equalSubject,
                diffSubject,
                expectedAscii,
                X500Name,
                PKIX_TRUE);

cleanup:

        PKIX_TEST_DECREF_AC(goodSubject);
        PKIX_TEST_DECREF_AC(equalSubject);
        PKIX_TEST_DECREF_AC(diffSubject);

        PKIX_TEST_RETURN();
}

static void testGetIssuer(
        PKIX_PL_Cert *goodObject,
        PKIX_PL_Cert *equalObject,
        PKIX_PL_Cert *diffObject)
{
        PKIX_PL_X500Name *goodIssuer = NULL;
        PKIX_PL_X500Name *equalIssuer = NULL;
        PKIX_PL_X500Name *diffIssuer = NULL;
        char *expectedAscii = "CN=yassir,OU=bcn,OU=east,O=sun,C=us";

        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_Cert_GetIssuer");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetIssuer
                (goodObject, &goodIssuer, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetIssuer
                (equalObject, &equalIssuer, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetIssuer
                (diffObject, &diffIssuer, plContext));

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (goodIssuer,
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

static void testAltNames(
        PKIX_PL_Cert *goodCert,
        PKIX_PL_Cert *diffCert,
        char *expectedAscii)
{
        PKIX_List *goodAltNames = NULL;
        PKIX_List *diffAltNames = NULL;
        PKIX_PL_GeneralName *goodAltName = NULL;
        PKIX_PL_GeneralName *diffAltName = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubjectAltNames
                                    (goodCert, &goodAltNames, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                                (goodAltNames,
                                0,
                                (PKIX_PL_Object **)&goodAltName,
                                plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubjectAltNames
                                    (diffCert, &diffAltNames, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                                (diffAltNames,
                                0,
                                (PKIX_PL_Object **)&diffAltName,
                                plContext));

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (goodAltName, goodAltName, diffAltName,
                expectedAscii, GeneralName, PKIX_TRUE);

cleanup:
        PKIX_TEST_DECREF_AC(goodAltNames);
        PKIX_TEST_DECREF_AC(goodAltName);
        PKIX_TEST_DECREF_AC(diffAltNames);
        PKIX_TEST_DECREF_AC(diffAltName);
        PKIX_TEST_RETURN();
}

static void testAltNamesNone(PKIX_PL_Cert *cert){

        PKIX_List *altNames = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubjectAltNames
                                    (cert, &altNames, plContext));

        if (altNames != NULL){
                testError("unexpected mismatch");
                (void) printf("Actual value:\t%p\n", (void *)altNames);
                (void) printf("Expected value:\tNULL\n");
                goto cleanup;
        }

cleanup:

        PKIX_TEST_DECREF_AC(altNames);
        PKIX_TEST_RETURN();

}
static void testAltNamesMultiple(){
        PKIX_List *altNames = NULL;
        PKIX_PL_GeneralName *firstAltName = NULL;
        PKIX_Int32 firstExpectedType = PKIX_RFC822_NAME;
        PKIX_PL_GeneralName *secondAltName = NULL;
        PKIX_Int32 secondExpectedType = PKIX_DNS_NAME;


        char *expectedAscii =
                "[\n"
                "\tVersion:         v3\n"
                "\tSerialNumber:    2d\n"
                "\tIssuer:          OU=labs,O=sun,C=us\n"
                "\tSubject:         CN=yassir,OU=labs,O=sun,C=us\n"
                "\tValidity: [From: Mon Feb 09, 2004\n"
        /*      "\tValidity: [From: Mon Feb 09 14:43:52 2004\n" */
                "\t           To:   Mon Feb 09, 2004]\n"
        /*      "\t           To:   Mon Feb 09 14:43:52 2004]\n" */
                "\tSubjectAltNames: (yassir@sun.com, sunray.sun.com)\n"
                "\tAuthorityKeyId:  (null)\n"
                "\tSubjectKeyId:    (null)\n"
                "\tSubjPubKeyAlgId: ANSI X9.57 DSA Signature\n"
                "\tCritExtOIDs:     (EMPTY)\n"
                "\tExtKeyUsages:    (null)\n"
                "\tBasicConstraint: (null)\n"
                "\tCertPolicyInfo:  (null)\n"
                "\tPolicyMappings:  (null)\n"
                "\tExplicitPolicy:  -1\n"
                "\tInhibitMapping:  -1\n"
                "\tInhibitAnyPolicy:-1\n"
                "\tNameConstraints: (null)\n"
                "\tAuthorityInfoAccess: (null)\n"
                "\tSubjectInfoAccess: (null)\n"
                "\tCacheFlag:       0\n"
                "]\n";

        PKIX_TEST_STD_VARS();

        testToStringHelper
                ((PKIX_PL_Object *)altNameMultipleCert,
                expectedAscii,
                plContext);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubjectAltNames
                (altNameMultipleCert, &altNames, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (altNames, 0, (PKIX_PL_Object **)&firstAltName, plContext));

        if (firstAltName->type != firstExpectedType){
                testError("unexpected mismatch");
                (void) printf("Actual value:\t%d\n", firstAltName->type);
                (void) printf("Expected value:\t%d\n", firstExpectedType);
                goto cleanup;
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (altNames, 1, (PKIX_PL_Object **)&secondAltName, plContext));

        if (secondAltName->type != secondExpectedType){
                testError("unexpected mismatch");
                (void) printf("Actual value:\t%d\n", secondAltName->type);
                (void) printf("Expected value:\t%d\n", secondExpectedType);
                goto cleanup;
        }

cleanup:
        PKIX_TEST_DECREF_AC(altNames);
        PKIX_TEST_DECREF_AC(firstAltName);
        PKIX_TEST_DECREF_AC(secondAltName);
        PKIX_TEST_RETURN();
}

static void testGetSubjectAltNames(char *dataCentralDir){

        char *expectedAscii = NULL;

        createCertsWithSubjectAltNames(dataCentralDir);

        subTest("PKIX_PL_Cert_GetSubjectAltNames <DNS>");
        expectedAscii = "east.sun.com";
        testAltNames(altNameDnsCert, altNameDnsCert_diff, expectedAscii);

        subTest("PKIX_PL_Cert_GetSubjectAltNames <RFC822>");
        expectedAscii = "alice.barnes@bcn.east.sun.com";
        testAltNames(altNameRfc822Cert, altNameRfc822Cert_diff, expectedAscii);

        /*
         *this should work once bugzilla bug #233586 is fixed.
         *subTest("PKIX_PL_Cert_GetSubjectAltNames <X400Address>");
         *expectedAscii = "X400Address: <DER-encoded value>";
         *testAltNames(altNameX400Cert, altNameX400Cert_diff, expectedAscii);
         */

        subTest("PKIX_PL_Cert_GetSubjectAltNames <DN>");
        expectedAscii = "CN=elley,OU=labs,O=sun,C=us";
        testAltNames(altNameDnCert, altNameDnCert_diff, expectedAscii);

        /*
         * this should work once bugzilla bug #233586 is fixed.
         * subTest("PKIX_PL_Cert_GetSubjectAltNames <EdiPartyName>");
         * expectedAscii = "EDIPartyName: <DER-encoded value>";
         * testAltNames(altNameEdiCert, altNameEdiCert_diff, expectedAscii);
         */

        subTest("PKIX_PL_Cert_GetSubjectAltNames <URI>");
        expectedAscii = "http://www.sun.com";
        testAltNames(altNameUriCert, altNameUriCert_diff, expectedAscii);

        subTest("PKIX_PL_Cert_GetSubjectAltNames <IP>");
        expectedAscii = "1.2.3.4";
        testAltNames(altNameIpCert, altNameIpCert_diff, expectedAscii);

        subTest("PKIX_PL_Cert_GetSubjectAltNames <OID>");
        expectedAscii = "1.2.39";
        testAltNames(altNameOidCert, altNameOidCert_diff, expectedAscii);

        subTest("PKIX_PL_Cert_GetSubjectAltNames <Other>");
        expectedAscii = "1.7.26.97";
        testAltNames(altNameOtherCert, altNameOtherCert_diff, expectedAscii);

        subTest("PKIX_PL_Cert_GetSubjectAltNames <none>");
        testAltNamesNone(altNameNoneCert);

        subTest("PKIX_PL_Cert_GetSubjectAltNames <Multiple>");
        testAltNamesMultiple();
}

static void testGetSubjectPublicKey(
        PKIX_PL_Cert *goodObject,
        PKIX_PL_Cert *equalObject,
        PKIX_PL_Cert *diffObject)
{
        PKIX_PL_PublicKey *goodPubKey = NULL;
        PKIX_PL_PublicKey *equalPubKey = NULL;
        PKIX_PL_PublicKey *diffPubKey = NULL;
        char *expectedAscii = "ANSI X9.57 DSA Signature";

        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_Cert_GetSubjectPublicKey");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubjectPublicKey
                (goodObject, &goodPubKey, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubjectPublicKey
                (equalObject, &equalPubKey, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubjectPublicKey
                (diffObject, &diffPubKey, plContext));

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (goodPubKey, equalPubKey, diffPubKey,
                expectedAscii, PublicKey, PKIX_TRUE);

cleanup:

        PKIX_TEST_DECREF_AC(goodPubKey);
        PKIX_TEST_DECREF_AC(equalPubKey);
        PKIX_TEST_DECREF_AC(diffPubKey);

        PKIX_TEST_RETURN();
}

static void testGetSubjectPublicKeyAlgId(PKIX_PL_Cert *goodObject){
        PKIX_PL_OID *pkixPubKeyOID = NULL;
        char *expectedAscii = "1.2.840.10040.4.1";

        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_Cert_GetSubjectPublicKeyAlgId");

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Cert_GetSubjectPublicKeyAlgId
                (goodObject, &pkixPubKeyOID, plContext));

        testToStringHelper
                ((PKIX_PL_Object *)pkixPubKeyOID, expectedAscii, plContext);

cleanup:

        PKIX_TEST_DECREF_AC(pkixPubKeyOID);
        PKIX_TEST_RETURN();
}

static void
testCritExtensionsPresent(PKIX_PL_Cert *cert)
{
        PKIX_List *critOIDList = NULL;
        char *firstOIDAscii = "2.5.29.15";
        PKIX_PL_OID *firstOID = NULL;
        char *secondOIDAscii = "2.5.29.19";
        PKIX_PL_OID *secondOID = NULL;

        PKIX_TEST_STD_VARS();


        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetCriticalExtensionOIDs
                (cert, &critOIDList, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (critOIDList, 0, (PKIX_PL_Object **)&firstOID, plContext));
        testToStringHelper
                ((PKIX_PL_Object *)firstOID, firstOIDAscii, plContext);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (critOIDList, 1, (PKIX_PL_Object **)&secondOID, plContext));

        testToStringHelper
                ((PKIX_PL_Object *)secondOID, secondOIDAscii, plContext);

cleanup:

        PKIX_TEST_DECREF_AC(critOIDList);
        PKIX_TEST_DECREF_AC(firstOID);
        PKIX_TEST_DECREF_AC(secondOID);

        PKIX_TEST_RETURN();
}

static void
testCritExtensionsAbsent(PKIX_PL_Cert *cert)
{
        PKIX_List *oidList = NULL;
        PKIX_Boolean empty;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetCriticalExtensionOIDs
                                    (cert, &oidList, plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_IsEmpty(oidList, &empty, plContext));

        if (!empty){
                pkixTestErrorMsg = "unexpected mismatch";
        }

cleanup:

        PKIX_TEST_DECREF_AC(oidList);
        PKIX_TEST_RETURN();
}


static void
testAllExtensionsAbsent(char *dataCentralDir)
{
        PKIX_List *oidList = NULL;
        PKIX_PL_Cert *cert = NULL;
        PKIX_Boolean empty;

        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_Cert_Create <noExtensionsCert>");
        cert = createCert(dataCentralDir, "noExtensionsCert", plContext);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetCriticalExtensionOIDs
                                    (cert, &oidList, plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_IsEmpty(oidList, &empty, plContext));

        if (!empty){
                pkixTestErrorMsg = "unexpected mismatch";
        }

cleanup:
        PKIX_TEST_DECREF_AC(oidList);
        PKIX_TEST_DECREF_AC(cert);

        PKIX_TEST_RETURN();
}

static void
testGetCriticalExtensionOIDs(char *dataCentralDir, PKIX_PL_Cert *goodObject)
{
        subTest("PKIX_PL_Cert_GetCriticalExtensionOIDs "
                "<CritExtensionsPresent>");
        testCritExtensionsPresent(goodObject);


        subTest("PKIX_PL_Cert_GetCriticalExtensionOIDs "
                "<CritExtensionsAbsent>");
        testCritExtensionsAbsent(altNameOidCert);


        subTest("PKIX_PL_Cert_GetCriticalExtensionOIDs "
                "<AllExtensionsAbsent>");
        testAllExtensionsAbsent(dataCentralDir);
}

static void
testKeyIdentifiersMatching(char *dataCentralDir)
{
        PKIX_PL_Cert *subjKeyIDCert = NULL;
        PKIX_PL_Cert *authKeyIDCert = NULL;
        PKIX_PL_ByteArray *subjKeyID = NULL;
        PKIX_PL_ByteArray *authKeyID = NULL;
        PKIX_PL_ByteArray *subjKeyID_diff = NULL;

        char *expectedAscii =
                "[116, 021, 213, 036, 028, 189, 094, 101, 136, 031, 225,"
                " 139, 009, 126, 127, 234, 025, 072, 078, 097]";

        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_Cert_Create <subjKeyIDCert>");
        subjKeyIDCert = createCert
                (dataCentralDir, "keyIdentifier/subjKeyIDCert", plContext);

        subTest("PKIX_PL_Cert_Create <authKeyIDCert>");
        authKeyIDCert = createCert
                (dataCentralDir, "keyIdentifier/authKeyIDCert", plContext);

        subTest("PKIX_PL_Cert_GetSubjectKeyIdentifier <good>");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubjectKeyIdentifier
                                    (subjKeyIDCert, &subjKeyID, plContext));

        subTest("PKIX_PL_Cert_GetAuthorityKeyIdentifier <equal>");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetAuthorityKeyIdentifier
                                    (authKeyIDCert, &authKeyID, plContext));

        subTest("PKIX_PL_Cert_GetSubjectKeyIdentifier <diff>");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Cert_GetSubjectKeyIdentifier
                (authKeyIDCert, &subjKeyID_diff, plContext));

        PKIX_TEST_EQ_HASH_TOSTR_DUP
            (subjKeyID,
            authKeyID,
            subjKeyID_diff,
            expectedAscii,
            ByteArray,
            PKIX_TRUE);

cleanup:

        PKIX_TEST_DECREF_AC(subjKeyIDCert);
        PKIX_TEST_DECREF_AC(authKeyIDCert);
        PKIX_TEST_DECREF_AC(subjKeyID);
        PKIX_TEST_DECREF_AC(authKeyID);
        PKIX_TEST_DECREF_AC(subjKeyID_diff);

        PKIX_TEST_RETURN();
}

static void
testKeyIdentifierAbsent(PKIX_PL_Cert *cert)
{
        PKIX_PL_ByteArray *subjKeyID = NULL;
        PKIX_PL_ByteArray *authKeyID = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubjectKeyIdentifier
                                    (cert, &subjKeyID, plContext));

        if (subjKeyID != NULL){
                pkixTestErrorMsg = "unexpected mismatch";
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetAuthorityKeyIdentifier
                                    (cert, &authKeyID, plContext));

        if (authKeyID != NULL){
                pkixTestErrorMsg = "unexpected mismatch";
        }


cleanup:

        PKIX_TEST_DECREF_AC(subjKeyID);
        PKIX_TEST_DECREF_AC(authKeyID);

        PKIX_TEST_RETURN();
}

static void
testGetKeyIdentifiers(char *dataCentralDir, PKIX_PL_Cert *goodObject)
{
        testKeyIdentifiersMatching(dataCentralDir);
        testKeyIdentifierAbsent(goodObject);
}

static void
testVerifyKeyUsage(
        char *dataCentralDir,
        char *dataDir,
        PKIX_PL_Cert *multiKeyUsagesCert)
{
        PKIX_PL_Cert *encipherOnlyCert = NULL;
        PKIX_PL_Cert *decipherOnlyCert = NULL;
        PKIX_PL_Cert *noKeyUsagesCert = NULL;
        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_Cert_Create <encipherOnlyCert>");
        encipherOnlyCert = createCert
                (dataCentralDir, "keyUsage/encipherOnlyCert", plContext);

        subTest("PKIX_PL_Cert_Create <decipherOnlyCert>");
        decipherOnlyCert = createCert
                (dataCentralDir, "keyUsage/decipherOnlyCert", plContext);

        subTest("PKIX_PL_Cert_Create <noKeyUsagesCert>");
        noKeyUsagesCert = createCert
                (dataCentralDir, "keyUsage/noKeyUsagesCert", plContext);

        subTest("PKIX_PL_Cert_VerifyKeyUsage <key_cert_sign>");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Cert_VerifyKeyUsage
                (multiKeyUsagesCert, PKIX_KEY_CERT_SIGN, plContext));

        subTest("PKIX_PL_Cert_VerifyKeyUsage <multiKeyUsages>");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_VerifyKeyUsage
                                (multiKeyUsagesCert,
                                PKIX_KEY_CERT_SIGN | PKIX_DIGITAL_SIGNATURE,
                                plContext));

        subTest("PKIX_PL_Cert_VerifyKeyUsage <encipher_only>");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Cert_VerifyKeyUsage
                (encipherOnlyCert, PKIX_ENCIPHER_ONLY, plContext));

        subTest("PKIX_PL_Cert_VerifyKeyUsage <noKeyUsages>");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Cert_VerifyKeyUsage
                (noKeyUsagesCert, PKIX_ENCIPHER_ONLY, plContext));

        subTest("PKIX_PL_Cert_VerifyKeyUsage <decipher_only>");
        PKIX_TEST_EXPECT_ERROR
                (PKIX_PL_Cert_VerifyKeyUsage
                (decipherOnlyCert, PKIX_DECIPHER_ONLY, plContext));

cleanup:
        PKIX_TEST_DECREF_AC(encipherOnlyCert);
        PKIX_TEST_DECREF_AC(decipherOnlyCert);
        PKIX_TEST_DECREF_AC(noKeyUsagesCert);

        PKIX_TEST_RETURN();
}

static void
testGetExtendedKeyUsage(char *dataCentralDir)
{

        PKIX_PL_Cert *codeSigningEKUCert = NULL;
        PKIX_PL_Cert *multiEKUCert = NULL;
        PKIX_PL_Cert *noEKUCert = NULL;
        PKIX_List *firstExtKeyUsage = NULL;
        PKIX_List *secondExtKeyUsage = NULL;
        PKIX_List *thirdExtKeyUsage = NULL;
        PKIX_PL_OID *firstOID = NULL;
        char *oidAscii = "1.3.6.1.5.5.7.3.3";
        PKIX_PL_OID *secondOID = NULL;
        char *secondOIDAscii = "1.3.6.1.5.5.7.3.1";
        PKIX_PL_OID *thirdOID = NULL;
        char *thirdOIDAscii = "1.3.6.1.5.5.7.3.2";
        PKIX_PL_OID *fourthOID = NULL;
        PKIX_UInt32 length = 0;

        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_Cert_Create <codeSigningEKUCert>");
        codeSigningEKUCert = createCert
                (dataCentralDir, "extKeyUsage/codeSigningEKUCert", plContext);

        subTest("PKIX_PL_Cert_Create <multiEKUCert>");
        multiEKUCert = createCert
                (dataCentralDir, "extKeyUsage/multiEKUCert", plContext);

        subTest("PKIX_PL_Cert_Create <noEKUCert>");
        noEKUCert = createCert
                (dataCentralDir, "extKeyUsage/noEKUCert", plContext);

        subTest("PKIX_PL_Cert_ExtendedKeyUsage <codeSigningEKU>");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetExtendedKeyUsage
                (codeSigningEKUCert, &firstExtKeyUsage, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (firstExtKeyUsage, 0, (PKIX_PL_Object **)&firstOID, plContext));
        testToStringHelper((PKIX_PL_Object *)firstOID, oidAscii, plContext);

        subTest("PKIX_PL_Cert_ExtendedKeyUsage <multiEKU>");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetExtendedKeyUsage
                (multiEKUCert, &secondExtKeyUsage, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength
                (secondExtKeyUsage, &length, plContext));

        if (length != 3){
                testError("unexpected mismatch");
                (void) printf("Actual value:\t%d\n", length);
                (void) printf("Expected value:\t3\n");
                goto cleanup;
        }

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_GetItem
                (secondExtKeyUsage,
                0,
                (PKIX_PL_Object **)&secondOID,
                plContext));

        testToStringHelper((PKIX_PL_Object *)secondOID, oidAscii, plContext);

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_GetItem
                (secondExtKeyUsage,
                1,
                (PKIX_PL_Object **)&thirdOID,
                plContext));

        testToStringHelper
                ((PKIX_PL_Object *)thirdOID, secondOIDAscii, plContext);

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_GetItem
                (secondExtKeyUsage,
                2,
                (PKIX_PL_Object **)&fourthOID,
                plContext));

        testToStringHelper
                ((PKIX_PL_Object *)fourthOID, thirdOIDAscii, plContext);

        subTest("PKIX_PL_Cert_ExtendedKeyUsage <noEKU>");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetExtendedKeyUsage
                (noEKUCert, &thirdExtKeyUsage, plContext));

        if (thirdExtKeyUsage != NULL){
                testError("unexpected mismatch");
                (void) printf("Actual value:\t%p\n", (void *)thirdExtKeyUsage);
                (void) printf("Expected value:\tNULL\n");
                goto cleanup;
        }

cleanup:

        PKIX_TEST_DECREF_AC(firstOID);
        PKIX_TEST_DECREF_AC(secondOID);
        PKIX_TEST_DECREF_AC(thirdOID);
        PKIX_TEST_DECREF_AC(fourthOID);

        PKIX_TEST_DECREF_AC(firstExtKeyUsage);
        PKIX_TEST_DECREF_AC(secondExtKeyUsage);
        PKIX_TEST_DECREF_AC(thirdExtKeyUsage);

        PKIX_TEST_DECREF_AC(codeSigningEKUCert);
        PKIX_TEST_DECREF_AC(multiEKUCert);
        PKIX_TEST_DECREF_AC(noEKUCert);

        PKIX_TEST_RETURN();
}

static void testMakeInheritedDSAPublicKey(char *dataCentralDir){
        PKIX_PL_PublicKey *firstKey = NULL;
        PKIX_PL_PublicKey *secondKey = NULL;
        PKIX_PL_PublicKey *resultKeyPositive = NULL;
        PKIX_PL_PublicKey *resultKeyNegative = NULL;
        PKIX_PL_Cert *firstCert = NULL;
        PKIX_PL_Cert *secondCert = NULL;

        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_Cert_Create <dsaWithoutParams>");
        firstCert = createCert
                (dataCentralDir, "publicKey/dsaWithoutParams", plContext);

        subTest("PKIX_PL_Cert_Create <dsaWithParams>");
        secondCert = createCert
                (dataCentralDir, "publicKey/dsaWithParams", plContext);

        subTest("PKIX_PL_Cert_GetSubjectPublicKey <firstKey>");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Cert_GetSubjectPublicKey
                (firstCert, &firstKey, plContext));

        subTest("PKIX_PL_Cert_GetSubjectPublicKey <secondKey>");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubjectPublicKey
                (secondCert, &secondKey, plContext));

        subTest("PKIX_PL_PublicKey_MakeInheritedDSAPublicKey <positive>");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_PublicKey_MakeInheritedDSAPublicKey
                (firstKey, secondKey, &resultKeyPositive, plContext));

        if (resultKeyPositive == NULL){
                testError("PKIX_PL_PublicKey_MakeInheritedDSAPublicKey failed");
        }

        subTest("PKIX_PL_PublicKey_MakeInheritedDSAPublicKey <negative>");
        PKIX_TEST_EXPECT_ERROR(PKIX_PL_PublicKey_MakeInheritedDSAPublicKey
                (firstKey, firstKey, &resultKeyNegative, plContext));

cleanup:

        PKIX_TEST_DECREF_AC(firstCert);
        PKIX_TEST_DECREF_AC(secondCert);

        PKIX_TEST_DECREF_AC(firstKey);
        PKIX_TEST_DECREF_AC(secondKey);
        PKIX_TEST_DECREF_AC(resultKeyPositive);
        PKIX_TEST_DECREF_AC(resultKeyNegative);

        PKIX_TEST_RETURN();
}

static void testVerifySignature(char *dataCentralDir){
        PKIX_PL_Cert *firstCert = NULL;
        PKIX_PL_Cert *secondCert = NULL;
        PKIX_PL_PublicKey *firstPubKey = NULL;
        PKIX_PL_PublicKey *secondPubKey = NULL;

        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_Cert_Create <labs2yassir>");
        firstCert = createCert(dataCentralDir, "publicKey/labs2yassir", plContext);

        subTest("PKIX_PL_Cert_Create <yassir2labs>");
        secondCert = createCert(dataCentralDir, "publicKey/yassir2labs", plContext);

        subTest("PKIX_PL_Cert_GetSubjectPublicKey <labs2yassir>");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Cert_GetSubjectPublicKey
                (firstCert, &firstPubKey, plContext));

        subTest("PKIX_PL_Cert_GetSubjectPublicKey <yassir2labs>");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Cert_GetSubjectPublicKey
                (secondCert, &secondPubKey, plContext));

        subTest("PKIX_PL_Cert_VerifySignature <positive>");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Cert_VerifySignature
                (secondCert, firstPubKey, plContext));

        subTest("PKIX_PL_Cert_VerifySignature <negative>");
        PKIX_TEST_EXPECT_ERROR
                (PKIX_PL_Cert_VerifySignature
                (secondCert, secondPubKey, plContext));

cleanup:

        PKIX_TEST_DECREF_AC(firstCert);
        PKIX_TEST_DECREF_AC(secondCert);
        PKIX_TEST_DECREF_AC(firstPubKey);
        PKIX_TEST_DECREF_AC(secondPubKey);

        PKIX_TEST_RETURN();
}

static void
testCheckValidity(
        PKIX_PL_Cert *olderCert,
        PKIX_PL_Cert *newerCert)
{
        /*
         * olderCert has the following Validity:
         *  notBefore = August 19, 1999: 20:19:56 GMT
         *  notAfter  = August 18, 2000: 20:19:56 GMT
         *
         * newerCert has the following Validity:
         *  notBefore = November 13, 2003: 16:46:03 GMT
         *  notAfter  = February 13, 2009: 16:46:03 GMT
         */

        /*  olderDateAscii = March    29, 2000: 13:48:47 GMT */
        char *olderAscii = "000329134847Z";
        PKIX_PL_String *olderString = NULL;
        PKIX_PL_Date *olderDate = NULL;

        /*  newerDateAscii = March    29, 2004: 13:48:47 GMT */
        char *newerAscii = "040329134847Z";
        PKIX_PL_String *newerString = NULL;
        PKIX_PL_Date *newerDate = NULL;

        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_Cert_CheckValidity <creating Dates>");

        /* create newer date when newer cert is valid but older cert is not */
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
            (PKIX_ESCASCII, newerAscii, 0, &newerString, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Date_Create_UTCTime
                (newerString, &newerDate, plContext));

        /* create older date when older cert is valid but newer cert is not */
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
            (PKIX_ESCASCII, olderAscii, 0, &olderString, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Date_Create_UTCTime
                (olderString, &olderDate, plContext));

        /* check validity of both certificates using olderDate */
        subTest("PKIX_PL_Cert_CheckValidity <olderDate:positive>");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Cert_CheckValidity(olderCert, olderDate, plContext));

        subTest("PKIX_PL_Cert_CheckValidity <olderDate:negative>");
        PKIX_TEST_EXPECT_ERROR
                (PKIX_PL_Cert_CheckValidity(newerCert, olderDate, plContext));

        /* check validity of both certificates using newerDate */
        subTest("PKIX_PL_Cert_CheckValidity <newerDate:positive>");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Cert_CheckValidity(newerCert, newerDate, plContext));

        subTest("PKIX_PL_Cert_CheckValidity <newerDate:negative>");
        PKIX_TEST_EXPECT_ERROR
                (PKIX_PL_Cert_CheckValidity(olderCert, newerDate, plContext));

        /*
         * check validity of both certificates using current time
         * NOTE: these "now" tests will not work when the current
         * time is after newerCert.notAfter (~ February 13, 2009)
         */
        subTest("PKIX_PL_Cert_CheckValidity <now:positive>");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Cert_CheckValidity(newerCert, NULL, plContext));

        subTest("PKIX_PL_Cert_CheckValidity <now:negative>");
        PKIX_TEST_EXPECT_ERROR
                (PKIX_PL_Cert_CheckValidity(olderCert, NULL, plContext));

cleanup:
        PKIX_TEST_DECREF_AC(olderString);
        PKIX_TEST_DECREF_AC(newerString);
        PKIX_TEST_DECREF_AC(olderDate);
        PKIX_TEST_DECREF_AC(newerDate);

        PKIX_TEST_RETURN();
}

static void
readCertBasicConstraints(
        char *dataDir,
        char *goodCertName,
        char *diffCertName,
        PKIX_PL_CertBasicConstraints **goodBasicConstraints,
        PKIX_PL_CertBasicConstraints **equalBasicConstraints,
        PKIX_PL_CertBasicConstraints **diffBasicConstraints){

        PKIX_PL_Cert *goodCert = NULL;
        PKIX_PL_Cert *equalCert = NULL;
        PKIX_PL_Cert *diffCert = NULL;

        PKIX_TEST_STD_VARS();

        createCerts(dataDir, goodCertName, diffCertName,
                &goodCert, &equalCert, &diffCert);
        /*
         * Warning: pointer will be NULL if BasicConstraints
         * extension is not present in the certificate. */
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetBasicConstraints
                (goodCert, goodBasicConstraints, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetBasicConstraints
                (equalCert, equalBasicConstraints, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetBasicConstraints
                (diffCert, diffBasicConstraints, plContext));

cleanup:

        PKIX_TEST_DECREF_AC(goodCert);
        PKIX_TEST_DECREF_AC(equalCert);
        PKIX_TEST_DECREF_AC(diffCert);

        PKIX_TEST_RETURN();
}

static void
testBasicConstraintsHelper(
        char *dataDir,
        char *goodCertName,
        char *diffCertName,
        char *expectedAscii)
{
        PKIX_PL_CertBasicConstraints *goodBasicConstraints = NULL;
        PKIX_PL_CertBasicConstraints *equalBasicConstraints = NULL;
        PKIX_PL_CertBasicConstraints *diffBasicConstraints = NULL;

        PKIX_TEST_STD_VARS();

        readCertBasicConstraints
                (dataDir,
                goodCertName,
                diffCertName,
                &goodBasicConstraints,
                &equalBasicConstraints,
                &diffBasicConstraints);

        /*
         * The standard test macro is applicable only
         * if BasicConstraint extension is present
         * in the certificate. Otherwise some
         * pointers will be null.
         */
        if ((goodBasicConstraints) &&
            (equalBasicConstraints) &&
            (diffBasicConstraints)) {
                PKIX_TEST_EQ_HASH_TOSTR_DUP
                        (goodBasicConstraints,
                        equalBasicConstraints,
                        diffBasicConstraints,
                        expectedAscii,
                        BasicConstraints,
                        PKIX_TRUE);
        } else {
            /* Test what we can */
            if (goodBasicConstraints) {
                if (!equalBasicConstraints) {
                    testError
                        ("Unexpected NULL value of equalBasicConstraints");
                    goto cleanup;
                }
                subTest("PKIX_PL_BasicConstraints_Equals   <match>");
                testEqualsHelper
                    ((PKIX_PL_Object *)(goodBasicConstraints),
                    (PKIX_PL_Object *)(equalBasicConstraints),
                    PKIX_TRUE,
                    plContext);
                subTest("PKIX_PL_BasicConstraints_Hashcode <match>");
                testHashcodeHelper
                    ((PKIX_PL_Object *)(goodBasicConstraints),
                    (PKIX_PL_Object *)(equalBasicConstraints),
                    PKIX_TRUE,
                    plContext);
                if (diffBasicConstraints) {
                    subTest("PKIX_PL_BasicConstraints_Equals   <non-match>");
                    testEqualsHelper
                        ((PKIX_PL_Object *)(goodBasicConstraints),
                        (PKIX_PL_Object *)(diffBasicConstraints),
                        PKIX_FALSE,
                        plContext);
                    subTest("PKIX_PL_BasicConstraints_Hashcode <non-match>");
                    testHashcodeHelper
                        ((PKIX_PL_Object *)(goodBasicConstraints),
                        (PKIX_PL_Object *)(diffBasicConstraints),
                        PKIX_FALSE,
                        plContext);
                }
                subTest("PKIX_PL_BasicConstraints_Duplicate");
                testDuplicateHelper
                        ((PKIX_PL_Object *)goodBasicConstraints, plContext);
            }
            if (expectedAscii) {
                subTest("PKIX_PL_BasicConstraints_ToString");
                testToStringHelper
                    ((PKIX_PL_Object *)(goodBasicConstraints),
                    expectedAscii,
                    plContext);
            }
        }

cleanup:

        PKIX_TEST_DECREF_AC(goodBasicConstraints);
        PKIX_TEST_DECREF_AC(equalBasicConstraints);
        PKIX_TEST_DECREF_AC(diffBasicConstraints);

        PKIX_TEST_RETURN();
}

static void
testBasicConstraints_GetCAFlag(char *dataCentralDir)
{
        /*
         * XXX  When we have a certificate with a non-null Basic
         * Constraints field and a value of FALSE for CAFlag,
         * this test should be modified to use that
         * certificate for diffCertName, and to verify that
         * GetCAFlag returns a FALSE value. But our certificates for
         * non-CAs are created with no BasicConstraints extension.
         */
        PKIX_PL_CertBasicConstraints *goodBasicConstraints = NULL;
        PKIX_PL_CertBasicConstraints *equalBasicConstraints = NULL;
        PKIX_PL_CertBasicConstraints *diffBasicConstraints = NULL;
        char *goodCertName = "yassir2yassir";
        char *diffCertName = "nss2alice";
        PKIX_Boolean goodCAFlag = PKIX_FALSE;
        PKIX_Boolean diffCAFlag = PKIX_FALSE;

        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_BasicConstraints_GetCAFlag");

        readCertBasicConstraints
                (dataCentralDir,
                goodCertName,
                diffCertName,
                &goodBasicConstraints,
                &equalBasicConstraints,
                &diffBasicConstraints);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_BasicConstraints_GetCAFlag
                (goodBasicConstraints, &goodCAFlag, plContext));
        if (!goodCAFlag) {
                testError("BasicConstraint CAFlag unexpectedly FALSE");
                goto cleanup;
        }

        if (diffBasicConstraints) {
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_BasicConstraints_GetCAFlag
                        (diffBasicConstraints, &diffCAFlag, plContext));
                if (diffCAFlag) {
                        testError("BasicConstraint CAFlag unexpectedly TRUE");
                        goto cleanup;
                }
        }

cleanup:

        PKIX_TEST_DECREF_AC(goodBasicConstraints);
        PKIX_TEST_DECREF_AC(equalBasicConstraints);
        PKIX_TEST_DECREF_AC(diffBasicConstraints);

        PKIX_TEST_RETURN();
}

static void
testBasicConstraints_GetPathLenConstraint(char *dataCentralDir)
{
        PKIX_PL_CertBasicConstraints *goodBasicConstraints = NULL;
        PKIX_PL_CertBasicConstraints *equalBasicConstraints = NULL;
        PKIX_PL_CertBasicConstraints *diffBasicConstraints = NULL;
        char *goodCertName = "yassir2yassir";
        char *diffCertName = "sun2sun";
        PKIX_Int32 goodPathLen = 0;
        PKIX_Int32 diffPathLen = 0;

        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_BasicConstraints_GetPathLenConstraint");

        readCertBasicConstraints
                (dataCentralDir,
                goodCertName,
                diffCertName,
                &goodBasicConstraints,
                &equalBasicConstraints,
                &diffBasicConstraints);

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_BasicConstraints_GetPathLenConstraint
                (goodBasicConstraints, &goodPathLen, plContext));
        if (0 != goodPathLen) {
                testError("unexpected basicConstraint pathLen");
                (void) printf("Actual value:\t%d\n", goodPathLen);
                (void) printf("Expected value:\t0\n");
                goto cleanup;
        }

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_BasicConstraints_GetPathLenConstraint
                (diffBasicConstraints, &diffPathLen, plContext));
        if (1 != diffPathLen) {
                testError("unexpected basicConstraint pathLen");
                (void) printf("Actual value:\t%d\n", diffPathLen);
                (void) printf("Expected value:\t1\n");
                goto cleanup;
        }

cleanup:

        PKIX_TEST_DECREF_AC(goodBasicConstraints);
        PKIX_TEST_DECREF_AC(equalBasicConstraints);
        PKIX_TEST_DECREF_AC(diffBasicConstraints);

        PKIX_TEST_RETURN();
}

static void
testGetBasicConstraints(char *dataCentralDir)
{
        char *goodCertName = NULL;
        char *diffCertName = NULL;

        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_Cert_GetBasicConstraints <CA(0) and non-CA>");
        goodCertName = "yassir2yassir";
        diffCertName = "nss2alice";
        testBasicConstraintsHelper
                (dataCentralDir, goodCertName, diffCertName, "CA(0)");

        subTest("PKIX_PL_Cert_GetBasicConstraints <non-CA and CA(1)>");
        goodCertName = "nss2alice";
        diffCertName = "sun2sun";
        testBasicConstraintsHelper
                (dataCentralDir, goodCertName, diffCertName, NULL);

        subTest("PKIX_PL_Cert_GetBasicConstraints <CA(0) and CA(1)>");
        goodCertName = "yassir2bcn";
        diffCertName = "sun2sun";
        testBasicConstraintsHelper
                (dataCentralDir, goodCertName, diffCertName, "CA(0)");

        subTest("PKIX_PL_Cert_GetBasicConstraints <CA(-1) and CA(1)>");
        goodCertName = "anchor2dsa";
        diffCertName = "sun2sun";
        testBasicConstraintsHelper
                (dataCentralDir, goodCertName, diffCertName, "CA(-1)");

        PKIX_TEST_RETURN();
}

static void
testGetPolicyInformation(char *dataDir)
{

        char *goodCertName =
                "UserNoticeQualifierTest15EE.crt";
        char *equalCertName =
                "UserNoticeQualifierTest15EE.crt";
        char *diffCertName =
                "UserNoticeQualifierTest17EE.crt";
        PKIX_Boolean isImmutable = PKIX_FALSE;
        PKIX_PL_Cert *goodCert = NULL;
        PKIX_PL_Cert *equalCert = NULL;
        PKIX_PL_Cert *diffCert = NULL;
        PKIX_List* goodPolicyInfo = NULL;
        PKIX_List* equalPolicyInfo = NULL;
        PKIX_List* diffPolicyInfo = NULL;
        PKIX_PL_CertPolicyInfo *goodPolicy = NULL;
        PKIX_PL_CertPolicyInfo *equalPolicy = NULL;
        PKIX_PL_CertPolicyInfo *diffPolicy = NULL;

        PKIX_TEST_STD_VARS();
        subTest("PKIX_PL_Cert_GetPolicyInformation");

        /*
         * Get the cert, then the list of policyInfos.
         * Take the first policyInfo from the list.
         */

        /* Get the PolicyInfo objects */
        goodCert = createCert(dataDir, goodCertName, plContext);
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetPolicyInformation
                (goodCert, &goodPolicyInfo, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (goodPolicyInfo, 0, (PKIX_PL_Object **)&goodPolicy, plContext));

        equalCert = createCert(dataDir, equalCertName, plContext);
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetPolicyInformation
                (equalCert, &equalPolicyInfo, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_GetItem
                (equalPolicyInfo,
                0,
                (PKIX_PL_Object **)&equalPolicy,
                plContext));

        diffCert = createCert(dataDir, diffCertName, plContext);
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetPolicyInformation
                (diffCert, &diffPolicyInfo, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (diffPolicyInfo, 0, (PKIX_PL_Object **)&diffPolicy, plContext));

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (goodPolicy,
                equalPolicy,
                diffPolicy,
                NULL,
                CertPolicyInfo,
                PKIX_FALSE);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_IsImmutable
                (goodPolicyInfo, &isImmutable, plContext));

        if (isImmutable != PKIX_TRUE) {
                testError("PolicyInfo List is not immutable");
        }

cleanup:

        PKIX_TEST_DECREF_AC(goodPolicy);
        PKIX_TEST_DECREF_AC(equalPolicy);
        PKIX_TEST_DECREF_AC(diffPolicy);
        PKIX_TEST_DECREF_AC(goodPolicyInfo);
        PKIX_TEST_DECREF_AC(equalPolicyInfo);
        PKIX_TEST_DECREF_AC(diffPolicyInfo);
        PKIX_TEST_DECREF_AC(goodCert);
        PKIX_TEST_DECREF_AC(equalCert);
        PKIX_TEST_DECREF_AC(diffCert);
        PKIX_TEST_RETURN();
}

static void
testCertPolicy_GetPolicyId(char *dataDir)
{
        char *goodCertName =
                "UserNoticeQualifierTest15EE.crt";
        char *equalCertName =
                "UserNoticeQualifierTest16EE.crt";
        char *diffCertName =
                "UserNoticeQualifierTest17EE.crt";
        PKIX_PL_Cert *goodCert = NULL;
        PKIX_PL_Cert *equalCert = NULL;
        PKIX_PL_Cert *diffCert = NULL;
        PKIX_List* goodPolicyInfo = NULL;
        PKIX_List* equalPolicyInfo = NULL;
        PKIX_List* diffPolicyInfo = NULL;
        PKIX_PL_CertPolicyInfo *goodPolicy = NULL;
        PKIX_PL_CertPolicyInfo *equalPolicy = NULL;
        PKIX_PL_CertPolicyInfo *diffPolicy = NULL;
        PKIX_PL_OID *goodID = NULL;
        PKIX_PL_OID *equalID = NULL;
        PKIX_PL_OID *diffID = NULL;

        PKIX_TEST_STD_VARS();
        subTest("PKIX_PL_CertPolicyInfo_GetPolicyId");

        /*
         * Get the cert, then the list of policyInfos.
         * Take the first policyInfo from the list.
         * Finally, get the policyInfo's ID.
         */

        /* Get the PolicyInfo objects */
        goodCert = createCert(dataDir, goodCertName, plContext);
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetPolicyInformation
                (goodCert, &goodPolicyInfo, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (goodPolicyInfo, 0, (PKIX_PL_Object **)&goodPolicy, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CertPolicyInfo_GetPolicyId
                (goodPolicy, &goodID, plContext));

        equalCert = createCert(dataDir, equalCertName, plContext);
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetPolicyInformation
                (equalCert, &equalPolicyInfo, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (equalPolicyInfo,
                0,
                (PKIX_PL_Object **)&equalPolicy,
                plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CertPolicyInfo_GetPolicyId
                (equalPolicy, &equalID, plContext));

        diffCert = createCert(dataDir, diffCertName, plContext);
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetPolicyInformation
                (diffCert, &diffPolicyInfo, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (diffPolicyInfo, 0, (PKIX_PL_Object **)&diffPolicy, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CertPolicyInfo_GetPolicyId
                (diffPolicy, &diffID, plContext));

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (goodID, equalID, diffID, NULL, OID, PKIX_FALSE);

cleanup:

        PKIX_TEST_DECREF_AC(goodID);
        PKIX_TEST_DECREF_AC(equalID);
        PKIX_TEST_DECREF_AC(diffID);
        PKIX_TEST_DECREF_AC(goodPolicy);
        PKIX_TEST_DECREF_AC(equalPolicy);
        PKIX_TEST_DECREF_AC(diffPolicy);
        PKIX_TEST_DECREF_AC(goodPolicyInfo);
        PKIX_TEST_DECREF_AC(equalPolicyInfo);
        PKIX_TEST_DECREF_AC(diffPolicyInfo);
        PKIX_TEST_DECREF_AC(goodCert);
        PKIX_TEST_DECREF_AC(equalCert);
        PKIX_TEST_DECREF_AC(diffCert);
        PKIX_TEST_RETURN();
}

static void
testCertPolicy_GetPolQualifiers(char *dataDir)
{
        char *goodCertName =
                "UserNoticeQualifierTest15EE.crt";
        char *equalCertName =
                "UserNoticeQualifierTest16EE.crt";
        char *diffCertName =
                "UserNoticeQualifierTest18EE.crt";
        PKIX_Boolean isImmutable = PKIX_FALSE;
        PKIX_PL_Cert *goodCert = NULL;
        PKIX_PL_Cert *equalCert = NULL;
        PKIX_PL_Cert *diffCert = NULL;
        PKIX_List* goodPolicyInfo = NULL;
        PKIX_List* equalPolicyInfo = NULL;
        PKIX_List* diffPolicyInfo = NULL;
        PKIX_PL_CertPolicyInfo *goodPolicy = NULL;
        PKIX_PL_CertPolicyInfo *equalPolicy = NULL;
        PKIX_PL_CertPolicyInfo *diffPolicy = NULL;
        PKIX_List* goodQuals = NULL;
        PKIX_List* equalQuals = NULL;
        PKIX_List* diffQuals = NULL;

        PKIX_TEST_STD_VARS();
        subTest("PKIX_PL_CertPolicyInfo_GetPolQualifiers");

        /*
         * Get the cert, then the list of policyInfos.
         * Take the first policyInfo from the list.
         * Get its list of PolicyQualifiers.
         */

        /* Get the PolicyInfo objects */
        goodCert = createCert(dataDir, goodCertName, plContext);
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetPolicyInformation
                (goodCert, &goodPolicyInfo, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (goodPolicyInfo, 0, (PKIX_PL_Object **)&goodPolicy, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CertPolicyInfo_GetPolQualifiers
                (goodPolicy, &goodQuals, plContext));

        equalCert = createCert(dataDir, equalCertName, plContext);
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetPolicyInformation
                (equalCert, &equalPolicyInfo, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_GetItem
                (equalPolicyInfo,
                0,
                (PKIX_PL_Object **)&equalPolicy,
                plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CertPolicyInfo_GetPolQualifiers
                (equalPolicy, &equalQuals, plContext));

        diffCert = createCert(dataDir, diffCertName, plContext);
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetPolicyInformation
                (diffCert, &diffPolicyInfo, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (diffPolicyInfo, 0, (PKIX_PL_Object **)&diffPolicy, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CertPolicyInfo_GetPolQualifiers
                (diffPolicy, &diffQuals, plContext));

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (goodQuals,
                equalQuals,
                diffQuals,
                NULL,
                List,
                PKIX_FALSE);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_IsImmutable
                (goodQuals, &isImmutable, plContext));

        if (isImmutable != PKIX_TRUE) {
                testError("PolicyQualifier List is not immutable");
        }

cleanup:

        PKIX_TEST_DECREF_AC(goodCert);
        PKIX_TEST_DECREF_AC(goodPolicyInfo);
        PKIX_TEST_DECREF_AC(goodPolicy);
        PKIX_TEST_DECREF_AC(goodQuals);
        PKIX_TEST_DECREF_AC(equalCert);
        PKIX_TEST_DECREF_AC(equalPolicyInfo);
        PKIX_TEST_DECREF_AC(equalQuals);
        PKIX_TEST_DECREF_AC(equalPolicy);
        PKIX_TEST_DECREF_AC(diffCert);
        PKIX_TEST_DECREF_AC(diffPolicyInfo);
        PKIX_TEST_DECREF_AC(diffPolicy);
        PKIX_TEST_DECREF_AC(diffQuals);

        PKIX_TEST_RETURN();
}

static void
testPolicyQualifier_GetQualifier(char *dataDir)
{
        char *goodCertName =
                "UserNoticeQualifierTest15EE.crt";
        char *equalCertName =
                "UserNoticeQualifierTest16EE.crt";
        char *diffCertName =
                "UserNoticeQualifierTest18EE.crt";
        PKIX_PL_Cert *goodCert = NULL;
        PKIX_PL_Cert *equalCert = NULL;
        PKIX_PL_Cert *diffCert = NULL;
        PKIX_List* goodPolicyInfo = NULL;
        PKIX_List* equalPolicyInfo = NULL;
        PKIX_List* diffPolicyInfo = NULL;
        PKIX_PL_CertPolicyInfo *goodPolicy = NULL;
        PKIX_PL_CertPolicyInfo *equalPolicy = NULL;
        PKIX_PL_CertPolicyInfo *diffPolicy = NULL;
        PKIX_List* goodQuals = NULL;
        PKIX_List* equalQuals = NULL;
        PKIX_List* diffQuals = NULL;
        PKIX_PL_CertPolicyQualifier *goodPolQualifier = NULL;
        PKIX_PL_CertPolicyQualifier *equalPolQualifier = NULL;
        PKIX_PL_CertPolicyQualifier *diffPolQualifier = NULL;
        PKIX_PL_ByteArray *goodArray = NULL;
        PKIX_PL_ByteArray *equalArray = NULL;
        PKIX_PL_ByteArray *diffArray = NULL;

        PKIX_TEST_STD_VARS();
        subTest("PKIX_PL_PolicyQualifier_GetQualifier");

        /*
         * Get the cert, then the list of policyInfos.
         * Take the first policyInfo from the list.
         * Get its list of PolicyQualifiers.
         * Take the first policyQualifier from the list.
         * Finally, get the policyQualifier's ByteArray.
         */

        /* Get the PolicyInfo objects */
        goodCert = createCert(dataDir, goodCertName, plContext);
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetPolicyInformation
                (goodCert, &goodPolicyInfo, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (goodPolicyInfo, 0, (PKIX_PL_Object **)&goodPolicy, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CertPolicyInfo_GetPolQualifiers
                (goodPolicy, &goodQuals, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_GetItem
                (goodQuals,
                0,
                (PKIX_PL_Object **)&goodPolQualifier,
                plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_PolicyQualifier_GetQualifier
                (goodPolQualifier, &goodArray, plContext));

        equalCert = createCert(dataDir, equalCertName, plContext);
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetPolicyInformation
                (equalCert, &equalPolicyInfo, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (equalPolicyInfo,
                0,
                (PKIX_PL_Object **)&equalPolicy,
                plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CertPolicyInfo_GetPolQualifiers
                (equalPolicy, &equalQuals, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_GetItem
                (equalQuals,
                0,
                (PKIX_PL_Object **)&equalPolQualifier,
                plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_PolicyQualifier_GetQualifier
                (equalPolQualifier, &equalArray, plContext));

        diffCert = createCert(dataDir, diffCertName, plContext);
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetPolicyInformation
                (diffCert, &diffPolicyInfo, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (diffPolicyInfo, 0, (PKIX_PL_Object **)&diffPolicy, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CertPolicyInfo_GetPolQualifiers
                (diffPolicy, &diffQuals, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_GetItem
                (diffQuals,
                0,
                (PKIX_PL_Object **)&diffPolQualifier,
                plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_PolicyQualifier_GetQualifier
                (diffPolQualifier, &diffArray, plContext));

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (goodArray, equalArray, diffArray, NULL, ByteArray, PKIX_FALSE);

cleanup:

        PKIX_TEST_DECREF_AC(goodArray);
        PKIX_TEST_DECREF_AC(equalArray);
        PKIX_TEST_DECREF_AC(diffArray);
        PKIX_TEST_DECREF_AC(goodPolQualifier);
        PKIX_TEST_DECREF_AC(equalPolQualifier);
        PKIX_TEST_DECREF_AC(diffPolQualifier);
        PKIX_TEST_DECREF_AC(goodQuals);
        PKIX_TEST_DECREF_AC(equalQuals);
        PKIX_TEST_DECREF_AC(diffQuals);
        PKIX_TEST_DECREF_AC(goodPolicy);
        PKIX_TEST_DECREF_AC(equalPolicy);
        PKIX_TEST_DECREF_AC(diffPolicy);
        PKIX_TEST_DECREF_AC(goodPolicyInfo);
        PKIX_TEST_DECREF_AC(equalPolicyInfo);
        PKIX_TEST_DECREF_AC(diffPolicyInfo);
        PKIX_TEST_DECREF_AC(goodCert);
        PKIX_TEST_DECREF_AC(equalCert);
        PKIX_TEST_DECREF_AC(diffCert);
        PKIX_TEST_RETURN();
}

static void
testPolicyQualifier_GetPolicyQualifierId(char *dataDir)
{
        char *goodCertName =
                "UserNoticeQualifierTest15EE.crt";
        char *equalCertName =
                "UserNoticeQualifierTest16EE.crt";
        char *diffCertName =
                "CPSPointerQualifierTest20EE.crt";
        PKIX_PL_Cert *goodCert = NULL;
        PKIX_PL_Cert *equalCert = NULL;
        PKIX_PL_Cert *diffCert = NULL;
        PKIX_List* goodPolicyInfo = NULL;
        PKIX_List* equalPolicyInfo = NULL;
        PKIX_List* diffPolicyInfo = NULL;
        PKIX_PL_CertPolicyInfo *goodPolicy = NULL;
        PKIX_PL_CertPolicyInfo *equalPolicy = NULL;
        PKIX_PL_CertPolicyInfo *diffPolicy = NULL;
        PKIX_List* goodQuals = NULL;
        PKIX_List* equalQuals = NULL;
        PKIX_List* diffQuals = NULL;
        PKIX_PL_CertPolicyQualifier *goodPolQualifier = NULL;
        PKIX_PL_CertPolicyQualifier *equalPolQualifier = NULL;
        PKIX_PL_CertPolicyQualifier *diffPolQualifier = NULL;
        PKIX_PL_OID *goodID = NULL;
        PKIX_PL_OID *equalID = NULL;
        PKIX_PL_OID *diffID = NULL;

        PKIX_TEST_STD_VARS();
        subTest("PKIX_PL_PolicyQualifier_GetPolicyQualifierId");

        /*
         * Get the cert, then the list of policyInfos.
         * Take the first policyInfo from the list.
         * Get its list of PolicyQualifiers.
         * Take the first policyQualifier from the list.
         * Finally, get the policyQualifier's ID.
         */

        /* Get the PolicyQualifier objects */
        goodCert = createCert(dataDir, goodCertName, plContext);
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetPolicyInformation
                (goodCert, &goodPolicyInfo, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (goodPolicyInfo, 0, (PKIX_PL_Object **)&goodPolicy, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CertPolicyInfo_GetPolQualifiers
                (goodPolicy, &goodQuals, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_GetItem
                (goodQuals,
                0,
                (PKIX_PL_Object **)&goodPolQualifier,
                plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_PolicyQualifier_GetPolicyQualifierId
                (goodPolQualifier, &goodID, plContext));

        equalCert = createCert(dataDir, equalCertName, plContext);
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetPolicyInformation
                (equalCert, &equalPolicyInfo, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (equalPolicyInfo,
                0,
                (PKIX_PL_Object **)&equalPolicy,
                plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CertPolicyInfo_GetPolQualifiers
                (equalPolicy, &equalQuals, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_GetItem
                (equalQuals,
                0,
                (PKIX_PL_Object **)&equalPolQualifier,
                plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_PolicyQualifier_GetPolicyQualifierId
                (equalPolQualifier, &equalID, plContext));

        diffCert = createCert(dataDir, diffCertName, plContext);
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetPolicyInformation
                (diffCert, &diffPolicyInfo, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (diffPolicyInfo, 0, (PKIX_PL_Object **)&diffPolicy, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CertPolicyInfo_GetPolQualifiers
                (diffPolicy, &diffQuals, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_List_GetItem
                (diffQuals,
                0,
                (PKIX_PL_Object **)&diffPolQualifier,
                plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_PolicyQualifier_GetPolicyQualifierId
                (diffPolQualifier, &diffID, plContext));

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (goodID, equalID, diffID, NULL, OID, PKIX_FALSE);

cleanup:

        PKIX_TEST_DECREF_AC(goodID);
        PKIX_TEST_DECREF_AC(equalID);
        PKIX_TEST_DECREF_AC(diffID);
        PKIX_TEST_DECREF_AC(goodPolQualifier);
        PKIX_TEST_DECREF_AC(equalPolQualifier);
        PKIX_TEST_DECREF_AC(diffPolQualifier);
        PKIX_TEST_DECREF_AC(goodQuals);
        PKIX_TEST_DECREF_AC(equalQuals);
        PKIX_TEST_DECREF_AC(diffQuals);
        PKIX_TEST_DECREF_AC(goodPolicy);
        PKIX_TEST_DECREF_AC(equalPolicy);
        PKIX_TEST_DECREF_AC(diffPolicy);
        PKIX_TEST_DECREF_AC(goodPolicyInfo);
        PKIX_TEST_DECREF_AC(equalPolicyInfo);
        PKIX_TEST_DECREF_AC(diffPolicyInfo);
        PKIX_TEST_DECREF_AC(goodCert);
        PKIX_TEST_DECREF_AC(equalCert);
        PKIX_TEST_DECREF_AC(diffCert);
        PKIX_TEST_RETURN();
}

static void
testAreCertPoliciesCritical(char *dataCentralDir, char *dataDir)
{

        char *trueCertName = "CertificatePoliciesCritical.crt";
        char *falseCertName = "UserNoticeQualifierTest15EE.crt";
        PKIX_PL_Cert *trueCert = NULL;
        PKIX_PL_Cert *falseCert = NULL;
        PKIX_Boolean trueVal = PKIX_FALSE;
        PKIX_Boolean falseVal = PKIX_FALSE;

        PKIX_TEST_STD_VARS();
        subTest("PKIX_PL_Cert_AreCertPoliciesCritical - <true>");

        trueCert = createCert(dataCentralDir, trueCertName, plContext);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_AreCertPoliciesCritical
                (trueCert, &trueVal, plContext));

        if (trueVal != PKIX_TRUE) {
                testError("unexpected mismatch");
                (void) printf("Actual value:\t%d\n", trueVal);
                (void) printf("Expected value:\t1\n");
                goto cleanup;
        }

        subTest("PKIX_PL_Cert_AreCertPoliciesCritical - <false>");

        falseCert = createCert(dataDir, falseCertName, plContext);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_AreCertPoliciesCritical
                (falseCert, &falseVal, plContext));

        if (falseVal != PKIX_FALSE) {
                testError("unexpected mismatch");
                (void) printf("Actual value:\t%d\n", falseVal);
                (void) printf("Expected value:\t0\n");
                goto cleanup;
        }

cleanup:

        PKIX_TEST_DECREF_AC(trueCert);
        PKIX_TEST_DECREF_AC(falseCert);

        PKIX_TEST_RETURN();
}

static void
testCertPolicyConstraints(char *dataDir)
{
        char *requireExplicitPolicy2CertName =
                "requireExplicitPolicy2CACert.crt";
        char *inhibitPolicyMapping5CertName =
                "inhibitPolicyMapping5CACert.crt";
        char *inhibitAnyPolicy5CertName =
                "inhibitAnyPolicy5CACert.crt";
        char *inhibitAnyPolicy0CertName =
                "inhibitAnyPolicy0CACert.crt";
        PKIX_PL_Cert *requireExplicitPolicy2Cert = NULL;
        PKIX_PL_Cert *inhibitPolicyMapping5Cert = NULL;
        PKIX_PL_Cert *inhibitAnyPolicy5Cert = NULL;
        PKIX_PL_Cert *inhibitAnyPolicy0Cert = NULL;
        PKIX_Int32 skipCerts = 0;

        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_Cert_GetRequireExplicitPolicy");
        requireExplicitPolicy2Cert = createCert
                (dataDir, requireExplicitPolicy2CertName, plContext);
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetRequireExplicitPolicy
                (requireExplicitPolicy2Cert, &skipCerts, NULL));
        PR_ASSERT(skipCerts == 2);

        subTest("PKIX_PL_Cert_GetPolicyMappingInhibited");
        inhibitPolicyMapping5Cert = createCert
                (dataDir, inhibitPolicyMapping5CertName, plContext);
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetPolicyMappingInhibited
                (inhibitPolicyMapping5Cert, &skipCerts, NULL));
        PR_ASSERT(skipCerts == 5);

        subTest("PKIX_PL_Cert_GetInhibitAnyPolicy");
        inhibitAnyPolicy5Cert = createCert
                (dataDir, inhibitAnyPolicy5CertName, plContext);
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetInhibitAnyPolicy
                (inhibitAnyPolicy5Cert, &skipCerts, NULL));
        PR_ASSERT(skipCerts == 5);

        inhibitAnyPolicy0Cert = createCert
                (dataDir, inhibitAnyPolicy0CertName, plContext);
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetInhibitAnyPolicy
                (inhibitAnyPolicy0Cert, &skipCerts, NULL));
        PR_ASSERT(skipCerts == 0);

cleanup:

        PKIX_TEST_DECREF_AC(requireExplicitPolicy2Cert);
        PKIX_TEST_DECREF_AC(inhibitPolicyMapping5Cert);
        PKIX_TEST_DECREF_AC(inhibitAnyPolicy5Cert);
        PKIX_TEST_DECREF_AC(inhibitAnyPolicy0Cert);

        PKIX_TEST_RETURN();
}

static void
testCertPolicyMaps(char *dataDir)
{
        char *policyMappingsCertName =
                "P1Mapping1to234CACert.crt";
        char *expectedAscii =
                "2.16.840.1.101.3.2.1.48.1=>2.16.840.1.101.3.2.1.48.2";

        PKIX_PL_Cert *policyMappingsCert = NULL;
        PKIX_List *mappings = NULL;
        PKIX_PL_CertPolicyMap *goodMap = NULL;
        PKIX_PL_CertPolicyMap *equalMap = NULL;
        PKIX_PL_CertPolicyMap *diffMap = NULL;
        PKIX_PL_OID *goodOID = NULL;
        PKIX_PL_OID *equalOID = NULL;
        PKIX_PL_OID *diffOID = NULL;

        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_Cert_GetPolicyMappings");

        policyMappingsCert = createCert
                (dataDir, policyMappingsCertName, plContext);
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetPolicyMappings
                (policyMappingsCert, &mappings, NULL));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (mappings, 0, (PKIX_PL_Object **)&goodMap, NULL));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (mappings, 0, (PKIX_PL_Object **)&equalMap, NULL));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (mappings, 2, (PKIX_PL_Object **)&diffMap, NULL));

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (goodMap,
                equalMap,
                diffMap,
                expectedAscii,
                CertPolicyMap,
                PKIX_TRUE);

        subTest("PKIX_PL_CertPolicyMap_GetIssuerDomainPolicy");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CertPolicyMap_GetIssuerDomainPolicy
                (goodMap, &goodOID, NULL));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CertPolicyMap_GetIssuerDomainPolicy
                (diffMap, &equalOID, NULL));
        subTest("PKIX_PL_CertPolicyMap_GetSubjectDomainPolicy");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_CertPolicyMap_GetSubjectDomainPolicy
                (diffMap, &diffOID, NULL));

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (goodOID,
                equalOID,
                diffOID,
                "2.16.840.1.101.3.2.1.48.1",
                OID,
                PKIX_FALSE);

        subTest("pkix_pl_CertPolicyMap_Destroy");
        PKIX_TEST_DECREF_BC(goodMap);
        PKIX_TEST_DECREF_BC(equalMap);
        PKIX_TEST_DECREF_BC(diffMap);

cleanup:
        PKIX_TEST_DECREF_AC(policyMappingsCert);
        PKIX_TEST_DECREF_AC(mappings);
        PKIX_TEST_DECREF_AC(goodOID);
        PKIX_TEST_DECREF_AC(equalOID);
        PKIX_TEST_DECREF_AC(diffOID);

        PKIX_TEST_RETURN();
}


static void
testNameConstraints(char *dataDir)
{
        char *firstPname = "nameConstraintsDN3subCA2Cert.crt";
        char *secondPname = "nameConstraintsDN4CACert.crt";
        char *thirdPname = "nameConstraintsDN5CACert.crt";
        char *lastPname = "InvalidDNnameConstraintsTest3EE.crt";
        PKIX_PL_Cert *firstCert = NULL;
        PKIX_PL_Cert *secondCert = NULL;
        PKIX_PL_Cert *thirdCert = NULL;
        PKIX_PL_Cert *lastCert = NULL;
        PKIX_PL_CertNameConstraints *firstNC = NULL;
        PKIX_PL_CertNameConstraints *secondNC = NULL;
        PKIX_PL_CertNameConstraints *thirdNC = NULL;
        PKIX_PL_CertNameConstraints *firstMergedNC = NULL;
        PKIX_PL_CertNameConstraints *secondMergedNC = NULL;
        char *firstExpectedAscii =
                "[\n"
                "\t\tPermitted Name:  (O=Test Certificates,C=US)\n"
                "\t\tExcluded Name:   (OU=excludedSubtree1,O=Test Certificates,"
                "C=US, OU=excludedSubtree2,O=Test Certificates,C=US)\n"
                "\t]\n";
        char *secondExpectedAscii =
                "[\n"
                "\t\tPermitted Name:  (O=Test Certificates,C=US, "
                "OU=permittedSubtree1,O=Test Certificates,C=US)\n"
                "\t\tExcluded Name:   (OU=excludedSubtree1,"
                "O=Test Certificates,"
                "C=US, OU=excludedSubtree2,O=Test Certificates,C=US, "
                "OU=excludedSubtree1,OU=permittedSubtree1,"
                "O=Test Certificates,C=US)\n"
                "\t]\n";

        PKIX_TEST_STD_VARS();
        subTest("PKIX_PL_CertNameConstraints");

        firstCert = createCert(dataDir, firstPname, plContext);
        secondCert = createCert(dataDir, secondPname, plContext);
        thirdCert = createCert(dataDir, thirdPname, plContext);
        lastCert = createCert(dataDir, lastPname, plContext);

        subTest("PKIX_PL_Cert_GetNameConstraints <total=3>");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetNameConstraints
                (firstCert, &firstNC, NULL));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetNameConstraints
                (secondCert, &secondNC, NULL));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetNameConstraints
                (thirdCert, &thirdNC, NULL));

        subTest("PKIX_PL_Cert_MergeNameConstraints <1st and 2nd>");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_MergeNameConstraints
                (firstNC, secondNC, &firstMergedNC, NULL));

        subTest("PKIX_PL_Cert_MergeNameConstraints <1st+2nd and 3rd>");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_MergeNameConstraints
                (firstMergedNC, thirdNC, &secondMergedNC, NULL));

        testToStringHelper
                ((PKIX_PL_Object *)firstMergedNC,
                firstExpectedAscii,
                plContext);

        testToStringHelper
                ((PKIX_PL_Object *)secondMergedNC,
                secondExpectedAscii,
                plContext);

        subTest("PKIX_PL_Cert_CheckNameConstraints <permitted>");

        /* Subject: CN=nameConstraints DN3 subCA2,O=Test Certificates,C=US */
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_CheckNameConstraints
                (firstCert, firstMergedNC, NULL));

        subTest("PKIX_PL_Cert_CheckNameConstraints <OU in excluded>");

        /*
         * Subject: CN=Invalid DN nameConstraints EE Certificate Test3,
         * OU=permittedSubtree1,O=Test Certificates,C=US
         */
        PKIX_TEST_EXPECT_ERROR(PKIX_PL_Cert_CheckNameConstraints
                (lastCert, secondMergedNC, NULL));

        subTest("PKIX_PL_Cert_CheckNameConstraints <excluded>");

        /*
         * Subject: CN=Invalid DN nameConstraints EE Certificate Test3,
         * OU=permittedSubtree1,O=Test Certificates,C=US
         * SubjectAltNames: CN=Invalid DN nameConstraints EE Certificate
         * Test3,OU=excludedSubtree1,O=Test Certificates,C=US
         */
        PKIX_TEST_EXPECT_ERROR(PKIX_PL_Cert_CheckNameConstraints
                (lastCert, firstMergedNC, NULL));

        subTest("PKIX_PL_Cert_CheckNameConstraints <excluded>");

        PKIX_TEST_EXPECT_ERROR(PKIX_PL_Cert_CheckNameConstraints
                (firstCert, secondMergedNC, NULL));

cleanup:

        PKIX_TEST_DECREF_AC(firstCert);
        PKIX_TEST_DECREF_AC(secondCert);
        PKIX_TEST_DECREF_AC(thirdCert);
        PKIX_TEST_DECREF_AC(lastCert);
        PKIX_TEST_DECREF_AC(firstNC);
        PKIX_TEST_DECREF_AC(secondNC);
        PKIX_TEST_DECREF_AC(thirdNC);
        PKIX_TEST_DECREF_AC(firstMergedNC);
        PKIX_TEST_DECREF_AC(secondMergedNC);

        PKIX_TEST_RETURN();
}

static void
testDestroy(void *goodObject, void *equalObject, void *diffObject)
{
        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_Cert_Destroy");

        PKIX_TEST_DECREF_BC(goodObject);
        PKIX_TEST_DECREF_BC(equalObject);
        PKIX_TEST_DECREF_BC(diffObject);

        PKIX_TEST_DECREF_BC(altNameNoneCert);
        PKIX_TEST_DECREF_BC(altNameOtherCert);
        PKIX_TEST_DECREF_BC(altNameOtherCert_diff);
        PKIX_TEST_DECREF_BC(altNameRfc822Cert);
        PKIX_TEST_DECREF_BC(altNameRfc822Cert_diff);
        PKIX_TEST_DECREF_BC(altNameDnsCert);
        PKIX_TEST_DECREF_BC(altNameDnsCert_diff);
        PKIX_TEST_DECREF_BC(altNameX400Cert);
        PKIX_TEST_DECREF_BC(altNameX400Cert_diff);
        PKIX_TEST_DECREF_BC(altNameDnCert);
        PKIX_TEST_DECREF_BC(altNameDnCert_diff);
        PKIX_TEST_DECREF_BC(altNameEdiCert);
        PKIX_TEST_DECREF_BC(altNameEdiCert_diff);
        PKIX_TEST_DECREF_BC(altNameUriCert);
        PKIX_TEST_DECREF_BC(altNameUriCert_diff);
        PKIX_TEST_DECREF_BC(altNameIpCert);
        PKIX_TEST_DECREF_BC(altNameIpCert_diff);
        PKIX_TEST_DECREF_BC(altNameOidCert);
        PKIX_TEST_DECREF_BC(altNameOidCert_diff);
        PKIX_TEST_DECREF_BC(altNameMultipleCert);

cleanup:

        PKIX_TEST_RETURN();

}

static
void printUsage(void) {
        (void) printf("\nUSAGE:\ttest_cert <test-purpose> <data-central-dir> <data-dir>\n\n");
}

int test_cert(int argc, char *argv[]) {

        PKIX_PL_Cert *goodObject = NULL;
        PKIX_PL_Cert *equalObject = NULL;
        PKIX_PL_Cert *diffObject = NULL;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;

        char *dataCentralDir = NULL;
        char *dataDir = NULL;
        char *goodInput = "yassir2bcn";
        char *diffInput = "nss2alice";

        char *expectedAscii =
                "[\n"
                "\tVersion:         v3\n"
                "\tSerialNumber:    37bc66ec\n"
                "\tIssuer:          CN=yassir,OU=bcn,OU=east,O=sun,C=us\n"
                "\tSubject:         OU=bcn,OU=east,O=sun,C=us\n"
                "\tValidity: [From: Thu Aug 19, 1999\n"
        /*      "\tValidity: [From: Thu Aug 19 16:19:56 1999\n"  */
                "\t           To:   Fri Aug 18, 2000]\n"
        /*      "\t           To:   Fri Aug 18 16:19:56 2000]\n" */
                "\tSubjectAltNames: (null)\n"
                "\tAuthorityKeyId:  (null)\n"
                "\tSubjectKeyId:    (null)\n"
                "\tSubjPubKeyAlgId: ANSI X9.57 DSA Signature\n"
                "\tCritExtOIDs:     (2.5.29.15, 2.5.29.19)\n"
                "\tExtKeyUsages:    (null)\n"
                "\tBasicConstraint: CA(0)\n"
                "\tCertPolicyInfo:  (null)\n"
                "\tPolicyMappings:  (null)\n"
                "\tExplicitPolicy:  -1\n"
                "\tInhibitMapping:  -1\n"
                "\tInhibitAnyPolicy:-1\n"
                "\tNameConstraints: (null)\n"
                "\tAuthorityInfoAccess: (null)\n"
                "\tSubjectInfoAccess: (null)\n"
                "\tCacheFlag:       0\n"
                "]\n";

        PKIX_TEST_STD_VARS();

        startTests("Cert");

        PKIX_TEST_EXPECT_NO_ERROR(
            PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

        if (argc < 3+j) {
                printUsage();
                return (0);
        }

        dataCentralDir = argv[2+j];
        dataDir = argv[3+j];

        createCerts
                (dataCentralDir,
                goodInput,
                diffInput,
                &goodObject,
                &equalObject,
                &diffObject);

        testToStringHelper
                ((PKIX_PL_Object*)goodObject, expectedAscii, plContext);

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (goodObject,
                equalObject,
                diffObject,
                expectedAscii,
                Cert,
                PKIX_TRUE);

        testVerifyKeyUsage(dataCentralDir, dataDir, goodObject);


        testGetExtendedKeyUsage(dataCentralDir);
        testGetKeyIdentifiers(dataCentralDir, goodObject);

        testGetVersion(goodObject);
        testGetSerialNumber(goodObject, equalObject, diffObject);

        testGetSubject(goodObject, equalObject, diffObject);
        testGetIssuer(goodObject, equalObject, diffObject);

        testGetSubjectAltNames(dataCentralDir);
        testGetCriticalExtensionOIDs(dataCentralDir, goodObject);

        testGetSubjectPublicKey(goodObject, equalObject, diffObject);
        testGetSubjectPublicKeyAlgId(goodObject);
        testMakeInheritedDSAPublicKey(dataCentralDir);

        testCheckValidity(goodObject, diffObject);

        testBasicConstraints_GetCAFlag(dataCentralDir);
        testBasicConstraints_GetPathLenConstraint(dataCentralDir);
        testGetBasicConstraints(dataCentralDir);

        /* Basic Policy Processing */
        testGetPolicyInformation(dataDir);
        testCertPolicy_GetPolicyId(dataDir);
        testCertPolicy_GetPolQualifiers(dataDir);
        testPolicyQualifier_GetPolicyQualifierId(dataDir);
        testPolicyQualifier_GetQualifier(dataDir);
        testAreCertPoliciesCritical(dataCentralDir, dataDir);

        /* Advanced Policy Processing */
        testCertPolicyConstraints(dataDir);
        testCertPolicyMaps(dataDir);

        testNameConstraints(dataDir);

        testVerifySignature(dataCentralDir);

        testDestroy(goodObject, equalObject, diffObject);

cleanup:

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("Cert");

        return (0);
}
