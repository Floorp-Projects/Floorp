/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

void _MD_EarlyInit(void)
{
}

PRWord *_MD_HomeGCRegisters(PRThread *t, int isCurrent, int *np)
{
#ifndef _PR_PTHREADS
    if (isCurrent) {
	(void) setjmp(CONTEXT(t));
    }
    *np = sizeof(CONTEXT(t)) / sizeof(PRWord);
    return (PRWord *) CONTEXT(t);
#else
	*np = 0;
	return NULL;
#endif
}

#ifndef _PR_PTHREADS
void
_MD_SET_PRIORITY(_MDThread *thread, PRUintn newPri)
{
    return;
}

PRStatus
_MD_InitializeThread(PRThread *thread)
{
	return PR_SUCCESS;
}

PRStatus
_MD_WAIT(PRThread *thread, PRIntervalTime ticks)
{
    PR_ASSERT(!(thread->flags & _PR_GLOBAL_SCOPE));
    _PR_MD_SWITCH_CONTEXT(thread);
    return PR_SUCCESS;
}

PRStatus
_MD_WAKEUP_WAITER(PRThread *thread)
{
    if (thread) {
	PR_ASSERT(!(thread->flags & _PR_GLOBAL_SCOPE));
    }
    return PR_SUCCESS;
}

/* These functions should not be called for OSF1 */
void
_MD_YIELD(void)
{
    PR_NOT_REACHED("_MD_YIELD should not be called for OSF1.");
}

PRStatus
_MD_CREATE_THREAD(
    PRThread *thread,
    void (*start) (void *),
    PRThreadPriority priority,
    PRThreadScope scope,
    PRThreadState state,
    PRUint32 stackSize)
{
    PR_NOT_REACHED("_MD_CREATE_THREAD should not be called for OSF1.");
	return PR_FAILURE;
}
#endif /* ! _PR_PTHREADS */

#ifdef _PR_HAVE_ATOMIC_CAS

#include <c_asm.h>

#define _PR_OSF_ATOMIC_LOCK 1

void 
PR_StackPush(PRStack *stack, PRStackElem *stack_elem)
{
long locked;

	do {
		while ((long) stack->prstk_head.prstk_elem_next ==
							_PR_OSF_ATOMIC_LOCK)
			;
		locked = __ATOMIC_EXCH_QUAD(&stack->prstk_head.prstk_elem_next,
								_PR_OSF_ATOMIC_LOCK);	

	} while (locked == _PR_OSF_ATOMIC_LOCK);
	stack_elem->prstk_elem_next = (PRStackElem *) locked;
	/*
	 * memory-barrier instruction
	 */
	asm("mb");
	stack->prstk_head.prstk_elem_next = stack_elem;
}

PRStackElem * 
PR_StackPop(PRStack *stack)
{
PRStackElem *element;
long locked;

	do {
		while ((long)stack->prstk_head.prstk_elem_next == _PR_OSF_ATOMIC_LOCK)
			;
		locked = __ATOMIC_EXCH_QUAD(&stack->prstk_head.prstk_elem_next,
								_PR_OSF_ATOMIC_LOCK);	

	} while (locked == _PR_OSF_ATOMIC_LOCK);

	element = (PRStackElem *) locked;

	if (element == NULL) {
		stack->prstk_head.prstk_elem_next = NULL;
	} else {
		stack->prstk_head.prstk_elem_next =
			element->prstk_elem_next;
	}
	/*
	 * memory-barrier instruction
	 */
	asm("mb");
	return element;
}
#endif /* _PR_HAVE_ATOMIC_CAS */


/*
** thread_suspend and thread_resume are used by the gc code
** in nsprpub/pr/src/pthreads/ptthread.c
**
** These routines are never called for the current thread, and
** there is no check for that - so beware!
*/
int thread_suspend(PRThread *thr_id) {

    extern int pthread_suspend_np (
			pthread_t                       thread,
			__pthreadLongUint_t             *regs,
			void                            *spare);

    __pthreadLongUint_t regs[34];
    int res;

    /*
    ** A return res < 0 indicates that the thread was suspended
    ** but register information could not be obtained
    */

    res = pthread_suspend_np(thr_id->id,&regs[0],0);
    if (res==0)
	thr_id->sp = (void *) regs[30]; 

    thr_id->suspend |= PT_THREAD_SUSPENDED;

    /* Always succeeds */
    return 0;
}

int thread_resume(PRThread *thr_id) {
    extern int pthread_resume_np(pthread_t thread);
    int res;

    res = pthread_resume_np (thr_id->id);
	
    thr_id->suspend |= PT_THREAD_RESUMED;

    return 0;
}

/*
** Stubs for nspr_symvec.opt
**
** PR_ResumeSet, PR_ResumeTest, and PR_SuspendAllSuspended
** (defined in ptthread.c) used to be exported by mistake
** (because they look like public functions).  They have been
** converted into static functions.
**
** There is an existing third-party binary that uses NSPR: the
** Java plugin for Mozilla.  Because it is part of the Java
** SDK, we have no control over its releases.  So we need these
** stub functions to occupy the slots that used to be occupied
** by PR_ResumeSet, PR_ResumeTest, and PR_SuspendAllSuspended
** in the symbol vector so that LIBNSPR4 is backward compatible.
*/

void PR_VMS_Stub1(void) { }
void PR_VMS_Stub2(void) { }
void PR_VMS_Stub3(void) { }
