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

#ifndef nspr_xhppa_defs_h___
#define nspr_xhppa_defs_h___

/*
 * Internal configuration macros
 */

#define PR_LINKER_ARCH    "hpux"
#define _PR_SI_SYSNAME   "HPUX"
#define _PR_SI_ARCHITECTURE "hppa1.1"
#define PR_DLL_SUFFIX        ".sl"

#define _PR_VMBASE        0x30000000 
#define _PR_STACK_VMBASE    0x50000000
#define _MD_DEFAULT_STACK_SIZE    65536L
#define _MD_MMAP_FLAGS          MAP_PRIVATE

#define NEED_TIME_R

#define HAVE_STACK_GROWING_UP
#undef	HAVE_WEAK_IO_SYMBOLS
#undef	HAVE_WEAK_MALLOC_SYMBOLS
#define	HAVE_DLL
#define USE_HPSHL
#define HAVE_STRERROR

#undef _PR_HAVE_ATOMIC_OPS

#if !defined(_PR_PTHREADS)

#include <syscall.h>
#include <setjmp.h>

#define USE_SETJMP

#define _MD_GET_SP(_t) (*((int *)((_t)->md.jb) + 1))
#define PR_NUM_GCREGS _JBLEN
/* Caveat: This makes jmp_buf full of doubles. */
#define CONTEXT(_th) ((_th)->md.jb)

    /* Stack needs two frames (64 bytes) at the bottom */ \
#define _MD_SET_THR_SP(_t, _sp)     ((_MD_GET_SP(_t)) = (int) (_sp + 64 *2))
#define SAVE_CONTEXT(_th)           _setjmp(CONTEXT(_th))
#define GOTO_CONTEXT(_th)           _longjmp(CONTEXT(_th), 1)

#if !defined(PTHREADS_USER)

#define _MD_INIT_CONTEXT(_thread, _sp, _main, status) \
{ \
    *(status) = PR_TRUE; \
    if (_setjmp(CONTEXT(_thread))) (*_main)(); \
    /* Stack needs two frames (64 bytes) at the bottom */ \
    (_MD_GET_SP(_thread)) = (int) ((_sp) + 64*2); \
}

#define _MD_SWITCH_CONTEXT(_thread) \
    if (!_setjmp(CONTEXT(_thread))) { \
    (_thread)->md.errcode = errno; \
    _PR_Schedule(); \
    }

/*
** Restore a thread context, saved by _MD_SWITCH_CONTEXT
*/
#define _MD_RESTORE_CONTEXT(_thread) \
{ \
    errno = (_thread)->md.errcode; \
    _MD_SET_CURRENT_THREAD(_thread); \
    _longjmp(CONTEXT(_thread), 1); \
}

/* Machine-dependent (MD) data structures.  HP-UX has no native threads. */

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

#define _MD_INIT_RUNNING_CPU(cpu)       _MD_unix_init_running_cpu(cpu)
#define _MD_INIT_THREAD                 _MD_InitializeThread
#define _MD_EXIT_THREAD(thread)
#define _MD_SUSPEND_THREAD(thread)      _MD_suspend_thread
#define _MD_RESUME_THREAD(thread)       _MD_resume_thread
#define _MD_CLEAN_THREAD(_thread)

#else /* PTHREADS_USER	*/

#include "_nspr_pthread.h"

#endif /* PTHREADS_USER	*/

#endif  /* !defined(_PR_PTHREADS) */

#if !defined(PTHREADS_USER)
#define _MD_EARLY_INIT                 	_MD_EarlyInit
#define _MD_FINAL_INIT					_PR_UnixInit
#endif 

#define _MD_GET_INTERVAL                  _PR_UNIX_GetInterval
#define _MD_INTERVAL_PER_SEC              _PR_UNIX_TicksPerSecond

/*
 * We wrapped the select() call.  _MD_SELECT refers to the built-in,
 * unwrapped version.
 */
#define _MD_SELECT(nfds,r,w,e,tv) syscall(SYS_select,nfds,r,w,e,tv)
#define _MD_POLL(fds,nfds,timeout) syscall(SYS_poll,fds,nfds,timeout)

extern void _MD_hpux_install_sigfpe_handler(void);

#ifdef HPUX11
extern void _MD_hpux_map_sendfile_error(int err);
#if !defined(_PR_PTHREADS)
extern PRInt32 _PR_HPUXTransmitFile(PRFileDesc *sd, PRFileDesc *fd,
        const void *headers, PRInt32 hlen,
        PRTransmitFileFlags flags, PRIntervalTime timeout);
#endif /* !_PR_PTHREADS */
#endif /* HPUX11 */

#endif /* nspr_xhppa_defs_h___ */
