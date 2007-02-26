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
 * test_string2.c
 *
 * Tests International Strings
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

void *plContext = NULL;

void
createString(
        PKIX_PL_String **vivaEspanaString,
        PKIX_PL_String **straussString,
        PKIX_PL_String **gorbachevString,
        PKIX_PL_String **testUTF16String,
        PKIX_PL_String **chineseString,
        PKIX_PL_String **jeanRenoString)
{
        /* this is meant to fail - it highlights bug 0002 */
        unsigned char utf16String[4] = { 0xF8, 0x60,
                                        0xFC, 0x60};

        unsigned char chinese[16] = { 0xe7, 0xab, 0xa0,
                                        0xe5, 0xad, 0x90,
                                        0xe6, 0x80, 0xa1,
                                        0x20,
                                        0xe4, 0xb8, 0xad,
                                        0xe5, 0x9b, 0xbd
        };

        char* jeanReno = "Jean R\303\251no is an actor.";
        char* gorbachev = /* This is the name "Gorbachev" in cyrllic */
        "\xd0\x93\xd0\xbe\xd1\x80\xd0\xb1\xd0\xb0\xd1\x87\xd1\x91\xd0\xb2";

        char *vivaEspana =
                "&#x00A1;Viva Espa&#x00f1;a!";

        char *strauss =
                "Strau&#x00Df; was born in &#x00D6;sterreich";

        PKIX_TEST_STD_VARS();

        /* ---------------------------- */
        subTest("String Creation");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create(
                                        PKIX_ESCASCII,
                                        vivaEspana,
                                        PL_strlen(vivaEspana),
                                        vivaEspanaString,
                                        plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create(
                                        PKIX_ESCASCII,
                                        strauss,
                                        PL_strlen(strauss),
                                        straussString,
                                        plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create(
                                        PKIX_UTF8,
                                        gorbachev,
                                        PL_strlen(gorbachev),
                                        gorbachevString,
                                        plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create(
                                        PKIX_UTF16,
                                        utf16String,
                                        4,
                                        testUTF16String,
                                        plContext));


        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create(
                                        PKIX_UTF8,
                                        chinese,
                                        16,
                                        chineseString,
                                        plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create(
                                        PKIX_UTF8,
                                        jeanReno,
                                        PL_strlen(jeanReno),
                                        jeanRenoString,
                                        plContext));

cleanup:

        PKIX_TEST_RETURN();
}

void
testGetEncoded(PKIX_PL_String *string, PKIX_UInt32 format)
{
        void *dest = NULL;
        PKIX_UInt32 length;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_GetEncoded
                                (string,
                                format,
                                &dest,
                                &length,
                                plContext));

        if (dest){
                (void) printf("\tResult: %s\n", (char *)dest);
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(dest, plContext));
        }

cleanup:
        PKIX_TEST_RETURN();
}


