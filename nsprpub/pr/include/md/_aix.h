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

#ifndef nspr_aix_defs_h___
#define nspr_aix_defs_h___

#include <sys/types.h>
#if defined(_PR_PTHREADS) || defined(PTHREADS_USER)
#include <pthread.h>
#endif

/*
 * To pick up fd_set.  In AIX 4.2, fd_set is defined in <sys/time.h>,
 * which is included by _unixos.h.
 */
#ifdef AIX4_1
#include <sys/select.h>
#include <sys/poll.h>
#endif

/*
 * Internal configuration macros
 */

#define PR_LINKER_ARCH          "aix"
#define _PR_SI_SYSNAME		"AIX"
#define _PR_SI_ARCHITECTURE	"rs6000"
#define PR_DLL_SUFFIX		".so"

#define _PR_VMBASE	 	0x30000000
#define _PR_STACK_VMBASE	0x50000000
#define _MD_DEFAULT_STACK_SIZE	65536L
#define _MD_MMAP_FLAGS		MAP_PRIVATE

#define NEED_TIME_R
#undef  HAVE_STACK_GROWING_UP
#undef	HAVE_WEAK_IO_SYMBOLS
#undef	HAVE_WEAK_MALLOC_SYMBOLS
#define	HAVE_DLL
#define	USE_DLFCN

#undef _PR_HAVE_ATOMIC_OPS

#define _MD_GET_INTERVAL                  _PR_UNIX_GetInterval
#define _MD_INTERVAL_PER_SEC              _PR_UNIX_TicksPerSecond

#define USE_SETJMP

#include <setjmp.h>

#define _MD_GET_SP(_t)				(_t)->md.jb[3]
#define _MD_SET_THR_SP(_t, _sp)		((_t)->md.jb[3] = (int) (_sp - 2 * 64))
#define PR_NUM_GCREGS				_JBLEN

#define CONTEXT(_th) 				((_th)->md.jb)
#define SAVE_CONTEXT(_th)			_setjmp(CONTEXT(_th))
#define GOTO_CONTEXT(_th)			_longjmp(CONTEXT(_th), 1)

#ifdef PTHREADS_USER
#include "_nspr_pthread.h"
#else

/*
** Initialize the thread context preparing it to execute _main.
*/
#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)	      \
    PR_BEGIN_MACRO				      \
        *status = PR_TRUE;              \
	if (setjmp(CONTEXT(_thread))) {	\
	    (*_main)();			\
	}				\
	_MD_GET_SP(_thread) = (int) (_sp - 2 * 64);		\
    PR_END_MACRO

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
    errno = (_thread)->md.errcode; \
    _MD_SET_CURRENT_THREAD(_thread); \
    longjmp(CONTEXT(_thread), 1); \
}

/* Machine-dependent (MD) data structures */

struct _MDThread {
    jmp_buf jb;
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

#if !defined(_PR_PTHREADS)
#define _MD_INIT_LOCKS()
#endif

#define _MD_NEW_LOCK(lock) PR_SUCCESS
#define _MD_FREE_LOCK(lock)
#define _MD_LOCK(lock)
#define _MD_UNLOCK(lock)
#define _MD_INIT_IO()
#define _MD_IOQ_LOCK()
#define _MD_IOQ_UNLOCK()

#define _MD_EARLY_INIT          	_MD_EarlyInit
#define _MD_FINAL_INIT			_PR_UnixInit
#define _MD_INIT_RUNNING_CPU(cpu)	_MD_unix_init_running_cpu(cpu)
#define _MD_INIT_THREAD			_MD_InitializeThread
#define _MD_EXIT_THREAD(thread)
#define	_MD_SUSPEND_THREAD(thread)
#define	_MD_RESUME_THREAD(thread)
#define _MD_CLEAN_THREAD(_thread)
#endif /* PTHREADS_USER */

#ifdef AIX4_1
#define _MD_SELECT	select
#define _MD_POLL	poll
#endif

#endif /* nspr_aix_defs_h___ */
