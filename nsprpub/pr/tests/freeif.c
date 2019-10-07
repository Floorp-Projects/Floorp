/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A test to see if the macros PR_DELETE and PR_FREEIF are
 * properly defined.  (See Bugzilla bug #39110.)
 */

#include "nspr.h"

#include <stdio.h>
#include <stdlib.h>

static void Noop(void) { }

static void Fail(void)
{
    printf("FAIL\n");
    exit(1);
}

int main(int argc, char **argv)
{
    int foo = 1;
    char *ptr = NULL;

    /* this fails to compile with the old definition of PR_DELETE */
    if (foo) {
        PR_DELETE(ptr);
    }
    else {
        Noop();
    }

    /* this nests incorrectly with the old definition of PR_FREEIF */
    if (foo) {
        PR_FREEIF(ptr);
    }
    else {
        Fail();
    }

    printf("PASS\n");
    return 0;
}
