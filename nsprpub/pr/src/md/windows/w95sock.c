/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Win95 Sockets module
 *
 */

#if defined(_WIN64)
#include <winsock2.h>
#endif
#include "primpl.h"

#define READ_FD     1
#define WRITE_FD    2
#define CONNECT_FD  3

static PRInt32 socket_io_wait(
    PROsfd osfd,
    PRInt32 fd_type,
    PRIntervalTime timeout);


/* --- SOCKET IO --------------------------------------------------------- */

static PRBool socketFixInet6RcvBuf = PR_FALSE;

void _PR_MD_InitSockets(void)
{
    OSVERSIONINFO osvi;

    memset(&osvi, 0, sizeof(osvi));
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    GetVersionEx(&osvi);

    if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1)
    {
        /* if Windows XP (32-bit) */
        socketFixInet6RcvBuf = PR_TRUE;
    }
}

void _PR_MD_CleanupSockets(void)
{
    socketFixInet6RcvBuf = PR_FALSE;
}

PROsfd
_PR_MD_SOCKET(int af, int type, int flags)
{
    SOCKET sock;
    u_long one = 1;

    sock = socket(af, type, flags);

    if (sock == INVALID_SOCKET )
    {
        _PR_MD_MAP_SOCKET_ERROR(WSAGetLastError());
        return (PROsfd)sock;
    }

    /*
    ** Make the socket Non-Blocking
    */
    if (ioctlsocket( sock, FIONBIO, &one) != 0)
    {
        PR_SetError(PR_UNKNOWN_ERROR, WSAGetLastError());
        closesocket(sock);
        return -1;
    }

    if (af == AF_INET6 && socketFixInet6RcvBuf)
    {
        int bufsize;
        int len = sizeof(bufsize);
        int rv;

        /* Windows XP 32-bit returns an error on getpeername() for AF_INET6
         * sockets if the receive buffer size is greater than 65535 before
         * the connection is initiated. The default receive buffer size may
         * be 128000 so fix it here to always be <= 65535. See bug 513659
         * and IBM DB2 support technote "Receive/Send IPv6 Socket Size
         * Problem in Windows XP SP2 & SP3".
         */
        rv = getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&bufsize, &len);
        if (rv == 0 && bufsize > 65535)
        {
            bufsize = 65535;
            setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&bufsize, len);
        }
    }

    return (PROsfd)sock;
}

/*
** _MD_CloseSocket() -- Close a socket
**
*/
PRInt32
_MD_CloseSocket(PROsfd osfd)
{
    PRInt32 rv;

    rv = closesocket((SOCKET) osfd );
    if (rv < 0) {
        _PR_MD_MAP_CLOSE_ERROR(WSAGetLastError());
    }

    return rv;
}

PRInt32
_MD_SocketAvailable(PRFileDesc *fd)
{
    PRInt32 result;

    if (ioctlsocket(fd->secret->md.osfd, FIONREAD, &result) < 0) {
        PR_SetError(PR_BAD_DESCRIPTOR_ERROR, WSAGetLastError());
        return -1;
    }
    return result;
}

PROsfd _MD_Accept(
    PRFileDesc *fd,
    PRNetAddr *raddr,
    PRUint32 *rlen,
    PRIntervalTime timeout )
{
    PROsfd osfd = fd->secret->md.osfd;
    SOCKET sock;
    PRInt32 rv, err;

    while ((sock = accept(osfd, (struct sockaddr *) raddr, rlen)) == -1)
    {
        err = WSAGetLastError();
        if ((err == WSAEWOULDBLOCK) && (!fd->secret->nonblocking))
        {
            if ((rv = socket_io_wait(osfd, READ_FD, timeout)) < 0)
            {
                break;
            }
        }
        else
        {
            _PR_MD_MAP_ACCEPT_ERROR(err);
            break;
        }
    }
    return(sock);
} /* end _MD_accept() */

PRInt32
_PR_MD_CONNECT(PRFileDesc *fd, const PRNetAddr *addr, PRUint32 addrlen,
               PRIntervalTime timeout)
{
    PROsfd osfd = fd->secret->md.osfd;
    PRInt32 rv;
    int     err;

    if ((rv = connect(osfd, (struct sockaddr *) addr, addrlen)) == -1)
    {
        err = WSAGetLastError();
        if ((!fd->secret->nonblocking) && (err == WSAEWOULDBLOCK))
        {
            rv = socket_io_wait(osfd, CONNECT_FD, timeout);
            if ( rv < 0 )
            {
                return(-1);
            }
            else
            {
                PR_ASSERT(rv > 0);
                /* it's connected */
                return(0);
            }
        }
        _PR_MD_MAP_CONNECT_ERROR(err);
    }
    return rv;
}

