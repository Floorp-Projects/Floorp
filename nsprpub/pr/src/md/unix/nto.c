/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "primpl.h"

#include <setjmp.h>

/* Fake this out */
int socketpair (int foo, int foo2, int foo3, int sv[2])
{
    printf("error in socketpair\n");
    exit (-1);
}

void _MD_EarlyInit(void)
{
}

PRWord *_MD_HomeGCRegisters(PRThread *t, int isCurrent, int *np)
{
#ifndef _PR_PTHREADS
    if (isCurrent) {
        (void) setjmp(CONTEXT(t));
    }

    *np = sizeof(CONTEXT(t)) / sizeof(PRWord);
    return (PRWord *) CONTEXT(t);
#else
    *np = 0;
    return NULL;
#endif
}
