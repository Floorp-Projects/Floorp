/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>

#include "nss.h"
#include "secerr.h"

/*
 * Regression test for bug 495097.
 *
 * NSS_InitReadWrite("sql:<dbdir>") should fail with SEC_ERROR_BAD_DATABASE
 * if the directory <dbdir> doesn't exist.
 */

int main()
{
    SECStatus status;
    int error;

    status = NSS_InitReadWrite("sql:/no/such/db/dir");
    if (status == SECSuccess) {
        fprintf(stderr, "NSS_InitReadWrite succeeded unexpectedly\n");
        exit(1);
    }
    error = PORT_GetError();
    if (error != SEC_ERROR_BAD_DATABASE) {
        fprintf(stderr, "NSS_InitReadWrite failed with the wrong error code: "
                "%d\n", error);
        exit(1);
    }
    printf("PASS\n");
    return 0;
}