PRInt32
_PR_MD_BIND(PRFileDesc *fd, const PRNetAddr *addr, PRUint32 addrlen)
{
    PRInt32 rv;

    rv = bind(fd->secret->md.osfd, (const struct sockaddr *)&(addr->inet), addrlen);

    if (rv == SOCKET_ERROR)  {
        _PR_MD_MAP_BIND_ERROR(WSAGetLastError());
        return -1;
    }

    return 0;
}

PRInt32
_PR_MD_LISTEN(PRFileDesc *fd, PRIntn backlog)
{
    PRInt32 rv;

    rv = listen(fd->secret->md.osfd, backlog);

    if (rv == SOCKET_ERROR)  {
        _PR_MD_MAP_DEFAULT_ERROR(WSAGetLastError());
        return -1;
    }

    return 0;
}

PRInt32
_PR_MD_RECV(PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags,
            PRIntervalTime timeout)
{
    PROsfd osfd = fd->secret->md.osfd;
    PRInt32 rv, err;
    int osflags;

    if (0 == flags) {
        osflags = 0;
    } else {
        PR_ASSERT(PR_MSG_PEEK == flags);
        osflags = MSG_PEEK;
    }
    while ((rv = recv( osfd, buf, amount, osflags)) == -1)
    {
        if (((err = WSAGetLastError()) == WSAEWOULDBLOCK)
            && (!fd->secret->nonblocking))
        {
            rv = socket_io_wait(osfd, READ_FD, timeout);
            if ( rv < 0 )
            {
                return -1;
            }
        }
        else
        {
            _PR_MD_MAP_RECV_ERROR(err);
            break;
        }
    } /* end while() */
    return(rv);
}

PRInt32
_PR_MD_SEND(PRFileDesc *fd, const void *buf, PRInt32 amount, PRIntn flags,
            PRIntervalTime timeout)
{
    PROsfd osfd = fd->secret->md.osfd;
    PRInt32 rv, err;
    PRInt32 bytesSent = 0;

    while(bytesSent < amount )
    {
        while ((rv = send( osfd, buf, amount, 0 )) == -1)
        {
            if (((err = WSAGetLastError()) == WSAEWOULDBLOCK)
                && (!fd->secret->nonblocking))
            {
                rv = socket_io_wait(osfd, WRITE_FD, timeout);
                if ( rv < 0 )
                {
                    return -1;
                }
            }
            else
            {
                _PR_MD_MAP_SEND_ERROR(err);
                return -1;
            }
        }
        bytesSent += rv;
        if (fd->secret->nonblocking)
        {
            break;
        }
        if (bytesSent < amount)
        {
            rv = socket_io_wait(osfd, WRITE_FD, timeout);
            if ( rv < 0 )
            {
                return -1;
            }
        }
    }
    return bytesSent;
}

PRInt32
_PR_MD_SENDTO(PRFileDesc *fd, const void *buf, PRInt32 amount, PRIntn flags,
              const PRNetAddr *addr, PRUint32 addrlen, PRIntervalTime timeout)
{
    PROsfd osfd = fd->secret->md.osfd;
    PRInt32 rv, err;
    PRInt32 bytesSent = 0;

    do {
        while ((rv = sendto( osfd, buf, amount, 0, (struct sockaddr *) addr,
                             addrlen)) == -1)
        {
            if (((err = WSAGetLastError()) == WSAEWOULDBLOCK)
                && (!fd->secret->nonblocking))
            {
                rv = socket_io_wait(osfd, WRITE_FD, timeout);
                if ( rv < 0 )
                {
                    return -1;
                }
            }
            else
            {
                _PR_MD_MAP_SENDTO_ERROR(err);
                return -1;
            }
        }
        bytesSent += rv;
        if (fd->secret->nonblocking)
        {
            break;
        }
        if (bytesSent < amount)
        {
            rv = socket_io_wait(osfd, WRITE_FD, timeout);
            if (rv < 0)
            {
                return -1;
            }
        }
    } while(bytesSent < amount);
    return bytesSent;
}

