/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * testutil.c
 *
 * Utility error handling functions
 *
 */

#include "testutil.h"

/*
 * static global variable to keep track of total number of errors for
 * a particular test suite (eg. all the OID tests)
 */
static int errCount = 0;

/*
 * FUNCTION: startTests
 * DESCRIPTION:
 *
 *  Prints standard message for starting the test suite with the name pointed
 *  to by "testName". This function should be called in the beginning of every
 *  test suite.
 *
 * PARAMETERS:
 *  "testName"
 *      Address of string representing name of test suite.
 * THREAD SAFETY:
 *  Not Thread Safe - assumes exclusive access to "errCount"
 *  (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns nothing.
 */
void
startTests(char *testName)
{
    (void)printf("*START OF TESTS FOR %s:\n", testName);
    errCount = 0;
}

/*
 * FUNCTION: endTests
 * DESCRIPTION:
 *
 *  Prints standard message for ending the test suite with the name pointed
 *  to by "testName", followed by a success/failure message. This function
 *  should be called at the end of every test suite.
 *
 * PARAMETERS:
 *  "testName"
 *      Address of string representing name of test suite.
 * THREAD SAFETY:
 *  Not Thread Safe - assumes exclusive access to "errCount"
 *  (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns nothing.
 */
void
endTests(char *testName)
{
    char plural = ' ';

    (void)printf("*END OF TESTS FOR %s: ", testName);
    if (errCount > 0) {
        if (errCount > 1)
            plural = 's';
        (void)printf("%d SUBTEST%c FAILED.\n\n", errCount, plural);
    } else {
        (void)printf("ALL TESTS COMPLETED SUCCESSFULLY.\n\n");
    }
}

/*
 * FUNCTION: subTest
 * DESCRIPTION:
 *
 *  Prints standard message for starting the subtest with the name pointed to
 *  by "subTestName". This function should be called at the beginning of each
 *  subtest.
 *
 * PARAMETERS:
 *  "subTestName"
 *      Address of string representing name of subTest.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns nothing.
 */
void
subTest(char *subTestName)
{
    (void)printf("TESTING: %s ...\n", subTestName);
}

/*
 * FUNCTION: testErrorUndo
 * DESCRIPTION:
 *
 *  Decrements the global variable "errCount" and prints a test failure
 *  expected message followed by the string pointed to by "msg". This function
 *  should be called when an expected error condition is encountered in the
 *  tests. Calling this function *correct* the previous errCount increment.
 *  It should only be called ONCE per subtest.
 *
 * PARAMETERS:
 *  "msg"
 *      Address of text of error message.
 * THREAD SAFETY:
 *  Not Thread Safe - assumes exclusive access to "errCount"
 *  (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns nothing.
 */
void
testErrorUndo(char *msg)
{
    --errCount;
    (void)printf("TEST FAILURE *** EXPECTED *** :%s\n", msg);
}

/*
 * FUNCTION: testError
 * DESCRIPTION:
 *
 *  Increments the global variable "errCount" and prints a standard test
 *  failure message followed by the string pointed to by "msg". This function
 *  should be called when an unexpected error condition is encountered in the
 *  tests. It should only be called ONCE per subtest.
 *
 * PARAMETERS:
 *  "msg"
 *      Address of text of error message.
 * THREAD SAFETY:
 *  Not Thread Safe - assumes exclusive access to "errCount"
 *  (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns nothing.
 */
void
testError(char *msg)
{
    ++errCount;
    (void)printf("TEST FAILURE: %s\n", msg);
}

/*
 * FUNCTION: PKIX_String2ASCII
 * DESCRIPTION:
 *
 *  Converts String object pointed to by "string" to its ASCII representation
 *  and returns the converted value. Returns NULL upon failure.
 *
 *  XXX Might want to use ESCASCII_DEBUG to show control characters, etc.
 *
 * PARAMETERS:
 *  "string"
 *      Address of String to be converted to ASCII. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns the ASCII representation of "string" upon success;
 *  NULL upon failure.
 */
char *
PKIX_String2ASCII(PKIX_PL_String *string, void *plContext)
{
    PKIX_UInt32 length;
    char *asciiString = NULL;
    PKIX_Error *errorResult;

    errorResult = PKIX_PL_String_GetEncoded(string,
                                            PKIX_ESCASCII,
                                            (void **)&asciiString,
                                            &length,
                                            plContext);

    if (errorResult)
        goto cleanup;

cleanup:

    if (errorResult) {
        return (NULL);
    }

    return (asciiString);
}

/*
 * FUNCTION: PKIX_Error2ASCII
 * DESCRIPTION:
 *
 *  Converts Error pointed to by "error" to its ASCII representation and
 *  returns the converted value. Returns NULL upon failure.
 *
 * PARAMETERS:
 *  "error"
 *      Address of Error to be converted to ASCII. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns the ASCII representation of "error" upon success;
 *  NULL upon failure.
 */
