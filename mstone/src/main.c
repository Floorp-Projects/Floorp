/* -*- Mode: C; c-file-style: "stroustrup"; comment-column: 40 -*- */
/* 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Mailstone utility, 
 * released March 17, 2000.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1999-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s):	Dan Christian <robodan@netscape.com>
 *			Marcel DePaolis <marcel@netcape.com>
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License Version 2 or later (the "GPL"), in
 * which case the provisions of the GPL are applicable instead of
 * those above.  If you wish to allow use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GPL.  If you do not delete the
 * provisions above, a recipient may use your version of this file
 * under either the NPL or the GPL.
 */
/*
  main.c handles all the initialization and then forks off sub processes.
*/

#include "bench.h"

/* really globalize variables */
volatile int gf_timeexpired = 0;
time_t	gt_testtime = 0;		/* time of test, in seconds */
time_t	gt_startedtime = 0;		/* when we started */
volatile time_t	gt_shutdowntime = 0;	/* startedtime + testtime */
time_t	gt_stopinterval = 0;	/* MAX (ramptime/5, 10) */
time_t	gt_aborttime = 0;		/* startedtime + testtime + ramptime*/

int	gn_record_telemetry = 0;
int     gn_total_weight = 0;
int	gn_client_throttle = 0;
int	gn_maxerrorcnt = 0;
int	gn_maxBlockCnt = 0;
int	gn_numprocs = 0;
int	gn_numthreads = 0;
int     gn_debug = 0;
int	gn_feedback_secs = 5;
int	gf_abortive_close = 0;
int	gn_number_of_commands = 0;
int	gf_imapForceUniqueness = 0;
char	gs_dateStamp[DATESTAMP_LEN];
char	gs_thishostname[MAXHOSTNAMELEN+10] = "";
char	*gs_parsename = gs_thishostname; /* name used during parsing */
pid_t 	gn_myPID = 0;
mail_command_t *g_loaded_comm_list;	/* actually a dynamic array */

protocol_t	g_protocols[] = {	/* array of protocol information */
    {
     "SMTP",
     SmtpParseStart,
     SmtpParseEnd,
     sendSMTPStart,
     sendSMTPLoop,
     sendSMTPEnd,
     pishStatsFormat,
     pishStatsInit,
     pishStatsUpdate,
     pishStatsOutput,
    },
    {
     "POP3",
     Pop3ParseStart,
     Pop3ParseEnd,
     doPop3Start,
     doPop3Loop,
     doPop3End,
     pishStatsFormat,
     pishStatsInit,
     pishStatsUpdate,
     pishStatsOutput,
    },
    {
     "IMAP4",
     Imap4ParseStart,
     Imap4ParseEnd,
     doImap4Start,
     doImap4Loop,
     doImap4End,
     pishStatsFormat,
     pishStatsInit,
     pishStatsUpdate,
     pishStatsOutput,
    },
    {
     "HTTP",
     HttpParseStart,
     HttpParseEnd,
     doHttpStart,
     doHttpLoop,
     doHttpEnd,
     HttpStatsFormat,
     HttpStatsInit,
     HttpStatsUpdate,
     HttpStatsOutput,
    },
    {
     "WMAP",
     WmapParseStart,
     WmapParseEnd,
     doWmapStart,
     doWmapLoop,
     doWmapEnd,
     WmapStatsFormat,
     WmapStatsInit,
     WmapStatsUpdate,
     WmapStatsOutput,
    },
    {
     NULL,				/* terminate the list */
    },
};

/* End of globals */

static time_t	ramptime = 0;

static char	mailmaster[MAXHOSTNAMELEN];
static NETPORT	listenport = 0;
static char	*commandsfilename = NULL;
static int	f_usestdin = 0;
#if 0
static struct hostent 	mailserv_phe, mailmast_phe;
static struct protoent	mailserv_ppe, mailmast_ppe; 
static unsigned long	mailserv_addr, mailmast_addr; 
static short 		mailserv_type, mailmast_type;               /* socket type */
#endif

