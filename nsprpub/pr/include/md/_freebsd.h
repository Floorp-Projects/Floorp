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

#ifndef nspr_freebsd_defs_h___
#define nspr_freebsd_defs_h___

#include <sys/syscall.h>

#define PR_LINKER_ARCH	"freebsd"
#define _PR_SI_SYSNAME  "FREEBSD"
#define _PR_SI_ARCHITECTURE "x86"
#define PR_DLL_SUFFIX		".so.1.0"

#define _PR_VMBASE              0x30000000
#define _PR_STACK_VMBASE	0x50000000
#define _MD_DEFAULT_STACK_SIZE	65536L
#define _MD_MMAP_FLAGS          MAP_PRIVATE

#undef  HAVE_STACK_GROWING_UP
#define HAVE_DLL
#define USE_DLFCN

#define USE_SETJMP

#ifndef _PR_PTHREADS
#include <setjmp.h>

#define PR_CONTEXT_TYPE	sigjmp_buf

#define CONTEXT(_th) ((_th)->md.context)

#define _MD_GET_SP(_th)    (_th)->md.context[0]._sjb[2]
#define PR_NUM_GCREGS	_JBLEN

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

PR_EXTERN(void) _MD_EarlyInit(void);
#define _MD_EARLY_INIT                  _MD_EarlyInit
#define _MD_FINAL_INIT			_PR_UnixInit
#define _MD_GET_INTERVAL                  _PR_UNIX_GetInterval
#define _MD_INTERVAL_PER_SEC              _PR_UNIX_TicksPerSecond

/*
 * We wrapped the select() call.  _MD_SELECT refers to the built-in,
 * unwrapped version.
 */
#define _MD_SELECT(nfds,r,w,e,tv) syscall(SYS_select,nfds,r,w,e,tv)

#if defined(_PR_NEED_FAKE_POLL)

/*
 * XXX: FreeBSD2 doesn't have poll(), but our pthreads code calls poll().
 * As a temporary measure, I implemented a fake poll() using select().
 * Here are the struct and macro definitions copied from sys/poll.h
 * on Solaris 2.5.
 */

struct pollfd {
    int fd;
    short events;
    short revents;
};

/* poll events */

#define	POLLIN		0x0001		/* fd is readable */
#define	POLLPRI		0x0002		/* high priority info at fd */
#define	POLLOUT		0x0004		/* fd is writeable (won't block) */
#define	POLLRDNORM	0x0040		/* normal data is readable */
#define	POLLWRNORM	POLLOUT
#define	POLLRDBAND	0x0080		/* out-of-band data is readable */
#define	POLLWRBAND	0x0100		/* out-of-band data is writeable */

#define	POLLNORM	POLLRDNORM

#define	POLLERR		0x0008		/* fd has error condition */
#define	POLLHUP		0x0010		/* fd has been hung up on */
#define	POLLNVAL	0x0020		/* invalid pollfd entry */

extern int poll(struct pollfd *, unsigned long, int);

#endif /* _PR_NEED_FAKE_POLL */

/* freebsd has INADDR_LOOPBACK defined, but in /usr/include/rpc/types.h, and I didn't
   want to be including that.. */
#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK         (u_long)0x7F000001
#endif

#endif /* nspr_linux_defs_h___ */
