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

#include <string.h>

/************************************************************************/

/* These two functions are only used in assertions. */
#if defined(DEBUG)

static PRBool IsValidNetAddr(const PRNetAddr *addr)
{
    if ((addr != NULL)
#ifdef XP_UNIX
	    && (addr->raw.family != AF_UNIX)
#endif
#ifdef _PR_INET6
	    && (addr->raw.family != AF_INET6)
#endif
	    && (addr->raw.family != AF_INET)) {
        return PR_FALSE;
    }
    return PR_TRUE;
}

static PRBool IsValidNetAddrLen(const PRNetAddr *addr, PRInt32 addr_len)
{
    /*
     * The definition of the length of a Unix domain socket address
     * is not uniform, so we don't check it.
     */
    if ((addr != NULL)
#ifdef XP_UNIX
            && (addr->raw.family != AF_UNIX)
#endif
            && (PR_NETADDR_SIZE(addr) != addr_len)) {
        /*
         * The accept(), getsockname(), etc. calls on some platforms
         * do not set the actual socket address length on return.
         * In this case, we verifiy addr_len is still the value we
         * passed in (i.e., sizeof(PRNetAddr)).
         */
#if defined(QNX)
        if (sizeof(PRNetAddr) != addr_len) {
            return PR_FALSE;
        } else {
            return PR_TRUE;
        }
#else
        return PR_FALSE;
#endif
    }
    return PR_TRUE;
}

#endif /* DEBUG */

static PRInt32 PR_CALLBACK SocketWritev(PRFileDesc *fd, const PRIOVec *iov,
PRInt32 iov_size, PRIntervalTime timeout)
{
	PRThread *me = _PR_MD_CURRENT_THREAD();
	int w = 0;
	const PRIOVec *tmp_iov;
#define LOCAL_MAXIOV    8
	PRIOVec local_iov[LOCAL_MAXIOV];
	PRIOVec *iov_copy = NULL;
	int tmp_out;
	int index, iov_cnt;
	int count=0, sz = 0;    /* 'count' is the return value. */

	if (_PR_PENDING_INTERRUPT(me)) {
		me->flags &= ~_PR_INTERRUPT;
		PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
		return -1;
	}
	if (_PR_IO_PENDING(me)) {
		PR_SetError(PR_IO_PENDING_ERROR, 0);
		return -1;
	}

    /*
     * Assume the first writev will succeed.  Copy iov's only on
     * failure.
     */
    tmp_iov = iov;
    for (index = 0; index < iov_size; index++)
        sz += iov[index].iov_len;

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

			if (tmp_iov == iov) {
				/*
				 * The first writev failed so we
				 * must copy iov's around.
				 * Avoid calloc/free if there
				 * are few enough iov's.
				 */
				if (iov_size - index <= LOCAL_MAXIOV)
					iov_copy = local_iov;
				else if ((iov_copy = (PRIOVec *) PR_CALLOC((iov_size - index) *
					sizeof *iov_copy)) == NULL) {
					PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
					return -1;
				}
				tmp_iov = iov_copy;
			}

			PR_ASSERT(tmp_iov == iov_copy);

			/* fill in the first partial read */
			iov_copy[0].iov_base = &(((char *)iov[index].iov_base)[tmp_out]);
			iov_copy[0].iov_len = iov[index].iov_len - tmp_out;
			index++;

			/* copy the remaining vectors */
			for (iov_cnt=1; index<iov_size; iov_cnt++, index++) {
				iov_copy[iov_cnt].iov_base = iov[index].iov_base;
				iov_copy[iov_cnt].iov_len = iov[index].iov_len;
			}
		}
	}

	if (iov_copy != local_iov)
		PR_DELETE(iov_copy);
	return count;
}

/************************************************************************/

PR_IMPLEMENT(PRFileDesc *) PR_ImportTCPSocket(PRInt32 osfd)
{
PRFileDesc *fd;

	if (!_pr_initialized) _PR_ImplicitInitialization();
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

	if (!_pr_initialized) _PR_ImplicitInitialization();
	fd = PR_AllocFileDesc(osfd, PR_GetUDPMethods());
	if (fd != NULL)
		_PR_MD_MAKE_NONBLOCK(fd);
	else
		_PR_MD_CLOSE_SOCKET(osfd);
	return(fd);
}


static const PRIOMethods* PR_GetSocketPollFdMethods();

PR_IMPLEMENT(PRFileDesc*) PR_CreateSocketPollFd(PRInt32 osfd)
{
    PRFileDesc *fd;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    fd = _PR_Getfd();

    if (fd == NULL) PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    else
    {
        fd->secret->md.osfd = osfd;
        fd->secret->inheritable = PR_FALSE;
    	fd->secret->state = _PR_FILEDESC_OPEN;
        fd->methods = PR_GetSocketPollFdMethods();
    }

    return fd;
}  /* PR_CreateSocketPollFD */

