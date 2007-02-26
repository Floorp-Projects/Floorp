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
 * test_object.c
 *
 * Test Object Allocation, toString, Equals, Destruction
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

void *plContext = NULL;

static PKIX_Error *
destructor(
        /* ARGSUSED */ PKIX_PL_Object *object,
        /* ARGSUSED */ void *plContext)
{
        (void) printf("\tUser defined destructor called\n");
        return (NULL);
}

static PKIX_Error*
toStringCallback(
        PKIX_PL_Object *obj,
        PKIX_PL_String **pString,
        /* ARGSUSED */ void* plContext) {

        PKIX_Error *errorResult;
        PKIX_UInt32 type;
        char *format = "(addr: %x, type: %d)";
        PKIX_PL_String *formatString = NULL;

        errorResult = PKIX_PL_String_Create(
                                        PKIX_ESCASCII,
                                        format,
                                        PL_strlen(format),
                                        &formatString,
                                        plContext);
        if (errorResult) testError("PKIX_PL_String_Create failed");

        if (pString == plContext)
                testError("Null String");

        type = (unsigned int)0;

        (void) PKIX_PL_Object_GetType(obj, &type, plContext);

        errorResult = PKIX_PL_Sprintf(pString, plContext,
                                    formatString,
                                    (int)obj, type);
        if (errorResult) testError("PKIX_PL_Sprintf failed");


        errorResult = PKIX_PL_Object_DecRef((PKIX_PL_Object*)formatString,
                                        plContext);
        if (errorResult) testError("PKIX_PL_Object_DecRef failed");

        return (NULL);
}

static PKIX_Error *
comparator(
        PKIX_PL_Object *first,
        PKIX_PL_Object *second,
        PKIX_Int32 *pValue,
        /* ARGSUSED */ void *plContext)
{
        if (*(char *)first > *(char *)second)
                *pValue = 1;
        else if (*(char *)first < *(char *)second)
                *pValue = -1;
        else
                *pValue = 0;
        return (NULL);
}


PKIX_Error *
hashcodeCallback(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pValue,
        /* ARGSUSED */ void *plContext)
{
        *pValue = 123456789;
        return (NULL);
}

static PKIX_Error*
equalsCallback(
        PKIX_PL_Object *first,
        PKIX_PL_Object *second,
        PKIX_Boolean *result,
        void* plContext) {

        PKIX_UInt32 firstType = 0, secondType = 0;

        if ((first == plContext)||(second == plContext))
                testError("Null Object");

        (void) PKIX_PL_Object_GetType(first, &firstType, plContext);

        (void) PKIX_PL_Object_GetType(second, &secondType, plContext);

        *result = (firstType == secondType)?PKIX_TRUE:PKIX_FALSE;

        return (NULL);
}

void
createObjects(
        PKIX_PL_Object **obj,
        PKIX_PL_Object **obj2,
        PKIX_PL_Object **obj3,
        PKIX_PL_Object **obj4)
{
        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_RegisterType
                    (1000, /* type */
                    "thousand", /* description */
                    NULL, /* destructor */
                    NULL, /* equals */
                    (PKIX_PL_HashcodeCallback)hashcodeCallback,
                    NULL,  /* toString */
                    NULL,  /* Comparator */
                    NULL,
                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Alloc
                    (1000, /* type */
                    12, /* size */
                    obj,
                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_RegisterType
                    (2000, /* type */
                    "two thousand" /* description */,
                    (PKIX_PL_DestructorCallback)destructor,
                    (PKIX_PL_EqualsCallback)equalsCallback,
                    NULL, /* hashcode */
                    (PKIX_PL_ToStringCallback)toStringCallback,
                    (PKIX_PL_ComparatorCallback)comparator,
                    NULL,
                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Alloc
                    (2000, /* type */
                    1, /* size */
                    obj2,
                    plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Alloc
                    (2000, /* type */
                    1, /* size */
                    obj4,
                    plContext));

        *obj3 = *obj;
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_IncRef(*obj3, plContext));

cleanup:
        PKIX_TEST_RETURN();
}


void
testGetType(
            PKIX_PL_Object *obj,
            PKIX_PL_Object *obj2,
            PKIX_PL_Object *obj3)
{
        PKIX_UInt32 testType;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Object_GetType(obj, &testType, plContext));

        if (testType != 1000)
                testError("Object 1 returned the wrong type");

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Object_GetType(obj2, &testType, plContext));
        if (testType != 2000)
                testError("Object 2 returned the wrong type");

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Object_GetType(obj3, &testType, plContext));
        if (testType != 1000)
                testError("Object 3 returned the wrong type");

cleanup:

        PKIX_TEST_RETURN();
}

void
testCompare(
            PKIX_PL_Object *obj2,
            PKIX_PL_Object *obj4)
{
        PKIX_Int32 cmpResult;
        PKIX_TEST_STD_VARS();

        *(char *)obj2 = 0x20;
        *(char *)obj4 = 0x10;
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Object_Compare(obj2, obj4, &cmpResult, plContext));
        if (cmpResult <= 0) testError("Invalid Result from Object Compare");

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Object_Compare(obj4, obj2, &cmpResult, plContext));
        if (cmpResult >= 0) testError("Invalid Result from Object Compare");

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_Object_Compare(obj4, obj4, &cmpResult, plContext));

        *(char *)obj2 = 0x10;
        if (cmpResult != 0) testError("Invalid Result from Object Compare");


cleanup:

        PKIX_TEST_RETURN();
}


void
testDestroy(
            PKIX_PL_Object *obj,
            PKIX_PL_Object *obj2,
            PKIX_PL_Object *obj3,
            PKIX_PL_Object *obj4)
{
        PKIX_TEST_STD_VARS();

        PKIX_TEST_DECREF_BC(obj);
        PKIX_TEST_DECREF_BC(obj2);
        PKIX_TEST_DECREF_BC(obj3);
        PKIX_TEST_DECREF_BC(obj4);

cleanup:

        PKIX_TEST_RETURN();
}

int main(int argc, char *argv[]) {

        PKIX_PL_Object *obj, *obj2, *obj3, *obj4;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;
        PKIX_Boolean useArenas = PKIX_FALSE;

        PKIX_TEST_STD_VARS();

        startTests("Objects");

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                                    (PKIX_TRUE, /* nssInitNeeded */
                                    useArenas,
                                    PKIX_MAJOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    &actualMinorVersion,
                                    &plContext));

        subTest("PKIX_PL_Object_Create");
        createObjects(&obj, &obj2, &obj3, &obj4);

        PKIX_TEST_EQ_HASH_TOSTR_DUP(obj, obj3, obj2, NULL, Object, PKIX_FALSE);

        subTest("PKIX_PL_Object_GetType");
        testGetType(obj, obj2, obj3);

        subTest("PKIX_PL_Object_Compare");
        testCompare(obj2, obj4);

        subTest("PKIX_PL_Object_Destroy");
        testDestroy(obj, obj2, obj3, obj4);


cleanup:

        PKIX_Shutdown(plContext);

        endTests("Objects");

        return (0);

}
