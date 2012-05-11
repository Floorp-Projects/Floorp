/* -*- Mode: C++; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

#define READ_FD     1
#define WRITE_FD    2

/*
** This is a support routine to handle "deferred" i/o on sockets. 
** It uses "select", so it is subject to all of the BeOS limitations
** (only READ notification, only sockets)
*/

/*
 * socket_io_wait --
 *
 * wait for socket i/o, periodically checking for interrupt
 *
 */

static PRInt32 socket_io_wait(PRInt32 osfd, PRInt32 fd_type,
                              PRIntervalTime timeout)
{
    PRInt32 rv = -1;
    struct timeval tv;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRIntervalTime epoch, now, elapsed, remaining;
    PRBool wait_for_remaining;
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
        FD_ZERO(&rd_wr);
        do {
            FD_SET(osfd, &rd_wr);
            if (fd_type == READ_FD)
                rv = _MD_SELECT(osfd + 1, &rd_wr, NULL, NULL, &tv);
            else
                rv = _MD_SELECT(osfd + 1, NULL, &rd_wr, NULL, &tv);
            if (rv == -1 && (syserror = _MD_ERRNO()) != EINTR) {
#ifdef BONE_VERSION
                _PR_MD_MAP_SELECT_ERROR(syserror);
#else
                if (syserror == EBADF) {
                    PR_SetError(PR_BAD_DESCRIPTOR_ERROR, EBADF);
                } else {
                    PR_SetError(PR_UNKNOWN_ERROR, syserror);
                }
#endif
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
        FD_ZERO(&rd_wr);
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
            if (fd_type == READ_FD)
                rv = _MD_SELECT(osfd + 1, &rd_wr, NULL, NULL, &tv);
            else
                rv = _MD_SELECT(osfd + 1, NULL, &rd_wr, NULL, &tv);
            /*
             * we don't consider EINTR a real error
             */
            if (rv == -1 && (syserror = _MD_ERRNO()) != EINTR) {
#ifdef BONE_VERSION
                _PR_MD_MAP_SELECT_ERROR(syserror);
#else
                if (syserror == EBADF) {
                    PR_SetError(PR_BAD_DESCRIPTOR_ERROR, EBADF);
                } else {
                    PR_SetError(PR_UNKNOWN_ERROR, syserror);
                }
#endif
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
                    if (wait_for_remaining) {
                        now += remaining;
                    } else {
                        now += PR_SecondsToInterval(tv.tv_sec)
                               + PR_MicrosecondsToInterval(tv.tv_usec);
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
_MD_recv (PRFileDesc *fd, void *buf, PRInt32 amount, PRInt32 flags,
          PRIntervalTime timeout)
{
    PRInt32 osfd = fd->secret->md.osfd;
    PRInt32 rv, err;
    PRThread *me = _PR_MD_CURRENT_THREAD();

#ifndef BONE_VERSION
    if (fd->secret->md.sock_state & BE_SOCK_SHUTDOWN_READ) {
        _PR_MD_MAP_RECV_ERROR(EPIPE);
        return -1;
    }
#endif

#ifdef BONE_VERSION
    /*
    ** Gah, stupid hack.  If reading a zero amount, instantly return success.
    ** BONE beta 6 returns EINVAL for reads of zero bytes, which parts of
    ** mozilla use to check for socket availability.
    */

    if( 0 == amount ) return(0);
#endif

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

        } else
            break;
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
#ifdef _PR_HAVE_SOCKADDR_LEN
    if (rv != -1) {
        /* ignore the sa_len field of struct sockaddr */
        if (addr) {
            addr->raw.family = ((struct sockaddr *) addr)->sa_family;
        }
    }
#endif /* _PR_HAVE_SOCKADDR_LEN */
    return(rv);
}

PRInt32
_MD_send (PRFileDesc *fd, const void *buf, PRInt32 amount, PRInt32 flags,
          PRIntervalTime timeout)
{
    PRInt32 osfd = fd->secret->md.osfd;
    PRInt32 rv, err;
    PRThread *me = _PR_MD_CURRENT_THREAD();

#ifndef BONE_VERSION
    if (fd->secret->md.sock_state & BE_SOCK_SHUTDOWN_WRITE)
    {
        _PR_MD_MAP_SEND_ERROR(EPIPE);
        return -1;
    }
#endif

    while ((rv = send(osfd, buf, amount, flags)) == -1) {
        err = _MD_ERRNO();

        if ((err == EAGAIN) || (err == EWOULDBLOCK)) {
            if (fd->secret->nonblocking) {
                break;
            }

#ifndef BONE_VERSION
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
#else /* BONE_VERSION */
            if ((rv = socket_io_wait(osfd, WRITE_FD, timeout))< 0)
                goto done;
#endif

        } else if ((err == EINTR) && (!_PR_PENDING_INTERRUPT(me))) {
            continue;

        } else {
            break;
        }
    }

#ifdef BONE_VERSION
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
#endif /* BONE_VERSION */
    
    if (rv < 0) {
        _PR_MD_MAP_SEND_ERROR(err);
    }

#ifdef BONE_VERSION
done:
#endif
    return(rv);
}

PRInt32
_MD_sendto (PRFileDesc *fd, const void *buf, PRInt32 amount, PRIntn flags,
            const PRNetAddr *addr, PRUint32 addrlen, PRIntervalTime timeout)
{
    PRInt32 osfd = fd->secret->md.osfd;
    PRInt32 rv, err;
    PRThread *me = _PR_MD_CURRENT_THREAD();
#ifdef _PR_HAVE_SOCKADDR_LEN
    PRNetAddr addrCopy;

    addrCopy = *addr;
    ((struct sockaddr *) &addrCopy)->sa_len = addrlen;
    ((struct sockaddr *) &addrCopy)->sa_family = addr->raw.family;

    while ((rv = sendto(osfd, buf, amount, flags,
                        (struct sockaddr *) &addrCopy, addrlen)) == -1) {
#else
    while ((rv = sendto(osfd, buf, amount, flags,
                        (struct sockaddr *) addr, addrlen)) == -1) {
#endif
        err = _MD_ERRNO();

        if ((err == EAGAIN) || (err == EWOULDBLOCK)) {
            if (fd->secret->nonblocking) {
                break;
            }

#ifdef BONE_VERSION
            if ((rv = socket_io_wait(osfd, WRITE_FD, timeout))< 0)
                goto done;
#endif
        } else if ((err == EINTR) && (!_PR_PENDING_INTERRUPT(me))) {
            continue;

        } else {
            break;
        }
    }

    if (rv < 0) {
        _PR_MD_MAP_SENDTO_ERROR(err);
    }

#ifdef BONE_VERSION
done:
#endif
    return(rv);
}

#ifdef BONE_VERSION

PRInt32 _MD_writev(
    PRFileDesc *fd, const PRIOVec *iov,
    PRInt32 iov_size, PRIntervalTime timeout)
{
    PRInt32 rv, err;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRInt32 index, amount = 0;
    PRInt32 osfd = fd->secret->md.osfd;
    struct iovec osiov[PR_MAX_IOVECTOR_SIZE];

    /* Ensured by PR_Writev */
    PR_ASSERT(iov_size <= PR_MAX_IOVECTOR_SIZE);

    /*
     * We can't pass iov to writev because PRIOVec and struct iovec
     * may not be binary compatible.  Make osiov a copy of iov and
     * pass osiov to writev.
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

    while ((rv = writev(osfd, osiov, iov_size)) == -1) {
        err = _MD_ERRNO();
        if ((err == EAGAIN) || (err == EWOULDBLOCK))    {
            if (fd->secret->nonblocking) {
                break;
            }
            if ((rv = socket_io_wait(osfd, WRITE_FD, timeout))<0)
                goto done;

        } else if ((err == EINTR) && (!_PR_PENDING_INTERRUPT(me))){
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

#endif /* BONE_VERSION */

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
    if (rv < 0) {
        _PR_MD_MAP_ACCEPT_ERROR(err);
    } else if (addr != NULL) {
        /* bug 134099 */
        err = getpeername(rv, (struct sockaddr *) addr, (_PRSockLen_t *)addrlen);
    }
done:
#ifdef _PR_HAVE_SOCKADDR_LEN
    if (rv != -1) {
        /* Mask off the first byte of struct sockaddr (the length field) */
        if (addr) {
            addr->raw.family = ((struct sockaddr *) addr)->sa_family;
        }
    }
#endif /* _PR_HAVE_SOCKADDR_LEN */
    return(rv);
}

PRInt32
_MD_connect (PRFileDesc *fd, const PRNetAddr *addr, PRUint32 addrlen,
             PRIntervalTime timeout)
{
    PRInt32 rv, err;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRInt32 osfd = fd->secret->md.osfd;

#ifndef BONE_VERSION
    fd->secret->md.connectValueValid = PR_FALSE;
#endif
#ifdef _PR_HAVE_SOCKADDR_LEN
    PRNetAddr addrCopy;

    addrCopy = *addr;
    ((struct sockaddr *) &addrCopy)->sa_len = addrlen;
    ((struct sockaddr *) &addrCopy)->sa_family = addr->raw.family;
#endif

    /* (Copied from unix.c)
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
#ifdef _PR_HAVE_SOCKADDR_LEN
    if ((rv = connect(osfd, (struct sockaddr *)&addrCopy, addrlen)) == -1) {
#else
    if ((rv = connect(osfd, (struct sockaddr *)addr, addrlen)) == -1) {
#endif
        err = _MD_ERRNO();
#ifndef BONE_VERSION
        fd->secret->md.connectReturnValue = rv;
        fd->secret->md.connectReturnError = err;
        fd->secret->md.connectValueValid = PR_TRUE;
#endif
        if( err == EINTR ) {

            if( _PR_PENDING_INTERRUPT(me)) {

                me->flags &= ~_PR_INTERRUPT;
                PR_SetError( PR_PENDING_INTERRUPT_ERROR, 0);
                return -1;
            }
#ifndef BONE_VERSION
            snooze( 100000L );
#endif
            goto retry;
        }

#ifndef BONE_VERSION
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
            if (connectCount < sizeof(connectList)/sizeof(connectList[0])) {
                connectList[connectCount].osfd = osfd;
                memcpy(&connectList[connectCount].addr, addr, addrlen);
                connectList[connectCount].addrlen = addrlen;
                connectList[connectCount].timeout = timeout;
                connectCount++;
                PR_Unlock(_connectLock);
                _PR_MD_MAP_CONNECT_ERROR(err);
            } else {
                PR_Unlock(_connectLock);
                PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, 0);
            }
            return rv;
        }
#else /* BONE_VERSION */
        if(!fd->secret->nonblocking && (err == EINTR)) {

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
            err = _MD_beos_get_nonblocking_connect_error(osfd);
            if (err != 0) {
                _PR_MD_MAP_CONNECT_ERROR(err);
                return -1;
            }
            return 0;
        }
#endif

        _PR_MD_MAP_CONNECT_ERROR(err);
    }

    return rv;
}

PRInt32
_MD_bind (PRFileDesc *fd, const PRNetAddr *addr, PRUint32 addrlen)
{
    PRInt32 rv, err;
#ifdef _PR_HAVE_SOCKADDR_LEN
    PRNetAddr addrCopy;

    addrCopy = *addr;
    ((struct sockaddr *) &addrCopy)->sa_len = addrlen;
    ((struct sockaddr *) &addrCopy)->sa_family = addr->raw.family;
    rv = bind(fd->secret->md.osfd, (struct sockaddr *) &addrCopy, (int )addrlen);
#else
    rv = bind(fd->secret->md.osfd, (struct sockaddr *) addr, (int )addrlen);
#endif
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

#ifndef BONE_VERSION
    /* Bug workaround!  Setting listen to 0 on Be accepts no connections.
    ** On most UN*Xes this sets the default.
    */

    if( backlog == 0 ) backlog = 5;
#endif

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

#ifndef BONE_VERSION
    if (how == PR_SHUTDOWN_SEND)
        fd->secret->md.sock_state = BE_SOCK_SHUTDOWN_WRITE;
    else if (how == PR_SHUTDOWN_RCV)
        fd->secret->md.sock_state = BE_SOCK_SHUTDOWN_READ;
    else if (how == PR_SHUTDOWN_BOTH) {
        fd->secret->md.sock_state = (BE_SOCK_SHUTDOWN_WRITE | BE_SOCK_SHUTDOWN_READ);
    }

    return 0;
#else /* BONE_VERSION */
    rv = shutdown(fd->secret->md.osfd, how);
    if (rv < 0) {
        err = _MD_ERRNO();
        _PR_MD_MAP_SHUTDOWN_ERROR(err);
    }
    return(rv);
#endif
}

PRInt32
_MD_socketpair (int af, int type, int flags, PRInt32 *osfd)
{
    return PR_NOT_IMPLEMENTED_ERROR;
}

PRInt32
_MD_close_socket (PRInt32 osfd)
{
#ifdef BONE_VERSION
    close( osfd );
#else
    closesocket( osfd );
#endif
}

PRStatus
_MD_getsockname (PRFileDesc *fd, PRNetAddr *addr, PRUint32 *addrlen)
{
    PRInt32 rv, err;

    rv = getsockname(fd->secret->md.osfd,
                     (struct sockaddr *) addr, (_PRSockLen_t *)addrlen);
#ifdef _PR_HAVE_SOCKADDR_LEN
    if (rv == 0) {
        /* ignore the sa_len field of struct sockaddr */
        if (addr) {
            addr->raw.family = ((struct sockaddr *) addr)->sa_family;
        }
    }
#endif /* _PR_HAVE_SOCKADDR_LEN */
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

#ifdef _PR_HAVE_SOCKADDR_LEN
    if (rv == 0) {
        /* ignore the sa_len field of struct sockaddr */
        if (addr) {
            addr->raw.family = ((struct sockaddr *) addr)->sa_family;
        }
    }
#endif /* _PR_HAVE_SOCKADDR_LEN */

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
    PRInt32 rv, err;

    rv = getsockopt(fd->secret->md.osfd, level, optname,
                    optval, (_PRSockLen_t *)optlen);
    if (rv < 0) {
        err = _MD_ERRNO();
        _PR_MD_MAP_GETSOCKOPT_ERROR(err);
    }

    return rv==0?PR_SUCCESS:PR_FAILURE;
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

#ifndef BONE_VERSION
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
#else
PRInt32
_MD_socket(PRInt32 domain, PRInt32 type, PRInt32 proto)
{
    PRInt32 osfd, err;

    osfd = socket(domain, type, proto);

    if (osfd == -1) {
        err = _MD_ERRNO();
        _PR_MD_MAP_SOCKET_ERROR(err);
    }

    return(osfd);
}
#endif

PRInt32
_MD_socketavailable (PRFileDesc *fd)
{
#ifdef BONE_VERSION
    PRInt32 result;

    if (ioctl(fd->secret->md.osfd, FIONREAD, &result) < 0) {
        _PR_MD_MAP_SOCKETAVAILABLE_ERROR(_MD_ERRNO());
        return -1;
    }
    return result;
#else
    return PR_NOT_IMPLEMENTED_ERROR;
#endif
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

#ifndef BONE_VERSION
PRInt32
_MD_beos_get_nonblocking_connect_error(PRFileDesc *fd)
{
    int rv;
    int flags = 0;

    rv = recv(fd->secret->md.osfd, NULL, 0, flags);
    PR_ASSERT(-1 == rv || 0 == rv);
    if (-1 == rv && errno != EAGAIN && errno != EWOULDBLOCK) {
        return errno;
    }
    return 0;  /* no error */
}
#else
PRInt32
_MD_beos_get_nonblocking_connect_error(int osfd)
{
    return PR_NOT_IMPLEMENTED_ERROR;
    //    int err;
    //    _PRSockLen_t optlen = sizeof(err);
    //    if (getsockopt(osfd, SOL_SOCKET, SO_ERROR, (char *) &err, &optlen) == -1) {
    //        return errno;
    //    } else {
    //        return err;
    //    }
}
#endif /* BONE_VERSION */