char *
PKIX_Error2ASCII(PKIX_Error *error, void *plContext)
{
    PKIX_UInt32 length;
    char *asciiString = NULL;
    PKIX_PL_String *pkixString = NULL;
    PKIX_Error *errorResult = NULL;

    errorResult = PKIX_PL_Object_ToString((PKIX_PL_Object *)error, &pkixString, plContext);
    if (errorResult)
        goto cleanup;

    errorResult = PKIX_PL_String_GetEncoded(pkixString,
                                            PKIX_ESCASCII,
                                            (void **)&asciiString,
                                            &length,
                                            plContext);

cleanup:

    if (pkixString) {
        if (PKIX_PL_Object_DecRef((PKIX_PL_Object *)pkixString, plContext)) {
            return (NULL);
        }
    }

    if (errorResult) {
        return (NULL);
    }

    return (asciiString);
}

/*
 * FUNCTION: PKIX_Object2ASCII
 * DESCRIPTION:
 *
 *  Converts Object pointed to by "object" to its ASCII representation and
 *  returns the converted value. Returns NULL upon failure.
 *
 * PARAMETERS:
 *  "object"
 *      Address of Object to be converted to ASCII. Must be non-NULL.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns the ASCII representation of "object" upon success;
 *  NULL upon failure.
 */
char *
PKIX_Object2ASCII(PKIX_PL_Object *object)
{
    PKIX_UInt32 length;
    char *asciiString = NULL;
    PKIX_PL_String *pkixString = NULL;
    PKIX_Error *errorResult = NULL;

    errorResult = PKIX_PL_Object_ToString(object, &pkixString, NULL);
    if (errorResult)
        goto cleanup;

    errorResult = PKIX_PL_String_GetEncoded(pkixString, PKIX_ESCASCII, (void **)&asciiString, &length, NULL);

cleanup:

    if (pkixString) {
        if (PKIX_PL_Object_DecRef((PKIX_PL_Object *)pkixString, NULL)) {
            return (NULL);
        }
    }

    if (errorResult) {
        return (NULL);
    }

    return (asciiString);
}

/*
 * FUNCTION: PKIX_Cert2ASCII
 * DESCRIPTION:
 *
 *  Converts Cert pointed to by "cert" to its partial ASCII representation and
 *  returns the converted value. Returns NULL upon failure.
 *
 * PARAMETERS:
 *  "cert"
 *      Address of Cert to be converted to ASCII. Must be non-NULL.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns the partial ASCII representation of "cert" upon success;
 *  NULL upon failure.
 */
char *
PKIX_Cert2ASCII(PKIX_PL_Cert *cert)
{
    PKIX_PL_X500Name *issuer = NULL;
    void *issuerAscii = NULL;
    PKIX_PL_X500Name *subject = NULL;
    void *subjectAscii = NULL;
    void *asciiString = NULL;
    PKIX_Error *errorResult = NULL;
    PKIX_UInt32 numChars;

    /* Issuer */
    errorResult = PKIX_PL_Cert_GetIssuer(cert, &issuer, NULL);
    if (errorResult)
        goto cleanup;

    issuerAscii = PKIX_Object2ASCII((PKIX_PL_Object *)issuer);

    /* Subject */
    errorResult = PKIX_PL_Cert_GetSubject(cert, &subject, NULL);
    if (errorResult)
        goto cleanup;

    if (subject) {
        subjectAscii = PKIX_Object2ASCII((PKIX_PL_Object *)subject);
    }

    errorResult = PKIX_PL_Malloc(200, &asciiString, NULL);
    if (errorResult)
        goto cleanup;

    numChars =
        PR_snprintf(asciiString,
                    200,
                    "Issuer=%s\nSubject=%s\n",
                    issuerAscii,
                    subjectAscii);

    if (!numChars)
        goto cleanup;

cleanup:

    if (issuer) {
        if (PKIX_PL_Object_DecRef((PKIX_PL_Object *)issuer, NULL)) {
            return (NULL);
        }
    }

    if (subject) {
        if (PKIX_PL_Object_DecRef((PKIX_PL_Object *)subject, NULL)) {
            return (NULL);
        }
    }

    if (PKIX_PL_Free((PKIX_PL_Object *)issuerAscii, NULL)) {
        return (NULL);
    }

    if (PKIX_PL_Free((PKIX_PL_Object *)subjectAscii, NULL)) {
        return (NULL);
    }

    if (errorResult) {
        return (NULL);
    }

    return (asciiString);
}

