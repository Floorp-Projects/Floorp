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
 * test_subjectinfoaccess.c
 *
 * Test Subject InfoAccess Type
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
        PKIX_PL_InfoAccess *sia = NULL;
        PKIX_PL_InfoAccess *siaDup = NULL;
        PKIX_PL_InfoAccess *siaDiff = NULL;
        PKIX_PL_GeneralName *location = NULL;
        char *certPathName = NULL;
        char *dirName = NULL;
        PKIX_UInt32 method = 0;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 size, i;
        PKIX_UInt32 j = 0;
        PKIX_Boolean useArenas = PKIX_FALSE;
        char *expectedAscii = "[method:caRepository, "
                "location:http://betty.nist.gov/pathdiscoverytestsuite/"
                "p7cfiles/IssuedByTrustAnchor1.p7c]";

        PKIX_TEST_STD_VARS();

        startTests("SubjectInfoAccess");

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

        subTest("Creating Cert with Subject Info Access");
        cert = createCert(dirName, certPathName, plContext);

        certPathName = argv[4+j];

        subTest("Creating Cert with Subject Info Access");
        certDiff = createCert(dirName, certPathName, plContext);

        subTest("Getting Subject Info Access");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetSubjectInfoAccess
                (cert, &siaList, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength
                (siaList, &size, plContext));

        if (size != 1) {
                pkixTestErrorMsg = "unexpected number of AIA";
                goto cleanup;
        }
 
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (siaList, 0, (PKIX_PL_Object **) &sia, plContext));

        subTest("PKIX_PL_InfoAccess_GetMethod");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_InfoAccess_GetMethod
                (sia, &method, plContext));
        if (method != PKIX_INFOACCESS_CA_REPOSITORY) {
                pkixTestErrorMsg = "unexpected method of AIA";
                goto cleanup;
        }

        subTest("PKIX_PL_InfoAccess_GetLocation");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_InfoAccess_GetLocation
                (sia, &location, plContext));
        if (!location) {
                pkixTestErrorMsg = "Cannot get AIA location";
                goto cleanup;
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (siaList, 0, (PKIX_PL_Object **) &siaDup, plContext));

        subTest("Getting Authority Info Access as difference comparison");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetAuthorityInfoAccess
                (certDiff, &aiaList, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength
                (aiaList, &size, plContext));

        if (size != 1) {
                pkixTestErrorMsg = "unexpected number of AIA";
                goto cleanup;
        }
 
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetItem
                (aiaList, 0, (PKIX_PL_Object **) &siaDiff, plContext));

        subTest("Checking: Equal, Hash and ToString");
        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (sia, siaDup, siaDiff, expectedAscii, InfoAccess, PKIX_FALSE);



cleanup:

        PKIX_TEST_DECREF_AC(location);
        PKIX_TEST_DECREF_AC(sia);
        PKIX_TEST_DECREF_AC(siaDup);
        PKIX_TEST_DECREF_AC(siaDiff);
        PKIX_TEST_DECREF_AC(aiaList);
        PKIX_TEST_DECREF_AC(siaList);
        PKIX_TEST_DECREF_AC(cert);
        PKIX_TEST_DECREF_AC(certDiff);
     
        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("Subjectinfoaccess");

        return (0);
}
