/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <string.h>

#include "secutil.h"

#if defined(XP_UNIX)
#include <unistd.h>
#endif

#if defined(_WINDOWS)
#include <process.h>	/* for getpid() */
#endif

#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>

#include "nspr.h"
#include "prio.h"
#include "prerror.h"
#include "prnetdb.h"
#include "prclist.h"
#include "plgetopt.h"

#ifndef PORT_Sprintf
#define PORT_Sprintf sprintf
#endif

#ifndef PORT_Strstr
#define PORT_Strstr strstr
#endif

#ifndef PORT_Malloc
#define PORT_Malloc PR_Malloc
#endif

static int handle_connection( PRFileDesc *, PRFileDesc *, int );

static const char inheritableSockName[] = { "SELFSERV_LISTEN_SOCKET" };

#define DEFAULT_BULK_TEST 16384
#define MAX_BULK_TEST     1048576 /* 1 MB */
static PRBool testBulk;

/* data and structures for shutdown */
static int	stopping;

static PRBool  noDelay;
static int	verbose;

static PRThread * acceptorThread;

static PRLogModuleInfo *lm;

#define PRINTF  if (verbose)  printf
#define FPRINTF if (verbose) fprintf
#define FLUSH	if (verbose) { fflush(stdout); fflush(stderr); }
#define VLOG(arg) PR_LOG(lm,PR_LOG_DEBUG,arg)

static void
Usage(const char *progName)
{
    fprintf(stderr, 

"Usage: %s -p port [-Dbv]\n"
"         [-t threads] [-i pid_file]\n"
"-D means disable Nagle delays in TCP\n"
"-b means try binding to the port and exit\n"
"-v means verbose output\n"
"-t threads -- specify the number of threads to use for connections.\n"
"-i pid_file file to write the process id of selfserve\n"
	,progName);
}

static const char *
errWarn(char * funcString)
{
    PRErrorCode  perr      = PR_GetError();
    const char * errString = SECU_Strerror(perr);

    fprintf(stderr, "selfserv: %s returned error %d:\n%s\n",
            funcString, perr, errString);
    return errString;
}

static void
errExit(char * funcString)
{
    errWarn(funcString);
    exit(3);
}


#define MAX_VIRT_SERVER_NAME_ARRAY_INDEX  10

/**************************************************************************
** Begin thread management routines and data.
**************************************************************************/
#define MIN_THREADS 3
#define DEFAULT_THREADS 8
#define MAX_THREADS 4096
#define MAX_PROCS 25
static int  maxThreads = DEFAULT_THREADS;


typedef struct jobStr {
    PRCList     link;
    PRFileDesc *tcp_sock;
    PRFileDesc *model_sock;
    int         requestCert;
} JOB;

static PZLock    * qLock; /* this lock protects all data immediately below */
static PRLock    * lastLoadedCrlLock; /* this lock protects lastLoadedCrl variable */
static PZCondVar * jobQNotEmptyCv;
static PZCondVar * freeListNotEmptyCv;
static PZCondVar * threadCountChangeCv;
static int  threadCount;
static PRCList  jobQ;
static PRCList  freeJobs;
static JOB *jobTable;

SECStatus
setupJobs(int maxJobs)
{
    int i;

    jobTable = (JOB *)PR_Calloc(maxJobs, sizeof(JOB));
    if (!jobTable)
    	return SECFailure;

    PR_INIT_CLIST(&jobQ);
    PR_INIT_CLIST(&freeJobs);

    for (i = 0; i < maxJobs; ++i) {
	JOB * pJob = jobTable + i;
	PR_APPEND_LINK(&pJob->link, &freeJobs);
    }
    return SECSuccess;
}

typedef int startFn(PRFileDesc *a, PRFileDesc *b, int c);

typedef enum { rs_idle = 0, rs_running = 1, rs_zombie = 2 } runState;

typedef struct perThreadStr {
    PRFileDesc *a;
    PRFileDesc *b;
    int         c;
    int         rv;
    startFn  *  startFunc;
    PRThread *  prThread;
    runState	state;
} perThread;

