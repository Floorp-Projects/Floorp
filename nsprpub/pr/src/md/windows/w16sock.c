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

#include "primpl.h"

static  int winsockNotPresent = 0;

void
_PR_MD_INIT_IO()
{
    int rv;
    
    WORD WSAVersion = 0x0101;
    WSADATA WSAData;

    rv = WSAStartup( WSAVersion, &WSAData );
    if ( rv != 0 )
    {
        _PR_MD_MAP_WSASTARTUP_ERROR(WSAGetLastError());
        winsockNotPresent = 1;
    }
    return;
}

void
_PR_MD_CLEANUP_BEFORE_EXIT(void)
{
    int rv;
    int err;
    
    rv = WSACleanup();
    if ( rv == SOCKET_ERROR )
    {
        err = WSAGetLastError();
        PR_ASSERT(0);
    }
    return;
} /* end _PR_MD_CLEANUP_BEFORE_EXIT() */

/* --- SOCKET IO --------------------------------------------------------- */

PRStatus 
_MD_WindowsGetHostName(char *name, PRUint32 namelen)
{
    PRIntn  rv;
    PRInt32 syserror;
    
    rv = gethostname(name, (PRInt32) namelen);
    if (0 == rv) {
        return PR_SUCCESS;
    }
    syserror = WSAGetLastError();
    PR_ASSERT(WSANOTINITIALISED != syserror);
    _PR_MD_MAP_GETHOSTNAME_ERROR(syserror);
    return PR_FAILURE;
}


PRInt32
_PR_MD_SOCKET(int af, int type, int flags)
{
    SOCKET      sock;
    PRUint32    one = 1;
    PRInt32     rv;
    PRInt32     err;

    if ( winsockNotPresent )
        return( (PRInt32)INVALID_SOCKET );

    sock = socket(af, type, flags);

    if (sock == INVALID_SOCKET ) 
    {
        int rv = GetLastError();
        closesocket(sock);
        _PR_MD_MAP_SOCKET_ERROR(rv);
        return (PRInt32)INVALID_SOCKET;
    }

    /*
    ** Make the socket Non-Blocking
    */
    rv = ioctlsocket( sock, FIONBIO, &one);
    if ( rv != 0 )
    {
        err = WSAGetLastError();
        return -1;
    }

    return (PRInt32)sock;
}


PRInt32
_PR_MD_SOCKETAVAILABLE(PRFileDesc *fd)
{
    PRUint32 result;

    if (ioctlsocket(fd->secret->md.osfd, FIONREAD, &result) < 0) {
        PR_SetError(PR_BAD_DESCRIPTOR_ERROR, WSAGetLastError());
        return -1;
    }
    return result;
}


/*
** _MD_CloseSocket() -- Close a socket
**
*/
PRInt32
_PR_MD_CLOSE_SOCKET(PRInt32 osfd)
{
    PRInt32 rv;

    rv = closesocket((SOCKET) osfd );
    if (rv < 0)
        _PR_MD_MAP_CLOSE_ERROR(WSAGetLastError());

    return rv;
}

PRInt32 _PR_MD_LISTEN(PRFileDesc *fd, PRIntn backlog)
{
    int rv, err;

	rv = listen(fd->secret->md.osfd, backlog);
	if ( rv == SOCKET_ERROR ) {
		_PR_MD_MAP_LISTEN_ERROR(WSAGetLastError());
        return(-1);
	}
	return(rv);
}

PRInt32
_PR_MD_ACCEPT(PRFileDesc *fd, PRNetAddr *addr, PRUint32 *addrlen,
       PRIntervalTime timeout )
{
    PRInt32 osfd = fd->secret->md.osfd;
    PRThread    *me = _PR_MD_CURRENT_THREAD();
    PRInt32     err;
    PRIntn      rv;

    MD_ASSERTINT( *addrlen );    

    while ((rv = (SOCKET)accept(osfd, (struct sockaddr *) addr,
                                        (int *)addrlen)) == INVALID_SOCKET ) {
        err = WSAGetLastError();
		if ( err == WSAEWOULDBLOCK ) {
			if (fd->secret->nonblocking) {
				break;
			}
            if (_PR_WaitForFD(osfd, PR_POLL_READ, timeout) == 0) {
                if ( _PR_PENDING_INTERRUPT(me))
                {
                    me->flags &= ~_PR_INTERRUPT;
                    PR_SetError( PR_PENDING_INTERRUPT_ERROR, 0);
                } else
                {
                    PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                }
                rv = -1;
                goto done;
            } else if (_PR_PENDING_INTERRUPT(me)) {
                me->flags &= ~_PR_INTERRUPT;
                PR_SetError( PR_PENDING_INTERRUPT_ERROR, 0);
                rv = -1;
                goto done;
            }
        } else if ((err == WSAEINTR) && (!_PR_PENDING_INTERRUPT(me))){
            continue;
        } else {
            break;
        }
    }
    if (rv < 0) {
        _PR_MD_MAP_ACCEPT_ERROR(err);
    }
done:
    if ( rv == INVALID_SOCKET )
        return(-1 );
    else
        return(rv);
} /* end _MD_Accept() */


