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
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
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
/*
 * secport.c - portability interfaces for security libraries
 *
 * This file abstracts out libc functionality that libsec depends on
 * 
 * NOTE - These are not public interfaces. These stubs are to allow the 
 * SW FORTEZZA to link with some low level security functions without dragging
 * in NSPR.
 *
 * $Id: stub.c,v 1.6 2003/04/20 04:23:29 wtc%netscape.com Exp $
 */

#include "seccomon.h"
#include "prmem.h"
#include "prerror.h"
#include "plarena.h"
#include "secerr.h"
#include "prmon.h"
#include "prbit.h"

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

char *
PORT_Strdup(const char *str)
{
    size_t len = PORT_Strlen(str)+1;
    char *newstr;

    newstr = (char *)PORT_Alloc(len);
    if (newstr) {
        PORT_Memcpy(newstr, str, len);
    }
    return newstr;
}

void
PORT_SetError(int value)
{	
    return;
}

int
PORT_GetError(void)
{
    return(1);
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

/*
 * Do not use NSPR stubs for MinGW because they can't resolve references
 * to the _imp__PR_XXX symbols.  This is merely an expedient hack and not
 * the right solution.
 */
#if !(defined(WIN32) && defined(__GNUC__))
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

PR_IMPLEMENT(void)
PR_Free(void *ptr) { free(ptr); }

PR_IMPLEMENT(void)
PR_SetError(PRErrorCode errorCode, PRInt32 oserr) { return; }

PR_IMPLEMENT(void)
PR_SetErrorText(PRIntn textLength, const char *text) { return; }
#endif /* ! (WIN32 && GCC) */


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

/*
 * Do not use NSPR stubs for MinGW because they can't resolve references
 * to the _imp__PR_XXX symbols.  This is merely an expedient hack and not
 * the right solution.
 */
#if !(defined(WIN32) && defined(__GNUC__))
/* now make the RNG happy */ /* This is not atomic! */
PR_IMPLEMENT(PRInt32) PR_AtomicIncrement(PRInt32 *val) { return ++(*val); }
/* This is not atomic! */
PR_IMPLEMENT(PRInt32) PR_AtomicDecrement(PRInt32 *val) { return --(*val); }

PR_IMPLEMENT(PRStatus) PR_Sleep(PRIntervalTime ticks) { return PR_SUCCESS; }

#include "nssilock.h"
#include "fmutex.h"
PR_IMPLEMENT(PRLock *)
PR_NewLock(void) {
	PRLock *lock = NULL;

	FMUTEX_Create((void **)&lock);

	/* if we don't have a lock, FMUTEX can deal with things */
	if (lock == NULL) lock=(PRLock *) 1;
	return lock;
}

PR_IMPLEMENT(void) 
PR_DestroyLock(PRLock *lock) {
	FMUTEX_Destroy(lock);
}

PR_IMPLEMENT(void) 
PR_Lock(PRLock *lock) {
	FMUTEX_Lock(lock);
}

PR_IMPLEMENT(PRStatus) 
PR_Unlock(PRLock *lock) {
	FMUTEX_Unlock(lock);
	return PR_SUCCESS;
}

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

