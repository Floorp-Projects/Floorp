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

#ifndef nspr_unixware_defs_h___
#define nspr_unixware_defs_h___

/*
 * Internal configuration macros
 */

#define PR_LINKER_ARCH	"unixware"
#define _PR_SI_SYSNAME		"UnixWare"
#define _PR_SI_ARCHITECTURE	"x86"
#define PR_DLL_SUFFIX		".so"

#define _PR_VMBASE	 	0x30000000
#define _PR_STACK_VMBASE	0x50000000
#define _MD_DEFAULT_STACK_SIZE	65536L
#define _MD_MMAP_FLAGS          MAP_PRIVATE

#ifndef	HAVE_WEAK_IO_SYMBOLS
#define	HAVE_WEAK_IO_SYMBOLS
#endif

#undef  HAVE_STACK_GROWING_UP
#define HAVE_NETCONFIG
#define	HAVE_DLL
#define	USE_DLFCN
#define HAVE_STRERROR
#define NEED_STRFTIME_LOCK
#define NEED_TIME_R
#define _PR_NEED_STRCASECMP

#define USE_SETJMP

#include <setjmp.h>

#define _SETJMP setjmp
#define _LONGJMP longjmp
#define _PR_CONTEXT_TYPE         jmp_buf
#define _MD_GET_SP(_t)           (_t)->md.context[4]
#define _PR_NUM_GCREGS	_JBLEN

#define CONTEXT(_th) ((_th)->md.context)

/*
** Initialize the thread context preparing it to execute _main.
*/
#define _MD_INIT_CONTEXT(_thread, _sp, _main, status) \
{								  \
    *status = PR_TRUE; \
    if(_SETJMP(CONTEXT(_thread))) (*_main)(); \
    _MD_GET_SP(_thread) = (int) ((_sp) - 128); \
}

#define _MD_SWITCH_CONTEXT(_thread)  \
    if (!_SETJMP(CONTEXT(_thread))) { \
	(_thread)->md.errcode = errno;  \
	_PR_Schedule();		     \
    }

/*
** Restore a thread context, saved by _MD_SWITCH_CONTEXT
*/
#define _MD_RESTORE_CONTEXT(_thread) \
{				     \
    errno = (_thread)->md.errcode;	     \
    _MD_SET_CURRENT_THREAD(_thread); \
    _LONGJMP(CONTEXT(_thread), 1);    \
}

/* Machine-dependent (MD) data structures.
 * Don't use SVR4 native threads (yet). 
 */

struct _MDThread {
    _PR_CONTEXT_TYPE context;
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

/*
 * We wrapped the select() call.  _MD_SELECT refers to the built-in,
 * unwrapped version.
 */
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
extern int _select(int nfds, fd_set *readfds, fd_set *writefds,
	fd_set *execptfds, struct timeval *timeout);
#define _MD_SELECT _select

#define _MD_POLL _poll
extern int _poll(struct pollfd *fds, unsigned long nfds, int timeout);

#endif /* nspr_unixware_defs_h___ */