#if defined(_WIN64)

static PRCallOnceType _pr_has_connectex_once;
typedef BOOL (PASCAL FAR * _pr_win_connectex_ptr)(_In_ SOCKET s, _In_reads_bytes_(namelen) const struct sockaddr FAR *name, _In_ int namelen, _In_reads_bytes_opt_(dwSendDataLength) PVOID lpSendBuffer, _In_ DWORD dwSendDataLength, _Out_ LPDWORD lpdwBytesSent, _Inout_ LPOVERLAPPED lpOverlapped);

#ifndef WSAID_CONNECTEX
#define WSAID_CONNECTEX \
  {0x25a207b9,0xddf3,0x4660,{0x8e,0xe9,0x76,0xe5,0x8c,0x74,0x06,0x3e}}
#endif
#ifndef SIO_GET_EXTENSION_FUNCTION_POINTER
#define SIO_GET_EXTENSION_FUNCTION_POINTER 0xC8000006
#endif
#ifndef TCP_FASTOPEN
#define TCP_FASTOPEN 15
#endif

#ifndef SO_UPDATE_CONNECT_CONTEXT
#define SO_UPDATE_CONNECT_CONTEXT 0x7010
#endif

static _pr_win_connectex_ptr _pr_win_connectex = NULL;

static PRStatus PR_CALLBACK _pr_set_connectex(void)
{
    _pr_win_connectex = NULL;
    SOCKET sock;
    PRInt32 dwBytes;
    int rc;

    /* Dummy socket needed for WSAIoctl */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        return PR_SUCCESS;
    }

    GUID guid = WSAID_CONNECTEX;
    rc = WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
                  &guid, sizeof(guid),
                  &_pr_win_connectex, sizeof(_pr_win_connectex),
                  &dwBytes, NULL, NULL);
    if (rc != 0) {
        _pr_win_connectex = NULL;
        return PR_SUCCESS;
    }

    rc = closesocket(sock);
    return PR_SUCCESS;
}

