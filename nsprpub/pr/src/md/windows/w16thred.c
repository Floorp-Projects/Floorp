/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
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

#include "primpl.h"
#include <sys/timeb.h>
#include <stdio.h>

/*
** DispatchTrace -- define a thread dispatch trace entry
**
** The DispatchTrace oject(s) are instantiated in a single
** array. Think of the array as a push-down stack; entry
** zero is the most recent, entry one the next most recent, etc.
** For each time PR_MD_RESTORE_CONTEXT() is called, the array
** is Pushed down and entry zero is overwritten with data
** for the newly dispatched thread.
**
** Function TraceDispatch() manages the DispatchTrace array.
**
*/
typedef struct DispatchTrace
{
    PRThread *          thread;
    PRUint32            state;
    PRInt16             mdThreadNumber;
    PRInt16             unused;
    PRThreadPriority    priority;
    
} DispatchTrace, *DispatchTracePtr ;

static void TraceDispatch( PRThread *thread );


PRThread                *_pr_primordialThread;

/*
** Note: the static variables must be on the data-segment because
** the stack is destroyed during shadow-stack copy operations.
**
*/
static char * pSource;          /* ptr to sourc of a "shadow-stack" copy */
static char * pTarget;          /* ptr to target of a "shadow-stack" copy */
static int   cxByteCount;       /* number of bytes for "shadow-stack" copy */
static int   bytesMoved;        /* instrumentation: WRT "shadow-stack" copy */
static FILE *    file1 = 0;     /* instrumentation: WRT debug */

#define NUM_DISPATCHTRACE_OBJECTS  24
static DispatchTrace dt[NUM_DISPATCHTRACE_OBJECTS] = {0}; /* instrumentation: WRT dispatch */
static PRUint32 dispatchCount = 0;  /* instrumentation: number of thread dispatches */

static int OldPriorityOfPrimaryThread   = -1;
static int TimeSlicesOnNonPrimaryThread =  0;
static PRUint32 threadNumber = 1;   /* Instrumentation: monotonically increasing number */



/*
** _PR_MD_FINAL_INIT() -- Final MD Initialization
**
** Poultry Problems! ... The stack, as allocated by PR_NewStack()
** is called from here, late in initialization, because PR_NewStack()
** requires lots of things working. When some elements of the
** primordial thread are created, early in initialization, the
** shadow stack is not one of these things. The "shadow stack" is
** created here, late in initiailization using PR_NewStack(), to
** ensure consistency in creation of the related objects.
** 
** A new ThreadStack, and all its affiliated structures, is allocated
** via the call to PR_NewStack(). The PRThread structure in the
** new stack is ignored; the old PRThread structure is used (why?).
** The old PRThreadStack structure is abandoned.
**
*/
void
_PR_MD_FINAL_INIT()
{
    PRThreadStack *     stack = 0;
    PRInt32             stacksize = 0;
    PRThread *          me = _PR_MD_CURRENT_THREAD();
    
    _PR_ADJUST_STACKSIZE( stacksize );
    stack = _PR_NewStack( stacksize );
    
    me->stack = stack;
    stack->thr = me;
    
    return;
} /* --- end _PR_MD_FINAL_INIT() --- */


void
_MD_INIT_RUNNING_CPU( struct _PRCPU *cpu )
{
	PR_INIT_CLIST(&(cpu->md.ioQ));
	cpu->md.ioq_max_osfd = -1;
	cpu->md.ioq_timeout = PR_INTERVAL_NO_TIMEOUT;
}    


void
_PR_MD_YIELD( void )
{
    PR_ASSERT(0);
}

/*
** _PR_MD_INIT_STACK() -- Win16 specific Stack initialization.
**
**
*/

void
_PR_MD_INIT_STACK( PRThreadStack *ts, PRIntn redzone )
{
    ts->md.stackTop = ts->stackTop - sizeof(PRThread);
    ts->md.cxByteCount = 0;
    
    return;
} /* --- end _PR_MD_INIT_STACK() --- */

/*
**  _PR_MD_INIT_THREAD() -- Win16 specific Thread initialization.
**
*/
PRStatus
_PR_MD_INIT_THREAD(PRThread *thread)
{
    if ( thread->flags & _PR_PRIMORDIAL)
    {
        _pr_primordialThread = thread;
        thread->md.threadNumber = 1;
    }
    else
    {
        thread->md.threadNumber = ++threadNumber;
    }

    thread->md.magic = _MD_MAGIC_THREAD;
    strcpy( thread->md.guardBand, "GuardBand" );
    
    return PR_SUCCESS;
}


