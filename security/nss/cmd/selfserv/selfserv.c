/*
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
 */

/* -r flag is interepreted as follows:
 *	1 -r  means request, not require, on initial handshake.
 *	2 -r's mean request  and require, on initial handshake.
 *	3 -r's mean request, not require, on second handshake.
 *	4 -r's mean request  and require, on second handshake.
 */
#include <stdio.h>
#include <string.h>

#include "secutil.h"

#if defined(XP_UNIX)
#include <unistd.h>
#endif

#if defined(_WINDOWS)
#include <process.h>	/* for getpid() */
#endif

#ifdef XP_OS2_VACPP
#include <Process.h>	/* for getpid() */
#endif

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
#include "pk11func.h"
#include "secitem.h"
#include "nss.h"
#include "ssl.h"
#include "sslproto.h"

#ifndef PORT_Sprintf
#define PORT_Sprintf sprintf
#endif

#ifndef PORT_Strstr
#define PORT_Strstr strstr
#endif

#ifndef PORT_Malloc
#define PORT_Malloc PR_Malloc
#endif

#define NUM_SID_CACHE_ENTRIES 1024

static int handle_connection( PRFileDesc *, PRFileDesc *, int );

static const char envVarName[] = { SSL_ENV_VAR_NAME };
static const char inheritableSockName[] = { "SELFSERV_LISTEN_SOCKET" };


const int ssl2CipherSuites[] = {
    SSL_EN_RC4_128_WITH_MD5,			/* A */
    SSL_EN_RC4_128_EXPORT40_WITH_MD5,		/* B */
    SSL_EN_RC2_128_CBC_WITH_MD5,		/* C */
    SSL_EN_RC2_128_CBC_EXPORT40_WITH_MD5,	/* D */
    SSL_EN_DES_64_CBC_WITH_MD5,			/* E */
    SSL_EN_DES_192_EDE3_CBC_WITH_MD5,		/* F */
    0
};

const int ssl3CipherSuites[] = {
    SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA,	/* a */
    SSL_FORTEZZA_DMS_WITH_RC4_128_SHA,		/* b */
    SSL_RSA_WITH_RC4_128_MD5,			/* c */
    SSL_RSA_WITH_3DES_EDE_CBC_SHA,		/* d */
    SSL_RSA_WITH_DES_CBC_SHA,			/* e */
    SSL_RSA_EXPORT_WITH_RC4_40_MD5,		/* f */
    SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5,		/* g */
    SSL_FORTEZZA_DMS_WITH_NULL_SHA,		/* h */
    SSL_RSA_WITH_NULL_MD5,			/* i */
    SSL_RSA_FIPS_WITH_3DES_EDE_CBC_SHA,		/* j */
    SSL_RSA_FIPS_WITH_DES_CBC_SHA,		/* k */
    TLS_RSA_EXPORT1024_WITH_DES_CBC_SHA,	/* l */
    TLS_RSA_EXPORT1024_WITH_RC4_56_SHA,	        /* m */
    SSL_RSA_WITH_RC4_128_SHA,			/* n */
    TLS_DHE_DSS_WITH_RC4_128_SHA,		/* o */
    SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA,		/* p */
    SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA,		/* q */
    SSL_DHE_RSA_WITH_DES_CBC_SHA,		/* r */
    SSL_DHE_DSS_WITH_DES_CBC_SHA,		/* s */
    TLS_DHE_DSS_WITH_AES_128_CBC_SHA, 	    	/* t */
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA,       	/* u */
    TLS_RSA_WITH_AES_128_CBC_SHA,     	    	/* v */
    TLS_DHE_DSS_WITH_AES_256_CBC_SHA, 	    	/* w */
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA,       	/* x */
    TLS_RSA_WITH_AES_256_CBC_SHA,     	    	/* y */
    0
};

/* data and structures for shutdown */
static int	stopping;

static PRBool  noDelay;
static int     requestCert;
static int	verbose;
static SECItem	bigBuf;

static PRThread * acceptorThread;

static PRLogModuleInfo *lm;

/* Add custom password handler because SECU_GetModulePassword 
 * makes automation of this program next to impossible.
 */

char *
ownPasswd(PK11SlotInfo *info, PRBool retry, void *arg)
{
	char * passwd = NULL;

	if ( (!retry) && arg ) {
		passwd = PL_strdup((char *)arg);
	}

	return passwd;
}

#define PRINTF  if (verbose)  printf
#define FPRINTF if (verbose) fprintf
#define FLUSH	if (verbose) { fflush(stdout); fflush(stderr); }
#define VLOG(arg) PR_LOG(lm,PR_LOG_DEBUG,arg)