PRInt32
_PR_MD_TCPSENDTO(PRFileDesc *fd, const void *buf, PRInt32 amount, PRIntn flags,
                 const PRNetAddr *addr, PRUint32 addrlen, PRIntervalTime timeout)
{
    if (!_fd_waiting_for_overlapped_done_lock) {
        PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
        return PR_FAILURE;
    }

    if (PR_CallOnce(&_pr_has_connectex_once, _pr_set_connectex) != PR_SUCCESS) {
        PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
        return PR_FAILURE;
    }

    if (_pr_win_connectex == NULL) {
        PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
        return PR_FAILURE;
    }

    PROsfd osfd = fd->secret->md.osfd;
    PRInt32 rv, err;
    PRInt32 bytesSent = 0;
    DWORD rvSent;

    BOOL option = 1;
    rv = setsockopt((SOCKET)osfd, IPPROTO_TCP, TCP_FASTOPEN, (char*)&option, sizeof(option));
    if (rv != 0) {
        err = WSAGetLastError();
        PR_LOG(_pr_io_lm, PR_LOG_MIN,
               ("_PR_MD_TCPSENDTO error set opt TCP_FASTOPEN failed %d\n", err));
        if (err == WSAENOPROTOOPT) {
            PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
        } else {
            _PR_MD_MAP_SETSOCKOPT_ERROR(err);
        }
        return -1;
    }

    /* ConnectEx requires the socket to be initially bound. We will use INADDR_ANY. */
    PRNetAddr bindAddr;
    memset(&bindAddr, 0, sizeof(bindAddr));
    bindAddr.raw.family = addr->raw.family;

    rv = bind((SOCKET)osfd, (const struct sockaddr *)&(bindAddr.inet), PR_NETADDR_SIZE(&bindAddr));
    if (rv != 0) {
        err = WSAGetLastError();
        PR_LOG(_pr_io_lm, PR_LOG_MIN,
               ("_PR_MD_TCPSENDTO error bind failed %d\n", err));
        _PR_MD_MAP_SETSOCKOPT_ERROR(err);
        return -1;
    }

    PR_LOG(_pr_io_lm, PR_LOG_MIN,
           ("_PR_MD_TCPSENDTO calling _pr_win_connectex  %d %p\n", amount, (char*)buf));

    rvSent = 0;
    memset(&fd->secret->ol, 0, sizeof(fd->secret->ol));
    /* ConnectEx return TRUE on a success and FALSE on an error. */
    if (_pr_win_connectex( (SOCKET)osfd, (struct sockaddr *) addr,
                           addrlen, buf, amount,
                           &rvSent, &fd->secret->ol) == TRUE) {
        /* When ConnectEx is used, all previously set socket options and
         * property are not enabled and to enable them
         * SO_UPDATE_CONNECT_CONTEXT option need to be set. */
        rv = setsockopt((SOCKET)osfd, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
        if (rv != 0) {
            err = WSAGetLastError();
            PR_LOG(_pr_io_lm, PR_LOG_MIN,
                   ("_PR_MD_TCPSENDTO setting SO_UPDATE_CONNECT_CONTEXT failed %d\n", err));
            _PR_MD_MAP_SETSOCKOPT_ERROR(err);
            return -1;
        }
        /* We imitate Linux here. SendTo will return number of bytes send but
         * it can not return connection success at the same time, so we return
         * number of bytes send and "connection success" will be return on the
         * connectcontinue. */
        fd->secret->alreadyConnected = PR_TRUE;
        return rvSent;
    } else {
        err = WSAGetLastError();
        PR_LOG(_pr_io_lm, PR_LOG_MIN,
               ("_PR_MD_TCPSENDTO error _pr_win_connectex failed %d\n", err));
        if (err != ERROR_IO_PENDING) {
            _PR_MD_MAP_CONNECT_ERROR(err);
            return -1;
        } else if (fd->secret->nonblocking) {
            /* Remember that overlapped structure is set. We will need to get
             * the final result of ConnectEx call. */
            fd->secret->overlappedActive = PR_TRUE;

            /* ConnectEx will copy supplied data to a internal buffer and send
             * them during Fast Open or after connect. Therefore we can assumed
             * this data already send. */
            if (amount > 0) {
                return amount;
            }

            _PR_MD_MAP_CONNECT_ERROR(WSAEWOULDBLOCK);
            return -1;
        }
        // err is ERROR_IO_PENDING and socket is blocking, so query
        // GetOverlappedResult.
        err = ERROR_IO_INCOMPLETE;
        while (err == ERROR_IO_INCOMPLETE) {
            rv = socket_io_wait(osfd, WRITE_FD, timeout);
            if ( rv < 0 ) {
                return -1;
            }
            rv = GetOverlappedResult(osfd, &fd->secret->ol, &rvSent, FALSE);
            if ( rv == TRUE ) {
                return rvSent;
            } else {
                err = WSAGetLastError();
                if (err != ERROR_IO_INCOMPLETE) {
                    _PR_MD_MAP_CONNECT_ERROR(err);
                    return -1;
                }
            }
        }
    }
    return -1;
}
#endif

PRInt32
_PR_MD_RECVFROM(PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags,
                PRNetAddr *addr, PRUint32 *addrlen, PRIntervalTime timeout)
{
    PROsfd osfd = fd->secret->md.osfd;
    PRInt32 rv, err;

    while ((rv = recvfrom( osfd, buf, amount, 0, (struct sockaddr *) addr,
                           addrlen)) == -1)
    {
        if (((err = WSAGetLastError()) == WSAEWOULDBLOCK)
            && (!fd->secret->nonblocking))
        {
            rv = socket_io_wait(osfd, READ_FD, timeout);
            if ( rv < 0)
            {
                return -1;
            }
        }
        else
        {
            _PR_MD_MAP_RECVFROM_ERROR(err);
            break;
        }
    }
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
        rv = _PR_MD_SEND(fd, iov[index].iov_base, iov[index].iov_len, 0, timeout);
        if (rv > 0) {
            sent += rv;
        }
        if ( rv != iov[index].iov_len )
        {
            if (rv < 0)
            {
                if (fd->secret->nonblocking
                    && (PR_GetError() == PR_WOULD_BLOCK_ERROR)
                    && (sent > 0))
                {
                    return sent;
                }
                else
                {
                    return -1;
                }
            }
            /* Only a nonblocking socket can have partial sends */
            PR_ASSERT(fd->secret->nonblocking);
            return sent;
        }
    }
    return sent;
}

