/*
 * Implementation of Socks protocol.
 * None of this code is supported any longer.
 * NSS officially does NOT support Socks.
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 *
 * $Id: sslsocks.c,v 1.3 2000/09/19 06:05:28 wtc%netscape.com Exp $
 */
#include "prtypes.h"
#include "prnetdb.h"
#include "cert.h"
#include "ssl.h"
#include "sslimpl.h"
#include "prsystem.h"
#include <stdio.h>
#include "nspr.h"

#ifdef XP_UNIX
#include "prprf.h"
#endif

#ifdef XP_UNIX
#define SOCKS_FILE	"/etc/socks.conf"
#endif
#ifdef XP_MAC
#define SOCKS_FILE NULL
#endif
#ifdef XP_WIN
#define SOCKS_FILE  NULL
#endif
#ifdef XP_OS2
#define SOCKS_FILE  NULL
#endif 

#define SOCKS_VERSION	4

#define DEF_SOCKD_PORT	1080

#define SOCKS_CONNECT	1
#define SOCKS_BIND	2

#define SOCKS_RESULT	90
#define SOCKS_FAIL	91
#define SOCKS_NO_IDENTD	92 /* Failed to connect to Identd on client machine */
#define SOCKS_BAD_ID	93 /* Client's Identd reported a different user-id */

#define MAKE_IN_ADDR(a,b,c,d) \
    PR_htonl(((PRUint32)(a) << 24) | ((PRUint32)(b) << 16) | ((c) << 8) | (d))

struct sslSocksInfoStr {
    PRUint32  	sockdHost;
    PRUint16  	sockdPort;

    char       	direct;
    char       	didBind;

    PRNetAddr 	bindAddr;

    /* Data returned by sockd.  */
    PRUint32  	destHost;
    PRUint16  	destPort;
};

typedef enum {
    OP_LESS	= 1,
    OP_EQUAL	= 2,
    OP_LEQUAL	= 3,
    OP_GREATER	= 4,
    OP_NOTEQUAL	= 5,
    OP_GEQUAL	= 6,
    OP_ALWAYS	= 7
} SocksOp;

typedef struct SocksConfItemStr SocksConfItem;

struct SocksConfItemStr {
    SocksConfItem *next;
    PRUint32       daddr;	/* host IP addr, in network byte order. */
    PRUint32       dmask;	/* mask for IP,  in network byte order. */
    PRUint16       port;	/* port number,  in host    byte order. */
    SocksOp        op;
    char           direct;
};

static PRUint32 ourHost;		/* network byte order. */
static SocksConfItem *ssl_socks_confs;

SECStatus
ssl_CreateSocksInfo(sslSocket *ss)
{
    sslSocksInfo *si;

    if (ss->socks) {
	/* Already been done */
	return SECSuccess;
    }

    si = (sslSocksInfo*) PORT_ZAlloc(sizeof(sslSocksInfo));
    if (si) {
	ss->socks = si;
	if (!ss->gather) {
	    ss->gather = ssl_NewGather();
	    if (!ss->gather) {
		return SECFailure;
	    }
	}
	return SECSuccess;
    }
    return SECFailure;
}

SECStatus
ssl_CopySocksInfo(sslSocket *ss, sslSocket *os)
{
    SECStatus rv;

#ifdef __cplusplus
    os = os;
#endif
    rv = ssl_CreateSocksInfo(ss);
    return rv;
}

void
ssl_DestroySocksInfo(sslSocksInfo *si)
{
    if (si) {
	PORT_Memset(si, 0x2f, sizeof *si);
	PORT_Free(si);
    }
}

/* Sets the global variable ourHost to the IP address returned from 
 *    calling GetHostByName on our system's name.
 * Called from SSL_ReadSocksConfFile().
 */
static SECStatus
GetOurHost(void)
{
    PRStatus   rv;
    PRHostEnt  hpbuf;
    char       name[100];
    char       dbbuf[PR_NETDB_BUF_SIZE];

    PR_GetSystemInfo(PR_SI_HOSTNAME, name, sizeof name);

    rv = PR_GetHostByName(name, dbbuf, sizeof dbbuf, &hpbuf);
    if (rv != PR_SUCCESS)
	return SECFailure;

#undef  h_addr
#define h_addr  h_addr_list[0]  /* address, in network byte order. */

    PORT_Memcpy(&ourHost, hpbuf.h_addr, hpbuf.h_length);
    return SECSuccess;
}

