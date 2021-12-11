/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A regression test for bug 794316 */

#include <stdio.h>
#include <stdlib.h>

#include "prio.h"

static PRIOMethods dummyMethods;

int main()
{
    PRDescIdentity topId, middleId, bottomId;
    PRFileDesc *top, *middle, *bottom;
    PRFileDesc *fd;

    topId = PR_GetUniqueIdentity("top");
    middleId = PR_GetUniqueIdentity("middle");
    bottomId = PR_GetUniqueIdentity("bottom");

    top = PR_CreateIOLayerStub(topId, &dummyMethods);
    middle = PR_CreateIOLayerStub(middleId, &dummyMethods);
    bottom = PR_CreateIOLayerStub(bottomId, &dummyMethods);

    fd = bottom;
    PR_PushIOLayer(fd, PR_TOP_IO_LAYER, middle);
    PR_PushIOLayer(fd, PR_TOP_IO_LAYER, top);

    top = fd;
    middle = top->lower;
    bottom = middle->lower;

    /* Verify that the higher pointers are correct. */
    if (middle->higher != top) {
        fprintf(stderr, "middle->higher is wrong\n");
        fprintf(stderr, "FAILED\n");
        exit(1);
    }
    if (bottom->higher != middle) {
        fprintf(stderr, "bottom->higher is wrong\n");
        fprintf(stderr, "FAILED\n");
        exit(1);
    }

    top = PR_PopIOLayer(fd, topId);
    top->dtor(top);

    middle = fd;
    bottom = middle->lower;

    /* Verify that the higher pointer is correct. */
    if (bottom->higher != middle) {
        fprintf(stderr, "bottom->higher is wrong\n");
        fprintf(stderr, "FAILED\n");
        exit(1);
    }

    middle = PR_PopIOLayer(fd, middleId);
    middle->dtor(middle);
    if (fd->identity != bottomId) {
        fprintf(stderr, "The bottom layer has the wrong identity\n");
        fprintf(stderr, "FAILED\n");
        exit(1);
    }
    fd->dtor(fd);

    printf("PASS\n");
    return 0;
}
