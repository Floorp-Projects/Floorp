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

#include <errno.h>

#define OTUNIXERRORS 1		/* We want OpenTransport error codes */
#include <OpenTransport.h>
#include <OpenTptInternet.h>	// All the internet typedefs

#include "macsock.h" /* from macsock library */
#include "primpl.h"

void _MD_InitNetAccess()
{
}

static void macsock_map_error(int err)
{
	switch (err) {
		case EBADF:
			PR_SetError(PR_BAD_DESCRIPTOR_ERROR, err);
			break;
		case EADDRNOTAVAIL:
			PR_SetError(PR_ADDRESS_NOT_AVAILABLE_ERROR, err);
			break;
		case EINPROGRESS:
			PR_SetError(PR_IN_PROGRESS_ERROR, err);
			break;
		case EWOULDBLOCK:
			PR_SetError(PR_WOULD_BLOCK_ERROR, err);
			break;
		case ENOTSOCK:
			PR_SetError(PR_NOT_SOCKET_ERROR, err);
			break;
		case EAFNOSUPPORT:
			PR_SetError(PR_ADDRESS_NOT_SUPPORTED_ERROR, err);
			break;
		case ETIMEDOUT:
			PR_SetError(PR_IO_TIMEOUT_ERROR, err);
			break;
		case ECONNREFUSED:
			PR_SetError(PR_CONNECT_REFUSED_ERROR, err);
			break;
		case ENETUNREACH:
			PR_SetError(PR_NETWORK_UNREACHABLE_ERROR, err);
			break;
		case EADDRINUSE:
			PR_SetError(PR_ADDRESS_IN_USE_ERROR, err);
			break;
		case EFAULT:
			PR_SetError(PR_ACCESS_FAULT_ERROR, err);
			break;
		/*
		 * UNIX domain sockets are not supported in NSPR
		 */
		case EACCES:
			PR_SetError(PR_ADDRESS_NOT_SUPPORTED_ERROR, err);
			break;
		case EINTR:
			PR_SetError(PR_PENDING_INTERRUPT_ERROR, err);
			break;
		case EINVAL:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, err);
			break;
		case EIO:
			PR_SetError(PR_IO_ERROR, err);
			break;
		case ENOENT:
			PR_SetError(PR_ADDRESS_NOT_SUPPORTED_ERROR, err);
			break;
		case ENXIO:
			PR_SetError(PR_IO_ERROR, err);
			break;
		case EPROTOTYPE:
			PR_SetError(PR_ADDRESS_NOT_SUPPORTED_ERROR, err);
			break;
		default:
			PR_SetError(PR_UNKNOWN_ERROR, err);
			break;
	}
}

// Errors returned:
// ENETDOWN - no MacTCP driver
// EPROTONOSUPPORT - bad socket type/protocol
// ENOBUFS - not enough space for another socket, or failure in socket creation routine
PRInt32 _MD_socket(int domain, int type, int protocol)
{
	int	err;
	
	err = macsock_socket(domain, type, protocol);
	if (err == -1) {
		macsock_map_error(errno);
	    return -1;
	 }
	 
	return err;
}

// Errors:
// EBADF -- bad socket id
// EFAULT -- bad address format
PRInt32 _MD_bind(PRFileDesc *fd, PRNetAddr *addr, PRUint32 addrlen)
{
	int	err;
	int sID = fd->secret->md.osfd;
	
	err = macsock_bind(sID, (const struct sockaddr *)addr, addrlen);
	if (err == -1) {
		macsock_map_error(errno);
	    return -1;
	 }
	 
	return err;
}	

// Errors:
// EBADF -- bad socket id
// EOPNOTSUPP -- socket is already connected, and closing
// EISCONN -- already connected
// EINPROGRESS -- connecting right now
PRInt32 _MD_listen(PRFileDesc *fd, PRIntn backlog)
{
	int	err;
	int sID = fd->secret->md.osfd;

	err = macsock_listen(sID, backlog);
	if (err == -1) {
		macsock_map_error(errno);
	    return -1;
	 }
	 
	return err;
}	

