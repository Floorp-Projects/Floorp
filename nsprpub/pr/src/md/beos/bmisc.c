/* -*- Mode: C++; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http:// www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 */

#include "primpl.h"

#include <stdlib.h>

PRLock *_connectLock = NULL;
PRUint32 connectCount = 0;
ConnectListNode connectList[64];
                                   
void
_MD_cleanup_before_exit (void)
{
}

void
_MD_exit (PRIntn status)
{
    exit(status);
}

void
_MD_early_init (void)
{
}

static PRLock *monitor = NULL;

void
_MD_final_init (void)
{
    _connectLock = PR_NewLock();
    PR_ASSERT(NULL != _connectLock); 
    connectCount = 0;
}

void
_MD_AtomicInit (void)
{
    if (monitor == NULL) {
        monitor = PR_NewLock();
    }
}

/*
** This is exceedingly messy.  atomic_add returns the last value, NSPR expects the new value.
** We just add or subtract 1 from the result.  The actual memory update is atomic.
*/

PRInt32
_MD_AtomicAdd( PRInt32 *ptr, PRInt32 val )
{
    return( ( atomic_add( (long *)ptr, val ) ) + val );
}

PRInt32
_MD_AtomicIncrement( PRInt32 *val )
{
    return( ( atomic_add( (long *)val, 1 ) ) + 1 );
}

PRInt32
_MD_AtomicDecrement( PRInt32 *val )
{
    return( ( atomic_add( (long *)val, -1 ) ) - 1 );
}

PRInt32
_MD_AtomicSet( PRInt32 *val, PRInt32 newval )
{
    PRInt32 rv;

    if (!_pr_initialized) {
        _PR_ImplicitInitialization();
    }
    PR_Lock(monitor);
    rv = *val;
    *val = newval;
    PR_Unlock(monitor);
    return rv;
}