PRInt32
_PR_MD_CONNECT(PRFileDesc *fd, const PRNetAddr *addr, PRUint32 addrlen, 
               PRIntervalTime timeout)
{
    PRInt32 osfd = fd->secret->md.osfd;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRInt32 rv, err;

    while ((rv = connect(osfd, (struct sockaddr *)addr, addrlen)) == -1) {
        err = WSAGetLastError();
        if (err == WSAEISCONN) {
            rv = 0;
            break;
        }
        /* for winsock1.1, it reports EALREADY as EINVAL */
		if ((err == WSAEWOULDBLOCK)
            ||(err == WSAEALREADY) 
            || (err = WSAEINVAL)) {
			if (fd->secret->nonblocking) {
				break;
			}
            if (_PR_WaitForFD(osfd, PR_POLL_WRITE, timeout) == 0) {
                if ( _PR_PENDING_INTERRUPT(me))
                {
                    me->flags &= ~_PR_INTERRUPT;
                    PR_SetError( PR_PENDING_INTERRUPT_ERROR, 0);
                } else
                {
                    PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                }
                rv = -1;
                goto done;
            } else if (_PR_PENDING_INTERRUPT(me)) {
                me->flags &= ~_PR_INTERRUPT;
                PR_SetError( PR_PENDING_INTERRUPT_ERROR, 0);
                rv = -1;
                goto done;
            }
        } else if ((err == WSAEINTR) && (!_PR_PENDING_INTERRUPT(me))){
            continue;
        } else {
            break;
        }
    }

    if (rv < 0) {
        _PR_MD_MAP_CONNECT_ERROR(err);
    }
done:
    return rv;
}

PRInt32
_PR_MD_BIND(PRFileDesc *fd, const PRNetAddr *addr, PRUint32 addrlen)
{
    PRInt32 rv;
    int one = 1;

    rv = bind(fd->secret->md.osfd, (const struct sockaddr *)&(addr->inet), addrlen);

    if (rv == SOCKET_ERROR)  {
        _PR_MD_MAP_BIND_ERROR(WSAGetLastError());
        return -1;
    }

    return 0;
}


PRInt32
_PR_MD_RECV(PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags, 
            PRIntervalTime timeout)
{
    PRInt32 osfd = fd->secret->md.osfd;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRInt32 rv, err;

    while ((rv = recv(osfd,buf,amount,flags)) == -1) {
        err = WSAGetLastError();
		if ( err == WSAEWOULDBLOCK ) {
			if (fd->secret->nonblocking) {
				break;
			}
            if (_PR_WaitForFD(osfd, PR_POLL_READ, timeout) == 0) {
                if ( _PR_PENDING_INTERRUPT(me))
                {
                    me->flags &= ~_PR_INTERRUPT;
                    PR_SetError( PR_PENDING_INTERRUPT_ERROR, 0);
                } else
                {
                    PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                }
                rv = -1;
                goto done;
            } else if (_PR_PENDING_INTERRUPT(me)) {
                me->flags &= ~_PR_INTERRUPT;
                PR_SetError( PR_PENDING_INTERRUPT_ERROR, 0);
                rv = -1;
                goto done;
            }
        } else if ((err == WSAEINTR) && (!_PR_PENDING_INTERRUPT(me))){
            continue;
        } else {
            break;
        }
    }
    if (rv < 0) {
        _PR_MD_MAP_RECV_ERROR(err);
    }
done:
    return(rv);
}

PRInt32
_PR_MD_SEND(PRFileDesc *fd, const void *buf, PRInt32 amount, PRIntn flags,
            PRIntervalTime timeout)
{
    PRInt32 osfd = fd->secret->md.osfd;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRInt32 rv, err;

    while ((rv = send(osfd,buf,amount,flags)) == -1) {
        err = WSAGetLastError();
		if ( err == WSAEWOULDBLOCK ) {
			if (fd->secret->nonblocking) {
				break;
			}
            if (_PR_WaitForFD(osfd, PR_POLL_WRITE, timeout) == 0) {
                if ( _PR_PENDING_INTERRUPT(me))
                {
                    me->flags &= ~_PR_INTERRUPT;
                    PR_SetError( PR_PENDING_INTERRUPT_ERROR, 0);
                } else
                {
                    PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                }
                rv = -1;
                goto done;
            } else if (_PR_PENDING_INTERRUPT(me)) {
                me->flags &= ~_PR_INTERRUPT;
                PR_SetError( PR_PENDING_INTERRUPT_ERROR, 0);
                rv = -1;
                goto done;
            }
        } else if ((err == WSAEINTR) && (!_PR_PENDING_INTERRUPT(me))){
            continue;
        } else {
            break;
        }
    }
    if (rv < 0) {
        _PR_MD_MAP_SEND_ERROR(err);
    }
done:
    return rv;
}