static void
usage(const char *progname)
{
    fprintf(stderr, "Usage: %s [options] -n <clients> -t <time> <-s | -u <commandFile>>\n\n", 
			progname);
    fprintf(stderr, "  required parameters:\n");
    fprintf(stderr, "    -n numprocs      number of clients processes to run [1-%d]\n",
	      MAXPROCSPERNODE);
    fprintf(stderr, "    -t testtime      test duration (mins) or #[hmsd]\n");
    fprintf(stderr, "    -s               use stdin for commands\n");
    fprintf(stderr, "    -u commandlist   file for commands\n\n");

    fprintf(stderr, "  options:\n");
    fprintf(stderr, "    -h               help - this message\n\n");
    fprintf(stderr, "    -H name          hostname for parsing HOSTS=\n");
    fprintf(stderr, "    -A               use abortive close\n");
    fprintf(stderr, "    -T opsperhour    client throttle\n");
    fprintf(stderr, "    -f summpersec    frequency to send summary results\n");
    fprintf(stderr, "    -d               debug\n");
    fprintf(stderr, "    -D datestamp     assign a datestamp\n");
    fprintf(stderr, "    -N numthreads    number of clients threads per process\n");
    fprintf(stderr, "    -m maxErrorCnt   threshold to force test abort\n");
    fprintf(stderr, "    -M maxBlockCnt   number of blocks to run\n");
    fprintf(stderr, "    -R ramptime      test rampup time (secs)\n");
    fprintf(stderr, "    -v               print version\n");
    fprintf(stderr, "    -r               record all transactions\n\n");
    exit(1);
} /* END usage() */

void
parseArgs(int argc, char **argv)
{
    int		getoptch;
    extern char	*optarg;
    extern int	optind;

    /* 
     * PARSE THE COMMAND LINE OPTIONS
     */

    while((getoptch = 
	getopt(argc,argv,"hf:H:T:t:u:c:m:M:n:N:R:D:Adrsv")) != EOF)
    {
        switch(getoptch)
        {
	case 'h':
	    usage(argv[0]);
	    break;
	case 'H':			/* name for parsing */
	    gs_parsename = mystrdup(optarg);
	    break;
	case 'A':
	    gf_abortive_close = 1;
	    break;
	case 'T':
	    /* client throttles to ops/hour */
	    gn_client_throttle = atoi(optarg);
	    break;
	case 'f':
	    /* feedback frequency in seconds */
	    gn_feedback_secs = atoi(optarg);
	    break;
	case 'd':
            gn_debug = 1;
            break;
	case 'D':
	    strcpy(gs_dateStamp, optarg);
	    break;
	case 'm':
            gn_maxerrorcnt = atoi(optarg);
            break;
	case 'M':
            gn_maxBlockCnt = atoi(optarg);
            break;
	case 'n':
            gn_numprocs = atoi(optarg);
            break;
	case 'N':
	    gn_numthreads = atoi(optarg);
	    break;
	case 'u':
	    commandsfilename = mystrdup(optarg);
	    break;
	case 's':
	    f_usestdin = 1;
	    break;
	case 't':
	    gt_testtime = time_atoi(optarg);
	    break;
	case 'R':
	    ramptime = time_atoi(optarg);
	    break;
	case 'v':
	    fprintf(stderr, "Netscape " MAILCLIENT_VERSION "\n");
	    fprintf(stderr, "Copyright (c) Netscape Communications Corporation 1997, 1998, 1999\n");
	    exit(1);
	    break;
	case 'r':	
	    gn_record_telemetry = 1;
	    break;
        default:
            usage(argv[0]);
        }
    }
}

#define SIZEOF_NTOABUF  ((3+1)*4)

char *
safe_inet_ntoa(struct in_addr ina, char *psz)
{
  sprintf(psz,"%d.%d.%d.%d",
	  ((unsigned char *)&ina.s_addr)[0],
	  ((unsigned char *)&ina.s_addr)[1],
	  ((unsigned char *)&ina.s_addr)[2],
	  ((unsigned char *)&ina.s_addr)[3]);
  return psz;
}

/* look up the host name and protocol
 * called from each protocol (via connectSocket)
 */

