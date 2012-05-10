/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *------------------------------------------------------------------------
 * File: uxwrap.c
 *
 *     Our wrapped versions of the Unix select() and poll() system calls.
 *
 *------------------------------------------------------------------------
 */

#include "primpl.h"

#if defined(_PR_PTHREADS) || defined(_PR_GLOBAL_THREADS_ONLY) || defined(QNX)
/* Do not wrap select() and poll(). */
#else  /* defined(_PR_PTHREADS) || defined(_PR_GLOBAL_THREADS_ONLY) */
/* The include files for select() */
#ifdef IRIX
#include <unistd.h>
#include <bstring.h>
#endif

#include <string.h>
#include <sys/types.h>
#include <sys/time.h>

#define ZAP_SET(_to, _width)				      \
    PR_BEGIN_MACRO					      \
	memset(_to, 0,					      \
	       ((_width + 8*sizeof(int)-1) / (8*sizeof(int))) \
		* sizeof(int)				      \
	       );					      \
    PR_END_MACRO

/* see comments in ns/cmd/xfe/mozilla.c (look for "PR_XGetXtHackFD") */
static int _pr_xt_hack_fd = -1;
 
int PR_XGetXtHackFD(void)
{
    int fds[2];
 
    if (_pr_xt_hack_fd == -1) {
        if (!pipe(fds)) {
            _pr_xt_hack_fd = fds[0];
        }
    }
    return _pr_xt_hack_fd;
}

static int (*_pr_xt_hack_okayToReleaseXLock)(void) = 0;

void PR_SetXtHackOkayToReleaseXLockFn(int (*fn)(void))
{
    _pr_xt_hack_okayToReleaseXLock = fn; 
}


/*
 *-----------------------------------------------------------------------
 *  select() --
 *
 *    Wrap up the select system call so that we can deschedule
 *    a thread that tries to wait for i/o.
 *
 *-----------------------------------------------------------------------
 */

#if defined(HPUX9)
int select(size_t width, int *rl, int *wl, int *el, const struct timeval *tv)
#elif defined(NEXTSTEP)
int wrap_select(int width, fd_set *rd, fd_set *wr, fd_set *ex,
        const struct timeval *tv)
#elif defined(AIX_RENAME_SELECT)
int wrap_select(unsigned long width, void *rl, void *wl, void *el,
        struct timeval *tv)
#elif defined(_PR_SELECT_CONST_TIMEVAL)
int select(int width, fd_set *rd, fd_set *wr, fd_set *ex,
        const struct timeval *tv)
