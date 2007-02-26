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
 * test_string.c
 *
 * Tests Strings.
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

void *plContext = NULL;

void
createString(
        PKIX_PL_String **testString,
        PKIX_UInt32 format,
        char *stringAscii,
        PKIX_UInt32 length)
{
        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_String_Create
                (format, stringAscii, length, testString, plContext));

cleanup:

        PKIX_TEST_RETURN();
}

void
createStringOther(
        PKIX_PL_String **testEscAscii,
        PKIX_PL_String **testUtf16,
        PKIX_PL_String **ampString,
        PKIX_PL_String **testDebugAscii,
        PKIX_PL_String **testNullString,
        PKIX_UInt32 *utf16data)
{
        char *nullText = "Hi&#x0000; there!";

        char *escAsciiString =
                "&#x00A1;&#x00010000;&#x0FFF;&#x00100001;";

        char *debugAsciiString =
                "string with&#x000A;newlines and&#x0009;tabs";

        char * utfAmp = "\x00&";

        PKIX_TEST_STD_VARS();

        createString(testEscAscii,
                    PKIX_ESCASCII,
                    escAsciiString,
                    PL_strlen(escAsciiString));

        createString(testUtf16, PKIX_UTF16, (char *)utf16data, 12);

        createString(ampString, PKIX_UTF16, utfAmp, 2);

        createString(testDebugAscii,
                    PKIX_ESCASCII_DEBUG,
                    debugAsciiString,
                    PL_strlen(debugAsciiString));

        createString(testNullString,
                    PKIX_ESCASCII_DEBUG,
                    nullText,
                    PL_strlen(nullText));

        goto cleanup;

cleanup:

        PKIX_TEST_RETURN();
}

void
testGetEncoded(
        PKIX_PL_String *testEscAscii,
        PKIX_PL_String *testString0,
        PKIX_PL_String *testDebugAscii,
        PKIX_PL_String *testNullString,
        PKIX_UInt32 *utf16data)
{
        char *temp = NULL;
        void *dest = NULL;
        void *dest2 = NULL;
        char *plainText = "string with\nnewlines and\ttabs";
        PKIX_UInt32 length, length2, i;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_GetEncoded(testEscAscii,
                                        PKIX_UTF16,
                                        &dest,
                                        &length,
                                        plContext));
        for (i = 0; i < length; i++) {
                if (((char*)dest)[i] != ((char*)utf16data)[i]) {
                        testError("UTF-16 Data Differs from Source");
                        printf("%d-th char is different -%c-%c-\n", i,
                               ((char*)dest)[i], ((char*)utf16data)[i]);
                }
        }

        length = 0;
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(dest, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_GetEncoded(testNullString,
                                        PKIX_UTF16,
                                        &dest,
                                        &length,
                                        plContext));

        length = 0;
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(dest, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_GetEncoded(testString0,
                                        PKIX_ESCASCII_DEBUG,
                                        &dest,
                                        &length,
                                        plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_GetEncoded(testDebugAscii,
                                        PKIX_ESCASCII_DEBUG,
                                        &dest2,
                                        &length2,
                                        plContext));

        for (i = 0; (i < length) && (i < length2); i++)
                if (((char*)dest)[i] != ((char*)dest2)[i]) {
                        testError("Equivalent strings are unequal");
                        break;
                }

        length = 0;
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(dest, plContext));
        length2 = 0;
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(dest2, plContext));

        temp = PKIX_String2ASCII(testDebugAscii, plContext);
        if (temp){
                if (PL_strcmp(plainText, temp) != 0)
                        testError("Debugged ASCII does not match "
                                    "equivalent EscAscii");
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        }

cleanup:

        PKIX_TEST_RETURN();
}