/*
** Setup default SocksConfItem list so that loopback is direct, things to the
** same subnet (?) address are direct, everything else uses sockd
*/
static void
BuildDefaultConfList(void)
{
    SocksConfItem *ci;
    SocksConfItem **lp;

    /* Put loopback onto direct list */
    lp = &ssl_socks_confs;
    ci = (SocksConfItem*) PORT_ZAlloc(sizeof(SocksConfItem));
    if (!ci) {
	return;
    }
    ci->direct = 1;
    ci->daddr  = MAKE_IN_ADDR(127,0,0,1);
    ci->dmask  = MAKE_IN_ADDR(255,255,255,255);
    ci->op     = OP_ALWAYS;
    *lp = ci;
    lp = &ci->next;

    /* Put our hosts's subnet onto direct list */
    ci = (SocksConfItem*) PORT_ZAlloc(sizeof(SocksConfItem));
    if (!ci) {
	return;
    }
    ci->direct = 1;
    ci->daddr  = ourHost;
    ci->dmask  = MAKE_IN_ADDR(255,255,255,0);
    ci->op     = OP_ALWAYS;
    *lp = ci;
    lp = &ci->next;

    /* Everything else goes to sockd */
    ci = (SocksConfItem*) PORT_ZAlloc(sizeof(SocksConfItem));
    if (!ci) {
	return;
    }
    ci->daddr = MAKE_IN_ADDR(255,255,255,255);
    ci->op    = OP_ALWAYS;
    *lp = ci;
}

static int
FragmentLine(char *cp, char **argv, int maxargc)
{
    int argc = 0;
    char *save;
    char ch;

    save = cp;
    for (; (ch = *cp) != 0; cp++) {
	if ((ch == '#') || (ch == '\n')) {
	    /* Done */
	    break;
	}
	if (ch == ':') {
	    break;
	}
	if ((ch == ' ') || (ch == '\t')) {
	    /* Seperator. see if it seperated anything */
	    if (cp - save > 0) {
		/* Put a null at the end of the word */
		*cp = 0;
		argc++;
		*argv++ = save;
		SSL_TRC(20, ("%d: SSL: argc=%d word=\"%s\"",
			     SSL_GETPID(), argc, save));
		if (argc == maxargc) {
		    return argc;
		}
	    }
	    save = cp + 1;
	}
    }
    if (cp - save > 0) {
	*cp = 0;
	argc++;
	*argv = save;
	SSL_TRC(20, ("%d: SSL: argc=%d word=\"%s\"",
		     SSL_GETPID(), argc, save));
    }
    return argc;
}

/* XXX inet_addr? */
static char *
ConvertOne(char *cp, unsigned char *rvp)
{
    char *s = PORT_Strchr(cp, '.');
    if (s) {
	*s = 0;
    }
    *rvp = PORT_Atoi(cp) & 0xff;
    return s ? s+1 : cp;
}

/* returns host address in network byte order. */
static PRUint32
ConvertAddr(char *buf)
{
    unsigned char b0, b1, b2, b3;
    PRUint32 addr;

    buf = ConvertOne(buf, &b0);
    buf = ConvertOne(buf, &b1);
    buf = ConvertOne(buf, &b2);
    buf = ConvertOne(buf, &b3);
    addr = ((PRUint32)b0 << 24) |
	   ((PRUint32)b1 << 16) |
	   ((PRUint32)b2 << 8) |
	    (PRUint32)b3;		/* host byte order. */

    return PR_htonl(addr);		/* network byte order. */
}

static char *
ReadLine(char *buf, int len, PRFileDesc *fd)
{
    char c, *p = buf;
    PRInt32 n;

    while(len > 0) {
	n = PR_Read(fd, &c, 1);
	if (n < 0)
	    return NULL;
	if (n == 0) {
	    if (p == buf) {
		return NULL;
	    }
	    *p = '\0';
	    return buf;
	}
	if (c == '\n') {
	    *p = '\0';
	    return buf;
	}
	*p++ = c;
	len--;
    }
    *p = '\0';
    return buf;
}