static void
Usage(const char *progName)
{
    fprintf(stderr, 

"Usage: %s -n rsa_nickname -p port [-3DRTmrvx] [-w password] [-t threads]\n"
"         [-i pid_file] [-c ciphers] [-d dbdir] [-f fortezza_nickname] \n"
"         [-M maxProcs] [-l]\n"
"-3 means disable SSL v3\n"
"-D means disable Nagle delays in TCP\n"
"-T means disable TLS\n"
"-R means disable detection of rollback from TLS to SSL3\n"
"-m means test the model-socket feature of SSL_ImportFD.\n"
"-r flag is interepreted as follows:\n"
"    1 -r  means request, not require, cert on initial handshake.\n"
"    2 -r's mean request  and require, cert on initial handshake.\n"
"    3 -r's mean request, not require, cert on second handshake.\n"
"    4 -r's mean request  and require, cert on second handshake.\n"
"-v means verbose output\n"
"-x means use export policy.\n"
"-M maxProcs tells how many processes to run in a multi-process server\n"
"-t threads -- specify the number of threads to use for connections.\n"
"-i pid_file file to write the process id of selfserve\n"
"-c ciphers   Letter(s) chosen from the following list\n"
"-l means use local threads instead of global threads"
"A    SSL2 RC4 128 WITH MD5\n"
"B    SSL2 RC4 128 EXPORT40 WITH MD5\n"
"C    SSL2 RC2 128 CBC WITH MD5\n"
"D    SSL2 RC2 128 CBC EXPORT40 WITH MD5\n"
"E    SSL2 DES 64 CBC WITH MD5\n"
"F    SSL2 DES 192 EDE3 CBC WITH MD5\n"
"\n"
"a    SSL3 FORTEZZA DMS WITH FORTEZZA CBC SHA\n"
"b    SSL3 FORTEZZA DMS WITH RC4 128 SHA\n"
"c    SSL3 RSA WITH RC4 128 MD5\n"
"d    SSL3 RSA WITH 3DES EDE CBC SHA\n"
"e    SSL3 RSA WITH DES CBC SHA\n"
"f    SSL3 RSA EXPORT WITH RC4 40 MD5\n"
"g    SSL3 RSA EXPORT WITH RC2 CBC 40 MD5\n"
"h    SSL3 FORTEZZA DMS WITH NULL SHA\n"
"i    SSL3 RSA WITH NULL MD5\n"
"j    SSL3 RSA FIPS WITH 3DES EDE CBC SHA\n"
"k    SSL3 RSA FIPS WITH DES CBC SHA\n"
"l    SSL3 RSA EXPORT WITH DES CBC SHA\t(new)\n"
"m    SSL3 RSA EXPORT WITH RC4 56 SHA\t(new)\n"
"n    SSL3 RSA WITH RC4 128 SHA\n"
"v    TLS_RSA_WITH_AES_128_CBC_SHA\n"
"y    TLS_RSA_WITH_AES_256_CBC_SHA\n"
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


/**************************************************************************
** 
** Routines for disabling SSL ciphers.
**
**************************************************************************/

void
disableAllSSLCiphers(void)
{
    const PRUint16 *cipherSuites = SSL_ImplementedCiphers;
    int             i            = SSL_NumImplementedCiphers;
    SECStatus       rv;

    /* disable all the SSL3 cipher suites */
    while (--i >= 0) {
	PRUint16 suite = cipherSuites[i];
        rv = SSL_CipherPrefSetDefault(suite, PR_FALSE);
	if (rv != SECSuccess) {
	    printf("SSL_CipherPrefSetDefault didn't like value 0x%04x (i = %d)\n",
	    	   suite, i);
	    errWarn("SSL_CipherPrefSetDefault");
	    exit(2);
	}
    }
}

static SECStatus
mySSLAuthCertificate(void *arg, PRFileDesc *fd, PRBool checkSig,
		     PRBool isServer)
{
    SECStatus rv;
    CERTCertificate *    peerCert;

    peerCert = SSL_PeerCertificate(fd);

    PRINTF("selfserv: Subject: %s\nselfserv: Issuer : %s\n",
           peerCert->subjectName, peerCert->issuerName);

    rv = SSL_AuthCertificate(arg, fd, checkSig, isServer);

    if (rv == SECSuccess) {
	fputs("selfserv: -- SSL3: Certificate Validated.\n", stderr);
    } else {
    	int err = PR_GetError();
	FPRINTF(stderr, "selfserv: -- SSL3: Certificate Invalid, err %d.\n%s\n", 
                err, SECU_Strerror(err));
    }
    CERT_DestroyCertificate(peerCert);
    FLUSH;
    return rv;  
}

void 
printSecurityInfo(PRFileDesc *fd)
{
    CERTCertificate * cert      = NULL;
    SSL3Statistics *  ssl3stats = SSL_GetStatistics();
    SECStatus         result;
    SSLChannelInfo    channel;
    SSLCipherSuiteInfo suite;

    PRINTF(
    	"selfserv: %ld cache hits; %ld cache misses, %ld cache not reusable\n",
    	ssl3stats->hch_sid_cache_hits, ssl3stats->hch_sid_cache_misses,
	ssl3stats->hch_sid_cache_not_ok);

    result = SSL_GetChannelInfo(fd, &channel, sizeof channel);
    if (result == SECSuccess && 
        channel.length == sizeof channel && 
	channel.cipherSuite) {
	result = SSL_GetCipherSuiteInfo(channel.cipherSuite, 
					&suite, sizeof suite);
	if (result == SECSuccess) {
	    FPRINTF(stderr, 
	    "selfserv: SSL version %d.%d using %d-bit %s with %d-bit %s MAC\n",
	       channel.protocolVersion >> 8, channel.protocolVersion & 0xff,
	       suite.effectiveKeyBits, suite.symCipherName, 
	       suite.macBits, suite.macAlgorithmName);
	    FPRINTF(stderr, 
	    "selfserv: Server Auth: %d-bit %s, Key Exchange: %d-bit %s\n",
	       channel.authKeyBits, suite.authAlgorithmName,
	       channel.keaKeyBits,  suite.keaTypeName);
    	}
    }
    if (requestCert)
	cert = SSL_RevealCert(fd);
    if (cert) {
	char * ip = CERT_NameToAscii(&cert->issuer);
	char * sp = CERT_NameToAscii(&cert->subject);
        if (sp) {
	    FPRINTF(stderr, "selfserv: subject DN: %s\n", sp);
	    PR_Free(sp);
	}
        if (ip) {
	    FPRINTF(stderr, "selfserv: issuer  DN: %s\n", ip);
	    PR_Free(ip);
	}
	CERT_DestroyCertificate(cert);
	cert = NULL;
    }
    FLUSH;
}

/**************************************************************************
** Begin thread management routines and data.
**************************************************************************/
#define MIN_THREADS 3
#define DEFAULT_THREADS 8
#define MAX_THREADS 128
#define MAX_PROCS 25
static int  maxThreads = DEFAULT_THREADS;


typedef struct jobStr {
    PRCList     link;
    PRFileDesc *tcp_sock;
    PRFileDesc *model_sock;
    int         requestCert;
} JOB;

static PZLock    * qLock; /* this lock protects all data immediately below */
static PZCondVar * jobQNotEmptyCv;
static PZCondVar * freeListNotEmptyCv;
static PZCondVar * threadCountChangeCv;
static int  threadCount;
static int  qCount;
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
    PZ_Unlock(qLock); 

    DESTROY_CONDVAR(jobQNotEmptyCv);
    DESTROY_CONDVAR(freeListNotEmptyCv);
    DESTROY_CONDVAR(threadCountChangeCv);

    DESTROY_LOCK(qLock);
    PR_Free(jobTable);
    PR_Free(threads);
}


/**************************************************************************
** End   thread management routines.
**************************************************************************/

PRBool useModelSocket  = PR_FALSE;
PRBool disableSSL3     = PR_FALSE;
PRBool disableTLS      = PR_FALSE;
PRBool disableRollBack  = PR_FALSE;

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

#ifdef FULL_DUPLEX_CAPABLE

struct lockedVarsStr {
    PZLock *	lock;
    int		count;
    int		waiters;
    PZCondVar *	condVar;
};

typedef struct lockedVarsStr lockedVars;

void 
lockedVars_Init( lockedVars * lv)
{
    lv->count   = 0;
    lv->waiters = 0;
    lv->lock    = PZ_NewLock(nssILockSelfServ);
    lv->condVar = PZ_NewCondVar(lv->lock);
}

void
lockedVars_Destroy( lockedVars * lv)
{
    PZ_DestroyCondVar(lv->condVar);
    lv->condVar = NULL;

    PZ_DestroyLock(lv->lock);
    lv->lock = NULL;
}

void
lockedVars_WaitForDone(lockedVars * lv)
{
    PZ_Lock(lv->lock);
    while (lv->count > 0) {
    	PZ_WaitCondVar(lv->condVar, PR_INTERVAL_NO_TIMEOUT);
    }
    PZ_Unlock(lv->lock);
}

int	/* returns count */
lockedVars_AddToCount(lockedVars * lv, int addend)
{
    int rv;

    PZ_Lock(lv->lock);
    rv = lv->count += addend;
    if (rv <= 0) {
	PZ_NotifyCondVar(lv->condVar);
    }
    PZ_Unlock(lv->lock);
    return rv;
}

int
do_writes(
    PRFileDesc *	ssl_sock,
    PRFileDesc *	model_sock,
    int         	requestCert
    )
{
    int			sent  = 0;
    int 		count = 0;
    lockedVars *	lv = (lockedVars *)model_sock;

    VLOG(("selfserv: do_writes: starting"));
    while (sent < bigBuf.len) {

	count = PR_Write(ssl_sock, bigBuf.data + sent, bigBuf.len - sent);
	if (count < 0) {
	    errWarn("PR_Write bigBuf");
	    break;
	}
	FPRINTF(stderr, "selfserv: PR_Write wrote %d bytes from bigBuf\n", count );
	sent += count;
    }
    if (count >= 0) {	/* last write didn't fail. */
    	PR_Shutdown(ssl_sock, PR_SHUTDOWN_SEND);
    }

    /* notify the reader that we're done. */
    lockedVars_AddToCount(lv, -1);
    FLUSH;
    VLOG(("selfserv: do_writes: exiting"));
    return (sent < bigBuf.len) ? SECFailure : SECSuccess;
}

static int 
handle_fdx_connection(
    PRFileDesc *       tcp_sock,
    PRFileDesc *       model_sock,
    int                requestCert
    )
{
    PRFileDesc *       ssl_sock	= NULL;
    SECStatus          result;
    int                firstTime = 1;
    lockedVars         lv;
    PRSocketOptionData opt;
    char               buf[10240];


    VLOG(("selfserv: handle_fdx_connection: starting"));
    opt.option             = PR_SockOpt_Nonblocking;
    opt.value.non_blocking = PR_FALSE;
    PR_SetSocketOption(tcp_sock, &opt);

    if (useModelSocket && model_sock) {
	SECStatus rv;
	ssl_sock = SSL_ImportFD(model_sock, tcp_sock);
	if (!ssl_sock) {
	    errWarn("SSL_ImportFD with model");
	    goto cleanup;
	}
	rv = SSL_ResetHandshake(ssl_sock, /* asServer */ 1);
	if (rv != SECSuccess) {
	    errWarn("SSL_ResetHandshake");
	    goto cleanup;
	}
    } else {
	ssl_sock = tcp_sock;
    }

    lockedVars_Init(&lv);
    lockedVars_AddToCount(&lv, 1);

    /* Attempt to launch the writer thread. */
    result = launch_thread(do_writes, ssl_sock, (PRFileDesc *)&lv, 
                           requestCert);

    if (result == SECSuccess) 
      do {
	/* do reads here. */
	int count;
	count = PR_Read(ssl_sock, buf, sizeof buf);
	if (count < 0) {
	    errWarn("FDX PR_Read");
	    break;
	}
	FPRINTF(stderr, "selfserv: FDX PR_Read read %d bytes.\n", count );
	if (firstTime) {
	    firstTime = 0;
	    printSecurityInfo(ssl_sock);
	}
    } while (lockedVars_AddToCount(&lv, 0) > 0);

    /* Wait for writer to finish */
    lockedVars_WaitForDone(&lv);
    lockedVars_Destroy(&lv);
    FLUSH;

cleanup:
    if (ssl_sock)
	PR_Close(ssl_sock);
    else
	PR_Close(tcp_sock);

    VLOG(("selfserv: handle_fdx_connection: exiting"));
    return SECSuccess;
}

#endif

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
    memset(buf, 0, sizeof buf);

    VLOG(("selfserv: handle_connection: starting"));
    opt.option             = PR_SockOpt_Nonblocking;
    opt.value.non_blocking = PR_FALSE;
    PR_SetSocketOption(tcp_sock, &opt);

    VLOG(("selfserv: handle_connection: starting\n"));
    if (useModelSocket && model_sock) {
	SECStatus rv;
	ssl_sock = SSL_ImportFD(model_sock, tcp_sock);
	if (!ssl_sock) {
	    errWarn("SSL_ImportFD with model");
	    goto cleanup;
	}
	rv = SSL_ResetHandshake(ssl_sock, /* asServer */ 1);
	if (rv != SECSuccess) {
	    errWarn("SSL_ResetHandshake");
	    goto cleanup;
	}
    } else {
	ssl_sock = tcp_sock;
    }

    if (noDelay) {
	opt.option         = PR_SockOpt_NoDelay;
	opt.value.no_delay = PR_TRUE;
	status = PR_SetSocketOption(ssl_sock, &opt);
	if (status != PR_SUCCESS) {
	    errWarn("PR_SetSocketOption(PR_SockOpt_NoDelay, PR_TRUE)");
	    PR_Close(ssl_sock);
	    return SECFailure;
	}
    }

    while (1) {
	newln = 0;
	reqLen     = 0;
	rv = PR_Read(ssl_sock, pBuf, bufRem);
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
	if (firstTime) {
	    firstTime = 0;
	    printSecurityInfo(ssl_sock);
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
	     * If succesful, then write it to the client.
	     */
	    fnEnd = strpbrk(fnBegin, " \r\n");
	    if (fnEnd) {
		int fnLen = fnEnd - fnBegin;
		if (fnLen < sizeof fileName) {
		    strncpy(fileName, fnBegin, fnLen);
		    fileName[fnLen] = 0;	/* null terminate */
		    status = PR_GetFileInfo(fileName, &info);
		    if (status == PR_SUCCESS &&
			info.type == PR_FILE_FILE &&
			info.size >= 0 ) {
			local_file_fd = PR_Open(fileName, PR_RDONLY, 0);
		    }
		}
	    }
	}
	/* if user has requested client auth in a subsequent handshake,
	 * do it here.
	 */
	if (requestCert > 2) { /* request cert was 3 or 4 */
	    CERTCertificate *  cert =  SSL_PeerCertificate(ssl_sock);
	    if (cert) {
		CERT_DestroyCertificate(cert);
	    } else {
		rv = SSL_OptionSet(ssl_sock, SSL_REQUEST_CERTIFICATE, 1);
		if (rv < 0) {
		    errWarn("second SSL_OptionSet SSL_REQUEST_CERTIFICATE");
		    break;
		}
		rv = SSL_OptionSet(ssl_sock, SSL_REQUIRE_CERTIFICATE, 
				(requestCert == 4));
		if (rv < 0) {
		    errWarn("second SSL_OptionSet SSL_REQUIRE_CERTIFICATE");
		    break;
		}
		rv = SSL_ReHandshake(ssl_sock, PR_TRUE);
		if (rv != 0) {
		    errWarn("SSL_ReHandshake");
		    break;
		}
		rv = SSL_ForceHandshake(ssl_sock);
		if (rv < 0) {
		    errWarn("SSL_ForceHandshake");
		    break;
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
	    if (errLen > sizeof msgBuf - 1) 
	    	errLen = sizeof msgBuf - 1;
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

/*	    printSecurityInfo(ssl_sock); */
	}

	iovs[numIOVs].iov_base = (char *)EOFmsg;
	iovs[numIOVs].iov_len  = sizeof EOFmsg - 1;
	numIOVs++;

	rv = PR_Writev(ssl_sock, iovs, numIOVs, PR_INTERVAL_NO_TIMEOUT);
	if (rv < 0) {
	    errWarn("PR_Writev");
	    break;
	}
    } while (0);

cleanup:
    PR_Close(ssl_sock);
    if (local_file_fd)
	PR_Close(local_file_fd);
    VLOG(("selfserv: handle_connection: exiting\n"));

    /* do a nice shutdown if asked. */
    if (!strncmp(buf, stopCmd, sizeof stopCmd - 1)) {
	stopping = 1;
        VLOG(("selfserv: handle_connection: stop command"));
	PR_Interrupt(acceptorThread);
        PZ_TraceFlush();
    }
    VLOG(("selfserv: handle_connection: exiting"));
    return SECSuccess;	/* success */
}

SECStatus
do_accepts(
    PRFileDesc *listen_sock,
    PRFileDesc *model_sock,
    int         requestCert
    )
{
    PRNetAddr   addr;
    PRErrorCode  perr;

    VLOG(("selfserv: do_accepts: starting"));
    PR_SetThreadPriority( PR_GetCurrentThread(), PR_PRIORITY_HIGH);

    acceptorThread = PR_GetCurrentThread();
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
	    PR_Close(tcp_sock);
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
    PR_Close(listen_sock);
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
	errExit("PR_SetSocketOption(PR_SockOpt_Nonblocking)");
    }

    opt.option=PR_SockOpt_Reuseaddr;
    opt.value.reuse_addr = PR_TRUE;
    prStatus = PR_SetSocketOption(listen_sock, &opt);
    if (prStatus < 0) {
	errExit("PR_SetSocketOption(PR_SockOpt_Reuseaddr)");
    }

    prStatus = PR_Bind(listen_sock, &addr);
    if (prStatus < 0) {
	errExit("PR_Bind");
    }

    prStatus = PR_Listen(listen_sock, listenQueueDepth);
    if (prStatus < 0) {
	errExit("PR_Listen");
    }
    return listen_sock;
}

void
server_main(
    PRFileDesc *        listen_sock,
    int                 requestCert, 
    SECKEYPrivateKey ** privKey,
    CERTCertificate **  cert)
{
    PRFileDesc *model_sock	= NULL;
    int         rv;
    SSLKEAType  kea;
    SECStatus	secStatus;

    if (useModelSocket) {
    	model_sock = PR_NewTCPSocket();
	if (model_sock == NULL) {
	    errExit("PR_NewTCPSocket on model socket");
	}
	model_sock = SSL_ImportFD(NULL, model_sock);
	if (model_sock == NULL) {
	    errExit("SSL_ImportFD");
	}
    } else {
	model_sock = listen_sock = SSL_ImportFD(NULL, listen_sock);
	if (listen_sock == NULL) {
	    errExit("SSL_ImportFD");
	}
    }

    /* do SSL configuration. */
    /* all suites except RSA_NULL_MD5 are enabled by default */

#if 0
    /* This is supposed to be true by default.
    ** Setting it explicitly should not be necessary.
    ** Let's test and make sure that's true.
    */
    rv = SSL_OptionSet(model_sock, SSL_SECURITY, 1);
    if (rv < 0) {
	errExit("SSL_OptionSet SSL_SECURITY");
    }
#endif

    rv = SSL_OptionSet(model_sock, SSL_ENABLE_SSL3, !disableSSL3);
    if (rv != SECSuccess) {
	errExit("error enabling SSLv3 ");
    }

    rv = SSL_OptionSet(model_sock, SSL_ENABLE_TLS, !disableTLS);
    if (rv != SECSuccess) {
	errExit("error enabling TLS ");
    }

    rv = SSL_OptionSet(model_sock, SSL_ROLLBACK_DETECTION, !disableRollBack);
    if (rv != SECSuccess) {
	errExit("error enabling RollBack detection ");
    }

    for (kea = kt_rsa; kea < kt_kea_size; kea++) {
	if (cert[kea] != NULL) {
	    secStatus = SSL_ConfigSecureServer(model_sock, 
	    		cert[kea], privKey[kea], kea);
	    if (secStatus != SECSuccess)
		errExit("SSL_ConfigSecureServer");
	}
    }

    if (bigBuf.data) { /* doing FDX */
	rv = SSL_OptionSet(model_sock, SSL_ENABLE_FDX, 1);
	if (rv < 0) {
	    errExit("SSL_OptionSet SSL_ENABLE_FDX");
	}
    }

    /* This cipher is not on by default. The Acceptance test
     * would like it to be. Turn this cipher on.
     */

    secStatus = SSL_CipherPrefSetDefault( SSL_RSA_WITH_NULL_MD5, PR_TRUE);
    if ( secStatus != SECSuccess ) {
	errExit("SSL_CipherPrefSetDefault:SSL_RSA_WITH_NULL_MD5");
    }


    if (requestCert) {
	SSL_AuthCertificateHook(model_sock, mySSLAuthCertificate, 
	                        (void *)CERT_GetDefaultCertDB());
	if (requestCert <= 2) { 
	    rv = SSL_OptionSet(model_sock, SSL_REQUEST_CERTIFICATE, 1);
	    if (rv < 0) {
		errExit("first SSL_OptionSet SSL_REQUEST_CERTIFICATE");
	    }
	    rv = SSL_OptionSet(model_sock, SSL_REQUIRE_CERTIFICATE, 
	                    (requestCert == 2));
	    if (rv < 0) {
		errExit("first SSL_OptionSet SSL_REQUIRE_CERTIFICATE");
	    }
	}
    }
    /* end of ssl configuration. */


    /* Now, do the accepting, here in the main thread. */
    rv = do_accepts(listen_sock, model_sock, requestCert);

    terminateWorkerThreads();

    if (useModelSocket && model_sock) {
    	PR_Close(model_sock);
    }

}

SECStatus
readBigFile(const char * fileName)
{
    PRFileInfo  info;
    PRStatus	status;
    SECStatus	rv	= SECFailure;
    int		count;
    int		hdrLen;
    PRFileDesc *local_file_fd = NULL;

    status = PR_GetFileInfo(fileName, &info);

    if (status == PR_SUCCESS &&
	info.type == PR_FILE_FILE &&
	info.size > 0 &&
	NULL != (local_file_fd = PR_Open(fileName, PR_RDONLY, 0))) {

	hdrLen      = PORT_Strlen(outHeader);
	bigBuf.len  = hdrLen + info.size;
	bigBuf.data = PORT_Malloc(bigBuf.len + 4095);
	if (!bigBuf.data) {
	    errWarn("PORT_Malloc");
	    goto done;
	}

	PORT_Memcpy(bigBuf.data, outHeader, hdrLen);

	count = PR_Read(local_file_fd, bigBuf.data + hdrLen, info.size);
	if (count != info.size) {
	    errWarn("PR_Read local file");
	    goto done;
	}
	rv = SECSuccess;
done:
	PR_Close(local_file_fd);
    }
    return rv;
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

void
beAGoodParent(int argc, char **argv, int maxProcs, PRFileDesc * listen_sock)
{
    PRProcess *     newProcess;
    PRProcessAttr * attr;
    int             i;
    PRInt32         exitCode;
    PRStatus        rv;

    rv = PR_SetFDInheritable(listen_sock, PR_TRUE);
    if (rv != PR_SUCCESS)
	errExit("PR_SetFDInheritable");

    attr = PR_NewProcessAttr();
    if (!attr)
	errExit("PR_NewProcessAttr");

    rv = PR_ProcessAttrSetInheritableFD(attr, listen_sock, inheritableSockName);
    if (rv != PR_SUCCESS)
	errExit("PR_ProcessAttrSetInheritableFD");

    for (i = 0; i < maxProcs; ++i) {
	newProcess = haveAChild(argc, argv, attr);
	if (!newProcess) 
	    break;
    }

    rv = PR_SetFDInheritable(listen_sock, PR_FALSE);
    if (rv != PR_SUCCESS)
	errExit("PR_SetFDInheritable");

    while (numChildren > 0) {
	newProcess = child[numChildren - 1];
	PR_WaitProcess(newProcess, &exitCode);
	fprintf(stderr, "Child %d exited with exit code %x\n", 
		numChildren, exitCode);
	numChildren--;
    }
    exit(0);
}

#ifdef DEBUG_nelsonb
void
WaitForDebugger(void)
{

    int waiting       = 12;
    int myPid         = _getpid();
    PRIntervalTime    nrval = PR_SecondsToInterval(5);

    while (waiting) {
    	printf("child %d is waiting to be debugged!\n", myPid);
	PR_Sleep(nrval); 
	--waiting;
    }
}
#endif

int
main(int argc, char **argv)
{
    char *               progName    = NULL;
    char *               nickName    = NULL;
    char *               fNickName   = NULL;
    const char *         fileName    = NULL;
    char *               cipherString= NULL;
    const char *         dir         = ".";
    char *               passwd      = NULL;
    const char *         pidFile     = NULL;
    char *               tmp;
    char *               envString;
    PRFileDesc *         listen_sock;
    CERTCertificate *    cert   [kt_kea_size] = { NULL };
    SECKEYPrivateKey *   privKey[kt_kea_size] = { NULL };
    int                  optionsFound = 0;
    int                  maxProcs     = 1;
    unsigned short       port        = 0;
    SECStatus            rv;
    PRStatus             prStatus;
    PRBool               useExportPolicy = PR_FALSE;
    PRBool               useLocalThreads = PR_FALSE;
    PLOptState		*optstate;
    PLOptStatus          status;


    tmp = strrchr(argv[0], '/');
    tmp = tmp ? tmp + 1 : argv[0];
    progName = strrchr(tmp, '\\');
    progName = progName ? progName + 1 : tmp;

    PR_Init( PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);

    optstate = PL_CreateOptState(argc, argv, "2:3DM:RTc:d:p:mn:hi:f:rt:vw:xl");
    while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
	++optionsFound;
	switch(optstate->option) {
	case '2': fileName = optstate->value; break;

	case '3': disableSSL3 = PR_TRUE; break;

	case 'D': noDelay = PR_TRUE; break;

        case 'l': useLocalThreads = PR_TRUE; break;

	case 'M': 
	    maxProcs = PORT_Atoi(optstate->value); 
	    if (maxProcs < 1)         maxProcs = 1;
	    if (maxProcs > MAX_PROCS) maxProcs = MAX_PROCS;
	    break;

	case 'R': disableRollBack = PR_TRUE; break;

	case 'T': disableTLS = PR_TRUE; break;

	case 'c': cipherString = strdup(optstate->value); break;

	case 'd': dir = optstate->value; break;

	case 'f': fNickName = strdup(optstate->value); break;

	case 'h': Usage(progName); exit(0); break;

	case 'm': useModelSocket = PR_TRUE; break;

	case 'n': nickName = strdup(optstate->value); break;

	case 'i': pidFile = optstate->value; break;

	case 'p': port = PORT_Atoi(optstate->value); break;

	case 'r': ++requestCert; break;

	case 't':
	    maxThreads = PORT_Atoi(optstate->value);
	    if ( maxThreads > MAX_THREADS ) maxThreads = MAX_THREADS;
	    if ( maxThreads < MIN_THREADS ) maxThreads = MIN_THREADS;
	    break;

	case 'v': verbose++; break;

	case 'w': passwd = strdup(optstate->value); break;

	case 'x': useExportPolicy = PR_TRUE; break;

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

    if ((nickName == NULL) && (fNickName == NULL)) {
	fprintf(stderr, "Required arg '-n' (rsa nickname) not supplied.\n");
	fprintf(stderr, "Run '%s -h' for usage information.\n", progName);
        exit(6);
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

    envString = getenv(envVarName);
    tmp = getenv("TMP");
    if (!tmp)
	tmp = getenv("TMPDIR");
    if (!tmp)
	tmp = getenv("TEMP");
    if (envString) {
	/* we're one of the children in a multi-process server. */
	listen_sock = PR_GetInheritedFD(inheritableSockName);
	if (!listen_sock)
	    errExit("PR_GetInheritedFD");
#ifndef WINNT
	/* we can't do this on NT because it breaks NSPR and
	PR_Accept will fail on the socket in the child process if
	the socket state is change to non inheritable
	It is however a security issue to leave it accessible,
	but it is OK for a test server such as selfserv.
	NSPR should fix it eventually . see bugzilla 101617
	and 102077
	*/
	prStatus = PR_SetFDInheritable(listen_sock, PR_FALSE);
	if (prStatus != PR_SUCCESS)
	    errExit("PR_SetFDInheritable");
#endif
#ifdef DEBUG_nelsonb
	WaitForDebugger();
#endif
	rv = SSL_InheritMPServerSIDCache(envString);
	if (rv != SECSuccess)
	    errExit("SSL_InheritMPServerSIDCache");
    } else if (maxProcs > 1) {
	/* we're going to be the parent in a multi-process server.  */
	listen_sock = getBoundListenSocket(port);
	rv = SSL_ConfigMPServerSIDCache(NUM_SID_CACHE_ENTRIES, 0, 0, tmp);
	if (rv != SECSuccess)
	    errExit("SSL_ConfigMPServerSIDCache");
	beAGoodParent(argc, argv, maxProcs, listen_sock);
	exit(99); /* should never get here */
    } else {
	/* we're an ordinary single process server. */
	listen_sock = getBoundListenSocket(port);
	prStatus = PR_SetFDInheritable(listen_sock, PR_FALSE);
	if (prStatus != PR_SUCCESS)
	    errExit("PR_SetFDInheritable");
	rv = SSL_ConfigServerSessionIDCache(NUM_SID_CACHE_ENTRIES, 0, 0, tmp);
	if (rv != SECSuccess)
	    errExit("SSL_ConfigServerSessionIDCache");
    }

    lm = PR_NewLogModule("TestCase");

    if (fileName)
    	readBigFile(fileName);

    /* set our password function */
    PK11_SetPasswordFunc( passwd ? ownPasswd : SECU_GetModulePassword);

    /* Call the libsec initialization routines */
    rv = NSS_Init(dir);
    if (rv != SECSuccess) {
    	fputs("NSS_Init failed.\n", stderr);
		exit(8);
    }

    /* set the policy bits true for all the cipher suites. */
    if (useExportPolicy)
	NSS_SetExportPolicy();
    else
	NSS_SetDomesticPolicy();

    /* all the SSL2 and SSL3 cipher suites are enabled by default. */
    if (cipherString) {
    	int ndx;

	/* disable all the ciphers, then enable the ones we want. */
	disableAllSSLCiphers();

	while (0 != (ndx = *cipherString++)) {
	    const int *cptr;
	    int  cipher;

	    if (! isalpha(ndx)) {
		fprintf(stderr, 
			"Non-alphabetic char in cipher string (-c arg).\n");
		exit(9);
	    }
	    cptr = islower(ndx) ? ssl3CipherSuites : ssl2CipherSuites;
	    for (ndx &= 0x1f; (cipher = *cptr++) != 0 && --ndx > 0; ) 
		/* do nothing */;
	    if (cipher) {
		SECStatus status;
		status = SSL_CipherPrefSetDefault(cipher, SSL_ALLOWED);
		if (status != SECSuccess) 
		    SECU_PrintError(progName, "SSL_CipherPrefSet()");
	    }
	}
    }

    if (nickName) {
	cert[kt_rsa] = PK11_FindCertFromNickname(nickName, passwd);
	if (cert[kt_rsa] == NULL) {
	    fprintf(stderr, "selfserv: Can't find certificate %s\n", nickName);
	    exit(10);
	}
	privKey[kt_rsa] = PK11_FindKeyByAnyCert(cert[kt_rsa], passwd);
	if (privKey[kt_rsa] == NULL) {
	    fprintf(stderr, "selfserv: Can't find Private Key for cert %s\n", 
	            nickName);
	    exit(11);
	}
    }
    if (fNickName) {
	cert[kt_fortezza] = PK11_FindCertFromNickname(fNickName, NULL);
	if (cert[kt_fortezza] == NULL) {
	    fprintf(stderr, "selfserv: Can't find certificate %s\n", fNickName);
	    exit(12);
	}
	privKey[kt_fortezza] = PK11_FindKeyByAnyCert(cert[kt_fortezza], NULL);
    }

    /* allocate the array of thread slots, and launch the worker threads. */
    rv = launch_threads(&jobLoop, 0, 0, requestCert, useLocalThreads);

    if ( rv == SECSuccess) {
	server_main(listen_sock, requestCert, privKey, cert);
    }

    VLOG(("selfserv: server_thread: exiting"));

    NSS_Shutdown();
    PR_Cleanup();
    return 0;
}
