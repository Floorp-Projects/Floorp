/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* OS/2 Sockets module
 *
 */

/*Note from DSR111297 - it should be noted that there are two flavors of select() on OS/2    */
/*There is standard BSD (which is kind of slow) and a new flavor of select() that takes      */
/*an integer list of sockets, the number of read sockets, write sockets, except sockets, and */
/*a millisecond count for timeout. In the interest of performance I have choosen the OS/2    */
/*specific version of select(). See OS/2 TCP/IP Programmer's Toolkit for more info.          */

#include "primpl.h"

#include <sys/time.h> /* For timeval. */

#define _PR_INTERRUPT_CHECK_INTERVAL_SECS 5
#define READ_FD   1
#define WRITE_FD  2

/* --- SOCKET IO --------------------------------------------------------- */


PRInt32
_PR_MD_SOCKET(int domain, int type, int flags)
{
    PRInt32 osfd, err;

    osfd = socket(domain, type, flags);

    if (osfd == -1)
    {
        err = sock_errno();
        _PR_MD_MAP_SOCKET_ERROR(err);
    }

    return(osfd);
}

/*
** _MD_CloseSocket() -- Close a socket
**
*/
PRInt32
_MD_CloseSocket(PRInt32 osfd)
{
    PRInt32 rv, err;

    rv = soclose(osfd);
    if (rv == -1) {
        err = sock_errno();
        _PR_MD_MAP_CLOSE_ERROR(err);
    }
    return rv;
}

PRInt32
_MD_SocketAvailable(PRFileDesc *fd)
{
    PRInt32 result;

    if (so_ioctl(fd->secret->md.osfd, FIONREAD, (char *) &result, sizeof(result)) < 0) {
        PR_SetError(PR_BAD_DESCRIPTOR_ERROR, sock_errno());
        return -1;
    }
    return result;
}