int
SSL_ReadSocksConfFile(PRFileDesc *fp)
{
    SocksConfItem * ci;
    SocksConfItem **lp;
    char *          file       = "socks file"; /* XXX Move to nav */
    SocksOp         op;
    int             direct;
    int             port       = 0;
    int             lineNumber = 0;
    int             rv         = GetOurHost();

    if (rv < 0) {
	/* If we can't figure out our host id, use socks. Loser! */
	return SECFailure;
    }

#if 0 /* XXX Move to nav */
    fp = XP_FileOpen(file, xpSocksConfig, XP_FILE_READ);
#endif
    if (!fp) {
	BuildDefaultConfList();
	return SECSuccess;
    }

    /* Parse config file and generate config item list */
    lp = &ssl_socks_confs;
    for (;;) {
	char *    s;
	char *    argv[10];
	int       argc;
	PRUint32  daddr;
	PRUint32  dmask;
	char      buf[1000];

	s = ReadLine(buf, sizeof buf, fp);
	if (!s) {
	    break;
	}
	lineNumber++;
	argc = FragmentLine(buf, argv, 10);
	if (argc < 3) {
	    if (argc == 0) {
		/* must be a comment/empty line */
		continue;
	    }
#ifdef XP_UNIX
	    PR_fprintf(PR_STDERR, "%s:%d: bad config line\n",
		       file, lineNumber);
#endif
	    continue;
	}
	if (PORT_Strcmp(argv[0], "direct") == 0) {
	    direct = 1;
	} else if (PORT_Strcmp(argv[0], "sockd") == 0) {
	    direct = 0;
	} else {
#ifdef XP_UNIX
	    PR_fprintf(PR_STDERR, "%s:%d: bad command: \"%s\"\n",
		       file, lineNumber, argv[0]);
#endif
	    continue;
	}

	/* Look for port spec */
	op = OP_ALWAYS;
	if (argc > 4) {
	    if (PORT_Strcmp(argv[3], "lt") == 0) {
		op = OP_LESS;
	    } else if (PORT_Strcmp(argv[3], "eq") == 0) {
		op = OP_EQUAL;
	    } else if (PORT_Strcmp(argv[3], "le") == 0) {
		op = OP_LEQUAL;
	    } else if (PORT_Strcmp(argv[3], "gt") == 0) {
		op = OP_GREATER;
	    } else if (PORT_Strcmp(argv[3], "neq") == 0) {
		op = OP_NOTEQUAL;
	    } else if (PORT_Strcmp(argv[3], "ge") == 0) {
		op = OP_GEQUAL;
	    } else {
#ifdef XP_UNIX
		PR_fprintf(PR_STDERR, "%s:%d: bad comparison op: \"%s\"\n",
			   file, lineNumber, argv[3]);
#endif
		continue;
	    }
	    port = PORT_Atoi(argv[4]);
	}

	ci = (SocksConfItem*) PORT_ZAlloc(sizeof(SocksConfItem));
	if (!ci) {
	    break;
	}
	daddr      = ConvertAddr(argv[1]);	/* net byte order. */
	dmask      = ConvertAddr(argv[2]);	/* net byte order. */
	ci->daddr  = daddr;			/* net byte order. */
	ci->dmask  = dmask;			/* net byte order. */
	ci->direct = direct;
	ci->op     = op;
	ci->port   = port;			/* host byte order. */
	daddr      = PR_ntohl(daddr);		/* host byte order. */
	dmask      = PR_ntohl(dmask);		/* host byte order. */
	SSL_TRC(10, (
"%d: SSL: line=%d direct=%d addr=%d.%d.%d.%d mask=%d.%d.%d.%d op=%d port=%d",
		     SSL_GETPID(), lineNumber, ci->direct,
		     (daddr >> 24) & 0xff,
		     (daddr >> 16) & 0xff,
		     (daddr >>  8) & 0xff,
		     (daddr >>  0) & 0xff,
		     (dmask >> 24) & 0xff,
		     (dmask >> 16) & 0xff,
		     (dmask >>  8) & 0xff,
		     (dmask >>  0) & 0xff,
		     ci->op, ci->port));
	*lp = ci;
	lp = &ci->next;
    }


    if (!ssl_socks_confs) {
	/* Empty file. Fix it for the user */
	BuildDefaultConfList();
    }
    return SECSuccess;
}