PRInt32
_PR_MD_SENDTO(PRFileDesc*fd, const void *buf, PRInt32 amount, PRIntn flags,
              const PRNetAddr *addr, PRUint32 addrlen, PRIntervalTime timeout)
{
    PRInt32 osfd = fd->secret->md.osfd;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRInt32 rv, err;

    while ((rv = sendto(osfd, buf, amount, flags,
            (struct sockaddr *) addr, addrlen)) == -1) {
        err = WSAGetLastError();
		if ( err == WSAEWOULDBLOCK ) {
			if (fd->secret->nonblocking) {
				break;
			}
            if (_PR_WaitForFD(osfd, PR_POLL_WRITE, timeout) == 0) {
                if ( _PR_PENDING_INTERRUPT(me))
                {
                    me->flags &= ~_PR_INTERRUPT;
                    PR_SetError( PR_PENDING_INTERRUPT_ERROR, 0);
                } else
                {
                    PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                }
                rv = -1;
                goto done;
            } else if (_PR_PENDING_INTERRUPT(me)) {
                me->flags &= ~_PR_INTERRUPT;
                PR_SetError( PR_PENDING_INTERRUPT_ERROR, 0);
                rv = -1;
                goto done;
            }
        } else if ((err == WSAEINTR) && (!_PR_PENDING_INTERRUPT(me))){
            continue;
        } else {
            break;
        }
    }
    if (rv < 0) {
        _PR_MD_MAP_SENDTO_ERROR(err);
    }
done:
    return rv;
}

PRInt32
_PR_MD_RECVFROM(PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags,
                PRNetAddr *addr, PRUint32 *addrlen, PRIntervalTime timeout)
{
    PRInt32 osfd = fd->secret->md.osfd;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRInt32 rv, err;

    while ((*addrlen = PR_NETADDR_SIZE(addr)),
                ((rv = recvfrom(osfd, buf, amount, flags,
                (struct sockaddr FAR *) addr,(int FAR *)addrlen)) == -1)) {
        err = WSAGetLastError();
		if ( err == WSAEWOULDBLOCK ) {
			if (fd->secret->nonblocking) {
				break;
			}
            if (_PR_WaitForFD(osfd, PR_POLL_READ, timeout) == 0) {
                if ( _PR_PENDING_INTERRUPT(me))
                {
                    me->flags &= ~_PR_INTERRUPT;
                    PR_SetError( PR_PENDING_INTERRUPT_ERROR, 0);
                } else
                {
                    PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                }
                rv = -1;
                goto done;
            } else if (_PR_PENDING_INTERRUPT(me)) {
                me->flags &= ~_PR_INTERRUPT;
                PR_SetError( PR_PENDING_INTERRUPT_ERROR, 0);
                rv = -1;
                goto done;
            }
        } else if ((err == WSAEINTR) && (!_PR_PENDING_INTERRUPT(me))){
            continue;
        } else {
            break;
        }
    }
    if (rv < 0) {
        _PR_MD_MAP_RECVFROM_ERROR(err);
    }
done:
    return(rv);
}

PRInt32
_PR_MD_WRITEV(PRFileDesc *fd, const PRIOVec *iov, PRInt32 iov_size, PRIntervalTime timeout)
{
    int index;
    int sent = 0;
    int rv;

    for (index=0; index < iov_size; index++) 
    {

/*
 * XXX To be fixed
 * should call PR_Send
 */

        rv = _PR_MD_SEND(fd, iov[index].iov_base, iov[index].iov_len, 0, timeout);
        if (rv > 0) 
            sent += rv;
        if ( rv != iov[index].iov_len ) 
        {
            if (sent <= 0)
                return -1;
            return -1;
        }
    }
    return sent;
}

PRInt32
_PR_MD_SHUTDOWN(PRFileDesc *fd, PRIntn how)
{
PRInt32 rv;

    rv = shutdown(fd->secret->md.osfd, how);
    if (rv < 0)
        _PR_MD_MAP_SHUTDOWN_ERROR(WSAGetLastError());
    return rv;
}

PRStatus
_PR_MD_GETSOCKNAME(PRFileDesc *fd, PRNetAddr *addr, PRUint32 *len)
{
    PRInt32 rv;

    rv = getsockname((SOCKET)fd->secret->md.osfd, (struct sockaddr *)addr, (int *)len);
    if (rv==0)
        return PR_SUCCESS;
    else {
        _PR_MD_MAP_GETSOCKNAME_ERROR(WSAGetLastError());
        return PR_FAILURE;
    }
}

PRStatus
_PR_MD_GETPEERNAME(PRFileDesc *fd, PRNetAddr *addr, PRUint32 *len)
{
    PRInt32 rv;

    rv = getpeername((SOCKET)fd->secret->md.osfd, (struct sockaddr *)addr, (int*)len);
    if (rv==0)
        return PR_SUCCESS;
    else {
        _PR_MD_MAP_GETPEERNAME_ERROR(WSAGetLastError());
        return PR_FAILURE;
    }
}

