/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_buildparams.c
 *
 * Test BuildParams Type
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;

static void
testDestroy(void *goodObject, void *equalObject, void *diffObject)
{
        PKIX_TEST_STD_VARS();

        subTest("PKIX_BuildParams_Destroy");

        PKIX_TEST_DECREF_BC(goodObject);
        PKIX_TEST_DECREF_BC(equalObject);
        PKIX_TEST_DECREF_BC(diffObject);

cleanup:

        PKIX_TEST_RETURN();

}

static
void testGetProcParams(
        PKIX_BuildParams *goodObject,
        PKIX_BuildParams *equalObject){

        PKIX_ProcessingParams *goodProcParams = NULL;
        PKIX_ProcessingParams *equalProcParams = NULL;

        PKIX_TEST_STD_VARS();
        subTest("PKIX_BuildParams_GetProcessingParams");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_BuildParams_GetProcessingParams
                (goodObject, &goodProcParams, NULL));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_BuildParams_GetProcessingParams
                (equalObject, &equalProcParams, NULL));

        testEqualsHelper
                ((PKIX_PL_Object *)goodProcParams,
                (PKIX_PL_Object *)equalProcParams,
                PKIX_TRUE,
                plContext);

cleanup:

        PKIX_TEST_DECREF_AC(goodProcParams);
        PKIX_TEST_DECREF_AC(equalProcParams);

        PKIX_TEST_RETURN();
}

static
void printUsage(char *pName){
        printf("\nUSAGE: %s <central-data-dir>\n\n", pName);
}

int test_buildparams(int argc, char *argv[]) {

        PKIX_BuildParams *goodObject = NULL;
        PKIX_BuildParams *equalObject = NULL;
        PKIX_BuildParams *diffObject = NULL;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;

        char *dataCentralDir = NULL;
        char *goodInput = "yassir2yassir";
        char *diffInput = "yassir2bcn";

        char *expectedAscii =
                "[\n"
                "\tProcessing Params: \n"
                "\t********BEGIN PROCESSING PARAMS********\n"
                "\t\t"
                "[\n"
                "\tTrust Anchors: \n"
                "\t********BEGIN LIST OF TRUST ANCHORS********\n"
                "\t\t"
"([\n"
                "\tTrusted CA Name:         "
                "CN=yassir,OU=bcn,OU=east,O=sun,C=us\n"
                "\tTrusted CA PublicKey:    ANSI X9.57 DSA Signature\n"
                "\tInitial Name Constraints:(null)\n"
                "]\n"
                ", [\n"
                "\tTrusted CA Name:         OU=bcn,OU=east,O=sun,C=us\n"
                "\tTrusted CA PublicKey:    ANSI X9.57 DSA Signature\n"
                "\tInitial Name Constraints:(null)\n"
                "]\n"
                ")\n"
                "\t********END LIST OF TRUST ANCHORS********\n"
                "\tDate:    \t\t(null)\n"
                "\tTarget Constraints:    (null)\n"
                "\tInitial Policies:      (null)\n"
                "\tQualifiers Rejected:   FALSE\n"
                "\tCert Stores:           (EMPTY)\n"
                "\tResource Limits:       (null)\n"
                "\tCRL Checking Enabled:  0\n"
                "]\n"
                "\n"
                "\t********END PROCESSING PARAMS********\n"
                "]\n";

        PKIX_TEST_STD_VARS();

        startTests("BuildParams");

        PKIX_TEST_EXPECT_NO_ERROR(
            PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

        if (argc < 2){
                printUsage(argv[0]);
                return (0);
        }

        dataCentralDir = argv[j+1];

        subTest("PKIX_BuildParams_Create");

        goodObject = createBuildParams
                (dataCentralDir,
                goodInput,
                diffInput,
                NULL,
                NULL,
                PKIX_FALSE,
                plContext);

        equalObject = createBuildParams
                (dataCentralDir,
                goodInput,
                diffInput,
                NULL,
                NULL,
                PKIX_FALSE,
                plContext);

        diffObject = createBuildParams
                (dataCentralDir,
                diffInput,
                goodInput,
                NULL,
                NULL,
                PKIX_FALSE,
                plContext);

        testGetProcParams(goodObject, equalObject);

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (goodObject,
                equalObject,
                diffObject,
                expectedAscii,
                BuildParams,
                PKIX_FALSE);

        testDestroy(goodObject, equalObject, diffObject);

cleanup:

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("BuildParams");

        return (0);
}