PRInt32
_PR_MD_SHUTDOWN(PRFileDesc *fd, PRIntn how)
{
    PRInt32 rv;

    rv = shutdown(fd->secret->md.osfd, how);
    if (rv < 0) {
        _PR_MD_MAP_SHUTDOWN_ERROR(WSAGetLastError());
    }
    return rv;
}

PRStatus
_PR_MD_GETSOCKNAME(PRFileDesc *fd, PRNetAddr *addr, PRUint32 *len)
{
    PRInt32 rv;

    rv = getsockname((SOCKET)fd->secret->md.osfd, (struct sockaddr *)addr, len);
    if (rv==0) {
        return PR_SUCCESS;
    } else {
        _PR_MD_MAP_GETSOCKNAME_ERROR(WSAGetLastError());
        return PR_FAILURE;
    }
}

PRStatus
_PR_MD_GETPEERNAME(PRFileDesc *fd, PRNetAddr *addr, PRUint32 *len)
{
    PRInt32 rv;

    rv = getpeername((SOCKET)fd->secret->md.osfd, (struct sockaddr *)addr, len);
    if (rv==0) {
        return PR_SUCCESS;
    } else {
        _PR_MD_MAP_GETPEERNAME_ERROR(WSAGetLastError());
        return PR_FAILURE;
    }
}

PRStatus
_PR_MD_GETSOCKOPT(PRFileDesc *fd, PRInt32 level, PRInt32 optname, char* optval, PRInt32* optlen)
{
    PRInt32 rv;

    rv = getsockopt((SOCKET)fd->secret->md.osfd, level, optname, optval, optlen);
    if (rv==0) {
        return PR_SUCCESS;
    } else {
        _PR_MD_MAP_GETSOCKOPT_ERROR(WSAGetLastError());
        return PR_FAILURE;
    }
}

PRStatus
_PR_MD_SETSOCKOPT(PRFileDesc *fd, PRInt32 level, PRInt32 optname, const char* optval, PRInt32 optlen)
{
    PRInt32 rv;

    rv = setsockopt((SOCKET)fd->secret->md.osfd, level, optname, optval, optlen);
    if (rv==0) {
        return PR_SUCCESS;
    } else {
        _PR_MD_MAP_SETSOCKOPT_ERROR(WSAGetLastError());
        return PR_FAILURE;
    }
}

void
_MD_MakeNonblock(PRFileDesc *f)
{
    return; /* do nothing */
}



/*
 * socket_io_wait --
 *
 * Wait for socket i/o, periodically checking for interrupt.
 *
 * This function returns 1 on success.  On failure, it returns
 * -1 and sets the error codes.  It never returns 0.
 */
#define _PR_INTERRUPT_CHECK_INTERVAL_SECS 5

