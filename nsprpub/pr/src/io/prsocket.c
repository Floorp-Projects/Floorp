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

#ifdef XP_UNIX
#include <fcntl.h>
#endif
#include <string.h>

#if defined(SVR4) || defined(SUNOS4)
/* To pick up FIONREAD */
#include <sys/filio.h>
#endif

/************************************************************************/

static PRInt32 PR_CALLBACK SocketWritev(PRFileDesc *fd, PRIOVec *iov, PRInt32 iov_size,
PRIntervalTime timeout)
{
	PRThread *me = _PR_MD_CURRENT_THREAD();
	int w = 0;
	PRIOVec *tmp_iov = NULL;
	int tmp_out;
	int index, iov_cnt;
	int count=0, sz = 0;    /* 'count' is the return value. */
#if defined(XP_UNIX)
	struct timeval tv, *tvp;
	fd_set wd;

	FD_ZERO(&wd);
	if (timeout == PR_INTERVAL_NO_TIMEOUT)
		tvp = NULL;
	else if (timeout != PR_INTERVAL_NO_WAIT) {
		tv.tv_sec = PR_IntervalToSeconds(timeout);
		tv.tv_usec = PR_IntervalToMicroseconds(
		    timeout - PR_SecondsToInterval(tv.tv_sec));
		tvp = &tv;
	}
#endif

	if (_PR_PENDING_INTERRUPT(me)) {
		me->flags &= ~_PR_INTERRUPT;
		PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
		return -1;
	}
	if (_PR_IO_PENDING(me)) {
		PR_SetError(PR_IO_PENDING_ERROR, 0);
		return -1;
	}

	tmp_iov = (PRIOVec *)PR_CALLOC(iov_size * sizeof(PRIOVec));
	if (!tmp_iov) {
		PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
		return -1;
	}

	for (index=0; index<iov_size; index++) {
		sz += iov[index].iov_len;
		tmp_iov[index].iov_base = iov[index].iov_base;
		tmp_iov[index].iov_len = iov[index].iov_len;
	}
	iov_cnt = iov_size;

	while (sz > 0) {

		w = _PR_MD_WRITEV(fd, tmp_iov, iov_cnt, timeout);
		if (w < 0) {
					count = -1;
					break;
		}
		count += w;
		if (fd->secret->nonblocking) {
			break;
		}
		sz -= w;

		if (sz > 0) {
			/* find the next unwritten vector */
			for ( index = 0, tmp_out = count;
			    tmp_out >= iov[index].iov_len;
			    tmp_out -= iov[index].iov_len, index++){;} /* nothing to execute */


			/* fill in the first partial read */
			tmp_iov[0].iov_base = &(((char *)iov[index].iov_base)[tmp_out]);
			tmp_iov[0].iov_len = iov[index].iov_len - tmp_out;
			index++;

			/* copy the remaining vectors */
			for (iov_cnt=1; index<iov_size; iov_cnt++, index++) {
				tmp_iov[iov_cnt].iov_base = iov[index].iov_base;
				tmp_iov[iov_cnt].iov_len = iov[index].iov_len;
			}
		}
	}

	if (tmp_iov)
		PR_DELETE(tmp_iov);
	return count;
}

/************************************************************************/

PR_IMPLEMENT(PRFileDesc *) PR_ImportTCPSocket(PRInt32 osfd)
{
PRFileDesc *fd;

	fd = PR_AllocFileDesc(osfd, PR_GetTCPMethods());
	if (fd != NULL)
		_PR_MD_MAKE_NONBLOCK(fd);
	else
		_PR_MD_CLOSE_SOCKET(osfd);
	return(fd);
}

PR_IMPLEMENT(PRFileDesc *) PR_ImportUDPSocket(PRInt32 osfd)
{
PRFileDesc *fd;

	fd = PR_AllocFileDesc(osfd, PR_GetUDPMethods());
	if (fd != NULL)
		_PR_MD_MAKE_NONBLOCK(fd);
	else
		_PR_MD_CLOSE_SOCKET(osfd);
	return(fd);
}

static PRStatus PR_CALLBACK SocketConnect(
    PRFileDesc *fd, const PRNetAddr *addr, PRIntervalTime timeout)
{
	PRInt32 rv;    /* Return value of _PR_MD_CONNECT */
	PRThread *me = _PR_MD_CURRENT_THREAD();

	if (_PR_PENDING_INTERRUPT(me)) {
		me->flags &= ~_PR_INTERRUPT;
		PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
		return PR_FAILURE;
	}

	rv = _PR_MD_CONNECT(fd, addr, PR_NETADDR_SIZE(addr), timeout);
	PR_LOG(_pr_io_lm, PR_LOG_MAX, ("connect -> %d", rv));
	if (rv == 0)
		return PR_SUCCESS;
	else
		return PR_FAILURE;
}

PR_IMPLEMENT(PRStatus) PR_GetConnectStatus(const PRPollDesc *pd)
{
    PRInt32 osfd;
    PRFileDesc *bottom = pd->fd;
    int err;

    if (pd->out_flags & PR_POLL_NVAL) {
        PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
        return PR_FAILURE;
    }
    if ((pd->out_flags & (PR_POLL_WRITE | PR_POLL_EXCEPT | PR_POLL_ERR)) == 0) {
        PR_ASSERT(pd->out_flags == 0);
        PR_SetError(PR_IN_PROGRESS_ERROR, 0);
        return PR_FAILURE;
    }

    while (bottom->lower != NULL) {
        bottom = bottom->lower;
    }
    osfd = bottom->secret->md.osfd;

#if defined(XP_UNIX)

    err = _MD_unix_get_nonblocking_connect_error(osfd);
    if (err != 0) {
        _PR_MD_MAP_CONNECT_ERROR(err);
        return PR_FAILURE;
    }
    return PR_SUCCESS;

#elif defined(WIN32) || defined(WIN16)

    if (pd->out_flags & PR_POLL_EXCEPT) {
        int len = sizeof(err);
#if defined(WIN32)
/* Note: There is a bug in Win32 WinSock. The sleep circumvents the
** bug. See wtc. /s lth.
*/
        Sleep(0);
#endif /* WIN32 */
        if (getsockopt(osfd, (int)SOL_SOCKET, SO_ERROR, (char *) &err, &len)
                == SOCKET_ERROR) {
            _PR_MD_MAP_GETSOCKOPT_ERROR(WSAGetLastError());
            return PR_FAILURE;
        }
        if (err != 0) {
            _PR_MD_MAP_CONNECT_ERROR(err);
        } else {
            PR_SetError(PR_UNKNOWN_ERROR, 0);
        }
        return PR_FAILURE;
    }

    PR_ASSERT(pd->out_flags & PR_POLL_WRITE);
    return PR_SUCCESS;

#elif defined(XP_OS2)

    if (pd->out_flags & PR_POLL_EXCEPT) {
        int len = sizeof(err);
        if (getsockopt(osfd, SOL_SOCKET, SO_ERROR, (char *) &err, &len)
                < 0) {
            _PR_MD_MAP_GETSOCKOPT_ERROR(sock_errno());
            return PR_FAILURE;
        }
        if (err != 0) {
            _PR_MD_MAP_CONNECT_ERROR(err);
        } else {
            PR_SetError(PR_UNKNOWN_ERROR, 0);
        }
        return PR_FAILURE;
    }

    PR_ASSERT(pd->out_flags & PR_POLL_WRITE);
    return PR_SUCCESS;

#elif defined(XP_MAC)

    err = _MD_mac_get_nonblocking_connect_error(osfd);
    if (err == -1)
        return PR_FAILURE;
	else     
		return PR_SUCCESS;

#else
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
#endif
}

