/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/*
 * This file implements _PR_MD_PR_POLL for OS/2.
 */

#ifdef XP_OS2_EMX
 #include <sys/time.h> /* For timeval. */
#endif

#include "primpl.h"

#ifndef BSD_SELECT
/* Utility functions called when using OS/2 select */

PRBool IsSocketSet( PRInt32 osfd, int* socks, int start, int count )
{
  int i;
  PRBool isSet = PR_FALSE;

  for( i = start; i < start+count; i++ )
  {
    if( socks[i] == osfd )
      isSet = PR_TRUE;
  }
  
  return isSet; 
}
#endif

PRInt32 _PR_MD_PR_POLL(PRPollDesc *pds, PRIntn npds, PRIntervalTime timeout)
{
#ifdef BSD_SELECT
    fd_set rd, wt, ex;
#else
    int rd, wt, ex;
    int* socks;
    unsigned long msecs;
    int i, j;
#endif
    PRFileDesc *bottom;
    PRPollDesc *pd, *epd;
    PRInt32 maxfd = -1, ready, err;
    PRIntervalTime remaining, elapsed, start;

#ifdef BSD_SELECT
    struct timeval tv, *tvp = NULL;

    FD_ZERO(&rd);
    FD_ZERO(&wt);
    FD_ZERO(&ex);
#else
    rd = 0;
    wt = 0;
    ex = 0;
    socks = (int) PR_MALLOC( npds * 3 * sizeof(int) );
    
    if (!socks)
    {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        return -1;
    }
#endif

    ready = 0;
    for (pd = pds, epd = pd + npds; pd < epd; pd++)
    {
        PRInt16 in_flags_read = 0, in_flags_write = 0;
        PRInt16 out_flags_read = 0, out_flags_write = 0;

        if ((NULL != pd->fd) && (0 != pd->in_flags))
        {
            if (pd->in_flags & PR_POLL_READ)
            {
                in_flags_read = (pd->fd->methods->poll)(
                    pd->fd, pd->in_flags & ~PR_POLL_WRITE, &out_flags_read);
            }
            if (pd->in_flags & PR_POLL_WRITE)
            {
                in_flags_write = (pd->fd->methods->poll)(
                    pd->fd, pd->in_flags & ~PR_POLL_READ, &out_flags_write);
            }
            if ((0 != (in_flags_read & out_flags_read)) ||
                (0 != (in_flags_write & out_flags_write)))
            {
                /* this one's ready right now */
                if (0 == ready)
                {
                    /*
                     * We will have to return without calling the
                     * system poll/select function.  So zero the
                     * out_flags fields of all the poll descriptors
                     * before this one.
                     */
                    PRPollDesc *prev;
                    for (prev = pds; prev < pd; prev++)
                    {
                        prev->out_flags = 0;
                    }
                }
                ready += 1;
                pd->out_flags = out_flags_read | out_flags_write;
            }
            else
            {
                pd->out_flags = 0;  /* pre-condition */

                /* make sure this is an NSPR supported stack */
                bottom = PR_GetIdentitiesLayer(pd->fd, PR_NSPR_IO_LAYER);
                PR_ASSERT(NULL != bottom);  /* what to do about that? */
                if ((NULL != bottom) &&
                    (_PR_FILEDESC_OPEN == bottom->secret->state))
                {
                    if (0 == ready)
                    {
                        PRInt32 osfd = bottom->secret->md.osfd;
                        if (osfd > maxfd) 
                            maxfd = osfd;
                        if (in_flags_read & PR_POLL_READ)
                        {
                            pd->out_flags |= _PR_POLL_READ_SYS_READ;
#ifdef BSD_SELECT
                            FD_SET(osfd, &rd);
#else
                            socks[rd] = osfd;
                            rd++;              
#endif
                        }
                        if (in_flags_read & PR_POLL_WRITE)
                        {
                            pd->out_flags |= _PR_POLL_READ_SYS_WRITE;
#ifdef BSD_SELECT
                            FD_SET(osfd, &wt);
#else
                            socks[npds+wt] = osfd;
                            wt++;              
#endif
                        }
                        if (in_flags_write & PR_POLL_READ)
                        {
                            pd->out_flags |= _PR_POLL_WRITE_SYS_READ;
#ifdef BSD_SELECT
                            FD_SET(osfd, &rd);
#else
                            socks[rd] = osfd;
                            rd++;              
#endif
                        }
                        if (in_flags_write & PR_POLL_WRITE)
                        {
                            pd->out_flags |= _PR_POLL_WRITE_SYS_WRITE;
#ifdef BSD_SELECT
                            FD_SET(osfd, &wt);
#else
                            socks[npds+wt] = osfd;
                            wt++;              
#endif
                        }
                        if (pd->in_flags & PR_POLL_EXCEPT)
                        {
#ifdef BSD_SELECT
                            FD_SET(osfd, &ex);
#else
                            socks[npds*2+ex] = osfd;
                            ex++;
#endif
                        }
                    }
                }
                else
                {
                    if (0 == ready)
                    {
                        PRPollDesc *prev;
                        for (prev = pds; prev < pd; prev++)
                        {
                            prev->out_flags = 0;
                        }
                    }
                    ready += 1;  /* this will cause an abrupt return */
                    pd->out_flags = PR_POLL_NVAL;  /* bogii */
                }
            }
        }
        else
        {
            pd->out_flags = 0;
        }
    }

    if (0 != ready)
    {
#ifndef BSD_SELECT
        free(socks);
#endif
        return ready;  /* no need to block */
    }

    remaining = timeout;
    start = PR_IntervalNow();

retry:
#ifdef BSD_SELECT
    if (timeout != PR_INTERVAL_NO_TIMEOUT)
    {
        PRInt32 ticksPerSecond = PR_TicksPerSecond();
        tv.tv_sec = remaining / ticksPerSecond;
        tv.tv_usec = remaining - (ticksPerSecond * tv.tv_sec);
        tv.tv_usec = (PR_USEC_PER_SEC * tv.tv_usec) / ticksPerSecond;
        tvp = &tv;
    }

    ready = _MD_SELECT(maxfd + 1, &rd, &wt, &ex, tvp);
#else
    switch (timeout)
    {
        case PR_INTERVAL_NO_WAIT:
            msecs = 0;
            break;
        case PR_INTERVAL_NO_TIMEOUT:
            msecs = -1;
            break;
        default:
            msecs = PR_IntervalToMilliseconds(remaining);
    }

     /* compact array */
    for( i = rd, j = npds; j < npds+wt; i++,j++ )
        socks[i] = socks[j];
    for( i = rd+wt, j = npds*2; j < npds*2+ex; i++,j++ )
        socks[i] = socks[j];
    
    ready = _MD_SELECT(socks, rd, wt, ex, msecs);
#endif

    if (ready == -1 && errno == EINTR)
    {
        if (timeout == PR_INTERVAL_NO_TIMEOUT)
            goto retry;
        else
        {
            elapsed = (PRIntervalTime) (PR_IntervalNow() - start);
            if (elapsed > timeout)
                ready = 0;  /* timed out */
            else
            {
                remaining = timeout - elapsed;
                goto retry;
            }
        }
    }

    /*
    ** Now to unravel the select sets back into the client's poll
    ** descriptor list. Is this possibly an area for pissing away
    ** a few cycles or what?
    */
    if (ready > 0)
    {
        ready = 0;
        for (pd = pds, epd = pd + npds; pd < epd; pd++)
        {
            PRInt16 out_flags = 0;
            if ((NULL != pd->fd) && (0 != pd->in_flags))
            {
                PRInt32 osfd;
                bottom = PR_GetIdentitiesLayer(pd->fd, PR_NSPR_IO_LAYER);
                PR_ASSERT(NULL != bottom);

                osfd = bottom->secret->md.osfd;

#ifdef BSD_SELECT
                if (FD_ISSET(osfd, &rd))
#else
                if( IsSocketSet(osfd, socks, 0, rd) )        
#endif
                {
                    if (pd->out_flags & _PR_POLL_READ_SYS_READ)
                        out_flags |= PR_POLL_READ;
                    if (pd->out_flags & _PR_POLL_WRITE_SYS_READ)
                        out_flags |= PR_POLL_WRITE;
                } 

#ifdef BSD_SELECT
                if (FD_ISSET(osfd, &wt))
#else
                if( IsSocketSet(osfd, socks, rd, wt) )        
#endif
                {
                    if (pd->out_flags & _PR_POLL_READ_SYS_WRITE)
                        out_flags |= PR_POLL_READ;
                    if (pd->out_flags & _PR_POLL_WRITE_SYS_WRITE)
                        out_flags |= PR_POLL_WRITE;
                } 

#ifdef BSD_SELECT
                if (FD_ISSET(osfd, &ex))
#else
                if( IsSocketSet(osfd, socks, rd+wt, ex) )        
#endif
                {
                    out_flags |= PR_POLL_EXCEPT;
                }
            }
            pd->out_flags = out_flags;
            if (out_flags) ready++;
        }
        PR_ASSERT(ready > 0);
    }
    else if (ready < 0)
    {
        err = _MD_ERRNO();
        if (err == EBADF)
        {
            /* Find the bad fds */
            int optval;
            int optlen = sizeof(optval);
            ready = 0;
            for (pd = pds, epd = pd + npds; pd < epd; pd++)
            {
                pd->out_flags = 0;
                if ((NULL != pd->fd) && (0 != pd->in_flags))
                {
                    bottom = PR_GetIdentitiesLayer(pd->fd, PR_NSPR_IO_LAYER);
                    if (getsockopt(bottom->secret->md.osfd, SOL_SOCKET,
                        SO_TYPE, (char *) &optval, &optlen) == -1)
                    {
                        PR_ASSERT(sock_errno() == ENOTSOCK);
                        if (sock_errno() == ENOTSOCK)
                        {
                            pd->out_flags = PR_POLL_NVAL;
                            ready++;
                        }
                    }
                }
            }
            PR_ASSERT(ready > 0);
        }
        else
            _PR_MD_MAP_SELECT_ERROR(err);
    }

#ifndef BSD_SELECT
    free(socks);
#endif
    return ready;
}