static PRInt32 socket_io_wait(
    PROsfd osfd,
    PRInt32 fd_type,
    PRIntervalTime timeout)
{
    PRInt32 rv = -1;
    struct timeval tv;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRIntervalTime elapsed, remaining;
    PRBool wait_for_remaining;
    fd_set rd_wr, ex;
    int err, len;

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
            FD_ZERO(&rd_wr);
            FD_ZERO(&ex);
            do {
                FD_SET(osfd, &rd_wr);
                FD_SET(osfd, &ex);
                switch( fd_type )
                {
                    case READ_FD:
                        rv = _MD_SELECT(0, &rd_wr, NULL, NULL, &tv);
                        break;
                    case WRITE_FD:
                        rv = _MD_SELECT(0, NULL, &rd_wr, NULL, &tv);
                        break;
                    case CONNECT_FD:
                        rv = _MD_SELECT(0, NULL, &rd_wr, &ex, &tv);
                        break;
                    default:
                        PR_ASSERT(0);
                        break;
                } /* end switch() */
                if (rv == -1 )
                {
                    _PR_MD_MAP_SELECT_ERROR(WSAGetLastError());
                    break;
                }
                if ( rv > 0 && fd_type == CONNECT_FD )
                {
                    /*
                     * Call Sleep(0) to work around a Winsock timing bug.
                     */
                    Sleep(0);
                    if (FD_ISSET((SOCKET)osfd, &ex))
                    {
                        len = sizeof(err);
                        if (getsockopt(osfd, SOL_SOCKET, SO_ERROR,
                                       (char *) &err, &len) == SOCKET_ERROR)
                        {
                            _PR_MD_MAP_GETSOCKOPT_ERROR(WSAGetLastError());
                            return -1;
                        }
                        if (err != 0) {
                            _PR_MD_MAP_CONNECT_ERROR(err);
                        }
                        else {
                            PR_SetError(PR_UNKNOWN_ERROR, 0);
                        }
                        return -1;
                    }
                    if (FD_ISSET((SOCKET)osfd, &rd_wr))
                    {
                        /* it's connected */
                        return 1;
                    }
                    PR_ASSERT(0);
                }
                if (_PR_PENDING_INTERRUPT(me)) {
                    me->flags &= ~_PR_INTERRUPT;
                    PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
                    rv = -1;
                    break;
                }
            } while (rv == 0);
            break;
        default:
            remaining = timeout;
            FD_ZERO(&rd_wr);
            FD_ZERO(&ex);
            do {
                /*
                 * We block in _MD_SELECT for at most
                 * _PR_INTERRUPT_CHECK_INTERVAL_SECS seconds,
                 * so that there is an upper limit on the delay
                 * before the interrupt bit is checked.
                 */
                wait_for_remaining = PR_TRUE;
                tv.tv_sec = PR_IntervalToSeconds(remaining);
                if (tv.tv_sec > _PR_INTERRUPT_CHECK_INTERVAL_SECS) {
                    wait_for_remaining = PR_FALSE;
                    tv.tv_sec = _PR_INTERRUPT_CHECK_INTERVAL_SECS;
                    tv.tv_usec = 0;
                } else {
                    tv.tv_usec = PR_IntervalToMicroseconds(
                                     remaining -
                                     PR_SecondsToInterval(tv.tv_sec));
                }
                FD_SET(osfd, &rd_wr);
                FD_SET(osfd, &ex);
                switch( fd_type )
                {
                    case READ_FD:
                        rv = _MD_SELECT(0, &rd_wr, NULL, NULL, &tv);
                        break;
                    case WRITE_FD:
                        rv = _MD_SELECT(0, NULL, &rd_wr, NULL, &tv);
                        break;
                    case CONNECT_FD:
                        rv = _MD_SELECT(0, NULL, &rd_wr, &ex, &tv);
                        break;
                    default:
                        PR_ASSERT(0);
                        break;
                } /* end switch() */
                if (rv == -1)
                {
                    _PR_MD_MAP_SELECT_ERROR(WSAGetLastError());
                    break;
                }
                if ( rv > 0 && fd_type == CONNECT_FD )
                {
                    /*
                     * Call Sleep(0) to work around a Winsock timing bug.
                     */
                    Sleep(0);
                    if (FD_ISSET((SOCKET)osfd, &ex))
                    {
                        len = sizeof(err);
                        if (getsockopt(osfd, SOL_SOCKET, SO_ERROR,
                                       (char *) &err, &len) == SOCKET_ERROR)
                        {
                            _PR_MD_MAP_GETSOCKOPT_ERROR(WSAGetLastError());
                            return -1;
                        }
                        if (err != 0) {
                            _PR_MD_MAP_CONNECT_ERROR(err);
                        }
                        else {
                            PR_SetError(PR_UNKNOWN_ERROR, 0);
                        }
                        return -1;
                    }
                    if (FD_ISSET((SOCKET)osfd, &rd_wr))
                    {
                        /* it's connected */
                        return 1;
                    }
                    PR_ASSERT(0);
                }
                if (_PR_PENDING_INTERRUPT(me)) {
                    me->flags &= ~_PR_INTERRUPT;
                    PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
                    rv = -1;
                    break;
                }
                /*
                 * We loop again if _MD_SELECT timed out and the
                 * timeout deadline has not passed yet.
                 */
                if (rv == 0 )
                {
                    if (wait_for_remaining) {
                        elapsed = remaining;
                    } else {
                        elapsed = PR_SecondsToInterval(tv.tv_sec)
                                  + PR_MicrosecondsToInterval(tv.tv_usec);
                    }
                    if (elapsed >= remaining) {
                        PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                        rv = -1;
                        break;
                    } else {
                        remaining = remaining - elapsed;
                    }
                }
            } while (rv == 0 );
            break;
    }
    return(rv);
} /* end socket_io_wait() */
