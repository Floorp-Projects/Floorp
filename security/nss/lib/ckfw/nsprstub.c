/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
/*
 * secport.c - portability interfaces for security libraries
 *
 * This file abstracts out libc functionality that libsec depends on
 * 
 * NOTE - These are not public interfaces. These stubs are to allow the 
 * SW FORTEZZA to link with some low level security functions without dragging
 * in NSPR.
 *
 * $Id: nsprstub.c,v 1.6 2004/07/29 22:51:00 jpierre%netscape.com Exp $
 */

#include "seccomon.h"
#include "prmem.h"
#include "prerror.h"
#include "plarena.h"
#include "secerr.h"
#include "prmon.h"
#include "prbit.h"
#include "ck.h"

#ifdef notdef
unsigned long port_allocFailures;

/* locations for registering Unicode conversion functions.  
 *  Is this the appropriate location?  or should they be
 *     moved to client/server specific locations?
 */
PORTCharConversionFunc ucs4Utf8ConvertFunc;
PORTCharConversionFunc ucs2Utf8ConvertFunc;
PORTCharConversionWSwapFunc  ucs2AsciiConvertFunc;

void *
PORT_Alloc(size_t bytes)
{
    void *rv;

    /* Always allocate a non-zero amount of bytes */
    rv = (void *)malloc(bytes ? bytes : 1);
    if (!rv) {
	++port_allocFailures;
    }
    return rv;
}

void *
PORT_Realloc(void *oldptr, size_t bytes)
{
    void *rv;

    rv = (void *)realloc(oldptr, bytes);
    if (!rv) {
	++port_allocFailures;
    }
    return rv;
}

void *
PORT_ZAlloc(size_t bytes)
{
    void *rv;

    /* Always allocate a non-zero amount of bytes */
    rv = (void *)calloc(1, bytes ? bytes : 1);
    if (!rv) {
	++port_allocFailures;
    }
    return rv;
}

void
PORT_Free(void *ptr)
{
    if (ptr) {
	free(ptr);
    }
}

void
PORT_ZFree(void *ptr, size_t len)
{
    if (ptr) {
	memset(ptr, 0, len);
	free(ptr);
    }
}

/********************* Arena code follows *****************************/


PLArenaPool *
PORT_NewArena(unsigned long chunksize)
{
    PLArenaPool *arena;
    
    arena = (PLArenaPool*)PORT_ZAlloc(sizeof(PLArenaPool));
    if ( arena != NULL ) {
	PR_InitArenaPool(arena, "security", chunksize, sizeof(double));
    }
    return(arena);
}

void *
PORT_ArenaAlloc(PLArenaPool *arena, size_t size)
{
    void *p;

    PL_ARENA_ALLOCATE(p, arena, size);
    if (p == NULL) {
	++port_allocFailures;
    }

    return(p);
}

void *
PORT_ArenaZAlloc(PLArenaPool *arena, size_t size)
{
    void *p;

    PL_ARENA_ALLOCATE(p, arena, size);
    if (p == NULL) {
	++port_allocFailures;
    } else {
	PORT_Memset(p, 0, size);
    }

    return(p);
}

/* need to zeroize!! */
void
PORT_FreeArena(PLArenaPool *arena, PRBool zero)
{
    PR_FinishArenaPool(arena);
    PORT_Free(arena);
}

void *
PORT_ArenaGrow(PLArenaPool *arena, void *ptr, size_t oldsize, size_t newsize)
{
    PORT_Assert(newsize >= oldsize);
    
    PL_ARENA_GROW(ptr, arena, oldsize, ( newsize - oldsize ) );
    
    return(ptr);
}

void *
PORT_ArenaMark(PLArenaPool *arena)
{
    void * result;

    result = PL_ARENA_MARK(arena);
    return result;
}

void
PORT_ArenaRelease(PLArenaPool *arena, void *mark)
{
    PL_ARENA_RELEASE(arena, mark);
}

void
PORT_ArenaUnmark(PLArenaPool *arena, void *mark)
{
    /* do nothing */
}

char *
PORT_ArenaStrdup(PLArenaPool *arena,const char *str) {
    int len = PORT_Strlen(str)+1;
    char *newstr;

    newstr = (char*)PORT_ArenaAlloc(arena,len);
    if (newstr) {
        PORT_Memcpy(newstr,str,len);
    }
    return newstr;
}
#endif

/*
 * replace the nice thread-safe Error stack code with something
 * that will work without all the NSPR features.
 */
static PRInt32 stack[2] = {0, 0};

PR_IMPLEMENT(void)
nss_SetError(PRUint32 value)
{	
    stack[0] = value;
    return;
}

PR_IMPLEMENT(PRInt32)
NSS_GetError(void)
{
    return(stack[0]);
}


