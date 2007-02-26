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
 * test_certchainchecker.c
 *
 * Test Cert Chain Checker
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

void *plContext = NULL;

PKIX_Error *dummyChecker_Check(
        PKIX_CertChainChecker *checker,
        PKIX_PL_Cert *cert,
        PKIX_List *unresolvedCriticalExtensions,
        void **pNBIOContext,
        void *plContext)
{
        goto cleanup;

cleanup:

        return(NULL);
}


void test_CertChainChecker_Duplicate(PKIX_CertChainChecker *original)
{
        PKIX_Boolean originalForward = PKIX_FALSE;
        PKIX_Boolean copyForward = PKIX_FALSE;
        PKIX_Boolean originalForwardDir = PKIX_FALSE;
        PKIX_Boolean copyForwardDir = PKIX_FALSE;
        PKIX_CertChainChecker *copy = NULL;
        PKIX_CertChainChecker_CheckCallback originalCallback = NULL;
        PKIX_CertChainChecker_CheckCallback copyCallback = NULL;
        PKIX_PL_Object *originalState = NULL;
        PKIX_PL_Object *copyState = NULL;
        PKIX_List *originalList = NULL;
        PKIX_List *copyList = NULL;

        PKIX_TEST_STD_VARS();

        subTest("CertChainChecker_Duplicate");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Duplicate
                ((PKIX_PL_Object *)original,
                (PKIX_PL_Object **)&copy,
                plContext));

        subTest("CertChainChecker_GetCheckCallback");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertChainChecker_GetCheckCallback
                (original, &originalCallback, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertChainChecker_GetCheckCallback
                (copy, &copyCallback, plContext));
        if (originalCallback != copyCallback) {
                pkixTestErrorMsg = "CheckCallback functions are not equal!";
                goto cleanup;
        }

        subTest("CertChainChecker_IsForwardCheckingSupported");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertChainChecker_IsForwardCheckingSupported
                (original, &originalForward, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertChainChecker_IsForwardCheckingSupported
                (copy, &copyForward, plContext));
        if (originalForward != copyForward) {
                pkixTestErrorMsg = "ForwardChecking booleans are not equal!";
                goto cleanup;
        }

        subTest("CertChainChecker_IsForwardDirectionExpected");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertChainChecker_IsForwardDirectionExpected
                (original, &originalForwardDir, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertChainChecker_IsForwardDirectionExpected
                (copy, &copyForwardDir, plContext));
        if (originalForwardDir != copyForwardDir) {
                pkixTestErrorMsg = "ForwardDirection booleans are not equal!";
                goto cleanup;
        }

        subTest("CertChainChecker_GetCertChainCheckerState");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertChainChecker_GetCertChainCheckerState
                (original, &originalState, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertChainChecker_GetCertChainCheckerState
                (copy, &copyState, plContext));
        testEqualsHelper(originalState, copyState, PKIX_TRUE, plContext);

        subTest("CertChainChecker_GetSupportedExtensions");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertChainChecker_GetSupportedExtensions
                (original, &originalList, plContext));
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertChainChecker_GetSupportedExtensions
                (copy, &copyList, plContext));
        testEqualsHelper
                ((PKIX_PL_Object *)originalList,
                (PKIX_PL_Object *)copyList,
                PKIX_TRUE,
                plContext);

cleanup:

        PKIX_TEST_DECREF_AC(copy);
        PKIX_TEST_DECREF_AC(originalState);
        PKIX_TEST_DECREF_AC(copyState);
        PKIX_TEST_DECREF_AC(originalList);
        PKIX_TEST_DECREF_AC(copyList);

        PKIX_TEST_RETURN();
}

int main(int argc, char *argv[]) {

        PKIX_UInt32 actualMinorVersion;
        PKIX_PL_OID *bcOID = NULL;
        PKIX_PL_OID *ncOID = NULL;
        PKIX_PL_OID *cpOID = NULL;
        PKIX_PL_OID *pmOID = NULL;
        PKIX_PL_OID *pcOID = NULL;
        PKIX_PL_OID *iaOID = NULL;
        PKIX_CertChainChecker *dummyChecker = NULL;
        PKIX_List *supportedExtensions = NULL;
        PKIX_PL_Object *initialState = NULL;
        PKIX_UInt32 j = 0;
        PKIX_Boolean useArenas = PKIX_FALSE;

        PKIX_TEST_STD_VARS();

        startTests("CertChainChecker");

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                                    (PKIX_TRUE, /* nssInitNeeded */
                                    useArenas,
                                    PKIX_MAJOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    &actualMinorVersion,
                                    &plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create
                (&supportedExtensions, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_OID_Create
                (PKIX_BASICCONSTRAINTS_OID, &bcOID, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (supportedExtensions, (PKIX_PL_Object *)bcOID, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_OID_Create
                (PKIX_NAMECONSTRAINTS_OID, &ncOID, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (supportedExtensions, (PKIX_PL_Object *)ncOID, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_OID_Create
                (PKIX_CERTIFICATEPOLICIES_OID, &cpOID, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (supportedExtensions, (PKIX_PL_Object *)cpOID, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_OID_Create
                (PKIX_POLICYMAPPINGS_OID, &pmOID, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (supportedExtensions, (PKIX_PL_Object *)pmOID, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_OID_Create
                (PKIX_POLICYCONSTRAINTS_OID, &pcOID, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (supportedExtensions, (PKIX_PL_Object *)pcOID, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_OID_Create
                (PKIX_INHIBITANYPOLICY_OID, &iaOID, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (supportedExtensions, (PKIX_PL_Object *)iaOID, plContext));

        PKIX_TEST_DECREF_BC(bcOID);
        PKIX_TEST_DECREF_BC(ncOID);
        PKIX_TEST_DECREF_BC(cpOID);
        PKIX_TEST_DECREF_BC(pmOID);
        PKIX_TEST_DECREF_BC(pcOID);
        PKIX_TEST_DECREF_BC(iaOID);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_IncRef
                ((PKIX_PL_Object *)supportedExtensions, plContext));

        initialState = (PKIX_PL_Object *)supportedExtensions;

        subTest("CertChainChecker_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CertChainChecker_Create
                (dummyChecker_Check, /* PKIX_CertChainChecker_CheckCallback */
                PKIX_FALSE,          /* forwardCheckingSupported */
                PKIX_FALSE,          /* forwardDirectionExpected */
                supportedExtensions,
                NULL,                /* PKIX_PL_Object *initialState */
                &dummyChecker,
                plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_CertChainChecker_SetCertChainCheckerState
                (dummyChecker, initialState, plContext));

        test_CertChainChecker_Duplicate(dummyChecker);

        subTest("CertChainChecker_Destroy");
        PKIX_TEST_DECREF_BC(dummyChecker);

cleanup:

        PKIX_TEST_DECREF_AC(dummyChecker);
        PKIX_TEST_DECREF_AC(initialState);
        PKIX_TEST_DECREF_AC(supportedExtensions);

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("CertChainChecker");

        return (0);
}