static PRFileDesc* PR_CALLBACK SocketAccept(PRFileDesc *fd, PRNetAddr *addr,
PRIntervalTime timeout)
{
	PRInt32 osfd;
	PRFileDesc *fd2;
	PRUint32 al;
	PRThread *me = _PR_MD_CURRENT_THREAD();
#ifdef WINNT
	PRNetAddr addrCopy;
#endif

	if (_PR_PENDING_INTERRUPT(me)) {
		me->flags &= ~_PR_INTERRUPT;
		PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
		return 0;
	}
	if (_PR_IO_PENDING(me)) {
		PR_SetError(PR_IO_PENDING_ERROR, 0);
		return 0;
	}

#ifdef WINNT
	if (addr == NULL) {
		addr = &addrCopy;
	}
#endif
	al = sizeof(PRNetAddr);
	osfd = _PR_MD_ACCEPT(fd, addr, &al, timeout);
	if (osfd == -1)
		return 0;

	fd2 = PR_AllocFileDesc(osfd, PR_GetTCPMethods());
	if (!fd2) {
		_PR_MD_CLOSE_SOCKET(osfd);
		return NULL;
	}

	fd2->secret->nonblocking = fd->secret->nonblocking;
#ifdef WINNT
	fd2->secret->md.io_model_committed = PR_TRUE;
	PR_ASSERT(al == PR_NETADDR_SIZE(addr));
	fd2->secret->md.accepted_socket = PR_TRUE;
	memcpy(&fd2->secret->md.peer_addr, addr, al);
#endif

	/*
	 * On some platforms, the new socket created by accept()
	 * inherits the nonblocking (or overlapped io) attribute
	 * of the listening socket.  As an optimization, these
	 * platforms can skip the following _PR_MD_MAKE_NONBLOCK
	 * call.
	 */
#if !defined(SOLARIS) && !defined(IRIX) && !defined(WINNT)
	_PR_MD_MAKE_NONBLOCK(fd2);
#endif

	PR_ASSERT((NULL == addr) || (PR_NETADDR_SIZE(addr) == al));
#if defined(_PR_INET6)
	PR_ASSERT((NULL == addr) || (addr->raw.family == AF_INET)
		|| (addr->raw.family == AF_INET6));
#else
	PR_ASSERT((NULL == addr) || (addr->raw.family == AF_INET));
#endif

	return fd2;
}

#ifdef WINNT
PR_IMPLEMENT(PRFileDesc*) PR_NTFast_Accept(PRFileDesc *fd, PRNetAddr *addr,
PRIntervalTime timeout)
{
	PRInt32 osfd;
	PRFileDesc *fd2;
	PRIntn al;
	PRThread *me = _PR_MD_CURRENT_THREAD();
	PRNetAddr addrCopy;

	if (_PR_PENDING_INTERRUPT(me)) {
		me->flags &= ~_PR_INTERRUPT;
		PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
		return 0;
	}
	if (_PR_IO_PENDING(me)) {
		PR_SetError(PR_IO_PENDING_ERROR, 0);
		return 0;
	}

		if (addr == NULL) {
			addr = &addrCopy;
		}
		al = PR_NETADDR_SIZE(addr);
		osfd = _PR_MD_FAST_ACCEPT(fd, addr, &al, timeout, PR_TRUE, NULL, NULL);
		if (osfd == -1) {
			return 0;
		}

	fd2 = PR_AllocFileDesc(osfd, PR_GetTCPMethods());
	if (!fd2) {
		_PR_MD_CLOSE_SOCKET(osfd);
	} else {
		fd2->secret->nonblocking = fd->secret->nonblocking;
		fd2->secret->md.io_model_committed = PR_TRUE;
	        PR_ASSERT(al == PR_NETADDR_SIZE(addr));
        	fd2->secret->md.accepted_socket = PR_TRUE;
        	memcpy(&fd2->secret->md.peer_addr, addr, al);
	}
	return fd2;
}
#endif /* WINNT */


static PRStatus PR_CALLBACK SocketBind(PRFileDesc *fd, const PRNetAddr *addr)
{
	PRInt32 result;
	int one = 1;

#if defined(_PR_INET6)
	PR_ASSERT(addr->raw.family == AF_INET || addr->raw.family == AF_INET6);
#else
	PR_ASSERT(addr->raw.family == AF_INET);
#endif

#ifdef HAVE_SOCKET_REUSEADDR
	if ( setsockopt (fd->secret->md.osfd, (int)SOL_SOCKET, SO_REUSEADDR, 
	    (const void *)&one, sizeof(one) ) < 0) {
		return PR_FAILURE;
	}
#endif

	result = _PR_MD_BIND(fd, addr, PR_NETADDR_SIZE(addr));
	if (result < 0) {
		return PR_FAILURE;
	}
	return PR_SUCCESS;
}

static PRStatus PR_CALLBACK SocketListen(PRFileDesc *fd, PRIntn backlog)
{
	PRInt32 result;

	result = _PR_MD_LISTEN(fd, backlog);
	if (result < 0) {
		return PR_FAILURE;
	}
	return PR_SUCCESS;
}

