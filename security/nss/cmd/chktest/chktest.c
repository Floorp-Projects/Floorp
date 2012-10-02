/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>

#include "blapi.h"
#include "secutil.h"

static int Usage()
{
    fprintf(stderr, "Usage:  chktest <full-path-to-shared-library>\n");
    fprintf(stderr, "        Will test for valid chk file.\n");
    fprintf(stderr, "        Will print SUCCESS or FAILURE.\n");
    exit(1);
}

int main(int argc, char **argv)
{
    SECStatus rv = SECFailure;
    PRBool good_result = PR_FALSE;

    if (argc != 2)
      return Usage();
    
    rv = RNG_RNGInit();
    if (rv != SECSuccess) {
        SECU_PrintPRandOSError("");
        return -1;
    }
    rv = BL_Init();
    if (rv != SECSuccess) {
        SECU_PrintPRandOSError("");
        return -1;
    }
    RNG_SystemInfoForRNG();

    good_result = BLAPI_SHVerifyFile(argv[1]);
    printf("%s\n", 
      (good_result ? "SUCCESS" : "FAILURE"));
    return (good_result) ? SECSuccess : SECFailure;
}