static int
ChooseAddress(sslSocket *ss, const PRNetAddr *direct)
{
    PRUint32 dstAddr;
    PRUint16 dstPort;
    SocksConfItem *ci;
    int rv;

    if (!ssl_socks_confs) {
	rv = SSL_ReadSocksConfFile(NULL);
	if (rv) {
	    return rv;
	}
    }

    /*
    ** Scan socks config info and look for a direct match or a force to
    ** use the sockd. Bail on first hit.
    */
    dstAddr = direct->inet.ip;
    dstPort = PR_ntohs(direct->inet.port);
    ci = ssl_socks_confs;
    while (ci) {
	SSL_TRC(10, (
	"%d: SSL[%d]: match, direct=%d daddr=0x%x mask=0x%x op=%d port=%d",
		 SSL_GETPID(), ss->fd, ci->direct, PR_ntohl(ci->daddr),
		 PR_ntohl(ci->dmask), ci->op, ci->port));
	if ((ci->daddr & ci->dmask)  == (dstAddr & ci->dmask)) {
	    int portMatch = 0;
	    switch (ci->op) {
	      case OP_LESS:	portMatch = dstPort <  ci->port; break;
	      case OP_EQUAL:	portMatch = dstPort == ci->port; break;
	      case OP_LEQUAL:	portMatch = dstPort <= ci->port; break;
	      case OP_GREATER:	portMatch = dstPort >  ci->port; break;
	      case OP_NOTEQUAL:	portMatch = dstPort != ci->port; break;
	      case OP_GEQUAL:	portMatch = dstPort >= ci->port; break;
	      case OP_ALWAYS:	portMatch = 1;                   break;
	    }
	    if (portMatch) {
		SSL_TRC(10, ("%d: SSL[%d]: socks config match",
			     SSL_GETPID(), ss->fd));
		return ci->direct;
	    }
	}
	ci = ci->next;
    }
    SSL_TRC(10, ("%d: SSL[%d]: socks config: no match",
		 SSL_GETPID(), ss->fd));
    return 0;
}

/*
** Find port # and host # of socks daemon. Use info in ss->socks struct
** when valid. If not valid, try to figure it all out.
*/
static int
FindDaemon(sslSocket *ss, PRNetAddr *out)
{
    sslSocksInfo *si;
    PRUint32 host;		/* network byte order. */
    PRUint16 port;		/* host    byte order. */

    PORT_Assert(ss->socks != 0);
    si = ss->socks;

    /* For now, assume we are using the socks daemon */
    host = si->sockdHost;
    port = si->sockdPort;
#ifdef XP_UNIX
    if (!port) {
	static char firstTime = 1;
	static PRUint16 sockdPort;

	if (firstTime) {
	    struct servent *sp;

	    firstTime = 0;
	    sp = getservbyname("socks", "tcp");
	    if (sp) {
		sockdPort = sp->s_port;
	    } else {
		SSL_TRC(10, ("%d: SSL[%d]: getservbyname of (socks,tcp) fails",
			     SSL_GETPID(), ss->fd));
	    }
	}
	port = sockdPort;
    }
#endif
    if (!port) {
	port = DEF_SOCKD_PORT;
    }
    if (host == 0) {
	SSL_TRC(10, ("%d: SSL[%d]: no socks server found",
		     SSL_GETPID(), ss->fd));
	PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
	return SECFailure;
    }

    /* We know the ip addr of the socks server */
    out->inet.family = PR_AF_INET;
    out->inet.port   = PR_htons(port);
    out->inet.ip     = host;
    host             = PR_ntohl(host);	/* now host byte order. */
    SSL_TRC(10, ("%d: SSL[%d]: socks server at %d.%d.%d.%d:%d",
		 SSL_GETPID(), ss->fd,
		 (host >> 24) & 0xff,
		 (host >> 16) & 0xff,
		 (host >>  8) & 0xff,
		 (host >>  0) & 0xff,
		 port));
    return SECSuccess;
}

/*
** Send our desired address and our user name to the socks daemon.
** cmd is either SOCKS_CONNECT (client) or SOCKS_BIND (server).
*/
static int
SayHello(sslSocket *ss, int cmd, const PRNetAddr *sa, char *user)
{
    int rv, len;
    unsigned char msg[8];
    PRUint16 port;
    PRUint32 host;

    /* Send dst message to sockd */
    port = sa->inet.port;
    host = sa->inet.ip;
    msg[0] = SOCKS_VERSION;
    msg[1] = cmd;
    PORT_Memcpy(msg+2, &port, 2);
    PORT_Memcpy(msg+4, &host, 4);
    SSL_TRC(10, ("%d: SSL[%d]: socks real dest=%d.%d.%d.%d:%d",
		 SSL_GETPID(), ss->fd, msg[4], msg[5], msg[6], msg[7],
		 port));

    rv = ssl_DefSend(ss, msg, sizeof(msg), 0);
    if (rv < 0) {
	goto io_error;
    }
    /* XXX Deal with short write !! */

    /* Send src-user message to sockd */
    len = strlen(user)+1;
    rv = ssl_DefSend(ss, (unsigned char *)user, len, 0);
    if (rv < 0) {
	goto io_error;
    }
    /* XXX Deal with short write !! */

    return SECSuccess;

  io_error:
    SSL_TRC(10, ("%d: SSL[%d]: socks, io error saying hello to sockd errno=%d",
		 SSL_GETPID(), ss->fd, PORT_GetError()));
    return SECFailure;
}