static PRStatus PR_CALLBACK SocketShutdown(PRFileDesc *fd, PRIntn how)
{
	PRInt32 result;

	result = _PR_MD_SHUTDOWN(fd, how);
	if (result < 0) {
		return PR_FAILURE;
	}
	return PR_SUCCESS;
}

static PRInt32 PR_CALLBACK SocketRecv(PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags,
PRIntervalTime timeout)
{
	PRInt32 rv;
	PRThread *me = _PR_MD_CURRENT_THREAD();

	if (_PR_PENDING_INTERRUPT(me)) {
		me->flags &= ~_PR_INTERRUPT;
		PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
		return -1;
	}
	if (_PR_IO_PENDING(me)) {
		PR_SetError(PR_IO_PENDING_ERROR, 0);
		return -1;
	}

	PR_LOG(_pr_io_lm, PR_LOG_MAX, ("recv: fd=%p osfd=%d buf=%p amount=%d",
		    						fd, fd->secret->md.osfd, buf, amount));
	rv = _PR_MD_RECV(fd, buf, amount, flags, timeout);
	PR_LOG(_pr_io_lm, PR_LOG_MAX, ("recv -> %d, error = %d, os error = %d",
		rv, PR_GetError(), PR_GetOSError()));
	return rv;
}

static PRInt32 PR_CALLBACK SocketRead(PRFileDesc *fd, void *buf, PRInt32 amount)
{
	return SocketRecv(fd, buf, amount, 0, PR_INTERVAL_NO_TIMEOUT);
}

static PRInt32 PR_CALLBACK SocketSend(PRFileDesc *fd, const void *buf, PRInt32 amount,
PRIntn flags, PRIntervalTime timeout)
{
	PRInt32 temp, count;
	PRThread *me = _PR_MD_CURRENT_THREAD();

	if (_PR_PENDING_INTERRUPT(me)) {
		me->flags &= ~_PR_INTERRUPT;
		PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
		return -1;
	}
	if (_PR_IO_PENDING(me)) {
		PR_SetError(PR_IO_PENDING_ERROR, 0);
		return -1;
	}

	count = 0;
	while (amount > 0) {
		PR_LOG(_pr_io_lm, PR_LOG_MAX,
		    ("send: fd=%p osfd=%d buf=%p amount=%d",
		    fd, fd->secret->md.osfd, buf, amount));
		temp = _PR_MD_SEND(fd, buf, amount, flags, timeout);
		if (temp < 0) {
					count = -1;
					break;
				}

		count += temp;
		if (fd->secret->nonblocking) {
			break;
		}
		buf = (const void*) ((const char*)buf + temp);

		amount -= temp;
	}
	PR_LOG(_pr_io_lm, PR_LOG_MAX, ("send -> %d", count));
	return count;
}

static PRInt32 PR_CALLBACK SocketWrite(PRFileDesc *fd, const void *buf, PRInt32 amount)
{
	return SocketSend(fd, buf, amount, 0, PR_INTERVAL_NO_TIMEOUT);
}

static PRStatus PR_CALLBACK SocketClose(PRFileDesc *fd)
{
	PRInt32 rv;

	if (!fd || fd->secret->state != _PR_FILEDESC_OPEN) {
		PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
		return PR_FAILURE;
	}

	fd->secret->state = _PR_FILEDESC_CLOSED;

	rv =  _PR_MD_CLOSE_SOCKET(fd->secret->md.osfd);
	PR_FreeFileDesc(fd);
	if (rv < 0) {
		return PR_FAILURE;
	}
	return PR_SUCCESS;
}

static PRInt32 PR_CALLBACK SocketAvailable(PRFileDesc *fd)
{
	PRInt32 rv;
	rv =  _PR_MD_SOCKETAVAILABLE(fd);
	return rv;		
}

static PRInt64 PR_CALLBACK SocketAvailable64(PRFileDesc *fd)
{
    PRInt64 rv;
    LL_I2L(rv, _PR_MD_SOCKETAVAILABLE(fd));
	return rv;		
}

static PRStatus PR_CALLBACK SocketSync(PRFileDesc *fd)
{
#if defined(XP_MAC)
#pragma unused (fd)
#endif

	return PR_SUCCESS;
}

static PRInt32 PR_CALLBACK SocketSendTo(
    PRFileDesc *fd, const void *buf, PRInt32 amount,
    PRIntn flags, const PRNetAddr *addr, PRIntervalTime timeout)
{
	PRInt32 temp, count;
	PRThread *me = _PR_MD_CURRENT_THREAD();

	if (_PR_PENDING_INTERRUPT(me)) {
		me->flags &= ~_PR_INTERRUPT;
		PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
		return -1;
	}
	if (_PR_IO_PENDING(me)) {
		PR_SetError(PR_IO_PENDING_ERROR, 0);
		return -1;
	}

#if defined(_PR_INET6)
	PR_ASSERT(addr->raw.family == AF_INET || addr->raw.family == AF_INET6);
#else
	PR_ASSERT(addr->raw.family == AF_INET);
#endif

	count = 0;
	while (amount > 0) {
		temp = _PR_MD_SENDTO(fd, buf, amount, flags,
		    addr, PR_NETADDR_SIZE(addr), timeout);
		if (temp < 0) {
					count = -1;
					break;
				}
		count += temp;
		if (fd->secret->nonblocking) {
			break;
		}
		buf = (const void*) ((const char*)buf + temp);
		amount -= temp;
	}
	return count;
}

static PRInt32 PR_CALLBACK SocketRecvFrom(PRFileDesc *fd, void *buf, PRInt32 amount,
PRIntn flags, PRNetAddr *addr, PRIntervalTime timeout)
{
	PRInt32 rv;
	PRUint32 al;
	PRThread *me = _PR_MD_CURRENT_THREAD();

	if (_PR_PENDING_INTERRUPT(me)) {
		me->flags &= ~_PR_INTERRUPT;
		PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
		return -1;
	}
	if (_PR_IO_PENDING(me)) {
		PR_SetError(PR_IO_PENDING_ERROR, 0);
		return -1;
	}

	al = sizeof(PRNetAddr);
	rv = _PR_MD_RECVFROM(fd, buf, amount, flags, addr, &al, timeout);
	return rv;
}

