/*
 * Copyright (c) 1982, 1986, 1990, 1993
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
 *	@(#)in.h	8.3 (Berkeley) 1/3/94
 */

/* Adapted for GUSI by Matthias Neeracher <neeri@iis.ee.ethz.ch> */

#ifndef _NETINET_IN_H_
#define _NETINET_IN_H_

/* xtonx() mandated available by XNS -- neeri */
#include <machine/endian.h>

/*
 * Constants and structures defined by the internet system,
 * Per RFC 790, September 1981, and numerous additions.
 */

/*
 * Protocols
 */
#define	IPPROTO_IP		0		/* dummy for IP */
#define	IPPROTO_ICMP	1		/* control message protocol */
#define	IPPROTO_IGMP	2		/* group mgmt protocol */
#define	IPPROTO_GGP		3		/* gateway^2 (deprecated) */
#define	IPPROTO_TCP		6		/* tcp */
#define	IPPROTO_EGP		8		/* exterior gateway protocol */
#define	IPPROTO_PUP		12		/* pup */
#define	IPPROTO_UDP		17		/* user datagram protocol */
#define	IPPROTO_IDP		22		/* xns idp */
#define	IPPROTO_TP		29 		/* tp-4 w/ class negotiation */
#define	IPPROTO_EON		80		/* ISO cnlp */
#define	IPPROTO_ENCAP	98		/* encapsulation header */

#define	IPPROTO_RAW		255		/* raw IP packet */
#define	IPPROTO_MAX		256


/*
 * Local port number conventions:
 * Ports < IPPORT_RESERVED are reserved for
 * privileged processes (e.g. root).
 * Ports > IPPORT_USERRESERVED are reserved
 * for servers, not necessarily privileged.
 */
#define	IPPORT_RESERVED		1024
#define	IPPORT_USERRESERVED	5000

/* Types mandated by XNS -- neeri */
typedef uint16_t	in_port_t;
typedef uint32_t	in_addr_t;

/*
 * Internet address (a structure for historical reasons)
 */
struct in_addr {
	in_addr_t s_addr;
};

/*
 * Definitions of bits in internet address integers.
 * On subnets, the decomposition of addresses to host and net parts
 * is done according to subnet mask, not the masks here.
 */
#define	IN_CLASSA(i)		(((long)(i) & 0x80000000) == 0)
#define	IN_CLASSA_NET		0xff000000
#define	IN_CLASSA_NSHIFT	24
#define	IN_CLASSA_HOST		0x00ffffff
#define	IN_CLASSA_MAX		128

#define	IN_CLASSB(i)		(((long)(i) & 0xc0000000) == 0x80000000)
#define	IN_CLASSB_NET		0xffff0000
#define	IN_CLASSB_NSHIFT	16
#define	IN_CLASSB_HOST		0x0000ffff
#define	IN_CLASSB_MAX		65536

#define	IN_CLASSC(i)		(((long)(i) & 0xe0000000) == 0xc0000000)
#define	IN_CLASSC_NET		0xffffff00
#define	IN_CLASSC_NSHIFT	8
#define	IN_CLASSC_HOST		0x000000ff

#define	IN_CLASSD(i)		(((long)(i) & 0xf0000000) == 0xe0000000)
#define	IN_CLASSD_NET		0xf0000000	/* These ones aren't really */
#define	IN_CLASSD_NSHIFT	28		/* net and host fields, but */
#define	IN_CLASSD_HOST		0x0fffffff	/* routing needn't know.    */
#define	IN_MULTICAST(i)		IN_CLASSD(i)

#define	IN_EXPERIMENTAL(i)	(((long)(i) & 0xf0000000) == 0xf0000000)
#define	IN_BADCLASS(i)		(((long)(i) & 0xf0000000) == 0xf0000000)

#define	INADDR_ANY		(u_long)0x00000000
#define	INADDR_BROADCAST	(u_long)0xffffffff	/* must be masked */
#define	INADDR_NONE		0xffffffff		/* -1 return */

#define	INADDR_UNSPEC_GROUP	(u_long)0xe0000000	/* 224.0.0.0 */
#define	INADDR_ALLHOSTS_GROUP	(u_long)0xe0000001	/* 224.0.0.1 */
#define	INADDR_MAX_LOCAL_GROUP	(u_long)0xe00000ff	/* 224.0.0.255 */

