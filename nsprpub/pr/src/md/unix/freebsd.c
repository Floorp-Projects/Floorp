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

#include "primpl.h"

#include <signal.h>

void _MD_EarlyInit(void)
{
    /*
     * Ignore FPE because coercion of a NaN to an int causes SIGFPE
     * to be raised.
     */
    struct sigaction act;

    act.sa_handler = SIG_IGN;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART;
    sigaction(SIGFPE, &act, 0);
}

PRWord *_MD_HomeGCRegisters(PRThread *t, int isCurrent, int *np)
{
#ifndef _PR_PTHREADS
    if (isCurrent) {
	(void) sigsetjmp(CONTEXT(t), 1);
    }
    *np = sizeof(CONTEXT(t)) / sizeof(PRWord);
    return (PRWord *) CONTEXT(t);
#else
	*np = 0;
	return NULL;
#endif
}

#ifndef _PR_PTHREADS
void
_MD_SET_PRIORITY(_MDThread *thread, PRUintn newPri)
{
    return;
}

PRStatus
_MD_InitializeThread(PRThread *thread)
{
	return PR_SUCCESS;
}

PRStatus
_MD_WAIT(PRThread *thread, PRIntervalTime ticks)
{
    PR_ASSERT(!(thread->flags & _PR_GLOBAL_SCOPE));
    _PR_MD_SWITCH_CONTEXT(thread);
    return PR_SUCCESS;
}

PRStatus
_MD_WAKEUP_WAITER(PRThread *thread)
{
    if (thread) {
	PR_ASSERT(!(thread->flags & _PR_GLOBAL_SCOPE));
    }
    return PR_SUCCESS;
}

/* These functions should not be called for OSF1 */
void
_MD_YIELD(void)
{
    PR_NOT_REACHED("_MD_YIELD should not be called for OSF1.");
}

PRStatus
_MD_CREATE_THREAD(
    PRThread *thread,
    void (*start) (void *),
    PRThreadPriority priority,
    PRThreadScope scope,
    PRThreadState state,
    PRUint32 stackSize)
{
    PR_NOT_REACHED("_MD_CREATE_THREAD should not be called for OSF1.");
	return PR_FAILURE;
}
#endif /* ! _PR_PTHREADS */

#if defined(_PR_NEED_FAKE_POLL)

#include <fcntl.h>

int poll(struct pollfd *filedes, unsigned long nfds, int timeout)
{
    int i;
    int rv;
    int maxfd;
    fd_set rd, wr, ex;
    struct timeval tv, *tvp;

    if (timeout < 0 && timeout != -1) {
	errno = EINVAL;
	return -1;
    }

    if (timeout == -1) {
        tvp = NULL;
    } else {
	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout % 1000) * 1000;
        tvp = &tv;
    }

    maxfd = -1;
    FD_ZERO(&rd);
    FD_ZERO(&wr);
    FD_ZERO(&ex);

    for (i = 0; i < nfds; i++) {
	int osfd = filedes[i].fd;
	int events = filedes[i].events;
	PRBool fdHasEvent = PR_FALSE;

	if (osfd < 0) {
            continue;  /* Skip this osfd. */
	}

	/*
	 * Map the native poll flags to nspr poll flags.
	 *     POLLIN, POLLRDNORM  ===> PR_POLL_READ
	 *     POLLOUT, POLLWRNORM ===> PR_POLL_WRITE
	 *     POLLPRI, POLLRDBAND ===> PR_POLL_EXCEPTION
	 *     POLLNORM, POLLWRBAND (and POLLMSG on some platforms)
	 *     are ignored.
	 *
	 * The output events POLLERR and POLLHUP are never turned on.
	 * POLLNVAL may be turned on.
	 */

	if (events & (POLLIN | POLLRDNORM)) {
	    FD_SET(osfd, &rd);
	    fdHasEvent = PR_TRUE;
	}
	if (events & (POLLOUT | POLLWRNORM)) {
	    FD_SET(osfd, &wr);
	    fdHasEvent = PR_TRUE;
	}
	if (events & (POLLPRI | POLLRDBAND)) {
	    FD_SET(osfd, &ex);
	    fdHasEvent = PR_TRUE;
	}
	if (fdHasEvent && osfd > maxfd) {
	    maxfd = osfd;
	}
    }

    rv = select(maxfd + 1, &rd, &wr, &ex, tvp);

    /* Compute poll results */
    if (rv > 0) {
	rv = 0;
        for (i = 0; i < nfds; i++) {
	    PRBool fdHasEvent = PR_FALSE;

	    filedes[i].revents = 0;
            if (filedes[i].fd < 0) {
                continue;
            }
	    if (FD_ISSET(filedes[i].fd, &rd)) {
		if (filedes[i].events & POLLIN) {
		    filedes[i].revents |= POLLIN;
		}
		if (filedes[i].events & POLLRDNORM) {
		    filedes[i].revents |= POLLRDNORM;
		}
		fdHasEvent = PR_TRUE;
	    }
	    if (FD_ISSET(filedes[i].fd, &wr)) {
		if (filedes[i].events & POLLOUT) {
		    filedes[i].revents |= POLLOUT;
		}
		if (filedes[i].events & POLLWRNORM) {
		    filedes[i].revents |= POLLWRNORM;
		}
		fdHasEvent = PR_TRUE;
	    }
	    if (FD_ISSET(filedes[i].fd, &ex)) {
		if (filedes[i].events & POLLPRI) {
		    filedes[i].revents |= POLLPRI;
		}
		if (filedes[i].events & POLLRDBAND) {
		    filedes[i].revents |= POLLRDBAND;
		}
		fdHasEvent = PR_TRUE;
	    }
	    if (fdHasEvent) {
		rv++;
            }
        }
	PR_ASSERT(rv > 0);
    } else if (rv == -1 && errno == EBADF) {
	rv = 0;
        for (i = 0; i < nfds; i++) {
	    filedes[i].revents = 0;
            if (filedes[i].fd < 0) {
                continue;
            }
	    if (fcntl(filedes[i].fd, F_GETFL, 0) == -1) {
		filedes[i].revents = POLLNVAL;
		rv++;
	    }
        }
	PR_ASSERT(rv > 0);
    }
    PR_ASSERT(-1 != timeout || rv != 0);

    return rv;
}
#endif /* _PR_NEED_FAKE_POLL */
