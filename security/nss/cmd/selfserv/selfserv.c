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

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>

#include "nspr.h"
#include "prio.h"
#include "prerror.h"
#include "prnetdb.h"
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

int cipherSuites[] = {
    SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA,
    SSL_FORTEZZA_DMS_WITH_RC4_128_SHA,
    SSL_RSA_WITH_RC4_128_MD5,
    SSL_RSA_WITH_3DES_EDE_CBC_SHA,
    SSL_RSA_WITH_DES_CBC_SHA,
    SSL_RSA_EXPORT_WITH_RC4_40_MD5,
    SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5,
    SSL_FORTEZZA_DMS_WITH_NULL_SHA,
    SSL_RSA_WITH_NULL_MD5,
    0
};

int ssl2CipherSuites[] = {
    SSL_EN_RC4_128_WITH_MD5,			/* A */
    SSL_EN_RC4_128_EXPORT40_WITH_MD5,		/* B */
    SSL_EN_RC2_128_CBC_WITH_MD5,		/* C */
    SSL_EN_RC2_128_CBC_EXPORT40_WITH_MD5,	/* D */
    SSL_EN_DES_64_CBC_WITH_MD5,			/* E */
    SSL_EN_DES_192_EDE3_CBC_WITH_MD5,		/* F */
    0
};

int ssl3CipherSuites[] = {
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
    0
};

/* data and structures for shutdown */
int	stopping;

int     requestCert;
int	verbose;
SECItem	bigBuf;

PRLogModuleInfo *lm;

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

"Usage: %s -n rsa_nickname -p port [-3RTmrvx] [-w password] [-t threads]\n"
"         [-i pid_file] [-c ciphers] [-d dbdir] [-f fortezza_nickname] \n"
"-3 means disable SSL v3\n"
"-T means disable TLS\n"
"-R means disable detection of rollback from TLS to SSL3\n"
"-m means test the model-socket feature of SSL_ImportFD.\n"
"-r flag is interepreted as follows:\n"
"    1 -r  means request, not require, cert on initial handshake.\n"
"    2 -r's mean request  and require, cert on initial handshake.\n"
"    3 -r's mean request, not require, cert on second handshake.\n"
"    4 -r's mean request  and require, cert on second handshake.\n"
"-t threads -- specify the number of threads to use for connections.\n"
"-v means verbose output\n"
"-x means use export policy.\n"
"-i pid_file file to write the process id of selfserve\n"
"-c ciphers   Letter(s) chosen from the following list\n"
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
"m    SSL3 RSA EXPORT WITH RC4 56 SHA\t(new)\n",
	progName);
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

static int
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
    FLUSH;
    return rv;  
}

void printSecurityInfo(PRFileDesc *fd)
{
    char * cp;	/* bulk cipher name */
    char * ip;	/* cert issuer DN */
    char * sp;	/* cert subject DN */
    int    op;	/* High, Low, Off */
    int    kp0;	/* total key bits */
    int    kp1;	/* secret key bits */
    int    result;
    SSL3Statistics * ssl3stats = SSL_GetStatistics();

    PRINTF("selfserv: %ld cache hits; %ld cache misses, %ld cache not reusable\n",
    	ssl3stats->hch_sid_cache_hits, ssl3stats->hch_sid_cache_misses,
	ssl3stats->hch_sid_cache_not_ok);

    result = SSL_SecurityStatus(fd, &op, &cp, &kp0, &kp1, &ip, &sp);
    if (result == SECSuccess) {
	PRINTF(
    "selfserv: bulk cipher %s, %d secret key bits, %d key bits, status: %d\n",
		cp, kp1, kp0, op);
	if (requestCert) {
	    PRINTF("selfserv: subject DN: %s\n"
		   "selfserv: issuer  DN: %s\n",  sp, ip);
	}
	PR_Free(cp);
	PR_Free(ip);
	PR_Free(sp);
    }
    FLUSH;
}