int
resolve_addrs(char *host,
	      char *protocol,
	      struct hostent *host_phe,
	      struct protoent *proto_ppe,
	      unsigned long *addr,
	      short *type)
{
    struct hostent *phe;
    struct protoent *ppe;
#ifdef USE_PTHREADS
#ifdef USE_GETHOSTBYNAME_R
    struct hostent he;
    struct protoent pe;
    char buf[512];
    int h_err;
#endif
#endif

    /* if IP address given, convert to internal form */
    if (host[0] >= '0' && host[0] <= '9') {
	*addr = inet_addr(host);
	if (*addr == INADDR_NONE)
	    return(returnerr(stderr,"Invalid IP address %s\n", host));
	 
    } else {
	/* look up by name */
#ifdef USE_GETHOSTBYNAME_R
	phe = gethostbyname_r(host, &he, buf, sizeof(buf), &h_err);
	errno = h_err;
#else
	phe = gethostbyname(host);
#endif

	if (phe == NULL)
	{
	    D_PRINTF(stderr, "Gethostbyname failed: %s", neterrstr() );
	    return(returnerr(stderr,"Can't get %s host entry\n", host));
	}
	memcpy(host_phe, phe, sizeof(struct hostent));
	memcpy((char *)addr, phe->h_addr, sizeof(*addr));
    }

    /* Map protocol name to protocol number */
#ifdef USE_GETPROTOBYNAME_R
    ppe = getprotobyname_r(protocol, &pe, buf, sizeof(buf));
#else
    ppe = getprotobyname(protocol);
#endif

    if (ppe == 0)
    {
	D_PRINTF(stderr, "protobyname returned %d\n",	ppe );
	return(returnerr(stderr,"Can't get %s protocol entry\n",protocol));
    }
    memcpy(proto_ppe, ppe, sizeof(struct protoent));

    /* D_PRINTF(stderr, "Protocol number %d\n", ppe->p_proto ); */

    /* Use protocol to choose a socket type */
    if (strcmp(protocol,"udp") == 0)
    {
	*type = SOCK_DGRAM;
    }
    else
    {
	*type = SOCK_STREAM;
	/* D_PRINTF(stderr, "Choosing SOCK_STREAM %d type %d %s\n",  
	    SOCK_STREAM, *type, neterrstr() ); */
    }

    return 0;
}

/* connect to a socket given the hostname and protocol */
SOCKET
connectSocket(ptcx_t ptcx,
	    resolved_addr_t *hostInfo,
	    char *protocol)
{
    struct sockaddr_in sin;  	/* an Internet endpoint address */
    SOCKET 	s;              /* socket descriptor */
    int 		type;           /* socket type */
    short 	proto;
    int 		returnval;	/* temporary return value */
    char		ntoa_buf[SIZEOF_NTOABUF];
 
    D_PRINTF(debugfile, "Beginning connectSocket; host=%s port=%d proto=%s\n",
	     hostInfo->hostName, hostInfo->portNum, protocol );

    sin.sin_family = AF_INET;
    memset((char *)&sin, 0, sizeof(sin));
    D_PRINTF(debugfile, "Zeroed address structure\n" );

    sin.sin_port = htons(hostInfo->portNum);
    D_PRINTF(debugfile, "Set port number %d\n", hostInfo->portNum);

    /* check if we've resolved this already */
    if ((hostInfo) && (hostInfo->resolved)) {
	sin.sin_addr.S_ADDR = hostInfo->host_addr;
	sin.sin_family = PF_INET;
	proto = hostInfo->host_ppe.p_proto;
	type = hostInfo->host_type;
    } else {
	struct hostent 	host_phe;
	struct protoent	host_ppe;
	unsigned long	host_addr;
	short 		host_type;       /* socket type */

	if (resolve_addrs(hostInfo->hostName, "tcp",
			  &host_phe, &host_ppe, &host_addr, &host_type)) {
	    return returnerr(debugfile,"Can't resolve hostname %s in get()\n",
			     hostInfo->hostName);
	}
	sin.sin_addr.S_ADDR = host_addr;
	sin.sin_family = PF_INET;
	proto = host_ppe.p_proto;
	type = host_type;
    }

    /* Allocate a socket */
    s = socket(PF_INET, type, proto);
  
    if (BADSOCKET(s))
    {
	D_PRINTF(debugfile, "Can't create socket: %s\n",neterrstr() );
	return BADSOCKET_VALUE;
    }
  
    /* Connect the socket */
    D_PRINTF(debugfile, "Trying to connect %d with size %d\n",
	     s, sizeof(sin));
    D_PRINTF(debugfile, "Address is family %d, port %d, addr %s\n", 
	     sin.sin_family, ntohs(sin.sin_port),
	     safe_inet_ntoa(sin.sin_addr, ntoa_buf) );
  
    returnval = connect(s, (struct sockaddr *)&sin, sizeof(sin));

    if (returnval < 0) {
	int err = GET_ERROR;  /* preserve the error code */

	D_PRINTF(debugfile, "Can't connect: %s\n", neterrstr() );
	NETCLOSE(s);

	SET_ERROR(err);
	return BADSOCKET_VALUE;
    }

    /* all done, returning socket descriptor */
    D_PRINTF(debugfile, "Returning %d from connectSocket call\n", s );
    return(s);

} /* END connectSocket() */

