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

/* List of free stack virtual memory chunks */
PRLock *_pr_stackLock;
PRCList _pr_freeStacks = PR_INIT_STATIC_CLIST(&_pr_freeStacks);
PRIntn _pr_numFreeStacks;
PRIntn _pr_maxFreeStacks = 4;

#ifdef DEBUG
/*
** A variable that can be set via the debugger...
*/
PRBool _pr_debugStacks = PR_FALSE;
#endif

/* How much space to leave between the stacks, at each end */
#define REDZONE		(2 << _pr_pageShift)

#define _PR_THREAD_STACK_PTR(_qp) \
    ((PRThreadStack*) ((char*) (_qp) - offsetof(PRThreadStack,links)))

void _PR_InitStacks(void)
{
    _pr_stackLock = PR_NewLock();
}

/*
** Allocate a stack for a thread.
*/
PRThreadStack *_PR_NewStack(PRUint32 stackSize)
{
    PRCList *qp;
    PRThreadStack *ts;
    PRThread *thr;

    /*
    ** Trim the list of free stacks. Trim it backwards, tossing out the
    ** oldest stack found first (this way more recent stacks have a
    ** chance of being present in the data cache).
    */
    PR_Lock(_pr_stackLock);
    qp = _pr_freeStacks.prev;
    while ((_pr_numFreeStacks > _pr_maxFreeStacks) && (qp != &_pr_freeStacks)) {
	ts = _PR_THREAD_STACK_PTR(qp);
	thr = _PR_THREAD_STACK_TO_PTR(ts);
	qp = qp->prev;
	/*
	 * skip stacks which are still being used
	 */
	if (thr->no_sched)
		continue;
	PR_REMOVE_LINK(&ts->links);

	/* Give platform OS to clear out the stack for debugging */
	_PR_MD_CLEAR_STACK(ts);

	_pr_numFreeStacks--;
	_PR_DestroySegment(ts->seg);
	PR_DELETE(ts);
    }

    /*
    ** Find a free thread stack. This searches the list of free'd up
    ** virtually mapped thread stacks.
    */
    qp = _pr_freeStacks.next;
    ts = 0;
    while (qp != &_pr_freeStacks) {
	ts = _PR_THREAD_STACK_PTR(qp);
	thr = _PR_THREAD_STACK_TO_PTR(ts);
	qp = qp->next;
	/*
	 * skip stacks which are still being used
	 */
	if ((!(thr->no_sched)) && ((ts->allocSize - 2*REDZONE) >= stackSize)) {
	    /*
	    ** Found a stack that is not in use and is big enough. Change
	    ** stackSize to fit it.
	    */
	    stackSize = ts->allocSize - 2*REDZONE;
	    PR_REMOVE_LINK(&ts->links);
	    _pr_numFreeStacks--;
	    ts->links.next = 0;
	    ts->links.prev = 0;
	    PR_Unlock(_pr_stackLock);
	    goto done;
	}
	ts = 0;
    }
    PR_Unlock(_pr_stackLock);

    if (!ts) {
	/* Make a new thread stack object. */
	ts = PR_NEWZAP(PRThreadStack);
	if (!ts) {
	    return NULL;
	}

	/*
	** Assign some of the virtual space to the new stack object. We
	** may not get that piece of VM, but if nothing else we will
	** advance the pointer so we don't collide (unless the OS screws
	** up).
	*/
	ts->allocSize = stackSize + 2*REDZONE;
	ts->seg = _PR_NewSegment(ts->allocSize, 0);
	if (!ts->seg) {
	    PR_DELETE(ts);
	    return NULL;
	}
	}

  done:
    ts->allocBase = (char*)ts->seg->vaddr;
    ts->flags = _PR_STACK_MAPPED;
    ts->stackSize = stackSize;

#ifdef HAVE_STACK_GROWING_UP
    ts->stackTop = ts->allocBase + REDZONE;
    ts->stackBottom = ts->stackTop + stackSize;
#else
    ts->stackBottom = ts->allocBase + REDZONE;
    ts->stackTop = ts->stackBottom + stackSize;
#endif

    PR_LOG(_pr_thread_lm, PR_LOG_NOTICE,
	   ("thread stack: base=0x%x limit=0x%x bottom=0x%x top=0x%x\n",
	    ts->allocBase, ts->allocBase + ts->allocSize - 1,
	    ts->allocBase + REDZONE,
	    ts->allocBase + REDZONE + stackSize - 1));
	    
    _PR_MD_INIT_STACK(ts,REDZONE);

    return ts;
}

/*
** Free the stack for the current thread
*/
void _PR_FreeStack(PRThreadStack *ts)
{
    if (!ts) {
	return;
    }
    if (ts->flags & _PR_STACK_PRIMORDIAL) {
	PR_DELETE(ts);
	return;
    }

    /*
    ** Put the stack on the free list. This is done because we are still
    ** using the stack. Next time a thread is created we will trim the
    ** list down; it's safe to do it then because we will have had to
    ** context switch to a live stack before another thread can be
    ** created.
    */
    PR_Lock(_pr_stackLock);
    PR_APPEND_LINK(&ts->links, _pr_freeStacks.prev);
    _pr_numFreeStacks++;
    PR_Unlock(_pr_stackLock);
}