// Errors:
// EBADF -- bad socket id
PRInt32 _MD_getsockname(PRFileDesc *fd, PRNetAddr *addr, PRUint32 *addrlen)
{
	int	err;
	int sID = fd->secret->md.osfd;

	err = macsock_getsockname(sID, (struct sockaddr *)addr, (int *)addrlen);
	if (err == -1) {
		macsock_map_error(errno);
	    return -1;
	 }
	 
	return err;
}	

// Errors: 
// EBADF - bad socket id
// ENOPROTOOPT - The option is unknown
PRStatus _MD_getsockopt(PRFileDesc *fd, PRInt32 level, PRInt32 optname, char* optval, PRInt32* optlen)
{
	int	err;
	int sID = fd->secret->md.osfd;

	err = macsock_getsockopt(sID, level, optname, optval, optlen);
	if (err == -1) {
		macsock_map_error(errno);
	    return PR_FAILURE;
	 }
	 
	return PR_SUCCESS;
}

// Errors:
// EBADF - bad socket id
// ENOTCONN - socket hasnÕt been properly created 
PRStatus _MD_setsockopt(PRFileDesc *fd, PRInt32 level, PRInt32 optname, const char* optval, PRInt32 optlen)
{
	int	err;
	int sID = fd->secret->md.osfd;

	err = macsock_setsockopt(sID, level, optname, optval, optlen);
	if (err == -1) {
		macsock_map_error(errno);
	    return PR_FAILURE;
	 }
	 
	return PR_SUCCESS;
}

PRInt32 _MD_socketavailable(PRFileDesc *fd, size_t *bytes)
{
	int	err;
	int sID = fd->secret->md.osfd;

	// Careful of the return value here.  0 => failure, 1 => success
	err = macsock_socketavailable(sID, bytes);
	if (err == 0) {
		_PR_MD_CURRENT_THREAD()->md.osErrCode = -1;
	    return -1;
	 }
	 
	return 0;
}

PRInt32 _MD_accept(PRFileDesc *fd, PRNetAddr *addr, PRUint32 *addrlen, PRIntervalTime timeout)
{
#pragma unused (timeout)

	int	err;
	int sID = fd->secret->md.osfd;

	err = macsock_accept(sID, (struct sockaddr *)addr, (int *)addrlen);
	if (err == -1) {
		macsock_map_error(errno);
	    return -1;
	 }
	 
	return err;
}

// Connect
// check the arguments validity
// issue the connect call to the stream
// Errors:
//	EBADF -- bad socket id, bad MacTCP stream
//	EAFNOSUPPORT -- bad address format
//	EADDRINUSE -- we are listening, or duplicate socket
//  EINPROGRESS -- we are connecting right now
//  EISCONN -- already connected
//  ECONNREFUSED -- other side has closed, or open has failed
//  EALREADY -- we are connected
//  EINTR -- user interrupted
PRInt32 _MD_connect(PRFileDesc *fd, PRNetAddr *addr, PRUint32 addrlen, PRIntervalTime timeout)
{
#pragma unused (timeout)

	int	err;
	int sID = fd->secret->md.osfd;

	err = macsock_connect(sID, (struct sockaddr *)addr, addrlen);
	if (err == -1) {
		macsock_map_error(errno);
	    return -1;
	 }
	 
	return err;
}

// Errors:
// EBADF - bad socket ID
// ENOTCONN - no such connection
PRInt32 _MD_recv(PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout)
{
#pragma unused (timeout)

	int	err;
	int sID = fd->secret->md.osfd;

	err = macsock_recv(sID, buf, amount, flags);
	if (err == -1) {
		macsock_map_error(errno);
	    return -1;
	 }
	 
	return err;
}

