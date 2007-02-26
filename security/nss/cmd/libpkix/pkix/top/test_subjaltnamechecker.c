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
 * test_subjaltnamechecker.c
 *
 * Test Subject Alternative Name Checking
 *
 */

/*
 * There is no subjaltnamechecker. Instead, targetcertchecker is doing
 * the job for checking subject alternative names' validity. For testing,
 * in order to enter names with various type, we create this test excutable
 * to parse different scenario.
 */

#include "testutil.h"
#include "testutil_nss.h"

#define PKIX_TEST_MAX_CERTS     10

void *plContext = NULL;

void printUsage1(char *pName){
        printf("\nUSAGE: %s test-name [ENE|EE] ", pName);
        printf("cert [certs].\n");
}

void printUsage2(char *name) {
        printf("\ninvalid test-name syntax - %s", name);
        printf("\ntest-name syntax: [01][DNORU]:<name>+...");
        printf("\n             [01] 1 - match all; 0 - match one");
        printf("\n    name - type can be specified as");
        printf("\n          [DNORU] D-Directory name");
        printf("\n                  N-DNS name");
        printf("\n                  O-OID name");
        printf("\n                  R-RFC822 name");
        printf("\n                  U-URI name");
        printf("\n                + separator for more names\n\n");
}

void printUsageMax(PKIX_UInt32 numCerts){
        printf("\nUSAGE ERROR: number of certs %d exceed maximum %d\n",
                numCerts, PKIX_TEST_MAX_CERTS);
}

PKIX_UInt32 getNameType(char *name){
        PKIX_UInt32 nameType;

        PKIX_TEST_STD_VARS();

        switch (*name) {
        case 'D':
                nameType = PKIX_DIRECTORY_NAME;
                break;
        case 'N':
                nameType = PKIX_DNS_NAME;
                break;
        case 'O':
                nameType = PKIX_OID_NAME;
                break;
        case 'R':
                nameType = PKIX_RFC822_NAME;
                break;
        case 'U':
                nameType = PKIX_URI_NAME;
                break;
        default:
                printUsage2(name);
                nameType = 0xFFFF;
        }

        goto cleanup;

cleanup:
        PKIX_TEST_RETURN();
        return (nameType);
}

