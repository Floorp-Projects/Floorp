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
 * test_verifynode.c
 *
 * Test VerifyNode Type
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

void *plContext =  NULL;

void printUsage(void) {
        (void) printf("\nUSAGE:\ttest_verifynode path cert1 cert2 cert3\n\n");
}

int main(int argc, char *argv[]) {

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
        PKIX_Boolean useArenas = PKIX_FALSE;
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

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                (PKIX_TRUE, /* nssInitNeeded */
                useArenas,
                PKIX_MAJOR_VERSION,
                PKIX_MINOR_VERSION,
                PKIX_MINOR_VERSION,
                &actualMinorVersion,
                &plContext));

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