int
set_abortive_close(SOCKET sock)
{
    struct linger linger_opt;

    linger_opt.l_onoff = 1;
    linger_opt.l_linger = 0;

    if (setsockopt(sock, SOL_SOCKET, SO_LINGER,
		   (char *) &linger_opt, sizeof(linger_opt)) < 0) {
	returnerr(stderr, "Couldn't set SO_LINGER = 0\n");
	return -1;
    }
    return 0;
}

void
initializeCommands(char *cfilename)
{
    char *cbuf = NULL;
    int cbuflen = 0;
    int cbufalloced = 0;
    int ret;
    FILE *cfile;

    D_PRINTF(stderr, "initializeCommands(%s)\n",
	     cfilename  ? cfilename : "STDIN");

    if (cfilename == NULL || strlen(cfilename) == 0) {
	cfile = stdin;
	cfilename = "stdin";
    } else {
	if ((cfile = fopen(cfilename, "r")) == NULL) {
	    D_PRINTF(stderr, "Cannot open commands file %s: errno=%d: %s\n",
		     cfilename, errno, strerror(errno));
	    errexit(stderr, "Cannot open commands file %s: errno=%d: %s\n",
		    cfilename, errno, strerror(errno));
	}
    }

#define CMDBUF_INCR 4096		/* longest allowed line */

    while (!feof(cfile)) {		/* read file in to char array */
	if ((cbuflen + CMDBUF_INCR + 1) > cbufalloced) { /* grow array */
	    cbufalloced += CMDBUF_INCR;
	    cbuf = (char *)myrealloc(cbuf, cbufalloced+1);
	    cbuf[cbuflen] = '\0';
	}
	ret = fread(cbuf+cbuflen, 1, CMDBUF_INCR, cfile);
	if (ret < 0) {
	    D_PRINTF(stderr, "Error reading commands file %s: errno=%d: %s\n",
		     cfilename, errno, strerror(errno));
	    errexit(stderr, "Error reading commands file %s: errno=%d: %s\n",
		    cfilename, errno, strerror(errno));
	}
	if (ret == 0)
	    break;
	cbuflen += ret;
	cbuf[cbuflen] = '\0';
    }
  
    if (cfile != stdin)
	fclose(cfile);

    /*  D_PRINTF(stderr, "Got commands len=%d:\n%s\n", cbuflen, cbuf); */

    /* Read commands into structure, make sure we have all req arguments */
    if ((gn_total_weight = load_commands(cbuf)) < 0) {
	D_PRINTF(stderr, "could not load %s\n", cfilename);
	errexit(stderr, "Could not load command file\n");
    }
    if (0 == gn_total_weight) {
	D_PRINTF(stderr, "No commands found for this host in %s\n", cfilename);
	errexit(stderr, "No command for current host in command file\n");
    }
}

