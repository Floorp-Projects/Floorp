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

#ifndef nspr_scoos5_defs_h___
#define nspr_scoos5_defs_h___

/*
 * Internal configuration macros
 */

#define PR_LINKER_ARCH	"scoos5"
#define PR_DLL_SUFFIX		".so"

#define _PR_SI_SYSNAME      "SCO"
#define _PR_SI_ARCHITECTURE "x86"
#define _PR_STACK_VMBASE    0x50000000

#define _MD_DEFAULT_STACK_SIZE	65536L
#define _MD_MMAP_FLAGS          MAP_PRIVATE

#undef  HAVE_STACK_GROWING_UP
#define	HAVE_DLL
#define	USE_DLFCN

#if !defined (HAVE_STRERROR)
#define HAVE_STRERROR
#endif

#ifndef	HAVE_WEAK_IO_SYMBOLS
#define	HAVE_WEAK_IO_SYMBOLS
#endif

#define NEED_STRFTIME_LOCK
#define NEED_TIME_R
#define _PR_RECV_BROKEN /* recv doesn't work on Unix Domain Sockets */

#define USE_SETJMP

#ifdef _PR_LOCAL_THREADS_ONLY
#include <setjmp.h>

#define _MD_GET_SP(_t)  (_t)->md.jb[4]
#define PR_NUM_GCREGS	_SIGJBLEN
#define PR_CONTEXT_TYPE	sigjmp_buf

#define CONTEXT(_th) ((_th)->md.jb)

#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)				\
{								  									\
	*status = PR_TRUE;												\
    if (sigsetjmp(CONTEXT(_thread),1)) {				  			\
		(*_main)(); 												\
    }								  								\
    _MD_GET_SP(_thread) = (int) ((_sp) - 64);	\
}

#define _MD_SWITCH_CONTEXT(_thread)  								\
    if (!sigsetjmp(CONTEXT(_thread), 1)) { 							\
		(_thread)->md.errcode = errno;  							\
		_PR_Schedule();		     									\
    }

/*
** Restore a thread context, saved by _MD_SWITCH_CONTEXT
*/
#define _MD_RESTORE_CONTEXT(_thread)								\
{				     												\
    errno = (_thread)->osErrorCode;	     							\
	_MD_SET_CURRENT_THREAD(_thread);								\
    siglongjmp(CONTEXT(_thread), 1);    							\
}

#endif /* _PR_LOCAL_THREADS_ONLY */

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

#define _MD_INIT_LOCKS()
#define _MD_NEW_LOCK(lock) PR_SUCCESS
#define _MD_FREE_LOCK(lock)
#define _MD_LOCK(lock)
#define _MD_UNLOCK(lock)
#define _MD_INIT_IO()
#define _MD_IOQ_LOCK()
#define _MD_IOQ_UNLOCK()

#define _MD_EARLY_INIT          	_MD_EarlyInit
#define _MD_FINAL_INIT				_PR_UnixInit
#define _MD_INIT_RUNNING_CPU(cpu) 	_MD_unix_init_running_cpu(cpu)
#define _MD_INIT_THREAD         	_MD_InitializeThread
#define _MD_EXIT_THREAD(thread)
#define	_MD_SUSPEND_THREAD(thread)
#define	_MD_RESUME_THREAD(thread)
#define _MD_CLEAN_THREAD(_thread)

#define _MD_GET_INTERVAL                  _PR_UNIX_GetInterval
#define _MD_INTERVAL_PER_SEC              _PR_UNIX_TicksPerSecond

#define _MD_SELECT		_select
#define _MD_POLL		_poll

#endif /* nspr_scoos5_defs_h___ */