PR_IMPLEMENT(PRInt32 *)
NSS_GetErrorStack(void)
{
    return(&stack[0]);
}

PR_IMPLEMENT(void)
nss_ClearErrorStack(void)
{
    stack[0] = 0;
    return;
}

#ifdef DEBUG
/*
 * replace the pointer tracking stuff for the same reasons.
 *  If you want to turn pointer tracking on, simply ifdef out this code and 
 *  link with real NSPR.
 */
PR_IMPLEMENT(PRStatus)
nssPointerTracker_initialize(nssPointerTracker *tracker)
{
    return PR_SUCCESS;
}


PR_IMPLEMENT(PRStatus)
nssPointerTracker_finalize(nssPointerTracker *tracker)
{
    return PR_SUCCESS;
}

PR_IMPLEMENT(PRStatus)
nssPointerTracker_add(nssPointerTracker *tracker, const void *pointer)
{
     return PR_SUCCESS;
}

PR_IMPLEMENT(PRStatus)
nssPointerTracker_remove(nssPointerTracker *tracker, const void *pointer)
{
     return PR_SUCCESS;
}

PR_IMPLEMENT(PRStatus)
nssPointerTracker_verify(nssPointerTracker *tracker, const void *pointer)
{
     return PR_SUCCESS;
}
#endif

/*
 * Do not use NSPR stubs for MinGW because they can't resolve references
 * to the _imp__PR_XXX symbols.  This is merely an expedient hack and not
 * the right solution.
 */
#if !(defined(WIN32) && defined(__GNUC__))
PR_IMPLEMENT(PRThread *)
PR_GetCurrentThread(void)
{
     return (PRThread *)1;
}



PR_IMPLEMENT(void)
PR_Assert(const char *expr, const char *file, int line) {
    return; 
}

PR_IMPLEMENT(void *)
PR_Alloc(PRUint32 bytes) { return malloc(bytes); }

PR_IMPLEMENT(void *)
PR_Malloc(PRUint32 bytes) { return malloc(bytes); }

PR_IMPLEMENT(void *)
PR_Calloc(PRUint32 blocks, PRUint32 bytes) { return calloc(blocks,bytes); }

PR_IMPLEMENT(void *)
PR_Realloc(void * blocks, PRUint32 bytes) { return realloc(blocks,bytes); }

PR_IMPLEMENT(void)
PR_Free(void *ptr) { free(ptr); }

#ifdef notdef
/* Old template; want to expunge it eventually. */
#include "secasn1.h"
#include "secoid.h"

const SEC_ASN1Template SECOID_AlgorithmIDTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SECAlgorithmID) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(SECAlgorithmID,algorithm), },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_ANY,
	  offsetof(SECAlgorithmID,parameters), },
    { 0, }
};

PR_IMPLEMENT(PRStatus) PR_Sleep(PRIntervalTime ticks) { return PR_SUCCESS; }

/* This is not atomic! */
PR_IMPLEMENT(PRInt32) PR_AtomicDecrement(PRInt32 *val) { return --(*val); }

PR_IMPLEMENT(PRInt32) PR_AtomicSet(PRInt32 *val) { return ++(*val); }

#endif

/* now make the RNG happy */ /* This is not atomic! */
PR_IMPLEMENT(PRInt32) PR_AtomicIncrement(PRInt32 *val) { return ++(*val); }
#endif /* ! (WIN32 && GCC) */

static CK_C_INITIALIZE_ARGS_PTR nssstub_pInitArgs = NULL;
static CK_C_INITIALIZE_ARGS nssstub_initArgs;
static NSSArena *nssstub_arena = NULL;
static CryptokiLockingState nssstub_LockingState = SingleThreaded;