int
readwriteStream(int ii, int fdin, int fdout)
{
    char buf[MAX (8192, 2*SIZEOF_SUMMARYTEXT+1)], *cp;
    int res;
    int toread = sizeof(buf)-1;
    int towrite;

    D_PRINTF(stderr, "readwriteStream(%d,%d,%d)\n", ii, fdin, fdout);

    /* structured as a while() for future nonblocking style */
    while (toread) {
	errno = 0;
	res = NETREAD(fdin, buf, sizeof(buf)-1);
	D_PRINTF(stderr, "read %d bytes from client %d\n", res, ii);
	if (res == 0) {
	    return -1; /* EOF unless O_NDELAY */
	} else if (res < 0) {
	    if (errno == EINTR || errno == EINTR) {
		return 0; /* go back to the poll loop to service others */
	    }

	    fprintf(stderr, "readwriteStream(%d,%d,%d) error reading: errno=%d: %s\n",
		    ii, fdin, fdout, errno, strerror(errno));
	    return -1;
	} else {
	    /* TODO: ...can do more data reduction here... */

	    toread -= res;

	    cp = buf;
	    towrite = res;
	    buf[towrite] = '\0';
	    /*D_PRINTF(stderr, "writing %d bytes to %d [%s]\n", towrite, fdout, buf);*/
	    D_PRINTF(stderr, "writing %d bytes to %d\n", towrite, fdout);
	    while (towrite) {
		res = write(fdout, cp, towrite);
		D_PRINTF(stderr, "wrote %d bytes to %d\n", res, fdout);
		if (res <= 0) {
		    D_PRINTF(stderr, "error writing to %d: errno=%d: %s\n", fdout, errno, strerror(errno));
		} else {
		    towrite -= res;
		    cp += res;
		}
	    }
	}
	toread = 0; /* just read one chunk at a time for now... */
    }
    return 0;
}

#ifdef _WIN32
#define DIRECT_OUT			/* report directly to stdout */

int
readwriteChildren(pccx_t pccxs)
{
    while (readwriteStream(0, pccxs[0].socket, 1) >= 0)
	;
    
    return 0;
}

#else /* !_WIN32 */

/* This is where the master process merges output and waits for 
   the spawned processes to exit.
   Note that no alarm has been set.  We never wait very long in poll.
*/
int
readwriteChildren(pccx_t pccxs)
{
    struct pollfd *pfds;
    int nfds;
    int ii;

    /*
     * Wait for all children to exit.
     */
    nfds=0;
    pfds = (struct pollfd *)mycalloc(sizeof(struct pollfd)*gn_numprocs);
    for (ii=0; ii < gn_numprocs; ++ii) {
	if (pccxs[ii].socket != -1) {
	    pfds[nfds].fd = pccxs[ii].socket;
	    ++nfds;
	}
    }

    fflush(stdout);

    while (nfds > 0) {
	int ret, closethisfd;
	if (time(0L) >=  gt_aborttime) {
	    D_PRINTF (stderr, "Time is up.  Signalling exit %d\n",
		      gf_timeexpired);
	    gf_timeexpired = EXIT_FASTEST; /* signal clean up and exit */
	    break;			/* just get out of here */
	}
	for (ii=0; ii < nfds; ++ii) {
	    pfds[ii].events = POLLIN;
	    pfds[ii].revents = 0;
	}

	D_PRINTF(stderr, "entering poll(nfds=%d)\n", nfds);
	ret = poll(pfds, nfds, 5*1000);
	D_PRINTF(stderr, "back from poll, ret=%d\n", ret);

	if (ret == 0)
	    continue;

	if (ret < 0) {
	    if (errno == EAGAIN || errno == EINTR)
		continue;

	    fprintf(stderr, "poll error: errno=%d: %s\n", errno, strerror(errno));
	    break;
	}

	for (ii = 0; ii < nfds; ++ii) {
	    closethisfd = 0;

	    if (pfds[ii].revents) {
		D_PRINTF(stderr, "poll says stdout fd=%d for client=%d is 0x%02x\n",
			 pfds[ii].fd, ii, pfds[ii].revents);
	    }

	    if (pfds[ii].revents & POLLIN) {
		if (readwriteStream(ii, pfds[ii].fd, 1) == -1) {
		    closethisfd = 1;
		}
	    } else if (pfds[ii].revents & (POLLHUP | POLLERR | POLLNVAL)) {
		if (pfds[ii].revents & POLLHUP)
		    D_PRINTF(stderr, "POLLHUP for stdout fd=%d for client=%d!\n",
			    pfds[ii].fd, ii);
		closethisfd = 1;
	    }

	    if (closethisfd) {
		D_PRINTF(stderr, "closing for slot=%d fd=%d nfds=%d\n",
			 ii, pfds[ii].fd, nfds);
		NETCLOSE(pfds[ii].fd);

		--nfds;	/* shrink poll array */
		/* NOTE: this re-orders the array */
		if (ii != nfds) {	/* move last one into old spot */
		    pfds[ii].fd = pfds[nfds].fd;
		    pfds[ii].events = pfds[nfds].events;
		    pfds[ii].revents = pfds[nfds].revents;
		    --ii;	/* check moved entry on the next loop */
		}
	    }
	}
    }
    if (nfds > 0) {
	d_printf (stderr,
		 "WARNING: Exiting with open clients nfds=%d time=%lu shutdowntime=%lu\n",
		  nfds, time(0L), gt_shutdowntime);
	for (ii = 0; ii < nfds; ++ii) {	/* close socket to make clients die */
	    D_PRINTF(stderr, "closing for slot=%d fd=%d\n", ii, pfds[ii].fd);
	    NETCLOSE(pfds[ii].fd);
	}
    }
    return 0;
}