PRInt32 _MD_send(PRFileDesc *fd,const void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout)
{
#pragma unused (timeout)

	int	err;
	int sID = fd->secret->md.osfd;

	err = macsock_send(sID, buf, amount, flags);
	if (err == -1) {
		macsock_map_error(errno);
	    return -1;
	 }
	 
	return err;
}

PRInt32 _MD_recvfrom(PRFileDesc *fd, void *buf, PRInt32 amount, 
                               PRIntn flags, PRNetAddr *addr, PRUint32 *addrlen,
                               PRIntervalTime timeout)
{
#pragma unused (timeout)

	int	err;
	int sID = fd->secret->md.osfd;

	err = macsock_recvfrom(sID, buf, amount, flags, (struct sockaddr *)addr, (int *)addrlen);
	if (err == -1) {
		macsock_map_error(errno);
	    return -1;
	 }
	 
	return err;
}

                               
PRInt32 _MD_sendto(PRFileDesc *fd,const void *buf, PRInt32 amount, 
                               PRIntn flags, PRNetAddr *addr, PRUint32 addrlen,
                               PRIntervalTime timeout)
{
#pragma unused (timeout)

	int	err;
	int sID = fd->secret->md.osfd;

	err = macsock_sendto(sID, buf, amount, flags, (struct sockaddr *)addr, addrlen);
	if (err == -1) {
		macsock_map_error(errno);
	    return -1;
	 }
	 
	return err;
}

// Errors:
// EBADF -- bad socket id
PRInt32 _MD_closesocket(PRInt32 osfd)
{
	int	err;
	int sID = osfd;

	err = macsock_close(sID);
	if (err == -1) {
		macsock_map_error(errno);
	    return -1;
	 }
	 
	return err;
}

PRInt32 _MD_writev(PRFileDesc *fd, struct PRIOVec *iov, PRInt32 iov_size, PRIntervalTime timeout)
{
#pragma unused (fd, iov, iov_size, timeout)

	PR_ASSERT(0);
	_PR_MD_CURRENT_THREAD()->md.osErrCode = unimpErr;
    return -1;
}                               

PRInt32 _MD_shutdown(PRFileDesc *fd, PRIntn how)
{
	int	err;
	int sID = fd->secret->md.osfd;

	err = macsock_shutdown(sID, how);
	if (err == -1) {
		macsock_map_error(errno);
	    return -1;
	 }
	 
	return err;
}                               

PRStatus _MD_getpeername(PRFileDesc *fd, PRNetAddr *addr, PRUint32 *addrlen)
{
	int	err;
	int sID = fd->secret->md.osfd;

	err = macsock_getpeername(sID, (struct sockaddr *)addr, (int *)addrlen);
	if (err == -1) {
		macsock_map_error(errno);
	    return PR_FAILURE;
	 }
	 
	return PR_SUCCESS;
}

void _MD_makenonblock(PRFileDesc *fd)
{
	int	err;
	int sID = fd->secret->md.osfd;
	int optval =1;

	err = macsock_setsockopt(sID, SOL_SOCKET, FIONBIO, (const void *)&optval, 0);
	if (err == -1) {
		macsock_map_error(errno);
	 }
}                               

struct hostent *gethostbyname(const char * name)
{
	return macsock_gethostbyname((char *)name);
}

struct hostent *gethostbyaddr(const void *addr, int addrlen, int type)
{
	return macsock_gethostbyaddr(addr, addrlen, type);
}

PRStatus _MD_gethostname(char *name, int namelen)
{
	int	err;

	err = macsock_gethostname(name, namelen);
	if (err != noErr) {
		macsock_map_error(err);
	    return PR_FAILURE;
	 }
	 
	return PR_SUCCESS;
}

#define kIPName		"ip"
static struct protoent sIPProto = {kIPName, NULL, INET_IP};
static struct protoent sTCPProto = {kTCPName, NULL, INET_TCP};
static struct protoent sUDPProto = {kUDPName, NULL, INET_UDP};

