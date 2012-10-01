/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_date.c
 *
 * Test Date Type
 *
 */



#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;

static void
createDates(char *goodInput, char *diffInput,
                    PKIX_PL_Date **goodDate,
                    PKIX_PL_Date **equalDate,
                    PKIX_PL_Date **diffDate){

        subTest("PKIX_PL_Date_Create <goodDate>");
        *goodDate = createDate(goodInput, plContext);

        subTest("PKIX_PL_Date_Create <equalDate>");
        *equalDate = createDate(goodInput, plContext);

        subTest("PKIX_PL_Date_Create <diffDate>");
        *diffDate = createDate(diffInput, plContext);

}

static void
testDestroy(void *goodObject, void *equalObject, void *diffObject)
{
        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_Date_Destroy");

        PKIX_TEST_DECREF_BC(goodObject);
        PKIX_TEST_DECREF_BC(equalObject);
        PKIX_TEST_DECREF_BC(diffObject);

cleanup:

        PKIX_TEST_RETURN();

}

static
void testDate(char *goodInput, char *diffInput){

        PKIX_PL_Date *goodDate = NULL;
        PKIX_PL_Date *equalDate = NULL;
        PKIX_PL_Date *diffDate = NULL;

        /*
         * The ASCII rep of the date will vary by platform and locale
         * This particular string was generated on a SPARC running Solaris 9
         * in an English locale
         */
        /* char *expectedAscii = "Mon Mar 29 08:48:47 2004"; */
        char *expectedAscii = "Mon Mar 29, 2004";

        PKIX_TEST_STD_VARS();

        createDates(goodInput, diffInput,
                    &goodDate, &equalDate, &diffDate);

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (goodDate, equalDate, diffDate, expectedAscii, Date, PKIX_TRUE);

        testDestroy(goodDate, equalDate, diffDate);

        PKIX_TEST_RETURN();

}

int test_date(int argc, char *argv[]) {

        char *goodInput = NULL;
        char *diffInput = NULL;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;

        PKIX_TEST_STD_VARS();

        startTests("Date");

        PKIX_TEST_EXPECT_NO_ERROR(
            PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

        goodInput = "040329134847Z";
        diffInput = "050329135847Z";
        testDate(goodInput, diffInput);

cleanup:

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("Date");

        return (0);
}
