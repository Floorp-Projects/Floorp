/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A regression test for bug 491441.  NSPR should not crash on startup in
 * PR_SetLogFile when the NSPR_LOG_MODULES and NSPR_LOG_FILE environment
 * variables are set.
 *
 * This test could be extended to be a full-blown test for NSPR_LOG_FILE.
 */

#include "prinit.h"
#include "prlog.h"

#include <stdio.h>
#include <stdlib.h>

int main()
{
    PRLogModuleInfo *test_lm;

    if (putenv("NSPR_LOG_MODULES=all:5") != 0) {
        fprintf(stderr, "putenv failed\n");
        exit(1);
    }
    if (putenv("NSPR_LOG_FILE=logfile.log") != 0) {
        fprintf(stderr, "putenv failed\n");
        exit(1);
    }

    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    test_lm = PR_NewLogModule("test");
    PR_LOG(test_lm, PR_LOG_MIN, ("logfile: test log message"));
    PR_Cleanup();
    return 0;
}
