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
 * testutil.h
 *
 * Utility functions for handling test errors
 *
 */

#ifndef _TESTUTIL_H
#define _TESTUTIL_H

#include "pkix.h"
#include "plstr.h"
#include "prprf.h"
#include "prlong.h"
#include "pkix_pl_common.h"
#include "secutil.h"
#include <stdio.h>
#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * In order to have a consistent format for displaying test information,
 * all tests are REQUIRED to use the functions provided by this library
 * (libtestutil.a) for displaying their information.
 *
 * A test using this library begins with a call to startTests with the test
 * name as the arg (which is used only for formatting). Before the first
 * subtest, a call to subTest should be made with the subtest name as the arg
 * (again, for formatting). If the subTest is successful, then no action
 * is needed. However, if the subTest is not successful, then a call
 * to testError should be made with a descriptive error message as the arg.
 * Note that a subTest MUST NOT call testError more than once.
 * Finally, a call to endTests is made with the test name as the arg (for
 * formatting). Note that most of these macros assume that a variable named
 * "plContext" of type (void *) has been defined by the test. As such, it
 * is essential that the test satisfy this condition.
 */

/*
 * PKIX_TEST_STD_VARS should be called at the beginning of every function
 * that uses PKIX_TEST_RETURN (e.g. subTests), but it should be called only
 * AFTER declaring local variables (so we don't get compiler warnings about
 * declarations after statements). PKIX_TEST_STD_VARS declares and initializes
 * several variables needed by the other test macros.
 */
#define PKIX_TEST_STD_VARS() \
        PKIX_Error *pkixTestErrorResult = NULL; \
        char *pkixTestErrorMsg = NULL;

/*
 * PKIX_TEST_EXPECT_NO_ERROR should be used to wrap a standard PKIX function
 * call (one which returns a pointer to PKIX_Error) that is expected to return
 * NULL (i.e. to succeed). If "pkixTestErrorResult" is not NULL,
 * "goto cleanup" is executed, where a testError call is made if there were
 * unexpected results. This macro MUST NOT be called after the "cleanup" label.
 *
 * Example Usage: PKIX_TEST_EXPECT_NO_ERROR(pkixFunc_expected_to_succeed(...));
 */

#define PKIX_TEST_EXPECT_NO_ERROR(func) \
        do { \
                pkixTestErrorResult = (func); \
                if (pkixTestErrorResult) { \
                        goto cleanup; \
                } \
        } while (0)

/*
 * PKIX_TEST_EXPECT_ERROR should be used to wrap a standard PKIX function call
 * (one which returns a pointer to PKIX_Error) that is expected to return
 * a non-NULL value (i.e. to fail). If "pkixTestErrorResult" is NULL,
 * "pkixTestErrorMsg" is set to a standard string and "goto cleanup"
 * is executed, where a testError call is made if there were unexpected
 * results. This macro MUST NOT be called after the "cleanup" label.
 *
 * Example Usage:  PKIX_TEST_EXPECT_ERROR(pkixFunc_expected_to_fail(...));
 */

#define PKIX_TEST_EXPECT_ERROR(func) \
        do { \
                pkixTestErrorResult = (func); \
                if (!pkixTestErrorResult){ \
                        pkixTestErrorMsg = \
                                "Should have thrown an error here."; \
                        goto cleanup; \
                } \
                PKIX_TEST_DECREF_BC(pkixTestErrorResult); \
        } while (0)

/*
 * PKIX_TEST_DECREF_BC is a convenience macro which should only be called
 * BEFORE the "cleanup" label ("BC"). If the input parameter is non-NULL, it
 * DecRefs the input parameter and wraps the function with
 * PKIX_TEST_EXPECT_NO_ERROR, which executes "goto cleanup" upon error.
 * This macro MUST NOT be called after the "cleanup" label.
 */

