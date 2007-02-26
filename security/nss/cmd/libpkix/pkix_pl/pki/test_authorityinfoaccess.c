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
 * test_authorityinfoaccess.c
 *
 * Test Authority InfoAccess Type
 *
 */



#include "testutil.h"
#include "testutil_nss.h"

void *plContext = NULL;

int main(int argc, char *argv[]) {

        PKIX_PL_Cert *cert = NULL;
        PKIX_PL_Cert *certDiff = NULL;
        PKIX_List *aiaList = NULL;
        PKIX_List *siaList = NULL;
        PKIX_PL_InfoAccess *aia = NULL;
        PKIX_PL_InfoAccess *aiaDup = NULL;
        PKIX_PL_InfoAccess *aiaDiff = NULL;
        char *certPathName = NULL;
        char *dirName = NULL;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 size, i;
        PKIX_UInt32 j = 0;
        PKIX_Boolean useArenas = PKIX_FALSE;
        char *expectedAscii = "[method:caIssuers, location:ldap:"
                "//betty.nist.gov/cn=CA,ou=Basic%20LDAP%20URI%20OU1,"
                "o=Test%20Certificates,c=US?cACertificate;binary,"
                "crossCertificatePair;binary]";

        PKIX_TEST_STD_VARS();

        startTests("AuthorityInfoAccess");

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                                    (PKIX_TRUE, /* nssInitNeeded */
                                    useArenas,
                                    PKIX_MAJOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    &actualMinorVersion,
                                    &plContext));

        if (argc < 5+j) {
                printf("Usage: %s <test-purpose> <cert> <diff-cert>\n", argv[0]);
        }

        dirName = argv[2+j];
        certPathName = argv[3+j];

        subTest("Creating Cert with Authority Info Access");
        cert = createCert(dirName, certPathName, plContext);

        certPathName = argv[4+j];

        subTest("Creating Cert with Subject Info Access");
        certDiff = createCert(dirName, certPathName, plContext);

        subTest("Getting Authority Info Access");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetAuthorityInfoAccess
                (cert, &aiaList, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength
                (aiaList, &size, plContext));

        if (size != 1) {
                pkixTestErrorMsg = "unexpected number of AIA";
                goto cleanup;
        }
 
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (aiaList, 0, (PKIX_PL_Object **) &aia, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (aiaList, 0, (PKIX_PL_Object **) &aiaDup, plContext));

        subTest("Getting Subject Info Access as difference comparison");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubjectInfoAccess
                (certDiff, &siaList, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength
                (siaList, &size, plContext));

        if (size != 1) {
                pkixTestErrorMsg = "unexpected number of AIA";
                goto cleanup;
        }
 
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (siaList, 0, (PKIX_PL_Object **) &aiaDiff, plContext));

        subTest("Checking: Equal, Hash and ToString");
        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (aia, aiaDup, aiaDiff, expectedAscii, InfoAccess, PKIX_FALSE);



cleanup:

        PKIX_TEST_DECREF_AC(aia);
        PKIX_TEST_DECREF_AC(aiaDup);
        PKIX_TEST_DECREF_AC(aiaDiff);
        PKIX_TEST_DECREF_AC(aiaList);
        PKIX_TEST_DECREF_AC(siaList);
        PKIX_TEST_DECREF_AC(cert);
        PKIX_TEST_DECREF_AC(certDiff);
     
        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("Authorityinfoaccess");

        return (0);
}