static PRInt32 PR_CALLBACK SocketAcceptRead(PRFileDesc *sd, PRFileDesc **nd, 
PRNetAddr **raddr, void *buf, PRInt32 amount,
PRIntervalTime timeout)
{
	PRInt32 rv;
	PRThread *me = _PR_MD_CURRENT_THREAD();

	if (_PR_PENDING_INTERRUPT(me)) {
		me->flags &= ~_PR_INTERRUPT;
		PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
		return -1;
	}
	if (_PR_IO_PENDING(me)) {
		PR_SetError(PR_IO_PENDING_ERROR, 0);
		return -1;
	}
	*nd = NULL;

#if defined(WINNT)
	{
	PRInt32 newSock;
	PRNetAddr *raddrCopy;

	if (raddr == NULL) {
		raddr = &raddrCopy;
	}
	rv = _PR_MD_ACCEPT_READ(sd, &newSock, raddr, buf, amount, timeout);
	if (rv < 0) {
		rv = -1;
	} else {
		/* Successfully accepted and read; create the new PRFileDesc */
		*nd = PR_AllocFileDesc(newSock, PR_GetTCPMethods());
		if (*nd == 0) {
			_PR_MD_CLOSE_SOCKET(newSock);
			/* PR_AllocFileDesc() has invoked PR_SetError(). */
			rv = -1;
		} else {
			(*nd)->secret->md.io_model_committed = PR_TRUE;
			(*nd)->secret->md.accepted_socket = PR_TRUE;
			memcpy(&(*nd)->secret->md.peer_addr, *raddr,
				PR_NETADDR_SIZE(*raddr));
		}
	}
	}
#else
	rv = _PR_EmulateAcceptRead(sd, nd, raddr, buf, amount, timeout);
#endif
	return rv;
}

#ifdef WINNT
PR_IMPLEMENT(PRInt32) PR_NTFast_AcceptRead(PRFileDesc *sd, PRFileDesc **nd, 
PRNetAddr **raddr, void *buf, PRInt32 amount,
PRIntervalTime timeout)
{
	PRInt32 rv;
	PRInt32 newSock;
	PRThread *me = _PR_MD_CURRENT_THREAD();
	PRNetAddr *raddrCopy;

	if (_PR_PENDING_INTERRUPT(me)) {
		me->flags &= ~_PR_INTERRUPT;
		PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
		return -1;
	}
	if (_PR_IO_PENDING(me)) {
		PR_SetError(PR_IO_PENDING_ERROR, 0);
		return -1;
	}
	*nd = NULL;

	if (raddr == NULL) {
		raddr = &raddrCopy;
	}
	rv = _PR_MD_FAST_ACCEPT_READ(sd, &newSock, raddr, buf, amount, 
	    timeout, PR_TRUE, NULL, NULL);
	if (rv < 0) {
		rv = -1;
	} else {
		/* Successfully accepted and read; create the new PRFileDesc */
		*nd = PR_AllocFileDesc(newSock, PR_GetTCPMethods());
		if (*nd == 0) {
			_PR_MD_CLOSE_SOCKET(newSock);
			/* PR_AllocFileDesc() has invoked PR_SetError(). */
			rv = -1;
		} else {
			(*nd)->secret->md.io_model_committed = PR_TRUE;
			(*nd)->secret->md.accepted_socket = PR_TRUE;
			memcpy(&(*nd)->secret->md.peer_addr, *raddr,
				PR_NETADDR_SIZE(*raddr));
		}
	}
	return rv;
}

PR_IMPLEMENT(PRInt32) PR_NTFast_AcceptRead_WithTimeoutCallback(
PRFileDesc *sd, PRFileDesc **nd, 
PRNetAddr **raddr, void *buf, PRInt32 amount,
PRIntervalTime timeout,
_PR_AcceptTimeoutCallback callback,
void *callbackArg)
{
	PRInt32 rv;
	PRInt32 newSock;
	PRThread *me = _PR_MD_CURRENT_THREAD();
	PRNetAddr *raddrCopy;

	if (_PR_PENDING_INTERRUPT(me)) {
		me->flags &= ~_PR_INTERRUPT;
		PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
		return -1;
	}
	if (_PR_IO_PENDING(me)) {
		PR_SetError(PR_IO_PENDING_ERROR, 0);
		return -1;
	}
	*nd = NULL;

	if (raddr == NULL) {
		raddr = &raddrCopy;
	}
	rv = _PR_MD_FAST_ACCEPT_READ(sd, &newSock, raddr, buf, amount,
	    timeout, PR_TRUE, callback, callbackArg);
	if (rv < 0) {
		rv = -1;
	} else {
		/* Successfully accepted and read; create the new PRFileDesc */
		*nd = PR_AllocFileDesc(newSock, PR_GetTCPMethods());
		if (*nd == 0) {
			_PR_MD_CLOSE_SOCKET(newSock);
			/* PR_AllocFileDesc() has invoked PR_SetError(). */
			rv = -1;
		} else {
			(*nd)->secret->md.io_model_committed = PR_TRUE;
			(*nd)->secret->md.accepted_socket = PR_TRUE;
			memcpy(&(*nd)->secret->md.peer_addr, *raddr,
				PR_NETADDR_SIZE(*raddr));
		}
	}
	return rv;
}
#endif /* WINNT */

#ifdef WINNT
PR_IMPLEMENT(void)
PR_NTFast_UpdateAcceptContext(PRFileDesc *socket, PRFileDesc *acceptSocket)
{
	_PR_MD_UPDATE_ACCEPT_CONTEXT(
		socket->secret->md.osfd, acceptSocket->secret->md.osfd);
}
#endif /* WINNT */