static PRInt32
socket_io_wait( PRInt32 osfd, PRInt32 fd_type, PRIntervalTime timeout )
{
    PRInt32 rv = -1;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRIntervalTime epoch, now, elapsed, remaining;
    PRBool wait_for_remaining;
    PRInt32 syserror;
#ifdef BSD_SELECT
    struct timeval tv;
    fd_set rd_wr;
#else
    int socks[1];
    long lTimeout;
#endif

    switch (timeout) {
        case PR_INTERVAL_NO_WAIT:
            PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
            break;
        case PR_INTERVAL_NO_TIMEOUT:
            /*
             * This is a special case of the 'default' case below.
             * Please see the comments there.
             */
#ifdef BSD_SELECT
            tv.tv_sec = _PR_INTERRUPT_CHECK_INTERVAL_SECS;
            tv.tv_usec = 0;
            FD_ZERO(&rd_wr);
            do {
                FD_SET(osfd, &rd_wr);
                if (fd_type == READ_FD) {
                    rv = bsdselect(osfd + 1, &rd_wr, NULL, NULL, &tv);
                }
                else {
                    rv = bsdselect(osfd + 1, NULL, &rd_wr, NULL, &tv);
                }
#else
            lTimeout = _PR_INTERRUPT_CHECK_INTERVAL_SECS * 1000;
            do {
                socks[0] = osfd;
                if (fd_type == READ_FD) {
                    rv = os2_select(socks, 1, 0, 0, lTimeout);
                }
                else {
                    rv = os2_select(socks, 0, 1, 0, lTimeout);
                }
#endif
                if (rv == -1 && (syserror = sock_errno()) != EINTR) {
                    _PR_MD_MAP_SELECT_ERROR(syserror);
                    break;
                }
                if (_PR_PENDING_INTERRUPT(me)) {
                    me->flags &= ~_PR_INTERRUPT;
                    PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
                    rv = -1;
                    break;
                }
            } while (rv == 0 || (rv == -1 && syserror == EINTR));
            break;
        default:
            now = epoch = PR_IntervalNow();
            remaining = timeout;
#ifdef BSD_SELECT
            FD_ZERO(&rd_wr);
#endif
            do {
                /*
                 * We block in select for at most
                 * _PR_INTERRUPT_CHECK_INTERVAL_SECS seconds,
                 * so that there is an upper limit on the delay
                 * before the interrupt bit is checked.
                 */
#ifdef BSD_SELECT
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
                if (fd_type == READ_FD) {
                    rv = bsdselect(osfd + 1, &rd_wr, NULL, NULL, &tv);
                }
                else {
                    rv = bsdselect(osfd + 1, NULL, &rd_wr, NULL, &tv);
                }
#else
                wait_for_remaining = PR_TRUE;
                lTimeout = PR_IntervalToMilliseconds(remaining);
                if (lTimeout > _PR_INTERRUPT_CHECK_INTERVAL_SECS * 1000) {
                    wait_for_remaining = PR_FALSE;
                    lTimeout = _PR_INTERRUPT_CHECK_INTERVAL_SECS * 1000;
                }
                socks[0] = osfd;
                if (fd_type == READ_FD) {
                    rv = os2_select(socks, 1, 0, 0, lTimeout);
                }
                else {
                    rv = os2_select(socks, 0, 1, 0, lTimeout);
                }
#endif
                /*
                 * we don't consider EINTR a real error
                 */
                if (rv == -1 && (syserror = sock_errno()) != EINTR) {
                    _PR_MD_MAP_SELECT_ERROR(syserror);
                    break;
                }
                if (_PR_PENDING_INTERRUPT(me)) {
                    me->flags &= ~_PR_INTERRUPT;
                    PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
                    rv = -1;
                    break;
                }
                /*
                 * We loop again if select timed out or got interrupted
                 * by a signal, and the timeout deadline has not passed yet.
                 */
                if (rv == 0 || (rv == -1 && syserror == EINTR)) {
                    /*
                     * If select timed out, we know how much time
                     * we spent in blocking, so we can avoid a
                     * PR_IntervalNow() call.
                     */
                    if (rv == 0) {
                        if (wait_for_remaining) {
                            now += remaining;
                        } else {
#ifdef BSD_SELECT
                            now += PR_SecondsToInterval(tv.tv_sec)
                                   + PR_MicrosecondsToInterval(tv.tv_usec);
#else
                            now += PR_MillisecondsToInterval(lTimeout);
#endif
                        }
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
_MD_Accept(PRFileDesc *fd, PRNetAddr *addr,
           PRUint32 *addrlen, PRIntervalTime timeout)
{
    PRInt32 osfd = fd->secret->md.osfd;
    PRInt32 rv, err;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    while ((rv = accept(osfd, (struct sockaddr*) addr, (int*)addrlen)) == -1)
    {
        err = sock_errno();
        if ((err == EWOULDBLOCK) || (err == ECONNABORTED))
        {
            if (fd->secret->nonblocking) {
                break;
            }
            if ((rv = socket_io_wait(osfd, READ_FD, timeout)) < 0) {
                goto done;
            }
        } else if ((err == EINTR) && (!_PR_PENDING_INTERRUPT(me))) {
            continue;
        } else {
            break;
        }
    }
    if (rv < 0) {
        _PR_MD_MAP_ACCEPT_ERROR(err);
    }
done:
    return(rv);
}

PRInt32
_PR_MD_CONNECT(PRFileDesc *fd, const PRNetAddr *addr, PRUint32 addrlen,
               PRIntervalTime timeout)
{
    PRInt32 rv, err;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRInt32 osfd = fd->secret->md.osfd;
    PRNetAddr addrCopy = *addr; /* Work around a bug in OS/2 where connect
                                 * modifies the sockaddr structure.
                                 * See Bugzilla bug 100776. */

    /*
     * We initiate the connection setup by making a nonblocking connect()
     * call.  If the connect() call fails, there are two cases we handle
     * specially:
     * 1. The connect() call was interrupted by a signal.  In this case
     *    we simply retry connect().
     * 2. The NSPR socket is nonblocking and connect() fails with
     *    EINPROGRESS.  We first wait until the socket becomes writable.
     *    Then we try to find out whether the connection setup succeeded
     *    or failed.
     */

retry:
    if ((rv = connect(osfd, (struct sockaddr *)&addrCopy, addrlen)) == -1)
    {
        err = sock_errno();

        if (err == EINTR) {
            if (_PR_PENDING_INTERRUPT(me)) {
                me->flags &= ~_PR_INTERRUPT;
                PR_SetError( PR_PENDING_INTERRUPT_ERROR, 0);
                return -1;
            }
            goto retry;
        }

        if (!fd->secret->nonblocking && (err == EINPROGRESS))
        {
            /*
             * socket_io_wait() may return -1 or 1.
             */

            rv = socket_io_wait(osfd, WRITE_FD, timeout);
            if (rv == -1) {
                return -1;
            }

            PR_ASSERT(rv == 1);
            if (_PR_PENDING_INTERRUPT(me)) {
                me->flags &= ~_PR_INTERRUPT;
                PR_SetError( PR_PENDING_INTERRUPT_ERROR, 0);
                return -1;
            }
            err = _MD_os2_get_nonblocking_connect_error(osfd);
            if (err != 0) {
                _PR_MD_MAP_CONNECT_ERROR(err);
                return -1;
            }
            return 0;
        }

        _PR_MD_MAP_CONNECT_ERROR(err);
    }

    return rv;
}  /* _MD_connect */

PRInt32
_PR_MD_BIND(PRFileDesc *fd, const PRNetAddr *addr, PRUint32 addrlen)
{
    PRInt32 rv, err;
    rv = bind(fd->secret->md.osfd, (struct sockaddr *) addr, (int )addrlen);
    if (rv < 0) {
        err = sock_errno();
        _PR_MD_MAP_BIND_ERROR(err);
    }
    return(rv);
}


PRInt32
_PR_MD_LISTEN(PRFileDesc *fd, PRIntn backlog)
{
    PRInt32 rv, err;
    rv = listen(fd->secret->md.osfd, backlog);
    if (rv < 0)  {
        err = sock_errno();
        _PR_MD_MAP_DEFAULT_ERROR(err);
    }
    return(rv);
}


PRInt32
_PR_MD_RECV(PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags,
            PRIntervalTime timeout)
{
    PRInt32 osfd = fd->secret->md.osfd;
    PRInt32 rv, err;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    while ((rv = recv(osfd,buf,amount,flags)) == -1)
    {
        err = sock_errno();
        if ((err == EWOULDBLOCK)) {
            if (fd->secret->nonblocking) {
                break;
            }
            if ((rv = socket_io_wait(osfd, READ_FD, timeout)) < 0) {
                goto done;
            }
        } else if ((err == EINTR) && (!_PR_PENDING_INTERRUPT(me))) {
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
    PRInt32 rv, err;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    while ((rv = send(osfd,buf,amount,flags)) == -1)
    {
        err = sock_errno();
        if ((err == EWOULDBLOCK)) {
            if (fd->secret->nonblocking) {
                break;
            }
            if ((rv = socket_io_wait(osfd, WRITE_FD, timeout)) < 0) {
                goto done;
            }
        } else if ((err == EINTR) && (!_PR_PENDING_INTERRUPT(me))) {
            continue;
        } else {
            break;
        }
    }

    /*
     * optimization; if bytes sent is less than "amount" call
     * select before returning. This is because it is likely that
     * the next send() call will return EWOULDBLOCK.
     */
    if ((!fd->secret->nonblocking) && (rv > 0) && (rv < amount)
        && (timeout != PR_INTERVAL_NO_WAIT))
    {
        if (socket_io_wait(osfd, WRITE_FD, timeout)< 0) {
            rv = -1;
            goto done;
        }
    }
    if (rv < 0) {
        _PR_MD_MAP_SEND_ERROR(err);
    }
done:
    return(rv);
}

PRInt32
_PR_MD_SENDTO(PRFileDesc *fd, const void *buf, PRInt32 amount, PRIntn flags,
              const PRNetAddr *addr, PRUint32 addrlen, PRIntervalTime timeout)
{
    PRInt32 osfd = fd->secret->md.osfd;
    PRInt32 rv, err;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    while ((rv = sendto(osfd, buf, amount, flags,
                        (struct sockaddr *) addr, addrlen)) == -1)
    {
        err = sock_errno();
        if ((err == EWOULDBLOCK))
        {
            if (fd->secret->nonblocking) {
                break;
            }
            if ((rv = socket_io_wait(osfd, WRITE_FD, timeout)) < 0) {
                goto done;
            }
        } else if ((err == EINTR) && (!_PR_PENDING_INTERRUPT(me))) {
            continue;
        } else {
            break;
        }
    }
    if (rv < 0) {
        _PR_MD_MAP_SENDTO_ERROR(err);
    }
done:
    return(rv);
}

PRInt32
_PR_MD_RECVFROM(PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags,
                PRNetAddr *addr, PRUint32 *addrlen, PRIntervalTime timeout)
{
    PRInt32 osfd = fd->secret->md.osfd;
    PRInt32 rv, err;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    while( (*addrlen = PR_NETADDR_SIZE(addr)),
           ((rv = recvfrom(osfd, buf, amount, flags,
                           (struct sockaddr *) addr, (int *)addrlen)) == -1))
    {
        err = sock_errno();
        if ((err == EWOULDBLOCK)) {
            if (fd->secret->nonblocking) {
                break;
            }
            if ((rv = socket_io_wait(osfd, READ_FD, timeout)) < 0) {
                goto done;
            }
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
_PR_MD_WRITEV(PRFileDesc *fd, const PRIOVec *iov, PRInt32 iov_size,
              PRIntervalTime timeout)
{
    PRInt32 rv, err;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRInt32 index, amount = 0;
    PRInt32 osfd = fd->secret->md.osfd;
    struct iovec osiov[PR_MAX_IOVECTOR_SIZE];

    /* Ensured by PR_Writev */
    PR_ASSERT(iov_size <= PR_MAX_IOVECTOR_SIZE);

    /*
     * We can't pass iov to so_writev because PRIOVec and struct iovec
     * may not be binary compatible.  Make osiov a copy of iov and
     * pass osiov to so_writev .
     */
    for (index = 0; index < iov_size; index++) {
        osiov[index].iov_base = iov[index].iov_base;
        osiov[index].iov_len = iov[index].iov_len;
    }

    /*
     * Calculate the total number of bytes to be sent; needed for
     * optimization later.
     * We could avoid this if this number was passed in; but it is
     * probably not a big deal because iov_size is usually small (less than
     * 3)
     */
    if (!fd->secret->nonblocking) {
        for (index=0; index<iov_size; index++) {
            amount += iov[index].iov_len;
        }
    }

    while ((rv = so_writev(osfd, osiov, iov_size)) == -1) {
        err = sock_errno();
        if ((err == EWOULDBLOCK))    {
            if (fd->secret->nonblocking) {
                break;
            }
            if ((rv = socket_io_wait(osfd, WRITE_FD, timeout))<0) {
                goto done;
            }
        } else if ((err == EINTR) && (!_PR_PENDING_INTERRUPT(me))) {
            continue;
        } else {
            break;
        }
    }

    /*
     * optimization; if bytes sent is less than "amount" call
     * select before returning. This is because it is likely that
     * the next writev() call will return EWOULDBLOCK.
     */
    if ((!fd->secret->nonblocking) && (rv > 0) && (rv < amount)
        && (timeout != PR_INTERVAL_NO_WAIT)) {
        if (socket_io_wait(osfd, WRITE_FD, timeout) < 0) {
            rv = -1;
            goto done;
        }
    }
    if (rv < 0) {
        _PR_MD_MAP_WRITEV_ERROR(err);
    }
done:
    return(rv);
}

PRInt32
_PR_MD_SHUTDOWN(PRFileDesc *fd, PRIntn how)
{
    PRInt32 rv;

    rv = shutdown(fd->secret->md.osfd, how);
    if (rv < 0) {
        _PR_MD_MAP_SHUTDOWN_ERROR(sock_errno());
    }
    return rv;
}

PRInt32
_PR_MD_SOCKETPAIR(int af, int type, int flags, PRInt32 *osfd)
{
    PRInt32 rv, err;

    rv = socketpair(af, type, flags, osfd);
    if (rv < 0) {
        err = _MD_ERRNO();
        _PR_MD_MAP_SOCKETPAIR_ERROR(err);
    }
    return rv;
}

PRStatus
_PR_MD_GETSOCKNAME(PRFileDesc *fd, PRNetAddr *addr, PRUint32 *addrlen)
{
    PRInt32 rv, err;

    rv = getsockname(fd->secret->md.osfd,
                     (struct sockaddr *) addr, (int *)addrlen);
    if (rv < 0) {
        err = sock_errno();
        _PR_MD_MAP_GETSOCKNAME_ERROR(err);
    }
    return rv==0?PR_SUCCESS:PR_FAILURE;
}

PRStatus
_PR_MD_GETPEERNAME(PRFileDesc *fd, PRNetAddr *addr, PRUint32 *addrlen)
{
    PRInt32 rv, err;

    rv = getpeername(fd->secret->md.osfd,
                     (struct sockaddr *) addr, (int *)addrlen);
    if (rv < 0) {
        err = sock_errno();
        _PR_MD_MAP_GETPEERNAME_ERROR(err);
    }
    return rv==0?PR_SUCCESS:PR_FAILURE;
}

PRStatus
_PR_MD_GETSOCKOPT(PRFileDesc *fd, PRInt32 level, PRInt32 optname,
                  char* optval, PRInt32* optlen)
{
    PRInt32 rv, err;

    rv = getsockopt(fd->secret->md.osfd, level, optname, optval, (int *)optlen);
    if (rv < 0) {
        err = sock_errno();
        _PR_MD_MAP_GETSOCKOPT_ERROR(err);
    }
    return rv==0?PR_SUCCESS:PR_FAILURE;
}

PRStatus
_PR_MD_SETSOCKOPT(PRFileDesc *fd, PRInt32 level, PRInt32 optname,
                  const char* optval, PRInt32 optlen)
{
    PRInt32 rv, err;

    rv = setsockopt(fd->secret->md.osfd, level, optname, optval, optlen);
    if (rv < 0) {
        err = sock_errno();
        _PR_MD_MAP_SETSOCKOPT_ERROR(err);
    }
    return rv==0?PR_SUCCESS:PR_FAILURE;
}

void
_MD_MakeNonblock(PRFileDesc *fd)
{
    PRInt32 osfd = fd->secret->md.osfd;
    PRInt32 err;
    PRUint32  one = 1;

    if (osfd <= 2) {
        /* Don't mess around with stdin, stdout or stderr */
        return;
    }

    err = so_ioctl( osfd, FIONBIO, (char *) &one, sizeof(one));
    if ( err != 0 )
    {
        err = sock_errno();
        _PR_MD_MAP_SOCKET_ERROR(err);
    }
}
