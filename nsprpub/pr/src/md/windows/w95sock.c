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

/* Win95 Sockets module
 *
 */

#include "primpl.h"


/* --- SOCKET IO --------------------------------------------------------- */


PRInt32
_PR_MD_SOCKET(int af, int type, int flags)
{
    SOCKET sock;
    PRUint32  one = 1;
    PRInt32   rv;
    PRInt32   err;

    sock = socket(af, type, flags);

    if (sock == INVALID_SOCKET ) 
    {
        int rv = WSAGetLastError();
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

/*
** _MD_CloseSocket() -- Close a socket
**
*/
PRInt32
_MD_CloseSocket(PRInt32 osfd)
{
    PRInt32 rv = SOCKET_ERROR;

    rv = closesocket((SOCKET) osfd );
	if (rv < 0)
		_PR_MD_MAP_SOCKET_ERROR(WSAGetLastError());

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

PRInt32
_MD_Accept(PRFileDesc *fd, PRNetAddr *raddr, PRUint32 *rlen,
              PRIntervalTime timeout )
{
    PRInt32 osfd = fd->secret->md.osfd;
    PRInt32 rv, err;
    fd_set rd;
    struct timeval tv, *tvp;

    FD_ZERO(&rd);
    FD_SET((SOCKET)osfd, &rd);
    if (timeout == PR_INTERVAL_NO_TIMEOUT) 
    {
        while ((rv = accept(osfd, (struct sockaddr *) raddr, rlen)) == -1) 
        {
            if (((err = WSAGetLastError()) == WSAEWOULDBLOCK) 
                && (!fd->secret->nonblocking))
            {
                if ((rv = select(osfd + 1, &rd, NULL, NULL,NULL)) == -1) {
					_PR_MD_MAP_SELECT_ERROR(WSAGetLastError());
                    break;
            } 
            } 
            else {
				_PR_MD_MAP_ACCEPT_ERROR(err);
                break;
        }
        }
        return(rv);
    } 
    else if (timeout == PR_INTERVAL_NO_WAIT) 
    {
        if ((rv = accept(osfd, (struct sockaddr *) raddr, rlen)) == -1)
        {
            if (((err = WSAGetLastError()) == WSAEWOULDBLOCK) 
                && (!fd->secret->nonblocking))
            {
				PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
            }
            else
            {
                _PR_MD_MAP_ACCEPT_ERROR(err);
            }
        }
            return(rv);
    } 
    else 
    {
retry:
        if ((rv = accept(osfd, (struct sockaddr *) raddr, rlen)) == -1) 
        {
            if (((err = WSAGetLastError()) == WSAEWOULDBLOCK) 
                && (!fd->secret->nonblocking))
            {
                tv.tv_sec = PR_IntervalToSeconds(timeout);
                tv.tv_usec = PR_IntervalToMicroseconds(
                    timeout - PR_SecondsToInterval(tv.tv_sec));
                tvp = &tv;

                rv = select(osfd + 1, &rd, NULL, NULL, tvp);
                if (rv > 0) {
                    goto retry;
                } 
                else if (rv == 0) 
                {
					PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                    rv = -1;
                } else {
					_PR_MD_MAP_SELECT_ERROR(WSAGetLastError());
                }
            } else {
				_PR_MD_MAP_ACCEPT_ERROR(err);
            }
        }
    }
    return(rv);
} /* end _MD_Accept() */


PRInt32
_PR_MD_CONNECT(PRFileDesc *fd, const PRNetAddr *addr, PRUint32 addrlen, 
               PRIntervalTime timeout)
{
    PRInt32 osfd = fd->secret->md.osfd;
    PRInt32 rv;
    int err, len;
    fd_set wd, ex;
    struct timeval tv, *tvp;

    if ((rv = connect(osfd, (struct sockaddr *) addr, addrlen)) == -1) 
    {
        err = WSAGetLastError();
        if ((!fd->secret->nonblocking) && (err == WSAEWOULDBLOCK))
        {
            if (timeout == PR_INTERVAL_NO_TIMEOUT)
                tvp = NULL;
            else 
            {
                tv.tv_sec = PR_IntervalToSeconds(timeout);
                tv.tv_usec = PR_IntervalToMicroseconds(
                timeout - PR_SecondsToInterval(tv.tv_sec));
                tvp = &tv;
            }

            FD_ZERO(&wd);
            FD_SET((SOCKET)osfd, &wd);
            FD_ZERO(&ex);
            FD_SET((SOCKET)osfd, &ex);
            rv = select(osfd + 1, NULL, &wd, &ex, tvp);
            if (rv > 0) 
            {
                if (FD_ISSET((SOCKET)osfd, &ex))
                {
                    Sleep(0);
                    len = sizeof(err);
                    if (getsockopt(osfd, SOL_SOCKET, SO_ERROR,
                            (char *) &err, &len) == SOCKET_ERROR)
                    {  
                        _PR_MD_MAP_GETSOCKOPT_ERROR(WSAGetLastError());
                        return -1;
                    }
                    if (err != 0)
                        _PR_MD_MAP_CONNECT_ERROR(err);
                    else
                        PR_SetError(PR_UNKNOWN_ERROR, 0);
                    return -1;
                }
                if (FD_ISSET((SOCKET)osfd, &wd))
                {
                    /* it's connected */
                    return 0;
                }
            } 
            else if (rv == 0) 
            {
				PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                return(-1);
            } else if (rv < 0) 
            {
				_PR_MD_MAP_SELECT_ERROR(WSAGetLastError());
                return(-1);
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
    PRInt32 rv, err;
    struct timeval tv, *tvp;
    fd_set rd;

    while ((rv = recv( osfd, buf, amount, 0)) == -1) 
    {
        if (((err = WSAGetLastError()) == WSAEWOULDBLOCK) 
            && (!fd->secret->nonblocking))
        {
            FD_ZERO(&rd);
            FD_SET((SOCKET)osfd, &rd);
            if (timeout == PR_INTERVAL_NO_TIMEOUT) 
            {
                tvp = NULL;
            } 
            else 
            {
                tv.tv_sec = PR_IntervalToSeconds(timeout);
                tv.tv_usec = PR_IntervalToMicroseconds(
                timeout - PR_SecondsToInterval(tv.tv_sec));
                tvp = &tv;
            }
            if ((rv = select(osfd + 1, &rd, NULL, NULL, tvp)) == -1) 
            {
				_PR_MD_MAP_SELECT_ERROR(WSAGetLastError());
                return -1;
            } 
            else if (rv == 0) 
            {
				PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                rv = -1;
                break;
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
    PRInt32 osfd = fd->secret->md.osfd;
    PRInt32 rv, err;
    struct timeval tv, *tvp;
    fd_set wd;
    PRInt32 bytesSent = 0;

    while(bytesSent < amount ) 
    {
        while ((rv = send( osfd, buf, amount, 0 )) == -1) 
        {
            if (((err = WSAGetLastError()) == WSAEWOULDBLOCK) 
                && (!fd->secret->nonblocking))
            {
                if ( timeout == PR_INTERVAL_NO_TIMEOUT ) 
                {
                    tvp = NULL;
                } 
                else 
                {
                    tv.tv_sec = PR_IntervalToSeconds(timeout);
                    tv.tv_usec = PR_IntervalToMicroseconds(
                        timeout - PR_SecondsToInterval(tv.tv_sec));
                    tvp = &tv;
                }
                FD_ZERO(&wd);
                FD_SET((SOCKET)osfd, &wd);
                if ((rv = select( osfd + 1, NULL, &wd, NULL,tvp)) == -1) {
					_PR_MD_MAP_SELECT_ERROR(WSAGetLastError());
                    break;
				}
                if (rv == 0) 
                {
					PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                        return -1;
                }
            } 
            else {
				_PR_MD_MAP_SEND_ERROR(err);
                return -1;
        }
        }
        bytesSent += rv;
        if (fd->secret->nonblocking)
        {
            break;
        }
        if ((rv >= 0) && (bytesSent < amount )) 
        {
            if ( timeout == PR_INTERVAL_NO_TIMEOUT ) 
            {
                tvp = NULL;
            } 
            else 
            {
                tv.tv_sec = PR_IntervalToSeconds(timeout);
                tv.tv_usec = PR_IntervalToMicroseconds(
                    timeout - PR_SecondsToInterval(tv.tv_sec));
                tvp = &tv;
            }
            FD_ZERO(&wd);
            FD_SET((SOCKET)osfd, &wd);
            if ((rv = select(osfd + 1, NULL, &wd, NULL,tvp)) == -1) {
				_PR_MD_MAP_SELECT_ERROR(WSAGetLastError());
                break;
			}
            if (rv == 0) 
            {
				PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
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
    PRInt32 osfd = fd->secret->md.osfd;
    PRInt32 rv, err;
    struct timeval tv, *tvp;
    fd_set wd;
    PRInt32 bytesSent = 0;

    while(bytesSent < amount) 
    {
        while ((rv = sendto( osfd, buf, amount, 0, (struct sockaddr *) addr,
                addrlen)) == -1) 
        {
            if (((err = WSAGetLastError()) == WSAEWOULDBLOCK) 
                && (!fd->secret->nonblocking))
            {
                if ( timeout == PR_INTERVAL_NO_TIMEOUT ) 
                {
                    tvp = NULL;
                } 
                else 
                {
                    tv.tv_sec = PR_IntervalToSeconds(timeout);
                    tv.tv_usec = PR_IntervalToMicroseconds(
                        timeout - PR_SecondsToInterval(tv.tv_sec));
                    tvp = &tv;
                }
                FD_ZERO(&wd);
                FD_SET((SOCKET)osfd, &wd);
                if ((rv = select(osfd + 1, NULL, &wd, NULL, tvp)) == -1) {
					_PR_MD_MAP_SELECT_ERROR(WSAGetLastError());
                    break;
				}
                if (rv == 0) 
                {
					PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                        return -1;
                }
            } 
            else {
				_PR_MD_MAP_SENDTO_ERROR(err);
                return -1;
        }
        }
        bytesSent += rv;
        if (fd->secret->nonblocking)
        {
            break;
        }
        if ((rv >= 0) && (bytesSent < amount )) 
        {
            if ( timeout == PR_INTERVAL_NO_TIMEOUT ) 
            {
                tvp = NULL;
            } 
            else 
            {
                tv.tv_sec = PR_IntervalToSeconds(timeout);
                tv.tv_usec = PR_IntervalToMicroseconds(
                    timeout - PR_SecondsToInterval(tv.tv_sec));
                tvp = &tv;
            }
            FD_ZERO(&wd);
            FD_SET((SOCKET)osfd, &wd);
            if ((rv = select( osfd + 1, NULL, &wd, NULL, tvp)) == -1) {
				_PR_MD_MAP_SELECT_ERROR(WSAGetLastError());
                break;
			}
            if (rv == 0) 
            {
				PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                    return -1;
            }
        }
    }
    return bytesSent;
}

PRInt32
_PR_MD_RECVFROM(PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags,
                PRNetAddr *addr, PRUint32 *addrlen, PRIntervalTime timeout)
{
    PRInt32 osfd = fd->secret->md.osfd;
    PRInt32 rv, err;
    struct timeval tv, *tvp;
    fd_set rd;

    while ((rv = recvfrom( osfd, buf, amount, 0, (struct sockaddr *) addr,
            addrlen)) == -1) 
    {
        if (((err = WSAGetLastError()) == WSAEWOULDBLOCK) 
            && (!fd->secret->nonblocking))
        {
            if (timeout == PR_INTERVAL_NO_TIMEOUT) 
            {
                tvp = NULL;
            } 
            else 
            {
                tv.tv_sec = PR_IntervalToSeconds(timeout);
                tv.tv_usec = PR_IntervalToMicroseconds(
                timeout - PR_SecondsToInterval(tv.tv_sec));
                tvp = &tv;
            }
            FD_ZERO(&rd);
            FD_SET((SOCKET)osfd, &rd);
            if ((rv = select(osfd + 1, &rd, NULL, NULL, tvp)) == -1) 
            {
				_PR_MD_MAP_SELECT_ERROR(WSAGetLastError());
                return -1;
            } else if (rv == 0) 
            {
				PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                rv = -1;
                break;
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
_PR_MD_WRITEV(PRFileDesc *fd, PRIOVec *iov, PRInt32 iov_size, PRIntervalTime timeout)
{
    int index;
    int sent = 0;
    int rv;

    for (index=0; index < iov_size; index++) 
    {
        rv = _PR_MD_SEND(fd, iov[index].iov_base, iov[index].iov_len, 0, timeout);
        if (rv > 0) 
            sent += rv;
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
	if (rv < 0)
		_PR_MD_MAP_SHUTDOWN_ERROR(WSAGetLastError());
	return rv;
}

PRStatus
_PR_MD_GETSOCKNAME(PRFileDesc *fd, PRNetAddr *addr, PRUint32 *len)
{
    PRInt32 rv;

    rv = getsockname((SOCKET)fd->secret->md.osfd, (struct sockaddr *)addr, len);
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

    rv = getpeername((SOCKET)fd->secret->md.osfd, (struct sockaddr *)addr, len);
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

    rv = getsockopt((SOCKET)fd->secret->md.osfd, level, optname, optval, optlen);
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
_MD_MakeNonblock(PRFileDesc *f)
{
    return; // do nothing!
}