/**************************************************************************
** Begin thread management routines and data.
**************************************************************************/
#define MIN_THREADS 3
#define DEFAULT_THREADS 8
#define MAX_THREADS 128
static int  maxThreads = DEFAULT_THREADS;


typedef int startFn(PRFileDesc *a, PRFileDesc *b, int c);

PZLock    * threadLock;
PZCondVar * threadQ;

PZLock    * stopLock;
PZCondVar * stopQ;
static int threadCount = 0;

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

perThread *threads;

void
thread_wrapper(void * arg)
{
    perThread * slot = (perThread *)arg;

    slot->rv = (* slot->startFunc)(slot->a, slot->b, slot->c);

    /* notify the thread exit handler. */
    PZ_Lock(threadLock);
    slot->state = rs_zombie;
    --threadCount;
    PZ_NotifyAllCondVar(threadQ);
    PZ_Unlock(threadLock);
}

SECStatus
launch_thread(
    startFn    *startFunc,
    PRFileDesc *a,
    PRFileDesc *b,
    int         c)
{
    perThread * slot;
    int         i;
    static int highWaterMark = 0;
    int workToDo = 1;

    PZ_Lock(threadLock);
    /* for each perThread structure in the threads array */
    while( workToDo )  {
        for (i = 0; i < maxThreads; ++i) {
            slot = threads + i;

            /* create new thread */
            if (( slot->state == rs_idle ) || ( slot->state == rs_zombie ))  {
                slot->state = 1;
                workToDo = 0;
                slot->a = a;
                slot->b = b;
                slot->c = c;
                slot->startFunc = startFunc;
                slot->prThread = PR_CreateThread(PR_USER_THREAD, 
				thread_wrapper, slot, PR_PRIORITY_NORMAL, 
				PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD, 0);
                if (slot->prThread == NULL) {
    	            printf("selfserv: Failed to launch thread!\n");
                    slot->state = rs_idle;
    	            return SECFailure;
                } 

                highWaterMark = ( i > highWaterMark )? i : highWaterMark;
                VLOG(("selfserv: Launched thread in slot %d, highWaterMark: %d \n", 
                      i, highWaterMark ));
                FLUSH;
                ++threadCount;
                break; /* we did what we came here for, leave */
            }
        } /* end for() */
        if ( workToDo )
            PZ_WaitCondVar(threadQ, PR_INTERVAL_NO_TIMEOUT);
    } /* end while() */
    PZ_Unlock(threadLock); 

    return SECSuccess;
}

void
destroy_thread_data(void)
{
    PORT_Memset(threads, 0, sizeof threads);

    if (threadQ) {
        PZ_DestroyCondVar(threadQ);
    	threadQ = NULL;
    }
    if (threadLock) {
        PZ_DestroyLock(threadLock);
        threadLock = NULL;
    }
}

/**************************************************************************
** End   thread management routines.
**************************************************************************/

PRBool useModelSocket  = PR_FALSE;
PRBool disableSSL3     = PR_FALSE;
PRBool disableTLS      = PR_FALSE;
PRBool disableRollBack  = PR_FALSE;

static const char stopCmd[] = { "GET /stop " };
static const char outHeader[] = {
    "HTTP/1.0 200 OK\r\n"
    "Server: Generic Web Server\r\n"
    "Date: Tue, 26 Aug 1997 22:10:05 GMT\r\n"
    "Content-type: text/plain\r\n"
    "\r\n"
};

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

int 
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