static perThread *threads;

void
thread_wrapper(void * arg)
{
    perThread * slot = (perThread *)arg;

    slot->rv = (* slot->startFunc)(slot->a, slot->b, slot->c);

    /* notify the thread exit handler. */
    PZ_Lock(qLock);
    slot->state = rs_zombie;
    --threadCount;
    PZ_NotifyAllCondVar(threadCountChangeCv);
    PZ_Unlock(qLock);
}

int 
jobLoop(PRFileDesc *a, PRFileDesc *b, int c)
{
    PRCList * myLink = 0;
    JOB     * myJob;

    PZ_Lock(qLock);
    do {
	myLink = 0;
	while (PR_CLIST_IS_EMPTY(&jobQ) && !stopping) {
            PZ_WaitCondVar(jobQNotEmptyCv, PR_INTERVAL_NO_TIMEOUT);
	}
	if (!PR_CLIST_IS_EMPTY(&jobQ)) {
	    myLink = PR_LIST_HEAD(&jobQ);
	    PR_REMOVE_AND_INIT_LINK(myLink);
	}
	PZ_Unlock(qLock);
	myJob = (JOB *)myLink;
	/* myJob will be null when stopping is true and jobQ is empty */
	if (!myJob) 
	    break;
	handle_connection( myJob->tcp_sock, myJob->model_sock, 
			   myJob->requestCert);
	PZ_Lock(qLock);
	PR_APPEND_LINK(myLink, &freeJobs);
	PZ_NotifyCondVar(freeListNotEmptyCv);
    } while (PR_TRUE);
    return 0;
}


SECStatus
launch_threads(
    startFn    *startFunc,
    PRFileDesc *a,
    PRFileDesc *b,
    int         c,
    PRBool      local)
{
    int i;
    SECStatus rv = SECSuccess;

    /* create the thread management serialization structs */
    qLock               = PZ_NewLock(nssILockSelfServ);
    jobQNotEmptyCv      = PZ_NewCondVar(qLock);
    freeListNotEmptyCv  = PZ_NewCondVar(qLock);
    threadCountChangeCv = PZ_NewCondVar(qLock);

    /* create monitor for crl reload procedure */
    lastLoadedCrlLock   = PR_NewLock();

    /* allocate the array of thread slots */
    threads = PR_Calloc(maxThreads, sizeof(perThread));
    if ( NULL == threads )  {
        fprintf(stderr, "Oh Drat! Can't allocate the perThread array\n");
        return SECFailure;
    }
    /* 5 is a little extra, intended to keep the jobQ from underflowing. 
    ** That is, from going empty while not stopping and clients are still
    ** trying to contact us.
    */
    rv = setupJobs(maxThreads + 5);
    if (rv != SECSuccess)
    	return rv;

    PZ_Lock(qLock);
    for (i = 0; i < maxThreads; ++i) {
    	perThread * slot = threads + i;

	slot->state = rs_running;
	slot->a = a;
	slot->b = b;
	slot->c = c;
	slot->startFunc = startFunc;
	slot->prThread = PR_CreateThread(PR_USER_THREAD, 
			thread_wrapper, slot, PR_PRIORITY_NORMAL, 
                        (PR_TRUE==local)?PR_LOCAL_THREAD:PR_GLOBAL_THREAD,
                        PR_UNJOINABLE_THREAD, 0);
	if (slot->prThread == NULL) {
	    printf("selfserv: Failed to launch thread!\n");
	    slot->state = rs_idle;
	    rv = SECFailure;
	    break;
	} 

	++threadCount;
    }
    PZ_Unlock(qLock); 

    return rv;
}

#define DESTROY_CONDVAR(name) if (name) { \
        PZ_DestroyCondVar(name); name = NULL; }
#define DESTROY_LOCK(name) if (name) { \
        PZ_DestroyLock(name); name = NULL; }
	