void
testSprintf(void)
{
        PKIX_Int32 x = 0xCAFE;
        PKIX_Int32 y = -12345;
        PKIX_PL_String *testString = NULL;
        PKIX_PL_String *formatString = NULL;
        PKIX_PL_String *sprintfString = NULL;
        char *plainText = "Testing Sprintf";
        char *format = "%s %x %u %d";
        char *convertedFormat = "%s %lx %lu %ld";
        char *temp = NULL;
        char *temp2 = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create(
                                        PKIX_ESCASCII,
                                        plainText,
                                        PL_strlen(plainText),
                                        &testString,
                                        plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create(
                                        PKIX_ESCASCII,
                                        format,
                                        11,
                                        &formatString,
                                        plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Sprintf(&sprintfString,
                                plContext,
                                formatString,
                                testString, x, y, y));
        PKIX_TEST_DECREF_BC(testString);

        temp = PR_smprintf(convertedFormat, plainText, x, y, y);
        temp2 = PKIX_String2ASCII(sprintfString, plContext);

        if (PL_strcmp(temp, temp2) != 0)
                testError("Sprintf produced incorrect output");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(temp2, plContext));


        PKIX_TEST_DECREF_BC(sprintfString);


        PKIX_TEST_DECREF_BC(formatString);


cleanup:

        PKIX_TEST_RETURN();
}

void
testErrorHandling(void)
{
        char *debugAsciiString =
                "string with&#x000A;newlines and&#x0009;tabs";

        PKIX_PL_String *testString = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_ERROR(PKIX_PL_String_Create(
                                        PKIX_ESCASCII,
                                        NULL,
                                        50,
                                        &testString,
                                        plContext));

        PKIX_TEST_EXPECT_ERROR(PKIX_PL_String_Create(PKIX_ESCASCII,
                                        "blah", 4, NULL, plContext));

        PKIX_TEST_EXPECT_ERROR(PKIX_PL_Sprintf(&testString, plContext, NULL));

        PKIX_TEST_EXPECT_ERROR
                (PKIX_PL_GetString(0, NULL, &testString, plContext));

        PKIX_TEST_EXPECT_ERROR(PKIX_PL_GetString(0, "blah", 0, plContext));

        /* ---------------------------- */
        subTest("Unicode Error Handling");

        /* &#x must be followed by 4 hexadecimal digits */
        PKIX_TEST_EXPECT_ERROR(PKIX_PL_String_Create(
                                            PKIX_ESCASCII,
                                            "&#x003k;",
                                            7,
                                            &testString,
                                            plContext));

        /* &#x must be followed by 4 hexadecimal digits */
        PKIX_TEST_EXPECT_ERROR(PKIX_PL_String_Create(
                                            PKIX_ESCASCII,
                                            "abc&#x00",
                                            8,
                                            &testString,
                                            plContext));

        /* &#x must be between 00010000-0010FFFF */
        PKIX_TEST_EXPECT_ERROR(PKIX_PL_String_Create(
                                            PKIX_ESCASCII,
                                            "&#x00200101;",
                                            11,
                                            &testString,
                                            plContext));

        /* &#x must be followed by 8 hexadecimal digits */
        PKIX_TEST_EXPECT_ERROR(PKIX_PL_String_Create(
                                            PKIX_ESCASCII,
                                            "&#x001000",
                                            10,
                                            &testString,
                                            plContext));

        /* &#x must be followed by 8 hexadecimal digits */
        PKIX_TEST_EXPECT_ERROR(PKIX_PL_String_Create(
                                            PKIX_ESCASCII,
                                            "&#x0010m00;",
                                            10,
                                            &testString,
                                            plContext));

        /* Byte values D800-DFFF are reserved */
        PKIX_TEST_EXPECT_ERROR(PKIX_PL_String_Create(
                                            PKIX_ESCASCII,
                                            "&#xD800;",
                                            7,
                                            &testString,
                                            plContext));

        /* Can't use &#x for regular characters */
        PKIX_TEST_EXPECT_ERROR(PKIX_PL_String_Create(
                                            PKIX_ESCASCII,
                                            "&#x0032;",
                                            7,
                                            &testString,
                                            plContext));

        /* Can't use non-printable characters */
        PKIX_TEST_EXPECT_ERROR(PKIX_PL_String_Create(
                                            PKIX_ESCASCII,
                                            "\xA1",
                                            1,
                                            &testString,
                                            plContext));

        /* Only legal \\ characters are \\, u and U */
        PKIX_TEST_EXPECT_ERROR(PKIX_PL_String_Create(
                                            PKIX_ESCASCII,
                                            "&blah",
                                            5,
                                            &testString,
                                            plContext));



        /* Surrogate pairs must be legal */
        PKIX_TEST_EXPECT_ERROR(PKIX_PL_String_Create(
                                            PKIX_UTF16,
                                            "\xd8\x00\x0\x66",
                                            4,
                                            &testString,
                                            plContext));

        /* Debugged EscASCII should not be accepted as EscASCII */
        PKIX_TEST_EXPECT_ERROR(PKIX_PL_String_Create(
                                        PKIX_ESCASCII,
                                        debugAsciiString,
                                        PL_strlen(debugAsciiString),
                                        &testString,
                                        plContext));
cleanup:

        PKIX_TEST_RETURN();
}

