/*
 * Copyright (c) 1982, 1985, 1986, 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * [¤3 Deleted as of 22Jul99, see
 *     ftp://ftp.cs.berkeley.edu/pub/4bsd/README.Impt.License.Change
 *	   for details]
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)socket.h	8.4 (Berkeley) 2/21/94
 */

/* Adapted for GUSI by Matthias Neeracher <neeri@iis.ee.ethz.ch> */

#ifndef _SYS_SOCKET_H_
#define	_SYS_SOCKET_H_

/*
 * Definitions related to sockets: types, address families, options.
 */

/*
 * Types
 */
#define	SOCK_STREAM	1		/* stream socket */
#define	SOCK_DGRAM	2		/* datagram socket */
#define	SOCK_RAW	3		/* raw-protocol interface */
#define	SOCK_RDM	4		/* reliably-delivered message */
#define	SOCK_SEQPACKET	5		/* sequenced packet stream */

/*
 * Option flags per-socket.
 */
#define	SO_DEBUG	0x0001		/* turn on debugging info recording */
#define	SO_ACCEPTCONN	0x0002		/* socket has had listen() */
#define	SO_REUSEADDR	0x0004		/* allow local address reuse */
#define	SO_KEEPALIVE	0x0008		/* keep connections alive */
#define	SO_DONTROUTE	0x0010		/* just use interface addresses */
#define	SO_BROADCAST	0x0020		/* permit sending of broadcast msgs */
#define	SO_USELOOPBACK	0x0040		/* bypass hardware when possible */
#define	SO_LINGER	0x0080		/* linger on close if data present */
#define	SO_OOBINLINE	0x0100		/* leave received OOB data in line */
#define	SO_REUSEPORT	0x0200		/* allow local address & port reuse */

/*
 * Additional options, not kept in so_options.
 */
#define SO_SNDBUF	0x1001		/* send buffer size */
#define SO_RCVBUF	0x1002		/* receive buffer size */
#define SO_SNDLOWAT	0x1003		/* send low-water mark */
#define SO_RCVLOWAT	0x1004		/* receive low-water mark */
#define SO_SNDTIMEO	0x1005		/* send timeout */
#define SO_RCVTIMEO	0x1006		/* receive timeout */
#define	SO_ERROR	0x1007		/* get error status and clear */
#define	SO_TYPE		0x1008		/* get socket type */

/* Mandated by XNS -- neeri */
typedef unsigned	socklen_t;

/*
 * Structure used for manipulating linger option.
 */
struct	linger {
	int	l_onoff;		/* option on/off */
	int	l_linger;		/* linger time */
};

/*
 * Level number for (get/set)sockopt() to apply to socket itself.
 */
#define	SOL_SOCKET	0xffff		/* options for socket level */

/*
 * Address families.
 */
#define	AF_UNSPEC	0			/* unspecified */
#define	AF_LOCAL		1			/* local to host (pipes, portals) */
#define	AF_UNIX		AF_LOCAL	/* backward compatibility */
#define	AF_INET		2			/* internetwork: UDP, TCP, etc. */
#define	AF_PPC		3			/* PPC Toolbox										*/
#define	AF_PAP		4			/* Printer Access Protocol 	*/
#define	AF_APPLETALK	16		/* Apple Talk */

#define	ATALK_SYMADDR 	272		/* Symbolic Address for AppleTalk 			*/

#define	AF_MAX		20

#define	PF_UNSPEC	0			/* unspecified */
#define	PF_LOCAL		1			/* local to host (pipes, portals) */
#define	PF_UNIX		AF_LOCAL	/* backward compatibility */
#define	PF_INET		2			/* internetwork: UDP, TCP, etc. */
#define	PF_PPC		3			/* PPC Toolbox										*/
#define	PF_PAP		4			/* Printer Access Protocol 	*/
#define	PF_APPLETALK	16		/* Apple Talk */

/* Mandated by XNS -- neeri */
#define SHUT_RD	0
#define SHUT_WR	1
#define SHUT_RDWR	2

#ifndef _SA_FAMILY_T_DEFINED
#define _SA_FAMILY_T_DEFINED
typedef unsigned short sa_family_t;
#endif

/*
 * Structure used by kernel to store most
 * addresses.
 */
struct sockaddr {
	sa_family_t	sa_family;		/* address family */
	char			sa_data[14];	/* actually longer; address value */
};

/*
 * Definitions for network related sysctl, CTL_NET.
 *
 * Second level is protocol family.
 * Third level is protocol number.
 *
 * Further levels are defined by the individual families below.
 */
#define NET_MAXID	AF_MAX