#define PKIX_TEST_DECREF_BC(obj) \
        do { \
                if (obj){ \
                        PKIX_TEST_EXPECT_NO_ERROR \
                        (PKIX_PL_Object_DecRef \
                                ((PKIX_PL_Object*)(obj), plContext)); \
                obj = NULL; \
                } \
        } while (0)

/*
 * PKIX_TEST_DECREF_AC is a convenience macro which should only be called
 * AFTER the "cleanup" label ("AC"). If the input parameter is non-NULL, it
 * DecRefs the input parameter. A pkixTestTempResult variable is used to prevent
 * incorrectly overwriting pkixTestErrorResult with NULL.
 * In the case DecRef succeeds, pkixTestTempResult will be NULL, and we won't
 * overwrite a previously set pkixTestErrorResult (if any). If DecRef fails,
 * then we do want to overwrite a previously set pkixTestErrorResult since a
 * DecRef failure is fatal and may be indicative of memory corruption.
 */

#define PKIX_TEST_DECREF_AC(obj) \
        do { \
                if (obj){ \
                        PKIX_Error *pkixTestTempResult = NULL; \
                        pkixTestTempResult = \
                        PKIX_PL_Object_DecRef \
                                ((PKIX_PL_Object*)(obj), plContext); \
                        if (pkixTestTempResult) \
                                pkixTestErrorResult = pkixTestTempResult; \
                        obj = NULL; \
                } \
        } while (0)

/*
 * PKIX_TEST_RETURN must always be AFTER the "cleanup" label. It does nothing
 * if everything went as expected. However, if there were unexpected results,
 * PKIX_TEST_RETURN calls testError, which displays a standard failure message
 * and increments the number of subtests that have failed. In the case
 * of an unexpected error, testError is called using the error's description
 * as an input and the error is DecRef'd. In the case of unexpected success
 * testError is called with a standard string.
 */
#define PKIX_TEST_RETURN() \
        { \
                if (pkixTestErrorMsg){ \
                        testError(pkixTestErrorMsg); \
                } else if (pkixTestErrorResult){ \
                        pkixTestErrorMsg = \
                                PKIX_Error2ASCII \
                                        (pkixTestErrorResult, plContext); \
                        if (pkixTestErrorMsg) { \
                                testError(pkixTestErrorMsg); \
                                PKIX_PL_Free \
                                        ((PKIX_PL_Object *)pkixTestErrorMsg, \
                                        plContext); \
                        } else { \
                                testError("PKIX_Error2ASCII Failed"); \
                        } \
                        if (pkixTestErrorResult != PKIX_ALLOC_ERROR()){ \
                                PKIX_PL_Object_DecRef \
                                ((PKIX_PL_Object*)pkixTestErrorResult, \
                                plContext); \
                                pkixTestErrorResult = NULL; \
                        } \
                } \
        }

/*
 * PKIX_TEST_EQ_HASH_TOSTR_DUP is a convenience macro which executes the
 * standard set of operations that test the Equals, Hashcode, ToString, and
 * Duplicate functions of an object type. The goodObj, equalObj, and diffObj
 * are as the names suggest. The expAscii parameter is the expected result of
 * calling ToString on the goodObj. If expAscii is NULL, then ToString will
 * not be called on the goodObj. The checkDuplicate parameter is treated as
 * a Boolean to indicate whether the Duplicate function should be tested. If
 * checkDuplicate is NULL, then Duplicate will not be called on the goodObj.
 * The type is the name of the function's family. For example, if the type is
 * Cert, this macro will call PKIX_PL_Cert_Equals, PKIX_PL_Cert_Hashcode, and
 * PKIX_PL_Cert_ToString.
 *
 * Note: If goodObj uses the default Equals and Hashcode functions, then
 * for goodObj and equalObj to be equal, they must have the same pointer value.
 */