void
terminateWorkerThreads(void)
{
    VLOG(("selfserv: server_thead: waiting on stopping"));
    PZ_Lock(qLock);
    PZ_NotifyAllCondVar(jobQNotEmptyCv);
    while (threadCount > 0) {
	PZ_WaitCondVar(threadCountChangeCv, PR_INTERVAL_NO_TIMEOUT);
    }
    /* The worker threads empty the jobQ before they terminate. */
    PORT_Assert(PR_CLIST_IS_EMPTY(&jobQ));
    PZ_Unlock(qLock); 

    DESTROY_CONDVAR(jobQNotEmptyCv);
    DESTROY_CONDVAR(freeListNotEmptyCv);
    DESTROY_CONDVAR(threadCountChangeCv);

    PR_DestroyLock(lastLoadedCrlLock);
    DESTROY_LOCK(qLock);
    PR_Free(jobTable);
    PR_Free(threads);
}

/**************************************************************************
** End   thread management routines.
**************************************************************************/

PRBool NoReuse         = PR_FALSE;
PRBool disableLocking  = PR_FALSE;
PRBool failedToNegotiateName  = PR_FALSE;


static const char stopCmd[] = { "GET /stop " };
static const char getCmd[]  = { "GET " };
static const char EOFmsg[]  = { "EOF\r\n\r\n\r\n" };
static const char outHeader[] = {
    "HTTP/1.0 200 OK\r\n"
    "Server: Generic Web Server\r\n"
    "Date: Tue, 26 Aug 1997 22:10:05 GMT\r\n"
    "Content-type: text/plain\r\n"
    "\r\n"
};

void stop_server()
{
    stopping = 1;
    PR_Interrupt(acceptorThread);
    PZ_TraceFlush();
}

