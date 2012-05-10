/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This test verifies that NSPR can be cleaned up and reinitialized. */

#include "nspr.h"
#include <stdio.h>

int main()
{
    PRStatus rv;

    fprintf(stderr, "Init 1\n");
    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    fprintf(stderr, "Cleanup 1\n");
    rv = PR_Cleanup();
    if (rv != PR_SUCCESS) {
        fprintf(stderr, "FAIL\n");
        return 1;
    }

    fprintf(stderr, "Init 2\n");
    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    fprintf(stderr, "Cleanup 2\n");
    rv = PR_Cleanup();
    if (rv != PR_SUCCESS) {
        fprintf(stderr, "FAIL\n");
        return 1;
    }

    fprintf(stderr, "PASS\n");
    return 0;
}
