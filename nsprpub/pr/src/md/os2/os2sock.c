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

/* OS/2 Sockets module
 *
 */

/*Note from DSR111297 - it should be noted that there are two flavors of select() on OS/2    */
/*There is standard BSD (which is kind of slow) and a new flavor of select() that takes      */
/*an integer list of sockets, the number of read sockets, write sockets, except sockets, and */
/*a millisecond count for timeout. In the interest of performance I have choosen the OS/2    */
/*specific version of select(). See OS/2 TCP/IP Programmer's Toolkit for more info.          */ 

#include "primpl.h"

#ifdef XP_OS2_EMX
    #include <sys/time.h> /* For timeval. */
#endif

void
_PR_MD_INIT_IO()
{
    sock_init();
}

/* --- SOCKET IO --------------------------------------------------------- */


PRInt32
_PR_MD_SOCKET(int af, int type, int flags)
{
    int sock;
    PRUint32  one = 1;
    PRInt32   rv;
    PRInt32   err;

    sock = socket(af, type, flags);

    if (sock == -1 ) 
    {
        int rv = sock_errno();
        soclose(sock);
		_PR_MD_MAP_SOCKET_ERROR(rv);
        return (PRInt32) -1;
    }

    /*
    ** Make the socket Non-Blocking
    */
    rv = ioctl( sock, FIONBIO, (char *) &one, sizeof(one));
    if ( rv != 0 )
    {
        err = sock_errno();
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
    PRInt32 rv = -1;

    rv = soclose((int) osfd );
	if (rv < 0)
		_PR_MD_MAP_SOCKET_ERROR(sock_errno());

    return rv;
}

PRInt32
_MD_SocketAvailable(PRFileDesc *fd)
{
    PRInt32 result;

    if (ioctl(fd->secret->md.osfd, FIONREAD, (char *) &result, sizeof(result)) < 0) {
		PR_SetError(PR_BAD_DESCRIPTOR_ERROR, sock_errno());
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
#ifdef BSD_SELECT
    fd_set rd;
    struct timeval tv, *tvp;

    FD_ZERO(&rd);
    FD_SET(osfd, &rd);
#else
    int socks[1];
    socks[0] = osfd; 
#endif
    if (timeout == PR_INTERVAL_NO_TIMEOUT) 
    {
        while ((rv = accept(osfd, (struct sockaddr *) raddr, (int *) rlen)) == -1) 
        {
            if (((err = sock_errno()) == EWOULDBLOCK) 
                && (!fd->secret->nonblocking))
            {
#ifdef BSD_SELECT
                if ((rv = select(osfd + 1, &rd, NULL, NULL,NULL)) == -1) {
#else
                if ((rv = select(socks, 1, 0, 0, -1)) == -1) {
#endif
					_PR_MD_MAP_SELECT_ERROR(sock_errno());
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
        if ((rv = accept(osfd, (struct sockaddr *) raddr, (int *) rlen)) == -1)
        {
            if (((err = sock_errno()) == EWOULDBLOCK) 
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
        if ((rv = accept(osfd, (struct sockaddr *) raddr, (int *) rlen)) == -1) 
        {
            if (((err = sock_errno()) == EWOULDBLOCK) 
                && (!fd->secret->nonblocking))
            {
#ifdef BSD_SELECT
                tv.tv_sec = PR_IntervalToSeconds(timeout);
                tv.tv_usec = PR_IntervalToMicroseconds(
                    timeout - PR_SecondsToInterval(tv.tv_sec));
                tvp = &tv;
                rv = select(osfd + 1, &rd, NULL, NULL, tvp);
#else
                long lTimeout = PR_IntervalToMilliseconds(timeout); 
                rv = select(socks, 1, 0, 0, lTimeout);
#endif
                if (rv > 0) {
                    goto retry;
                } 
                else if (rv == 0) 
                {
					PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                    rv = -1;
                } else {
					_PR_MD_MAP_SELECT_ERROR(sock_errno());
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
#ifdef BSD_SELECT
    fd_set wd, ex;
    struct timeval tv, *tvp;
#else
    int socks[1]; 
    long lTimeout = -1;
#endif

    if ((rv = connect(osfd, (struct sockaddr *) addr, addrlen)) == -1) 
    {
        err = sock_errno();
        if ((!fd->secret->nonblocking) && ((err == EINPROGRESS) || (err == EWOULDBLOCK)))
        {
#ifdef BSD_SELECT
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
            FD_SET(osfd, &wd);
            FD_ZERO(&ex);
            FD_SET(osfd, &ex);
            rv = select(osfd + 1, NULL, &wd, &ex, tvp);
#else
            if (timeout == PR_INTERVAL_NO_TIMEOUT)
                lTimeout = -1;
            else 
            {
                lTimeout = PR_IntervalToMilliseconds(timeout);  
            }
	    
            socks[0] = osfd; 
            rv = select(socks, 0, 1, 1, lTimeout);
#endif
            if (rv > 0) 
            {
#ifdef BSD_SELECT
               if (FD_ISSET(osfd, &ex))
               {
                   DosSleep(0);
                   len = sizeof(err);
                   if (getsockopt(osfd, SOL_SOCKET, SO_ERROR,
                           (char *) &err, &len) < 0)
                   {  
                       _PR_MD_MAP_GETSOCKOPT_ERROR(sock_errno());
                       return -1;
                   }
                   if (err != 0)
                       _PR_MD_MAP_CONNECT_ERROR(err);
                   else
                       PR_SetError(PR_UNKNOWN_ERROR, 0);
                   return -1;
               }
               if (FD_ISSET(osfd, &wd))
               {
                   /* it's connected */
                   return 0;
               }
#else
               if (getsockopt(osfd, SOL_SOCKET, SO_ERROR,
                       (char *) &err, &len) < 0)
               {  
                   _PR_MD_MAP_GETSOCKOPT_ERROR(sock_errno());
                   return -1;
               }
               else
                  return 0; /* It's connected ! */
#endif
            } 
            else if (rv == 0) 
            {
				PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                return(-1);
            } else if (rv < 0) 
            {
				_PR_MD_MAP_SELECT_ERROR(sock_errno());
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

    rv = bind(fd->secret->md.osfd, (struct sockaddr*) &(addr->inet), addrlen);

    if (rv == -1)  {
		_PR_MD_MAP_BIND_ERROR(sock_errno());
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
#ifdef BSD_SELECT
    struct timeval tv, *tvp;
    fd_set rd;
#else
    int socks[1]; 
    long lTimeout = -1; 
#endif

    while ((rv = recv( osfd, buf, amount, 0)) == -1) 
    {
        if (((err = sock_errno()) == EWOULDBLOCK) 
            && (!fd->secret->nonblocking))
        {
#ifdef BSD_SELECT
           FD_ZERO(&rd);
           FD_SET(osfd, &rd);
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
#else
            socks[0] = osfd; 
            if (timeout == PR_INTERVAL_NO_TIMEOUT) 
            {
                lTimeout = -1; 
            } 
            else 
            {
                lTimeout = PR_IntervalToMilliseconds(timeout); 
            }
            if ((rv = select(socks, 1, 0, 0, lTimeout)) == -1) 
#endif
            {
				_PR_MD_MAP_SELECT_ERROR(sock_errno());
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
#ifdef BSD_SELECT
    struct timeval tv, *tvp;
    fd_set wd;
#else
    int socks[1]; 
    long lTimeout = -1; 
#endif
    PRInt32 bytesSent = 0;

    while(bytesSent < amount ) 
    {
        while ((rv = send( osfd, (char *) buf, amount, 0 )) == -1) 
        {
            if (((err = sock_errno()) == EWOULDBLOCK) 
                && (!fd->secret->nonblocking))
            {
#ifdef BSD_SELECT
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
                FD_SET(osfd, &wd);
                if ((rv = select( osfd + 1, NULL, &wd, NULL,tvp)) == -1) {
#else
                if ( timeout == PR_INTERVAL_NO_TIMEOUT ) 
                {
                    lTimeout = -1; 
                } 
                else 
                {
                    lTimeout = PR_IntervalToMilliseconds(timeout); 
                }
                socks[0] = osfd; 
                if ((rv = select( socks, 0, 1, 0, lTimeout)) == -1) {
#endif
					_PR_MD_MAP_SELECT_ERROR(sock_errno());
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
#ifdef BSD_SELECT
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
           FD_SET(osfd, &wd);
           if ((rv = select(osfd + 1, NULL, &wd, NULL,tvp)) == -1) {
#else
            if ( timeout == PR_INTERVAL_NO_TIMEOUT ) 
            {
                lTimeout = -1; 
            } 
            else 
            {
                lTimeout = PR_IntervalToMilliseconds(timeout); 
            }
            socks[0] = osfd; 
            if ((rv = select(socks, 0, 1, 0,lTimeout)) == -1) {
#endif
				_PR_MD_MAP_SELECT_ERROR(sock_errno());
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
    PRInt32 bytesSent = 0;
#ifdef BSD_SELECT
    struct timeval tv, *tvp;
    fd_set wd;
#else
    int socks[1];
    long lTimeout = -1; 
#endif

    while(bytesSent < amount) 
    {
        while ((rv = sendto( osfd, (char *) buf, amount, 0, (struct sockaddr *) addr,
                addrlen)) == -1) 
        {
            if (((err = sock_errno()) == EWOULDBLOCK) 
                && (!fd->secret->nonblocking))
            {
#ifdef BSD_SELECT
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
               FD_SET(osfd, &wd);
               if ((rv = select(osfd + 1, NULL, &wd, NULL, tvp)) == -1) {
#else
                if ( timeout == PR_INTERVAL_NO_TIMEOUT ) 
                {
                    lTimeout = -1; 
                } 
                else 
                {
                    lTimeout = PR_IntervalToMilliseconds(timeout);
                }
                socks[0] = osfd; 
                if ((rv = select(socks, 0, 1, 0, lTimeout)) == -1) {
#endif
					_PR_MD_MAP_SELECT_ERROR(sock_errno());
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
#ifdef BSD_SELECT
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
           FD_SET(osfd, &wd);
           if ((rv = select( osfd + 1, NULL, &wd, NULL, tvp)) == -1) {
#else
            if ( timeout == PR_INTERVAL_NO_TIMEOUT ) 
            {
                lTimeout = -1; 
            } 
            else 
            {
                lTimeout = PR_IntervalToMilliseconds(timeout);  
            }
            socks[0] = osfd; 
            if ((rv = select( socks, 0, 1, 0, lTimeout)) == -1) {
#endif
				_PR_MD_MAP_SELECT_ERROR(sock_errno());
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
    PRUint32 addrlen_temp = *addrlen;
#ifdef BSD_SELECT
    struct timeval tv, *tvp;
    fd_set rd;
#else
    int socks[1]; 
    long lTimeout = -1; 
#endif

    while ((rv = recvfrom( osfd, (char *) buf, amount, 0, (struct sockaddr *) addr,
            (int *) addrlen)) == -1) 
    {
        if (((err = sock_errno()) == EWOULDBLOCK) 
            && (!fd->secret->nonblocking))
        {
#ifdef BSD_SELECT
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
           FD_SET(osfd, &rd);
           if ((rv = select(osfd + 1, &rd, NULL, NULL, tvp)) == -1) 
#else
            if (timeout == PR_INTERVAL_NO_TIMEOUT) 
            {
                lTimeout = -1;
            } 
            else 
            {
                lTimeout = PR_IntervalToMilliseconds(timeout);  
            }
            socks[0] = osfd; 
            if ((rv = select(socks, 1, 0, 0, lTimeout)) == -1) 
#endif
            {
				_PR_MD_MAP_SELECT_ERROR(sock_errno());
                return -1;
            } else if (rv == 0) 
            {
				PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                rv = -1;
                break;
            }

            /* recvfrom blows this value away if it fails first time */
            *addrlen = addrlen_temp;
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
		_PR_MD_MAP_SHUTDOWN_ERROR(sock_errno());
	return rv;
}

PRStatus
_PR_MD_GETSOCKNAME(PRFileDesc *fd, PRNetAddr *addr, PRUint32 *len)
{
    PRInt32 rv;

    rv = getsockname((int)fd->secret->md.osfd, (struct sockaddr *)addr, (int *) len);
    if (rv==0)
		return PR_SUCCESS;
	else {
		_PR_MD_MAP_GETSOCKNAME_ERROR(sock_errno());
		return PR_FAILURE;
	}
}

PRStatus
_PR_MD_GETPEERNAME(PRFileDesc *fd, PRNetAddr *addr, PRUint32 *len)
{
    PRInt32 rv;

    rv = getpeername((int)fd->secret->md.osfd, (struct sockaddr *)addr, (int *) len);
    if (rv==0)
		return PR_SUCCESS;
	else {
		_PR_MD_MAP_GETPEERNAME_ERROR(sock_errno());
		return PR_FAILURE;
	}
}

PRStatus
_PR_MD_GETSOCKOPT(PRFileDesc *fd, PRInt32 level, PRInt32 optname, char* optval, PRInt32* optlen)
{
    PRInt32 rv;

    rv = getsockopt((int)fd->secret->md.osfd, level, optname, optval, optlen);
    if (rv==0)
		return PR_SUCCESS;
	else {
		_PR_MD_MAP_GETSOCKOPT_ERROR(sock_errno());
		return PR_FAILURE;
	}
}

PRStatus
_PR_MD_SETSOCKOPT(PRFileDesc *fd, PRInt32 level, PRInt32 optname, const char* optval, PRInt32 optlen)
{
    PRInt32 rv;

    rv = setsockopt((int)fd->secret->md.osfd, level, optname, (char *) optval, optlen);
    if (rv==0)
		return PR_SUCCESS;
	else {
		_PR_MD_MAP_SETSOCKOPT_ERROR(sock_errno());
		return PR_FAILURE;
	}
}

void
_MD_MakeNonblock(PRFileDesc *f)
{
    return; /* do nothing! */ 
}
