/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** File:   ptmisc.c
** Descritpion:  Implemenation of miscellaneous methods for pthreads
*/

#if defined(_PR_PTHREADS)

#include "primpl.h"

#include <stdio.h>
#ifdef SOLARIS
#include <thread.h>
#endif

#define PT_LOG(f)

void _PR_InitCPUs(void) {
    PT_LOG("_PR_InitCPUs")
}
void _PR_InitStacks(void) {
    PT_LOG("_PR_InitStacks")
}

PR_IMPLEMENT(void) PR_SetConcurrency(PRUintn numCPUs)
{
#ifdef SOLARIS
    thr_setconcurrency(numCPUs);
#else
    PT_LOG("PR_SetConcurrency");
#endif
}

PR_IMPLEMENT(void) PR_SetThreadRecycleMode(PRUint32 flag)
{
    PT_LOG("PR_SetThreadRecycleMode")
}

#endif /* defined(_PR_PTHREADS) */

/* ptmisc.c */