#ifdef XP_OS2_EMX
HMTX thread_select_mutex = 0;	/* because EMX's select is not thread safe - duh! */

typedef struct _thread_select_st {
	int		nfds;
	int		isrdfds;
	struct _fd_set *readfds;
	int		iswrfds;
	struct _fd_set *writefds;
	int		isexfds;
	struct _fd_set *exceptfds;
	int		istimeout;
	struct timeval	timeout;
	volatile HEV	event;
	int		result;
	int		select_errno;
	volatile int	done;
} *pthread_select_t;
	
void _thread_select(void * arg)
{
	pthread_select_t	self = arg;
	int			result, chkstdin;
	struct _fd_set		readfds;
	struct _fd_set		writefds;
	struct _fd_set		exceptfds;
	HEV			event = self->event;

	chkstdin = (self->isrdfds && FD_ISSET(0,self->readfds))?1:0;

	do {
		struct timeval	timeout = {0L,0L};


		if (self->isrdfds) readfds = *self->readfds;
		if (self->iswrfds) writefds = *self->writefds;
		if (self->isexfds) exceptfds = *self->exceptfds;
		
		if (chkstdin) FD_CLR(0,&readfds);

		if (!thread_select_mutex) 
			DosCreateMutexSem(NULL,&thread_select_mutex,0,1);
		else
			DosRequestMutexSem(thread_select_mutex,SEM_INDEFINITE_WAIT);
		result = select(
			self->nfds, 
			self->isrdfds?&readfds:NULL,
			self->iswrfds?&writefds:NULL,
			self->isexfds?&exceptfds:NULL,
			&timeout);
		DosReleaseMutexSem(thread_select_mutex);

		if (chkstdin) {
			int charcount = 0, res;
			res = ioctl(0,FIONREAD,&charcount);
			if (res==0 && charcount>0) FD_SET(0,&readfds);
		}
				
		if (result>0) {
			self->done++;
			if (self->isrdfds) *self->readfds = readfds;
			if (self->iswrfds) *self->writefds = writefds;
			if (self->isexfds) *self->exceptfds = exceptfds;
		} else
		if (result) self->done++;
		else DosSleep(1);

	} while (self->event!=0 && self->done==0);

	if (self->event) {
		self->select_errno = (result < 0)?errno:0;
		self->result = result;
		self->done = 3;
		DosPostEventSem(event);
	} else {
		self->done = 3;
		free(self);
	}

}