#endif /* _WIN32 */

/* This is where each sub process starts */
THREAD_RET
launchChild(void *targ)
{
    SOCKET	clientsock;
    struct sockaddr_in saddr; /* server address */
    struct linger linger_opt;
    unsigned int testtimeleft;
    int ret;
    unsigned int thread_stagger_usec = 1000; /* 1000/sec */
    int pnum = (int)targ;

    gn_myPID = getpid();
    if (ramptime) {
	/* comvert to microseconds */
	if (gn_numthreads > 0) {
	    /* force intermediate result to double to avoid overflow */
	    thread_stagger_usec =  (unsigned int)((ramptime * (double)USECINSEC) / gn_numthreads);
	} else {
	    thread_stagger_usec = 0;
	}
    }

    if ((clientsock=socket(AF_INET, SOCK_STREAM, 0)) == -1) {
	errexit(stderr, "child socket(): %s\n", neterrstr());
    }

    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_addr.S_ADDR = inet_addr("127.0.0.1");
    saddr.sin_family = AF_INET;
    saddr.sin_port = listenport;

    if (connect(clientsock, (struct sockaddr *)&saddr, sizeof(saddr)) == -1) {
	NETCLOSE(clientsock);
	errexit(stderr, "child connect(): %s\n", neterrstr());
    }
#if 1
    linger_opt.l_onoff = 1;
    linger_opt.l_linger = 60;

    if (setsockopt(clientsock, SOL_SOCKET, SO_LINGER, (char *) &linger_opt, sizeof(linger_opt)) < 0) {
	NETCLOSE(clientsock);
	errexit(stderr, "child setsockopt(): %s\n", neterrstr());
    }
#endif
    D_PRINTF(stderr, "child %d: using socket %d\n", pnum, clientsock);


#if 0
    unsigned int fork_stagger_usec = 10000; /* 100/sec */
    if (pnum) {
	D_PRINTF(stderr, "child %d: delaying %d secs\n",
		 pnum, USECS_2_SECS(pnum * fork_stagger_usec));
	MS_usleep(pnum * fork_stagger_usec);
    }
#endif

    testtimeleft = gt_shutdowntime - time(0L);
	    
    D_PRINTF(stderr, "child %d: proceeding for %d remaining seconds\n",
	     pnum, testtimeleft);

    if (testtimeleft > 0) {
	/* This is where the test gets run */
#ifndef DIRECT_OUT
	ret = clientProc(pnum, clientsock,
			 testtimeleft, thread_stagger_usec);
#else
	ret = clientProc(pnum, fileno(stdout),
			 testtimeleft, thread_stagger_usec);
#endif
    } else {
	D_PRINTF(stderr, "child %d: Too late to start! shudowntime=%lu now=%lu\n",
		 pnum, testtimeleft, time(0L));
	ret = -1;
    }
	    
    D_PRINTF(stderr, "child %d: closing logging socket\n", pnum);
    NETCLOSE(clientsock);

#ifndef _WIN32
    return((void *)ret);
#endif
}

