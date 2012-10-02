/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_verifynode.c
 *
 * Test VerifyNode Type
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static void *plContext =  NULL;

static
void printUsage(void) {
        (void) printf("\nUSAGE:\ttest_verifynode path cert1 cert2 cert3\n\n");
}

int test_verifynode(int argc, char *argv[]) {

        /*
         * Create a tree with parent = cert1, child=cert2, grandchild=cert3
         */
        PKIX_PL_Cert *cert1 = NULL;
        PKIX_PL_Cert *cert2 = NULL;
	PKIX_PL_Cert *cert3 = NULL;
        PKIX_VerifyNode *parentNode = NULL;
        PKIX_VerifyNode *childNode = NULL;
        PKIX_VerifyNode *grandChildNode = NULL;
        PKIX_PL_String *parentString = NULL;

        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;
        char *dirName = NULL;
	char *twoNodeAscii = "CERT[Issuer:CN=Trust Anchor,O=Test Cert"
		"ificates,C=US, Subject:CN=Trust Anchor,O=Test Certif"
		"icates,C=US], depth=0, error=(null)\n. CERT[Issuer:C"
		"N=Trust Anchor,O=Test Certificates,C=US, Subject:CN="
		"Good CA,O=Test Certificates,C=US], depth=1, error=(null)";
	char *threeNodeAscii = "CERT[Issuer:CN=Trust Anchor,O=Test Ce"
		"rtificates,C=US, Subject:CN=Trust Anchor,O=Test Cert"
		"ificates,C=US], depth=0, error=(null)\n. CERT[Issuer"
		":CN=Trust Anchor,O=Test Certificates,C=US, Subject:C"
		"N=Good CA,O=Test Certificates,C=US], depth=1, error="
		"(null)\n. . CERT[Issuer:CN=Good CA,O=Test Certificat"
		"es,C=US, Subject:CN=Valid EE Certificate Test1,O=Tes"
		"t Certificates,C=US], depth=2, error=(null)";

        PKIX_TEST_STD_VARS();

        if (argc < 3) {
                printUsage();
                return (0);
        }

        startTests("VerifyNode");

        PKIX_TEST_EXPECT_NO_ERROR(
            PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

        dirName = argv[++j];

        subTest("Creating Certs");

        cert1 = createCert
                (dirName, argv[++j], plContext);

        cert2 = createCert
                (dirName, argv[++j], plContext);

        cert3 = createCert
                (dirName, argv[++j], plContext);

        subTest("Creating VerifyNode objects");

        PKIX_TEST_EXPECT_NO_ERROR(pkix_VerifyNode_Create
		(cert1, 0, NULL, &parentNode, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(pkix_VerifyNode_Create
		(cert2, 1, NULL, &childNode, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(pkix_VerifyNode_Create
		(cert3, 2, NULL, &grandChildNode, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(pkix_VerifyNode_AddToChain
		(parentNode, childNode, plContext));

        subTest("Creating VerifyNode ToString objects");

	testToStringHelper
		((PKIX_PL_Object *)parentNode, twoNodeAscii, plContext);

        PKIX_TEST_EXPECT_NO_ERROR(pkix_VerifyNode_AddToChain
		(parentNode, grandChildNode, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString
                ((PKIX_PL_Object*)parentNode, &parentString, plContext));
        (void) printf("parentNode is\n\t%s\n", parentString->escAsciiString);

	testToStringHelper
		((PKIX_PL_Object *)parentNode, threeNodeAscii, plContext);

cleanup:

        PKIX_TEST_DECREF_AC(cert1);
        PKIX_TEST_DECREF_AC(cert2);
        PKIX_TEST_DECREF_AC(parentNode);
        PKIX_TEST_DECREF_AC(childNode);
        PKIX_TEST_DECREF_AC(parentString);

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("VerifyNode");

        return (0);
}