PR_IMPLEMENT(PRStatus) PR_DestroySocketPollFd(PRFileDesc *fd)
{
    if (NULL == fd)
    {
        PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
        return PR_FAILURE;
    }
    fd->secret->state = _PR_FILEDESC_CLOSED;
    _PR_Putfd(fd);
    return PR_SUCCESS;
}  /* PR_DestroySocketPollFd */

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

#if defined(WIN32)
    /*
     * The sleep circumvents a bug in Win32 WinSock.
     * See Microsoft Knowledge Base article ID: Q165989.
     */
    Sleep(0);
#endif /* WIN32 */

    if (pd->out_flags & PR_POLL_EXCEPT) {
        int len = sizeof(err);
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

#elif defined(XP_BEOS)

    err = _MD_beos_get_nonblocking_connect_error(bottom);
    if( err != 0 ) {
	_PR_MD_MAP_CONNECT_ERROR(err);
	return PR_FAILURE;
    }
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
	fd2->secret->inheritable = fd->secret->inheritable;
#ifdef WINNT
	if (!fd2->secret->nonblocking && !fd2->secret->inheritable) {
		/*
		 * The new socket has been associated with an I/O
		 * completion port.  There is no going back.
		 */
		fd2->secret->md.io_model_committed = PR_TRUE;
	}
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

	PR_ASSERT(IsValidNetAddr(addr) == PR_TRUE);
	PR_ASSERT(IsValidNetAddrLen(addr, al) == PR_TRUE);

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

	PR_ASSERT(IsValidNetAddr(addr) == PR_TRUE);

#ifdef XP_UNIX
	if (addr->raw.family == AF_UNIX) {
		/* Disallow relative pathnames */
		if (addr->local.path[0] != '/') {
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
			return PR_FAILURE;
		}
	} else {
#endif

#ifdef HAVE_SOCKET_REUSEADDR
	if ( setsockopt (fd->secret->md.osfd, (int)SOL_SOCKET, SO_REUSEADDR, 
#ifdef XP_OS2_VACPP
	    (char *)&one, sizeof(one) ) < 0) {
#else
	    (const void *)&one, sizeof(one) ) < 0) {
#endif
		return PR_FAILURE;
	}
#endif

#ifdef XP_UNIX
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
	if (!fd || !fd->secret
			|| (fd->secret->state != _PR_FILEDESC_OPEN
			&& fd->secret->state != _PR_FILEDESC_CLOSED)) {
		PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
		return PR_FAILURE;
	}

	if (fd->secret->state == _PR_FILEDESC_OPEN) {
		if (_PR_MD_CLOSE_SOCKET(fd->secret->md.osfd) < 0) {
			return PR_FAILURE;
		}
		fd->secret->state = _PR_FILEDESC_CLOSED;
	}

	PR_FreeFileDesc(fd);
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

	PR_ASSERT(IsValidNetAddr(addr) == PR_TRUE);

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
	/* The socket must be in blocking mode. */
	if (sd->secret->nonblocking) {
		PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
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
	/* The socket must be in blocking mode. */
	if (sd->secret->nonblocking) {
		PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
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
	PR_ASSERT(IsValidNetAddr(addr) == PR_TRUE);
	PR_ASSERT(IsValidNetAddrLen(addr, addrlen) == PR_TRUE);
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
	PR_ASSERT(IsValidNetAddr(addr) == PR_TRUE);
	PR_ASSERT(IsValidNetAddrLen(addr, addrlen) == PR_TRUE);
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
#if !defined(XP_BEOS)
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
#else
            PR_SetError( PR_NOT_IMPLEMENTED_ERROR, 0 );
            return PR_FAILURE;
#endif
        }
        else
        {
            rv = _PR_MD_GETSOCKOPT(
                fd, level, name, (char*)optval, optlen);
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
#if !defined(XP_BEOS)
            struct linger linger;
            linger.l_onoff = ((PRLinger*)(optval))->polarity ? 1 : 0;
            linger.l_linger = PR_IntervalToSeconds(
                ((PRLinger*)(optval))->linger);
            rv = _PR_MD_SETSOCKOPT(
                fd, level, name, (char *) &linger, sizeof(linger));
#else
            PR_SetError( PR_NOT_IMPLEMENTED_ERROR, 0 );
            return PR_FAILURE;
#endif
        }
        else
        {
            rv = _PR_MD_SETSOCKOPT(
                fd, level, name, (const char*)optval, optlen);
        }
    }
    return rv;
}

