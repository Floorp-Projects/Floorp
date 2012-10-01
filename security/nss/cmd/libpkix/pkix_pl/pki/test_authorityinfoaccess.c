/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_authorityinfoaccess.c
 *
 * Test Authority InfoAccess Type
 *
 */



#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;

int test_authorityinfoaccess(int argc, char *argv[]) {

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
        char *expectedAscii = "[method:caIssuers, location:ldap:"
                "//betty.nist.gov/cn=CA,ou=Basic%20LDAP%20URI%20OU1,"
                "o=Test%20Certificates,c=US?cACertificate;binary,"
                "crossCertificatePair;binary]";

        PKIX_TEST_STD_VARS();

        startTests("AuthorityInfoAccess");

        PKIX_TEST_EXPECT_NO_ERROR(
            PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

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