static PRInt32 PR_CALLBACK SocketTransmitFile(PRFileDesc *sd, PRFileDesc *fd, 
const void *headers, PRInt32 hlen, PRTransmitFileFlags flags,
PRIntervalTime timeout)
{
	PRInt32 rv;
	PRThread *me = _PR_MD_CURRENT_THREAD();

	if (_PR_PENDING_INTERRUPT(me)) {
		me->flags &= ~_PR_INTERRUPT;
		PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
		return -1;
	}
	if (_PR_IO_PENDING(me)) {
		PR_SetError(PR_IO_PENDING_ERROR, 0);
		return -1;
	}
#if defined(WINNT)
	rv = _PR_MD_TRANSMITFILE(
		sd, fd,
		headers, hlen, flags, timeout);
	if (rv < 0) {
		rv = -1;
	}
	if (flags & PR_TRANSMITFILE_CLOSE_SOCKET) {
		/*
		 * This should be kept the same as SocketClose, except
		 * that _PR_MD_CLOSE_SOCKET(sd->secret->md.osfd) should
		 * not be called because the socket will be recycled.
		 */
		sd->secret->state = _PR_FILEDESC_CLOSED;
		PR_FreeFileDesc(sd);
	}
#else
#if defined(XP_UNIX)
	/*
	 * On HPUX11, we could call _PR_HPUXTransmitFile(), but that
	 * would require that we not override the malloc() functions.
	 */
	rv = _PR_UnixTransmitFile(sd, fd, headers, hlen, flags, timeout);
#else	/* XP_UNIX */
	rv = _PR_EmulateTransmitFile(sd, fd, headers, hlen, flags,
	    timeout);
#endif	/* XP_UNIX */
#endif	/* WINNT */

	return rv;
}

static PRStatus PR_CALLBACK SocketGetName(PRFileDesc *fd, PRNetAddr *addr)
{
	PRInt32 result;
	PRUint32 addrlen;

	addrlen = sizeof(PRNetAddr);
	result = _PR_MD_GETSOCKNAME(fd, addr, &addrlen);
	if (result < 0) {
		return PR_FAILURE;
	}
	PR_ASSERT(addrlen == PR_NETADDR_SIZE(addr));
#if defined(_PR_INET6)
	PR_ASSERT(addr->raw.family == AF_INET || addr->raw.family == AF_INET6);
#else
	PR_ASSERT(addr->raw.family == AF_INET);
#endif
	return PR_SUCCESS;
}

static PRStatus PR_CALLBACK SocketGetPeerName(PRFileDesc *fd, PRNetAddr *addr)
{
	PRInt32 result;
	PRUint32 addrlen;

	addrlen = sizeof(PRNetAddr);
	result = _PR_MD_GETPEERNAME(fd, addr, &addrlen);
	if (result < 0) {
		return PR_FAILURE;
	}
	PR_ASSERT(addrlen == PR_NETADDR_SIZE(addr));
#if defined(_PR_INET6)
	PR_ASSERT(addr->raw.family == AF_INET || addr->raw.family == AF_INET6);
#else
	PR_ASSERT(addr->raw.family == AF_INET);
#endif
	return PR_SUCCESS;
}

static PRStatus PR_CALLBACK SocketGetSockOpt(
    PRFileDesc *fd, PRSockOption optname, void* optval, PRInt32* optlen)
{
    PRInt32 level, name;
    PRStatus rv;

    /*
     * PR_SockOpt_Nonblocking is a special case that does not
     * translate to a getsockopt() call
     */
    if (PR_SockOpt_Nonblocking == optname)
    {
        PR_ASSERT(sizeof(PRIntn) <= *optlen);
        *((PRIntn *) optval) = (PRIntn) fd->secret->nonblocking;
        *optlen = sizeof(PRIntn);
        return PR_SUCCESS;
    }

    rv = _PR_MapOptionName(optname, &level, &name);
    if (PR_SUCCESS == rv)
    {
        if (PR_SockOpt_Linger == optname)
        {
            struct linger linger;
            PRInt32 len = sizeof(linger);
            rv = _PR_MD_GETSOCKOPT(
                fd, level, name, (char *) &linger, &len);
            if (PR_SUCCESS == rv)
            {
                ((PRLinger*)(optval))->polarity = linger.l_onoff
                    ? PR_TRUE : PR_FALSE;
                ((PRLinger*)(optval))->linger = PR_SecondsToInterval(
                    linger.l_linger);
                *optlen = sizeof(PRLinger);
            }
        }
        else
        {
            rv = _PR_MD_GETSOCKOPT(
                fd, level, name, optval, optlen);
        }
    }
    return rv;
}

static PRStatus PR_CALLBACK SocketSetSockOpt(
    PRFileDesc *fd, PRSockOption optname, const void* optval, PRInt32 optlen)
{
	PRInt32 level, name;
    PRStatus rv;

    /*
     * PR_SockOpt_Nonblocking is a special case that does not
     * translate to a setsockopt call.
     */
    if (PR_SockOpt_Nonblocking == optname)
    {
        PRBool fNonblocking = *((PRIntn *) optval) ? PR_TRUE : PR_FALSE;
        PR_ASSERT(sizeof(PRIntn) == optlen);
#ifdef WINNT
        PR_ASSERT((fd->secret->md.io_model_committed == PR_FALSE)
            || (fd->secret->nonblocking == fNonblocking));
        if (fd->secret->md.io_model_committed
            && (fd->secret->nonblocking != fNonblocking))
        {
            /*
             * On NT, once we have associated a socket with the io
             * completion port, we can't disassociate it.  So we
             * can't change the nonblocking option of the socket
             * afterwards.
             */
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
            return PR_FAILURE;
        }
#endif
        fd->secret->nonblocking = fNonblocking;
        return PR_SUCCESS;
    }

    rv = _PR_MapOptionName(optname, &level, &name);
    if (PR_SUCCESS == rv)
    {
        if (PR_SockOpt_Linger == optname)
        {
            struct linger linger;
            linger.l_onoff = ((PRLinger*)(optval))->polarity ? 1 : 0;
            linger.l_linger = PR_IntervalToSeconds(
                ((PRLinger*)(optval))->linger);
            rv = _PR_MD_SETSOCKOPT(
                fd, level, name, (char *) &linger, sizeof(linger));
        }
        else
        {
            rv = _PR_MD_SETSOCKOPT(
                fd, level, name, optval, optlen);
        }
    }
    return rv;
}