PRStatus
_PR_MD_GETSOCKOPT(PRFileDesc *fd, PRInt32 level, PRInt32 optname, char* optval, PRInt32* optlen)
{
    PRInt32 rv;

    rv = getsockopt((SOCKET)fd->secret->md.osfd, level, optname, optval, (int*)optlen);
    if (rv==0)
        return PR_SUCCESS;
    else {
        _PR_MD_MAP_GETSOCKOPT_ERROR(WSAGetLastError());
        return PR_FAILURE;
    }
}

PRStatus
_PR_MD_SETSOCKOPT(PRFileDesc *fd, PRInt32 level, PRInt32 optname, const char* optval, PRInt32 optlen)
{
    PRInt32 rv;

    rv = setsockopt((SOCKET)fd->secret->md.osfd, level, optname, optval, optlen);
    if (rv==0)
        return PR_SUCCESS;
    else {
        _PR_MD_MAP_SETSOCKOPT_ERROR(WSAGetLastError());
        return PR_FAILURE;
    }
}

void
_PR_MD_MAKE_NONBLOCK(PRFileDesc *f)
{
    return; // do nothing!
}

/*
** Wait for I/O on a single descriptor.
 *
 * return 0, if timed-out, else return 1
*/
PRInt32
_PR_WaitForFD(PRInt32 osfd, PRUintn how, PRIntervalTime timeout)
{
    _PRWin16PollDesc *pd;
    PRPollQueue      *pq;
    PRIntn is;
    PRInt32 rv = 1;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    PR_ASSERT(!(me->flags & _PR_IDLE_THREAD));

    pd = &me->md.thr_pd;
    pq = &me->md.thr_pq;
    if (timeout == PR_INTERVAL_NO_WAIT) return 0;

    pd->osfd = osfd;
    pd->in_flags = how;
    pd->out_flags = 0;

    pq->pds = pd;
    pq->npds = 1;

    _PR_INTSOFF(is);
    _PR_MD_IOQ_LOCK();
    _PR_THREAD_LOCK(me);

	if (_PR_PENDING_INTERRUPT(me)) {
    	_PR_THREAD_UNLOCK(me);
    	_PR_MD_IOQ_UNLOCK();
		return 0;
	}

    pq->thr = me;
    pq->on_ioq = PR_TRUE;
    pq->timeout = timeout;
    _PR_ADD_TO_IOQ((*pq), me->cpu);
    if (how == PR_POLL_READ) {
        FD_SET(osfd, &_PR_FD_READ_SET(me->cpu));
        (_PR_FD_READ_CNT(me->cpu))[osfd]++;
    } else if (how == PR_POLL_WRITE) {
        FD_SET(osfd, &_PR_FD_WRITE_SET(me->cpu));
        (_PR_FD_WRITE_CNT(me->cpu))[osfd]++;
    } else {
        FD_SET(osfd, &_PR_FD_EXCEPTION_SET(me->cpu));
        (_PR_FD_EXCEPTION_CNT(me->cpu))[osfd]++;
    }
    if (_PR_IOQ_MAX_OSFD(me->cpu) < osfd)
        _PR_IOQ_MAX_OSFD(me->cpu) = osfd;
    if (_PR_IOQ_TIMEOUT(me->cpu) > timeout)
        _PR_IOQ_TIMEOUT(me->cpu) = timeout;
        
    _PR_THREAD_LOCK(me);

    _PR_SLEEPQ_LOCK(me->cpu);
    _PR_ADD_SLEEPQ(me, timeout);
    me->state = _PR_IO_WAIT;
    me->io_pending = PR_TRUE;
    me->io_suspended = PR_FALSE;
    _PR_SLEEPQ_UNLOCK(me->cpu);
    _PR_THREAD_UNLOCK(me);
    _PR_MD_IOQ_UNLOCK();

    _PR_MD_WAIT(me, timeout);
    me->io_pending = PR_FALSE;
    me->io_suspended = PR_FALSE;

    /*
    ** If we timed out the pollq might still be on the ioq. Remove it
    ** before continuing.
    */
    if (pq->on_ioq) {
        _PR_INTSOFF(is);
        _PR_MD_IOQ_LOCK();
    /*
     * Need to check pq.on_ioq again
     */
        if (pq->on_ioq) {
            PR_REMOVE_LINK(&pq->links);
            if (how == PR_POLL_READ) {
                if ((--(_PR_FD_READ_CNT(me->cpu))[osfd]) == 0)
                    FD_CLR(osfd, &_PR_FD_READ_SET(me->cpu));
            
            } else if (how == PR_POLL_WRITE) {
                if ((--(_PR_FD_WRITE_CNT(me->cpu))[osfd]) == 0)
                    FD_CLR(osfd, &_PR_FD_WRITE_SET(me->cpu));
            } else {
                if ((--(_PR_FD_EXCEPTION_CNT(me->cpu))[osfd]) == 0)
                    FD_CLR(osfd, &_PR_FD_EXCEPTION_SET(me->cpu));
            }
        }
        _PR_MD_IOQ_UNLOCK();
        rv = 0;
    }
    _PR_FAST_INTSON(is);
   return(rv);
}