struct protoent *getprotobyname(const char * name)
{
    if (strcmp(name, kIPName) == 0)
    	return (&sIPProto);
    	
    if (strcmp(name, kTCPName) == 0)
    	return (&sTCPProto);
    	
    if (strcmp(name, kUDPName) == 0)
    	return (&sUDPProto);
    	
ErrorExit:
	_PR_MD_CURRENT_THREAD()->md.osErrCode = paramErr;
    return NULL;
}


struct protoent *getprotobynumber(int number)
{
    if (number == INET_IP)
    	return (&sIPProto);
    	
    if (number == INET_TCP)
    	return (&sTCPProto);
    	
    if (number == INET_UDP)
    	return (&sUDPProto);
    	
ErrorExit:
	_PR_MD_CURRENT_THREAD()->md.osErrCode = paramErr;
    return NULL;
}

PRInt32 _MD_poll(PRPollDesc *pds, PRIntn npds, PRIntervalTime timeout)
{
    PRPollDesc *pd, *epd;
    PRInt32 n;

	fd_set rd, wt, ex;
	struct timeval tv, *tvp = NULL;
	int maxfd = -1;

	FD_ZERO(&rd);
	FD_ZERO(&wt);
	FD_ZERO(&ex);

    for (pd = pds, epd = pd + npds; pd < epd; pd++) {
        PRInt32 osfd;
        PRInt16 in_flags = pd->in_flags;
        PRFileDesc *bottom = pd->fd;

        if (NULL == bottom) {
            continue;
        }
        while (bottom->lower != NULL) {
            bottom = bottom->lower;
        }
        osfd = bottom->secret->md.osfd;

		if (osfd > maxfd) {
			maxfd = osfd;
		}
		if (in_flags & PR_POLL_READ)  {
			FD_SET(osfd, &rd);
		}
		if (in_flags & PR_POLL_WRITE) {
			FD_SET(osfd, &wt);
		}
		if (in_flags & PR_POLL_EXCEPT) {
			FD_SET(osfd, &ex);
		}
    }
	if (timeout != PR_INTERVAL_NO_TIMEOUT) {
		tv.tv_sec = PR_IntervalToSeconds(timeout);
		tv.tv_usec = PR_IntervalToMicroseconds(timeout) % PR_USEC_PER_SEC;
		tvp = &tv;
	}
	
	n = select(maxfd + 1, &rd, &wt, &ex, tvp);
	
	if (n > 0) {
		n = 0;
		for (pd = pds, epd = pd + npds; pd < epd; pd++) {
			PRInt32 osfd;
			PRInt16 in_flags = pd->in_flags;
			PRInt16 out_flags = 0;
			PRFileDesc *bottom = pd->fd;

			if (NULL == bottom) {
				continue;
			}
			while (bottom->lower != NULL) {
				bottom = bottom->lower;
			}
			osfd = bottom->secret->md.osfd;

			if ((in_flags & PR_POLL_READ) && FD_ISSET(osfd, &rd))  {
				out_flags |= PR_POLL_READ;
			}
			if ((in_flags & PR_POLL_WRITE) && FD_ISSET(osfd, &wt)) {
				out_flags |= PR_POLL_WRITE;
			}
			if ((in_flags & PR_POLL_EXCEPT) && FD_ISSET(osfd, &ex)) {
				out_flags |= PR_POLL_EXCEPT;
			}
			pd->out_flags = out_flags;
			if (out_flags) {
				n++;
			}
		}
		/* 
		Can't do this assert because MacSock returns write fds even if we did not
		set it in the original write fd set.
		*/
		/*PR_ASSERT(n > 0);*/
	} else 
		PR_ASSERT(n == 0);

	return n;
}

int _MD_mac_get_nonblocking_connect_error(int osfd)
{
	int	err;

	err = macsock_getconnectstatus (osfd);
	if (err == -1) {
		macsock_map_error(errno);
	    return -1;
	 }
	 	 
	return err;
}

