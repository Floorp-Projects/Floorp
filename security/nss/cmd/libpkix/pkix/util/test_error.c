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
 * test_error.c
 *
 * Tests Error Object Creation, ToString, Callbacks and Destroy
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

void *plContext = NULL;

void createErrors(
        PKIX_Error **error,
        PKIX_Error **error2,
        PKIX_Error **error3,
        PKIX_Error **error5,
        PKIX_Error **error6,
        PKIX_Error **error7,
        char *descChar,
        char *descChar2,
        char *infoChar)

{
        PKIX_PL_String *descString = NULL;
        PKIX_PL_String *desc2String = NULL;
        PKIX_PL_String *infoString = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create(
                                        PKIX_ESCASCII,
                                        descChar,
                                        PL_strlen(descChar),
                                        &descString,
                                        plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create(
                                        PKIX_ESCASCII,
                                        descChar2,
                                        PL_strlen(descChar2),
                                        &desc2String,
                                        plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create(
                                        PKIX_ESCASCII,
                                        infoChar,
                                        PL_strlen(infoChar),
                                        &infoString,
                                        plContext));

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_Error_Create
                (PKIX_MEM_ERROR, NULL, NULL, desc2String, error2, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Error_Create
                                    (PKIX_OBJECT_ERROR,
                                    *error2,
                                    (PKIX_PL_Object*)infoString,
                                    descString,
                                    error,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Error_Create
                                    (PKIX_OBJECT_ERROR,
                                    *error2,
                                    (PKIX_PL_Object*)infoString,
                                    descString,
                                    error3,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Error_Create
                                    (PKIX_OBJECT_ERROR,
                                    NULL,
                                    (PKIX_PL_Object*)infoString,
                                    NULL,
                                    error5,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Error_Create
                                    (PKIX_MEM_ERROR,
                                    *error5,
                                    (PKIX_PL_Object*)infoString,
                                    NULL,
                                    error6,
                                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Error_Create
                                    (PKIX_OBJECT_ERROR,
                                    *error6,
                                    (PKIX_PL_Object*)infoString,
                                    NULL,
                                    error7,
                                    plContext));

cleanup:

        PKIX_TEST_DECREF_AC(descString);
        PKIX_TEST_DECREF_AC(desc2String);
        PKIX_TEST_DECREF_AC(infoString);

        PKIX_TEST_RETURN();
}

void testGetErrorCode(PKIX_Error *error, PKIX_Error *error2){

        PKIX_UInt32 code;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_Error_GetErrorCode(error, &code, plContext));

        if (code != PKIX_OBJECT_ERROR) {
                testError("Incorrect Code Returned");
        }

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_Error_GetErrorCode(error2, &code, plContext));

        if (code != PKIX_MEM_ERROR) {
                testError("Incorrect Code Returned");
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Error_GetErrorCode(PKIX_ALLOC_ERROR(),
                                            &code, plContext));
        if (code != PKIX_FATAL_ERROR) {
                testError("Incorrect Code Returned");
        }

cleanup:

        PKIX_TEST_RETURN();


}

void testGetDescription(
        PKIX_Error *error,
        PKIX_Error *error2,
        PKIX_Error *error3,
        char *descChar,
        char *descChar2){

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

void testGetCause(PKIX_Error *error, PKIX_Error *error2, PKIX_Error *error3){

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

void testGetSupplementaryInfo(PKIX_Error *error, char *infoChar){

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

void
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

void
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

void
testDestroy(PKIX_Error *error)
{
        PKIX_TEST_STD_VARS();

        PKIX_TEST_DECREF_BC(error);

cleanup:

        PKIX_TEST_RETURN();
}

int main(int argc, char *argv[]) {

        PKIX_Error *error, *error2, *error3, *error5, *error6, *error7;
        char *descChar = "Error Message";
        char *descChar2 = "Another Error Message";
        char *infoChar = "Auxiliary Info";
        PKIX_UInt32 actualMinorVersion;
        PKIX_Boolean useArenas = PKIX_FALSE;
        PKIX_UInt32 j = 0;

        PKIX_TEST_STD_VARS();

        startTests("Errors");

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                                    (PKIX_TRUE, /* nssInitNeeded */
                                    useArenas,
                                    PKIX_MAJOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    &actualMinorVersion,
                                    &plContext));

        subTest("PKIX_Error_Create");
        createErrors
                (&error,
                &error2,
                &error3,
                &error5,
                &error6,
                &error7,
                descChar,
                descChar2,
                infoChar);

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (error,
                error,
                error2,
                NULL,
                Error,
                PKIX_TRUE);

        subTest("PKIX_Error_GetErrorCode");
        testGetErrorCode(error, error2);

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
