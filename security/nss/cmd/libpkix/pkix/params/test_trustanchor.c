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
 * test_trustanchor.c
 *
 * Test TrustAnchor Type
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

void *plContext = NULL;

void createTrustAnchors(
        char *dirName,
        char *goodInput,
        PKIX_TrustAnchor **goodObject,
        PKIX_TrustAnchor **equalObject,
        PKIX_TrustAnchor **diffObject)
{
        subTest("PKIX_TrustAnchor_CreateWithNameKeyPair <goodObject>");
        *goodObject = createTrustAnchor
                        (dirName, goodInput, PKIX_FALSE, plContext);

        subTest("PKIX_TrustAnchor_CreateWithNameKeyPair <equalObject>");
        *equalObject = createTrustAnchor
                        (dirName, goodInput, PKIX_FALSE, plContext);

        subTest("PKIX_TrustAnchor_CreateWithCert <diffObject>");
        *diffObject = createTrustAnchor
                        (dirName, goodInput, PKIX_TRUE, plContext);
}

void testGetCAName(
        PKIX_PL_Cert *diffCert,
        PKIX_TrustAnchor *equalObject){

        PKIX_PL_X500Name *diffCAName = NULL;
        PKIX_PL_X500Name *equalCAName = NULL;

        PKIX_TEST_STD_VARS();
        subTest("PKIX_TrustAnchor_GetCAName");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubject
                                    (diffCert, &diffCAName, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_TrustAnchor_GetCAName
                                    (equalObject, &equalCAName, plContext));

        testEqualsHelper((PKIX_PL_Object *)diffCAName,
                        (PKIX_PL_Object *)equalCAName,
                        PKIX_TRUE,
                        plContext);

cleanup:

        PKIX_TEST_DECREF_AC(diffCAName);
        PKIX_TEST_DECREF_AC(equalCAName);

        PKIX_TEST_RETURN();
}

void testGetCAPublicKey(
        PKIX_PL_Cert *diffCert,
        PKIX_TrustAnchor *equalObject){

        PKIX_PL_PublicKey *diffPubKey = NULL;
        PKIX_PL_PublicKey *equalPubKey = NULL;

        PKIX_TEST_STD_VARS();
        subTest("PKIX_TrustAnchor_GetCAPublicKey");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubjectPublicKey
                                    (diffCert, &diffPubKey, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_TrustAnchor_GetCAPublicKey
                                    (equalObject, &equalPubKey, plContext));

        testEqualsHelper((PKIX_PL_Object *)diffPubKey,
                        (PKIX_PL_Object *)equalPubKey,
                        PKIX_TRUE,
                        plContext);

cleanup:

        PKIX_TEST_DECREF_AC(diffPubKey);
        PKIX_TEST_DECREF_AC(equalPubKey);

        PKIX_TEST_RETURN();
}

void testGetNameConstraints(char *dirName)
{
        PKIX_TrustAnchor *goodObject = NULL;
        PKIX_TrustAnchor *equalObject = NULL;
        PKIX_TrustAnchor *diffObject = NULL;
        PKIX_PL_Cert *diffCert;
        PKIX_PL_CertNameConstraints *diffNC = NULL;
        PKIX_PL_CertNameConstraints *equalNC = NULL;
        char *goodInput = "nameConstraintsDN5CACert.crt";
        char *expectedAscii =
                "[\n"
                "\tTrusted CA Name:         CN=nameConstraints DN5 CA,"
                "O=Test Certificates,C=US\n"
                "\tTrusted CA PublicKey:    PKCS #1 RSA Encryption\n"
                "\tInitial Name Constraints:[\n"
                "\t\tPermitted Name:  (OU=permittedSubtree1,"
                "O=Test Certificates,C=US)\n"
                "\t\tExcluded Name:   (OU=excludedSubtree1,"
                "OU=permittedSubtree1,O=Test Certificates,C=US)\n"
                "\t]\n"
                "\n"
                "]\n";

        PKIX_TEST_STD_VARS();

        subTest("Create TrustAnchors and compare");

        createTrustAnchors
                (dirName, goodInput, &goodObject, &equalObject, &diffObject);

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (goodObject,
                equalObject,
                diffObject,
                expectedAscii,
                TrustAnchor,
                PKIX_TRUE);

        subTest("PKIX_TrustAnchor_GetTrustedCert");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_TrustAnchor_GetTrustedCert
                                    (diffObject, &diffCert, plContext));

        subTest("PKIX_PL_Cert_GetNameConstraints");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetNameConstraints
                                    (diffCert, &diffNC, plContext));

        subTest("PKIX_TrustAnchor_GetNameConstraints");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_TrustAnchor_GetNameConstraints
                                    (equalObject, &equalNC, plContext));

        testEqualsHelper((PKIX_PL_Object *)diffNC,
                        (PKIX_PL_Object *)equalNC,
                        PKIX_TRUE,
                        plContext);

