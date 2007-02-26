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
 * test_buildparams.c
 *
 * Test BuildParams Type
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

void *plContext = NULL;

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

void printUsage(char *pName){
        printf("\nUSAGE: %s <central-data-dir>\n\n", pName);
}

int main(int argc, char *argv[]) {

        PKIX_BuildParams *goodObject = NULL;
        PKIX_BuildParams *equalObject = NULL;
        PKIX_BuildParams *diffObject = NULL;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;
	PKIX_Boolean useArenas = PKIX_FALSE;

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

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                                    (PKIX_TRUE, /* nssInitNeeded */
                                    useArenas,
                                    PKIX_MAJOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    &actualMinorVersion,
                                    &plContext));

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