static PRIOMethods tcpMethods = {
	PR_DESC_SOCKET_TCP,
	SocketClose,
	SocketRead,
	SocketWrite,
	SocketAvailable,
	SocketAvailable64,
	SocketSync,
	(PRSeekFN)_PR_InvalidInt,
	(PRSeek64FN)_PR_InvalidInt64,
	(PRFileInfoFN)_PR_InvalidStatus,
	(PRFileInfo64FN)_PR_InvalidStatus,
	SocketWritev,
	SocketConnect,
	SocketAccept,
	SocketBind,
	SocketListen,
	SocketShutdown,
	SocketRecv,
	SocketSend,
	(PRRecvfromFN)_PR_InvalidInt,
	(PRSendtoFN)_PR_InvalidInt,
	(PRPollFN)0,
	SocketAcceptRead,
	SocketTransmitFile,
	SocketGetName,
	SocketGetPeerName,
	SocketGetSockOpt,
	SocketSetSockOpt,
	_PR_SocketGetSocketOption,
	_PR_SocketSetSocketOption
};

static PRIOMethods udpMethods = {
	PR_DESC_SOCKET_UDP,
	SocketClose,
	SocketRead,
	SocketWrite,
	SocketAvailable,
	SocketAvailable64,
	SocketSync,
	(PRSeekFN)_PR_InvalidInt,
	(PRSeek64FN)_PR_InvalidInt64,
	(PRFileInfoFN)_PR_InvalidStatus,
	(PRFileInfo64FN)_PR_InvalidStatus,
	SocketWritev,
	SocketConnect,
	(PRAcceptFN)_PR_InvalidDesc,
	SocketBind,
	SocketListen,
	SocketShutdown,
	SocketRecv,
	SocketSend,
	SocketRecvFrom,
	SocketSendTo,
	(PRPollFN)0,
	(PRAcceptreadFN)_PR_InvalidInt,
	(PRTransmitfileFN)_PR_InvalidInt,
	SocketGetName,
	SocketGetPeerName,
	SocketGetSockOpt,
	SocketSetSockOpt,
	_PR_SocketGetSocketOption,
	_PR_SocketSetSocketOption
};

PR_IMPLEMENT(PRIOMethods*) PR_GetTCPMethods()
{
	return &tcpMethods;
}

PR_IMPLEMENT(PRIOMethods*) PR_GetUDPMethods()
{
	return &udpMethods;
}

PR_IMPLEMENT(PRFileDesc*) PR_Socket(PRInt32 domain, PRInt32 type, PRInt32 proto)
{
	PRInt32 osfd;
	int one = 1;
	PRFileDesc *fd;

	if (!_pr_initialized) _PR_ImplicitInitialization();
	if (AF_INET != domain
#if defined(_PR_INET6)
			&& AF_INET6 != domain
#endif
			) {
		PR_SetError(PR_ADDRESS_NOT_SUPPORTED_ERROR, 0);
		return NULL;
	}
	osfd = _PR_MD_SOCKET(domain, type, proto);
	if (osfd == -1) {
		return 0;
	}
#ifdef HAVE_SOCKET_KEEPALIVE
	/* "Keep-alive" packets are specific to TCP. */
	if (domain == AF_INET && type == SOCK_STREAM) {
		if (setsockopt(osfd, (int)SOL_SOCKET, SO_KEEPALIVE,
		    (const void *) &one, sizeof(one) ) < 0) {
			_PR_MD_CLOSE_SOCKET(osfd);
			return 0;
		}
	}
#endif
	if (type == SOCK_STREAM)
		fd = PR_AllocFileDesc(osfd, PR_GetTCPMethods());
	else
		fd = PR_AllocFileDesc(osfd, PR_GetUDPMethods());
	/*
	 * Make the sockets non-blocking
	 */
	if (fd != NULL)
		_PR_MD_MAKE_NONBLOCK(fd);
	else
		_PR_MD_CLOSE_SOCKET(osfd);
	return fd;
}

PR_IMPLEMENT(PRFileDesc *) PR_NewTCPSocket(void)
{
	PRInt32 domain = AF_INET;

#if defined(_PR_INET6)
	if (_pr_ipv6_enabled) {
		domain = AF_INET6;
	}
#endif
	return PR_Socket(domain, SOCK_STREAM, 0);
}

PR_IMPLEMENT(PRFileDesc*) PR_NewUDPSocket(void)
{
	PRInt32 domain = AF_INET;

#if defined(_PR_INET6)
	if (_pr_ipv6_enabled) {
		domain = AF_INET6;
	}
#endif
	return PR_Socket(domain, SOCK_DGRAM, 0);
}

PR_IMPLEMENT(PRStatus) PR_NewTCPSocketPair(PRFileDesc *f[])
{
#ifdef XP_UNIX
	PRInt32 rv, osfd[2];

	rv = _PR_MD_SOCKETPAIR(AF_UNIX, SOCK_STREAM, 0, osfd);
	if (rv == -1) {
		return PR_FAILURE;
	}

	f[0] = PR_AllocFileDesc(osfd[0], PR_GetTCPMethods());
	if (!f[0]) {
		_PR_MD_CLOSE_SOCKET(osfd[0]);
		_PR_MD_CLOSE_SOCKET(osfd[1]);
		/* PR_AllocFileDesc() has invoked PR_SetError(). */
		return PR_FAILURE;
	}
	f[1] = PR_AllocFileDesc(osfd[1], PR_GetTCPMethods());
	if (!f[1]) {
		PR_Close(f[0]);
		_PR_MD_CLOSE_SOCKET(osfd[1]);
		/* PR_AllocFileDesc() has invoked PR_SetError(). */
		return PR_FAILURE;
	}
	_PR_MD_MAKE_NONBLOCK(f[0]);
	_PR_MD_MAKE_NONBLOCK(f[1]);
	return PR_SUCCESS;
#endif

	/* XXX: this needs to be implemented for MAC and NT */
#ifdef XP_MAC
#pragma unused (f)

	PR_SetError(PR_NOT_IMPLEMENTED_ERROR, unimpErr);
	return PR_FAILURE;
#endif

#ifdef XP_PC
	PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
	return PR_FAILURE;
#endif
}

PR_IMPLEMENT(PRInt32)
PR_FileDesc2NativeHandle(PRFileDesc *fd)
{
	if (fd) {
		/*
		 * The fd may be layered.  Chase the links to the
		 * bottom layer to get the osfd.
		 */
		PRFileDesc *bottom = fd;
		while (bottom->lower != NULL) {
			bottom = bottom->lower;
		}
		return bottom->secret->md.osfd;
	} else {
		PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
		return -1;
	}
}

PR_IMPLEMENT(void)
PR_ChangeFileDescNativeHandle(PRFileDesc *fd, PRInt32 handle)
{
	if (fd)
		fd->secret->md.osfd = handle;
}

