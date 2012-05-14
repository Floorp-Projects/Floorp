/* -*- Mode: C++; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "primpl.h"
#include <stdio.h>

// void _PR_InitCPUs(void) {PT_LOG("_PR_InitCPUs")}
// void _MD_StartInterrupts(void) {PT_LOG("_MD_StartInterrupts")}

/* this is a total hack.. */

struct protoent* getprotobyname(const char* name)
{
    return 0;
}

struct protoent* getprotobynumber(int number)
{
    return 0;
}

/* this is needed by prinit for some reason */
void
_PR_InitStacks (void)
{
}

/* this is needed by prinit for some reason */
void
_PR_InitTPD (void)
{
}

/*
** Create extra virtual processor threads. Generally used with MP systems.
*/
PR_IMPLEMENT(void)
    PR_SetConcurrency (PRUintn numCPUs)
{
}

/*
** Set thread recycle mode to on (1) or off (0)
*/
PR_IMPLEMENT(void)
    PR_SetThreadRecycleMode (PRUint32 flag)
{
}

/*
** Get context registers, return with error for now.
*/

PR_IMPLEMENT(PRWord *)
_MD_HomeGCRegisters( PRThread *t, int isCurrent, int *np )
{
     return 0;
}

PR_IMPLEMENT(void *)
PR_GetSP( PRThread *t )
{
    return 0;
}

PR_IMPLEMENT(PRStatus)
PR_EnumerateThreads( PREnumerator func, void *arg )
{
    return PR_FAILURE;
}