void
testHTMLOutput(
        PKIX_PL_String *vivaEspanaString,
        PKIX_PL_String *straussString,
        PKIX_PL_String *gorbachevString,
        PKIX_PL_String *testUTF16String,
        PKIX_PL_String *chineseString,
        PKIX_PL_String *jeanRenoString)
{
        void *dest = NULL;
        PKIX_UInt32 length;

        FILE *htmlFile = NULL;

        PKIX_TEST_STD_VARS();

        /* Opening a file for output */
        htmlFile = fopen("utf8.html", "w");

        if (htmlFile != plContext) {
                (void) fprintf(htmlFile, "<html><head>\n");
                (void) fprintf(htmlFile, "<meta http-equiv=\"Content-Type\"");
                (void) fprintf(htmlFile,
                        "content = \"text/html; charset = UTF-8\">\n");
                (void) fprintf(htmlFile, "</head><body>\n");
                (void) fprintf(htmlFile, "<font size =\"+2\">\n");
        } else
                (void) printf("Could not open HTML file\n");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_GetEncoded(testUTF16String,
                                            PKIX_UTF8,
                                            &dest,
                                            &length,
                                            plContext));
        if (htmlFile != plContext) {
                (void) printf("%d bytes written to HTML file\n",
                        fwrite(dest, length, 1, htmlFile));
                (void) fprintf(htmlFile, "<BR>\n");
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(dest, plContext));
        dest = NULL;
        length = 0;

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_GetEncoded(chineseString,
                                            PKIX_UTF8,
                                            &dest,
                                            &length,
                                            plContext));
        if (htmlFile != plContext) {
                (void) printf("%d bytes written to HTML file\n",
                        fwrite(dest, length, 1, htmlFile));
                (void) fprintf(htmlFile, "<BR>\n");
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(dest, plContext));
        dest = NULL;
        length = 0;

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_GetEncoded(jeanRenoString,
                                            PKIX_UTF8,
                                            &dest,
                                            &length,
                                            plContext));
        if (htmlFile != plContext) {
                (void) printf("%d bytes written to HTML file\n",
                        fwrite(dest, length, 1, htmlFile));
                (void) fprintf(htmlFile, "<BR>\n");
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(dest, plContext));
        dest = NULL;
        length = 0;

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_GetEncoded(vivaEspanaString,
                                            PKIX_UTF8,
                                            &dest,
                                            &length,
                                            plContext));
        if (htmlFile != plContext) {
                (void) printf("%d bytes written to HTML file\n",
                        fwrite(dest, length, 1, htmlFile));
                (void) fprintf(htmlFile, "<BR>\n");
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(dest, plContext));
        dest = NULL;
        length = 0;

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_GetEncoded(straussString,
                                            PKIX_UTF8,
                                            &dest,
                                            &length,
                                            plContext));
        if (htmlFile != plContext) {
                (void) printf("%d bytes written to HTML file\n",
                        fwrite(dest, length, 1, htmlFile));
                (void) fprintf(htmlFile, "<BR>\n");
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(dest, plContext));
        dest = NULL;
        length = 0;



        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_GetEncoded(straussString,
                                            PKIX_UTF8,
                                            &dest,
                                            &length,
                                            plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(dest, plContext));
        dest = NULL;
        length = 0;

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_GetEncoded(gorbachevString,
                                            PKIX_UTF8,
                                            &dest,
                                            &length,
                                            plContext));
        if (htmlFile != plContext) {
                (void) printf("%d bytes written to HTML file\n",
                        fwrite(dest, length, 1, htmlFile));
                (void) fprintf(htmlFile, "<BR>\n");
        }

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(dest, plContext));
        dest = NULL;
        length = 0;

        if (htmlFile != plContext) {
                (void) fprintf(htmlFile, "</font>\n");
                (void) fprintf(htmlFile, "</body></html>\n");
                (void) fclose(htmlFile);
        }

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

        PKIX_PL_String *vivaEspanaString, *straussString, *testUTF16String;
        PKIX_PL_String *chineseString, *jeanRenoString, *gorbachevString;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;
        PKIX_Boolean useArenas = PKIX_FALSE;

        PKIX_TEST_STD_VARS();

        startTests("Unicode Strings");

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                                    (PKIX_TRUE, /* nssInitNeeded */
                                    useArenas,
                                    PKIX_MAJOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    &actualMinorVersion,
                                    &plContext));

        subTest("PKIX_PL_String_Create");
        createString(&vivaEspanaString,
                    &straussString,
                    &gorbachevString,
                    &testUTF16String,
                    &chineseString,
                    &jeanRenoString);

        subTest("Converting UTF-16 to EscASCII");
        testGetEncoded(testUTF16String, PKIX_ESCASCII);

        subTest("Converting UTF-8 to EscASCII");
        testGetEncoded(chineseString, PKIX_ESCASCII);

        subTest("Converting UTF-8 to EscASCII");
        testGetEncoded(jeanRenoString, PKIX_ESCASCII);

        subTest("Converting EscASCII to UTF-16");
        testGetEncoded(vivaEspanaString, PKIX_UTF16);

        subTest("Converting UTF-8 to UTF-16");
        testGetEncoded(chineseString, PKIX_UTF16);

        subTest("Creating HTML Output File \'utf8.html\'");
        testHTMLOutput(vivaEspanaString,
                    straussString,
                    gorbachevString,
                    testUTF16String,
                    chineseString,
                    jeanRenoString);

        subTest("Unicode Destructors");
        testDestroy(testUTF16String);
        testDestroy(chineseString);
        testDestroy(jeanRenoString);
        testDestroy(vivaEspanaString);
        testDestroy(straussString);
        testDestroy(gorbachevString);

cleanup:

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("Unicode Strings");

        return (0);

}