/* Handle the reply from the socks proxy/daemon. 
** Called from ssl_Do1stHandshake().
*/
static SECStatus
SocksHandleReply(sslSocket *ss)
{
    unsigned char *msg;
    unsigned char  cmd;

    PORT_Assert( ssl_Have1stHandshakeLock(ss) );

    ssl_GetRecvBufLock(ss);
    PORT_Assert(ss->gather != 0);

    msg = ss->gather->buf.buf;
    cmd = msg[1];
    SSL_TRC(10, ("%d: SSL[%d]: socks result: cmd=%d",
		 SSL_GETPID(), ss->fd, cmd));

    /* This is Bogus.  The socks spec says these fields are undefined in 
     * the reply from the socks daemon/proxy.  No point in saving garbage.
     */
    PORT_Memcpy(&ss->socks->destPort, msg+2, 2);
    PORT_Memcpy(&ss->socks->destHost, msg+4, 4);

    ss->gather->recordLen = 0;
    ssl_ReleaseRecvBufLock(ss);

    /* Check status back from sockd */
    switch (cmd) {
      case SOCKS_FAIL:
      case SOCKS_NO_IDENTD:
      case SOCKS_BAD_ID:
	SSL_DBG(("%d: SSL[%d]: sockd returns an error: %d",
		 SSL_GETPID(), ss->fd, cmd));
	PORT_SetError(PR_CONNECT_REFUSED_ERROR);
	return SECFailure;

      default:
	break;
    }

    /* All done */
    SSL_TRC(1, ("%d: SSL[%d]: using sockd at %d.%d.%d.%d",
		SSL_GETPID(), ss->fd,
		(PR_ntohl(ss->socks->sockdHost) >> 24) & 0xff,
		(PR_ntohl(ss->socks->sockdHost) >> 16) & 0xff,
		(PR_ntohl(ss->socks->sockdHost) >> 8) & 0xff,
		(PR_ntohl(ss->socks->sockdHost) >> 0) & 0xff));
    ss->handshake         = 0;
    ss->nextHandshake     = 0;
    return SECSuccess;
}

static SECStatus
SocksGatherRecord(sslSocket *ss)
{
    int rv;

    PORT_Assert( ssl_Have1stHandshakeLock(ss) );
    ssl_GetRecvBufLock(ss);
    rv = ssl2_GatherRecord(ss, 0);
    ssl_ReleaseRecvBufLock(ss);
    if (rv <= 0) {
	if (rv == 0)
	    /* Unexpected EOF */
	    PORT_SetError(PR_END_OF_FILE_ERROR);
	return SECFailure;
    }
    ss->handshake = 0;
    return SECSuccess;
}

static SECStatus
SocksStartGather(sslSocket *ss)
{
    int rv;

    ss->handshake     = SocksGatherRecord;
    ss->nextHandshake = SocksHandleReply;
    rv = ssl2_StartGatherBytes(ss, ss->gather, 8);
    if (rv <= 0) {
	if (rv == 0) {
	    /* Unexpected EOF */
	    PORT_SetError(PR_END_OF_FILE_ERROR);
	    return SECFailure;
	}
	return (SECStatus)rv;
    }
    ss->handshake = 0;
    return SECSuccess;
}

/************************************************************************/


/* BSDI etc. ain't got no cuserid() */
#if defined(__386BSD__) || defined(FREEBSD)
#define NEED_CUSERID 1
#endif

#ifdef NEED_CUSERID
#include <pwd.h>
static char *
my_cuserid(char *b)
{
    struct passwd *pw = getpwuid(getuid());

    if (!b) 
    	return pw ? pw->pw_name : NULL;

    if (!pw || !pw->pw_name)
	b[0] = '\0';
    else
	strcpy(b, pw->pw_name);
    return b;
}
#endif


