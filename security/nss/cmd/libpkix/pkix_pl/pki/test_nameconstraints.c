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
 * test_nameconstraints.c
 *
 * Test CERT Name Constraints Type
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

void *plContext = NULL;

static char *catDirName(char *platform, char *dir, void *plContext)
{
        char *pathName = NULL;
        PKIX_UInt32 dirLen;
        PKIX_UInt32 platformLen;

        PKIX_TEST_STD_VARS();

        dirLen = PL_strlen(dir);
        platformLen = PL_strlen(platform);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Malloc
                (platformLen + dirLen + 2, (void **)&pathName, plContext));

        PL_strcpy(pathName, platform);
        PL_strcat(pathName, "/");
        PL_strcat(pathName, dir);

cleanup:

        PKIX_TEST_RETURN();

        return (pathName);
}

static void
testNameConstraints(char *dataDir)
{
        char *goodPname = "nameConstraintsDN5CACert.crt";
        PKIX_PL_Cert *goodCert = NULL;
        PKIX_PL_CertNameConstraints *goodNC = NULL;
        char *expectedAscii =
                "[\n"
                "\t\tPermitted Name:  (OU=permittedSubtree1,"
                "O=Test Certificates,C=US)\n"
                "\t\tExcluded Name:   (OU=excludedSubtree1,"
                "OU=permittedSubtree1,O=Test Certificates,C=US)\n"
                "\t]\n";

        PKIX_TEST_STD_VARS();

        subTest("PKIX_PL_CertNameConstraints");

        goodCert = createCert(dataDir, goodPname, plContext);

        subTest("PKIX_PL_Cert_GetNameConstraints");

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Cert_GetNameConstraints
                (goodCert, &goodNC, plContext));

        testToStringHelper
                ((PKIX_PL_Object *)goodNC, expectedAscii, plContext);

cleanup:

        PKIX_TEST_DECREF_AC(goodNC);
        PKIX_TEST_DECREF_AC(goodCert);

        PKIX_TEST_RETURN();
}

void printUsage(void) {
        (void) printf
		("\nUSAGE:\ttest_nameconstraints <test-purpose>"
                 " <data-dir> <platform-prefix>\n\n");
}

/* Functional tests for CRL public functions */

int main(int argc, char *argv[]) {
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;
        char *platformDir = NULL;
        char *dataDir = NULL;
        char *combinedDir = NULL;
        PKIX_Boolean useArenas = PKIX_FALSE;

        /* Note XXX serialnumber and reasoncode need debug */

        PKIX_TEST_STD_VARS();

        startTests("NameConstraints");

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                (PKIX_TRUE, /* nssInitNeeded */
                useArenas,
                PKIX_MAJOR_VERSION,
                PKIX_MINOR_VERSION,
                PKIX_MINOR_VERSION,
                &actualMinorVersion,
                &plContext));

        if (argc < 3 + j) {
                printUsage();
                return (0);
        }

        dataDir = argv[2 + j];
        platformDir = argv[3 + j];
	combinedDir = catDirName(platformDir, dataDir, plContext);

        testNameConstraints(combinedDir);

cleanup:

        pkixTestErrorResult = PKIX_PL_Free(combinedDir, plContext);

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("NameConstraints");

        return (0);
}