#define PKIX_TEST_EQ_HASH_TOSTR_DUP(goodObj, equalObj, diffObj, \
                                        expAscii, type, checkDuplicate) \
        do { \
                subTest("PKIX_PL_" #type "_Equals   <match>"); \
                testEqualsHelper \
                        ((PKIX_PL_Object *)(goodObj), \
                        (PKIX_PL_Object *)(equalObj), \
                        PKIX_TRUE, \
                        plContext); \
                subTest("PKIX_PL_" #type "_Hashcode <match>"); \
                testHashcodeHelper \
                        ((PKIX_PL_Object *)(goodObj), \
                        (PKIX_PL_Object *)(equalObj), \
                        PKIX_TRUE, \
                        plContext); \
                subTest("PKIX_PL_" #type "_Equals   <non-match>"); \
                testEqualsHelper \
                        ((PKIX_PL_Object *)(goodObj), \
                        (PKIX_PL_Object *)(diffObj), \
                        PKIX_FALSE, \
                        plContext); \
                subTest("PKIX_PL_" #type "_Hashcode <non-match>"); \
                testHashcodeHelper \
                        ((PKIX_PL_Object *)(goodObj), \
                        (PKIX_PL_Object *)(diffObj), \
                        PKIX_FALSE, \
                        plContext); \
                if (expAscii){ \
                        subTest("PKIX_PL_" #type "_ToString"); \
                        testToStringHelper \
                                ((PKIX_PL_Object *)(goodObj), \
                                (expAscii), \
                                plContext); } \
                if (checkDuplicate){ \
                        subTest("PKIX_PL_" #type "_Duplicate"); \
                        testDuplicateHelper \
                                ((PKIX_PL_Object *)goodObj, plContext); } \
        } while (0)

/*
 * PKIX_TEST_DECREF_BC is a convenience macro which should only be called
 * BEFORE the "cleanup" label ("BC"). If the input parameter is non-NULL, it
 * DecRefs the input parameter and wraps the function with
 * PKIX_TEST_EXPECT_NO_ERROR, which executes "goto cleanup" upon error.
 * This macro MUST NOT be called after the "cleanup" label.
 */

#define PKIX_TEST_ABORT_ON_NULL(obj) \
        do { \
                if (!obj){ \
                        goto cleanup; \
                } \
        } while (0)

#define PKIX_TEST_ARENAS_ARG(arena) \
        (arena? \
        (PORT_Strcmp(arena, "-arenas") ? PKIX_FALSE : (j++, PKIX_TRUE)): \
        PKIX_FALSE)

#define PKIX_TEST_ERROR_RECEIVED (pkixTestErrorMsg || pkixTestErrorResult)

/* see source file for function documentation */

void startTests(char *testName);

void endTests(char *testName);

void subTest(char *subTestName);

void testError(char *msg);

extern PKIX_Error *
_ErrorCheck(PKIX_Error *errorResult);

extern PKIX_Error *
_OutputError(PKIX_Error *errorResult);

char* PKIX_String2ASCII(PKIX_PL_String *string, void *plContext);

char* PKIX_Error2ASCII(PKIX_Error *error, void *plContext);

char* PKIX_Object2ASCII(PKIX_PL_Object *object);

char *PKIX_Cert2ASCII(PKIX_PL_Cert *cert);

void
testHashcodeHelper(
        PKIX_PL_Object *goodObject,
        PKIX_PL_Object *otherObject,
        PKIX_Boolean match,
        void *plContext);

void
testToStringHelper(
        PKIX_PL_Object *goodObject,
        char *expected,
        void *plContext);

void
testEqualsHelper(
        PKIX_PL_Object *goodObject,
        PKIX_PL_Object *otherObject,
        PKIX_Boolean match,
        void *plContext);

void
testDuplicateHelper(
        PKIX_PL_Object *object,
        void *plContext);
void
testErrorUndo(char *msg);

#ifdef __cplusplus
}
#endif

#endif /* TESTUTIL_H */