static PRInt16 PR_CALLBACK SocketPoll(
    PRFileDesc *fd, PRInt16 in_flags, PRInt16 *out_flags)
{
#ifdef XP_MAC
#pragma unused( fd, in_flags )
#endif
    *out_flags = 0;
    return in_flags;
}  /* SocketPoll */

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
	SocketPoll,
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
	SocketPoll,
	(PRAcceptreadFN)_PR_InvalidInt,
	(PRTransmitfileFN)_PR_InvalidInt,
	SocketGetName,
	SocketGetPeerName,
	SocketGetSockOpt,
	SocketSetSockOpt,
	_PR_SocketGetSocketOption,
	_PR_SocketSetSocketOption
};


static PRIOMethods socketpollfdMethods = {
    (PRDescType) 0,
    (PRCloseFN)_PR_InvalidStatus,
    (PRReadFN)_PR_InvalidInt,
    (PRWriteFN)_PR_InvalidInt,
    (PRAvailableFN)_PR_InvalidInt,
    (PRAvailable64FN)_PR_InvalidInt64,
    (PRFsyncFN)_PR_InvalidStatus,
    (PRSeekFN)_PR_InvalidInt,
    (PRSeek64FN)_PR_InvalidInt64,
    (PRFileInfoFN)_PR_InvalidStatus,
    (PRFileInfo64FN)_PR_InvalidStatus,
    (PRWritevFN)_PR_InvalidInt,        
    (PRConnectFN)_PR_InvalidStatus,        
    (PRAcceptFN)_PR_InvalidDesc,        
    (PRBindFN)_PR_InvalidStatus,        
    (PRListenFN)_PR_InvalidStatus,        
    (PRShutdownFN)_PR_InvalidStatus,    
    (PRRecvFN)_PR_InvalidInt,        
    (PRSendFN)_PR_InvalidInt,        
    (PRRecvfromFN)_PR_InvalidInt,    
    (PRSendtoFN)_PR_InvalidInt,        
	SocketPoll,
    (PRAcceptreadFN)_PR_InvalidInt,   
    (PRTransmitfileFN)_PR_InvalidInt, 
    (PRGetsocknameFN)_PR_InvalidStatus,    
    (PRGetpeernameFN)_PR_InvalidStatus,    
    (PRGetsockoptFN)_PR_InvalidStatus,    
    (PRSetsockoptFN)_PR_InvalidStatus,    
    (PRGetsocketoptionFN)_PR_InvalidStatus,
    (PRSetsocketoptionFN)_PR_InvalidStatus
};

PR_IMPLEMENT(const PRIOMethods*) PR_GetTCPMethods()
{
	return &tcpMethods;
}

PR_IMPLEMENT(const PRIOMethods*) PR_GetUDPMethods()
{
	return &udpMethods;
}

static const PRIOMethods* PR_GetSocketPollFdMethods()
{
    return &socketpollfdMethods;
}  /* PR_GetSocketPollFdMethods */


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
#if defined(XP_UNIX)
			&& AF_UNIX != domain
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
#ifdef XP_OS2_VACPP
            (char *)&one, sizeof(one) ) < 0) {
#else
		    (const void *) &one, sizeof(one) ) < 0) {
#endif
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

	if (!_pr_initialized) _PR_ImplicitInitialization();

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
#elif defined(WINNT)
    /*
     * A socket pair is often used for interprocess communication,
     * so we need to make sure neither socket is associated with
     * the I/O completion port; otherwise it can't be used by a
     * child process.
     *
     * The default implementation below cannot be used for NT
     * because PR_Accept would have associated the I/O completion
     * port with the listening and accepted sockets.
     */
    SOCKET listenSock;
    SOCKET osfd[2];
    struct sockaddr_in selfAddr;
    int addrLen;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    osfd[0] = osfd[1] = INVALID_SOCKET;
    listenSock = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSock == INVALID_SOCKET) {
        goto failed;
    }
    selfAddr.sin_family = AF_INET;
    selfAddr.sin_port = 0;
    selfAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    addrLen = sizeof(selfAddr);
    if (bind(listenSock, (struct sockaddr *) &selfAddr,
            addrLen) == SOCKET_ERROR) {
        goto failed;
    }
    if (getsockname(listenSock, (struct sockaddr *) &selfAddr,
            &addrLen) == SOCKET_ERROR) {
        goto failed;
    }
    if (listen(listenSock, 5) == SOCKET_ERROR) {
        goto failed;
    }
    osfd[0] = socket(AF_INET, SOCK_STREAM, 0);
    if (osfd[0] == INVALID_SOCKET) {
        goto failed;
    }
    selfAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    /*
     * Only a thread is used to do the connect and accept.
     * I am relying on the fact that connect returns
     * successfully as soon as the connect request is put
     * into the listen queue (but before accept is called).
     * This is the behavior of the BSD socket code.  If
     * connect does not return until accept is called, we
     * will need to create another thread to call connect.
     */
    if (connect(osfd[0], (struct sockaddr *) &selfAddr,
            addrLen) == SOCKET_ERROR) {
        goto failed;
    }
    osfd[1] = accept(listenSock, NULL, NULL);
    if (osfd[1] == INVALID_SOCKET) {
        goto failed;
    }
    closesocket(listenSock);

    f[0] = PR_AllocFileDesc(osfd[0], PR_GetTCPMethods());
    if (!f[0]) {
        closesocket(osfd[0]);
        closesocket(osfd[1]);
        /* PR_AllocFileDesc() has invoked PR_SetError(). */
        return PR_FAILURE;
    }
    f[1] = PR_AllocFileDesc(osfd[1], PR_GetTCPMethods());
    if (!f[1]) {
        PR_Close(f[0]);
        closesocket(osfd[1]);
        /* PR_AllocFileDesc() has invoked PR_SetError(). */
        return PR_FAILURE;
    }
    return PR_SUCCESS;

