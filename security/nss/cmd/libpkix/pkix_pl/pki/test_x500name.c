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
 * test_x500name.c
 *
 * Test X500Name Type
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

void *plContext = NULL;

static PKIX_PL_X500Name *
createX500Name(char *asciiName, PKIX_Boolean expectedToPass){

        PKIX_PL_X500Name *x500Name = NULL;
        PKIX_PL_String *plString = NULL;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR
                (PKIX_PL_String_Create
                (PKIX_ESCASCII, asciiName, 0, &plString, plContext));

        if (expectedToPass){
                PKIX_TEST_EXPECT_NO_ERROR
                        (PKIX_PL_X500Name_Create
                        (plString, &x500Name, plContext));
        } else {
                PKIX_TEST_EXPECT_ERROR
                        (PKIX_PL_X500Name_Create
                        (plString, &x500Name, plContext));
        }

cleanup:

        PKIX_TEST_DECREF_AC(plString);

        PKIX_TEST_RETURN();

        return (x500Name);
}

static void
createX500Names(char *goodInput, char *diffInput, char *diffInputMatch,
            PKIX_PL_X500Name **goodObject,
            PKIX_PL_X500Name **equalObject,
            PKIX_PL_X500Name **diffObject,
            PKIX_PL_X500Name **diffObjectMatch)
{
        char *badAscii = "cn=yas#sir,ou=labs,o=sun,c=us";
        PKIX_PL_X500Name *badObject = NULL;

        subTest("PKIX_PL_X500Name_Create <goodObject>");
        *goodObject = createX500Name(goodInput, PKIX_TRUE);

        subTest("PKIX_PL_X500Name_Create <equalObject>");
        *equalObject = createX500Name(goodInput, PKIX_TRUE);

        subTest("PKIX_PL_X500Name_Create <diffObject>");
        *diffObject = createX500Name(diffInput, PKIX_TRUE);

        subTest("PKIX_PL_X500Name_Create <diffObjectMatch>");
        *diffObjectMatch = createX500Name(diffInputMatch, PKIX_TRUE);

        subTest("PKIX_PL_X500Name_Create <negative>");
        badObject = createX500Name(badAscii, PKIX_FALSE);
}

static void testMatchHelper
(PKIX_PL_X500Name *goodName, PKIX_PL_X500Name *otherName, PKIX_Boolean match)
{
        PKIX_Boolean cmpResult;

        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_X500Name_Match
                                    (goodName,
                                    otherName,
                                    &cmpResult,
                                    plContext));

        if ((match && !cmpResult) || (!match && cmpResult)){
                testError("unexpected mismatch");
                (void) printf("Actual value:\t%d\n", cmpResult);
                (void) printf("Expected value:\t%d\n", match);
        }

cleanup:

        PKIX_TEST_RETURN();

}

static void
testMatch(void *goodObject, void *diffObject, void *diffObjectMatch)
{
        subTest("PKIX_PL_X500Name_Match <match>");
        testMatchHelper((PKIX_PL_X500Name *)diffObject,
                        (PKIX_PL_X500Name *)diffObjectMatch,
                        PKIX_TRUE);

        subTest("PKIX_PL_X500Name_Match <non-match>");
        testMatchHelper((PKIX_PL_X500Name *)goodObject,
                        (PKIX_PL_X500Name *)diffObject,
                        PKIX_FALSE);
}

static void testDestroy
(void *goodObject, void *equalObject, void *diffObject, void *diffObjectMatch)
{
        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_X500Name_Destroy");

        PKIX_TEST_DECREF_BC(goodObject);
        PKIX_TEST_DECREF_BC(equalObject);
        PKIX_TEST_DECREF_BC(diffObject);
        PKIX_TEST_DECREF_BC(diffObjectMatch);

cleanup:

        PKIX_TEST_RETURN();

}

int main(int argc, char *argv[]) {

        PKIX_PL_X500Name *goodObject = NULL;
        PKIX_PL_X500Name *equalObject = NULL;
        PKIX_PL_X500Name *diffObject = NULL;
        PKIX_PL_X500Name *diffObjectMatch = NULL;
        PKIX_Boolean useArenas = PKIX_FALSE;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;

        /* goodInput is encoded in PKIX_ESCASCII */
        char *goodInput = "cn=Strau&#x00Df;,ou=labs,o=sun,c=us";
        char *diffInput = "cn=steve,ou=labs,o=sun,c=us";
        char *diffInputMatch = "Cn=SteVe,Ou=lABs,o=SUn,c=uS";
        char *expectedAscii = "CN=Strau&#x00DF;,OU=labs,O=sun,C=us";

        PKIX_TEST_STD_VARS();

        startTests("X500Name");

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                                    (PKIX_TRUE, /* nssInitNeeded */
                                    useArenas,
                                    PKIX_MAJOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    &actualMinorVersion,
                                    &plContext));

        createX500Names
                (goodInput, diffInput, diffInputMatch,
                &goodObject, &equalObject, &diffObject, &diffObjectMatch);

        PKIX_TEST_EQ_HASH_TOSTR_DUP
            (goodObject,
            equalObject,
            diffObject,
            expectedAscii,
            X500Name,
            PKIX_TRUE);

        testMatch(goodObject, diffObject, diffObjectMatch);

        testDestroy(goodObject, equalObject, diffObject, diffObjectMatch);

cleanup:

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("X500Name");

        return (0);
}