#define CTL_NET_NAMES { \
	{ 0, 0 }, \
	{ "unix", CTLTYPE_NODE }, \
	{ "inet", CTLTYPE_NODE }, \
	{ "ppc", CTLTYPE_NODE }, \
	{ "pap", CTLTYPE_NODE }, \
	{ "", CTLTYPE_NODE }, \
	{ "", CTLTYPE_NODE }, \
	{ "", CTLTYPE_NODE }, \
	{ "", CTLTYPE_NODE }, \
	{ "", CTLTYPE_NODE }, \
	{ "", CTLTYPE_NODE }, \
	{ "", CTLTYPE_NODE }, \
	{ "", CTLTYPE_NODE }, \
	{ "", CTLTYPE_NODE }, \
	{ "", CTLTYPE_NODE }, \
	{ "", CTLTYPE_NODE }, \
	{ "appletalk", CTLTYPE_NODE }, \
}

/*
 * PF_ROUTE - Routing table
 *
 * Three additional levels are defined:
 *	Fourth: address family, 0 is wildcard
 *	Fifth: type of info, defined below
 *	Sixth: flag(s) to mask with for NET_RT_FLAGS
 */
#define NET_RT_DUMP	1		/* dump; may limit to a.f. */
#define NET_RT_FLAGS	2		/* by flags, e.g. RESOLVING */
#define NET_RT_IFLIST	3		/* survey interface list */
#define	NET_RT_MAXID	4

#define CTL_NET_RT_NAMES { \
	{ 0, 0 }, \
	{ "dump", CTLTYPE_STRUCT }, \
	{ "flags", CTLTYPE_STRUCT }, \
	{ "iflist", CTLTYPE_STRUCT }, \
}

/*
 * Maximum queue length specifiable by listen.
 */
#define	SOMAXCONN	5

/*
 * Message header for recvmsg and sendmsg calls.
 * Used value-result for recvmsg, value only for sendmsg.
 */
struct msghdr {
	void *		msg_name;			/* optional address */
	socklen_t	msg_namelen;		/* size of address */
	struct iovec *msg_iov;			/* scatter/gather array */
	int			msg_iovlen;			/* # elements in msg_iov */
	void *		msg_control;		/* ancillary data, see below */
	socklen_t	msg_controllen;	/* ancillary data buffer len */
	int			msg_flags;			/* flags on received message */
};

#define	MSG_OOB			0x1		/* process out-of-band data */
#define	MSG_PEEK			0x2			/* peek at incoming message */
#define	MSG_DONTROUTE	0x4		/* send without using routing tables */
#define	MSG_EOR			0x8		/* data completes record */
#define	MSG_TRUNC		0x10		/* data discarded before delivery */
#define	MSG_CTRUNC		0x20		/* control data lost before delivery */
#define	MSG_WAITALL		0x40		/* wait for full request or error */
#define	MSG_DONTWAIT	0x80		/* this message should be nonblocking */

/*
 * Header for ancillary data objects in msg_control buffer.
 * Used for additional information with/about a datagram
 * not expressible by flags.  The format is a sequence
 * of message elements headed by cmsghdr structures.
 */
struct cmsghdr {
	socklen_t	cmsg_len;		/* data byte count, including hdr */
	int			cmsg_level;		/* originating protocol */
	int			cmsg_type;		/* protocol-specific type */
/* followed by	u_char  cmsg_data[]; */
};

/* given pointer to struct cmsghdr, return pointer to data */
#define	CMSG_DATA(cmsg)		((u_char *)((cmsg) + 1))

/* given pointer to struct cmsghdr, return pointer to next cmsghdr */
#define	CMSG_NXTHDR(mhdr, cmsg)	\
	(((caddr_t)(cmsg) + (cmsg)->cmsg_len + sizeof(struct cmsghdr) > \
	    (mhdr)->msg_control + (mhdr)->msg_controllen) ? \
	    (struct cmsghdr *)NULL : \
	    (struct cmsghdr *)((caddr_t)(cmsg) + ALIGN((cmsg)->cmsg_len)))

#define	CMSG_FIRSTHDR(mhdr)	((struct cmsghdr *)(mhdr)->msg_control)

/* "Socket"-level control message types: */
#define	SCM_RIGHTS	0x01		/* access rights (array of int) */

#include <sys/cdefs.h>

__BEGIN_DECLS
int	accept __P((int, struct sockaddr *, socklen_t *));
int	bind __P((int, const struct sockaddr *, socklen_t));
int	connect __P((int, const struct sockaddr *, socklen_t));
int	getpeername __P((int, struct sockaddr *, socklen_t *));
int	getsockname __P((int, struct sockaddr *, socklen_t *));
int	getsockopt __P((int, int, int, void *, socklen_t *));
int	listen __P((int, int));
ssize_t	recv __P((int, void *, size_t, int));
ssize_t	recvfrom __P((int, void *, size_t, int, struct sockaddr *, socklen_t *));
ssize_t	recvmsg __P((int, struct msghdr *, int));
ssize_t	send __P((int, const void *, size_t, int));
ssize_t	sendto __P((int, const void *,
	    size_t, int, const struct sockaddr *, socklen_t));
ssize_t	sendmsg __P((int, const struct msghdr *, int));
int	setsockopt __P((int, int, int, const void *, socklen_t));
int	shutdown __P((int, int));
int	socket __P((int, int, int));
int	socketpair __P((int, int, int, int *));
__END_DECLS

#endif /* !_SYS_SOCKET_H_ */