/*
 * FUNCTION: testHashcodeHelper
 * DESCRIPTION:
 *
 *  Computes the hashcode of the Object pointed to by "goodObject" and the
 *  Object pointed to by "otherObject" and compares them. If the result of the
 *  comparison is not the desired match as specified by "match", an error
 *  message is generated.
 *
 * PARAMETERS:
 *  "goodObject"
 *      Address of an object. Must be non-NULL.
 *  "otherObject"
 *      Address of another object. Must be non-NULL.
 *  "match"
 *      Boolean value representing the desired comparison result.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns nothing.
 */
void
testHashcodeHelper(
    PKIX_PL_Object *goodObject,
    PKIX_PL_Object *otherObject,
    PKIX_Boolean match,
    void *plContext)
{

    PKIX_UInt32 goodHash;
    PKIX_UInt32 otherHash;
    PKIX_Boolean cmpResult;
    PKIX_TEST_STD_VARS();

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Hashcode((PKIX_PL_Object *)goodObject, &goodHash, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Hashcode((PKIX_PL_Object *)otherObject, &otherHash, plContext));

    cmpResult = (goodHash == otherHash);

    if ((match && !cmpResult) || (!match && cmpResult)) {
        testError("unexpected mismatch");
        (void)printf("Hash1:\t%d\n", goodHash);
        (void)printf("Hash2:\t%d\n", otherHash);
    }

cleanup:

    PKIX_TEST_RETURN();
}

/*
 * FUNCTION: testToStringHelper
 * DESCRIPTION:
 *
 *  Calls toString on the Object pointed to by "goodObject" and compares the
 *  result to the string pointed to by "expected". If the results are not
 *  equal, an error message is generated.
 *
 * PARAMETERS:
 *  "goodObject"
 *      Address of Object. Must be non-NULL.
 *  "expected"
 *      Address of the desired string.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns nothing.
 */
void
testToStringHelper(
    PKIX_PL_Object *goodObject,
    char *expected,
    void *plContext)
{
    PKIX_PL_String *stringRep = NULL;
    char *actual = NULL;
    PKIX_TEST_STD_VARS();

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_ToString(goodObject, &stringRep, plContext));

    actual = PKIX_String2ASCII(stringRep, plContext);
    if (actual == NULL) {
        pkixTestErrorMsg = "PKIX_String2ASCII Failed";
        goto cleanup;
    }

    /*
         * If you are having trouble matching the string, uncomment the
         * PL_strstr function to figure out what's going on.
         */

    /*
            if (PL_strstr(actual, expected) == NULL){
                testError("PL_strstr failed");
            }
        */

    if (PL_strcmp(actual, expected) != 0) {
        testError("unexpected mismatch");
        (void)printf("Actual value:\t%s\n", actual);
        (void)printf("Expected value:\t%s\n", expected);
    }

cleanup:

    PKIX_PL_Free(actual, plContext);

    PKIX_TEST_DECREF_AC(stringRep);

    PKIX_TEST_RETURN();
}

/*
 * FUNCTION: testEqualsHelper
 * DESCRIPTION:
 *
 *  Checks if the Object pointed to by "goodObject" is Equal to the Object
 *  pointed to by "otherObject". If the result of the check is not the desired
 *  match as specified by "match", an error message is generated.
 *
 * PARAMETERS:
 *  "goodObject"
 *      Address of an Object. Must be non-NULL.
 *  "otherObject"
 *      Address of another Object. Must be non-NULL.
 *  "match"
 *      Boolean value representing the desired comparison result.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns nothing.
 */
void
testEqualsHelper(
    PKIX_PL_Object *goodObject,
    PKIX_PL_Object *otherObject,
    PKIX_Boolean match,
    void *plContext)
{

    PKIX_Boolean cmpResult;
    PKIX_TEST_STD_VARS();

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Equals(goodObject, otherObject, &cmpResult, plContext));

    if ((match && !cmpResult) || (!match && cmpResult)) {
        testError("unexpected mismatch");
        (void)printf("Actual value:\t%d\n", cmpResult);
        (void)printf("Expected value:\t%d\n", match);
    }

cleanup:

    PKIX_TEST_RETURN();
}

/*
 * FUNCTION: testDuplicateHelper
 * DESCRIPTION:
 *  Checks if the Object pointed to by "object" is equal to its duplicate.
 *  If the result of the check is not equality, an error message is generated.
 * PARAMETERS:
 *  "object"
 *      Address of Object. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns nothing.
 */
void
testDuplicateHelper(PKIX_PL_Object *object, void *plContext)
{
    PKIX_PL_Object *newObject = NULL;
    PKIX_Boolean cmpResult;

    PKIX_TEST_STD_VARS();

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Duplicate(object, &newObject, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Equals(object, newObject, &cmpResult, plContext));

    if (!cmpResult) {
        testError("unexpected mismatch");
        (void)printf("Actual value:\t%d\n", cmpResult);
        (void)printf("Expected value:\t%d\n", PKIX_TRUE);
    }

cleanup:

    PKIX_TEST_DECREF_AC(newObject);

    PKIX_TEST_RETURN();
}