#define	IN_LOOPBACKNET		127			/* official! */

/* Mandated by XNS -- neeri */
#ifndef _SA_FAMILY_T_DEFINED
#define _SA_FAMILY_T_DEFINED
typedef uint16_t sa_family_t;
#endif

/*
 * Socket address, internet style.
 */
struct sockaddr_in {
	sa_family_t		sin_family;
	in_port_t		sin_port;
	struct			in_addr sin_addr;
	unsigned char	sin_zero[8];
};

/*
 * Structure used to describe IP options.
 * Used to store options internally, to pass them to a process,
 * or to restore options retrieved earlier.
 * The ip_dst is used for the first-hop gateway when using a source route
 * (this gets put into the header proper).
 */
typedef struct {
	struct	in_addr ip_dst;		/* first hop, 0 w/o src rt */
	char	ip_opts[40];		/* actually variable in size */
} ip_opts;

/*
 * Options for use with [gs]etsockopt at the IP level.
 * First word of comment is data type; bool is stored in int.
 */
#define	IP_OPTIONS			 0x01
#define	IP_TOS				 0x02
#define	IP_TTL				 0x03
#define	IP_REUSEADDR		 0x04
#define	IP_DONTROUTE		 0x10
#define	IP_BROADCAST		 0x20
#define	IP_HDRINCL			 0x1002	
#define 	IP_RCVOPTS			 0x1005
#define 	IP_RCVDSTADDR		 0x1007
#define	IP_MULTICAST_IF	 0x1010		/* set/get IP multicast interface	*/
#define	IP_MULTICAST_TTL	 0x1011		/* set/get IP multicast timetolive	*/
#define	IP_MULTICAST_LOOP	 0x1012		/* set/get IP multicast loopback	*/
#define	IP_ADD_MEMBERSHIP	 0x1013		/* add an IP group membership		*/
#define	IP_DROP_MEMBERSHIP 0x1014		/* drop an IP group membership		*/
#define	IP_BROADCAST_IF	 0x1015		/* Set interface for broadcasts 	*/
#define	IP_RCVIFADDR		 0x1016		/* Set interface for broadcasts 	*/

/*
 * Defaults and limits for options
 */
#define	IP_DEFAULT_MULTICAST_TTL  1	/* normally limit m'casts to 1 hop  */
#define	IP_DEFAULT_MULTICAST_LOOP 1	/* normally hear sends if a member  */
#define	IP_MAX_MEMBERSHIPS	20	/* per socket; must fit in one mbuf */

/*
 * Argument structure for IP_ADD_MEMBERSHIP and IP_DROP_MEMBERSHIP.
 */
struct ip_mreq {
	struct	in_addr imr_multiaddr;	/* IP multicast address of group */
	struct	in_addr imr_interface;	/* local IP address of interface */
};

/*
 * Definitions for inet sysctl operations.
 *
 * Third level is protocol number.
 * Fourth level is desired variable within that protocol.
 */
#define	IPPROTO_MAXID	(IPPROTO_IDP + 1)	/* don't list to IPPROTO_MAX */

#define	CTL_IPPROTO_NAMES { \
	{ "ip", CTLTYPE_NODE }, \
	{ "icmp", CTLTYPE_NODE }, \
	{ "igmp", CTLTYPE_NODE }, \
	{ "ggp", CTLTYPE_NODE }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ "tcp", CTLTYPE_NODE }, \
	{ 0, 0 }, \
	{ "egp", CTLTYPE_NODE }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ "pup", CTLTYPE_NODE }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ "udp", CTLTYPE_NODE }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ "idp", CTLTYPE_NODE }, \
}

/*
 * Names for IP sysctl objects
 */
#define	IPCTL_FORWARDING	1	/* act as router */
#define	IPCTL_SENDREDIRECTS	2	/* may send redirects when forwarding */
#define	IPCTL_DEFTTL		3	/* default TTL */
#ifdef notyet
#define	IPCTL_DEFMTU		4	/* default MTU */
#endif
#define	IPCTL_MAXID		5

#define	IPCTL_NAMES { \
	{ 0, 0 }, \
	{ "forwarding", CTLTYPE_INT }, \
	{ "redirect", CTLTYPE_INT }, \
	{ "ttl", CTLTYPE_INT }, \
	{ "mtu", CTLTYPE_INT }, \
}

#endif /* _NETINET_IN_H_ */

