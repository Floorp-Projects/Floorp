/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "primpl.h"

#include <string.h>

#if defined(_WIN64)
#ifndef SO_UPDATE_CONNECT_CONTEXT
#define SO_UPDATE_CONNECT_CONTEXT 0x7010
#endif
#endif

/************************************************************************/

/* These two functions are only used in assertions. */
#if defined(DEBUG)

PRBool IsValidNetAddr(const PRNetAddr *addr)
{
    if ((addr != NULL)
#if defined(XP_UNIX) || defined(XP_OS2)
	    && (addr->raw.family != PR_AF_LOCAL)
#endif
	    && (addr->raw.family != PR_AF_INET6)
	    && (addr->raw.family != PR_AF_INET)) {
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
#if defined(XP_UNIX) || defined(XP_OS2)
            && (addr->raw.family != AF_UNIX)
#endif
            && (PR_NETADDR_SIZE(addr) != addr_len)) {
#if defined(LINUX) && __GLIBC__ == 2 && __GLIBC_MINOR__ == 1
        /*
         * In glibc 2.1, struct sockaddr_in6 is 24 bytes.  In glibc 2.2
         * and in the 2.4 kernel, struct sockaddr_in6 has the scope_id
         * field and is 28 bytes.  It is possible for socket functions
         * to return an addr_len greater than sizeof(struct sockaddr_in6).
         * We need to allow that.  (Bugzilla bug #77264)
         */
        if ((PR_AF_INET6 == addr->raw.family)
                && (sizeof(addr->ipv6) == addr_len)) {
            return PR_TRUE;
        }
#endif
        /*
         * The accept(), getsockname(), etc. calls on some platforms
         * do not set the actual socket address length on return.
         * In this case, we verifiy addr_len is still the value we
         * passed in (i.e., sizeof(PRNetAddr)).
         */
#if defined(QNX)
        if (sizeof(PRNetAddr) == addr_len) {
            return PR_TRUE;
        }
#endif
        return PR_FALSE;
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

PR_IMPLEMENT(PRFileDesc *) PR_ImportTCPSocket(PROsfd osfd)
{
PRFileDesc *fd;

	if (!_pr_initialized) _PR_ImplicitInitialization();
	fd = PR_AllocFileDesc(osfd, PR_GetTCPMethods());
	if (fd != NULL) {
		_PR_MD_MAKE_NONBLOCK(fd);
		_PR_MD_INIT_FD_INHERITABLE(fd, PR_TRUE);
#ifdef _PR_NEED_SECRET_AF
		/* this means we can only import IPv4 sockets here.
		 * but this is what the function in ptio.c does.
		 * We need a way to import IPv6 sockets, too.
		 */
		fd->secret->af = AF_INET;
#endif
	} else
		_PR_MD_CLOSE_SOCKET(osfd);
	return(fd);
}

PR_IMPLEMENT(PRFileDesc *) PR_ImportUDPSocket(PROsfd osfd)
{
PRFileDesc *fd;

	if (!_pr_initialized) _PR_ImplicitInitialization();
	fd = PR_AllocFileDesc(osfd, PR_GetUDPMethods());
	if (fd != NULL) {
		_PR_MD_MAKE_NONBLOCK(fd);
		_PR_MD_INIT_FD_INHERITABLE(fd, PR_TRUE);
	} else
		_PR_MD_CLOSE_SOCKET(osfd);
	return(fd);
}


static const PRIOMethods* PR_GetSocketPollFdMethods(void);

PR_IMPLEMENT(PRFileDesc*) PR_CreateSocketPollFd(PROsfd osfd)
{
    PRFileDesc *fd;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    fd = _PR_Getfd();

    if (fd == NULL) PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    else
    {
        fd->secret->md.osfd = osfd;
        fd->secret->inheritable = _PR_TRI_FALSE;
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
    const PRNetAddr *addrp = addr;
#if defined(_PR_INET6)
	PRNetAddr addrCopy;
#endif
	PRThread *me = _PR_MD_CURRENT_THREAD();

	if (_PR_PENDING_INTERRUPT(me)) {
		me->flags &= ~_PR_INTERRUPT;
		PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
		return PR_FAILURE;
	}
#if defined(_PR_INET6)
	if (addr->raw.family == PR_AF_INET6) {
		addrCopy = *addr;
		addrCopy.raw.family = AF_INET6;
		addrp = &addrCopy;
	}
#endif

	rv = _PR_MD_CONNECT(fd, addrp, PR_NETADDR_SIZE(addr), timeout);
	PR_LOG(_pr_io_lm, PR_LOG_MAX, ("connect -> %d", rv));
	if (rv == 0)
		return PR_SUCCESS;
	else
		return PR_FAILURE;
}

static PRStatus PR_CALLBACK SocketConnectContinue(
    PRFileDesc *fd, PRInt16 out_flags)
{
    PROsfd osfd;
    int err;

    if (out_flags & PR_POLL_NVAL) {
        PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
        return PR_FAILURE;
    }
    if ((out_flags & (PR_POLL_WRITE | PR_POLL_EXCEPT | PR_POLL_ERR)) == 0) {
        PR_ASSERT(out_flags == 0);
        PR_SetError(PR_IN_PROGRESS_ERROR, 0);
        return PR_FAILURE;
    }

    osfd = fd->secret->md.osfd;

#if defined(XP_UNIX)

    err = _MD_unix_get_nonblocking_connect_error(osfd);
    if (err != 0) {
        _PR_MD_MAP_CONNECT_ERROR(err);
        return PR_FAILURE;
    }
    return PR_SUCCESS;

#elif defined(WIN32) || defined(WIN16)

    if (out_flags & PR_POLL_EXCEPT) {
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

    PR_ASSERT(out_flags & PR_POLL_WRITE);

#if defined(_WIN64)
    if (fd->secret->alreadyConnected) {
        fd->secret->alreadyConnected = PR_FALSE;
    }
    /* TCP Fast Open on Windows must use ConnectEx, which uses overlapped
     * input/output.
     * To get result we need to use GetOverlappedResult. */
    if (fd->secret->overlappedActive) {
        PR_ASSERT(fd->secret->nonblocking);
        PRInt32 rvSent;
        if (GetOverlappedResult(osfd, &fd->secret->ol, &rvSent, FALSE) == TRUE) {
            fd->secret->overlappedActive = FALSE;
            PR_LOG(_pr_io_lm, PR_LOG_MIN,
               ("SocketConnectContinue GetOverlappedResult succeeded\n"));
            /* When ConnectEx is used, all previously set socket options and
             * property are not enabled and to enable them
             * SO_UPDATE_CONNECT_CONTEXT option need to be set. */
            if (setsockopt((SOCKET)osfd, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0) != 0) {
                err = WSAGetLastError();
                PR_LOG(_pr_io_lm, PR_LOG_MIN,
                       ("SocketConnectContinue setting SO_UPDATE_CONNECT_CONTEXT failed %d\n", err));
                _PR_MD_MAP_SETSOCKOPT_ERROR(err);
                return PR_FAILURE;
            }
            return PR_SUCCESS;
        } else {
            err = WSAGetLastError();
            PR_LOG(_pr_io_lm, PR_LOG_MIN,
               ("SocketConnectContinue GetOverlappedResult failed %d\n", err));
            if (err != ERROR_IO_PENDING) {
                _PR_MD_MAP_CONNECT_ERROR(err);
                fd->secret->overlappedActive = FALSE;
                return PR_FAILURE;
            } else {
                PR_SetError(PR_IN_PROGRESS_ERROR, 0);
                return PR_FAILURE;
            }
        }
    }
#endif

    return PR_SUCCESS;

#elif defined(XP_OS2)

    err = _MD_os2_get_nonblocking_connect_error(osfd);
    if (err != 0) {
        _PR_MD_MAP_CONNECT_ERROR(err);
        return PR_FAILURE;
    }
    return PR_SUCCESS;

#elif defined(XP_BEOS)

#ifdef BONE_VERSION  /* bug 122364 */
    /* temporary workaround until getsockopt(SO_ERROR) works in BONE */
    if (out_flags & PR_POLL_EXCEPT) {
        PR_SetError(PR_CONNECT_REFUSED_ERROR, 0);
        return PR_FAILURE;
    }
    PR_ASSERT(out_flags & PR_POLL_WRITE);
    return PR_SUCCESS;
#else
    err = _MD_beos_get_nonblocking_connect_error(fd);
    if( err != 0 ) {
        _PR_MD_MAP_CONNECT_ERROR(err);
        return PR_FAILURE;
    }
    else
        return PR_SUCCESS;
#endif /* BONE_VERSION */

#else
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
#endif
}

PR_IMPLEMENT(PRStatus) PR_GetConnectStatus(const PRPollDesc *pd)
{
    /* Find the NSPR layer and invoke its connectcontinue method */
    PRFileDesc *bottom = PR_GetIdentitiesLayer(pd->fd, PR_NSPR_IO_LAYER);

    if (NULL == bottom) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }
    return SocketConnectContinue(bottom, pd->out_flags);
}

static PRFileDesc* PR_CALLBACK SocketAccept(PRFileDesc *fd, PRNetAddr *addr,
PRIntervalTime timeout)
{
	PROsfd osfd;
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
	if (!fd2->secret->nonblocking && fd2->secret->inheritable != _PR_TRI_TRUE) {
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

#ifdef _PR_INET6
	if (addr && (AF_INET6 == addr->raw.family))
        addr->raw.family = PR_AF_INET6;
#endif
	PR_ASSERT(IsValidNetAddr(addr) == PR_TRUE);
	PR_ASSERT(IsValidNetAddrLen(addr, al) == PR_TRUE);

	return fd2;
}

#ifdef WINNT
PR_IMPLEMENT(PRFileDesc*) PR_NTFast_Accept(PRFileDesc *fd, PRNetAddr *addr,
PRIntervalTime timeout)
{
	PROsfd osfd;
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
#ifdef _PR_INET6
		if (AF_INET6 == addr->raw.family)
        	addr->raw.family = PR_AF_INET6;
#endif
#ifdef _PR_NEED_SECRET_AF
		fd2->secret->af = fd->secret->af;
#endif
	}
	return fd2;
}
#endif /* WINNT */


static PRStatus PR_CALLBACK SocketBind(PRFileDesc *fd, const PRNetAddr *addr)
{
	PRInt32 result;
    const PRNetAddr *addrp = addr;
#if defined(_PR_INET6)
	PRNetAddr addrCopy;
#endif

	PR_ASSERT(IsValidNetAddr(addr) == PR_TRUE);

#ifdef XP_UNIX
	if (addr->raw.family == AF_UNIX) {
		/* Disallow relative pathnames */
		if (addr->local.path[0] != '/') {
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
			return PR_FAILURE;
		}
	}
#endif /* XP_UNIX */

#if defined(_PR_INET6)
	if (addr->raw.family == PR_AF_INET6) {
		addrCopy = *addr;
		addrCopy.raw.family = AF_INET6;
		addrp = &addrCopy;
	}
#endif
	result = _PR_MD_BIND(fd, addrp, PR_NETADDR_SIZE(addr));
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

	if ((flags != 0) && (flags != PR_MSG_PEEK)) {
		PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
		return -1;
	}
	if (_PR_PENDING_INTERRUPT(me)) {
		me->flags &= ~_PR_INTERRUPT;
		PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
		return -1;
	}
	if (_PR_IO_PENDING(me)) {
		PR_SetError(PR_IO_PENDING_ERROR, 0);
		return -1;
	}

	PR_LOG(_pr_io_lm, PR_LOG_MAX,
		("recv: fd=%p osfd=%" PR_PRIdOSFD " buf=%p amount=%d flags=%d",
		fd, fd->secret->md.osfd, buf, amount, flags));

#ifdef _PR_HAVE_PEEK_BUFFER
	if (fd->secret->peekBytes != 0) {
		rv = (amount < fd->secret->peekBytes) ?
			amount : fd->secret->peekBytes;
		memcpy(buf, fd->secret->peekBuffer, rv);
		if (flags == 0) {
			/* consume the bytes in the peek buffer */
			fd->secret->peekBytes -= rv;
			if (fd->secret->peekBytes != 0) {
				memmove(fd->secret->peekBuffer,
					fd->secret->peekBuffer + rv,
					fd->secret->peekBytes);
			}
		}
		return rv;
	}

	/* allocate peek buffer, if necessary */
	if ((PR_MSG_PEEK == flags) && _PR_FD_NEED_EMULATE_MSG_PEEK(fd)) {
		PR_ASSERT(0 == fd->secret->peekBytes);
		/* impose a max size on the peek buffer */
		if (amount > _PR_PEEK_BUFFER_MAX) {
			amount = _PR_PEEK_BUFFER_MAX;
		}
		if (fd->secret->peekBufSize < amount) {
			if (fd->secret->peekBuffer) {
				PR_Free(fd->secret->peekBuffer);
			}
			fd->secret->peekBufSize = amount;
			fd->secret->peekBuffer = PR_Malloc(amount);
			if (NULL == fd->secret->peekBuffer) {
				fd->secret->peekBufSize = 0;
				PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
				return -1;
			}
		}
	}
#endif

	rv = _PR_MD_RECV(fd, buf, amount, flags, timeout);
	PR_LOG(_pr_io_lm, PR_LOG_MAX, ("recv -> %d, error = %d, os error = %d",
		rv, PR_GetError(), PR_GetOSError()));

#ifdef _PR_HAVE_PEEK_BUFFER
	if ((PR_MSG_PEEK == flags) && _PR_FD_NEED_EMULATE_MSG_PEEK(fd)) {
		if (rv > 0) {
			memcpy(fd->secret->peekBuffer, buf, rv);
			fd->secret->peekBytes = rv;
		}
	}
#endif

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
		    ("send: fd=%p osfd=%" PR_PRIdOSFD " buf=%p amount=%d",
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

#ifdef _PR_HAVE_PEEK_BUFFER
	if (fd->secret->peekBuffer) {
		PR_ASSERT(fd->secret->peekBufSize > 0);
		PR_DELETE(fd->secret->peekBuffer);
		fd->secret->peekBufSize = 0;
		fd->secret->peekBytes = 0;
	}
#endif

	PR_FreeFileDesc(fd);
	return PR_SUCCESS;
}

static PRInt32 PR_CALLBACK SocketAvailable(PRFileDesc *fd)
{
	PRInt32 rv;
#ifdef _PR_HAVE_PEEK_BUFFER
	if (fd->secret->peekBytes != 0) {
		return fd->secret->peekBytes;
	}
#endif
	rv =  _PR_MD_SOCKETAVAILABLE(fd);
	return rv;		
}

static PRInt64 PR_CALLBACK SocketAvailable64(PRFileDesc *fd)
{
    PRInt64 rv;
#ifdef _PR_HAVE_PEEK_BUFFER
    if (fd->secret->peekBytes != 0) {
        LL_I2L(rv, fd->secret->peekBytes);
        return rv;
    }
#endif
    LL_I2L(rv, _PR_MD_SOCKETAVAILABLE(fd));
	return rv;		
}

static PRStatus PR_CALLBACK SocketSync(PRFileDesc *fd)
{
	return PR_SUCCESS;
}

static PRInt32 PR_CALLBACK SocketSendTo(
    PRFileDesc *fd, const void *buf, PRInt32 amount,
    PRIntn flags, const PRNetAddr *addr, PRIntervalTime timeout)
{
	PRInt32 temp, count;
    const PRNetAddr *addrp = addr;
#if defined(_PR_INET6)
	PRNetAddr addrCopy;
#endif
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
#if defined(_PR_INET6)
	if (addr->raw.family == PR_AF_INET6) {
		addrCopy = *addr;
		addrCopy.raw.family = AF_INET6;
		addrp = &addrCopy;
	}
#endif

	count = 0;
	do {
		temp = _PR_MD_SENDTO(fd, buf, amount, flags,
		    addrp, PR_NETADDR_SIZE(addr), timeout);
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
	} while (amount > 0);
	return count;
}

#if defined(_WIN64) && defined(WIN95)
static PRInt32 PR_CALLBACK SocketTCPSendTo(
    PRFileDesc *fd, const void *buf, PRInt32 amount,
    PRIntn flags, const PRNetAddr *addr, PRIntervalTime timeout)
{
    PRInt32 temp, count;
    const PRNetAddr *addrp = addr;
#if defined(_PR_INET6)
    PRNetAddr addrCopy;
#endif
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
#if defined(_PR_INET6)
    if (addr->raw.family == PR_AF_INET6) {
        addrCopy = *addr;
        addrCopy.raw.family = AF_INET6;
        addrp = &addrCopy;
    }
#endif

    count = 0;
    while (amount > 0) {
        temp = _PR_MD_TCPSENDTO(fd, buf, amount, flags,
                                addrp, PR_NETADDR_SIZE(addr), timeout);
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
#endif

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
#ifdef _PR_INET6
	if (addr && (AF_INET6 == addr->raw.family))
        addr->raw.family = PR_AF_INET6;
#endif
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
	PROsfd newSock;
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
#ifdef _PR_INET6
			if (AF_INET6 == *raddr->raw.family)
        		*raddr->raw.family = PR_AF_INET6;
#endif
		}
	}
	}
#else
	rv = PR_EmulateAcceptRead(sd, nd, raddr, buf, amount, timeout);
#endif
	return rv;
}

#ifdef WINNT
PR_IMPLEMENT(PRInt32) PR_NTFast_AcceptRead(PRFileDesc *sd, PRFileDesc **nd, 
PRNetAddr **raddr, void *buf, PRInt32 amount,
PRIntervalTime timeout)
{
	PRInt32 rv;
	PROsfd newSock;
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
#ifdef _PR_INET6
			if (AF_INET6 == *raddr->raw.family)
        		*raddr->raw.family = PR_AF_INET6;
#endif
#ifdef _PR_NEED_SECRET_AF
			(*nd)->secret->af = sd->secret->af;
#endif
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
	PROsfd newSock;
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
#ifdef _PR_INET6
			if (AF_INET6 == *raddr->raw.family)
        		*raddr->raw.family = PR_AF_INET6;
#endif
#ifdef _PR_NEED_SECRET_AF
			(*nd)->secret->af = sd->secret->af;
#endif
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

static PRInt32 PR_CALLBACK SocketSendFile(
    PRFileDesc *sd, PRSendFileData *sfd,
    PRTransmitFileFlags flags, PRIntervalTime timeout)
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
	rv = _PR_MD_SENDFILE(sd, sfd, flags, timeout);
	if ((rv >= 0) && (flags == PR_TRANSMITFILE_CLOSE_SOCKET)) {
		/*
		 * This should be kept the same as SocketClose, except
		 * that _PR_MD_CLOSE_SOCKET(sd->secret->md.osfd) should
		 * not be called because the socket will be recycled.
		 */
		PR_FreeFileDesc(sd);
	}
#else
	rv = PR_EmulateSendFile(sd, sfd, flags, timeout);
#endif	/* WINNT */

	return rv;
}

static PRInt32 PR_CALLBACK SocketTransmitFile(PRFileDesc *sd, PRFileDesc *fd, 
const void *headers, PRInt32 hlen, PRTransmitFileFlags flags,
PRIntervalTime timeout)
{
	PRSendFileData sfd;

	sfd.fd = fd;
	sfd.file_offset = 0;
	sfd.file_nbytes = 0;
	sfd.header = headers;
	sfd.hlen = hlen;
	sfd.trailer = NULL;
	sfd.tlen = 0;

	return(SocketSendFile(sd, &sfd, flags, timeout));
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
#ifdef _PR_INET6
	if (AF_INET6 == addr->raw.family)
        addr->raw.family = PR_AF_INET6;
#endif
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
#ifdef _PR_INET6
	if (AF_INET6 == addr->raw.family)
        addr->raw.family = PR_AF_INET6;
#endif
	PR_ASSERT(IsValidNetAddr(addr) == PR_TRUE);
	PR_ASSERT(IsValidNetAddrLen(addr, addrlen) == PR_TRUE);
	return PR_SUCCESS;
}

static PRInt16 PR_CALLBACK SocketPoll(
    PRFileDesc *fd, PRInt16 in_flags, PRInt16 *out_flags)
{
    *out_flags = 0;

#if defined(_WIN64)
    if (in_flags & PR_POLL_WRITE) {
        if (fd->secret->alreadyConnected) {
            out_flags = PR_POLL_WRITE;
            return PR_POLL_WRITE;
        }
    }
#endif
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
#if defined(_WIN64) && defined(WIN95)
	SocketTCPSendTo, /* This is for fast open. We imitate Linux interface. */
#else
	(PRSendtoFN)_PR_InvalidInt,
#endif
	SocketPoll,
	SocketAcceptRead,
	SocketTransmitFile,
	SocketGetName,
	SocketGetPeerName,
	(PRReservedFN)_PR_InvalidInt,
	(PRReservedFN)_PR_InvalidInt,
	_PR_SocketGetSocketOption,
	_PR_SocketSetSocketOption,
    SocketSendFile, 
    SocketConnectContinue,
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt
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
	(PRReservedFN)_PR_InvalidInt,
	(PRReservedFN)_PR_InvalidInt,
	_PR_SocketGetSocketOption,
	_PR_SocketSetSocketOption,
    (PRSendfileFN)_PR_InvalidInt, 
    (PRConnectcontinueFN)_PR_InvalidStatus, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt
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
    (PRReservedFN)_PR_InvalidInt,    
    (PRReservedFN)_PR_InvalidInt,    
    (PRGetsocketoptionFN)_PR_InvalidStatus,
    (PRSetsocketoptionFN)_PR_InvalidStatus,
    (PRSendfileFN)_PR_InvalidInt, 
    (PRConnectcontinueFN)_PR_InvalidStatus, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt
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

#if !defined(_PR_INET6) || defined(_PR_INET6_PROBE)
PR_EXTERN(PRStatus) _pr_push_ipv6toipv4_layer(PRFileDesc *fd);

#if defined(_PR_INET6_PROBE)

extern PRBool _pr_ipv6_is_present(void);

PR_IMPLEMENT(PRBool) _pr_test_ipv6_socket()
{
	PROsfd osfd;

	osfd = _PR_MD_SOCKET(AF_INET6, SOCK_STREAM, 0);
	if (osfd != -1) {
		_PR_MD_CLOSE_SOCKET(osfd);
		return PR_TRUE;
	}
	return PR_FALSE;
}
#endif	/* _PR_INET6_PROBE */

#endif

PR_IMPLEMENT(PRFileDesc*) PR_Socket(PRInt32 domain, PRInt32 type, PRInt32 proto)
{
	PROsfd osfd;
	PRFileDesc *fd;
	PRInt32 tmp_domain = domain;

	if (!_pr_initialized) _PR_ImplicitInitialization();
	if (PR_AF_INET != domain
			&& PR_AF_INET6 != domain
#if defined(XP_UNIX) || defined(XP_OS2)
			&& PR_AF_LOCAL != domain
#endif
			) {
		PR_SetError(PR_ADDRESS_NOT_SUPPORTED_ERROR, 0);
		return NULL;
	}

#if defined(_PR_INET6_PROBE)
	if (PR_AF_INET6 == domain)
		domain = _pr_ipv6_is_present() ? AF_INET6 : AF_INET;
#elif defined(_PR_INET6)
	if (PR_AF_INET6 == domain)
		domain = AF_INET6;
#else
	if (PR_AF_INET6 == domain)
		domain = AF_INET;
#endif	/* _PR_INET6 */
	osfd = _PR_MD_SOCKET(domain, type, proto);
	if (osfd == -1) {
		return 0;
	}
	if (type == SOCK_STREAM)
		fd = PR_AllocFileDesc(osfd, PR_GetTCPMethods());
	else
		fd = PR_AllocFileDesc(osfd, PR_GetUDPMethods());
	/*
	 * Make the sockets non-blocking
	 */
	if (fd != NULL) {
		_PR_MD_MAKE_NONBLOCK(fd);
		_PR_MD_INIT_FD_INHERITABLE(fd, PR_FALSE);
#ifdef _PR_NEED_SECRET_AF
		fd->secret->af = domain;
#endif
#if defined(_PR_INET6_PROBE) || !defined(_PR_INET6)
		/*
		 * For platforms with no support for IPv6 
		 * create layered socket for IPv4-mapped IPv6 addresses
		 */
		if (PR_AF_INET6 == tmp_domain && PR_AF_INET == domain) {
			if (PR_FAILURE == _pr_push_ipv6toipv4_layer(fd)) {
				PR_Close(fd);
				fd = NULL;
			}
		}
#endif
	} else
		_PR_MD_CLOSE_SOCKET(osfd);

	return fd;
}

PR_IMPLEMENT(PRFileDesc *) PR_NewTCPSocket(void)
{
	PRInt32 domain = AF_INET;

	return PR_Socket(domain, SOCK_STREAM, 0);
}

PR_IMPLEMENT(PRFileDesc*) PR_NewUDPSocket(void)
{
	PRInt32 domain = AF_INET;

	return PR_Socket(domain, SOCK_DGRAM, 0);
}

PR_IMPLEMENT(PRFileDesc *) PR_OpenTCPSocket(PRIntn af)
{
	return PR_Socket(af, SOCK_STREAM, 0);
}

PR_IMPLEMENT(PRFileDesc*) PR_OpenUDPSocket(PRIntn af)
{
	return PR_Socket(af, SOCK_DGRAM, 0);
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
	_PR_MD_INIT_FD_INHERITABLE(f[0], PR_FALSE);
	_PR_MD_MAKE_NONBLOCK(f[1]);
	_PR_MD_INIT_FD_INHERITABLE(f[1], PR_FALSE);
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
    struct sockaddr_in selfAddr, peerAddr;
    int addrLen;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    osfd[0] = osfd[1] = INVALID_SOCKET;
    listenSock = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSock == INVALID_SOCKET) {
        goto failed;
    }
    selfAddr.sin_family = AF_INET;
    selfAddr.sin_port = 0;
    selfAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* BugZilla: 35408 */
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
    /*
     * A malicious local process may connect to the listening
     * socket, so we need to verify that the accepted connection
     * is made from our own socket osfd[0].
     */
    if (getsockname(osfd[0], (struct sockaddr *) &selfAddr,
            &addrLen) == SOCKET_ERROR) {
        goto failed;
    }
    osfd[1] = accept(listenSock, (struct sockaddr *) &peerAddr, &addrLen);
    if (osfd[1] == INVALID_SOCKET) {
        goto failed;
    }
    if (peerAddr.sin_port != selfAddr.sin_port) {
        /* the connection we accepted is not from osfd[0] */
        PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, 0);
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
    _PR_MD_INIT_FD_INHERITABLE(f[0], PR_FALSE);
    _PR_MD_INIT_FD_INHERITABLE(f[1], PR_FALSE);
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
    PRNetAddr selfAddr, peerAddr;
    PRUint16 port;

    f[0] = f[1] = NULL;
    listenSock = PR_NewTCPSocket();
    if (listenSock == NULL) {
        goto failed;
    }
    PR_InitializeNetAddr(PR_IpAddrLoopback, 0, &selfAddr); /* BugZilla: 35408 */
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
#ifdef _PR_CONNECT_DOES_NOT_BIND
    /*
     * If connect does not implicitly bind the socket (e.g., on
     * BeOS), we have to bind the socket so that we can get its
     * port with getsockname later.
     */
    PR_InitializeNetAddr(PR_IpAddrLoopback, 0, &selfAddr);
    if (PR_Bind(f[0], &selfAddr) == PR_FAILURE) {
        goto failed;
    }
#endif
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
    /*
     * A malicious local process may connect to the listening
     * socket, so we need to verify that the accepted connection
     * is made from our own socket f[0].
     */
    if (PR_GetSockName(f[0], &selfAddr) == PR_FAILURE) {
        goto failed;
    }
    f[1] = PR_Accept(listenSock, &peerAddr, PR_INTERVAL_NO_TIMEOUT);
    if (f[1] == NULL) {
        goto failed;
    }
    if (peerAddr.inet.port != selfAddr.inet.port) {
        /* the connection we accepted is not from f[0] */
        PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, 0);
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
    if (f[1]) {
        PR_Close(f[1]);
    }
    return PR_FAILURE;
#endif
}

PR_IMPLEMENT(PROsfd)
PR_FileDesc2NativeHandle(PRFileDesc *fd)
{
    if (fd) {
        fd = PR_GetIdentitiesLayer(fd, PR_NSPR_IO_LAYER);
    }
    if (!fd) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return -1;
    }
    return fd->secret->md.osfd;
}

PR_IMPLEMENT(void)
PR_ChangeFileDescNativeHandle(PRFileDesc *fd, PROsfd handle)
{
	if (fd)
		fd->secret->md.osfd = handle;
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

PR_IMPLEMENT(void) PR_FD_NSET(PROsfd fd, PR_fd_set *set)
{
	PR_ASSERT( set->nsize < PR_MAX_SELECT_DESC );

	set->narray[set->nsize++] = fd;
}

PR_IMPLEMENT(void) PR_FD_NCLR(PROsfd fd, PR_fd_set *set)
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

PR_IMPLEMENT(PRInt32) PR_FD_NISSET(PROsfd fd, PR_fd_set *set)
{
	PRUint32 index;
	for (index = 0; index<set->nsize; index++)
		if (set->narray[index] == fd) {
			return 1;
		}
	return 0;
}


#if !defined(NEED_SELECT)
#include "obsolete/probslet.h"

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