int
handle_connection( 
    PRFileDesc *tcp_sock,
    PRFileDesc *model_sock,
    int         requestCert
    )
{
    PRFileDesc *       ssl_sock = NULL;
    PRFileDesc *       local_file_fd = NULL;
    char  *            post;
    char  *            pBuf;			/* unused space at end of buf */
    const char *       errString;
    PRStatus           status;
    int                bufRem;			/* unused bytes at end of buf */
    int                bufDat;			/* characters received in buf */
    int                newln    = 0;		/* # of consecutive newlns */
    int                firstTime = 1;
    int                reqLen;
    int                rv;
    int                numIOVs;
    PRSocketOptionData opt;
    PRIOVec            iovs[16];
    char               msgBuf[160];
    char               buf[10240];
    char               fileName[513];

    pBuf   = buf;
    bufRem = sizeof buf;

    VLOG(("selfserv: handle_connection: starting"));
    opt.option             = PR_SockOpt_Nonblocking;
    opt.value.non_blocking = PR_FALSE;
    PR_SetSocketOption(tcp_sock, &opt);

    VLOG(("selfserv: handle_connection: starting\n"));
	ssl_sock = tcp_sock;

    if (noDelay) {
	opt.option         = PR_SockOpt_NoDelay;
	opt.value.no_delay = PR_TRUE;
	status = PR_SetSocketOption(ssl_sock, &opt);
	if (status != PR_SUCCESS) {
	    errWarn("PR_SetSocketOption(PR_SockOpt_NoDelay, PR_TRUE)");
            if (ssl_sock) {
	        PR_Close(ssl_sock);
            }
	    return SECFailure;
	}
    }

    while (1) {
	newln = 0;
	reqLen     = 0;
	rv = PR_Read(ssl_sock, pBuf, bufRem - 1);
	if (rv == 0 || 
	    (rv < 0 && PR_END_OF_FILE_ERROR == PR_GetError())) {
	    if (verbose)
		errWarn("HDX PR_Read hit EOF");
	    break;
	}
	if (rv < 0) {
	    errWarn("HDX PR_Read");
	    goto cleanup;
	}
	/* NULL termination */
	pBuf[rv] = 0;
	if (firstTime) {
	    firstTime = 0;
	}

	pBuf   += rv;
	bufRem -= rv;
	bufDat = pBuf - buf;
	/* Parse the input, starting at the beginning of the buffer.
	 * Stop when we detect two consecutive \n's (or \r\n's) 
	 * as this signifies the end of the GET or POST portion.
	 * The posted data follows.
	 */
	while (reqLen < bufDat && newln < 2) {
	    int octet = buf[reqLen++];
	    if (octet == '\n') {
		newln++;
	    } else if (octet != '\r') {
		newln = 0;
	    }
	}

	/* came to the end of the buffer, or second newln
	 * If we didn't get an empty line (CRLFCRLF) then keep on reading.
	 */
	if (newln < 2) 
	    continue;

	/* we're at the end of the HTTP request.
	 * If the request is a POST, then there will be one more
	 * line of data.
	 * This parsing is a hack, but ok for SSL test purposes.
	 */
	post = PORT_Strstr(buf, "POST ");
	if (!post || *post != 'P') 
	    break;

	/* It's a post, so look for the next and final CR/LF. */
	/* We should parse content length here, but ... */
	while (reqLen < bufDat && newln < 3) {
	    int octet = buf[reqLen++];
	    if (octet == '\n') {
		newln++;
	    }
	}
	if (newln == 3)
	    break;
    } /* read loop */

    bufDat = pBuf - buf;
    if (bufDat) do {	/* just close if no data */
	/* Have either (a) a complete get, (b) a complete post, (c) EOF */
	if (reqLen > 0 && !strncmp(buf, getCmd, sizeof getCmd - 1)) {
	    char *      fnBegin = buf + 4;
	    char *      fnEnd;
	    PRFileInfo  info;
	    /* try to open the file named.  
	     * If successful, then write it to the client.
	     */
	    fnEnd = strpbrk(fnBegin, " \r\n");
	    if (fnEnd) {
		int fnLen = fnEnd - fnBegin;
		if (fnLen < sizeof fileName) {
                    char *fnstart;
                    strncpy(fileName, fnBegin, fnLen);
                    fileName[fnLen] = 0;	/* null terminate */
                    fnstart = fileName;
                    /* strip initial / because our root is the current directory*/
                    while (*fnstart && *fnstart=='/')
                        ++fnstart;
		    status = PR_GetFileInfo(fnstart, &info);
		    if (status == PR_SUCCESS &&
			info.type == PR_FILE_FILE &&
			info.size >= 0 ) {
			local_file_fd = PR_Open(fnstart, PR_RDONLY, 0);
		    }
		}
	    }
	}

	numIOVs = 0;

	iovs[numIOVs].iov_base = (char *)outHeader;
	iovs[numIOVs].iov_len  = (sizeof(outHeader)) - 1;
	numIOVs++;

	if (local_file_fd) {
	    PRInt32     bytes;
	    int         errLen;
            bytes = PR_TransmitFile(ssl_sock, local_file_fd, outHeader,
                                    sizeof outHeader - 1,
                                    PR_TRANSMITFILE_KEEP_OPEN,
                                    PR_INTERVAL_NO_TIMEOUT);
            if (bytes >= 0) {
                bytes -= sizeof outHeader - 1;
                FPRINTF(stderr, 
                        "selfserv: PR_TransmitFile wrote %d bytes from %s\n",
                        bytes, fileName);
                break;
            }
            errString = errWarn("PR_TransmitFile");
            errLen = PORT_Strlen(errString);
            errLen = PR_MIN(errLen, sizeof msgBuf - 1);
            PORT_Memcpy(msgBuf, errString, errLen);
            msgBuf[errLen] = 0;
            
            iovs[numIOVs].iov_base = msgBuf;
            iovs[numIOVs].iov_len  = PORT_Strlen(msgBuf);
            numIOVs++;
	} else if (reqLen <= 0) {	/* hit eof */
	    PORT_Sprintf(msgBuf, "Get or Post incomplete after %d bytes.\r\n",
			 bufDat);

	    iovs[numIOVs].iov_base = msgBuf;
	    iovs[numIOVs].iov_len  = PORT_Strlen(msgBuf);
	    numIOVs++;
	} else if (reqLen < bufDat) {
	    PORT_Sprintf(msgBuf, "Discarded %d characters.\r\n", 
	                 bufDat - reqLen);

	    iovs[numIOVs].iov_base = msgBuf;
	    iovs[numIOVs].iov_len  = PORT_Strlen(msgBuf);
	    numIOVs++;
	}

	if (reqLen > 0) {
	    if (verbose > 1) 
	    	fwrite(buf, 1, reqLen, stdout);	/* display it */

	    iovs[numIOVs].iov_base = buf;
	    iovs[numIOVs].iov_len  = reqLen;
	    numIOVs++;
	}

        /* Don't add the EOF if we want to test bulk encryption */
        if (!testBulk) {
            iovs[numIOVs].iov_base = (char *)EOFmsg;
            iovs[numIOVs].iov_len  = sizeof EOFmsg - 1;
            numIOVs++;
        }

	rv = PR_Writev(ssl_sock, iovs, numIOVs, PR_INTERVAL_NO_TIMEOUT);
	if (rv < 0) {
	    errWarn("PR_Writev");
	    break;
	}

    } while (0);

cleanup:
    if (ssl_sock) {
        PR_Close(ssl_sock);
    } else if (tcp_sock) {
        PR_Close(tcp_sock);
    }
    if (local_file_fd)
	PR_Close(local_file_fd);
    VLOG(("selfserv: handle_connection: exiting\n"));

    /* do a nice shutdown if asked. */
    if (!strncmp(buf, stopCmd, sizeof stopCmd - 1)) {
        VLOG(("selfserv: handle_connection: stop command"));
        stop_server();
    }
    VLOG(("selfserv: handle_connection: exiting"));
    return SECSuccess;	/* success */
}

