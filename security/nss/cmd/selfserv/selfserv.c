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

int	stopping;
int	verbose;
SECItem	bigBuf;

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

static void
Usage(const char *progName)
{
    fprintf(stderr, 

"Usage: %s -n rsa_nickname -p port [-3RTmrvx] [-w password]\n"
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
    exit(1);
}

static void
networkStart(void)
{
#if defined(XP_WIN) && !defined(NSPR20)

    WORD wVersionRequested;  
    WSADATA wsaData; 
    int err; 
    wVersionRequested = MAKEWORD(1, 1); 
 
    err = WSAStartup(wVersionRequested, &wsaData); 
 
    if (err != 0) {
	/* Tell the user that we couldn't find a useable winsock.dll. */ 
	fputs("WSAStartup failed!\n", stderr);
	exit(1);
    }

/* Confirm that the Windows Sockets DLL supports 1.1.*/ 
/* Note that if the DLL supports versions greater */ 
/* than 1.1 in addition to 1.1, it will still return */ 
/* 1.1 in wVersion since that is the version we */ 
/* requested. */ 
 
    if ( LOBYTE( wsaData.wVersion ) != 1 || 
         HIBYTE( wsaData.wVersion ) != 1 ) { 
	/* Tell the user that we couldn't find a useable winsock.dll. */ 
	fputs("wrong winsock version\n", stderr);
	WSACleanup(); 
	exit(1); 
    } 
    /* The Windows Sockets DLL is acceptable. Proceed. */ 

#endif
}

static void
networkEnd(void)
{
#if defined(XP_WIN) && !defined(NSPR20)
    WSACleanup();
#endif
}

static const char *
errWarn(char * funcString)
{
    PRErrorCode  perr      = PR_GetError();
    const char * errString = SECU_Strerror(perr);

    fprintf(stderr, "exit after %s with error %d:\n%s\n",
            funcString, perr, errString);
    return errString;
}

static void
errExit(char * funcString)
{
#if defined (XP_WIN) && !defined(NSPR20)
    int          err;
    LPVOID       lpMsgBuf;

    err = WSAGetLastError();
 
    FormatMessage(
	FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
	NULL,
	err,
	MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
	(LPTSTR) &lpMsgBuf,
	0,
	NULL 
    );

    /* Display the string. */
  /*MessageBox( NULL, lpMsgBuf, "GetLastError", MB_OK|MB_ICONINFORMATION ); */
    fprintf(stderr, "%s\n", lpMsgBuf);

    /* Free the buffer. */
    LocalFree( lpMsgBuf );
#endif

    errWarn(funcString);
    exit(1);
}

void
disableSSL2Ciphers(void)
{
    int i;

    /* disable all the SSL2 cipher suites */
    for (i = 0; ssl2CipherSuites[i] != 0;  ++i) {
        SSL_EnableCipher(ssl2CipherSuites[i], SSL_NOT_ALLOWED);
    }
}

void
disableSSL3Ciphers(void)
{
    int i;

    /* disable all the SSL3 cipher suites */
    for (i = 0; ssl3CipherSuites[i] != 0;  ++i) {
        SSL_EnableCipher(ssl3CipherSuites[i], SSL_NOT_ALLOWED);
    }
}

static int
mySSLAuthCertificate(void *arg, PRFileDesc *fd, PRBool checkSig,
		     PRBool isServer)
{
    SECStatus rv;
    CERTCertificate *    peerCert;

    peerCert = SSL_PeerCertificate(fd);

    PRINTF("Subject: %s\nIssuer : %s\n",
           peerCert->subjectName, peerCert->issuerName);

    rv = SSL_AuthCertificate(arg, fd, checkSig, isServer);

    if (rv == SECSuccess) {
	fputs("-- SSL3: Certificate Validated.\n", stderr);
    } else {
    	int err = PR_GetError();
	FPRINTF(stderr, "-- SSL3: Certificate Invalid, err %d.\n%s\n", 
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

/* statistics from ssl3_SendClientHello (sch) */
extern long ssl3_sch_sid_cache_hits;
extern long ssl3_sch_sid_cache_misses;
extern long ssl3_sch_sid_cache_not_ok;

/* statistics from ssl3_HandleServerHello (hsh) */
extern long ssl3_hsh_sid_cache_hits;
extern long ssl3_hsh_sid_cache_misses;
extern long ssl3_hsh_sid_cache_not_ok;

/* statistics from ssl3_HandleClientHello (hch) */
extern long ssl3_hch_sid_cache_hits;
extern long ssl3_hch_sid_cache_misses;
extern long ssl3_hch_sid_cache_not_ok;

    result = SSL_SecurityStatus(fd, &op, &cp, &kp0, &kp1, &ip, &sp);
    if (result != SECSuccess)
    	return;
    PRINTF("bulk cipher %s, %d secret key bits, %d key bits, status: %d\n"
           "subject DN: %s\n"
	   "issuer  DN: %s\n", cp, kp1, kp0, op, sp, ip);
    PR_Free(cp);
    PR_Free(ip);
    PR_Free(sp);

    PRINTF("%ld cache hits; %ld cache misses, %ld cache not reusable\n",
    	ssl3_hch_sid_cache_hits, ssl3_hch_sid_cache_misses,
	ssl3_hch_sid_cache_not_ok);
    FLUSH;

}

/**************************************************************************
** Begin thread management routines and data.
**************************************************************************/

#define MAX_THREADS 32

typedef int startFn(PRFileDesc *a, PRFileDesc *b, int c);

PRLock    * threadLock;
PRCondVar * threadStartQ;
PRCondVar * threadEndQ;

int         numUsed;
int         numRunning;

typedef enum { rs_idle = 0, rs_running = 1, rs_zombie = 2 } runState;

typedef struct perThreadStr {
    PRFileDesc *a;
    PRFileDesc *b;
    int         c;
    int         rv;
    startFn  *  startFunc;
    PRThread *  prThread;
    PRBool	inUse;
    runState	running;
} perThread;

perThread threads[MAX_THREADS];

void
thread_wrapper(void * arg)
{
    perThread * slot = (perThread *)arg;

    /* wait for parent to finish launching us before proceeding. */
    PR_Lock(threadLock);
    PR_Unlock(threadLock);

    slot->rv = (* slot->startFunc)(slot->a, slot->b, slot->c);

    PR_Lock(threadLock);
    slot->running = rs_zombie;

    /* notify the thread exit handler. */
    PR_NotifyCondVar(threadEndQ);

    PR_Unlock(threadLock);
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

    if (!threadStartQ) {
	threadLock = PR_NewLock();
	threadStartQ = PR_NewCondVar(threadLock);
	threadEndQ   = PR_NewCondVar(threadLock);
    }
    PR_Lock(threadLock);
    while (numRunning >= MAX_THREADS) {
    	PR_WaitCondVar(threadStartQ, PR_INTERVAL_NO_TIMEOUT);
    }
    for (i = 0; i < numUsed; ++i) {
	slot = threads + i;
    	if (slot->running == rs_idle) 
	    break;
    }
    if (i >= numUsed) {
	if (i >= MAX_THREADS) {
	    /* something's really wrong here. */
	    PORT_Assert(i < MAX_THREADS);
	    PR_Unlock(threadLock);
	    return SECFailure;
	}
	++numUsed;
	PORT_Assert(numUsed == i + 1);
	slot = threads + i;
    }

    slot->a = a;
    slot->b = b;
    slot->c = c;

    slot->startFunc = startFunc;

    slot->prThread      = PR_CreateThread(PR_USER_THREAD,
                                      thread_wrapper, slot,
				      PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
				      PR_JOINABLE_THREAD, 0);
    if (slot->prThread == NULL) {
	PR_Unlock(threadLock);
	printf("Failed to launch thread!\n");
	return SECFailure;
    } 

    slot->inUse   = 1;
    slot->running = 1;
    ++numRunning;
    PR_Unlock(threadLock);
    PRINTF("Launched thread in slot %d \n", i);
    FLUSH;

    return SECSuccess;
}

int 
reap_threads(void)
{
    perThread * slot;
    int         i;

    if (!threadLock)
    	return 0;
    PR_Lock(threadLock);
    while (numRunning > 0) {
    	PR_WaitCondVar(threadEndQ, PR_INTERVAL_NO_TIMEOUT);
	for (i = 0; i < numUsed; ++i) {
	    slot = threads + i;
	    if (slot->running == rs_zombie)  {
		/* Handle cleanup of thread here. */
		PRINTF("Thread in slot %d returned %d\n", i, slot->rv);

		/* Now make sure the thread has ended OK. */
		PR_JoinThread(slot->prThread);
		slot->running = rs_idle;
		--numRunning;

		/* notify the thread launcher. */
		PR_NotifyCondVar(threadStartQ);
	    }
	}
    }

    /* Safety Sam sez: make sure count is right. */
    for (i = 0; i < numUsed; ++i) {
	slot = threads + i;
    	if (slot->running != rs_idle)  {
	    FPRINTF(stderr, "Thread in slot %d is in state %d!\n", 
	            i, slot->running);
    	}
    }
    PR_Unlock(threadLock);
    FLUSH;
    return 0;
}

void
destroy_thread_data(void)
{
    PORT_Memset(threads, 0, sizeof threads);

    if (threadEndQ) {
    	PR_DestroyCondVar(threadEndQ);
	threadEndQ = NULL;
    }
    if (threadStartQ) {
    	PR_DestroyCondVar(threadStartQ);
	threadStartQ = NULL;
    }
    if (threadLock) {
    	PR_DestroyLock(threadLock);
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
    PRLock *	lock;
    int		count;
    int		waiters;
    PRCondVar *	condVar;
};

typedef struct lockedVarsStr lockedVars;

void 
lockedVars_Init( lockedVars * lv)
{
    lv->count   = 0;
    lv->waiters = 0;
    lv->lock    = PR_NewLock();
    lv->condVar = PR_NewCondVar(lv->lock);
}

void
lockedVars_Destroy( lockedVars * lv)
{
    PR_DestroyCondVar(lv->condVar);
    lv->condVar = NULL;

    PR_DestroyLock(lv->lock);
    lv->lock = NULL;
}

void
lockedVars_WaitForDone(lockedVars * lv)
{
    PR_Lock(lv->lock);
    while (lv->count > 0) {
    	PR_WaitCondVar(lv->condVar, PR_INTERVAL_NO_TIMEOUT);
    }
    PR_Unlock(lv->lock);
}

int	/* returns count */
lockedVars_AddToCount(lockedVars * lv, int addend)
{
    int rv;

    PR_Lock(lv->lock);
    rv = lv->count += addend;
    if (rv <= 0) {
	PR_NotifyCondVar(lv->condVar);
    }
    PR_Unlock(lv->lock);
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

    while (sent < bigBuf.len) {

	count = PR_Write(ssl_sock, bigBuf.data + sent, bigBuf.len - sent);
	if (count < 0) {
	    errWarn("PR_Write bigBuf");
	    break;
	}
	FPRINTF(stderr, "PR_Write wrote %d bytes from bigBuf\n", count );
	sent += count;
    }
    if (count >= 0) {	/* last write didn't fail. */
    	PR_Shutdown(ssl_sock, PR_SHUTDOWN_SEND);
    }

    /* notify the reader that we're done. */
    lockedVars_AddToCount(lv, -1);
    FLUSH;
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
	FPRINTF(stderr, "FDX PR_Read read %d bytes.\n", count );
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
    int                i;
    int                rv;
    PRSocketOptionData opt;
    char               msgBuf[120];
    char               buf[10240];

    pBuf   = buf;
    bufRem = sizeof buf;
    memset(buf, 0, sizeof buf);

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

    while (1) {
	newln = 0;
	i     = 0;
	rv = PR_Read(ssl_sock, pBuf, bufRem);
	if (rv == 0) {
	    break;
	}
	if (rv < 0) {
	    errWarn("HDX PR_Read");
	    goto cleanup;
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
			    "PR_TransmitFile wrote %d bytes from %s\n",
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
		rv = SSL_Enable(ssl_sock, SSL_REQUEST_CERTIFICATE, 1);
		if (rv < 0) {
		    errWarn("second SSL_Enable SSL_REQUEST_CERTIFICATE");
		    break;
		}
		rv = SSL_Enable(ssl_sock, SSL_REQUIRE_CERTIFICATE, 
				(requestCert == 4));
		if (rv < 0) {
		    errWarn("second SSL_Enable SSL_REQUIRE_CERTIFICATE");
		    break;
		}
		rv = SSL_RedoHandshake(ssl_sock);
		if (rv != 0) {
		    errWarn("SSL_RedoHandshake");
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
	    /* fwrite(buf, 1, i, stdout);	/* display it */
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

    /* do a nice shutdown if asked. */
    if (!strncmp(buf, stopCmd, strlen(stopCmd))) {
	stopping = 1;
/*	return SECFailure; */
    }
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

    while (!stopping) {
	PRFileDesc *tcp_sock;
	SECStatus   result;

	FPRINTF(stderr, "\n\n\nAbout to call accept.\n");
	tcp_sock = PR_Accept(listen_sock, &addr, PR_INTERVAL_NO_TIMEOUT);
	if (tcp_sock == NULL) {
	    errWarn("PR_Accept");
	    break;
	}

	if (bigBuf.data != NULL)
	    result = launch_thread(handle_fdx_connection, tcp_sock, model_sock, requestCert);
	else
	    result = launch_thread(handle_connection, tcp_sock, model_sock, requestCert);

	if (result != SECSuccess) {
	    PR_Close(tcp_sock);
	    break;
        }
    }

    fprintf(stderr, "Closing listen socket.\n");
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

    networkStart();

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
    rv = SSL_Enable(model_sock, SSL_SECURITY, 1);
    if (rv < 0) {
	errExit("SSL_Enable SSL_SECURITY");
    }
#endif

    rv = SSL_Enable(model_sock, SSL_ENABLE_SSL3, !disableSSL3);
    if (rv != SECSuccess) {
	errExit("error enabling SSLv3 ");
    }

    rv = SSL_Enable(model_sock, SSL_ENABLE_TLS, !disableTLS);
    if (rv != SECSuccess) {
	errExit("error enabling TLS ");
    }

    rv = SSL_Enable(model_sock, SSL_ROLLBACK_DETECTION, !disableRollBack);
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
	rv = SSL_Enable(model_sock, SSL_ENABLE_FDX, 1);
	if (rv < 0) {
	    errExit("SSL_Enable SSL_ENABLE_FDX");
	}
    }

    /* This cipher is not on by default. The Acceptance test
     * would like it to be. Turn this cipher on.
     */

    secStatus = SSL_EnableCipher( SSL_RSA_WITH_NULL_MD5, PR_TRUE);
    if ( secStatus != SECSuccess ) {
	errExit("SSL_EnableCipher:SSL_RSA_WITH_NULL_MD5");
    }


    if (requestCert) {
	SSL_AuthCertificateHook(model_sock, mySSLAuthCertificate, 
	                        (void *)CERT_GetDefaultCertDB());
	if (requestCert <= 2) { 
	    rv = SSL_Enable(model_sock, SSL_REQUEST_CERTIFICATE, 1);
	    if (rv < 0) {
		errExit("first SSL_Enable SSL_REQUEST_CERTIFICATE");
	    }
	    rv = SSL_Enable(model_sock, SSL_REQUIRE_CERTIFICATE, 
	                    (requestCert == 2));
	    if (rv < 0) {
		errExit("first SSL_Enable SSL_REQUIRE_CERTIFICATE");
	    }
	}
    }
    /* end of ssl configuration. */


    rv = PR_Bind(listen_sock, &addr);
    if (rv < 0) {
	errExit("PR_Bind");
    }

    rv = PR_Listen(listen_sock, 5);
    if (rv < 0) {
	errExit("PR_Listen");
    }

    rv = launch_thread(do_accepts, listen_sock, model_sock, requestCert);
    if (rv != SECSuccess) {
    	PR_Close(listen_sock);
    } else {
	reap_threads();
	destroy_thread_data();
    }

    if (useModelSocket && model_sock) {
    	PR_Close(model_sock);
    }

    networkEnd();
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
    int                  requestCert = 0;
    unsigned short       port        = 0;
    SECStatus            rv;
    PRBool               useExportPolicy = PR_FALSE;
    PLOptState *optstate;

    tmp = strrchr(argv[0], '/');
    tmp = tmp ? tmp + 1 : argv[0];
    progName = strrchr(tmp, '\\');
    progName = progName ? progName + 1 : tmp;

    optstate = PL_CreateOptState(argc, argv, "RT2:3c:d:p:mn:i:f:rvw:x");
    while (PL_GetNextOpt(optstate) == PL_OPT_OK) {
	switch(optstate->option) {
	default:
	case '?': Usage(progName); 		break;

	case '2': fileName = optstate->value; 	break;

	case '3': disableSSL3 = PR_TRUE;	break;

	case 'R': disableRollBack = PR_TRUE;	break;

	case 'T': disableTLS  = PR_TRUE;	break;

        case 'c': cipherString = strdup(optstate->value); break;

	case 'd': dir = optstate->value; 	break;

	case 'f': fNickName = optstate->value; 	break;

        case 'm': useModelSocket = PR_TRUE; 	break;

        case 'n': nickName = optstate->value; 	break;

        case 'i': pidFile = optstate->value; 	break;

	case 'p': port = PORT_Atoi(optstate->value); break;

	case 'r': ++requestCert; 		break;

        case 'v': verbose++; 			break;

	case 'w': passwd = optstate->value;	break;

        case 'x': useExportPolicy = PR_TRUE; 	break;
	}
    }

    if ((nickName == NULL) && (fNickName == NULL))
	Usage(progName);

    if (port == 0)
	Usage(progName);

    if (pidFile) {
	FILE *tmpfile=fopen(pidFile,"w+");

	if (tmpfile) {
	    fprintf(tmpfile,"%d",getpid());
	    fclose(tmpfile);
        }
    }
	

    /* Call the NSPR initialization routines */
    PR_Init( PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);

    if (fileName)
    	readBigFile(fileName);

    /* set our password function */
    PK11_SetPasswordFunc( passwd ? ownPasswd : SECU_GetModulePassword);

    /* Call the libsec initialization routines */
    rv = NSS_Init(dir);
    if (rv != SECSuccess) {
    	fputs("NSS_Init failed.\n", stderr);
	exit(1);
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
	disableSSL2Ciphers();
	disableSSL3Ciphers();

	while (0 != (ndx = *cipherString++)) {
	    int *cptr;
	    int  cipher;

	    if (! isalpha(ndx))
	     	Usage(progName);
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
	    fprintf(stderr, "Can't find certificate %s\n", nickName);
	    exit(1);
	}

	privKey[kt_rsa] = PK11_FindKeyByAnyCert(cert[kt_rsa], passwd);
	if (privKey[kt_rsa] == NULL) {
	    fprintf(stderr, "Can't find Private Key for cert %s\n", nickName);
	    exit(1);
	}

    }
    if (fNickName) {
	cert[kt_fortezza] = PK11_FindCertFromNickname(fNickName, NULL);
	if (cert[kt_fortezza] == NULL) {
	    fprintf(stderr, "Can't find certificate %s\n", fNickName);
	    exit(1);
	}

	privKey[kt_fortezza] = PK11_FindKeyByAnyCert(cert[kt_fortezza], NULL);
    }

    rv = SSL_ConfigMPServerSIDCache(256, 0, 0, NULL);
    if (rv != SECSuccess) {
        errExit("SSL_ConfigMPServerSIDCache");
    }

    server_main(port, requestCert, privKey, cert);

    NSS_Shutdown();
    PR_Cleanup();
    return 0;
}

