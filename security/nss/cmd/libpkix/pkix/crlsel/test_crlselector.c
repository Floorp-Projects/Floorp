/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_crlselector.c
 *
 * Test CRLSelector Type
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;

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

static
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

static
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

int test_crlselector(int argc, char *argv[]){

        PKIX_PL_Date *context = NULL;
        PKIX_CRLSelector *goodObject = NULL;
        PKIX_CRLSelector *diffObject = NULL;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;
        char *asciiDate = "040329134847Z";

        PKIX_TEST_STD_VARS();

        startTests("CRLSelector");

        PKIX_TEST_EXPECT_NO_ERROR(
            PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

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