#ifdef XP_UNIX

void sigusr1_handler(int sig)
{
    VLOG(("selfserv: sigusr1_handler: stop server"));
    stop_server();
}

#endif

SECStatus
do_accepts(
    PRFileDesc *listen_sock,
    PRFileDesc *model_sock,
    int         requestCert
    )
{
    PRNetAddr   addr;
    PRErrorCode  perr;
#ifdef XP_UNIX
    struct sigaction act;
#endif

    VLOG(("selfserv: do_accepts: starting"));
    PR_SetThreadPriority( PR_GetCurrentThread(), PR_PRIORITY_HIGH);

    acceptorThread = PR_GetCurrentThread();
#ifdef XP_UNIX
    /* set up the signal handler */
    act.sa_handler = sigusr1_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    if (sigaction(SIGUSR1, &act, NULL)) {
        fprintf(stderr, "Error installing signal handler.\n");
        exit(1);
    }
#endif
    while (!stopping) {
	PRFileDesc *tcp_sock;
	PRCList    *myLink;

	FPRINTF(stderr, "\n\n\nselfserv: About to call accept.\n");
	tcp_sock = PR_Accept(listen_sock, &addr, PR_INTERVAL_NO_TIMEOUT);
	if (tcp_sock == NULL) {
    	    perr      = PR_GetError();
	    if ((perr != PR_CONNECT_RESET_ERROR &&
	         perr != PR_PENDING_INTERRUPT_ERROR) || verbose) {
		errWarn("PR_Accept");
	    } 
	    if (perr == PR_CONNECT_RESET_ERROR) {
		FPRINTF(stderr, 
		        "Ignoring PR_CONNECT_RESET_ERROR error - continue\n");
		continue;
	    }
	    stopping = 1;
	    break;
	}

        VLOG(("selfserv: do_accept: Got connection\n"));

	PZ_Lock(qLock);
	while (PR_CLIST_IS_EMPTY(&freeJobs) && !stopping) {
            PZ_WaitCondVar(freeListNotEmptyCv, PR_INTERVAL_NO_TIMEOUT);
	}
	if (stopping) {
	    PZ_Unlock(qLock);
            if (tcp_sock) {
	        PR_Close(tcp_sock);
            }
	    break;
	}
	myLink = PR_LIST_HEAD(&freeJobs);
	PR_REMOVE_AND_INIT_LINK(myLink);
	/* could release qLock here and reaquire it 7 lines below, but 
	** why bother for 4 assignment statements? 
	*/
	{
	    JOB * myJob = (JOB *)myLink;
	    myJob->tcp_sock    = tcp_sock;
	    myJob->model_sock  = model_sock;
	    myJob->requestCert = requestCert;
	}

	PR_APPEND_LINK(myLink, &jobQ);
	PZ_NotifyCondVar(jobQNotEmptyCv);
	PZ_Unlock(qLock);
    }

    FPRINTF(stderr, "selfserv: Closing listen socket.\n");
    VLOG(("selfserv: do_accepts: exiting"));
    if (listen_sock) {
        PR_Close(listen_sock);
    }
    return SECSuccess;
}

