/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_subjectinfoaccess.c
 *
 * Test Subject InfoAccess Type
 *
 */



#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;

int test_subjectinfoaccess(int argc, char *argv[]) {

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
        char *expectedAscii = "[method:caRepository, "
                "location:http://betty.nist.gov/pathdiscoverytestsuite/"
                "p7cfiles/IssuedByTrustAnchor1.p7c]";

        PKIX_TEST_STD_VARS();

        startTests("SubjectInfoAccess");

        PKIX_TEST_EXPECT_NO_ERROR(
            PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

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
