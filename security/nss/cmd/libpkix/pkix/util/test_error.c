/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_error.c
 *
 * Tests Error Object Creation, ToString, Callbacks and Destroy
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;

static
void createErrors(
        PKIX_Error **error,
        PKIX_Error **error2,
        PKIX_Error **error3,
        PKIX_Error **error5,
        PKIX_Error **error6,
        PKIX_Error **error7,
        char *infoChar)

{
        PKIX_PL_String *infoString = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create(
                                        PKIX_ESCASCII,
                                        infoChar,
                                        PL_strlen(infoChar),
                                        &infoString,
                                        plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Error_Create
                                  (PKIX_MEM_ERROR,
                                   NULL,
                                   NULL,
                                   PKIX_TESTANOTHERERRORMESSAGE,
                                   error2,
                                   plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Error_Create
                                    (PKIX_OBJECT_ERROR,
                                    *error2,
                                    (PKIX_PL_Object*)infoString,
                                     PKIX_TESTERRORMESSAGE,
                                    error,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Error_Create
                                    (PKIX_OBJECT_ERROR,
                                    *error2,
                                    (PKIX_PL_Object*)infoString,
                                     PKIX_TESTERRORMESSAGE,
                                    error3,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Error_Create
                                    (PKIX_OBJECT_ERROR,
                                    NULL,
                                    (PKIX_PL_Object*)infoString,
                                    0,
                                    error5,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Error_Create
                                    (PKIX_MEM_ERROR,
                                    *error5,
                                    (PKIX_PL_Object*)infoString,
                                    0,
                                    error6,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Error_Create
                                    (PKIX_OBJECT_ERROR,
                                    *error6,
                                    (PKIX_PL_Object*)infoString,
                                    0,
                                    error7,
                                    plContext));

cleanup:

        PKIX_TEST_DECREF_AC(infoString);

        PKIX_TEST_RETURN();
}

static
void testGetErrorClass(PKIX_Error *error, PKIX_Error *error2)
{
        PKIX_ERRORCLASS errClass;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_Error_GetErrorClass(error, &errClass, plContext));

        if (errClass != PKIX_OBJECT_ERROR) {
                testError("Incorrect Class Returned");
        }

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_Error_GetErrorClass(error2, &errClass, plContext));

        if (errClass != PKIX_MEM_ERROR) {
                testError("Incorrect Class Returned");
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Error_GetErrorClass(PKIX_ALLOC_ERROR(),
                                            &errClass, plContext));
        if (errClass != PKIX_FATAL_ERROR) {
                testError("Incorrect Class Returned");
        }

cleanup:
        PKIX_TEST_RETURN();
}

static
void testGetDescription(
        PKIX_Error *error,
        PKIX_Error *error2,
        PKIX_Error *error3,
        char *descChar,
        char *descChar2)
{

        PKIX_PL_String *targetString = NULL;
        char *temp = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Error_GetDescription
                                (error, &targetString, plContext));
        temp = PKIX_String2ASCII(targetString, plContext);
        PKIX_TEST_DECREF_BC(targetString);

        if (temp){
                if (PL_strcmp(temp, descChar) != 0) {
                        testError("Incorrect description returned");
                }
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }


        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Error_GetDescription
                    (error2, &targetString, plContext));
        temp = PKIX_String2ASCII(targetString, plContext);
        PKIX_TEST_DECREF_BC(targetString);

        if (temp){
                if (PL_strcmp(temp, descChar2) != 0) {
                        testError("Incorrect description returned");
                }
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }


        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Error_GetDescription
                    (error3, &targetString, plContext));
        temp = PKIX_String2ASCII(targetString, plContext);
        PKIX_TEST_DECREF_BC(targetString);

        if (temp){
                if (PL_strcmp(temp, descChar) != 0) {
                        testError("Incorrect description returned");
                }
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }

cleanup:

        PKIX_TEST_RETURN();
}

static
void testGetCause(PKIX_Error *error, PKIX_Error *error2, PKIX_Error *error3)
{

        PKIX_Error *error4 = NULL;
        PKIX_PL_String *targetString = NULL;
        char *temp = NULL;
        PKIX_Boolean boolResult;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_Error_GetCause(error, &error4, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Equals
                    ((PKIX_PL_Object*)error2,
                    (PKIX_PL_Object*)error4,
                    &boolResult, plContext));
        if (!boolResult)
                testError("Incorrect Cause returned");

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Object_ToString((PKIX_PL_Object*)error4,
                                        &targetString, plContext));

        temp = PKIX_String2ASCII(targetString, plContext);
        if (temp){
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }

        PKIX_TEST_DECREF_BC(targetString);
        PKIX_TEST_DECREF_BC(error4);

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_Error_GetCause(error3, &error4, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Equals
                    ((PKIX_PL_Object*)error2,
                    (PKIX_PL_Object*)error4,
                    &boolResult, plContext));
        if (!boolResult)
                testError("Incorrect Cause returned");

        PKIX_TEST_DECREF_BC(error4);

