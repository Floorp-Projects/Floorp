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

#ifndef nspr_sony_defs_h___
#define nspr_sony_defs_h___
 
#define PR_LINKER_ARCH		"sony"
#define _PR_SI_SYSNAME		"SONY"
#define _PR_SI_ARCHITECTURE	"mips"
#define PR_DLL_SUFFIX		".so"
 
#define _PR_VMBASE		0x30000000
#define _PR_STACK_VMBASE	0x50000000
#define _MD_DEFAULT_STACK_SIZE	65536L
#define _MD_MMAP_FLAGS          MAP_PRIVATE

#define _MD_GET_INTERVAL	_PR_UNIX_GetInterval
#define _MD_INTERVAL_PER_SEC	_PR_UNIX_TicksPerSecond

#if defined(_PR_LOCAL_THREADS_ONLY)
#include <ucontext.h>
#include <sys/regset.h>
 
#define PR_NUM_GCREGS	NGREG
#define PR_CONTEXT_TYPE	ucontext_t
 
#define CONTEXT(_thread)	(&(_thread)->md.context)
 
#define _MD_GET_SP(_t)		(_t)->md.context.uc_mcontext.gregs[CXT_SP]
 
/*
** Initialize the thread context preparing it to execute _main()
*/
#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)               \
{                                                                   \
    *status = PR_TRUE;  \
    getcontext(CONTEXT(_thread));                                    \
    CONTEXT(_thread)->uc_stack.ss_sp = (char*) (_thread)->stack->stackBottom; \
    CONTEXT(_thread)->uc_stack.ss_size = (_thread)->stack->stackSize; \
    _MD_GET_SP(_thread) = (greg_t) (_sp) - 64;             \
    makecontext(CONTEXT(_thread), _main, 0);              \
}
 
#define _MD_SWITCH_CONTEXT(_thread)      \
    if (!getcontext(CONTEXT(_thread))) { \
        (_thread)->md.errcode = errno;      \
        _PR_Schedule();                  \
    }
 
/*
** Restore a thread context, saved by _MD_SWITCH_CONTEXT
*/
#define _MD_RESTORE_CONTEXT(_thread)   \
{                                      \
    ucontext_t *uc = CONTEXT(_thread); \
    uc->uc_mcontext.gregs[CXT_V0] = 1; \
    uc->uc_mcontext.gregs[CXT_A3] = 0; \
    _MD_SET_CURRENT_THREAD(_thread);      \
    errno = (_thread)->md.errcode;        \
    setcontext(uc);                    \
}

/* Machine-dependent (MD) data structures */

struct _MDThread {
    PR_CONTEXT_TYPE context;
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
#define _MD_FINAL_INIT			_PR_UnixInit
#define _MD_INIT_RUNNING_CPU(cpu)	_MD_unix_init_running_cpu(cpu)
#define _MD_INIT_THREAD			_MD_InitializeThread
#define _MD_EXIT_THREAD(thread)
#define	_MD_SUSPEND_THREAD(thread)
#define	_MD_RESUME_THREAD(thread)
#define _MD_CLEAN_THREAD(_thread)

/* The following defines unwrapped versions of select() and poll(). */
extern int _select (int, fd_set *, fd_set *, fd_set *, struct timeval *);
#define _MD_SELECT              _select

#include <sys/poll.h>
extern int _poll(struct pollfd *fds, unsigned long nfds, int timeout);
#define _MD_POLL _poll
 
#endif /* _PR_LOCAL_THREADS_ONLY */
 
#undef	HAVE_STACK_GROWING_UP
#define	HAVE_DLL
#define	USE_DLFCN
#define	NEED_TIME_R
#define NEED_STRFTIME_LOCK
 
/*
** Missing function prototypes
*/
extern int gethostname(char *name, int namelen);
 
#endif /* nspr_sony_defs_h___ */
