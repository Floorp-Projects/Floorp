/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_generalname.c
 *
 * Test GeneralName Type
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;

static void
createGeneralNames(PKIX_UInt32 nameType, char *goodInput, char *diffInput,
                    PKIX_PL_GeneralName **goodName,
                    PKIX_PL_GeneralName **equalName,
                    PKIX_PL_GeneralName **diffName){

        subTest("PKIX_PL_GeneralName_Create <goodName>");
        *goodName = createGeneralName(nameType, goodInput, plContext);

        subTest("PKIX_PL_GeneralName_Create <equalName>");
        *equalName = createGeneralName(nameType, goodInput, plContext);

        subTest("PKIX_PL_GeneralName_Create <diffName>");
        *diffName = createGeneralName(nameType, diffInput, plContext);

}

static void
testDestroy(void *goodObject, void *equalObject, void *diffObject)
{
        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_GeneralName_Destroy");

        PKIX_TEST_DECREF_BC(goodObject);
        PKIX_TEST_DECREF_BC(equalObject);
        PKIX_TEST_DECREF_BC(diffObject);

cleanup:

        PKIX_TEST_RETURN();

}

static void testNameType
(PKIX_UInt32 nameType, char *goodInput, char *diffInput, char *expectedAscii){

        PKIX_PL_GeneralName *goodName = NULL;
        PKIX_PL_GeneralName *equalName = NULL;
        PKIX_PL_GeneralName *diffName = NULL;

        createGeneralNames(nameType, goodInput, diffInput,
                            &goodName, &equalName, &diffName);

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (goodName,
                equalName,
                diffName,
                expectedAscii,
                GeneralName,
                PKIX_TRUE);

        testDestroy(goodName, equalName, diffName);
}

int test_generalname(int argc, char *argv[]) {

        char *goodInput = NULL;
        char *diffInput = NULL;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;

        PKIX_TEST_STD_VARS();

        startTests("GeneralName");

        PKIX_TEST_EXPECT_NO_ERROR(
            PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

        goodInput = "john@sun.com";
        diffInput = "john@labs.com";
        testNameType(PKIX_RFC822_NAME, goodInput, diffInput, goodInput);

        goodInput = "example1.com";
        diffInput = "ex2.net";
        testNameType(PKIX_DNS_NAME, goodInput, diffInput, goodInput);

        goodInput = "cn=yassir, ou=labs, o=sun, c=us";
        diffInput = "cn=alice, ou=labs, o=sun, c=us";
        testNameType(PKIX_DIRECTORY_NAME,
                    goodInput,
                    diffInput,
                    "CN=yassir,OU=labs,O=sun,C=us");

        goodInput = "http://example1.com";
        diffInput = "http://ex2.net";
        testNameType(PKIX_URI_NAME, goodInput, diffInput, goodInput);

        goodInput = "1.2.840.11";
        diffInput = "1.2.840.115349";
        testNameType(PKIX_OID_NAME, goodInput, diffInput, goodInput);

        /*
         * We don't support creating PKIX_EDIPARTY_NAME,
         * PKIX_IP_NAME, OTHER_NAME, X400_ADDRESS from strings
         */

cleanup:

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("GeneralName");

        return (0);
}