cleanup:

        PKIX_TEST_DECREF_AC(diffNC);
        PKIX_TEST_DECREF_AC(equalNC);
        PKIX_TEST_DECREF_BC(diffCert);
        PKIX_TEST_DECREF_BC(goodObject);
        PKIX_TEST_DECREF_BC(equalObject);
        PKIX_TEST_DECREF_BC(diffObject);

        PKIX_TEST_RETURN();
}

static void
testDestroy(void *goodObject, void *equalObject, void *diffObject)
{
        PKIX_TEST_STD_VARS();

        subTest("PKIX_TrustAnchor_Destroy");

        PKIX_TEST_DECREF_BC(goodObject);
        PKIX_TEST_DECREF_BC(equalObject);
        PKIX_TEST_DECREF_BC(diffObject);

cleanup:

        PKIX_TEST_RETURN();

}

void printUsage(void) {
        (void) printf("\nUSAGE:\ttest_trustanchor <NIST_FILES_DIR> <central-data-dir>\n\n");
}

int main(int argc, char *argv[]) {

        PKIX_TrustAnchor *goodObject = NULL;
        PKIX_TrustAnchor *equalObject = NULL;
        PKIX_TrustAnchor *diffObject = NULL;
        PKIX_PL_Cert *diffCert = NULL;
        PKIX_UInt32 actualMinorVersion;
        PKIX_Boolean useArenas = PKIX_FALSE;
        PKIX_UInt32 j = 0;

        char *goodInput = "yassir2yassir";
        char *expectedAscii =
                "[\n"
                "\tTrusted CA Name:         "
                "CN=yassir,OU=bcn,OU=east,O=sun,C=us\n"
                "\tTrusted CA PublicKey:    ANSI X9.57 DSA Signature\n"
                "\tInitial Name Constraints:(null)\n"
                "]\n";
        char *dirName = NULL;
        char *dataCentralDir = NULL;

        PKIX_TEST_STD_VARS();

        startTests("TrustAnchor");

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                                    (PKIX_TRUE, /* nssInitNeeded */
                                    useArenas,
                                    PKIX_MAJOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    &actualMinorVersion,
                                    &plContext));

        if (argc < 3) {
                printUsage();
                return (0);
        }

        dirName = argv[j+1];
        dataCentralDir = argv[j+2];

        createTrustAnchors
                (dataCentralDir,
                goodInput,
                &goodObject,
                &equalObject,
                &diffObject);

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (goodObject,
                equalObject,
                diffObject,
                expectedAscii,
                TrustAnchor,
                PKIX_TRUE);

        subTest("PKIX_TrustAnchor_GetTrustedCert");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_TrustAnchor_GetTrustedCert
                                    (diffObject, &diffCert, plContext));

        testGetCAName(diffCert, equalObject);
        testGetCAPublicKey(diffCert, equalObject);

        testGetNameConstraints(dirName);

        testDestroy(goodObject, equalObject, diffObject);

cleanup:

        PKIX_TEST_DECREF_AC(diffCert);

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("TrustAnchor");

        return (0);
}