void
testDestroy(
        PKIX_PL_String *string)
{
        PKIX_TEST_STD_VARS();

        PKIX_TEST_DECREF_BC(string);

cleanup:

        PKIX_TEST_RETURN();
}


int main(int argc, char *argv[]) {

        PKIX_PL_String *testString[6] = {NULL};
        PKIX_PL_String *testNullString = NULL;
        PKIX_PL_String *testDebugAscii = NULL;
        PKIX_PL_String *testEscAscii = NULL;
        PKIX_PL_String *testUtf16 = NULL;
        PKIX_PL_String *ampString = NULL;
        unsigned char utf16Data[] = {0x00, 0xA1, 0xD8, 0x00,
                            0xDC, 0x00, 0x0F, 0xFF,
                            0xDB, 0xC0, 0xDC, 0x01};
        PKIX_UInt32 i, size = 6;

        char *plainText[6] = {
                "string with\nnewlines and\ttabs",
                "Not an escaped char: &amp;#x0012;",
                "Encode &amp; with &amp;amp; in ASCII",
                "&#x00A1;",
                "&amp;",
                "string with\nnewlines and\ttabs"
        };

        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;
        PKIX_Boolean useArenas = PKIX_FALSE;

        PKIX_TEST_STD_VARS();

        startTests("Strings");

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                                    (PKIX_TRUE, /* nssInitNeeded */
                                    useArenas,
                                    PKIX_MAJOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    &actualMinorVersion,
                                    &plContext));

        subTest("PKIX_PL_String_Create <ascii format>");
        for (i = 0; i < size; i++) {
                testString[i] = NULL;
                createString
                        (&testString[i],
                        PKIX_ESCASCII,
                        plainText[i],
                        PL_strlen(plainText[i]));
        }

        subTest("PKIX_PL_String_Create <other formats>");
        createStringOther
                (&testEscAscii,
                &testUtf16,
                &ampString,
                &testDebugAscii,
                &testNullString,
                (PKIX_UInt32 *)utf16Data);

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (testString[0],
                testString[5],
                testString[1],
                plainText[0],
                String,
                PKIX_TRUE);

        subTest("PKIX_PL_String_GetEncoded");
        testGetEncoded
                (testEscAscii,
                testString[0],
                testDebugAscii,
                testNullString,
                (PKIX_UInt32 *)utf16Data);

        subTest("PKIX_PL_Sprintf");
        testSprintf();

        subTest("PKIX_PL_String_Create <error_handling>");
        testErrorHandling();

        subTest("PKIX_PL_String_Destroy");
        for (i = 0; i < size; i++) {
                testDestroy(testString[i]);
        }
        testDestroy(testEscAscii);
        testDestroy(testUtf16);
        testDestroy(ampString);
        testDestroy(testDebugAscii);
        testDestroy(testNullString);

cleanup:

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("String");

        return (0);

}
