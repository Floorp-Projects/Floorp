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

#ifndef nspr_osf1_defs_h___
#define nspr_osf1_defs_h___

/*
 * Internal configuration macros
 */

#define PR_LINKER_ARCH	"osf"
#define _PR_SI_SYSNAME	"OSF"
#define _PR_SI_ARCHITECTURE "alpha"
#define PR_DLL_SUFFIX		".so"

#define _PR_VMBASE		0x30000000
#define _PR_STACK_VMBASE	0x50000000
#define _MD_DEFAULT_STACK_SIZE	131072L
/*
 * OSF1 needs the MAP_FIXED flag to ensure that mmap returns a pointer
 * with the upper 32 bits zero.  This is because Java sticks a pointer
 * into an int.
 */
#define _MD_MMAP_FLAGS          MAP_PRIVATE|MAP_FIXED

#undef  HAVE_STACK_GROWING_UP
#undef 	HAVE_WEAK_IO_SYMBOLS
#undef 	HAVE_WEAK_MALLOC_SYMBOLS
#define HAVE_DLL
#define HAVE_BSD_FLOCK

#define NEED_TIME_R
#define USE_DLFCN

#define USE_SETJMP

#include <setjmp.h>

/*
 * A jmp_buf is actually a struct sigcontext.  The sc_sp field of
 * struct sigcontext is the stack pointer.
 */
#define _MD_GET_SP(_t) (((struct sigcontext *) (_t)->md.context)->sc_sp)
#define PR_NUM_GCREGS _JBLEN
#define CONTEXT(_th) ((_th)->md.context)

/*
** Initialize a thread context to run "_main()" when started
*/
#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)	      \
{									  \
        *status = PR_TRUE;              \
    if (setjmp(CONTEXT(_thread))) {				\
	(*_main)();						\
    }								\
    _MD_GET_SP(_thread) = (long) ((_sp) - 64);			\
    _MD_GET_SP(_thread) &= ~15;					\
}

#define _MD_SWITCH_CONTEXT(_thread)  \
    if (!setjmp(CONTEXT(_thread))) { \
	(_thread)->md.errcode = errno;  \
	_PR_Schedule();		     \
    }

/*
** Restore a thread context, saved by _MD_SWITCH_CONTEXT
*/
#define _MD_RESTORE_CONTEXT(_thread) \
{				     \
    errno = (_thread)->md.errcode;     \
    _MD_SET_CURRENT_THREAD(_thread);	\
    longjmp(CONTEXT(_thread), 1);    \
}

/* Machine-dependent (MD) data structures */

struct _MDThread {
    jmp_buf context;
    int id;
    int errcode;
};

struct _MDThreadStack {
    PRInt8 notused;
};

struct _MDLock {
    PRInt8 notused;
};

struct _MDSemaphore {
    PRInt8 notused;
};

struct _MDCVar {
    PRInt8 notused;
};

struct _MDSegment {
    PRInt8 notused;
};

struct _MDCPU {
	struct _MDCPU_Unix md_unix;
};

#ifndef _PR_PTHREADS
#define _MD_INIT_LOCKS()
#endif
#define _MD_NEW_LOCK(lock) PR_SUCCESS
#define _MD_FREE_LOCK(lock)
#define _MD_LOCK(lock)
#define _MD_UNLOCK(lock)
#define _MD_INIT_IO()
#define _MD_IOQ_LOCK()
#define _MD_IOQ_UNLOCK()

/*
 * The following are copied from _sunos.h, _aix.h.  This means
 * some of them should probably be moved into _unixos.h.  But
 * _irix.h seems to be quite different in regard to these macros.
 */
#define _MD_GET_INTERVAL                  _PR_UNIX_GetInterval
#define _MD_INTERVAL_PER_SEC              _PR_UNIX_TicksPerSecond

#define _MD_EARLY_INIT		_MD_EarlyInit
#define _MD_FINAL_INIT		_PR_UnixInit
#define _MD_INIT_RUNNING_CPU(cpu) _MD_unix_init_running_cpu(cpu)
#define _MD_INIT_THREAD         _MD_InitializeThread
#define _MD_EXIT_THREAD(thread)
#define	_MD_SUSPEND_THREAD(thread)
#define	_MD_RESUME_THREAD(thread)
#define _MD_CLEAN_THREAD(_thread)

/* The following defines unwrapped versions of select() and poll(). */
extern int __select (int, fd_set *, fd_set *, fd_set *, struct timeval *);
#define _MD_SELECT              __select

#include <sys/poll.h>
#define _MD_POLL __poll
extern int __poll(struct pollfd filedes[], unsigned int nfds, int timeout);

/*
 * Atomic operations
 */

/* builtins.h is not available for OSF1 V3.2. */
#ifndef OSF1V3
#include <machine/builtins.h>
#define _PR_HAVE_ATOMIC_OPS
#define _MD_INIT_ATOMIC()
#define _MD_ATOMIC_INCREMENT(val) (__ATOMIC_INCREMENT_LONG(val) + 1)
#define _MD_ATOMIC_DECREMENT(val) (__ATOMIC_DECREMENT_LONG(val) - 1)
#define _MD_ATOMIC_SET(val, newval) __ATOMIC_EXCH_LONG(val, newval)
#endif /* OSF1V3 */

#endif /* nspr_osf1_defs_h___ */