#else
int select(int width, fd_set *rd, fd_set *wr, fd_set *ex, struct timeval *tv)
#endif
{
    int osfd;
    _PRUnixPollDesc *unixpds, *unixpd, *eunixpd;
    PRInt32 pdcnt;
    PRIntervalTime timeout;
    int retVal;
#if defined(HPUX9) || defined(AIX_RENAME_SELECT)
    fd_set *rd = (fd_set*) rl;
    fd_set *wr = (fd_set*) wl;
    fd_set *ex = (fd_set*) el;
#endif

#if 0
    /*
     * Easy special case: zero timeout.  Simply call the native
     * select() with no fear of blocking.
     */
    if (tv != NULL && tv->tv_sec == 0 && tv->tv_usec == 0) {
#if defined(HPUX9) || defined(AIX_RENAME_SELECT)
        return _MD_SELECT(width, rl, wl, el, tv);
#else
        return _MD_SELECT(width, rd, wr, ex, tv);
#endif
    }
#endif

    if (!_pr_initialized) {
        _PR_ImplicitInitialization();
    }
		
#ifndef _PR_LOCAL_THREADS_ONLY
    if (_PR_IS_NATIVE_THREAD(_PR_MD_CURRENT_THREAD())) {
        return _MD_SELECT(width, rd, wr, ex, tv);	
    }
#endif

    if (width < 0 || width > FD_SETSIZE) {
        errno = EINVAL;
        return -1;
    }

    /* Compute timeout */
    if (tv) {
        /*
         * These acceptable ranges for t_sec and t_usec are taken
         * from the select() man pages.
         */
        if (tv->tv_sec < 0 || tv->tv_sec > 100000000
                || tv->tv_usec < 0 || tv->tv_usec >= 1000000) {
            errno = EINVAL;
            return -1;
        }

        /* Convert microseconds to ticks */
        timeout = PR_MicrosecondsToInterval(1000000*tv->tv_sec + tv->tv_usec);
    } else {
        /* tv being a NULL pointer means blocking indefinitely */
        timeout = PR_INTERVAL_NO_TIMEOUT;
    }

    /* Check for no descriptors case (just doing a timeout) */
    if ((!rd && !wr && !ex) || !width) {
        PR_Sleep(timeout);
        return 0;
    }

    /*
     * Set up for PR_Poll().  The PRPollDesc array is allocated
     * dynamically.  If this turns out to have high performance
     * penalty, one can change to use a large PRPollDesc array
     * on the stack, and allocate dynamically only when it turns
     * out to be not large enough.
     *
     * I allocate an array of size 'width', which is the maximum
     * number of fds we may need to poll.
     */
    unixpds = (_PRUnixPollDesc *) PR_CALLOC(width * sizeof(_PRUnixPollDesc));
    if (!unixpds) {
        errno = ENOMEM;
        return -1;
    }

    pdcnt = 0;
    unixpd = unixpds;
    for (osfd = 0; osfd < width; osfd++) {
        int in_flags = 0;
        if (rd && FD_ISSET(osfd, rd)) {
            in_flags |= _PR_UNIX_POLL_READ;
        }
        if (wr && FD_ISSET(osfd, wr)) {
            in_flags |= _PR_UNIX_POLL_WRITE;
        }
        if (ex && FD_ISSET(osfd, ex)) {
            in_flags |= _PR_UNIX_POLL_EXCEPT;
        }
        if (in_flags) {
            unixpd->osfd = osfd;
            unixpd->in_flags = in_flags;
            unixpd->out_flags = 0;
            unixpd++;
            pdcnt++;
        }
    }

    /*
     * see comments in mozilla/cmd/xfe/mozilla.c (look for
     * "PR_XGetXtHackFD")
     */
   {
     int needToLockXAgain;
 
     needToLockXAgain = 0;
     if (rd && (_pr_xt_hack_fd != -1)
             && FD_ISSET(_pr_xt_hack_fd, rd) && PR_XIsLocked()
             && (!_pr_xt_hack_okayToReleaseXLock
             || _pr_xt_hack_okayToReleaseXLock())) {
         PR_XUnlock();
         needToLockXAgain = 1;
     }

    /* This is the potentially blocking step */
    retVal = _PR_WaitForMultipleFDs(unixpds, pdcnt, timeout);

     if (needToLockXAgain) {
         PR_XLock();
     }
   }

    if (retVal > 0) {
        /* Compute select results */
        if (rd) ZAP_SET(rd, width);
        if (wr) ZAP_SET(wr, width);
        if (ex) ZAP_SET(ex, width);

        /*
         * The return value can be either the number of ready file
         * descriptors or the number of set bits in the three fd_set's.
         */
        retVal = 0;  /* we're going to recompute */
        eunixpd = unixpds + pdcnt;
        for (unixpd = unixpds; unixpd < eunixpd; unixpd++) {
            if (unixpd->out_flags) {
                int nbits = 0;  /* The number of set bits on for this fd */

                if (unixpd->out_flags & _PR_UNIX_POLL_NVAL) {
                    errno = EBADF;
                    PR_LOG(_pr_io_lm, PR_LOG_ERROR,
                            ("select returns EBADF for %d", unixpd->osfd));
                    retVal = -1;
                    break;
                }
                /*
                 * If a socket has a pending error, it is considered
                 * both readable and writable.  (See W. Richard Stevens,
                 * Unix Network Programming, Vol. 1, 2nd Ed., Section 6.3,
                 * pp. 153-154.)  We also consider a socket readable if
                 * it has a hangup condition.
                 */
                if (rd && (unixpd->in_flags & _PR_UNIX_POLL_READ)
                        && (unixpd->out_flags & (_PR_UNIX_POLL_READ
                        | _PR_UNIX_POLL_ERR | _PR_UNIX_POLL_HUP))) {
                    FD_SET(unixpd->osfd, rd);
                    nbits++;
                }
                if (wr && (unixpd->in_flags & _PR_UNIX_POLL_WRITE)
                        && (unixpd->out_flags & (_PR_UNIX_POLL_WRITE
                        | _PR_UNIX_POLL_ERR))) {
                    FD_SET(unixpd->osfd, wr);
                    nbits++;
                }
                if (ex && (unixpd->in_flags & _PR_UNIX_POLL_WRITE)
                        && (unixpd->out_flags & PR_POLL_EXCEPT)) {
                    FD_SET(unixpd->osfd, ex);
                    nbits++;
                }
                PR_ASSERT(nbits > 0);
#if defined(HPUX) || defined(SOLARIS) || defined(SUNOS4) || defined(OSF1) || defined(AIX)
                retVal += nbits;
#else /* IRIX */
                retVal += 1;
#endif
            }
        }
    }

    PR_ASSERT(tv || retVal != 0);
    PR_LOG(_pr_io_lm, PR_LOG_MIN, ("select returns %d", retVal));
    PR_DELETE(unixpds);

    return retVal;
}

