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
 * test_generalname.c
 *
 * Test GeneralName Type
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

void *plContext = NULL;

static void
createGeneralNames(PKIX_UInt32 nameType, char *goodInput, char *diffInput,
                    PKIX_PL_GeneralName **goodName,
                    PKIX_PL_GeneralName **equalName,
                    PKIX_PL_GeneralName **diffName){

        subTest("PKIX_PL_GeneralName_Create <goodName>");
        *goodName = createGeneralName(nameType, goodInput, plContext);

        subTest("PKIX_PL_GeneralName_Create <equalName>");
        *equalName = createGeneralName(nameType, goodInput, plContext);

        subTest("PKIX_PL_GeneralName_Create <diffName>");
        *diffName = createGeneralName(nameType, diffInput, plContext);

}

static void
testDestroy(void *goodObject, void *equalObject, void *diffObject)
{
        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_GeneralName_Destroy");

        PKIX_TEST_DECREF_BC(goodObject);
        PKIX_TEST_DECREF_BC(equalObject);
        PKIX_TEST_DECREF_BC(diffObject);

cleanup:

        PKIX_TEST_RETURN();

}

void testNameType
(PKIX_UInt32 nameType, char *goodInput, char *diffInput, char *expectedAscii){

        PKIX_PL_GeneralName *goodName = NULL;
        PKIX_PL_GeneralName *equalName = NULL;
        PKIX_PL_GeneralName *diffName = NULL;

        createGeneralNames(nameType, goodInput, diffInput,
                            &goodName, &equalName, &diffName);

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (goodName,
                equalName,
                diffName,
                expectedAscii,
                GeneralName,
                PKIX_TRUE);

        testDestroy(goodName, equalName, diffName);
}

int main(int argc, char *argv[]) {

        char *goodInput = NULL;
        char *diffInput = NULL;
        PKIX_Boolean useArenas = PKIX_FALSE;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;

        PKIX_TEST_STD_VARS();

        startTests("GeneralName");

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                                    (PKIX_TRUE, /* nssInitNeeded */
                                    useArenas,
                                    PKIX_MAJOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    &actualMinorVersion,
                                    &plContext));

        goodInput = "john@sun.com";
        diffInput = "john@labs.com";
        testNameType(PKIX_RFC822_NAME, goodInput, diffInput, goodInput);

        goodInput = "example1.com";
        diffInput = "ex2.net";
        testNameType(PKIX_DNS_NAME, goodInput, diffInput, goodInput);

        goodInput = "cn=yassir, ou=labs, o=sun, c=us";
        diffInput = "cn=alice, ou=labs, o=sun, c=us";
        testNameType(PKIX_DIRECTORY_NAME,
                    goodInput,
                    diffInput,
                    "CN=yassir,OU=labs,O=sun,C=us");

        goodInput = "http://example1.com";
        diffInput = "http://ex2.net";
        testNameType(PKIX_URI_NAME, goodInput, diffInput, goodInput);

        goodInput = "1.2.840.11";
        diffInput = "1.2.840.115349";
        testNameType(PKIX_OID_NAME, goodInput, diffInput, goodInput);

        /*
         * We don't support creating PKIX_EDIPARTY_NAME,
         * PKIX_IP_NAME, OTHER_NAME, X400_ADDRESS from strings
         */

cleanup:

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("GeneralName");

        return (0);
}