int main(int argc, char *argv[]){

        PKIX_List *chain = NULL;
        PKIX_ValidateParams *valParams = NULL;
        PKIX_ValidateResult *valResult = NULL;
        PKIX_CertSelector *selector = NULL;
        PKIX_ComCertSelParams *selParams = NULL;
        PKIX_ProcessingParams *procParams = NULL;
        PKIX_PL_GeneralName *name = NULL;
        PKIX_UInt32 actualMinorVersion;
        char *certNames[PKIX_TEST_MAX_CERTS];
        PKIX_PL_Cert *certs[PKIX_TEST_MAX_CERTS];
        PKIX_UInt32 chainLength = 0;
        PKIX_UInt32 i = 0;
        PKIX_UInt32 j = 0;
        char *nameStr;
        char *nameEnd;
        char *names[PKIX_TEST_MAX_CERTS];
        PKIX_UInt32 numNames = 0;
        PKIX_UInt32 nameType;
        PKIX_Boolean matchAll = PKIX_TRUE;
        PKIX_Boolean testValid = PKIX_TRUE;
        PKIX_Boolean useArenas = PKIX_FALSE;
        char *dirName = NULL;
        char *anchorName = NULL;
	PKIX_VerifyNode *verifyTree = NULL;
	PKIX_PL_String *verifyString = NULL;

        PKIX_TEST_STD_VARS();

        if (argc < 5) {
                printUsage1(argv[0]);
                return (0);
        }

        startTests("SubjAltNameConstraintChecker");

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                                    (PKIX_TRUE, /* nssInitNeeded */
                                    useArenas,
                                    PKIX_MAJOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    &actualMinorVersion,
                                    &plContext));

	j++; /* skip test-purpose string */

        /* ENE = expect no error; EE = expect error */
        if (PORT_Strcmp(argv[2+j], "ENE") == 0) {
                testValid = PKIX_TRUE;
        } else if (PORT_Strcmp(argv[2+j], "EE") == 0) {
                testValid = PKIX_FALSE;
        } else {
                printUsage1(argv[0]);
                return (0);
        }

        /* taking out leading and trailing ", if any */
        nameStr = argv[1+j];
        subTest(nameStr);
        if (*nameStr == '"'){
                nameStr++;
                nameEnd = nameStr;
                while (*nameEnd != '"' && *nameEnd != '\0') {
                        nameEnd++;
                }
                *nameEnd = '\0';
        }

        /* extract first [0|1] inidcating matchAll or not */
        matchAll = (*nameStr == '0')?PKIX_FALSE:PKIX_TRUE;
        nameStr++;

        numNames = 0;
        while (*nameStr != '\0') {
                names[numNames++] = nameStr;
                while (*nameStr != '+' && *nameStr != '\0') {
                        nameStr++;
                }
                if (*nameStr == '+') {
                        *nameStr = '\0';
                        nameStr++;
                }
        }

        chainLength = (argc - j) - 4;
        if (chainLength > PKIX_TEST_MAX_CERTS) {
                printUsageMax(chainLength);
        }

        for (i = 0; i < chainLength; i++) {
                certNames[i] = argv[(4+j)+i];
                certs[i] = NULL;
        }

        /* SubjAltName for validation */

        subTest("Add Subject Alt Name for NameConstraint checking");

        subTest("Create Selector and ComCertSelParams");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertSelector_Create
                (NULL, NULL, &selector, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_Create
                (&selParams, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertSelector_SetCommonCertSelectorParams
                (selector, selParams, plContext));

        subTest("PKIX_ComCertSelParams_SetMatchAllSubjAltNames");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_SetMatchAllSubjAltNames
                (selParams, matchAll, plContext));

        subTest("PKIX_ComCertSelParams_AddSubjAltName(s)");
        for (i = 0; i < numNames; i++) {
                nameType = getNameType(names[i]);
                if (nameType == 0xFFFF) {
                        return (0);
                }
                nameStr = names[i] + 2;
                name = createGeneralName(nameType, nameStr, plContext);

                PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCertSelParams_AddSubjAltName
                        (selParams, name, plContext));
                PKIX_TEST_DECREF_BC(name);
        }

        subTest("SubjAltName-Constraints - Create Cert Chain");

        dirName = argv[3+j];

        chain = createCertChainPlus
                (dirName, certNames, certs, chainLength, plContext);

        subTest("SubjAltName-Constraints - Create Params");

        valParams = createValidateParams
                (dirName,
                argv[4+j],
                NULL,
                NULL,
                NULL,
                PKIX_FALSE,
                PKIX_FALSE,
                PKIX_FALSE,
                PKIX_FALSE,
                chain,
                plContext);

        subTest("PKIX_ValidateParams_getProcessingParams");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateParams_GetProcessingParams
                (valParams, &procParams, plContext));

        subTest("PKIX_ProcessingParams_SetTargetCertConstraints");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ProcessingParams_SetTargetCertConstraints
                (procParams, selector, plContext));

        subTest("Subject Alt Name - Validate Chain");

        if (testValid == PKIX_TRUE) {
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_ValidateChain
                        (valParams, &valResult, &verifyTree, plContext));
        } else {
                PKIX_TEST_EXPECT_ERROR(PKIX_ValidateChain
                        (valParams, &valResult, &verifyTree, plContext));
        }

cleanup:

        PKIX_PL_Free(anchorName, plContext);

        PKIX_TEST_DECREF_AC(verifyString);
        PKIX_TEST_DECREF_AC(verifyTree);
        PKIX_TEST_DECREF_AC(chain);
        PKIX_TEST_DECREF_AC(valParams);
        PKIX_TEST_DECREF_AC(valResult);
        PKIX_TEST_DECREF_AC(selector);
        PKIX_TEST_DECREF_AC(selParams);
        PKIX_TEST_DECREF_AC(procParams);
        PKIX_TEST_DECREF_AC(name);

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("SubjAltNameConstraintsChecker");

        return (0);
}