PRFileDesc *
getBoundListenSocket(unsigned short port)
{
    PRFileDesc *       listen_sock;
    int                listenQueueDepth = 5 + (2 * maxThreads);
    PRStatus	       prStatus;
    PRNetAddr          addr;
    PRSocketOptionData opt;

    addr.inet.family = PR_AF_INET;
    addr.inet.ip     = PR_INADDR_ANY;
    addr.inet.port   = PR_htons(port);

    listen_sock = PR_NewTCPSocket();
    if (listen_sock == NULL) {
	errExit("PR_NewTCPSocket");
    }

    opt.option = PR_SockOpt_Nonblocking;
    opt.value.non_blocking = PR_FALSE;
    prStatus = PR_SetSocketOption(listen_sock, &opt);
    if (prStatus < 0) {
        PR_Close(listen_sock);
	errExit("PR_SetSocketOption(PR_SockOpt_Nonblocking)");
    }

    opt.option=PR_SockOpt_Reuseaddr;
    opt.value.reuse_addr = PR_TRUE;
    prStatus = PR_SetSocketOption(listen_sock, &opt);
    if (prStatus < 0) {
        PR_Close(listen_sock);
	errExit("PR_SetSocketOption(PR_SockOpt_Reuseaddr)");
    }

#ifndef WIN95
    /* Set PR_SockOpt_Linger because it helps prevent a server bind issue
     * after clean shutdown . See bug 331413 .
     * Don't do it in the WIN95 build configuration because clean shutdown is
     * not implemented, and PR_SockOpt_Linger causes a hang in ssl.sh .
     * See bug 332348 */
    opt.option=PR_SockOpt_Linger;
    opt.value.linger.polarity = PR_TRUE;
    opt.value.linger.linger = PR_SecondsToInterval(1);
    prStatus = PR_SetSocketOption(listen_sock, &opt);
    if (prStatus < 0) {
        PR_Close(listen_sock);
        errExit("PR_SetSocketOption(PR_SockOpt_Linger)");
    }
#endif

    prStatus = PR_Bind(listen_sock, &addr);
    if (prStatus < 0) {
        PR_Close(listen_sock);
	errExit("PR_Bind");
    }

    prStatus = PR_Listen(listen_sock, listenQueueDepth);
    if (prStatus < 0) {
        PR_Close(listen_sock);
	errExit("PR_Listen");
    }
    return listen_sock;
}

void
server_main(
    PRFileDesc *        listen_sock,
    int                 requestCert, 
    SECKEYPrivateKey ** privKey,
    CERTCertificate **  cert,
    const char *expectedHostNameVal)
{
    PRFileDesc *model_sock	= NULL;

    /* Now, do the accepting, here in the main thread. */
    do_accepts(listen_sock, model_sock, requestCert);

    terminateWorkerThreads();

        if (model_sock) {
            PR_Close(model_sock);
        }

}

int          numChildren;
PRProcess *  child[MAX_PROCS];

PRProcess *
haveAChild(int argc, char **argv, PRProcessAttr * attr)
{
    PRProcess *  newProcess;

    newProcess = PR_CreateProcess(argv[0], argv, NULL, attr);
    if (!newProcess) {
	errWarn("Can't create new process.");
    } else {
	child[numChildren++] = newProcess;
    }
    return newProcess;
}

