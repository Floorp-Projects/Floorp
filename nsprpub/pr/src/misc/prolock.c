/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
**  prolock.c -- NSPR Ordered Lock
** 
**  Implement the API defined in prolock.h
** 
*/
#if defined(DEBUG) || defined(FORCE_NSPR_COUNTERS)
#include "prolock.h"
#include "prlog.h"
#include "prerror.h"

PR_IMPLEMENT(PROrderedLock *) 
    PR_CreateOrderedLock( 
        PRInt32 order,
        const char *name
)
{
#ifdef XP_MAC
#pragma unused( order, name )
#endif
    PR_ASSERT(!"Not implemented"); /* Not implemented yet */
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return NULL;
} /*  end PR_CreateOrderedLock() */


PR_IMPLEMENT(void) 
    PR_DestroyOrderedLock( 
        PROrderedLock *lock 
)
{
#ifdef XP_MAC
#pragma unused( lock )
#endif
    PR_ASSERT(!"Not implemented"); /* Not implemented yet */
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
} /*  end PR_DestroyOrderedLock() */


PR_IMPLEMENT(void) 
    PR_LockOrderedLock( 
        PROrderedLock *lock 
)
{
#ifdef XP_MAC
#pragma unused( lock )
#endif
    PR_ASSERT(!"Not implemented"); /* Not implemented yet */
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
} /*  end PR_LockOrderedLock() */


PR_IMPLEMENT(PRStatus) 
    PR_UnlockOrderedLock( 
        PROrderedLock *lock 
)
{
#ifdef XP_MAC
#pragma unused( lock )
#endif
    PR_ASSERT(!"Not implemented"); /* Not implemented yet */
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
} /*  end PR_UnlockOrderedLock() */

#else /* ! defined(FORCE_NSPR_ORDERED_LOCK) */
/*
** NSPR Ordered Lock is not defined when !DEBUG and !FORCE_NSPR_ORDERED_LOCK
**  
*/

/* Some compilers don't like an empty compilation unit. */
static int dummy = 0;
#endif /* defined(FORCE_NSPR_ORDERED_LOCK */
