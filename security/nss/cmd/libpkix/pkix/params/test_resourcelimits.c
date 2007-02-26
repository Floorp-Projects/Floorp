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
 * test_resourcelimits.c
 *
 * Test ResourceLimits Type
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

void *plContext = NULL;

static void
testDestroy(void *goodObject, void *equalObject, void *diffObject)
{
        PKIX_TEST_STD_VARS();

        subTest("PKIX_ResourceLimits_Destroy");

        PKIX_TEST_DECREF_BC(goodObject);
        PKIX_TEST_DECREF_BC(equalObject);
        PKIX_TEST_DECREF_BC(diffObject);

cleanup:

        PKIX_TEST_RETURN();

}

int main(int argc, char *argv[]) {

        PKIX_ResourceLimits *goodObject = NULL;
        PKIX_ResourceLimits *equalObject = NULL;
        PKIX_ResourceLimits *diffObject = NULL;
        PKIX_UInt32 maxTime = 0;
        PKIX_UInt32 maxFanout = 0;
        PKIX_UInt32 maxDepth = 0;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;
        PKIX_Boolean useArenas = PKIX_FALSE;
        char *expectedAscii =
                "[\n"
                "\tMaxTime:           		10\n"
                "\tMaxFanout:         		5\n"
                "\tMaxDepth:         		5\n"
                "]\n";

        PKIX_TEST_STD_VARS();

        startTests("ResourceLimits");

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                                    (PKIX_TRUE, /* nssInitNeeded */
                                    useArenas,
                                    PKIX_MAJOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    &actualMinorVersion,
                                    &plContext));

        subTest("PKIX_ResourceLimits_Create");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_Create
                                  (&goodObject, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_Create
                                  (&diffObject, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_Create
                                  (&equalObject, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxTime
                                  (goodObject, 10, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_GetMaxTime
                                  (goodObject, &maxTime, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxTime
                                  (equalObject, maxTime, plContext));
        maxTime++;
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxTime
                                  (diffObject, maxTime, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxFanout
                                  (goodObject, 5, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_GetMaxFanout
                                  (goodObject, &maxFanout, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxFanout
                                  (equalObject, maxFanout, plContext));
        maxFanout++;
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxFanout
                                  (diffObject, maxFanout, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxDepth
                                  (goodObject, 5, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_GetMaxDepth
                                  (goodObject, &maxDepth, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxDepth
                                  (equalObject, maxDepth, plContext));
        maxDepth++;
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_ResourceLimits_SetMaxDepth
                                  (diffObject, maxDepth, plContext));

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (goodObject,
                equalObject,
                diffObject,
                expectedAscii,
                ResourceLimits,
                PKIX_FALSE);

        testDestroy(goodObject, equalObject, diffObject);

cleanup:

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("ResourceLimits");

        return (0);
}