cleanup:

        PKIX_TEST_RETURN();


}

static
void testGetSupplementaryInfo(PKIX_Error *error, char *infoChar)
{

        PKIX_PL_Object *targetString = NULL;
        char *temp = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Error_GetSupplementaryInfo
                    (error, &targetString, plContext));
        temp = PKIX_String2ASCII((PKIX_PL_String*)targetString, plContext);
        PKIX_TEST_DECREF_BC(targetString);

        if (temp){
                if (PL_strcmp(temp, infoChar) != 0) {
                        testError("Incorrect info returned");
                }
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }

cleanup:

        PKIX_TEST_RETURN();


}

static void
testPrimitiveError(void)
{
        PKIX_PL_String *targetString = NULL;
        PKIX_PL_String *targetStringCopy = NULL;
        char *temp = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString
                    ((PKIX_PL_Object*)PKIX_ALLOC_ERROR(),
                    &targetString, plContext));

        temp = PKIX_String2ASCII(targetString, plContext);
        if (temp){
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }

        targetStringCopy = targetString;

        PKIX_TEST_DECREF_BC(targetString);

        /*
         *  We need to DECREF twice, b/c the PKIX_ALLOC_ERROR object
         *  which holds a cached copy of the stringRep can never be DECREF'd
         */
        PKIX_TEST_DECREF_BC(targetStringCopy);

cleanup:

        PKIX_TEST_RETURN();
}

static void
testChaining(PKIX_Error *error7)
{
        PKIX_PL_String *targetString = NULL;
        PKIX_Error *tempError = NULL;
        char *temp = NULL;
        PKIX_UInt32 i;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Object_ToString((PKIX_PL_Object*)error7,
                                        &targetString, plContext));

        temp = PKIX_String2ASCII(targetString, plContext);
        if (temp){
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }


        for (i = 0, tempError = error7; i < 2; i++) {
                PKIX_TEST_EXPECT_NO_ERROR
                        (PKIX_Error_GetCause(tempError, &tempError, plContext));
                if (tempError == NULL) {
                        testError("Unexpected end to error chain");
                        break;
                }

                PKIX_TEST_EXPECT_NO_ERROR
                        (PKIX_PL_Object_DecRef
                        ((PKIX_PL_Object*)tempError, plContext));
        }

        PKIX_TEST_DECREF_BC(targetString);


cleanup:

        PKIX_TEST_RETURN();
}

static void
testDestroy(PKIX_Error *error)
{
        PKIX_TEST_STD_VARS();

        PKIX_TEST_DECREF_BC(error);

cleanup:

        PKIX_TEST_RETURN();
}

int test_error(int argc, char *argv[]) 
{

        PKIX_Error *error, *error2, *error3, *error5, *error6, *error7;
        char *descChar = "Error Message";
        char *descChar2 = "Another Error Message";
        char *infoChar = "Auxiliary Info";
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;

        PKIX_TEST_STD_VARS();

        startTests("Errors");

        PKIX_TEST_EXPECT_NO_ERROR(
            PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

        subTest("PKIX_Error_Create");
        createErrors
                (&error,
                &error2,
                &error3,
                &error5,
                &error6,
                &error7,
                infoChar);

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (error,
                error,
                error2,
                NULL,
                Error,
                PKIX_TRUE);

        subTest("PKIX_Error_GetErrorClass");
        testGetErrorClass(error, error2);

        subTest("PKIX_Error_GetDescription");
        testGetDescription(error, error2, error3, descChar, descChar2);

        subTest("PKIX_Error_GetCause");
        testGetCause(error, error2, error3);

        subTest("PKIX_Error_GetSupplementaryInfo");
        testGetSupplementaryInfo(error, infoChar);

        subTest("Primitive Error Type");
        testPrimitiveError();

        subTest("Error Chaining");
        testChaining(error7);

        subTest("PKIX_Error_Destroy");
        testDestroy(error);
        testDestroy(error2);
        testDestroy(error3);
        testDestroy(error5);
        testDestroy(error6);
        testDestroy(error7);

cleanup:

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("Errors");

        return (0);

}