/*
 * Unblock threads waiting for I/O
 *    used when interrupting threads
 *
 * NOTE: The thread lock should held when this function is called.
 * On return, the thread lock is released.
 */
void _PR_Unblock_IO_Wait(PRThread *thr)
{
    int pri = thr->priority;
    _PRCPU *cpu = thr->cpu;
 
    PR_ASSERT(thr->flags & (_PR_ON_SLEEPQ | _PR_ON_PAUSEQ));
    _PR_SLEEPQ_LOCK(cpu);
    _PR_DEL_SLEEPQ(thr, PR_TRUE);
    _PR_SLEEPQ_UNLOCK(cpu);

    PR_ASSERT(!(thr->flags & _PR_IDLE_THREAD));
    thr->state = _PR_RUNNABLE;
    _PR_RUNQ_LOCK(cpu);
    _PR_ADD_RUNQ(thr, cpu, pri);
    _PR_RUNQ_UNLOCK(cpu);
    _PR_THREAD_UNLOCK(thr);
    _PR_MD_WAKEUP_WAITER(thr);
}

/*
** Scan through io queue and find any bad fd's that triggered the error
** from _MD_SELECT
*/
static void FindBadFDs(void)
{
    PRCList *q;
    PRThread *me = _MD_CURRENT_THREAD();
    int sockOpt;
    int sockOptLen = sizeof(sockOpt);

    PR_ASSERT(!_PR_IS_NATIVE_THREAD(me));
    q = (_PR_IOQ(me->cpu)).next;
    _PR_IOQ_MAX_OSFD(me->cpu) = -1;
    _PR_IOQ_TIMEOUT(me->cpu) = PR_INTERVAL_NO_TIMEOUT;
    while (q != &_PR_IOQ(me->cpu)) {
        PRPollQueue *pq = _PR_POLLQUEUE_PTR(q);
        PRBool notify = PR_FALSE;
        _PRWin16PollDesc *pds = pq->pds;
        _PRWin16PollDesc *epds = pds + pq->npds;
        PRInt32 pq_max_osfd = -1;

        q = q->next;
        for (; pds < epds; pds++) {
            PRInt32 osfd = pds->osfd;
            pds->out_flags = 0;
            PR_ASSERT(osfd >= 0 || pds->in_flags == 0);
            if (pds->in_flags == 0) {
                continue;  /* skip this fd */
            }
            if ( getsockopt(osfd, 
                    (int)SOL_SOCKET, 
                    SO_TYPE, 
                    (char*)&sockOpt, 
                    &sockOptLen) == SOCKET_ERROR ) 
            {
                if ( WSAGetLastError() == WSAENOTSOCK )
                {
                    PR_LOG(_pr_io_lm, PR_LOG_MAX,
                        ("file descriptor %d is bad", osfd));
                    pds->out_flags = PR_POLL_NVAL;
                    notify = PR_TRUE;
                }
            }
            if (osfd > pq_max_osfd) {
                pq_max_osfd = osfd;
            }
        }

        if (notify) {
            PRIntn pri;
            PR_REMOVE_LINK(&pq->links);
            pq->on_ioq = PR_FALSE;

            /*
         * Decrement the count of descriptors for each desciptor/event
         * because this I/O request is being removed from the
         * ioq
         */
            pds = pq->pds;
            for (; pds < epds; pds++) {
                PRInt32 osfd = pds->osfd;
                PRInt16 in_flags = pds->in_flags;
                PR_ASSERT(osfd >= 0 || in_flags == 0);
                if (in_flags & PR_POLL_READ) {
                    if (--(_PR_FD_READ_CNT(me->cpu))[osfd] == 0)
                        FD_CLR(osfd, &_PR_FD_READ_SET(me->cpu));
                }
                if (in_flags & PR_POLL_WRITE) {
                    if (--(_PR_FD_WRITE_CNT(me->cpu))[osfd] == 0)
                        FD_CLR(osfd, &_PR_FD_WRITE_SET(me->cpu));
                }
                if (in_flags & PR_POLL_EXCEPT) {
                    if (--(_PR_FD_EXCEPTION_CNT(me->cpu))[osfd] == 0)
                        FD_CLR(osfd, &_PR_FD_EXCEPTION_SET(me->cpu));
                }
            }

            _PR_THREAD_LOCK(pq->thr);
            if (pq->thr->flags & (_PR_ON_PAUSEQ|_PR_ON_SLEEPQ)) {
                _PRCPU *cpu = pq->thr->cpu;
                
                _PR_SLEEPQ_LOCK(pq->thr->cpu);
                _PR_DEL_SLEEPQ(pq->thr, PR_TRUE);
                _PR_SLEEPQ_UNLOCK(pq->thr->cpu);

                pri = pq->thr->priority;
                pq->thr->state = _PR_RUNNABLE;

                _PR_RUNQ_LOCK(cpu);
                _PR_ADD_RUNQ(pq->thr, cpu, pri);
                _PR_RUNQ_UNLOCK(cpu);
            }
            _PR_THREAD_UNLOCK(pq->thr);
        } else {
            if (pq->timeout < _PR_IOQ_TIMEOUT(me->cpu))
                _PR_IOQ_TIMEOUT(me->cpu) = pq->timeout;
            if (_PR_IOQ_MAX_OSFD(me->cpu) < pq_max_osfd)
                _PR_IOQ_MAX_OSFD(me->cpu) = pq_max_osfd;
        }
    }
} /* end FindBadFDs() */

