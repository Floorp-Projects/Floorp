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

#ifndef macksocket_h___
#define macksocket_h___

// macsock.h
// Interface visible to xp code
// C socket type definitions and routines
// from sys/socket.h
#include <OpenTptInternet.h>	// All the internet typedefs
#include <utime.h>				// For timeval
/*
 * sleep and delay conflict with the same in unistd.h from Metrowerks.  OT
 * defines them as 
 *
 *    extern pascal void		OTDelay(UInt32 seconds);
 *    extern pascal void		OTIdle(void);
 *
 *    #define sleep(x)	OTDelay(x)
 *    #define delay(x)	OTDelay(x)
 */

#undef sleep
#undef delay

#pragma once

#include "prio.h"

struct sockaddr {
	unsigned char	sa_len;			/* total length */
	unsigned char	sa_family;		/* address family */
	char	sa_data[14];		/* actually longer; address value */
};

// from netinet/in.h
struct in_addr {
	unsigned long s_addr;
};

struct sockaddr_in {
	unsigned char	sin_len;
	unsigned char	sin_family;		// AF_INET
	unsigned short	sin_port;
	struct	in_addr sin_addr;
	char		sin_zero[8];
};

struct	hostent {
	char	*h_name;	/* official name of host */
	char	**h_aliases;	/* alias list */
	int	h_addrtype;	/* host address type */
	int	h_length;	/* length of address */
	char	**h_addr_list;	/* list of addresses from name server */
#define	h_addr	h_addr_list[0]	/* address, for backward compatiblity */
};

// Necessary network defines, found by grepping unix headers when XP code would not compile
#define FIONBIO 1
#define SOCK_STREAM 1
#define SOCK_DGRAM 		2
#define IPPROTO_TCP 	INET_TCP		// Default TCP protocol
#define IPPROTO_UDP		INET_UDP		// Default UDP protocol
#define INADDR_ANY		kOTAnyInetAddress
#define SOL_SOCKET		XTI_GENERIC		// Any type of socket
#define SO_REUSEADDR	IP_REUSEADDR
#define MSG_PEEK		0x2				// Just look at a message waiting, donÕt actually read it.

typedef unsigned long u_long;

/* ldap.h has its own definition of fd_set */
/* select support */
#if !defined(FD_SET)
#define	NBBY    8
typedef long    fd_mask;
#define NFDBITS (sizeof(fd_mask) * NBBY)	/* bits per mask */

#ifndef howmany
#define howmany(x, y)   (((x)+((y)-1))/(y))
#endif
#define FD_SETSIZE 64
typedef	struct fd_set{
    fd_mask fds_bits[howmany(FD_SETSIZE, NFDBITS)];
} fd_set;

#define	FD_SET(n, p)    ((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define	FD_CLR(n, p)    ((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define	FD_ISSET(n, p)  ((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define	FD_ZERO(p)      memset (p, 0, sizeof(*(p)))
#endif /* !FD_SET */


#ifdef __cplusplus
extern "C" {
#endif

extern unsigned long inet_addr(const char *cp);
extern char *inet_ntoa(struct in_addr in);

inline unsigned long htonl(unsigned long hostlong) {return hostlong;}
inline unsigned long ntohl(unsigned long netlong) {return netlong;}
inline unsigned short ntohs(unsigned short netshort) {return netshort;}
inline unsigned short htons(unsigned short hostshort) {return hostshort;}


// UNIX look-alike routines
// They make sure that the arguments passed in are valid, and then
//
extern struct hostent *macsock_gethostbyaddr(const void *addr, int addrlen, int type);

extern int macsock_socket(int domain, int type, int protocol);
extern int macsock_ioctl(int sID, unsigned int request, void *value);
extern int macsock_connect(int sID, struct sockaddr *name, int namelen);
extern int macsock_write(int sID, const void *buffer, unsigned buflen);
extern int macsock_read(int sID, void *buf, unsigned nbyte);
extern int macsock_close(int sID);

extern int macsock_accept(int sID, struct sockaddr *addr, int *addrlen);
extern int macsock_bind(int sID, const struct sockaddr *name, int namelen);
extern int macsock_listen(int sID, int backlog);

extern int macsock_shutdown(int sID, int how);
extern int macsock_getpeername(int sID, struct sockaddr *name, int *namelen);
extern int macsock_getsockname(int sID, struct sockaddr *name, int *namelen);
extern int macsock_getsockopt(int sID, int level, int optname, void *optval,int *optlen);
extern int macsock_setsockopt(int sID, int level, int optname, const void *optval,int optlen);
extern int macsock_socketavailable(int sID, size_t *bytesAvailable);
extern int macsock_dup(int sID);

extern int macsock_send(int sID, const void *msg, int len, int flags);
extern int macsock_sendto(int sID, const void *msg, int len, int flags, struct sockaddr *toAddr, int toLen);
extern int macsock_recvfrom(int sID, void *buf, int len, int flags, struct sockaddr *from, int *fromLen);
extern int macsock_recv(int sID, void *buf, int len, int flags);

extern int select (int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);


#define macsock_gethostbyaddr PR_GetHostByAddr
#define macsock_socket PR_Socket
#define macsock_connect PR_Connect
#define macsock_write PR_Write
#define macsock_read PR_Read
#define macsock_close PR_Close
#define macsock_accept PR_Accept
#define macsock_bind PR_Bind
#define macsock_listen PR_Listen
#define macsock_shutdown PR_Shutdown
#define macsock_getpeername PR_GetPeerName
#define macsock_getsockname PR_GetSockName
#define macsock_getsockopt PR_GetSockOpt
#define macsock_setsockopt PR_SetSockOpt
#define macsock_socketavailable PR_SocketAvailable
#define macsock_send PR_Send
#define macsock_sendto PR_SendTo
#define macsock_recvfrom PR_RecvFrom
#define macsock_recv PR_Recv

#ifdef __cplusplus
}
#endif
//extern int errno;

/*
macsock_sendmsg
macsock_readv
macsock_writev
*/

/* New definitions that are not defined in macsock.h in macsock library */
struct	protoent {
    char	*p_name;		/* official protocol name */
    char	**p_aliases;	/* alias list */
    int		p_proto;		/* protocol # */
};

extern struct protoent *getprotobyname(const char * name);
extern struct protoent *getprotobynumber(int number);

extern int gethostname (char *name, int namelen);
extern struct hostent *gethostbyname(const char * name);
extern struct hostent *gethostbyaddr(const void *addr, int addrlen, int type);

#define INADDR_LOOPBACK	0x7F000001

#define SO_KEEPALIVE	TCP_KEEPALIVE
#define SO_RCVBUF		XTI_RCVBUF
#define SO_SNDBUF		XTI_SNDBUF
#define SO_LINGER       XTI_LINGER          /* linger on close if data present */

#define IPPROTO_IP		INET_IP

/* Get/Set sock opt until fixed in NSPR 2.0 */
struct  linger {
        int     l_onoff;                /* option on/off */
        int     l_linger;               /* linger time */
};

struct ip_mreq {
        struct in_addr  imr_multiaddr;  /* IP multicast address of group */
        struct in_addr  imr_interface;  /* local IP address of interface */
};

#endif /* macksocket_h___ */
