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
** Thread safe versions of malloc, free, realloc, calloc and cfree.
*/

#include "primpl.h"

/*
** The PR_Malloc, PR_Calloc, PR_Realloc, and PR_Free functions simply
** call their libc equivalents now.  This may seem redundant, but it
** ensures that we are calling into the same runtime library.  On
** Win32, it is possible to have multiple runtime libraries (e.g.,
** objects compiled with /MD and /MDd) in the same process, and
** they maintain separate heaps, which cannot be mixed.
*/
PR_IMPLEMENT(void *) PR_Malloc(PRUint32 size)
{
#if defined (WIN16)
    return PR_MD_malloc( (size_t) size);
#else
    return malloc(size);
#endif
}

PR_IMPLEMENT(void *) PR_Calloc(PRUint32 nelem, PRUint32 elsize)
{
#if defined (WIN16)
    return PR_MD_calloc( (size_t)nelem, (size_t)elsize );
    
#else
    return calloc(nelem, elsize);
#endif
}

PR_IMPLEMENT(void *) PR_Realloc(void *ptr, PRUint32 size)
{
#if defined (WIN16)
    return PR_MD_realloc( ptr, (size_t) size);
#else
    return realloc(ptr, size);
#endif
}

PR_IMPLEMENT(void) PR_Free(void *ptr)
{
#if defined (WIN16)
    PR_MD_free( ptr );
#else
    free(ptr);
#endif
}

/*
** Complexity alert!
**
** If malloc/calloc/free (etc.) were implemented to use pr lock's then
** the entry points could block when called if some other thread had the
** lock.
**
** Most of the time this isn't a problem. However, in the case that we
** are using the thread safe malloc code after PR_Init but before
** PR_AttachThread has been called (on a native thread that nspr has yet
** to be told about) we could get royally screwed if the lock was busy
** and we tried to context switch the thread away. In this scenario
** 	PR_CURRENT_THREAD() == NULL
**
** To avoid this unfortunate case, we use the low level locking
** facilities for malloc protection instead of the slightly higher level
** locking. This makes malloc somewhat faster so maybe it's a good thing
** anyway.
*/
#ifdef _PR_OVERRIDE_MALLOC

/* Imports */
extern void *_PR_UnlockedMalloc(size_t size);
extern void *_PR_UnlockedMemalign(size_t alignment, size_t size);
extern void _PR_UnlockedFree(void *ptr);
extern void *_PR_UnlockedRealloc(void *ptr, size_t size);
extern void *_PR_UnlockedCalloc(size_t n, size_t elsize);

static PRBool _PR_malloc_initialised = PR_FALSE;

#ifdef _PR_PTHREADS
static pthread_mutex_t _PR_MD_malloc_crustylock;

#define _PR_Lock_Malloc() {						\
    				if(PR_TRUE == _PR_malloc_initialised) { \
					PRStatus rv;			\
					rv = pthread_mutex_lock(&_PR_MD_malloc_crustylock); \
					PR_ASSERT(0 == rv);		\
				}

#define _PR_Unlock_Malloc() 	if(PR_TRUE == _PR_malloc_initialised) { \
					PRStatus rv;			\
					rv = pthread_mutex_unlock(&_PR_MD_malloc_crustylock); \
					PR_ASSERT(0 == rv);		\
				}					\
			  }
#else /* _PR_PTHREADS */
static _MDLock _PR_MD_malloc_crustylock;

#ifdef IRIX
#define _PR_Lock_Malloc() {						\
			   PRIntn _is;					\
    				if(PR_TRUE == _PR_malloc_initialised) { \
				if (_PR_MD_GET_ATTACHED_THREAD() && 		\
					!_PR_IS_NATIVE_THREAD( 		\
					_PR_MD_GET_ATTACHED_THREAD()))	\
						_PR_INTSOFF(_is); 	\
					_PR_MD_LOCK(&_PR_MD_malloc_crustylock); \
				}