/*
 * Redefine poll, when supported on platforms, for local threads
 */

/*
 * I am commenting out the poll() wrapper for Linux for now
 * because it is difficult to define _MD_POLL that works on all
 * Linux varieties.  People reported that glibc 2.0.7 on Debian
 * 2.0 Linux machines doesn't have the __syscall_poll symbol
 * defined.  (WTC 30 Nov. 1998)
 */
#if defined(_PR_POLL_AVAILABLE) && !defined(LINUX)

/*
 *-----------------------------------------------------------------------
 * poll() --
 *
 * RETURN VALUES: 
 *     -1:  fails, errno indicates the error.
 *      0:  timed out, the revents bitmasks are not set.
 *      positive value: the number of file descriptors for which poll()
 *          has set the revents bitmask.
 *
 *-----------------------------------------------------------------------
 */

#include <poll.h>

#if defined(AIX_RENAME_SELECT)
int wrap_poll(void *listptr, unsigned long nfds, long timeout)
#elif (defined(AIX) && !defined(AIX_RENAME_SELECT))
int poll(void *listptr, unsigned long nfds, long timeout)
#elif defined(OSF1) || (defined(HPUX) && !defined(HPUX9))
int poll(struct pollfd filedes[], unsigned int nfds, int timeout)
#elif defined(HPUX9)
int poll(struct pollfd filedes[], int nfds, int timeout)
#elif defined(NETBSD)
int poll(struct pollfd *filedes, nfds_t nfds, int timeout)
#elif defined(OPENBSD)
int poll(struct pollfd filedes[], nfds_t nfds, int timeout)
#elif defined(FREEBSD)
int poll(struct pollfd *filedes, unsigned nfds, int timeout)
#else
int poll(struct pollfd *filedes, unsigned long nfds, int timeout)
#endif
{
#ifdef AIX
    struct pollfd *filedes = (struct pollfd *) listptr;
#endif
    struct pollfd *pfd, *epfd;
    _PRUnixPollDesc *unixpds, *unixpd, *eunixpd;
    PRIntervalTime ticks;
    PRInt32 pdcnt;
    int ready;

    /*
     * Easy special case: zero timeout.  Simply call the native
     * poll() with no fear of blocking.
     */
    if (timeout == 0) {
#if defined(AIX)
        return _MD_POLL(listptr, nfds, timeout);
#else
        return _MD_POLL(filedes, nfds, timeout);
#endif
    }

    if (!_pr_initialized) {
        _PR_ImplicitInitialization();
    }

#ifndef _PR_LOCAL_THREADS_ONLY
    if (_PR_IS_NATIVE_THREAD(_PR_MD_CURRENT_THREAD())) {
    	return _MD_POLL(filedes, nfds, timeout);
    }
#endif

    /* We do not support the pollmsg structures on AIX */
#ifdef AIX
    PR_ASSERT((nfds & 0xff00) == 0);
#endif

    if (timeout < 0 && timeout != -1) {
        errno = EINVAL;
        return -1;
    }

    /* Convert timeout from miliseconds to ticks */
    if (timeout == -1) {
        ticks = PR_INTERVAL_NO_TIMEOUT;
    } else {
        ticks = PR_MillisecondsToInterval(timeout);
    }

    /* Check for no descriptor case (just do a timeout) */
    if (nfds == 0) {
        PR_Sleep(ticks);
        return 0;
    }

    unixpds = (_PRUnixPollDesc *)
            PR_MALLOC(nfds * sizeof(_PRUnixPollDesc));
    if (NULL == unixpds) {
        errno = EAGAIN;
        return -1;
    }

    pdcnt = 0;
    epfd = filedes + nfds;
    unixpd = unixpds;
    for (pfd = filedes; pfd < epfd; pfd++) {
        /*
         * poll() ignores negative fd's.
         */
        if (pfd->fd >= 0) {
            unixpd->osfd = pfd->fd;
#ifdef _PR_USE_POLL
            unixpd->in_flags = pfd->events;
#else
            /*
             * Map the poll events to one of the three that can be
             * represented by the select fd_sets:
             *     POLLIN, POLLRDNORM  ===> readable
             *     POLLOUT, POLLWRNORM ===> writable
             *     POLLPRI, POLLRDBAND ===> exception
             *     POLLNORM, POLLWRBAND (and POLLMSG on some platforms)
             *     are ignored.
             *
             * The output events POLLERR and POLLHUP are never turned on.
             * POLLNVAL may be turned on.
             */
            unixpd->in_flags = 0;
            if (pfd->events & (POLLIN
#ifdef POLLRDNORM
                    | POLLRDNORM
#endif
                    )) {
                unixpd->in_flags |= _PR_UNIX_POLL_READ;
            }
            if (pfd->events & (POLLOUT
#ifdef POLLWRNORM
                    | POLLWRNORM
#endif
                    )) {
                unixpd->in_flags |= _PR_UNIX_POLL_WRITE;
            }
            if (pfd->events & (POLLPRI
#ifdef POLLRDBAND
                    | POLLRDBAND
#endif
                    )) {
                unixpd->in_flags |= PR_POLL_EXCEPT;
            }
#endif  /* _PR_USE_POLL */
            unixpd->out_flags = 0;
            unixpd++;
            pdcnt++;
        }
    }

    ready = _PR_WaitForMultipleFDs(unixpds, pdcnt, ticks);
    if (-1 == ready) {
        if (PR_GetError() == PR_PENDING_INTERRUPT_ERROR) {
            errno = EINTR;  /* XXX we aren't interrupted by a signal, but... */
        } else {
            errno = PR_GetOSError();
        }
    }
    if (ready <= 0) {
        goto done;
    }

    /*
     * Copy the out_flags from the _PRUnixPollDesc structures to the
     * user's pollfd structures and free the allocated memory
     */
    unixpd = unixpds;
    for (pfd = filedes; pfd < epfd; pfd++) {
        pfd->revents = 0;
        if (pfd->fd >= 0) {
#ifdef _PR_USE_POLL
            pfd->revents = unixpd->out_flags;
#else
            if (0 != unixpd->out_flags) {
                if (unixpd->out_flags & _PR_UNIX_POLL_READ) {
                    if (pfd->events & POLLIN) {
                        pfd->revents |= POLLIN;
                    }
#ifdef POLLRDNORM
                    if (pfd->events & POLLRDNORM) {
                        pfd->revents |= POLLRDNORM;
                    }
#endif
                }
                if (unixpd->out_flags & _PR_UNIX_POLL_WRITE) {
                    if (pfd->events & POLLOUT) {
                        pfd->revents |= POLLOUT;
                    }
#ifdef POLLWRNORM
                    if (pfd->events & POLLWRNORM) {
                        pfd->revents |= POLLWRNORM;
                    }
#endif
                }
                if (unixpd->out_flags & _PR_UNIX_POLL_EXCEPT) {
                    if (pfd->events & POLLPRI) {
                        pfd->revents |= POLLPRI;
                    }
#ifdef POLLRDBAND
                    if (pfd->events & POLLRDBAND) {
                        pfd->revents |= POLLRDBAND;
                    }
#endif
                }
                if (unixpd->out_flags & _PR_UNIX_POLL_ERR) {
                    pfd->revents |= POLLERR;
                }
                if (unixpd->out_flags & _PR_UNIX_POLL_NVAL) {
                    pfd->revents |= POLLNVAL;
                }
                if (unixpd->out_flags & _PR_UNIX_POLL_HUP) {
                    pfd->revents |= POLLHUP;
                }
            }
#endif  /* _PR_USE_POLL */
            unixpd++;
        }
    }

done:
    PR_DELETE(unixpds);
    return ready;
}

#endif  /* !defined(LINUX) */

#endif  /* defined(_PR_PTHREADS) || defined(_PR_GLOBAL_THREADS_ONLY) */

/* uxwrap.c */