PR_IMPLEMENT(CK_RV)
nssSetLockArgs(CK_C_INITIALIZE_ARGS_PTR pInitArgs, CryptokiLockingState* returned)
{
    CK_ULONG count = (CK_ULONG)0;
    CK_BBOOL os_ok = CK_FALSE;
    CK_RV rv = CKR_OK;
    if (nssstub_pInitArgs == NULL) {
	if (pInitArgs != NULL) {
	    nssstub_initArgs = *pInitArgs;
	    nssstub_pInitArgs = &nssstub_initArgs;
            if( (CK_CREATEMUTEX )NULL != pInitArgs->CreateMutex  ) count++;
            if( (CK_DESTROYMUTEX)NULL != pInitArgs->DestroyMutex ) count++;
            if( (CK_LOCKMUTEX   )NULL != pInitArgs->LockMutex    ) count++;
            if( (CK_UNLOCKMUTEX )NULL != pInitArgs->UnlockMutex  ) count++;
            os_ok = (pInitArgs->flags & CKF_OS_LOCKING_OK) ? CK_TRUE : CK_FALSE;

            if( (0 != count) && (4 != count) ) {
                rv = CKR_ARGUMENTS_BAD;
                goto loser;
            }
	} else {
	    nssstub_pInitArgs = pInitArgs;
	}
	/* nssstub_arena = NSSArena_Create(); */
    }

    if( (0 == count) && (CK_TRUE == os_ok) ) {
      /*
       * This is case #2 in the description of C_Initialize:
       * The library will be called in a multithreaded way, but
       * no routines were specified: os locking calls should be
       * used.  Unfortunately, this can be hard.. like, I think
       * I may have to dynamically look up the entry points in
       * the instance of NSPR already going in the application.
       *
       * I know that *we* always specify routines, so this only
       * comes up if someone is using NSS to create their own
       * PCKS#11 modules for other products.  Oh, heck, I'll 
       * worry about this then.
       */
      rv = CKR_CANT_LOCK;
      goto loser;
    }

    if( 0 == count ) {
      /*
       * With the above test out of the way, we know this is case
       * #1 in the description of C_Initialize: this library will
       * not be called in a multithreaded way.
       */

      nssstub_LockingState = SingleThreaded;
    } else {
      /*
       * We know that we're in either case #3 or #4 in the description
       * of C_Initialize.  Case #3 says we should use the specified
       * functions, case #4 cays we can use either the specified ones
       * or the OS ones.  I'll use the specified ones.
       */
      nssstub_LockingState = MultiThreaded;
    }

    loser:
    *returned = nssstub_LockingState;
    return rv;
}

/*
 * Do not use NSPR stubs for MinGW because they can't resolve references
 * to the _imp__PR_XXX symbols.  This is merely an expedient hack and not
 * the right solution.
 */
#if !(defined(WIN32) && defined(__GNUC__))
#include "prlock.h"
PR_IMPLEMENT(PRLock *)
PR_NewLock(void) {
	PRLock *lock = NULL;
	NSSCKFWMutex *mlock = NULL;
	CK_RV error;

	mlock = nssCKFWMutex_Create(nssstub_pInitArgs,nssstub_LockingState,nssstub_arena,&error);
	lock = (PRLock *)mlock;

	/* if we don't have a lock, nssCKFWMutex can deal with things */
	if (lock == NULL) lock=(PRLock *) 1;
	return lock;
}

PR_IMPLEMENT(void) 
PR_DestroyLock(PRLock *lock) {
	NSSCKFWMutex *mlock = (NSSCKFWMutex *)lock;
	if (lock == (PRLock *)1) return;
	nssCKFWMutex_Destroy(mlock);
}

PR_IMPLEMENT(void) 
PR_Lock(PRLock *lock) {
	NSSCKFWMutex *mlock = (NSSCKFWMutex *)lock;
	if (lock == (PRLock *)1) return;
	nssCKFWMutex_Lock(mlock);
}

PR_IMPLEMENT(PRStatus) 
PR_Unlock(PRLock *lock) {
	NSSCKFWMutex *mlock = (NSSCKFWMutex *)lock;
	if (lock == (PRLock *)1) return PR_SUCCESS;
	nssCKFWMutex_Unlock(mlock);
	return PR_SUCCESS;
}

#ifdef notdef
#endif
/* this implementation is here to satisfy the PRMonitor use in plarena.c.
** It appears that it doesn't need re-entrant locks.  It could have used
** PRLock instead of PRMonitor.  So, this implementation just uses 
** PRLock for a PRMonitor.
*/
PR_IMPLEMENT(PRMonitor*) 
PR_NewMonitor(void)
{
    return (PRMonitor *) PR_NewLock();
}


PR_IMPLEMENT(void) 
PR_EnterMonitor(PRMonitor *mon)
{
    PR_Lock( (PRLock *)mon );
}

PR_IMPLEMENT(PRStatus) 
PR_ExitMonitor(PRMonitor *mon)
{
    return PR_Unlock( (PRLock *)mon );
}

#include "prinit.h"

/* This is NOT threadsafe.  It is merely a pseudo-functional stub.
*/
PR_IMPLEMENT(PRStatus) PR_CallOnce(
    PRCallOnceType *once,
    PRCallOnceFN    func)
{
    /* This is not really atomic! */
    if (1 == PR_AtomicIncrement(&once->initialized)) {
	once->status = (*func)();
    }  else {
    	/* Should wait to be sure that func has finished before returning. */
    }
    return once->status;
}

/*
** Compute the log of the least power of 2 greater than or equal to n
*/
PRIntn PR_CeilingLog2(PRUint32 i) {
	PRIntn log2;
	PR_CEILING_LOG2(log2,i);
	return log2;
}
#endif /* ! (WIN32 && GCC) */

/********************** end of arena functions ***********************/

