/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
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
**     PR Atomic operations
*/


#include "pratom.h"
#include "primpl.h"

/*
 * The following is a fallback implementation that emulates
 * emulates atomic operations for platforms without atomic
 * operations.  If a platform has atomic operations,
 * it should define the macro _PR_HAVE_ATOMIC_OPS, and
 * the following will not be compiled in.
 */

#ifndef _PR_HAVE_ATOMIC_OPS

/*
 * We use a single lock for all the emulated atomic operations.
 * The lock contention should be acceptable.
 */

#if defined(_PR_PTHREADS) && !defined(_PR_DCETHREADS)
/*
 * PR_AtomicDecrement() is used in NSPR's thread-specific data
 * destructor.  Because thread-specific data destructors may be
 * invoked after a PR_Cleanup() call, we need an implementation
 * of the atomic routines that doesn't need NSPR to be initialized.
 */
static pthread_mutex_t atomic_lock = PTHREAD_MUTEX_INITIALIZER;
void _PR_MD_INIT_ATOMIC()
{
}

PRInt32
_PR_MD_ATOMIC_INCREMENT(PRInt32 *val)
{
    PRInt32 rv;

    pthread_mutex_lock(&atomic_lock);
    rv = ++(*val);
    pthread_mutex_unlock(&atomic_lock);
    return rv;
}

PRInt32
_PR_MD_ATOMIC_ADD(PRInt32 *ptr, PRInt32 val)
{
    PRInt32 rv;

    pthread_mutex_lock(&atomic_lock);
    rv = ((*ptr) += val);
    pthread_mutex_unlock(&atomic_lock);
    return rv;
}

PRInt32
_PR_MD_ATOMIC_DECREMENT(PRInt32 *val)
{
    PRInt32 rv;

    pthread_mutex_lock(&atomic_lock);
    rv = --(*val);
    pthread_mutex_unlock(&atomic_lock);
    return rv;
}

PRInt32
_PR_MD_ATOMIC_SET(PRInt32 *val, PRInt32 newval)
{
    PRInt32 rv;

    pthread_mutex_lock(&atomic_lock);
    rv = *val;
    *val = newval;
    pthread_mutex_unlock(&atomic_lock);
    return rv;
}
#else  /* _PR_PTHREADS && !_PR_DCETHREADS */
static PRLock *atomic_lock = NULL;
void _PR_MD_INIT_ATOMIC()
{
    if (atomic_lock == NULL) {
        atomic_lock = PR_NewLock();
    }
}

PRInt32
_PR_MD_ATOMIC_INCREMENT(PRInt32 *val)
{
    PRInt32 rv;

    if (!_pr_initialized) {
        _PR_ImplicitInitialization();
    }
    PR_Lock(atomic_lock);
    rv = ++(*val);
    PR_Unlock(atomic_lock);
    return rv;
}

PRInt32
_PR_MD_ATOMIC_ADD(PRInt32 *ptr, PRInt32 val)
{
    PRInt32 rv;

    if (!_pr_initialized) {
        _PR_ImplicitInitialization();
    }
    PR_Lock(atomic_lock);
    rv = ((*ptr) += val);
    PR_Unlock(atomic_lock);
    return rv;
}

PRInt32
_PR_MD_ATOMIC_DECREMENT(PRInt32 *val)
{
    PRInt32 rv;

    if (!_pr_initialized) {
        _PR_ImplicitInitialization();
    }
    PR_Lock(atomic_lock);
    rv = --(*val);
    PR_Unlock(atomic_lock);
    return rv;
}

PRInt32
_PR_MD_ATOMIC_SET(PRInt32 *val, PRInt32 newval)
{
    PRInt32 rv;

    if (!_pr_initialized) {
        _PR_ImplicitInitialization();
    }
    PR_Lock(atomic_lock);
    rv = *val;
    *val = newval;
    PR_Unlock(atomic_lock);
    return rv;
}
#endif  /* _PR_PTHREADS && !_PR_DCETHREADS */

#endif  /* !_PR_HAVE_ATOMIC_OPS */

void _PR_InitAtomic(void)
{
    _PR_MD_INIT_ATOMIC();
}

PR_IMPLEMENT(PRInt32)
PR_AtomicIncrement(PRInt32 *val)
{
    return _PR_MD_ATOMIC_INCREMENT(val);
}

PR_IMPLEMENT(PRInt32)
PR_AtomicDecrement(PRInt32 *val)
{
    return _PR_MD_ATOMIC_DECREMENT(val);
}

PR_IMPLEMENT(PRInt32)
PR_AtomicSet(PRInt32 *val, PRInt32 newval)
{
    return _PR_MD_ATOMIC_SET(val, newval);
}

PR_IMPLEMENT(PRInt32)
PR_AtomicAdd(PRInt32 *ptr, PRInt32 val)
{
    return _PR_MD_ATOMIC_ADD(ptr, val);
}
/*
 * For platforms, which don't support the CAS (compare-and-swap) instruction
 * (or an equivalent), the stack operations are implemented by use of PRLock
 */

PR_IMPLEMENT(PRStack *)
PR_CreateStack(const char *stack_name)
{
PRStack *stack;

    if (!_pr_initialized) {
        _PR_ImplicitInitialization();
    }

    if ((stack = PR_NEW(PRStack)) == NULL) {
		return NULL;
	}
	if (stack_name) {
		stack->prstk_name = (char *) PR_Malloc(strlen(stack_name) + 1);
		if (stack->prstk_name == NULL) {
			PR_DELETE(stack);
			return NULL;
		}
		strcpy(stack->prstk_name, stack_name);
	} else
		stack->prstk_name = NULL;

#ifndef _PR_HAVE_ATOMIC_CAS
    stack->prstk_lock = PR_NewLock();
	if (stack->prstk_lock == NULL) {
		PR_Free(stack->prstk_name);
		PR_DELETE(stack);
		return NULL;
	}
#endif /* !_PR_HAVE_ATOMIC_CAS */

	stack->prstk_head.prstk_elem_next = NULL;
	
    return stack;
}

PR_IMPLEMENT(PRStatus)
PR_DestroyStack(PRStack *stack)
{
	if (stack->prstk_head.prstk_elem_next != NULL) {
		PR_SetError(PR_INVALID_STATE_ERROR, 0);
		return PR_FAILURE;
	}

	if (stack->prstk_name)
		PR_Free(stack->prstk_name);
#ifndef _PR_HAVE_ATOMIC_CAS
	PR_DestroyLock(stack->prstk_lock);
#endif /* !_PR_HAVE_ATOMIC_CAS */
	PR_DELETE(stack);

	return PR_SUCCESS;
}

#ifndef _PR_HAVE_ATOMIC_CAS

PR_IMPLEMENT(void)
PR_StackPush(PRStack *stack, PRStackElem *stack_elem)
{
    PR_Lock(stack->prstk_lock);
	stack_elem->prstk_elem_next = stack->prstk_head.prstk_elem_next;
	stack->prstk_head.prstk_elem_next = stack_elem;
    PR_Unlock(stack->prstk_lock);
    return;
}

PR_IMPLEMENT(PRStackElem *)
PR_StackPop(PRStack *stack)
{
PRStackElem *element;

    PR_Lock(stack->prstk_lock);
	element = stack->prstk_head.prstk_elem_next;
	if (element != NULL) {
		stack->prstk_head.prstk_elem_next = element->prstk_elem_next;
		element->prstk_elem_next = NULL;	/* debugging aid */
	}
    PR_Unlock(stack->prstk_lock);
    return element;
}
#endif /* !_PR_HAVE_ATOMIC_CAS */
