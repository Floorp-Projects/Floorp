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
 * test_crlselector.c
 *
 * Test CRLSelector Type
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

void *plContext = NULL;

static void
testGetMatchCallback(PKIX_CRLSelector *goodObject)
{
        PKIX_CRLSelector_MatchCallback mCallback = NULL;

        PKIX_TEST_STD_VARS();

        subTest("testGetMatchCallback");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CRLSelector_GetMatchCallback
                                    (goodObject, &mCallback, plContext));

        if (mCallback == NULL) {
                pkixTestErrorMsg = "MatchCallback is NULL";
        }

cleanup:

        PKIX_TEST_RETURN();

}

void testGetCRLSelectorContext(PKIX_CRLSelector *goodObject)
{
        PKIX_PL_Object *context = NULL;

        PKIX_TEST_STD_VARS();

        subTest("testGetCRLSelectorContext");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CRLSelector_GetCRLSelectorContext
                                    (goodObject, (void *)&context, plContext));

        if (context == NULL) {
                pkixTestErrorMsg = "CRLSelectorContext is NULL";
        }

cleanup:

        PKIX_TEST_DECREF_AC(context);
        PKIX_TEST_RETURN();
}

void testCommonCRLSelectorParams(PKIX_CRLSelector *goodObject){
        PKIX_ComCRLSelParams *setParams = NULL;
        PKIX_ComCRLSelParams *getParams = NULL;
        PKIX_PL_Date *setDate = NULL;
        char *asciiDate = "040329134847Z";

        PKIX_TEST_STD_VARS();

        subTest("PKIX_ComCRLSelParams_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCRLSelParams_Create
                                    (&setParams,
                                    plContext));

        subTest("PKIX_ComCRLSelParams_Date Create");

        setDate = createDate(asciiDate, plContext);

        subTest("PKIX_ComCRLSelParams_SetDateAndTime");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ComCRLSelParams_SetDateAndTime
                (setParams, setDate, plContext));

        subTest("PKIX_CRLSelector_SetCommonCRLSelectorParams");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CRLSelector_SetCommonCRLSelectorParams(
                goodObject, setParams, plContext));

        subTest("PKIX_CRLSelector_GetCommonCRLSelectorParams");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CRLSelector_GetCommonCRLSelectorParams(
                goodObject, &getParams, plContext));

        testEqualsHelper((PKIX_PL_Object *)setParams,
                        (PKIX_PL_Object *)getParams,
                        PKIX_TRUE,
                        plContext);

        testHashcodeHelper((PKIX_PL_Object *)setParams,
                        (PKIX_PL_Object *)getParams,
                        PKIX_TRUE,
                        plContext);

cleanup:

        PKIX_TEST_DECREF_AC(setDate);
        PKIX_TEST_DECREF_AC(setParams);
        PKIX_TEST_DECREF_AC(getParams);

        PKIX_TEST_RETURN();
}

/* Functional tests for CRLSelector public functions */

int main(int argc, char *argv[]){

        PKIX_PL_Date *context = NULL;
        PKIX_CRLSelector *goodObject = NULL;
        PKIX_CRLSelector *diffObject = NULL;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;
        PKIX_Boolean useArenas = PKIX_FALSE;
        char *asciiDate = "040329134847Z";

        PKIX_TEST_STD_VARS();

        startTests("CRLSelector");

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                                    (PKIX_TRUE, /* nssInitNeeded */
                                    useArenas,
                                    PKIX_MAJOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    &actualMinorVersion,
                                    &plContext));

        context = createDate(asciiDate, plContext);

        subTest("PKIX_CRLSelector_Create");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CRLSelector_Create
                                    (NULL,
                                    (PKIX_PL_Object *)context,
                                    &goodObject,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_CRLSelector_Create
                                    (NULL,
                                    (PKIX_PL_Object *)context,
                                    &diffObject,
                                    plContext));

        testGetMatchCallback(goodObject);

        testGetCRLSelectorContext(goodObject);

        testCommonCRLSelectorParams(goodObject);

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (goodObject,
                goodObject,
                diffObject,
                NULL,
                CRLSelector,
                PKIX_TRUE);

cleanup:

        PKIX_TEST_DECREF_AC(goodObject);
        PKIX_TEST_DECREF_AC(diffObject);
        PKIX_TEST_DECREF_AC(context);

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("CRLSelector");

        return (0);
}