/* sa identifies the server to which we want to connect.
 * First determine whether or not to use socks.
 * If not, connect directly to server.
 * If so,  connect to socks proxy, and send SOCKS_CONNECT cmd, but 
 *	Does NOT wait for reply from socks proxy.
 */
int
ssl_SocksConnect(sslSocket *ss, const PRNetAddr *sa)
{
    int rv, err, direct;
    PRNetAddr daemon;
    const PRNetAddr *sip;
    char *user;
    PRFileDesc *osfd = ss->fd->lower;

    /* Figure out where to connect to */
    rv = FindDaemon(ss, &daemon);
    if (rv) {
	return SECFailure;
    }
    direct = ChooseAddress(ss, sa);
    if (direct) {
	sip = sa;
	ss->socks->direct = 1;
    } else {
	sip = &daemon;
	ss->socks->direct = 0;
    }
    SSL_TRC(10, ("%d: SSL[%d]: socks %s connect to %d.%d.%d.%d:%d",
		 SSL_GETPID(), ss->fd,
		 direct ? "direct" : "sockd",
		 (PR_ntohl(sip->inet.ip) >> 24) & 0xff,
		 (PR_ntohl(sip->inet.ip) >> 16) & 0xff,
		 (PR_ntohl(sip->inet.ip) >> 8) & 0xff,
		 PR_ntohl(sip->inet.ip) & 0xff,
		 PR_ntohs(sip->inet.port)));

    /* Attempt first connection */
    rv = osfd->methods->connect(osfd, sip, ss->cTimeout);
    err = PORT_GetError();
#ifdef _WIN32
    PR_Sleep(PR_INTERVAL_NO_WAIT);     /* workaround NT winsock connect bug. */
#endif
    if (rv < 0) {
	if (err != PR_IS_CONNECTED_ERROR) {
	    return rv;
	}
	/* Async connect finished */
    }

    /* If talking to sockd, do handshake */
    if (!direct) {
	/* Find user */
#ifdef XP_UNIX
#ifdef NEED_CUSERID
	user = my_cuserid(NULL);
#else
	user = cuserid(NULL);
#endif
	if (!user) {
	    PORT_SetError(PR_UNKNOWN_ERROR);
	    SSL_DBG(("%d: SSL[%d]: cuserid fails, errno=%d",
		     SSL_GETPID(), ss->fd, PORT_GetError()));
	    return SECFailure;
	}
#else
	user = "SSL";
#endif

	/* Send our message to it */
	rv = SayHello(ss, SOCKS_CONNECT, sa, user);
	if (rv) {
	    return rv;
	}

	ss->handshake     = SocksStartGather;
	ss->nextHandshake = 0;

	/* save up who we're really talking to so we can index the cache */
	if ((sa->inet.family & 0xff) == PR_AF_INET) {
	     PR_ConvertIPv4AddrToIPv6(sa->inet.ip, &ss->peer);
	     ss->port = sa->inet.port;
	} else {
	     PORT_Assert(sa->ipv6.family == PR_AF_INET6);
	     ss->peer = sa->ipv6.ip;
	     ss->port = sa->ipv6.port;
	}
    }
    return 0;
}

/* Called from ssl_SocksBind(), SSL_BindForSockd(), and ssl_SocksAccept().
 * NOT called from ssl_SocksConnect().
 */
static int
SocksWaitForResponse(sslSocket *ss)
{
    int rv;

    ss->handshake = SocksStartGather;
    ss->nextHandshake = 0;

    /* Get response. Do it now, spinning if necessary (!) */
    for (;;) {
	ssl_Get1stHandshakeLock(ss);
	rv = ssl_Do1stHandshake(ss);
	ssl_Release1stHandshakeLock(ss);
	if (rv == SECWouldBlock || 
	   (rv == SECFailure && PORT_GetError() == PR_WOULD_BLOCK_ERROR)) {
#ifdef XP_UNIX
	    /*
	    ** Spinning is really evil under unix. Call select and
	    ** continue when a read select returns true. We only get
	    ** here if the socket was marked async before the bind
	    ** call.
	    */
	    PRPollDesc spin;
	    spin.fd       = ss->fd->lower;
	    spin.in_flags = PR_POLL_READ;
	    rv = PR_Poll(&spin, 1, PR_INTERVAL_NO_TIMEOUT);
	    if (rv < 0) {
		return rv;
	    }
#else
	    PRIntervalTime ticks = PR_MillisecondsToInterval(1);
	    PR_Sleep(ticks);
#endif
	    continue;
	}
	break;
    }
    return rv;
}

