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
 * stress_test.c
 *
 * Creates and deletes many objects
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

void *plContext = NULL;

int main(int argc, char *argv[]) {

        PKIX_UInt32 i, k, length, hashcode;
        PKIX_UInt32 size = 17576;
        char temp[4];
        PKIX_Boolean result;
        PKIX_PL_String *strings[17576], *tempString;
        PKIX_PL_String *utf16strings[17576];
        PKIX_PL_ByteArray *byteArrays[17576];
        void *dest;
        PKIX_PL_HashTable *ht = NULL;
        PKIX_UInt32 actualMinorVersion;
        PKIX_Boolean useArenas = PKIX_FALSE;
        PKIX_UInt32 j = 0;

        PKIX_TEST_STD_VARS();

        startTests("Stress Test");

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                                    (PKIX_TRUE, /* nssInitNeeded */
                                    useArenas,
                                    PKIX_MAJOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    &actualMinorVersion,
                                    &plContext));

        /* ---------------------------- */
        subTest("Create every three letter String");

        for (i = 0; i < 26; i++)
                for (j = 0; j < 26; j++)
                        for (k = 0; k < 26; k++) {
                                temp[0] = (char)('a'+i);
                                temp[1] = (char)('a'+j);
                                temp[2] = (char)('a'+k);
                                temp[3] = 0;
                                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                                (PKIX_ESCASCII, temp, 3,
                                &strings[26*(i*26+j)+k], plContext));
                        }

        /* ---------------------------- */
        subTest("Create a bytearray from each string's UTF-16 encoding");
        for (i = 0; i < size; i++) {
                PKIX_TEST_EXPECT_NO_ERROR
                        (PKIX_PL_String_GetEncoded
                        (strings[i],
                        PKIX_UTF16,
                        &dest,
                        &length,
                        plContext));

                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_ByteArray_Create
                            (dest, length, &byteArrays[i], plContext));
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(dest, plContext));
        }

        /* ---------------------------- */
        subTest("Create a copy string from each bytearray");
        for (i = 0; i < size; i++) {
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_String_Create
                            (PKIX_UTF16, *(void **)byteArrays[i], 6,
                            &utf16strings[i], plContext));
        }

        /* ---------------------------- */
        subTest("Compare each original string with the copy");
        for (i = 0; i < size; i++) {
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Equals
                            ((PKIX_PL_Object*)strings[i],
                            (PKIX_PL_Object*)utf16strings[i],
                            &result,
                            plContext));
                if (result == 0)
                        testError("Strings do not match");
        }

        /* ---------------------------- */
        subTest("Put each string into a Hashtable");
        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_HashTable_Create(size/2, 0, &ht, plContext));

        for (i = 0; i < size; i++) {
                PKIX_TEST_EXPECT_NO_ERROR
                        (PKIX_PL_Object_Hashcode
                        ((PKIX_PL_Object*)strings[i],
                        &hashcode,
                        plContext));

                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Add
                            (ht,
                            (void *)&hashcode,
                            (void*)strings[i],
                            plContext));
        }


        /* ---------------------------- */
        subTest("Compare each copy string with the hashtable entries ");
        for (i = 0; i < size; i++) {
                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Hashcode
                            ((PKIX_PL_Object*)utf16strings[i],
                            &hashcode,
                            plContext));

                PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_HashTable_Lookup
                            (ht,
                            (void *)&hashcode,
                            (PKIX_PL_Object**)&tempString,
                            plContext));

                if (tempString == NULL)
                        testError("String not found in hashtable");
                else {
                        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_Equals
                                    ((PKIX_PL_Object*)tempString,
                                    (PKIX_PL_Object*)utf16strings[i],
                                    &result,
                                    plContext));
                        if (result == 0)
                                testError("Strings do not match");
                        PKIX_TEST_DECREF_BC(tempString);
                }
        }

cleanup:

        /* ---------------------------- */
        subTest("Destroy All Objects");

        PKIX_TEST_DECREF_AC(ht);

        for (i = 0; i < size; i++) {
                PKIX_TEST_DECREF_AC(strings[i]);
                PKIX_TEST_DECREF_AC(utf16strings[i]);
                PKIX_TEST_DECREF_AC(byteArrays[i]);
        }

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("Stress Test");
        return (0);
}