int
main(int argc, char **argv)
{
    char *               progName    = NULL;
    const char *         pidFile     = NULL;
    char *               tmp;
    PRFileDesc *         listen_sock;
    int                  optionsFound = 0;
    unsigned short       port        = 0;
    SECStatus            rv;
    PRStatus             prStatus;
    PRBool               bindOnly = PR_FALSE;
    PRBool               useLocalThreads = PR_FALSE;
    PLOptState		*optstate;
    PLOptStatus          status;

    tmp = strrchr(argv[0], '/');
    tmp = tmp ? tmp + 1 : argv[0];
    progName = strrchr(tmp, '\\');
    progName = progName ? progName + 1 : tmp;

    PR_Init( PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);

    /* please keep this list of options in ASCII collating sequence.
    ** numbers, then capital letters, then lower case, alphabetical. 
    */
    optstate = PL_CreateOptState(argc, argv, 
        "Dbhi:p:t:v");
    while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
	++optionsFound;
	switch(optstate->option) {
	case 'D': noDelay = PR_TRUE; break;

        case 'b': bindOnly = PR_TRUE; break;

        case 'h': Usage(progName); exit(0); break;

	case 'i': pidFile = optstate->value; break;

	case 'p': port = PORT_Atoi(optstate->value); break;

	case 't':
	    maxThreads = PORT_Atoi(optstate->value);
	    if ( maxThreads > MAX_THREADS ) maxThreads = MAX_THREADS;
	    if ( maxThreads < MIN_THREADS ) maxThreads = MIN_THREADS;
	    break;

	case 'v': verbose++; break;

	default:
	case '?':
	    fprintf(stderr, "Unrecognized or bad option specified.\n");
	    fprintf(stderr, "Run '%s -h' for usage information.\n", progName);
	    exit(4);
	    break;
	}
    }
    PL_DestroyOptState(optstate);
    if (status == PL_OPT_BAD) {
	fprintf(stderr, "Unrecognized or bad option specified.\n");
	fprintf(stderr, "Run '%s -h' for usage information.\n", progName);
	exit(5);
    }
    if (!optionsFound) {
	Usage(progName);
	exit(51);
    } 

    /* The -b (bindOnly) option is only used by the ssl.sh test
     * script on Linux to determine whether a previous selfserv
     * process has fully died and freed the port.  (Bug 129701)
     */
    if (bindOnly) {
        listen_sock = getBoundListenSocket(port);
        if (!listen_sock) {
            exit(1);
        }
        if (listen_sock) {
            PR_Close(listen_sock);
        }
        exit(0);
    }

    if (port == 0) {
	fprintf(stderr, "Required argument 'port' must be non-zero value\n");
	exit(7);
    }

    if (pidFile) {
	FILE *tmpfile=fopen(pidFile,"w+");

	if (tmpfile) {
	    fprintf(tmpfile,"%d",getpid());
	    fclose(tmpfile);
	}
    }

    tmp = getenv("TMP");
    if (!tmp)
	tmp = getenv("TMPDIR");
    if (!tmp)
	tmp = getenv("TEMP");
    /* we're an ordinary single process server. */
    listen_sock = getBoundListenSocket(port);
    prStatus = PR_SetFDInheritable(listen_sock, PR_FALSE);
    if (prStatus != PR_SUCCESS)
        errExit("PR_SetFDInheritable");

    lm = PR_NewLogModule("TestCase");

/* allocate the array of thread slots, and launch the worker threads. */
    rv = launch_threads(&jobLoop, 0, 0, 0, useLocalThreads);

    if (rv == SECSuccess) {
	server_main(listen_sock, 0, 0, 0,
                    0);
    }

    VLOG(("selfserv: server_thread: exiting"));

    if (failedToNegotiateName) {
        fprintf(stderr, "selfserv: Failed properly negotiate server name\n");
        exit(1);
    }

    PR_Cleanup();
    printf("selfserv: normal termination\n");
    return 0;
}

