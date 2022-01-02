/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nspr_scoos5_defs_h___
#define nspr_scoos5_defs_h___

/*
 * Internal configuration macros
 */

#define PR_LINKER_ARCH  "scoos5"
#define PR_DLL_SUFFIX       ".so"

#define _PR_SI_SYSNAME      "SCO"
#define _PR_SI_ARCHITECTURE "x86"
#define _PR_STACK_VMBASE    0x50000000

#define _MD_DEFAULT_STACK_SIZE  65536L
#define _MD_MMAP_FLAGS          MAP_PRIVATE

#undef  HAVE_STACK_GROWING_UP
#define HAVE_DLL
#define USE_DLFCN

#if !defined (HAVE_STRERROR)
#define HAVE_STRERROR
#endif

#ifndef HAVE_WEAK_IO_SYMBOLS
#define HAVE_WEAK_IO_SYMBOLS
#endif

#define _PR_POLL_AVAILABLE
#define _PR_USE_POLL
#define _PR_NO_LARGE_FILES
#define _PR_STAT_HAS_ONLY_ST_ATIME

#define NEED_STRFTIME_LOCK
#define NEED_TIME_R
#define _PR_RECV_BROKEN /* recv doesn't work on Unix Domain Sockets */

#define USE_SETJMP

#ifdef _PR_LOCAL_THREADS_ONLY
#include <setjmp.h>

#define _MD_GET_SP(_t)  (_t)->md.jb[4]
#define PR_NUM_GCREGS   _SIGJBLEN
#define PR_CONTEXT_TYPE sigjmp_buf

#define CONTEXT(_th) ((_th)->md.jb)

#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)               \
{                                                                   \
    *status = PR_TRUE;                                              \
    if (sigsetjmp(CONTEXT(_thread),1)) {                            \
        (*_main)();                                                 \
    }                                                               \
    _MD_GET_SP(_thread) = (int) ((_sp) - 64);   \
}

#define _MD_SWITCH_CONTEXT(_thread)                                 \
    if (!sigsetjmp(CONTEXT(_thread), 1)) {                          \
        (_thread)->md.errcode = errno;                              \
        _PR_Schedule();                                             \
    }

/*
** Restore a thread context, saved by _MD_SWITCH_CONTEXT
*/
#define _MD_RESTORE_CONTEXT(_thread)                                \
{                                                                   \
    errno = (_thread)->osErrorCode;                                 \
    _MD_SET_CURRENT_THREAD(_thread);                                \
    siglongjmp(CONTEXT(_thread), 1);                                \
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

/*
 * md-specific cpu structure field
 */
#define _PR_MD_MAX_OSFD FD_SETSIZE

struct _MDCPU_Unix {
    PRCList ioQ;
    PRUint32 ioq_timeout;
    PRInt32 ioq_max_osfd;
    PRInt32 ioq_osfd_cnt;
#ifndef _PR_USE_POLL
    fd_set fd_read_set, fd_write_set, fd_exception_set;
    PRInt16 fd_read_cnt[_PR_MD_MAX_OSFD],fd_write_cnt[_PR_MD_MAX_OSFD],
            fd_exception_cnt[_PR_MD_MAX_OSFD];
#else
    struct pollfd *ioq_pollfds;
    int ioq_pollfds_size;
#endif  /* _PR_USE_POLL */
};

#define _PR_IOQ(_cpu)           ((_cpu)->md.md_unix.ioQ)
#define _PR_ADD_TO_IOQ(_pq, _cpu) PR_APPEND_LINK(&_pq.links, &_PR_IOQ(_cpu))
#define _PR_FD_READ_SET(_cpu)       ((_cpu)->md.md_unix.fd_read_set)
#define _PR_FD_READ_CNT(_cpu)       ((_cpu)->md.md_unix.fd_read_cnt)
#define _PR_FD_WRITE_SET(_cpu)      ((_cpu)->md.md_unix.fd_write_set)
#define _PR_FD_WRITE_CNT(_cpu)      ((_cpu)->md.md_unix.fd_write_cnt)
#define _PR_FD_EXCEPTION_SET(_cpu)  ((_cpu)->md.md_unix.fd_exception_set)
#define _PR_FD_EXCEPTION_CNT(_cpu)  ((_cpu)->md.md_unix.fd_exception_cnt)
#define _PR_IOQ_TIMEOUT(_cpu)       ((_cpu)->md.md_unix.ioq_timeout)
#define _PR_IOQ_MAX_OSFD(_cpu)      ((_cpu)->md.md_unix.ioq_max_osfd)
#define _PR_IOQ_OSFD_CNT(_cpu)      ((_cpu)->md.md_unix.ioq_osfd_cnt)
#define _PR_IOQ_POLLFDS(_cpu)       ((_cpu)->md.md_unix.ioq_pollfds)
#define _PR_IOQ_POLLFDS_SIZE(_cpu)  ((_cpu)->md.md_unix.ioq_pollfds_size)

#define _PR_IOQ_MIN_POLLFDS_SIZE(_cpu)  32

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

#define _MD_EARLY_INIT              _MD_EarlyInit
#define _MD_FINAL_INIT              _PR_UnixInit
#define _MD_INIT_RUNNING_CPU(cpu)   _MD_unix_init_running_cpu(cpu)
#define _MD_INIT_THREAD             _MD_InitializeThread
#define _MD_EXIT_THREAD(thread)
#define _MD_SUSPEND_THREAD(thread)
#define _MD_RESUME_THREAD(thread)
#define _MD_CLEAN_THREAD(_thread)

#define _MD_INTERVAL_USE_GTOD

#define _MD_SELECT      _select
#define _MD_POLL        _poll

#endif /* nspr_scoos5_defs_h___ */
