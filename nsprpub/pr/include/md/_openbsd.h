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

#ifndef nspr_openbsd_defs_h___
#define nspr_openbsd_defs_h___

#include <sys/syscall.h>

#define PR_LINKER_ARCH "openbsd"
#define _PR_SI_SYSNAME  "OpenBSD"
#if defined(__i386__)
#define _PR_SI_ARCHITECTURE "x86"
#elif defined(__alpha__)
#define _PR_SI_ARCHITECTURE "alpha"
#elif defined(__m68k__)
#define _PR_SI_ARCHITECTURE "m68k"
#elif defined(__powerpc__)
#define _PR_SI_ARCHITECTURE "powerpc"
#elif defined(__sparc__)
#define _PR_SI_ARCHITECTURE "sparc"
#elif defined(__arm32__)
#define _PR_SI_ARCHITECTURE "arm32"
#endif

#define PR_DLL_SUFFIX          ".so.1.0"

#define _PR_VMBASE              0x30000000
#define _PR_STACK_VMBASE       0x50000000
#define _MD_DEFAULT_STACK_SIZE 65536L
#define _MD_MMAP_FLAGS          MAP_PRIVATE

#undef  HAVE_STACK_GROWING_UP
#define HAVE_DLL
#define USE_DLFCN
#define _PR_HAVE_SOCKADDR_LEN

#define USE_SETJMP

#ifndef _PR_PTHREADS
#include <setjmp.h>

#define PR_CONTEXT_TYPE        sigjmp_buf

#define CONTEXT(_th) ((_th)->md.context)

#if defined(__i386__) || defined(__sparc__) || defined(__m68k__) || defined(__powerpc__)
#define JB_SP_INDEX 2
#elif defined(__alpha__)
#define JB_SP_INDEX 34
#elif defined(__arm32__)
/*
 * On the arm32, the jmpbuf regs underwent a namechange after NetBSD 1.3
 */
#ifdef JMPBUF_REG_R13
#define JB_SP_INDEX JMPBUF_REG_R13
#else
#define JB_SP_INDEX _JB_REG_R13
#endif
#else
#error "Need to define SP index in jmp_buf here"
#endif
#define _MD_GET_SP(_th)    (_th)->md.context[JB_SP_INDEX]

#define PR_NUM_GCREGS  _JBLEN

/*
** Initialize a thread context to run "_main()" when started
*/
#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)  \
{  \
    *status = PR_TRUE;  \
    if (sigsetjmp(CONTEXT(_thread), 1)) {  \
        _main();  \
    }  \
    _MD_GET_SP(_thread) = (unsigned char*) ((_sp) - 64); \
}

#define _MD_SWITCH_CONTEXT(_thread)  \
    if (!sigsetjmp(CONTEXT(_thread), 1)) {  \
        (_thread)->md.errcode = errno;  \
        _PR_Schedule();  \
    }

/*
** Restore a thread context, saved by _MD_SWITCH_CONTEXT
*/
#define _MD_RESTORE_CONTEXT(_thread) \
{   \
    errno = (_thread)->md.errcode;  \
    _MD_SET_CURRENT_THREAD(_thread);  \
    siglongjmp(CONTEXT(_thread), 1);  \
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

#define _MD_INIT_RUNNING_CPU(cpu)       _MD_unix_init_running_cpu(cpu)
#define _MD_INIT_THREAD                 _MD_InitializeThread
#define _MD_EXIT_THREAD(thread)
#define _MD_SUSPEND_THREAD(thread)      _MD_suspend_thread
#define _MD_RESUME_THREAD(thread)       _MD_resume_thread
#define _MD_CLEAN_THREAD(_thread)

#endif /* ! _PR_PTHREADS */

#define _MD_EARLY_INIT                  _MD_EarlyInit
#define _MD_FINAL_INIT                 _PR_UnixInit
#define _MD_GET_INTERVAL                  _PR_UNIX_GetInterval
#define _MD_INTERVAL_PER_SEC              _PR_UNIX_TicksPerSecond

/*
 * We wrapped the select() call.  _MD_SELECT refers to the built-in,
 * unwrapped version.
 */
#define _MD_SELECT(nfds,r,w,e,tv) syscall(SYS_select,nfds,r,w,e,tv)
#define _MD_POLL(fds,nfds,timeout) syscall(SYS_poll,fds,nfds,timeout)

#endif /* nspr_openbsd_defs_h___ */