/* Wait for childred to exit (after readwritechildren returns).
 */
int
waitChildren(void)
{
#ifndef _WIN32
    int pid;
    int	status;

    for (;;) {
	errno = 0;
	alarm(gt_stopinterval);		/* dont wait forever */
	pid = wait(&status);
	if (pid == -1) {
	    if (errno == ECHILD) {
		break;			/* none left.  finished */
	    }
	    if (errno == EINTR) {	/* alarm went off */
		if (time(0L) > (gt_aborttime+(EXIT_FAST*gt_stopinterval))) {
		    d_printf (stderr,
			      "WARNING: Aborting wait for children!\n");
		    break;
		}
	    }
	}
	if (WIFSIGNALED (status)) {	/* show error exits */
	    d_printf (stderr, "Client pid %d died with signal %d\n",
		     pid, WTERMSIG(status));
	} else {
	    D_PRINTF(stderr, "Client pid %d: status: %d errno=%d: %s\n",
		     pid, status, errno, strerror(errno));
	}
    }
#endif /* !_WIN32 */
    return 0;
}

int
main(int argc, char *argv[])
{
    int 	ii;
    struct tm	*tmptm;
    time_t	currentTime;
#ifdef _WIN32
    int 	err;
    WSADATA 	WSAData;
#endif
    int		ret;
    int		pid=0;
    pccx_t	pccxs; /* client process contexts */
    SOCKET	serversock;
    struct sockaddr_in saddr; /* server address */
    struct sockaddr_in caddr; /* client address */
    int		addr_len;
    char ntoabuf[SIZEOF_NTOABUF];

#ifdef _WIN32
    //MessageBeep(~0U);	/* announce our existence */

    err = WSAStartup(MAKEWORD(1,1), &WSAData);
    if (err != 0) {
	errexit(stderr, "Error in WSAStartup()\n");
    }

    atexit(sock_cleanup);

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
#endif /* _WIN32 */

    gn_myPID = getpid();
    gethostname(gs_thishostname, sizeof(gs_thishostname)-1);

    memset(mailmaster, 0, sizeof(mailmaster));
    memset(gs_dateStamp, 0, DATESTAMP_LEN*sizeof(char));

    parseArgs(argc, argv);

    returnerr(stderr, MAILCLIENT_VERSION "\n");
    returnerr(stderr, "procs=%d threads=%d seconds=%d ramptime=%d...\n",
	      gn_numprocs, gn_numthreads , gt_testtime, ramptime);
    if (ramptime > gt_testtime) {
	returnerr (stderr, "RampTime %d longer than TestTime %d.  Adjusting.\n",
		  ramptime, gt_testtime);
	ramptime = gt_testtime;
    }

    D_PRINTF(stderr, "Running in debug mode\n\n" );

    /* print the command line */
    if (gn_debug) {
      for (ii = 0; ii < argc; ii++)
	fprintf(stderr, "%s ", argv[ii] );
      fprintf(stderr, "\n\n" );
    }

    if (commandsfilename == NULL && f_usestdin == 0) {
        /* Must specify a message list */
	returnerr(stderr, "No mail message list specified (use <-s | -u commandsfilename>)\n");
        usage(argv[0]);
    }

    if (gt_testtime == 0) {         /* NO TEST TIME */
       usage(argv[0]);
    }

    if (gn_numprocs > MAXPROCSPERNODE || gn_numprocs < 1) {
	returnerr(stderr, "Number of clients must be between 1 and %d\n", MAXPROCSPERNODE);
	usage(argv[0]);
    }

    crank_limits();

    if (!gs_dateStamp[0]) {
	time(&currentTime);
	tmptm = localtime(&currentTime); 
	strftime(gs_dateStamp, DATESTAMP_LEN, "%Y%m%d%H%M%S", tmptm);
    }

    initializeCommands(commandsfilename); /* read command block */

    gt_startedtime = time(0L);
    gt_shutdowntime = gt_startedtime + gt_testtime; /* start clean shutdown */
    gt_stopinterval = MAX ((ramptime/EXIT_FASTEST), 1); /* signal period */
    /* this is when the master gives up on the children */
    gt_aborttime = gt_shutdowntime + gt_stopinterval*2*EXIT_FASTEST;


    if ((pccxs = (pccx_t)mycalloc(sizeof(ccx_t) * gn_numprocs)) == NULL) {
	errexit(stderr, "error mycalloc() pccxs\n");
    }

    D_PRINTF(stderr, "preparing serversock\n");

    if ((serversock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
	errexit(stderr, "socket() error: %s\n", neterrstr());
    }

    memset(&saddr, 0, sizeof(saddr));
    /*saddr.sin_addr.S_ADDR = htonl(INADDR_ANY);*/
    saddr.sin_addr.S_ADDR = inet_addr("127.0.0.1");
    saddr.sin_family = AF_INET;
    saddr.sin_port = 0;

    if (bind(serversock, (struct sockaddr *)&saddr, sizeof(saddr)) == -1) {
	NETCLOSE(serversock);
	errexit(stderr, "bind() error: %s\n", neterrstr());
    }

    if (listen(serversock, 512) == -1) {
	NETCLOSE(serversock);
	errexit(stderr, "listen() error: %s\n", neterrstr());
    }

    addr_len = sizeof(saddr);
    if (getsockname(serversock, (struct sockaddr *)&saddr, &addr_len) == -1) {
	NETCLOSE(serversock);
	errexit(stderr, "getsockname() error: %s\n", neterrstr());
    }

    listenport = saddr.sin_port;

    D_PRINTF(stderr, "listening on [%s:%d]\n",
	     safe_inet_ntoa(saddr.sin_addr, ntoabuf), ntohs(listenport));

    setup_signal_handlers ();		/* trap signals */

    for(ii = 0; ii < gn_numprocs; ++ii) {
	D_PRINTF(stderr, "parent: forking client %d\n", ii);

#ifdef _WIN32
	if (_beginthread(launchChild, NT_STACKSIZE, (void *)ii) == -1) {
	    errexit(stderr, "_beginthread failed: %d", GetLastError());
	}
#else
#ifdef DIRECT_OUT			/* for Unix, only if debugging */
	if (1 == gn_numprocs) {
	    fprintf (stderr, "Single process, NOT forking.\n");
	    launchChild (0);		/* DEBUG */
	    return 0;
	}
#endif
        switch(pid=fork()) {
	case 0: /* CHILD */
	    launchChild((void *)ii);
	    exit(ii);
	    break;
	case -1: /* ERROR */
	    fprintf(stderr, "parent: error forking child %d, errno=%d: %s\n", ii, errno, strerror(errno));
	    errexit(stderr, "Error forking child processes\n");
	    exit(1);
	default: /* PARENT */
	    break;
	}
#endif

	pccxs[ii].pid = pid;

	D_PRINTF(stderr, "parent: Accepting child %d pid=%d\n", ii, pid);
	/* Maybe fork everything before accepting */
	addr_len = sizeof(caddr);
	/* If the the child dies immediately, we hang here */
	if ((ret = accept(serversock, (struct sockaddr *)&caddr, &addr_len)) == -1) {
	    errexit(stderr, "accept() error: %s\n", neterrstr());
	}
	D_PRINTF(stderr, "parent: child %d using socket %d\n", ii, ret);
	pccxs[ii].socket = ret;
    } 

    readwriteChildren(pccxs);

    D_PRINTF(stderr, "done polling, now just wait\n");

    waitChildren();

    returnerr(stderr, "Master process done.\n");

    fflush(stdout);

    return 0;
} /* end main() */