/*
 * _PR_EmulateTransmitFile
 *
 *	Send file fd across socket sd. If headers is non-NULL, 'hlen'
 *	bytes of headers is sent before sending the file.
 *
 *	PR_TRANSMITFILE_CLOSE_SOCKET flag - close socket after sending file
 *	
 *	return number of bytes sent or -1 on error
 *
 */

PRInt32 _PR_EmulateTransmitFile(PRFileDesc *sd, PRFileDesc *fd, 
const void *headers, PRInt32 hlen, PRTransmitFileFlags flags,
PRIntervalTime timeout)
{
	PRInt32 rv, count = 0;
	PRInt32 rlen;
	PRThread *me = _PR_MD_CURRENT_THREAD();
	char *buf = NULL;
#define _TRANSMITFILE_BUFSIZE	(16 * 1024)

	if (_PR_PENDING_INTERRUPT(me)) {
		me->flags &= ~_PR_INTERRUPT;
		PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
		return -1;
	}

	buf = PR_MALLOC(_TRANSMITFILE_BUFSIZE);
	if (buf == NULL) {
		PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
		return -1;
	}

	/*
	 * send headers, first
	 */
	while (hlen) {
		rv =  PR_Send(sd, headers, hlen, 0, timeout);
		if (rv < 0) {
			/* PR_Send() has invoked PR_SetError(). */
			rv = -1;
			goto done;
		} else {
			count += rv;
			headers = (const void*) ((const char*)headers + rv);
			hlen -= rv;
		}
	}
	/*
	 * send file, next
	 */
	while ((rlen = PR_Read(fd, buf, _TRANSMITFILE_BUFSIZE)) > 0) {
		while (rlen) {
			char *bufptr = buf;

			rv =  PR_Send(sd, bufptr, rlen,0,PR_INTERVAL_NO_TIMEOUT);
			if (rv < 0) {
				/* PR_Send() has invoked PR_SetError(). */
				rv = -1;
				goto done;
			} else {
				count += rv;
				bufptr = ((char*)bufptr + rv);
				rlen -= rv;
			}
		}
	}
	if (rlen == 0) {
		/*
		 * end-of-file
		 */
		if (flags & PR_TRANSMITFILE_CLOSE_SOCKET)
			PR_Close(sd);
		rv = count;
	} else {
		PR_ASSERT(rlen < 0);
		/* PR_Read() has invoked PR_SetError(). */
		rv = -1;
	}

done:
	if (buf)
		PR_DELETE(buf);
	return rv;
}

/*
 * _PR_EmulateAcceptRead
 *
 *	Accept an incoming connection on sd, set *nd to point to the
 *	newly accepted socket, read 'amount' bytes from the accepted
 *	socket.
 *
 *	buf is a buffer of length = (amount + sizeof(PRNetAddr))
 *	*raddr points to the PRNetAddr of the accepted connection upon
 *	return
 *
 *	return number of bytes read or -1 on error
 *
 */
PRInt32 _PR_EmulateAcceptRead(PRFileDesc *sd, PRFileDesc **nd, 
PRNetAddr **raddr, void *buf, PRInt32 amount, PRIntervalTime timeout)
{
	PRInt32 rv;
	PRFileDesc *newsockfd;
	PRNetAddr remote;
	PRIntervalTime start, elapsed;

	if (PR_INTERVAL_NO_TIMEOUT != timeout) {
		start = PR_IntervalNow();
	}
	if ((newsockfd = PR_Accept(sd, &remote, timeout)) == NULL) {
		return -1;
	}

	if (PR_INTERVAL_NO_TIMEOUT != timeout) {
		elapsed = (PRIntervalTime) (PR_IntervalNow() - start);
		if (elapsed > timeout) {
			PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
			goto failed;
		} else {
			timeout = timeout - elapsed;
		}
	}

	rv = PR_Recv(newsockfd, buf, amount, 0, timeout);
	if (rv >= 0) {
		*nd = newsockfd;
		*raddr = (PRNetAddr *)((char *) buf + amount);
		memcpy(*raddr, &remote, PR_NETADDR_SIZE(&remote));
		return rv;
	}

failed:
	PR_Close(newsockfd);
	return -1;
}

/*
** Select compatibility
**
*/

PR_IMPLEMENT(void) PR_FD_ZERO(PR_fd_set *set)
{
	memset(set, 0, sizeof(PR_fd_set));
}

PR_IMPLEMENT(void) PR_FD_SET(PRFileDesc *fh, PR_fd_set *set)
{
	PR_ASSERT( set->hsize < PR_MAX_SELECT_DESC );

	set->harray[set->hsize++] = fh;
}

PR_IMPLEMENT(void) PR_FD_CLR(PRFileDesc *fh, PR_fd_set *set)
{
	PRUint32 index, index2;

	for (index = 0; index<set->hsize; index++)
		if (set->harray[index] == fh) {
			for (index2=index; index2 < (set->hsize-1); index2++) {
				set->harray[index2] = set->harray[index2+1];
			}
			set->hsize--;
			break;
		}
}

PR_IMPLEMENT(PRInt32) PR_FD_ISSET(PRFileDesc *fh, PR_fd_set *set)
{
	PRUint32 index;
	for (index = 0; index<set->hsize; index++)
		if (set->harray[index] == fh) {
			return 1;
		}
	return 0;
}

PR_IMPLEMENT(void) PR_FD_NSET(PRInt32 fd, PR_fd_set *set)
{
	PR_ASSERT( set->nsize < PR_MAX_SELECT_DESC );

	set->narray[set->nsize++] = fd;
}

PR_IMPLEMENT(void) PR_FD_NCLR(PRInt32 fd, PR_fd_set *set)
{
	PRUint32 index, index2;

	for (index = 0; index<set->nsize; index++)
		if (set->narray[index] == fd) {
			for (index2=index; index2 < (set->nsize-1); index2++) {
				set->narray[index2] = set->narray[index2+1];
			}
			set->nsize--;
			break;
		}
}

PR_IMPLEMENT(PRInt32) PR_FD_NISSET(PRInt32 fd, PR_fd_set *set)
{
	PRUint32 index;
	for (index = 0; index<set->nsize; index++)
		if (set->narray[index] == fd) {
			return 1;
		}
	return 0;
}


