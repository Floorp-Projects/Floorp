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

/*
 *------------------------------------------------------------------------
 * File: uxwrap.c
 *
 *     Our wrapped versions of the Unix select() and poll() system calls.
 *
 *------------------------------------------------------------------------
 */

#include "primpl.h"

#if defined(_PR_PTHREADS) || defined(_PR_GLOBAL_THREADS_ONLY)
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

#define COPY_SET(_to, _from, _width)			      \
    PR_BEGIN_MACRO					      \
	memcpy(_to, _from,				      \
	       ((_width + 8*sizeof(int)-1) / (8*sizeof(int))) \
		* sizeof(int)				      \
	       );					      \
    PR_END_MACRO

/* An internal global variable defined in prfile.c */
extern PRIOMethods _pr_fileMethods;


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
#elif defined(AIX4_1)
int wrap_select(unsigned long width, void *rl, void *wl, void *el,
	struct timeval *tv)
#elif (defined(BSDI) && !defined(BSDI_2))
int select(int width, fd_set *rd, fd_set *wr, fd_set *ex,
        const struct timeval *tv)
#else
int select(int width, fd_set *rd, fd_set *wr, fd_set *ex, struct timeval *tv)
#endif
{
    int i;
    int npds;
    void *pollset;
    PRPollDesc *pd;
    PRFileDesc *prfd;
    PRFilePrivate *secret;
    PRIntervalTime timeout;
    int retVal;
#if defined(HPUX9) || defined(AIX4_1)
    fd_set *rd = (fd_set*) rl;
    fd_set *wr = (fd_set*) wl;
    fd_set *ex = (fd_set*) el;
#endif
    fd_set r, w, x;

#if 0
    /*
     * Easy special case: zero timeout.  Simply call the native
     * select() with no fear of blocking.
     */
    if (tv != NULL && tv->tv_sec == 0 && tv->tv_usec == 0) {
#if defined(HPUX9) || defined(AIX4_1)
        return _MD_SELECT(width, rl, wl, el, tv);
#else
        return _MD_SELECT(width, rd, wr, ex, tv);
#endif
    }
#endif

	if (!_pr_initialized)
		_PR_ImplicitInitialization();
		
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

    if (rd) { COPY_SET(&r, rd, width); }
    if (wr) { COPY_SET(&w, wr, width); }
    if (ex) { COPY_SET(&x, ex, width); }

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
    pollset = PR_CALLOC(width *
        (sizeof(PRPollDesc) + sizeof(PRFileDesc) + sizeof(PRFilePrivate)));
    if (!pollset) {
        errno = ENOMEM;
        return -1;
    }
    pd = (PRPollDesc*)pollset;
    prfd = (PRFileDesc*)(&pd[width]);
    secret = (PRFilePrivate*)(&prfd[width]);

    for (npds = 0, i = 0; i < width; i++) {
	    int in_flags = 0;
	    if (rd && FD_ISSET(i, &r)) {
	        in_flags |= PR_POLL_READ;
	    }
	    if (wr && FD_ISSET(i, &w)) {
	        in_flags |= PR_POLL_WRITE;
	    }
	    if (ex && FD_ISSET(i, &x)) {
	        in_flags |= PR_POLL_EXCEPT;
	    }
	    if (in_flags) {
	        prfd[npds].secret = &secret[npds];
	        prfd[npds].secret->state = _PR_FILEDESC_OPEN;
	        prfd[npds].secret->md.osfd = i;
	        prfd[npds].methods = &_pr_fileMethods;

	        pd[npds].fd = &prfd[npds];
	        pd[npds].in_flags = in_flags;
	        pd[npds].out_flags = 0;
            npds += 1;
	    }
    }

  /* see comments in ns/cmd/xfe/mozilla.c (look for "PR_XGetXtHackFD") */
    {
	
     int needToLockXAgain;
 
     needToLockXAgain = 0;
     if (rd && (_pr_xt_hack_fd != -1) &&
 		FD_ISSET(_pr_xt_hack_fd, &r) && PR_XIsLocked() &&
		(!_pr_xt_hack_okayToReleaseXLock || _pr_xt_hack_okayToReleaseXLock())) {
 	PR_XUnlock();
 	needToLockXAgain = 1;
     }

    /* This is the potentially blocking step */
    retVal = PR_Poll(pd, npds, timeout);

     if (needToLockXAgain) {
 	PR_XLock();
     }
   }

    if (retVal > 0)
    {
        /* Compute select results */
        if (rd) ZAP_SET(rd, width);
        if (wr) ZAP_SET(wr, width);
        if (ex) ZAP_SET(ex, width);

        /*
         * The return value can be either the number of ready file
         * descriptors or the number of set bits in the three fd_set's.
         */
        retVal = 0;  /* we're going to recompute */
        for (i = 0; i < npds; ++i, pd++)
        {
	        if (pd->out_flags) {
	            int nbits = 0;  /* The number of set bits on for this fd */

	            if (pd->out_flags & PR_POLL_NVAL) {
		            errno = EBADF;
		            PR_LOG(_pr_io_lm, PR_LOG_ERROR,
		                   ("select returns EBADF for %d", pd->fd));
		            retVal = -1;
		            break;
	            }
	            if (rd && (pd->out_flags & PR_POLL_READ)) {
		            FD_SET(pd->fd->secret->md.osfd, rd);
		            nbits++;
	            }
	                if (wr && (pd->out_flags & PR_POLL_WRITE)) {
		            FD_SET(pd->fd->secret->md.osfd, wr);
		            nbits++;
	            }
	                if (ex && (pd->out_flags & PR_POLL_EXCEPT)) {
		            FD_SET(pd->fd->secret->md.osfd, ex);
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
    PR_DELETE(pollset);

    return retVal;
}

/*
 * Linux, BSDI, and FreeBSD don't have poll()
 */

#if !defined(LINUX) && !defined(FREEBSD) && !defined(BSDI)

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

#if defined(AIX4_1)
int wrap_poll(void *listptr, unsigned long nfds, long timeout)
#elif (defined(AIX) && !defined(AIX4_1))
int poll(void *listptr, unsigned long nfds, long timeout)
#elif defined(OSF1) || (defined(HPUX) && !defined(HPUX9))
int poll(struct pollfd filedes[], unsigned int nfds, int timeout)
#elif defined(HPUX9)
int poll(struct pollfd filedes[], int nfds, int timeout)
#else
int poll(struct pollfd *filedes, unsigned long nfds, int timeout)
#endif
{
#ifdef AIX
    struct pollfd *filedes = (struct pollfd *) listptr;
#endif
    void *pollset;
    PRPollDesc *pd;
    PRFileDesc *prfd;
    PRFilePrivate *secret;
    int i;
    PRUint32 ticks;
    PRInt32 retVal;

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
    	retVal = _MD_POLL(filedes, nfds, timeout);
	return(retVal);
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
    } else if (timeout == 0) {
	ticks = PR_INTERVAL_NO_WAIT;
    } else {
        ticks = PR_MillisecondsToInterval(timeout);
    }

    /* Check for no descriptor case (just do a timeout) */
    if (nfds == 0) {
	PR_Sleep(ticks);
	return 0;
    }

    pollset = PR_CALLOC(nfds *
        (sizeof(PRPollDesc) + sizeof(PRFileDesc) + sizeof(PRFilePrivate)));
    if (!pollset) {
        errno = EAGAIN;
        return -1;
    }
    pd = (PRPollDesc*)pollset;
    prfd = (PRFileDesc*)(&pd[nfds]);
    secret = (PRFilePrivate*)(&prfd[nfds]);

    for (i = 0; i < nfds; i++) {
        prfd[i].secret = &secret[i];
        prfd[i].secret->state = _PR_FILEDESC_OPEN;
        prfd[i].secret->md.osfd = filedes[i].fd;
        prfd[i].methods = &_pr_fileMethods;

        pd[i].fd = &prfd[i];
        pd[i].out_flags = 0;
        
	/*
	 * poll() ignores negative fd's.  We emulate this behavior
	 * by making sure the in_flags for a negative fd is zero.
	 */
	if (filedes[i].fd < 0) {
	    pd[i].in_flags = 0;
	    continue;
	}
#ifdef _PR_USE_POLL
	pd[i].in_flags = filedes[i].events;
#else
	/*
	 * Map the native poll flags to nspr20 poll flags.
	 *     POLLIN, POLLRDNORM  ===> PR_POLL_READ
	 *     POLLOUT, POLLWRNORM ===> PR_POLL_WRITE
	 *     POLLPRI, POLLRDBAND ===> PR_POLL_EXCEPT
	 *     POLLNORM, POLLWRBAND (and POLLMSG on some platforms)
	 *     are ignored.
	 *
	 * The output events POLLERR and POLLHUP are never turned on.
	 * POLLNVAL may be turned on.
	 */
	pd[i].in_flags = 0;
	if (filedes[i].events & (POLLIN
#ifdef POLLRDNORM
		| POLLRDNORM
#endif
		)) {
	    pd[i].in_flags |= PR_POLL_READ;
	}
	if (filedes[i].events & (POLLOUT
#ifdef POLLWRNORM
		| POLLWRNORM
#endif
		)) {
            pd[i].in_flags |= PR_POLL_WRITE;
        }
	if (filedes[i].events & (POLLPRI
#ifdef POLLRDBAND
		| POLLRDBAND
#endif
		)) {
	    pd[i].in_flags |= PR_POLL_EXCEPT;
	}
#endif  /* _PR_USE_POLL */
    }

    retVal = PR_Poll(pd, nfds, ticks);

    if (retVal > 0) {
	/* Set the revents bitmasks */
        for (i = 0; i < nfds; i++) {
	    PR_ASSERT(filedes[i].fd >= 0 || pd[i].in_flags == 0);
	    if (filedes[i].fd < 0) {
		continue;  /* skip negative fd's */
	    }
#ifdef _PR_USE_POLL
	    filedes[i].revents = pd[i].out_flags;
#else
	    filedes[i].revents = 0;
            if (0 == pd[i].out_flags) {
                continue;
            }
	    if (pd[i].out_flags & PR_POLL_READ) {
		if (filedes[i].events & POLLIN)
	            filedes[i].revents |= POLLIN;
#ifdef POLLRDNORM
		if (filedes[i].events & POLLRDNORM)
		    filedes[i].revents |= POLLRDNORM;
#endif
	    }
	    if (pd[i].out_flags & PR_POLL_WRITE) {
		if (filedes[i].events & POLLOUT)
	            filedes[i].revents |= POLLOUT;
#ifdef POLLWRNORM
		if (filedes[i].events & POLLWRNORM)
		    filedes[i].revents |= POLLWRNORM;
#endif
	    }
	    if (pd[i].out_flags & PR_POLL_EXCEPT) {
		if (filedes[i].events & POLLPRI)
	            filedes[i].revents |= POLLPRI;
#ifdef POLLRDBAND
		if (filedes[i].events & POLLRDBAND)
		    filedes[i].revents |= POLLRDBAND;
#endif
	    }
	    if (pd[i].out_flags & PR_POLL_ERR) {
		filedes[i].revents |= POLLERR;
	    }
	    if (pd[i].out_flags & PR_POLL_NVAL) {
	        filedes[i].revents |= POLLNVAL;
	    }
#endif  /* _PR_USE_POLL */
        }
    }

    PR_DELETE(pollset);

    return retVal;
}

#endif  /* !defined(LINUX) */

#endif  /* defined(_PR_PTHREADS) || defined(_PR_GLOBAL_THREADS_ONLY) */

/* uxwrap.c */

