/* -*- Mode: C++; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http:// www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 */

#include "primpl.h"

#include <signal.h>
#include <unistd.h>
#include <memory.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>

/*
 * Make sure _PRSockLen_t is 32-bit, because we will cast a PRUint32* or
 * PRInt32* pointer to a _PRSockLen_t* pointer.
 */
#define _PRSockLen_t int

/*
** Global lock variable used to bracket calls into rusty libraries that
** aren't thread safe (like libc, libX, etc).
*/
static PRLock *_pr_rename_lock = NULL;
static PRMonitor *_pr_Xfe_mon = NULL;

/* 
** This is a support routine to handle "deferred" i/o on sockets. 
** It uses "select", so it is subject to all of the BeOS limitations
** (only READ notification, only sockets)
*/
#define READ_FD		1
#define WRITE_FD	2

static PRInt32 socket_io_wait(PRInt32 osfd, PRInt32 fd_type,
    PRIntervalTime timeout)
{
	PRInt32 rv = -1;
	struct timeval tv, *tvp;
	PRThread *me = _PR_MD_CURRENT_THREAD();
	PRIntervalTime epoch, now, elapsed, remaining;
	PRInt32 syserror;
	fd_set rd_wr;

	switch (timeout) {
		case PR_INTERVAL_NO_WAIT:
			PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
			break;
		case PR_INTERVAL_NO_TIMEOUT:
			/*
			 * This is a special case of the 'default' case below.
			 * Please see the comments there.
			 */
			tv.tv_sec = _PR_INTERRUPT_CHECK_INTERVAL_SECS;
			tv.tv_usec = 0;
			tvp = &tv;
			FD_ZERO(&rd_wr);
			do {
				FD_SET(osfd, &rd_wr);
				if (fd_type == READ_FD)
					rv = _MD_SELECT(osfd + 1, &rd_wr, NULL, NULL, tvp);
				else
					rv = _MD_SELECT(osfd + 1, NULL, &rd_wr, NULL, tvp);
				if (rv == -1 && (syserror = _MD_ERRNO()) != EINTR) {
					if (syserror == EBADF) {
						PR_SetError(PR_BAD_DESCRIPTOR_ERROR, EBADF);
					} else {
						PR_SetError(PR_UNKNOWN_ERROR, syserror);
					}
					if( _PR_PENDING_INTERRUPT(me)) {
						me->flags &= ~_PR_INTERRUPT;
						PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
						rv = -1;
						break;
					}
					break;
				}
			} while (rv == 0 || (rv == -1 && syserror == EINTR));
			break;
		default:
			now = epoch = PR_IntervalNow();
			remaining = timeout;
			tvp = &tv;
			FD_ZERO(&rd_wr);
			do {
				/*
				 * We block in _MD_SELECT for at most
				 * _PR_INTERRUPT_CHECK_INTERVAL_SECS seconds,
				 * so that there is an upper limit on the delay
				 * before the interrupt bit is checked.
				 */
				tv.tv_sec = PR_IntervalToSeconds(remaining);
				if (tv.tv_sec > _PR_INTERRUPT_CHECK_INTERVAL_SECS) {
					tv.tv_sec = _PR_INTERRUPT_CHECK_INTERVAL_SECS;
					tv.tv_usec = 0;
				} else {
					tv.tv_usec = PR_IntervalToMicroseconds(
						remaining -
						PR_SecondsToInterval(tv.tv_sec));
				}
				FD_SET(osfd, &rd_wr);
				if (fd_type == READ_FD)
					rv = _MD_SELECT(osfd + 1, &rd_wr, NULL, NULL, tvp);
				else
					rv = _MD_SELECT(osfd + 1, NULL, &rd_wr, NULL, tvp);
				/*
				 * we don't consider EINTR a real error
				 */
				if (rv == -1 && (syserror = _MD_ERRNO()) != EINTR) {
					if (syserror == EBADF) {
						PR_SetError(PR_BAD_DESCRIPTOR_ERROR, EBADF);
					} else {
						PR_SetError(PR_UNKNOWN_ERROR, syserror);
					}
					break;
				}
		                if (_PR_PENDING_INTERRUPT(me)) {
		                    me->flags &= ~_PR_INTERRUPT;
		                    PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
		                    rv = -1;
		                    break;
		                }
				/*
				 * We loop again if _MD_SELECT timed out or got interrupted
				 * by a signal, and the timeout deadline has not passed yet.
				 */
				if (rv == 0 || (rv == -1 && syserror == EINTR)) {
					/*
					 * If _MD_SELECT timed out, we know how much time
					 * we spent in blocking, so we can avoid a
					 * PR_IntervalNow() call.
					 */
					if (rv == 0) {
						now += PR_SecondsToInterval(tv.tv_sec)
							+ PR_MicrosecondsToInterval(tv.tv_usec);
					} else {
						now = PR_IntervalNow();
					}
					elapsed = (PRIntervalTime) (now - epoch);
					if (elapsed >= timeout) {
						PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
						rv = -1;
						break;
					} else {
						remaining = timeout - elapsed;
					}
				}
			} while (rv == 0 || (rv == -1 && syserror == EINTR));
			break;
	}
	return(rv);
}