/* sa identifies the server address we want to bind to.
 * First, determine if we need to register with a socks proxy.
 * If socks, then Connect to Socks proxy daemon, send SOCKS_BIND message,
 *    wait for response from socks proxy. 
 */
int
ssl_SocksBind(sslSocket *ss, const PRNetAddr *sa)
{
    sslSocksInfo * si;
    PRFileDesc *   osfd 	= ss->fd->lower;
    char *         user;
    int            rv;
    int            direct;
    PRNetAddr      daemon;

    PORT_Assert(ss->socks != 0);
    si = ss->socks;

    /* Figure out where to connect to */
    rv = FindDaemon(ss, &daemon);
    if (rv) {
	return SECFailure;
    }
    direct = ChooseAddress(ss, sa);
    if (direct) {
	ss->socks->direct = 1;
	rv = osfd->methods->bind(osfd, sa);
	PORT_Memcpy(&ss->socks->bindAddr, sa, sizeof(PRNetAddr));
    } else {
	ss->socks->direct = 0;
	SSL_TRC(10, ("%d: SSL[%d]: socks sockd bind to %d.%d.%d.%d:%d",
		     SSL_GETPID(), ss->fd,
		     (PR_ntohl(daemon.inet.ip) >> 24) & 0xff,
		     (PR_ntohl(daemon.inet.ip) >> 16) & 0xff,
		     (PR_ntohl(daemon.inet.ip) >> 8) & 0xff,
		     PR_ntohl(daemon.inet.ip) & 0xff,
		     PR_ntohs(daemon.inet.port)));

	/* First connect to socks daemon. ASYNC connects must be disabled! */
	rv = osfd->methods->connect(osfd, &daemon, ss->cTimeout);
	if (rv < 0) {
	    return rv;
	}

	/* Find user */
#ifdef XP_UNIX
#ifdef NEED_CUSERID
	user = my_cuserid(NULL);
#else
	user = cuserid(NULL);
#endif
	if (!user) {
	    SSL_DBG(("%d: SSL[%d]: cuserid fails, errno=%d",
		     SSL_GETPID(), ss->fd, PORT_GetError()));
	    PORT_SetError(PR_UNKNOWN_ERROR);
	    return SECFailure;
	}
#else
	user = "SSL";
#endif
	/* Send message to sockd */
	rv = SayHello(ss, SOCKS_BIND, sa, user);
	if (rv) {
	    return rv;
	}

	/* SocksGatherRecord up bind response from sockd */
	rv = SocksWaitForResponse(ss);
	if (rv == 0) {
	    /* Done */
	    si->bindAddr.inet.family = PR_AF_INET;
	    si->bindAddr.inet.port   = si->destPort;
	    if (PR_ntohl(si->destHost) == PR_INADDR_ANY) {
		si->bindAddr.inet.ip = daemon.inet.ip;
	    } else {
		si->bindAddr.inet.ip = si->destHost;
	    }
	}
    }
    si->didBind = 1;
    return rv;
}


PRFileDesc *
ssl_SocksAccept(sslSocket *ss, PRNetAddr *addr)
{
    PORT_Assert(0);
#if 0 /* XXX This doesn't work. */
    sslSocket *ns;
    sslSocksInfo *si;
    PRFileDesc *fd, *osfd = ss->fd->lower;
    int rv;

    PORT_Assert(ss->socks != 0);
    si = ss->socks;

    if (!si->didBind || si->direct) {
	/*
	** If we didn't do the bind yet this call will generate an error
	** from the OS. If we did do the bind then we must be direct and
	** let the OS do the accept.
	*/
	fd = osfd->methods->accept(osfd, addr, ss->cTimeout);
	return NULL;
    }

    /* Get next accept response from server */
    rv = SocksWaitForResponse(ss);
    if (rv) {
	return NULL;
    }

    /* Handshake finished. Give dest address back to caller */
    addr->inet.family = PR_AF_INET;
    addr->inet.port   = si->destPort;
    addr->inet.ip     = si->destHost;

    /* Dup the descriptor and return it */
    fd = osfd->methods->dup(osfd);
    if (fd == NULL) {
	return NULL;
    }

    /* Dup the socket structure */
    ns = ssl_DupSocket(ss, fd);
    if (ns == NULL) {
	PR_Close(fd);
	return NULL;
    }

    return fd;
#else
    return NULL;
#endif /* 0 */
}

