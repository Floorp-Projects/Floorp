/* -*- Mode: C++; c-basic-offset: 4 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
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