PRInt32
_MD_recv (PRFileDesc *fd, void *buf, PRInt32 amount, PRInt32 flags,
          PRIntervalTime timeout)
{
    PRInt32 osfd = fd->secret->md.osfd;
    PRInt32 rv, err;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    if (fd->secret->md.sock_state & BE_SOCK_SHUTDOWN_READ) {
	_PR_MD_MAP_RECV_ERROR(EPIPE);
	return -1;
    }

    while ((rv = recv(osfd, buf, amount, flags)) == -1) {
	err = _MD_ERRNO();

	if ((err == EAGAIN) || (err == EWOULDBLOCK)) {
	    if (fd->secret->nonblocking) {
		break;
	    }
	    /* If socket was supposed to be blocking, 
		wait a while for the condition to be
		satisfied. */
	    if ((rv = socket_io_wait(osfd, READ_FD, timeout)) < 0)
		goto done;
	} else if ((err == EINTR) && (!_PR_PENDING_INTERRUPT(me))){
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
_MD_recvfrom (PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags,
              PRNetAddr *addr, PRUint32 *addrlen, PRIntervalTime timeout)
{
    PRInt32 osfd = fd->secret->md.osfd;
    PRInt32 rv, err;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    while ((*addrlen = PR_NETADDR_SIZE(addr)),
	   ((rv = recvfrom(osfd, buf, amount, flags,
			   (struct sockaddr *) addr,
                           (_PRSockLen_t *)addrlen)) == -1)) {
	err = _MD_ERRNO();

	if ((err == EAGAIN) || (err == EWOULDBLOCK)) {
	    if (fd->secret->nonblocking) {
		break;
	    }
            if ((rv = socket_io_wait(osfd, READ_FD, timeout)) < 0)
                goto done;

	} else if ((err == EINTR) && (!_PR_PENDING_INTERRUPT(me))) {
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
_MD_send (PRFileDesc *fd, const void *buf, PRInt32 amount, PRInt32 flags,
          PRIntervalTime timeout)
{
    PRInt32 osfd = fd->secret->md.osfd;
    PRInt32 rv, err;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    if (fd->secret->md.sock_state & BE_SOCK_SHUTDOWN_WRITE)
    {
	_PR_MD_MAP_SEND_ERROR(EPIPE);
	return -1;
    }

    while ((rv = send(osfd, buf, amount, flags)) == -1) {
	err = _MD_ERRNO();

	if ((err == EAGAIN) || (err == EWOULDBLOCK)) {
	    if (fd->secret->nonblocking) {
		break;
	    }

	    if( _PR_PENDING_INTERRUPT(me)) {

                me->flags &= ~_PR_INTERRUPT;
                PR_SetError( PR_PENDING_INTERRUPT_ERROR, 0);
                return -1;
            }

	    /* in UNIX implementations, you could do a socket_io_wait here.
	     * but since BeOS doesn't yet support WRITE notification in select,
	     * you're spanked.
	     */
	     snooze( 10000L );
	     continue;
	
	} else if ((err == EINTR) && (!_PR_PENDING_INTERRUPT(me))) {
	    continue;

	} else {
	    break;
	}
    }

    if (rv < 0) {
	_PR_MD_MAP_SEND_ERROR(err);
    }

    return(rv);
}

PRInt32
_MD_sendto (PRFileDesc *fd, const void *buf, PRInt32 amount, PRIntn flags,
            const PRNetAddr *addr, PRUint32 addrlen, PRIntervalTime timeout)
{
    PRInt32 osfd = fd->secret->md.osfd;
    PRInt32 rv, err;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    while ((rv = sendto(osfd, buf, amount, flags,
			(struct sockaddr *) addr, addrlen)) == -1) {
	err = _MD_ERRNO();

	if ((err == EAGAIN) || (err == EWOULDBLOCK))	{
	    if (fd->secret->nonblocking) {
		break;
	    }

	    printf( "This should be a blocking sendto call!!!\n" );
	} else if ((err == EINTR) && (!_PR_PENDING_INTERRUPT(me))) {
	    continue;

	} else {
	    break;
	}
    }

    if (rv < 0) {
	_PR_MD_MAP_SENDTO_ERROR(err);
    }

    return(rv);
}

PRInt32
_MD_accept (PRFileDesc *fd, PRNetAddr *addr, PRUint32 *addrlen,
            PRIntervalTime timeout)
{
    PRInt32 osfd = fd->secret->md.osfd;
    PRInt32 rv, err;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    while ((rv = accept(osfd, (struct sockaddr *) addr,
		    (_PRSockLen_t *)addrlen)) == -1) {
	err = _MD_ERRNO();

	if ((err == EAGAIN) || (err == EWOULDBLOCK)) {
	    if (fd->secret->nonblocking) {
		    break;
	    }
	    /* If it's SUPPOSED to be a blocking thread, wait
	     * a while to see if the triggering condition gets
	     * satisfied.
	     */
	    /* Assume that we're always using a native thread */
	    if ((rv = socket_io_wait(osfd, READ_FD, timeout)) < 0)
		goto done;
	} else if ((err == EINTR) && (!_PR_PENDING_INTERRUPT(me))) {
	    continue;
	} else {
	    break;
	}
    }

    if (addr) addr->raw.family = AF_INET;

    if (rv < 0) {
		_PR_MD_MAP_ACCEPT_ERROR(err);
    }
done:
#ifdef _PR_HAVE_SOCKADDR_LEN
    if (rv != -1) {
	/* Mask off the first byte of struct sockaddr (the length field) */
	if (addr) {
	    *((unsigned char *) addr) = 0;
#ifdef IS_LITTLE_ENDIAN
	    addr->raw.family = ntohs(addr->raw.family);
#endif
	}
    }
#endif /* _PR_HAVE_SOCKADDR_LEN */
    return(rv);
}

PRInt32
_MD_connect (PRFileDesc *fd, const PRNetAddr *addr, PRUint32 addrlen,
             PRIntervalTime timeout)
{
    PRInt32 osfd = fd->secret->md.osfd;
    PRInt32 rv, err;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    fd->secret->md.connectValueValid = PR_FALSE;

retry:
    if ((rv = connect(osfd, (struct sockaddr *)addr, addrlen)) == -1) {
        err = _MD_ERRNO();
	fd->secret->md.connectReturnValue = rv;
	fd->secret->md.connectReturnError = err;
	fd->secret->md.connectValueValid = PR_TRUE;

	if( err == EINTR ) {

	    if( _PR_PENDING_INTERRUPT(me)) {

		me->flags &= ~_PR_INTERRUPT;
                PR_SetError( PR_PENDING_INTERRUPT_ERROR, 0);
                return -1;
            }
	    snooze( 100000L );
            goto retry;
        }

        if(!fd->secret->nonblocking && ((err == EINPROGRESS) || (err==EAGAIN) || (err==EALREADY))) {

	    /*
	    ** There's no timeout on this connect, but that's not
	    ** a big deal, since the connect times out anyways
	    ** after 30 seconds.   Just sleep for 1/10th of a second
	    ** and retry until we go through or die.
	    */

	    if( _PR_PENDING_INTERRUPT(me)) {
                me->flags &= ~_PR_INTERRUPT;
                PR_SetError( PR_PENDING_INTERRUPT_ERROR, 0);
                return -1;
            } 

	    goto retry;
	}

	if( fd->secret->nonblocking && ((err == EAGAIN) || (err == EINPROGRESS))) {
	    PR_Lock(_connectLock);
	    connectList[connectCount].osfd = osfd;
	    memcpy(&connectList[connectCount].addr, addr, addrlen);
	    connectList[connectCount].addrlen = addrlen;
	    connectList[connectCount].timeout = timeout;
	    connectCount++;
	    PR_Unlock(_connectLock);
	    _PR_MD_MAP_CONNECT_ERROR(err);
	    return rv;
	}

	_PR_MD_MAP_CONNECT_ERROR(err);
    }

    return rv;
}

PRInt32
_MD_bind (PRFileDesc *fd, const PRNetAddr *addr, PRUint32 addrlen)
{
    PRInt32 rv, err;

    rv = bind(fd->secret->md.osfd, (struct sockaddr *) addr, (int )addrlen);

    if (rv < 0) {
        err = _MD_ERRNO();
        _PR_MD_MAP_BIND_ERROR(err);
    }

    return(rv);
}

PRInt32
_MD_listen (PRFileDesc *fd, PRIntn backlog)
{
    PRInt32 rv, err;

    /* Bug workaround!  Setting listen to 0 on Be accepts no connections.
    ** On most UN*Xes this sets the default.
    */

    if( backlog == 0 ) backlog = 5;
 
    rv = listen(fd->secret->md.osfd, backlog);
    if (rv < 0) {
        err = _MD_ERRNO();
        _PR_MD_MAP_LISTEN_ERROR(err);
    }

    return(rv);
}

PRInt32
_MD_shutdown (PRFileDesc *fd, PRIntn how)
{
    PRInt32 rv, err;

    if (how == PR_SHUTDOWN_SEND)
	fd->secret->md.sock_state = BE_SOCK_SHUTDOWN_WRITE;
    else if (how == PR_SHUTDOWN_RCV)
	fd->secret->md.sock_state = BE_SOCK_SHUTDOWN_READ;
    else if (how == PR_SHUTDOWN_BOTH) {
	fd->secret->md.sock_state = (BE_SOCK_SHUTDOWN_WRITE | BE_SOCK_SHUTDOWN_READ);
    }

    return 0;
}

PRInt32
_MD_socketpair (int af, int type, int flags, PRInt32 *osfd)
{
    return PR_NOT_IMPLEMENTED_ERROR;
}

PRInt32
_MD_close_socket (PRInt32 osfd)
{
    closesocket( osfd );
}

PRStatus
_MD_getsockname (PRFileDesc *fd, PRNetAddr *addr, PRUint32 *addrlen)
{
    PRInt32 rv, err;

    rv = getsockname(fd->secret->md.osfd,
                     (struct sockaddr *) addr, (_PRSockLen_t *)addrlen);

    if (rv < 0) {
        err = _MD_ERRNO();
        _PR_MD_MAP_GETSOCKNAME_ERROR(err);
    }

    return rv==0?PR_SUCCESS:PR_FAILURE;
}

PRStatus
_MD_getpeername (PRFileDesc *fd, PRNetAddr *addr, PRUint32 *addrlen)
{
    PRInt32 rv, err;

    rv = getpeername(fd->secret->md.osfd,
                     (struct sockaddr *) addr, (_PRSockLen_t *)addrlen);

    if (rv < 0) {
        err = _MD_ERRNO();
        _PR_MD_MAP_GETPEERNAME_ERROR(err);
    }
    return rv==0?PR_SUCCESS:PR_FAILURE;
}

PRStatus
_MD_getsockopt (PRFileDesc *fd, PRInt32 level,
                PRInt32 optname, char* optval, PRInt32* optlen)
{
    return PR_NOT_IMPLEMENTED_ERROR;

#if 0
    PRInt32 rv, err;

    rv = getsockopt(fd->secret->md.osfd, level, optname,
                    optval, (_PRSockLen_t *)optlen);
    if (rv < 0) {
        err = _MD_ERRNO();
        _PR_MD_MAP_GETSOCKOPT_ERROR(err);
    }

    return rv==0?PR_SUCCESS:PR_FAILURE;
#endif
}

PRStatus
_MD_setsockopt (PRFileDesc *fd, PRInt32 level,
                PRInt32 optname, const char* optval, PRInt32 optlen)
{
    PRInt32 rv, err;

    rv = setsockopt(fd->secret->md.osfd, level, optname, optval, optlen);
    if (rv < 0) {
        err = _MD_ERRNO();
        _PR_MD_MAP_SETSOCKOPT_ERROR(err);
    }
    return rv==0?PR_SUCCESS:PR_FAILURE;
}

PRInt32
_MD_accept_read (PRFileDesc *sd, PRInt32 *newSock, PRNetAddr **raddr,
                 void *buf, PRInt32 amount, PRIntervalTime timeout)
{
    return PR_NOT_IMPLEMENTED_ERROR;
}

PRInt32
_MD_transmitfile (PRFileDesc *sock, PRFileDesc *file, const void *headers, PRInt32 hlen, PRInt32 flags, PRIntervalTime timeout)
{
    return PR_NOT_IMPLEMENTED_ERROR;
}

PRInt32
_MD_socket (int af, int type, int flags)
{
    PRInt32 osfd, err;

    osfd = socket( af, type, 0 );

    if( -1 == osfd ) {

	err = _MD_ERRNO();
	_PR_MD_MAP_SOCKET_ERROR( err );
    }

    return( osfd );
}

PRInt32
_MD_socketavailable (PRFileDesc *fd)
{
    return PR_NOT_IMPLEMENTED_ERROR;
}

PRInt32
_MD_get_socket_error (void)
{
    return PR_NOT_IMPLEMENTED_ERROR;
}

PRStatus
_MD_gethostname (char *name, PRUint32 namelen)
{
PRInt32 rv, err;

    rv = gethostname(name, namelen);
    if (rv == 0)
    {
	err = _MD_ERRNO();
	_PR_MD_MAP_GETHOSTNAME_ERROR(err);
	return PR_FAILURE;
    }
    return PR_SUCCESS;
}

PRInt32
_MD_beos_get_nonblocking_connect_error(PRFileDesc *fd)
{
    int rv;
    int flags = 0;

    if( fd->secret->md.connectValueValid == PR_TRUE )

	if( fd->secret->md.connectReturnValue == -1 )

	    return fd->secret->md.connectReturnError;
	else
	    return 0;  /* No error */

    rv = recv(fd->secret->md.osfd, NULL, 0, flags);
    PR_ASSERT(-1 == rv || 0 == rv);
    if (-1 == rv && errno != EAGAIN && errno != EWOULDBLOCK) {
        return errno;
    }
    return 0;  /* no error */
}