/*
** Called by the scheduler when there is nothing to do. This means that
** all threads are blocked on some monitor somewhere.
**
** Pause the current CPU. longjmp to the cpu's pause stack
*/
PRInt32 _PR_MD_PAUSE_CPU( PRIntervalTime ticks)
{
    PRThread *me = _MD_CURRENT_THREAD();
    struct timeval timeout, *tvp;
    fd_set r, w, e;
    fd_set *rp, *wp, *ep;
    PRInt32 max_osfd, nfd;
    PRInt32 rv;
    PRCList *q;
    PRUint32 min_timeout;

    PR_ASSERT(_PR_MD_GET_INTSOFF() != 0);

    /*
     * assigment of fd_sets
     */
    r = _PR_FD_READ_SET(me->cpu);
    w = _PR_FD_WRITE_SET(me->cpu);
    e = _PR_FD_EXCEPTION_SET(me->cpu);

    rp = &r;
    wp = &w;
    ep = &e;

    max_osfd = _PR_IOQ_MAX_OSFD(me->cpu) + 1;
    min_timeout = _PR_IOQ_TIMEOUT(me->cpu);
    /*
    ** Compute the minimum timeout value: make it the smaller of the
    ** timeouts specified by the i/o pollers or the timeout of the first
    ** sleeping thread.
    */
    q = _PR_SLEEPQ(me->cpu).next;

    if (q != &_PR_SLEEPQ(me->cpu)) {
        PRThread *t = _PR_THREAD_PTR(q);

        if (t->sleep < min_timeout) {
            min_timeout = t->sleep;
        }
    }
    if (min_timeout > ticks) {
        min_timeout = ticks;
    }

    if (min_timeout == PR_INTERVAL_NO_TIMEOUT) {
        tvp = NULL;
    } else {
        timeout.tv_sec = PR_IntervalToSeconds(min_timeout);
        timeout.tv_usec = PR_IntervalToMicroseconds(min_timeout)
            % PR_USEC_PER_SEC;
        tvp = &timeout;
    }

    _PR_MD_IOQ_UNLOCK();
    _MD_CHECK_FOR_EXIT();
    /*
     * check for i/o operations
     */

    nfd = _MD_SELECT(max_osfd, rp, wp, ep, tvp);

    _MD_CHECK_FOR_EXIT();
    _PR_MD_IOQ_LOCK();
    /*
    ** Notify monitors that are associated with the selected descriptors.
    */
    if (nfd > 0) {
        q = _PR_IOQ(me->cpu).next;
        _PR_IOQ_MAX_OSFD(me->cpu) = -1;
        _PR_IOQ_TIMEOUT(me->cpu) = PR_INTERVAL_NO_TIMEOUT;
        while (q != &_PR_IOQ(me->cpu)) {
            PRPollQueue *pq = _PR_POLLQUEUE_PTR(q);
            PRBool notify = PR_FALSE;
            _PRWin16PollDesc *pds = pq->pds;
            _PRWin16PollDesc *epds = pds + pq->npds;
            PRInt32 pq_max_osfd = -1;

            q = q->next;
            for (; pds < epds; pds++) {
                PRInt32 osfd = pds->osfd;
                PRInt16 in_flags = pds->in_flags;
                PRInt16 out_flags = 0;
                PR_ASSERT(osfd >= 0 || in_flags == 0);
                if ((in_flags & PR_POLL_READ) && FD_ISSET(osfd, rp)) {
                    out_flags |= PR_POLL_READ;
                }
                if ((in_flags & PR_POLL_WRITE) && FD_ISSET(osfd, wp)) {
                    out_flags |= PR_POLL_WRITE;
                }
                if ((in_flags & PR_POLL_EXCEPT) && FD_ISSET(osfd, ep)) {
                    out_flags |= PR_POLL_EXCEPT;
                }
                pds->out_flags = out_flags;
                if (out_flags) {
                    notify = PR_TRUE;
                }
                if (osfd > pq_max_osfd) {
                    pq_max_osfd = osfd;
                }
            }
            if (notify == PR_TRUE) {
                PRIntn pri;
                PRThread *thred;

                PR_REMOVE_LINK(&pq->links);
                pq->on_ioq = PR_FALSE;

                /*
                 * Decrement the count of descriptors for each desciptor/event
                 * because this I/O request is being removed from the
                 * ioq
                 */
                pds = pq->pds;
                for (; pds < epds; pds++) {
                    PRInt32 osfd = pds->osfd;
                    PRInt16 in_flags = pds->in_flags;
                    PR_ASSERT(osfd >= 0 || in_flags == 0);
                    if (in_flags & PR_POLL_READ) {
                        if (--(_PR_FD_READ_CNT(me->cpu))[osfd] == 0)
                            FD_CLR(osfd, &_PR_FD_READ_SET(me->cpu));
                    }
                    if (in_flags & PR_POLL_WRITE) {
                        if (--(_PR_FD_WRITE_CNT(me->cpu))[osfd] == 0)
                            FD_CLR(osfd, &_PR_FD_WRITE_SET(me->cpu));
                    }
                    if (in_flags & PR_POLL_EXCEPT) {
                        if (--(_PR_FD_EXCEPTION_CNT(me->cpu))[osfd] == 0)
                            FD_CLR(osfd, &_PR_FD_EXCEPTION_SET(me->cpu));
                    }
                }
                 thred = pq->thr;
                _PR_THREAD_LOCK(thred);
                if (pq->thr->flags & (_PR_ON_PAUSEQ|_PR_ON_SLEEPQ)) {
                    _PRCPU *cpu = thred->cpu;
                    _PR_SLEEPQ_LOCK(pq->thr->cpu);
                    _PR_DEL_SLEEPQ(pq->thr, PR_TRUE);
                    _PR_SLEEPQ_UNLOCK(pq->thr->cpu);

                    pri = pq->thr->priority;
                    pq->thr->state = _PR_RUNNABLE;

                    pq->thr->cpu = cpu;
                    _PR_RUNQ_LOCK(cpu);
                    _PR_ADD_RUNQ(pq->thr, cpu, pri);
                    _PR_RUNQ_UNLOCK(cpu);
                    if (_pr_md_idle_cpus > 1)
                        _PR_MD_WAKEUP_WAITER(thred);
                }
                _PR_THREAD_UNLOCK(thred);
            } else {
                if (pq->timeout < _PR_IOQ_TIMEOUT(me->cpu))
                    _PR_IOQ_TIMEOUT(me->cpu) = pq->timeout;
                if (_PR_IOQ_MAX_OSFD(me->cpu) < pq_max_osfd)
                    _PR_IOQ_MAX_OSFD(me->cpu) = pq_max_osfd;
            }
        }
    } else if (nfd < 0) {
        if ( WSAGetLastError() == WSAENOTSOCK )
        {
            FindBadFDs();
        } else {
            PR_LOG(_pr_io_lm, PR_LOG_MAX, ("select() failed with errno %d",
                errno));
        }
    }
    _PR_MD_IOQ_UNLOCK();
    return(0);
    
} /* end _PR_MD_PAUSE_CPU() */