#if !defined(NEED_SELECT)
#if !defined(XP_MAC)
#include "obsolete/probslet.h"
#else
#include "probslet.h"
#endif

#define PD_INCR 20

static PRPollDesc *_pr_setfd(
    PR_fd_set *set, PRInt16 flags, PRPollDesc *polldesc)
{
    PRUintn fsidx, pdidx;
    PRPollDesc *poll = polldesc;

    if (NULL == set) return poll;

	/* First set the pr file handle osfds */
	for (fsidx = 0; fsidx < set->hsize; fsidx++)
	{
	    for (pdidx = 0; 1; pdidx++)
        {
            if ((PRFileDesc*)-1 == poll[pdidx].fd)
            {
                /* our vector is full - extend and condition it */
                poll = PR_Realloc(
                    poll, (pdidx + 1 + PD_INCR) * sizeof(PRPollDesc));
                if (NULL == poll) goto out_of_memory;
                memset(
                    poll + pdidx * sizeof(PRPollDesc),
                    0, PD_INCR * sizeof(PRPollDesc));
                poll[pdidx + PD_INCR].fd = (PRFileDesc*)-1;
            }
            if ((NULL == poll[pdidx].fd)
            || (poll[pdidx].fd == set->harray[fsidx]))
            {
                /* PR_ASSERT(0 == (poll[pdidx].in_flags & flags)); */
                /* either empty or prevously defined */
                poll[pdidx].fd = set->harray[fsidx];  /* possibly redundant */
                poll[pdidx].in_flags |= flags;  /* possibly redundant */
                break;
            }
        }
	}

#if 0
	/* Second set the native osfds */
	for (fsidx = 0; fsidx < set->nsize; fsidx++)
	{
	    for (pdidx = 0; ((PRFileDesc*)-1 != poll[pdidx].fd); pdidx++)
        {
            if ((PRFileDesc*)-1 == poll[pdidx].fd)
            {
                /* our vector is full - extend and condition it */
                poll = PR_Realloc(
                    poll, (pdidx + PD_INCR) * sizeof(PRPollDesc));
                if (NULL == poll) goto out_of_memory;
                memset(
                    poll + pdidx * sizeof(PRPollDesc),
                    0, PD_INCR * sizeof(PRPollDesc));
                poll[(pdidx + PD_INCR)].fd = (PRFileDesc*)-1;
            }
            if ((NULL == poll[pdidx].fd)
            || (poll[pdidx].fd == set->narray[fsidx]))
            {
                /* either empty or prevously defined */
                poll[pdidx].fd = set->narray[fsidx];
                PR_ASSERT(0 == (poll[pdidx].in_flags & flags));
                poll[pdidx].in_flags |= flags;
                break;
            }
        }
	}
#endif /* 0 */

	return poll;

out_of_memory:
    if (NULL != polldesc) PR_DELETE(polldesc);
    return NULL;
}  /* _pr_setfd */

#endif /* !defined(NEED_SELECT) */

PR_IMPLEMENT(PRInt32) PR_Select(
    PRInt32 unused, PR_fd_set *pr_rd, PR_fd_set *pr_wr, 
    PR_fd_set *pr_ex, PRIntervalTime timeout)
{

#if !defined(NEED_SELECT)
    PRInt32 npds = 0; 
    /*
    ** Find out how many fds are represented in the three lists.
    ** Then allocate a polling descriptor for the logical union
    ** (there can't be any overlapping) and call PR_Poll().
    */

    PRPollDesc *copy, *poll;

    static PRBool warning = PR_TRUE;
    if (warning) warning = _PR_Obsolete( "PR_Select()", "PR_Poll()");

    /* try to get an initial guesss at how much space we need */
    npds = 0;
    if ((NULL != pr_rd) && ((pr_rd->hsize + pr_rd->nsize - npds) > 0))
        npds = pr_rd->hsize + pr_rd->nsize;
    if ((NULL != pr_wr) && ((pr_wr->hsize + pr_wr->nsize - npds) > 0))
        npds = pr_wr->hsize + pr_wr->nsize;
    if ((NULL != pr_ex) && ((pr_ex->hsize + pr_ex->nsize - npds) > 0))
        npds = pr_ex->hsize + pr_ex->nsize;

    if (0 == npds)
    {
        PR_Sleep(timeout);
        return 0;
    }

    copy = poll = PR_Calloc(npds + PD_INCR, sizeof(PRPollDesc));
    if (NULL == poll) goto out_of_memory;
    poll[npds + PD_INCR - 1].fd = (PRFileDesc*)-1;

    poll = _pr_setfd(pr_rd, PR_POLL_READ, poll);
    if (NULL == poll) goto out_of_memory;
    poll = _pr_setfd(pr_wr, PR_POLL_WRITE, poll);
    if (NULL == poll) goto out_of_memory;
    poll = _pr_setfd(pr_ex, PR_POLL_EXCEPT, poll);
    if (NULL == poll) goto out_of_memory;
    unused = 0;
    while (NULL != poll[unused].fd && (PRFileDesc*)-1 != poll[unused].fd)
    {
        ++unused;
    }

    PR_ASSERT(unused > 0);
    npds = PR_Poll(poll, unused, timeout);

    if (npds > 0)
    {
        /* Copy the results back into the fd sets */
        if (NULL != pr_rd) pr_rd->nsize = pr_rd->hsize = 0;
        if (NULL != pr_wr) pr_wr->nsize = pr_wr->hsize = 0;
        if (NULL != pr_ex) pr_ex->nsize = pr_ex->hsize = 0;
        for (copy = &poll[unused - 1]; copy >= poll; --copy)
        {
            if (copy->out_flags & PR_POLL_NVAL)
            {
                PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
                npds = -1;
                break;
            }
            if (copy->out_flags & PR_POLL_READ)
                if (NULL != pr_rd) pr_rd->harray[pr_rd->hsize++] = copy->fd;
            if (copy->out_flags & PR_POLL_WRITE)
                if (NULL != pr_wr) pr_wr->harray[pr_wr->hsize++] = copy->fd;
            if (copy->out_flags & PR_POLL_EXCEPT)
                if (NULL != pr_ex) pr_ex->harray[pr_ex->hsize++] = copy->fd;
        }
    }
    PR_DELETE(poll);

    return npds;
out_of_memory:
    PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    return -1;    

#endif /* !defined(NEED_SELECT) */
    
}