int
handle_connection( 
    PRFileDesc *tcp_sock,
    PRFileDesc *model_sock,
    int         requestCert
    )
{
    PRFileDesc *       ssl_sock = NULL;
    char  *            post;
    char  *            pBuf;			/* unused space at end of buf */
    const char *       errString;
    PRStatus           status;
    int                bufRem;			/* unused bytes at end of buf */
    int                bufDat;			/* characters received in buf */
    int                newln    = 0;		/* # of consecutive newlns */
    int                firstTime = 1;
    int                i;
    int                rv;
    PRSocketOptionData opt;
    char               msgBuf[120];
    char               buf[10240];

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

    while (1) {
	newln = 0;
	i     = 0;
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
	while (i < bufDat && newln < 2) {
	    int octet = buf[i++];
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
	while (i < bufDat && newln < 3) {
	    int octet = buf[i++];
	    if (octet == '\n') {
		newln++;
	    }
	}
	if (newln == 3)
	    break;
    }

    bufDat = pBuf - buf;
    if (bufDat) do {	/* just close if no data */
	/* Have either (a) a complete get, (b) a complete post, (c) EOF */
	if (i > 0 && !strncmp(buf, stopCmd, 4)) {
	    PRFileDesc *local_file_fd = NULL;
	    PRInt32     bytes;
	    char *      pSave;
	    PRFileInfo  info;
	    char        saveChar;
	    /* try to open the file named.  
	     * If succesful, then write it to the client.
	     */
	    pSave = strpbrk(buf + 4, " \r\n");
	    if (pSave) {
		saveChar = *pSave;
		*pSave = 0;
	    }
	    status = PR_GetFileInfo(buf + 4, &info);
	    if (status == PR_SUCCESS &&
		info.type == PR_FILE_FILE &&
		info.size >= 0 &&
		NULL != (local_file_fd = PR_Open(buf + 4, PR_RDONLY, 0))) {

		bytes = PR_TransmitFile(ssl_sock, local_file_fd, outHeader,
					sizeof outHeader - 1, 
					PR_TRANSMITFILE_KEEP_OPEN,
					PR_INTERVAL_NO_TIMEOUT);
		if (bytes < 0) {
		    errString = errWarn("PR_TransmitFile");
		    i = PORT_Strlen(errString);
		    PORT_Memcpy(buf, errString, i);
		    goto send_answer;
		} else {
		    bytes -= sizeof outHeader - 1;
		    FPRINTF(stderr, 
			    "selfserv: PR_TransmitFile wrote %d bytes from %s\n",
			    bytes, buf + 4);
		}
		PR_Close(local_file_fd);
		break;
	    }
	    /* file didn't open. */
	    if (pSave) {
		*pSave = saveChar;	/* put it back. */
	    }
	}
send_answer:
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

	rv = PR_Write(ssl_sock, outHeader, (sizeof(outHeader)) - 1);
	if (rv < 0) {
	    errWarn("PR_Write");
	    break;
	}
	if (i <= 0) {	/* hit eof */
	    PORT_Sprintf(msgBuf, "Get or Post incomplete after %d bytes.\r\n",
			 bufDat);
	    rv = PR_Write(ssl_sock, msgBuf, PORT_Strlen(msgBuf));
	    if (rv < 0) {
		errWarn("PR_Write");
		break;
	    }
	} else {
	    if (verbose > 1) fwrite(buf, 1, i, stdout);	/* display it */
	    rv = PR_Write(ssl_sock, buf, i);
	    if (rv < 0) {
		errWarn("PR_Write");
		break;
	    }
	    printSecurityInfo(ssl_sock);
	    if (i < bufDat) {
		PORT_Sprintf(buf, "Discarded %d characters.\r\n", rv - i);
		rv = PR_Write(ssl_sock, buf, PORT_Strlen(buf));
		if (rv < 0) {
		    errWarn("PR_Write");
		    break;
		}
	    }
	}
	rv = PR_Write(ssl_sock, "EOF\r\n\r\n\r\n", 9);
	if (rv < 0) {
	    errWarn("PR_Write");
	    break;
	}
    } while (0);

cleanup:
    PR_Close(ssl_sock);
    VLOG(("selfserv: handle_connection: exiting\n"));

    /* do a nice shutdown if asked. */
    if (!strncmp(buf, stopCmd, strlen(stopCmd))) {
	stopping = 1;
        VLOG(("selfserv: handle_connection: stop command"));
        PZ_TraceFlush();
    }
    VLOG(("selfserv: handle_connection: exiting"));
    return SECSuccess;	/* success */
}

int
do_accepts(
    PRFileDesc *listen_sock,
    PRFileDesc *model_sock,
    int         requestCert
    )
{
    PRNetAddr   addr;

    VLOG(("selfserv: do_accepts: starting"));
    PR_SetThreadPriority( PR_GetCurrentThread(), PR_PRIORITY_HIGH);

    while (!stopping) {
	PRFileDesc *tcp_sock;
	SECStatus   result;

	FPRINTF(stderr, "\n\n\nselfserv: About to call accept.\n");
	tcp_sock = PR_Accept(listen_sock, &addr, PR_INTERVAL_NO_TIMEOUT);
	if (tcp_sock == NULL) {
	    errWarn("PR_Accept");
	    break;
	}

        VLOG(("selfserv: do_accept: Got connection\n"));
	result = launch_thread((bigBuf.data != NULL) ? 
	                        handle_fdx_connection : handle_connection, 
	                        tcp_sock, model_sock, requestCert);

	if (result != SECSuccess) {
	    PR_Close(tcp_sock);
	    break;
        }
    }

    fprintf(stderr, "selfserv: Closing listen socket.\n");
    VLOG(("selfserv: do_accepts: exiting"));
    PR_Close(listen_sock);
    return SECSuccess;
}

void
server_main(
    unsigned short      port, 
    int                 requestCert, 
    SECKEYPrivateKey ** privKey,
    CERTCertificate **  cert)
{
    PRFileDesc *listen_sock;
    PRFileDesc *model_sock	= NULL;
    int         rv;
    SSLKEAType  kea;
    PRNetAddr   addr;
    SECStatus	secStatus;
    PRSocketOptionData opt;
    int         listenQueueDepth = 5 + (2 * maxThreads);

    /* create the thread management serialization structs */
    threadLock = PZ_NewLock(nssILockSelfServ);
    threadQ   = PZ_NewCondVar(threadLock);
    stopLock = PZ_NewLock(nssILockSelfServ);
    stopQ = PZ_NewCondVar(stopLock);


    addr.inet.family = PR_AF_INET;
    addr.inet.ip     = PR_INADDR_ANY;
    addr.inet.port   = PR_htons(port);

    /* all suites except RSA_NULL_MD5 are enabled by default */

    listen_sock = PR_NewTCPSocket();
    if (listen_sock == NULL) {
	errExit("PR_NewTCPSocket");
    }

    opt.option = PR_SockOpt_Nonblocking;
    opt.value.non_blocking = PR_FALSE;
    PR_SetSocketOption(listen_sock, &opt);

    opt.option=PR_SockOpt_Reuseaddr;
    opt.value.reuse_addr = PR_TRUE;
    PR_SetSocketOption(listen_sock, &opt);

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


    rv = PR_Bind(listen_sock, &addr);
    if (rv < 0) {
	errExit("PR_Bind");
    }

    rv = PR_Listen(listen_sock, listenQueueDepth);
    if (rv < 0) {
	errExit("PR_Listen");
    }

    rv = launch_thread(do_accepts, listen_sock, model_sock, requestCert);
    if (rv != SECSuccess) {
    	PR_Close(listen_sock);
    } else {
        VLOG(("selfserv: server_thead: waiting on stopping"));
        PZ_Lock( stopLock );
        while ( !stopping && threadCount > 0 ) {
            PZ_WaitCondVar(stopQ, PR_INTERVAL_NO_TIMEOUT);
        }
        PZ_Unlock( stopLock );
	destroy_thread_data();
        VLOG(("selfserv: server_thread: exiting"));
    }

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

int
main(int argc, char **argv)
{
    char *               progName    = NULL;
    char *               nickName    = NULL;
    char *               fNickName   = NULL;
    char *               fileName    = NULL;
    char *               cipherString= NULL;
    char *               dir         = ".";
    char *               passwd      = NULL;
    char *		 pidFile    = NULL;
    char *               tmp;
    CERTCertificate *    cert   [kt_kea_size] = { NULL };
    SECKEYPrivateKey *   privKey[kt_kea_size] = { NULL };
    int                  optionsFound = 0;
    unsigned short       port        = 0;
    SECStatus            rv;
    PRBool               useExportPolicy = PR_FALSE;
    PLOptState		*optstate;
    PLOptStatus          status;


    tmp = strrchr(argv[0], '/');
    tmp = tmp ? tmp + 1 : argv[0];
    progName = strrchr(tmp, '\\');
    progName = progName ? progName + 1 : tmp;

    optstate = PL_CreateOptState(argc, argv, "RT2:3c:d:p:mn:hi:f:rt:vw:x");
    while (status = PL_GetNextOpt(optstate) == PL_OPT_OK) {
	++optionsFound;
	switch(optstate->option) {
	case '2': fileName = optstate->value; break;

	case '3': disableSSL3 = PR_TRUE; break;

	case 'R': disableRollBack = PR_TRUE; break;

	case 'T': disableTLS = PR_TRUE; break;

	case 'c': cipherString = strdup(optstate->value); break;

	case 'd': dir = optstate->value; break;

	case 'f': fNickName = optstate->value; break;

	case 'h': Usage(progName); exit(0); break;

	case 'm': useModelSocket = PR_TRUE; break;

	case 'n': nickName = optstate->value; break;

	case 'i': pidFile = optstate->value; break;

	case 'p': port = PORT_Atoi(optstate->value); break;

	case 'r': ++requestCert; break;

	case 't':
	    maxThreads = PORT_Atoi(optstate->value);
	    if ( maxThreads > MAX_THREADS ) maxThreads = MAX_THREADS;
	    if ( maxThreads < MIN_THREADS ) maxThreads = MIN_THREADS;
	    break;

	case 'v': verbose++; break;

	case 'w': passwd = optstate->value; break;

	case 'x': useExportPolicy = PR_TRUE; break;

	default:
	case '?':
	    fprintf(stderr, "Unrecognized or bad option specified.\n");
	    fprintf(stderr, "Run '%s -h' for usage information.\n", progName);
	    exit(4);
	    break;
	}
    }
    if (status == PL_OPT_BAD) {
	fprintf(stderr, "Unrecognized or bad option specified.\n");
	fprintf(stderr, "Run '%s -h' for usage information.\n", progName);
	exit(5);
    }
    if (!optionsFound) {
	Usage(progName);
	exit(51);
    } 

    /* allocate the array of thread slots */
    threads = PR_Calloc(maxThreads, sizeof(perThread));
    if ( NULL == threads )  {
        fprintf(stderr, "Oh Drat! Can't allocate the perThread array\n");
        goto mainExit;
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


    /* Call the NSPR initialization routines */
    PR_Init( PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);

    lm = PR_NewLogModule("TestCase");

    /* allocate the array of thread slots */
    threads = PR_Calloc(maxThreads, sizeof(perThread));
    if ( NULL == threads )  {
        fprintf(stderr, "Oh Drat! Can't allocate the perThread array\n");
        goto mainExit;
    }

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
	    int *cptr;
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
	    fprintf(stderr, "selfserv: Can't find Private Key for cert %s\n", nickName);
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

    rv = SSL_ConfigMPServerSIDCache(256, 0, 0, NULL);
    if (rv != SECSuccess) {
        errExit("SSL_ConfigMPServerSIDCache");
    }

    server_main(port, requestCert, privKey, cert);

mainExit:
    NSS_Shutdown();
    PR_Cleanup();
    return 0;
}