/*
** _MD_pr_poll() -- Implement MD polling
**
** The function was snatched (re-used) from the unix implementation.
** 
** The native thread stuff was deleted.
** The pollqueue is instantiated on the mdthread structure
** to keep the stack frame from being corrupted when this
** thread is waiting on the poll.
**
*/
extern PRInt32 
_MD_PR_POLL(PRPollDesc *pds, PRIntn npds,
                        PRIntervalTime timeout)
{
    PRPollDesc *pd, *epd;
    PRInt32 n, err, pdcnt;
    PRIntn is;
    _PRWin16PollDesc *spds, *spd;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRPollQueue *pq;

    pq = &me->md.thr_pq;
    
    /*
     * XXX
     *   PRPollDesc has a PRFileDesc field, fd, while the IOQ
     *   is a list of PRPollQueue structures, each of which contains
     *   a _PRWin16PollDesc. A _PRWin16PollDesc struct contains
     *   the OS file descriptor, osfd, and not a PRFileDesc.
     *   So, we have allocate memory for _PRWin16PollDesc structures,
     *   copy the flags information from the pds list and have pq
     *   point to this list of _PRWin16PollDesc structures.
     *
     *   It would be better if the memory allocation can be avoided.
     */

    spds = (_PRWin16PollDesc*) PR_MALLOC(npds * sizeof(_PRWin16PollDesc));
    if (!spds) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
           return -1;
    }
    spd = spds;

    _PR_INTSOFF(is);
    _PR_MD_IOQ_LOCK();
       _PR_THREAD_LOCK(me);

    if (_PR_PENDING_INTERRUPT(me)) {
        me->flags &= ~_PR_INTERRUPT;
        PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
        _PR_THREAD_UNLOCK(me);
        _PR_MD_IOQ_UNLOCK();
        PR_DELETE(spds);
           return -1;
    }

    pdcnt = 0;
    for (pd = pds, epd = pd + npds; pd < epd; pd++) {
        PRInt32 osfd;
        PRInt16 in_flags = pd->in_flags;
        PRFileDesc *bottom = pd->fd;

        if ((NULL == bottom) || (in_flags == 0)) {
            continue;
        }
        while (bottom->lower != NULL) {
            bottom = bottom->lower;
        }
        osfd = bottom->secret->md.osfd;

        PR_ASSERT(osfd >= 0 || in_flags == 0);

        spd->osfd = osfd;
        spd->in_flags = pd->in_flags;
        spd++;
        pdcnt++;

        if (in_flags & PR_POLL_READ)  {
            FD_SET(osfd, &_PR_FD_READ_SET(me->cpu));
            _PR_FD_READ_CNT(me->cpu)[osfd]++;
        }
        if (in_flags & PR_POLL_WRITE) {
            FD_SET(osfd, &_PR_FD_WRITE_SET(me->cpu));
            (_PR_FD_WRITE_CNT(me->cpu))[osfd]++;
        }
        if (in_flags & PR_POLL_EXCEPT) {
            FD_SET(osfd, &_PR_FD_EXCEPTION_SET(me->cpu));
            (_PR_FD_EXCEPTION_CNT(me->cpu))[osfd]++;
        }
        if (osfd > _PR_IOQ_MAX_OSFD(me->cpu))
            _PR_IOQ_MAX_OSFD(me->cpu) = osfd;
    }
    if (timeout < _PR_IOQ_TIMEOUT(me->cpu))
        _PR_IOQ_TIMEOUT(me->cpu) = timeout;


    pq->pds = spds;
    pq->npds = pdcnt;

    pq->thr = me;
    pq->on_ioq = PR_TRUE;
    pq->timeout = timeout;
    _PR_ADD_TO_IOQ((*pq), me->cpu);
    _PR_SLEEPQ_LOCK(me->cpu);
    _PR_ADD_SLEEPQ(me, timeout);
    me->state = _PR_IO_WAIT;
    me->io_pending = PR_TRUE;
    me->io_suspended = PR_FALSE;
    _PR_SLEEPQ_UNLOCK(me->cpu);
    _PR_THREAD_UNLOCK(me);
    _PR_MD_IOQ_UNLOCK();

    _PR_MD_WAIT(me, timeout);

    me->io_pending = PR_FALSE;
    me->io_suspended = PR_FALSE;

    /*
     * Copy the out_flags from the _PRWin16PollDesc structures to the
     * user's PRPollDesc structures and free the allocated memory
     */
    spd = spds;
    for (pd = pds, epd = pd + npds; pd < epd; pd++) {
        if ((NULL == pd->fd) || (pd->in_flags == 0)) {
            pd->out_flags = 0;
            continue;
        }
        pd->out_flags = spd->out_flags;
        spd++;
    }
    PR_DELETE(spds);

    /*
    ** If we timed out the pollq might still be on the ioq. Remove it
    ** before continuing.
    */
    if (pq->on_ioq) {
        _PR_INTSOFF(is);
        _PR_MD_IOQ_LOCK();
        /*
         * Need to check pq.on_ioq again
         */
        if (pq->on_ioq == PR_TRUE) {
            PR_REMOVE_LINK(&pq->links);
            for (pd = pds, epd = pd + npds; pd < epd; pd++) {
                PRInt32 osfd;
                PRInt16 in_flags = pd->in_flags;
                PRFileDesc *bottom = pd->fd;

                if ((NULL == bottom) || (in_flags == 0)) {
                    continue;
                }
                while (bottom->lower != NULL) {
                    bottom = bottom->lower;
                }
                osfd = bottom->secret->md.osfd;
                PR_ASSERT(osfd >= 0 || in_flags == 0);
                if (in_flags & PR_POLL_READ)  {
                    if (--(_PR_FD_READ_CNT(me->cpu))[osfd] == 0)
                        FD_CLR(osfd, &_PR_FD_READ_SET(me->cpu));
                }
                if (in_flags & PR_POLL_WRITE) {
                    if (--(_PR_FD_WRITE_CNT(me->cpu))[osfd] == 0)
                            FD_CLR(osfd, &_PR_FD_WRITE_SET(me->cpu));
                }
                if (in_flags & PR_POLL_EXCEPT) {
                    if (--(_PR_FD_EXCEPTION_CNT(me->cpu))[osfd] == 0)
                            FD_CLR(osfd, &_PR_FD_EXCEPTION_SET(me->cpu));
                }
            }
        }
        _PR_MD_IOQ_UNLOCK();
        _PR_INTSON(is);
    }
    if (_PR_PENDING_INTERRUPT(me)) {
        me->flags &= ~_PR_INTERRUPT;
        PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
           return -1;
    } else {
        n = 0;
        if (pq->on_ioq == PR_FALSE) {
            /* Count the number of ready descriptors */
            while (--npds >= 0) {
            if (pds->out_flags) {
                n++;
            }
                pds++;
            }
        }
        return n;
    }
} /* end _MD_pr_poll() */
