/* -*- Mode: C++; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 */

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