int
ssl_SocksListen(sslSocket *ss, int backlog)
{
    PRFileDesc *osfd = ss->fd->lower;
    int rv;

    PORT_Assert(ss->socks != 0);

    if (ss->socks->direct) {
	rv = osfd->methods->listen(osfd, backlog);
	return rv;
    }
    return 0;
}

int
ssl_SocksGetsockname(sslSocket *ss, PRNetAddr *name)
{
    PRFileDesc *osfd = ss->fd->lower;
    int rv;

    PORT_Assert(ss->socks != 0);
    if (!ss->socks->didBind || ss->socks->direct) {
	rv = osfd->methods->getsockname(osfd, name);
	return rv;
    }

    PORT_Memcpy(name, &ss->socks->bindAddr, sizeof(PRNetAddr));
    return 0;
}

int
ssl_SocksRecv(sslSocket *ss, unsigned char *buf, int len, int flags)
{
    int rv;

    PORT_Assert(ss->socks != 0);

    if (ss->handshake) {
	ssl_Get1stHandshakeLock(ss);
	rv = ssl_Do1stHandshake(ss);
	ssl_Release1stHandshakeLock(ss);
	if (rv < 0) {
	    return rv;
	}
	rv = ssl_SendSavedWriteData(ss, &ss->saveBuf, ssl_DefSend);
	if (rv < 0) {
	    return SECFailure;
	}
	/* XXX Deal with short write !! */
    }

    rv = ssl_DefRecv(ss, buf, len, flags);
    SSL_TRC(2, ("%d: SSL[%d]: recving %d bytes from sockd",
		SSL_GETPID(), ss->fd, rv));
    return rv;
}

int
ssl_SocksRead(sslSocket *ss, unsigned char *buf, int len)
{
    return ssl_SocksRecv(ss, buf, len, 0);
}

int
ssl_SocksSend(sslSocket *ss, const unsigned char *buf, int len, int flags)
{
    int rv;

    PORT_Assert(ss->socks != 0);

    if (len == 0) 
    	return 0;
    if (ss->handshake) {
	ssl_Get1stHandshakeLock(ss);
	rv = ssl_Do1stHandshake(ss);
	ssl_Release1stHandshakeLock(ss);
	if (rv < 0) {
	    if (rv == SECWouldBlock) {
		return len;		/* ????? XXX */
	    }
	    return rv;
	}
	rv = ssl_SendSavedWriteData(ss, &ss->saveBuf, ssl_DefSend);
	if (rv < 0) {
	    return SECFailure;
	}
	/* XXX Deal with short write !! */
    }

    SSL_TRC(2, ("%d: SSL[%d]: sending %d bytes using socks",
		SSL_GETPID(), ss->fd, len));

    /* Send out the data */
    rv = ssl_DefSend(ss, buf, len, flags);
    /* XXX Deal with short write !! */
    return rv;
}

int
ssl_SocksWrite(sslSocket *ss, const unsigned char *buf, int len)
{
    return ssl_SocksSend(ss, buf, len, 0);
}

/* returns  > 0 if direct
 * returns == 0 if socks
 * returns  < 0 if error.
 */
int
SSL_CheckDirectSock(PRFileDesc *s)
{
    sslSocket *ss;

    ss = ssl_FindSocket(s);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in CheckDirectSock", SSL_GETPID(), s));
	return SECFailure;
    }

    if (ss->socks != NULL) {
	return ss->socks->direct;
    }
    return SECFailure;
}


SECStatus
SSL_ConfigSockd(PRFileDesc *s, PRUint32 host, PRUint16 port)
{
    sslSocket *ss;
    SECStatus rv;

    ss = ssl_FindSocket(s);
    if (!ss) {
	SSL_DBG(("%d: SSL[%d]: bad socket in ConfigSocks", SSL_GETPID(), s));
	return SECFailure;
    }

    /* Create socks info if not already done */
    rv = ssl_CreateSocksInfo(ss);
    if (rv) {
	return rv;
    }
    ss->socks->sockdHost = host;
    ss->socks->sockdPort = port;
    return SECSuccess;
}