#define _PR_Unlock_Malloc() 	if(PR_TRUE == _PR_malloc_initialised) { \
					_PR_MD_UNLOCK(&_PR_MD_malloc_crustylock); \
				if (_PR_MD_GET_ATTACHED_THREAD() && 		\
					!_PR_IS_NATIVE_THREAD( 		\
					_PR_MD_GET_ATTACHED_THREAD()))	\
						_PR_INTSON(_is);	\
				}					\
			  }
#else	/* IRIX */
#define _PR_Lock_Malloc() {						\
			   PRIntn _is;					\
    				if(PR_TRUE == _PR_malloc_initialised) { \
				if (_PR_MD_CURRENT_THREAD() && 		\
					!_PR_IS_NATIVE_THREAD( 		\
					_PR_MD_CURRENT_THREAD()))	\
						_PR_INTSOFF(_is); 	\
					_PR_MD_LOCK(&_PR_MD_malloc_crustylock); \
				}

#define _PR_Unlock_Malloc() 	if(PR_TRUE == _PR_malloc_initialised) { \
					_PR_MD_UNLOCK(&_PR_MD_malloc_crustylock); \
				if (_PR_MD_CURRENT_THREAD() && 		\
					!_PR_IS_NATIVE_THREAD( 		\
					_PR_MD_CURRENT_THREAD()))	\
						_PR_INTSON(_is);	\
				}					\
			  }
#endif	/* IRIX	*/
#endif /* _PR_PTHREADS */

PR_IMPLEMENT(PRStatus) _PR_MallocInit(void)
{
    PRStatus rv = PR_SUCCESS;

    if( PR_TRUE == _PR_malloc_initialised ) return PR_SUCCESS;

#ifdef _PR_PTHREADS
    {
	int status;
	pthread_mutexattr_t mattr;

	status = PTHREAD_MUTEXATTR_INIT(&mattr);
	PR_ASSERT(0 == status);
	status = PTHREAD_MUTEX_INIT(_PR_MD_malloc_crustylock, mattr);
	PR_ASSERT(0 == status);
	status = PTHREAD_MUTEXATTR_DESTROY(&mattr);
	PR_ASSERT(0 == status);
    }
#else /* _PR_PTHREADS */
    _MD_NEW_LOCK(&_PR_MD_malloc_crustylock);
#endif /* _PR_PTHREADS */

    if( PR_SUCCESS == rv )
    {
        _PR_malloc_initialised = PR_TRUE;
    }

    return rv;
}

void *malloc(size_t size)
{
    void *p;
    _PR_Lock_Malloc();
    p = _PR_UnlockedMalloc(size);
    _PR_Unlock_Malloc();
    return p;
}

#if defined(IRIX)
void *memalign(size_t alignment, size_t size)
{
    void *p;
    _PR_Lock_Malloc();
    p = _PR_UnlockedMemalign(alignment, size);
    _PR_Unlock_Malloc();
    return p;
}

void *valloc(size_t size)
{
    return(memalign(sysconf(_SC_PAGESIZE),size));
}
#endif	/* IRIX */

void free(void *ptr)
{
    _PR_Lock_Malloc();
    _PR_UnlockedFree(ptr);
    _PR_Unlock_Malloc();
}

void *realloc(void *ptr, size_t size)
{
    void *p;
    _PR_Lock_Malloc();
    p = _PR_UnlockedRealloc(ptr, size);
    _PR_Unlock_Malloc();
    return p;
}

void *calloc(size_t n, size_t elsize)
{
    void *p;
    _PR_Lock_Malloc();
    p = _PR_UnlockedCalloc(n, elsize);
    _PR_Unlock_Malloc();
    return p;
}

void cfree(void *p)
{
    _PR_Lock_Malloc();
    _PR_UnlockedFree(p);
    _PR_Unlock_Malloc();
}

void _PR_InitMem(void)
{
    PRStatus rv;
    rv = _PR_MallocInit();
    PR_ASSERT(PR_SUCCESS == rv);
}

#endif /* _PR_OVERRIDE_MALLOC */