PRStatus
_PR_MD_WAIT(PRThread *thread, PRIntervalTime ticks)
{
    _MD_SWITCH_CONTEXT( thread );
    
    return( PR_SUCCESS );
}

void *PR_W16GetExceptionContext(void)
{
    return _MD_CURRENT_THREAD()->md.exceptionContext;
}

void
PR_W16SetExceptionContext(void *context)
{
    _MD_CURRENT_THREAD()->md.exceptionContext = context;
}




/*
** _MD_RESTORE_CONTEXT() -- Resume execution of thread 't'.
**
** Win16 threading is based on the NSPR 2.0 general model of
** user threads. It differs from the general model in that a 
** single "real" stack segment is used for execution of all 
** threads. The context of the suspended threads is preserved
** in the md.context [and related members] of the PRThread 
** structure. The stack context of the suspended thread is
** preserved in a "shadow stack" object.
**
** _MD_RESTORE_CONTEXT() implements most of the thread switching
** for NSPR's implementation of Win16 theads.
**
** Operations Notes:
**
** Function PR_NewStack() in prustack.c allocates a new
** PRThreadStack, PRStack, PRSegment, and a "shadow" stack
** for a thread. These structures are wired together to
** form the basis of Win16 threads. The thread and shadow
** stack structures are created as part of PR_CreateThread().
** 
** Note! Some special "magic" is applied to the "primordial"
** thread. The physical layout of the PRThread, PRThreadStack,
** shadow stack, ... is somewhat different. Watch yourself when
** mucking around with it. ... See _PR_MD_FINAL_INIT() for most
** of the special treatment of the primordial thread.
**
** Function _PR_MD_INIT_STACK() initializes the value of
** PRThreadStack member md.cxByteCount to zero; there
** is no context to be restored for a thread's initial
** dispatch. The value of member md.stackTop is set to
** point to the highest usable address on the shadow stack.
** This point corresponds to _pr_top_of_task_stack on the
** system's operating stack.
**
** _pr_top_of_task_stack points to a place on the system stack
** considered to be "close to the top". Stack context is preserved
** relative to this point.
**
** Reminder: In x86 architecture, the stack grows "down".
** That is: the stack pointer (SP register) is decremented
** to push objects onto the stack or when a call is made.
** 
** Function _PR_MD_WAIT() invokes macro _MD_SWITCH_CONTEXT();
** this causes the hardware registers to be preserved in a
** CATCHBUF structure using function Catch() [see _win16.h], 
** then calls PR_Schedule() to select a new thread for dispatch. 
** PR_Schedule() calls _MD_RESTORE_CONTEXT() to cause the thread 
** being suspended's stack to be preserved, to restore the 
** stack of the to-be-dispactched thread, and to restore the 
** to-be-dispactched thread's hardware registers.
**
** At the moment _PR_MD_RESTORE_CONTEXT() is called, the stack
** pointer (SP) is less than the reference pointer
** _pr_top_of_task_stack. The distance difference between the SP and
** _pr_top_of_task_stack is the amount of stack that must be preserved.
** This value, cxByteCount, is calculated then preserved in the
** PRThreadStack.md.cxByteCount for later use (size of stack
** context to restore) when this thread is dispatched again.
** 
** A C language for() loop is used to copy, byte-by-byte, the
** stack data being preserved starting at the "address of t"
** [Note: 't' is the argument passed to _PR_MD_RESTORE_CONTEXT()]
** for the length of cxByteCount.
**
** variables pSource and pTarget are the calculated source and
** destination pointers for the stack copy operation. These
** variables are static scope because they cannot be instantiated
** on the stack itself, since the stack is clobbered by restoring
** the to-be-dispatched thread's stack context.
**
** After preserving the suspended thread's stack and architectural
** context, the to-be-dispatched thread's stack context is copied
** from its shadow stack to the system operational stack. The copy
** is done in a small fragment of in-line assembly language. Note:
** In NSPR 1.0, a while() loop was used to do the copy; when compiled
** with the MS C 1.52c compiler, the short while loop used no
** stack variables. The Watcom compiler, specified for use on NSPR 2.0,
** uses stack variables to implement the same while loop. This is
** a no-no! The copy operation clobbers these variables making the
** results of the copy ... unpredictable ... So, a short piece of
** inline assembly language is used to effect the copy.
**
** Following the restoration of the to-be-dispatched thread's
** stack context, another short inline piece of assemble language
** is used to set the SP register to correspond to what it was
** when the to-be-dispatched thread was suspended. This value
** uses the thread's stack->md.cxByteCount as a negative offset 
** from _pr_top_of_task_stack as the new value of SP.
**
** Finally, Function Throw() is called to restore the architectural
** context of the to-be-dispatched thread.
**
** At this point, the newly dispatched thread appears to resume
** execution following the _PR_MD_SWITCH_CONTEXT() macro.
**
** OK, this ain't rocket-science, but it can confuse you easily.
** If you have to work on this stuff, please take the time to
** draw, on paper, the structures (PRThread, PRThreadStack,
** PRSegment, the "shadow stack", the system stack and the related
** global variables). Hand step it thru the debugger to make sure
** you understand it very well before making any changes. ...
** YMMV.
** 
*/
void _MD_RESTORE_CONTEXT(PRThread *t)
{
    dispatchCount++;
    TraceDispatch( t );
    /*	
    **	This is a good opportunity to make sure that the main
    **	mozilla thread actually gets some time.  If interrupts
    **	are on, then we know it is safe to check if the main
    **	thread is being starved.  If moz has not been scheduled
    **	for a long time, then then temporarily bump the fe priority 
    **	up so that it gets to run at least one. 
    */	
// #if 0 // lth. condition off for debug.
    if (_pr_primordialThread == t) {
        if (OldPriorityOfPrimaryThread != -1) {
            PR_SetThreadPriority(_pr_primordialThread, OldPriorityOfPrimaryThread);
            OldPriorityOfPrimaryThread = -1;
        }
        TimeSlicesOnNonPrimaryThread = 0;
    } else {
        TimeSlicesOnNonPrimaryThread++;
    }

    if ((TimeSlicesOnNonPrimaryThread >= 20) && (OldPriorityOfPrimaryThread == -1)) {
        OldPriorityOfPrimaryThread = PR_GetThreadPriority(_pr_primordialThread);
        PR_SetThreadPriority(_pr_primordialThread, 31);
        TimeSlicesOnNonPrimaryThread = 0;
    }
// #endif
    /*
    ** Save the Task Stack into the "shadow stack" of the current thread
    */
    cxByteCount  = (int) ((PRUint32) _pr_top_of_task_stack - (PRUint32) &t );
    pSource      = (char *) &t;
    pTarget      = (char *)((PRUint32)_pr_currentThread->stack->md.stackTop 
                            - (PRUint32)cxByteCount );
    _pr_currentThread->stack->md.cxByteCount = cxByteCount;
    
    for( bytesMoved = 0; bytesMoved < cxByteCount; bytesMoved++ )
        *(pTarget + bytesMoved ) = *(pSource + bytesMoved );
    
    /* Mark the new thread as the current thread */
    _pr_currentThread = t;

    /*
    ** Now copy the "shadow stack" of the new thread into the Task Stack
    **
    ** REMEMBER:
    **    After the stack has been copied, ALL local variables in this function
    **    are invalid !!
    */
    cxByteCount  = t->stack->md.cxByteCount;
    pSource      = t->stack->md.stackTop - cxByteCount;
    pTarget      = _pr_top_of_task_stack - cxByteCount;
    
    errno = (_pr_currentThread)->md.errcode;
    
    __asm 
    {
        mov cx, cxByteCount
        mov si, WORD PTR [pSource]
        mov di, WORD PTR [pTarget]
        mov ax, WORD PTR [pTarget + 2]
        mov es, ax
        mov ax, WORD PTR [pSource + 2]
        mov bx, ds
        mov ds, ax
        rep movsb
        mov ds, bx
    }

    /* 
    ** IMPORTANT:
    ** ----------
    ** SS:SP is now invalid :-( This means that all local variables and
    ** function arguments are invalid and NO function calls can be
    ** made !!! We must fix up SS:SP so that function calls can safely
    ** be made...
    */

    __asm {
        mov     ax, WORD PTR [_pr_top_of_task_stack]
        sub     ax, cxByteCount
        mov     sp, ax
    };

    /*
    ** Resume execution of thread: t by restoring the thread's context.
    **
    */
    Throw((_pr_currentThread)->md.context, 1);
} /* --- end MD_RESTORE_CONTEXT() --- */


static void TraceDispatch( PRThread *thread )
{
    int i;
    
    /*
    ** push all DispatchTrace objects to down one slot.
    ** Note: the last entry is lost; last-1 becomes last, etc.
    */
    for( i = NUM_DISPATCHTRACE_OBJECTS -2; i >= 0; i-- )
    {
        dt[i +1] = dt[i];
    }
    
    /*
    ** Build dt[0] from t
    */
    dt->thread = thread;
    dt->state = thread->state;
    dt->mdThreadNumber = thread->md.threadNumber;
    dt->priority = thread->priority;
    
    return;
} /* --- end TraceDispatch() --- */


/* $$ end W16thred.c */
