/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nspr.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    PRAddrInfo *ai;
    void *iter;
    PRNetAddr addr;

    ai = PR_GetAddrInfoByName(argv[1], PR_AF_UNSPEC, PR_AI_ADDRCONFIG);
    if (ai == NULL) {
        fprintf(stderr, "PR_GetAddrInfoByName failed: (%d, %d)\n",
            PR_GetError(), PR_GetOSError());
        exit(1);
    }
    printf("%s\n", PR_GetCanonNameFromAddrInfo(ai));
    iter = NULL;
    while ((iter = PR_EnumerateAddrInfo(iter, ai, 0, &addr)) != NULL) {
        char buf[128];
        PR_NetAddrToString(&addr, buf, sizeof buf);
        printf("%s\n", buf);
    }
    PR_FreeAddrInfo(ai);
    return 0;
}