failed:
    if (listenSock != INVALID_SOCKET) {
        closesocket(listenSock);
    }
    if (osfd[0] != INVALID_SOCKET) {
        closesocket(osfd[0]);
    }
    if (osfd[1] != INVALID_SOCKET) {
        closesocket(osfd[1]);
    }
    return PR_FAILURE;
#else /* not Unix or NT */
    /*
     * default implementation
     */
    PRFileDesc *listenSock;
    PRNetAddr selfAddr;
    PRUint16 port;

    f[0] = f[1] = NULL;
    listenSock = PR_NewTCPSocket();
    if (listenSock == NULL) {
        goto failed;
    }
    PR_InitializeNetAddr(PR_IpAddrAny, 0, &selfAddr);
    if (PR_Bind(listenSock, &selfAddr) == PR_FAILURE) {
        goto failed;
    }
    if (PR_GetSockName(listenSock, &selfAddr) == PR_FAILURE) {
        goto failed;
    }
    port = ntohs(selfAddr.inet.port);
    if (PR_Listen(listenSock, 5) == PR_FAILURE) {
        goto failed;
    }
    f[0] = PR_NewTCPSocket();
    if (f[0] == NULL) {
        goto failed;
    }
    PR_InitializeNetAddr(PR_IpAddrLoopback, port, &selfAddr);

    /*
     * Only a thread is used to do the connect and accept.
     * I am relying on the fact that PR_Connect returns
     * successfully as soon as the connect request is put
     * into the listen queue (but before PR_Accept is called).
     * This is the behavior of the BSD socket code.  If
     * connect does not return until accept is called, we
     * will need to create another thread to call connect.
     */
    if (PR_Connect(f[0], &selfAddr, PR_INTERVAL_NO_TIMEOUT)
            == PR_FAILURE) {
        goto failed;
    }
    f[1] = PR_Accept(listenSock, NULL, PR_INTERVAL_NO_TIMEOUT);
    if (f[1] == NULL) {
        goto failed;
    }
    PR_Close(listenSock);
    return PR_SUCCESS;

failed:
    if (listenSock) {
        PR_Close(listenSock);
    }
    if (f[0]) {
        PR_Close(f[0]);
    }
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

	buf = (char*)PR_MALLOC(_TRANSMITFILE_BUFSIZE);
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
 *	buf is a buffer of length = (amount + 2 * sizeof(PRNetAddr))
 *	*raddr points to the PRNetAddr of the accepted connection upon
 *	return
 *
 *	return number of bytes read or -1 on error
 *
 */
PRInt32 _PR_EmulateAcceptRead(
    PRFileDesc *sd, PRFileDesc **nd, PRNetAddr **raddr,
    void *buf, PRInt32 amount, PRIntervalTime timeout)
{
    PRInt32 rv = -1;
    PRNetAddr remote;
    PRFileDesc *accepted = NULL;

    /* The socket must be in blocking mode. */
    if (sd->secret->nonblocking)
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return rv;
    }

    /*
    ** The timeout does not apply to the accept portion of the
    ** operation - it waits indefinitely.
    */
    accepted = PR_Accept(sd, &remote, PR_INTERVAL_NO_TIMEOUT);
    if (NULL == accepted) return rv;

    rv = PR_Recv(accepted, buf, amount, 0, timeout);
    if (rv >= 0)
    {
        /* copy the new info out where caller can see it */
        PRPtrdiff aligned = (PRPtrdiff)buf + amount + sizeof(void*) - 1;
        *raddr = (PRNetAddr*)(aligned & ~(sizeof(void*) - 1));
        memcpy(*raddr, &remote, PR_NETADDR_SIZE(&remote));
        *nd = accepted;
        return rv;
    }

    PR_Close(accepted);
    return rv;
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
                poll = (PRPollDesc*)PR_Realloc(
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

    copy = poll = (PRPollDesc*)PR_Calloc(npds + PD_INCR, sizeof(PRPollDesc));
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