PRInt32
_MD_SELECT(int nfds, fd_set *readfds, fd_set *writefds,
                  fd_set *exceptfds, struct timeval *timeout)
{
	pthread_select_t sel;
	HEV		ev = 0;
	HTIMER		timer = 0;
	int		result = 0;
	APIRET		rc;
	unsigned long	msecs = SEM_INDEFINITE_WAIT;

	if (timeout) {
		if (timeout->tv_sec != 0 || timeout->tv_usec != 0) 
			msecs = (timeout->tv_sec * 1000L) + (timeout->tv_usec / 1000L);
		else
			msecs = SEM_IMMEDIATE_RETURN;
	};

	if (!(sel = (pthread_select_t) malloc(sizeof(struct _thread_select_st)))) {
		result = -1;
		errno = ENOMEM;
	} else {
		sel->nfds = nfds;
		sel->isrdfds = readfds?1:0;
		if (sel->isrdfds) sel->readfds = readfds;
		sel->iswrfds = writefds?1:0;
		if (sel->iswrfds) sel->writefds = writefds;
		sel->isexfds = exceptfds?1:0;
		if (sel->isexfds) sel->exceptfds = exceptfds;
		sel->istimeout = timeout?1:0;
		if (sel->istimeout) sel->timeout = *timeout;
	
		rc = DosCreateEventSem(NULL,&ev,0,FALSE);

		sel->event = ev;
		if (msecs == SEM_IMMEDIATE_RETURN)
			sel->done = 1;
		else
			sel->done = 0;

		if (_beginthread(_thread_select,NULL,65536,(void *)sel) == -1) {
			result = -1; sel->event = 0;
			DosCloseEventSem(ev);
		} else {
			rc = DosWaitEventSem(ev,msecs);
			if ((!sel->done) && (msecs != SEM_IMMEDIATE_RETURN)) {	/* Interrupted by other thread or timeout */
				sel->event = 0;
				result = 0;
				errno = ETIMEDOUT;
				
			} else {
				while (sel->done && sel->done != 3) {
					DosSleep(1);
				}
				sel->event = 0;
				result = sel->result;
				if (sel->select_errno) errno = sel->select_errno;
				free(sel);
			}
			rc = DosCloseEventSem(ev);
		}
	}

	return (result);
}

#endif
