/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <stdio.h>
#include <string.h>

#include "secutil.h"
#include "basicutil.h"

#if defined(XP_UNIX)
#include <unistd.h>
#endif
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>

#include "plgetopt.h"

#include "nspr.h"
#include "prio.h"
#include "prnetdb.h"
#include "prerror.h"

#include "pk11func.h"
#include "secitem.h"
#include "sslproto.h"
#include "nss.h"
#include "ssl.h"

#ifndef PORT_Sprintf
#define PORT_Sprintf sprintf
#endif

#ifndef PORT_Strstr
#define PORT_Strstr strstr
#endif

#ifndef PORT_Malloc
#define PORT_Malloc PR_Malloc
#endif

#define RD_BUF_SIZE (60 * 1024)

/* Include these cipher suite arrays to re-use tstclnt's
 * cipher selection code.
 */

int ssl3CipherSuites[] = {
    -1,                                /* SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA* a */
    -1,                                /* SSL_FORTEZZA_DMS_WITH_RC4_128_SHA     * b */
    TLS_RSA_WITH_RC4_128_MD5,          /* c */
    TLS_RSA_WITH_3DES_EDE_CBC_SHA,     /* d */
    TLS_RSA_WITH_DES_CBC_SHA,          /* e */
    -1,                                /* TLS_RSA_EXPORT_WITH_RC4_40_MD5        * f */
    -1,                                /* TLS_RSA_EXPORT_WITH_RC2_CBC_40_MD5    * g */
    -1,                                /* SSL_FORTEZZA_DMS_WITH_NULL_SHA        * h */
    TLS_RSA_WITH_NULL_MD5,             /* i */
    -1,                                /* SSL_RSA_FIPS_WITH_3DES_EDE_CBC_SHA    * j */
    -1,                                /* SSL_RSA_FIPS_WITH_DES_CBC_SHA         * k */
    -1,                                /* TLS_RSA_EXPORT1024_WITH_DES_CBC_SHA   * l */
    -1,                                /* TLS_RSA_EXPORT1024_WITH_RC4_56_SHA    * m */
    TLS_RSA_WITH_RC4_128_SHA,          /* n */
    TLS_DHE_DSS_WITH_RC4_128_SHA,      /* o */
    TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA, /* p */
    TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA, /* q */
    TLS_DHE_RSA_WITH_DES_CBC_SHA,      /* r */
    TLS_DHE_DSS_WITH_DES_CBC_SHA,      /* s */
    TLS_DHE_DSS_WITH_AES_128_CBC_SHA,  /* t */
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA,  /* u */
    TLS_RSA_WITH_AES_128_CBC_SHA,      /* v */
    TLS_DHE_DSS_WITH_AES_256_CBC_SHA,  /* w */
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA,  /* x */
    TLS_RSA_WITH_AES_256_CBC_SHA,      /* y */
    TLS_RSA_WITH_NULL_SHA,             /* z */
    0
};

#define NO_FULLHS_PERCENTAGE -1

/* This global string is so that client main can see
 * which ciphers to use.
 */

static const char *cipherString;

static PRInt32 certsTested;
static int MakeCertOK;
static int NoReuse;
static int fullhs = NO_FULLHS_PERCENTAGE; /* percentage of full handshakes to
                                          ** perform */
static PRInt32 globalconid = 0;           /* atomically set */
static int total_connections;             /* total number of connections to perform */
static int total_connections_rounded_down_to_hundreds;
static int total_connections_modulo_100;

static PRBool NoDelay;
static PRBool QuitOnTimeout = PR_FALSE;
static PRBool ThrottleUp = PR_FALSE;

static PRLock *threadLock; /* protects the global variables below */
static PRTime lastConnectFailure;
static PRTime lastConnectSuccess;
static PRTime lastThrottleUp;
static PRInt32 remaining_connections; /* number of connections left */
static int active_threads = 8;        /* number of threads currently trying to
                               ** connect */
static PRInt32 numUsed;
/* end of variables protected by threadLock */

static SSL3Statistics *ssl3stats;

static int failed_already = 0;
static SSLVersionRange enabledVersions;
static PRBool disableLocking = PR_FALSE;
static PRBool ignoreErrors = PR_FALSE;
static PRBool enableSessionTickets = PR_FALSE;
static PRBool enableCompression = PR_FALSE;
static PRBool enableFalseStart = PR_FALSE;
static PRBool enableCertStatus = PR_FALSE;

PRIntervalTime maxInterval = PR_INTERVAL_NO_TIMEOUT;

static const SSLSignatureScheme *enabledSigSchemes = NULL;
static unsigned int enabledSigSchemeCount = 0;

char *progName;

secuPWData pwdata = { PW_NONE, 0 };

int stopping;
int verbose;
SECItem bigBuf;

#define PRINTF   \
    if (verbose) \
    printf
#define FPRINTF  \
    if (verbose) \
    fprintf

static void
Usage(void)
{
    fprintf(stderr,
            "Usage: %s [-n nickname] [-p port] [-d dbdir] [-c connections]\n"
            "          [-BDNovqs] [-f filename] [-N | -P percentage]\n"
            "          [-w dbpasswd] [-C cipher(s)] [-t threads] [-W pwfile]\n"
            "          [-V [min-version]:[max-version]] [-a sniHostName]\n"
            "          [-J signatureschemes] hostname\n"
            " where -v means verbose\n"
            "       -o flag is interpreted as follows:\n"
            "          1 -o   means override the result of server certificate validation.\n"
            "          2 -o's mean skip server certificate validation altogether.\n"
            "       -D means no TCP delays\n"
            "       -q means quit when server gone (timeout rather than retry forever)\n"
            "       -s means disable SSL socket locking\n"
            "       -N means no session reuse\n"
            "       -P means do a specified percentage of full handshakes (0-100)\n"
            "       -V [min]:[max] restricts the set of enabled SSL/TLS protocols versions.\n"
            "          All versions are enabled by default.\n"
            "          Possible values for min/max: ssl3 tls1.0 tls1.1 tls1.2\n"
            "          Example: \"-V ssl3:\" enables SSL 3 and newer.\n"
            "       -U means enable throttling up threads\n"
            "       -T enable the cert_status extension (OCSP stapling)\n"
            "       -u enable TLS Session Ticket extension\n"
            "       -z enable compression\n"
            "       -g enable false start\n"
            "       -J enable signature schemes\n"
            "          This takes a comma separated list of signature schemes in preference\n"
            "          order.\n"
            "          Possible values are:\n"
            "          rsa_pkcs1_sha1, rsa_pkcs1_sha256, rsa_pkcs1_sha384, rsa_pkcs1_sha512,\n"
            "          ecdsa_sha1, ecdsa_secp256r1_sha256, ecdsa_secp384r1_sha384,\n"
            "          ecdsa_secp521r1_sha512,\n"
            "          rsa_pss_rsae_sha256, rsa_pss_rsae_sha384, rsa_pss_rsae_sha512,\n"
            "          rsa_pss_pss_sha256, rsa_pss_pss_sha384, rsa_pss_pss_sha512,\n"
            "          dsa_sha1, dsa_sha256, dsa_sha384, dsa_sha512\n",
            progName);
    exit(1);
}

static void
errWarn(char *funcString)
{
    PRErrorCode perr = PR_GetError();
    PRInt32 oserr = PR_GetOSError();
    const char *errString = SECU_Strerror(perr);

    fprintf(stderr, "strsclnt: %s returned error %d, OS error %d: %s\n",
            funcString, perr, oserr, errString);
}

static void
errExit(char *funcString)
{
    errWarn(funcString);
    exit(1);
}

/**************************************************************************
**
** Routines for disabling SSL ciphers.
**
**************************************************************************/

void
disableAllSSLCiphers(void)
{
    const PRUint16 *cipherSuites = SSL_GetImplementedCiphers();
    int i = SSL_GetNumImplementedCiphers();
    SECStatus rv;

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

/* This invokes the "default" AuthCert handler in libssl.
** The only reason to use this one is that it prints out info as it goes.
*/
static SECStatus
mySSLAuthCertificate(void *arg, PRFileDesc *fd, PRBool checkSig,
                     PRBool isServer)
{
    SECStatus rv;
    CERTCertificate *peerCert;
    const SECItemArray *csa;

    if (MakeCertOK >= 2) {
        return SECSuccess;
    }
    peerCert = SSL_PeerCertificate(fd);

    PRINTF("strsclnt: Subject: %s\nstrsclnt: Issuer : %s\n",
           peerCert->subjectName, peerCert->issuerName);
    csa = SSL_PeerStapledOCSPResponses(fd);
    if (csa) {
        PRINTF("Received %d Cert Status items (OCSP stapled data)\n",
               csa->len);
    }
    /* invoke the "default" AuthCert handler. */
    rv = SSL_AuthCertificate(arg, fd, checkSig, isServer);

    PR_ATOMIC_INCREMENT(&certsTested);
    if (rv == SECSuccess) {
        fputs("strsclnt: -- SSL: Server Certificate Validated.\n", stderr);
    }
    CERT_DestroyCertificate(peerCert);
    /* error, if any, will be displayed by the Bad Cert Handler. */
    return rv;
}

static SECStatus
myBadCertHandler(void *arg, PRFileDesc *fd)
{
    PRErrorCode err = PR_GetError();
    if (!MakeCertOK)
        fprintf(stderr,
                "strsclnt: -- SSL: Server Certificate Invalid, err %d.\n%s\n",
                err, SECU_Strerror(err));
    return (MakeCertOK ? SECSuccess : SECFailure);
}

void
printSecurityInfo(PRFileDesc *fd)
{
    CERTCertificate *cert = NULL;
    SECStatus result;
    SSLChannelInfo channel;
    SSLCipherSuiteInfo suite;

    static int only_once;

    if (only_once && verbose < 2)
        return;
    only_once = 1;

    result = SSL_GetChannelInfo(fd, &channel, sizeof channel);
    if (result == SECSuccess &&
        channel.length == sizeof channel &&
        channel.cipherSuite) {
        result = SSL_GetCipherSuiteInfo(channel.cipherSuite,
                                        &suite, sizeof suite);
        if (result == SECSuccess) {
            FPRINTF(stderr,
                    "strsclnt: SSL version %d.%d using %d-bit %s with %d-bit %s MAC\n",
                    channel.protocolVersion >> 8, channel.protocolVersion & 0xff,
                    suite.effectiveKeyBits, suite.symCipherName,
                    suite.macBits, suite.macAlgorithmName);
            FPRINTF(stderr,
                    "strsclnt: Server Auth: %d-bit %s, Key Exchange: %d-bit %s\n"
                    "          Compression: %s\n",
                    channel.authKeyBits, suite.authAlgorithmName,
                    channel.keaKeyBits, suite.keaTypeName,
                    channel.compressionMethodName);
        }
    }

    cert = SSL_LocalCertificate(fd);
    if (!cert)
        cert = SSL_PeerCertificate(fd);

    if (verbose && cert) {
        char *ip = CERT_NameToAscii(&cert->issuer);
        char *sp = CERT_NameToAscii(&cert->subject);
        if (sp) {
            fprintf(stderr, "strsclnt: subject DN: %s\n", sp);
            PORT_Free(sp);
        }
        if (ip) {
            fprintf(stderr, "strsclnt: issuer  DN: %s\n", ip);
            PORT_Free(ip);
        }
    }
    if (cert) {
        CERT_DestroyCertificate(cert);
        cert = NULL;
    }
    fprintf(stderr,
            "strsclnt: %ld cache hits; %ld cache misses, %ld cache not reusable\n"
            "          %ld stateless resumes\n",
            ssl3stats->hsh_sid_cache_hits,
            ssl3stats->hsh_sid_cache_misses,
            ssl3stats->hsh_sid_cache_not_ok,
            ssl3stats->hsh_sid_stateless_resumes);
}

/**************************************************************************
** Begin thread management routines and data.
**************************************************************************/

#define MAX_THREADS 128

typedef SECStatus startFn(void *a, void *b, int c);

static PRInt32 numConnected;
static int max_threads; /* peak threads allowed */

typedef struct perThreadStr {
    void *a;
    void *b;
    int tid;
    int rv;
    startFn *startFunc;
    PRThread *prThread;
    PRBool inUse;
} perThread;

perThread threads[MAX_THREADS];

void
thread_wrapper(void *arg)
{
    perThread *slot = (perThread *)arg;
    PRBool done = PR_FALSE;

    do {
        PRBool doop = PR_FALSE;
        PRBool dosleep = PR_FALSE;
        PRTime now = PR_Now();

        PR_Lock(threadLock);
        if (!(slot->tid < active_threads)) {
            /* this thread isn't supposed to be running */
            if (!ThrottleUp) {
                /* we'll never need this thread again, so abort it */
                done = PR_TRUE;
            } else if (remaining_connections > 0) {
                /* we may still need this thread, so just sleep for 1s */
                dosleep = PR_TRUE;
                /* the conditions to trigger a throttle up are :
                ** 1. last PR_Connect failure must have happened more than
                **    10s ago
                ** 2. last throttling up must have happened more than 0.5s ago
                ** 3. there must be a more recent PR_Connect success than
                **    failure
                */
                if ((now - lastConnectFailure > 10 * PR_USEC_PER_SEC) &&
                    ((!lastThrottleUp) || ((now - lastThrottleUp) >=
                                           (PR_USEC_PER_SEC / 2))) &&
                    (lastConnectSuccess > lastConnectFailure)) {
                    /* try throttling up by one thread */
                    active_threads = PR_MIN(max_threads, active_threads + 1);
                    fprintf(stderr, "active_threads set up to %d\n",
                            active_threads);
                    lastThrottleUp = PR_MAX(now, lastThrottleUp);
                }
            } else {
                /* no more connections left, we are done */
                done = PR_TRUE;
            }
        } else {
            /* this thread should run */
            if (--remaining_connections >= 0) { /* protected by threadLock */
                doop = PR_TRUE;
            } else {
                done = PR_TRUE;
            }
        }
        PR_Unlock(threadLock);
        if (doop) {
            slot->rv = (*slot->startFunc)(slot->a, slot->b, slot->tid);
            PRINTF("strsclnt: Thread in slot %d returned %d\n",
                   slot->tid, slot->rv);
        }
        if (dosleep) {
            PR_Sleep(PR_SecondsToInterval(1));
        }
    } while (!done && (!failed_already || ignoreErrors));
}

SECStatus
launch_thread(
    startFn *startFunc,
    void *a,
    void *b,
    int tid)
{
    PRUint32 i;
    perThread *slot;

    PR_Lock(threadLock);

    PORT_Assert(numUsed < MAX_THREADS);
    if (!(numUsed < MAX_THREADS)) {
        PR_Unlock(threadLock);
        return SECFailure;
    }

    i = numUsed++;
    slot = &threads[i];
    slot->a = a;
    slot->b = b;
    slot->tid = tid;

    slot->startFunc = startFunc;

    slot->prThread = PR_CreateThread(PR_USER_THREAD,
                                     thread_wrapper, slot,
                                     PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
                                     PR_JOINABLE_THREAD, 0);
    if (slot->prThread == NULL) {
        PR_Unlock(threadLock);
        printf("strsclnt: Failed to launch thread!\n");
        return SECFailure;
    }

    slot->inUse = 1;
    PR_Unlock(threadLock);
    PRINTF("strsclnt: Launched thread in slot %d \n", i);

    return SECSuccess;
}

/* join all the threads */
int
reap_threads(void)
{
    int i;

    for (i = 0; i < MAX_THREADS; ++i) {
        if (threads[i].prThread) {
            PR_JoinThread(threads[i].prThread);
            threads[i].prThread = NULL;
        }
    }
    return 0;
}

void
destroy_thread_data(void)
{
    PORT_Memset(threads, 0, sizeof threads);

    if (threadLock) {
        PR_DestroyLock(threadLock);
        threadLock = NULL;
    }
}

void
init_thread_data(void)
{
    threadLock = PR_NewLock();
}

/**************************************************************************
** End   thread management routines.
**************************************************************************/

PRBool useModelSocket = PR_TRUE;

static const char outHeader[] = {
    "HTTP/1.0 200 OK\r\n"
    "Server: Netscape-Enterprise/2.0a\r\n"
    "Date: Tue, 26 Aug 1997 22:10:05 GMT\r\n"
    "Content-type: text/plain\r\n"
    "\r\n"
};

struct lockedVarsStr {
    PRLock *lock;
    int count;
    int waiters;
    PRCondVar *condVar;
};

typedef struct lockedVarsStr lockedVars;

void
lockedVars_Init(lockedVars *lv)
{
    lv->count = 0;
    lv->waiters = 0;
    lv->lock = PR_NewLock();
    lv->condVar = PR_NewCondVar(lv->lock);
}

void
lockedVars_Destroy(lockedVars *lv)
{
    PR_DestroyCondVar(lv->condVar);
    lv->condVar = NULL;

    PR_DestroyLock(lv->lock);
    lv->lock = NULL;
}

void
lockedVars_WaitForDone(lockedVars *lv)
{
    PR_Lock(lv->lock);
    while (lv->count > 0) {
        PR_WaitCondVar(lv->condVar, PR_INTERVAL_NO_TIMEOUT);
    }
    PR_Unlock(lv->lock);
}

int /* returns count */
    lockedVars_AddToCount(lockedVars *lv, int addend)
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

SECStatus
do_writes(
    void *a,
    void *b,
    int c)
{
    PRFileDesc *ssl_sock = (PRFileDesc *)a;
    lockedVars *lv = (lockedVars *)b;
    unsigned int sent = 0;
    int count = 0;

    while (sent < bigBuf.len) {

        count = PR_Send(ssl_sock, bigBuf.data + sent, bigBuf.len - sent,
                        0, maxInterval);
        if (count < 0) {
            errWarn("PR_Send bigBuf");
            break;
        }
        FPRINTF(stderr, "strsclnt: PR_Send wrote %d bytes from bigBuf\n",
                count);
        sent += count;
    }
    if (count >= 0) { /* last write didn't fail. */
        PR_Shutdown(ssl_sock, PR_SHUTDOWN_SEND);
    }

    /* notify the reader that we're done. */
    lockedVars_AddToCount(lv, -1);
    return (sent < bigBuf.len) ? SECFailure : SECSuccess;
}

int
handle_fdx_connection(PRFileDesc *ssl_sock, int connection)
{
    SECStatus result;
    int firstTime = 1;
    int countRead = 0;
    lockedVars lv;
    char *buf;

    lockedVars_Init(&lv);
    lockedVars_AddToCount(&lv, 1);

    /* Attempt to launch the writer thread. */
    result = launch_thread(do_writes, ssl_sock, &lv, connection);

    if (result != SECSuccess)
        goto cleanup;

    buf = PR_Malloc(RD_BUF_SIZE);

    if (buf) {
        do {
            /* do reads here. */
            PRInt32 count;

            count = PR_Recv(ssl_sock, buf, RD_BUF_SIZE, 0, maxInterval);
            if (count < 0) {
                errWarn("PR_Recv");
                break;
            }
            countRead += count;
            FPRINTF(stderr,
                    "strsclnt: connection %d read %d bytes (%d total).\n",
                    connection, count, countRead);
            if (firstTime) {
                firstTime = 0;
                printSecurityInfo(ssl_sock);
            }
        } while (lockedVars_AddToCount(&lv, 0) > 0);
        PR_Free(buf);
        buf = 0;
    }

    /* Wait for writer to finish */
    lockedVars_WaitForDone(&lv);
    lockedVars_Destroy(&lv);

    FPRINTF(stderr,
            "strsclnt: connection %d read %d bytes total. -----------------------\n",
            connection, countRead);

cleanup:
    /* Caller closes the socket. */

    return SECSuccess;
}

const char request[] = { "GET /abc HTTP/1.0\r\n\r\n" };

SECStatus
handle_connection(PRFileDesc *ssl_sock, int tid)
{
    int countRead = 0;
    PRInt32 rv;
    char *buf;

    buf = PR_Malloc(RD_BUF_SIZE);
    if (!buf)
        return SECFailure;

    /* compose the http request here. */

    rv = PR_Send(ssl_sock, request, strlen(request), 0, maxInterval);
    if (rv <= 0) {
        errWarn("PR_Send");
        PR_Free(buf);
        buf = 0;
        failed_already = 1;
        return SECFailure;
    }
    printSecurityInfo(ssl_sock);

    /* read until EOF */
    while (1) {
        rv = PR_Recv(ssl_sock, buf, RD_BUF_SIZE, 0, maxInterval);
        if (rv == 0) {
            break; /* EOF */
        }
        if (rv < 0) {
            errWarn("PR_Recv");
            failed_already = 1;
            break;
        }

        countRead += rv;
        FPRINTF(stderr,
                "strsclnt: connection on thread %d read %d bytes (%d total).\n",
                tid, rv, countRead);
    }
    PR_Free(buf);
    buf = 0;

    /* Caller closes the socket. */

    FPRINTF(stderr,
            "strsclnt: connection on thread %d read %d bytes total. ---------\n",
            tid, countRead);

    return SECSuccess; /* success */
}

#define USE_SOCK_PEER_ID 1

#ifdef USE_SOCK_PEER_ID

PRInt32 lastFullHandshakePeerID;

void
myHandshakeCallback(PRFileDesc *socket, void *arg)
{
    PR_ATOMIC_SET(&lastFullHandshakePeerID, (PRInt32)((char *)arg - (char *)NULL));
}

#endif

/* one copy of this function is launched in a separate thread for each
** connection to be made.
*/
SECStatus
do_connects(
    void *a,
    void *b,
    int tid)
{
    PRNetAddr *addr = (PRNetAddr *)a;
    PRFileDesc *model_sock = (PRFileDesc *)b;
    PRFileDesc *ssl_sock = 0;
    PRFileDesc *tcp_sock = 0;
    PRStatus prStatus;
    PRUint32 sleepInterval = 50; /* milliseconds */
    SECStatus rv = SECSuccess;
    PRSocketOptionData opt;

retry:

    tcp_sock = PR_OpenTCPSocket(addr->raw.family);
    if (tcp_sock == NULL) {
        errExit("PR_OpenTCPSocket");
    }

    opt.option = PR_SockOpt_Nonblocking;
    opt.value.non_blocking = PR_FALSE;
    prStatus = PR_SetSocketOption(tcp_sock, &opt);
    if (prStatus != PR_SUCCESS) {
        errWarn("PR_SetSocketOption(PR_SockOpt_Nonblocking, PR_FALSE)");
        PR_Close(tcp_sock);
        return SECSuccess;
    }

    if (NoDelay) {
        opt.option = PR_SockOpt_NoDelay;
        opt.value.no_delay = PR_TRUE;
        prStatus = PR_SetSocketOption(tcp_sock, &opt);
        if (prStatus != PR_SUCCESS) {
            errWarn("PR_SetSocketOption(PR_SockOpt_NoDelay, PR_TRUE)");
            PR_Close(tcp_sock);
            return SECSuccess;
        }
    }

    prStatus = PR_Connect(tcp_sock, addr, PR_INTERVAL_NO_TIMEOUT);
    if (prStatus != PR_SUCCESS) {
        PRErrorCode err = PR_GetError(); /* save error code */
        PRInt32 oserr = PR_GetOSError();
        if (ThrottleUp) {
            PRTime now = PR_Now();
            PR_Lock(threadLock);
            lastConnectFailure = PR_MAX(now, lastConnectFailure);
            PR_Unlock(threadLock);
            PR_SetError(err, oserr); /* restore error code */
        }
        if ((err == PR_CONNECT_REFUSED_ERROR) ||
            (err == PR_CONNECT_RESET_ERROR)) {
            int connections = numConnected;

            PR_Close(tcp_sock);
            PR_Lock(threadLock);
            if (connections > 2 && active_threads >= connections) {
                active_threads = connections - 1;
                fprintf(stderr, "active_threads set down to %d\n",
                        active_threads);
            }
            PR_Unlock(threadLock);

            if (QuitOnTimeout && sleepInterval > 40000) {
                fprintf(stderr,
                        "strsclnt: Client timed out waiting for connection to server.\n");
                exit(1);
            }
            PR_Sleep(PR_MillisecondsToInterval(sleepInterval));
            sleepInterval <<= 1;
            goto retry;
        }
        errWarn("PR_Connect");
        goto done;
    } else {
        if (ThrottleUp) {
            PRTime now = PR_Now();
            PR_Lock(threadLock);
            lastConnectSuccess = PR_MAX(now, lastConnectSuccess);
            PR_Unlock(threadLock);
        }
    }

    ssl_sock = SSL_ImportFD(model_sock, tcp_sock);
    /* XXX if this import fails, close tcp_sock and return. */
    if (!ssl_sock) {
        PR_Close(tcp_sock);
        return SECSuccess;
    }
    if (fullhs != NO_FULLHS_PERCENTAGE) {
#ifdef USE_SOCK_PEER_ID
        char sockPeerIDString[512];
        static PRInt32 sockPeerID = 0; /* atomically incremented */
        PRInt32 thisPeerID;
#endif
        PRInt32 savid = PR_ATOMIC_INCREMENT(&globalconid);
        PRInt32 conid = 1 + (savid - 1) % 100;
        /* don't change peer ID on the very first handshake, which is always
           a full, so the session gets stored into the client cache */
        if ((savid != 1) &&
            (((savid <= total_connections_rounded_down_to_hundreds) &&
              (conid <= fullhs)) ||
             (conid * 100 <= total_connections_modulo_100 * fullhs)))
#ifdef USE_SOCK_PEER_ID
        {
            /* force a full handshake by changing the socket peer ID */
            thisPeerID = PR_ATOMIC_INCREMENT(&sockPeerID);
        } else {
            /* reuse previous sockPeerID for restart handhsake */
            thisPeerID = lastFullHandshakePeerID;
        }
        PR_snprintf(sockPeerIDString, sizeof(sockPeerIDString), "ID%d",
                    thisPeerID);
        SSL_SetSockPeerID(ssl_sock, sockPeerIDString);
        SSL_HandshakeCallback(ssl_sock, myHandshakeCallback,
                              (char *)NULL + thisPeerID);
#else
            /* force a full handshake by setting the no cache option */
            SSL_OptionSet(ssl_sock, SSL_NO_CACHE, 1);
#endif
    }
    rv = SSL_ResetHandshake(ssl_sock, /* asServer */ 0);
    if (rv != SECSuccess) {
        errWarn("SSL_ResetHandshake");
        goto done;
    }

    PR_ATOMIC_INCREMENT(&numConnected);

    if (bigBuf.data != NULL) {
        (void)handle_fdx_connection(ssl_sock, tid);
    } else {
        (void)handle_connection(ssl_sock, tid);
    }

    PR_ATOMIC_DECREMENT(&numConnected);

done:
    if (ssl_sock) {
        PR_Close(ssl_sock);
    } else if (tcp_sock) {
        PR_Close(tcp_sock);
    }
    return rv;
}

typedef struct {
    PRLock *lock;
    char *nickname;
    CERTCertificate *cert;
    SECKEYPrivateKey *key;
    void *wincx;
} cert_and_key;

PRBool
FindCertAndKey(cert_and_key *Cert_And_Key)
{
    if ((NULL == Cert_And_Key->nickname) || (0 == strcmp(Cert_And_Key->nickname, "none"))) {
        return PR_TRUE;
    }
    Cert_And_Key->cert = CERT_FindUserCertByUsage(CERT_GetDefaultCertDB(),
                                                  Cert_And_Key->nickname, certUsageSSLClient,
                                                  PR_FALSE, Cert_And_Key->wincx);
    if (Cert_And_Key->cert) {
        Cert_And_Key->key = PK11_FindKeyByAnyCert(Cert_And_Key->cert, Cert_And_Key->wincx);
    }
    if (Cert_And_Key->cert && Cert_And_Key->key) {
        return PR_TRUE;
    } else {
        return PR_FALSE;
    }
}

PRBool
LoggedIn(CERTCertificate *cert, SECKEYPrivateKey *key)
{
    if ((cert->slot) && (key->pkcs11Slot) &&
        (!PK11_NeedLogin(cert->slot) ||
         PR_TRUE == PK11_IsLoggedIn(cert->slot, NULL)) &&
        (!PK11_NeedLogin(key->pkcs11Slot) ||
         PR_TRUE == PK11_IsLoggedIn(key->pkcs11Slot, NULL))) {
        return PR_TRUE;
    }

    return PR_FALSE;
}

SECStatus
StressClient_GetClientAuthData(void *arg,
                               PRFileDesc *socket,
                               struct CERTDistNamesStr *caNames,
                               struct CERTCertificateStr **pRetCert,
                               struct SECKEYPrivateKeyStr **pRetKey)
{
    cert_and_key *Cert_And_Key = (cert_and_key *)arg;

    if (!pRetCert || !pRetKey) {
        /* bad pointers, can't return a cert or key */
        return SECFailure;
    }

    *pRetCert = NULL;
    *pRetKey = NULL;

    if (Cert_And_Key && Cert_And_Key->nickname) {
        while (PR_TRUE) {
            if (Cert_And_Key && Cert_And_Key->lock) {
                int timeout = 0;
                PR_Lock(Cert_And_Key->lock);

                if (Cert_And_Key->cert) {
                    *pRetCert = CERT_DupCertificate(Cert_And_Key->cert);
                }

                if (Cert_And_Key->key) {
                    *pRetKey = SECKEY_CopyPrivateKey(Cert_And_Key->key);
                }
                PR_Unlock(Cert_And_Key->lock);
                if (!*pRetCert || !*pRetKey) {
                    /* one or both of them failed to copy. Either the source was NULL, or there was
                    ** an out of memory condition. Free any allocated copy and fail */
                    if (*pRetCert) {
                        CERT_DestroyCertificate(*pRetCert);
                        *pRetCert = NULL;
                    }
                    if (*pRetKey) {
                        SECKEY_DestroyPrivateKey(*pRetKey);
                        *pRetKey = NULL;
                    }
                    break;
                }
                /* now check if those objects are valid */
                if (PR_FALSE == LoggedIn(*pRetCert, *pRetKey)) {
                    /* token is no longer logged in, it was removed */

                    /* first, delete and clear our invalid local objects */
                    CERT_DestroyCertificate(*pRetCert);
                    SECKEY_DestroyPrivateKey(*pRetKey);
                    *pRetCert = NULL;
                    *pRetKey = NULL;

                    PR_Lock(Cert_And_Key->lock);
                    /* check if another thread already logged back in */
                    if (PR_TRUE == LoggedIn(Cert_And_Key->cert, Cert_And_Key->key)) {
                        /* yes : try again */
                        PR_Unlock(Cert_And_Key->lock);
                        continue;
                    }
                    /* this is the thread to retry */
                    CERT_DestroyCertificate(Cert_And_Key->cert);
                    SECKEY_DestroyPrivateKey(Cert_And_Key->key);
                    Cert_And_Key->cert = NULL;
                    Cert_And_Key->key = NULL;

                    /* now look up the cert and key again */
                    while (PR_FALSE == FindCertAndKey(Cert_And_Key)) {
                        PR_Sleep(PR_SecondsToInterval(1));
                        timeout++;
                        if (timeout >= 60) {
                            printf("\nToken pulled and not reinserted early enough : aborting.\n");
                            exit(1);
                        }
                    }
                    PR_Unlock(Cert_And_Key->lock);
                    continue;
                    /* try again to reduce code size */
                }
                return SECSuccess;
            }
        }
        *pRetCert = NULL;
        *pRetKey = NULL;
        return SECFailure;
    } else {
        /* no cert configured, automatically find the right cert. */
        CERTCertificate *cert = NULL;
        SECKEYPrivateKey *privkey = NULL;
        CERTCertNicknames *names;
        int i;
        void *proto_win = NULL;
        SECStatus rv = SECFailure;

        if (Cert_And_Key) {
            proto_win = Cert_And_Key->wincx;
        }

        names = CERT_GetCertNicknames(CERT_GetDefaultCertDB(),
                                      SEC_CERT_NICKNAMES_USER, proto_win);
        if (names != NULL) {
            for (i = 0; i < names->numnicknames; i++) {
                cert = CERT_FindUserCertByUsage(CERT_GetDefaultCertDB(),
                                                names->nicknames[i], certUsageSSLClient,
                                                PR_FALSE, proto_win);
                if (!cert)
                    continue;
                /* Only check unexpired certs */
                if (CERT_CheckCertValidTimes(cert, PR_Now(), PR_TRUE) !=
                    secCertTimeValid) {
                    CERT_DestroyCertificate(cert);
                    continue;
                }
                rv = NSS_CmpCertChainWCANames(cert, caNames);
                if (rv == SECSuccess) {
                    privkey = PK11_FindKeyByAnyCert(cert, proto_win);
                    if (privkey)
                        break;
                }
                rv = SECFailure;
                CERT_DestroyCertificate(cert);
            }
            CERT_FreeNicknames(names);
        }
        if (rv == SECSuccess) {
            *pRetCert = cert;
            *pRetKey = privkey;
        }
        return rv;
    }
}

int
hexchar_to_int(int c)
{
    if (((c) >= '0') && ((c) <= '9'))
        return (c) - '0';
    if (((c) >= 'a') && ((c) <= 'f'))
        return (c) - 'a' + 10;
    if (((c) >= 'A') && ((c) <= 'F'))
        return (c) - 'A' + 10;
    failed_already = 1;
    return -1;
}

void
client_main(
    unsigned short port,
    int connections,
    cert_and_key *Cert_And_Key,
    const char *hostName,
    const char *sniHostName)
{
    PRFileDesc *model_sock = NULL;
    int i;
    int rv;
    PRStatus status;
    PRNetAddr addr;

    status = PR_StringToNetAddr(hostName, &addr);
    if (status == PR_SUCCESS) {
        addr.inet.port = PR_htons(port);
    } else {
        /* Lookup host */
        PRAddrInfo *addrInfo;
        void *enumPtr = NULL;

        addrInfo = PR_GetAddrInfoByName(hostName, PR_AF_UNSPEC,
                                        PR_AI_ADDRCONFIG | PR_AI_NOCANONNAME);
        if (!addrInfo) {
            SECU_PrintError(progName, "error looking up host");
            return;
        }
        do {
            enumPtr = PR_EnumerateAddrInfo(enumPtr, addrInfo, port, &addr);
        } while (enumPtr != NULL &&
                 addr.raw.family != PR_AF_INET &&
                 addr.raw.family != PR_AF_INET6);
        PR_FreeAddrInfo(addrInfo);
        if (enumPtr == NULL) {
            SECU_PrintError(progName, "error looking up host address");
            return;
        }
    }

    /* all suites except RSA_NULL_MD5 are enabled by Domestic Policy */
    NSS_SetDomesticPolicy();

    /* all SSL3 cipher suites are enabled by default. */
    if (cipherString) {
        int ndx;

        /* disable all the ciphers, then enable the ones we want. */
        disableAllSSLCiphers();

        while (0 != (ndx = *cipherString)) {
            const char *startCipher = cipherString++;
            int cipher = 0;

            if (ndx == ':') {
                cipher = hexchar_to_int(*cipherString++);
                cipher <<= 4;
                cipher |= hexchar_to_int(*cipherString++);
                cipher <<= 4;
                cipher |= hexchar_to_int(*cipherString++);
                cipher <<= 4;
                cipher |= hexchar_to_int(*cipherString++);
                if (cipher <= 0) {
                    fprintf(stderr, "strsclnt: Invalid cipher value: %-5.5s\n",
                            startCipher);
                    failed_already = 1;
                    return;
                }
            } else {
                if (isalpha(ndx)) {
                    ndx = tolower(ndx) - 'a';
                    if (ndx < PR_ARRAY_SIZE(ssl3CipherSuites)) {
                        cipher = ssl3CipherSuites[ndx];
                    }
                }
                if (cipher <= 0) {
                    fprintf(stderr, "strsclnt: Invalid cipher letter: %c\n",
                            *startCipher);
                    failed_already = 1;
                    return;
                }
            }
            rv = SSL_CipherPrefSetDefault(cipher, PR_TRUE);
            if (rv != SECSuccess) {
                fprintf(stderr,
                        "strsclnt: SSL_CipherPrefSetDefault(0x%04x) failed\n",
                        cipher);
                failed_already = 1;
                return;
            }
        }
    }

    /* configure model SSL socket. */

    model_sock = PR_OpenTCPSocket(addr.raw.family);
    if (model_sock == NULL) {
        errExit("PR_OpenTCPSocket for model socket");
    }

    model_sock = SSL_ImportFD(NULL, model_sock);
    if (model_sock == NULL) {
        errExit("SSL_ImportFD");
    }

    /* do SSL configuration. */

    rv = SSL_OptionSet(model_sock, SSL_SECURITY, enabledVersions.min != 0);
    if (rv < 0) {
        errExit("SSL_OptionSet SSL_SECURITY");
    }

    rv = SSL_VersionRangeSet(model_sock, &enabledVersions);
    if (rv != SECSuccess) {
        errExit("error setting SSL/TLS version range ");
    }

    if (enabledSigSchemes) {
        rv = SSL_SignatureSchemePrefSet(model_sock, enabledSigSchemes,
                                        enabledSigSchemeCount);
        if (rv < 0) {
            errExit("SSL_SignatureSchemePrefSet");
        }
    }

    if (bigBuf.data) { /* doing FDX */
        rv = SSL_OptionSet(model_sock, SSL_ENABLE_FDX, 1);
        if (rv < 0) {
            errExit("SSL_OptionSet SSL_ENABLE_FDX");
        }
    }

    if (NoReuse) {
        rv = SSL_OptionSet(model_sock, SSL_NO_CACHE, 1);
        if (rv < 0) {
            errExit("SSL_OptionSet SSL_NO_CACHE");
        }
    }

    if (disableLocking) {
        rv = SSL_OptionSet(model_sock, SSL_NO_LOCKS, 1);
        if (rv < 0) {
            errExit("SSL_OptionSet SSL_NO_LOCKS");
        }
    }

    if (enableSessionTickets) {
        rv = SSL_OptionSet(model_sock, SSL_ENABLE_SESSION_TICKETS, PR_TRUE);
        if (rv != SECSuccess)
            errExit("SSL_OptionSet SSL_ENABLE_SESSION_TICKETS");
    }

    if (enableCompression) {
        rv = SSL_OptionSet(model_sock, SSL_ENABLE_DEFLATE, PR_TRUE);
        if (rv != SECSuccess)
            errExit("SSL_OptionSet SSL_ENABLE_DEFLATE");
    }

    if (enableFalseStart) {
        rv = SSL_OptionSet(model_sock, SSL_ENABLE_FALSE_START, PR_TRUE);
        if (rv != SECSuccess)
            errExit("SSL_OptionSet SSL_ENABLE_FALSE_START");
    }

    if (enableCertStatus) {
        rv = SSL_OptionSet(model_sock, SSL_ENABLE_OCSP_STAPLING, PR_TRUE);
        if (rv != SECSuccess)
            errExit("SSL_OptionSet SSL_ENABLE_OCSP_STAPLING");
    }

    SSL_SetPKCS11PinArg(model_sock, &pwdata);

    SSL_SetURL(model_sock, hostName);

    SSL_AuthCertificateHook(model_sock, mySSLAuthCertificate,
                            (void *)CERT_GetDefaultCertDB());
    SSL_BadCertHook(model_sock, myBadCertHandler, NULL);

    SSL_GetClientAuthDataHook(model_sock, StressClient_GetClientAuthData, (void *)Cert_And_Key);

    if (sniHostName) {
        SSL_SetURL(model_sock, sniHostName);
    }
    /* I'm not going to set the HandshakeCallback function. */

    /* end of ssl configuration. */

    init_thread_data();

    remaining_connections = total_connections = connections;
    total_connections_modulo_100 = total_connections % 100;
    total_connections_rounded_down_to_hundreds =
        total_connections - total_connections_modulo_100;

    if (!NoReuse) {
        remaining_connections = 1;
        launch_thread(do_connects, &addr, model_sock, 0);
        /* wait for the first connection to terminate, then launch the rest. */
        reap_threads();
        remaining_connections = total_connections - 1;
    }
    if (remaining_connections > 0) {
        active_threads = PR_MIN(active_threads, remaining_connections);
        /* Start up the threads */
        for (i = 0; i < active_threads; i++) {
            launch_thread(do_connects, &addr, model_sock, i);
        }
        reap_threads();
    }
    destroy_thread_data();

    PR_Close(model_sock);
}

SECStatus
readBigFile(const char *fileName)
{
    PRFileInfo info;
    PRStatus status;
    SECStatus rv = SECFailure;
    int count;
    int hdrLen;
    PRFileDesc *local_file_fd = NULL;

    status = PR_GetFileInfo(fileName, &info);

    if (status == PR_SUCCESS &&
        info.type == PR_FILE_FILE &&
        info.size > 0 &&
        NULL != (local_file_fd = PR_Open(fileName, PR_RDONLY, 0))) {

        hdrLen = PORT_Strlen(outHeader);
        bigBuf.len = hdrLen + info.size;
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
    const char *dir = ".";
    const char *fileName = NULL;
    char *hostName = NULL;
    char *nickName = NULL;
    char *tmp = NULL;
    int connections = 1;
    int exitVal;
    int tmpInt;
    unsigned short port = 443;
    SECStatus rv;
    PLOptState *optstate;
    PLOptStatus status;
    cert_and_key Cert_And_Key;
    char *sniHostName = NULL;

    /* Call the NSPR initialization routines */
    PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
    SSL_VersionRangeGetSupported(ssl_variant_stream, &enabledVersions);

    tmp = strrchr(argv[0], '/');
    tmp = tmp ? tmp + 1 : argv[0];
    progName = strrchr(tmp, '\\');
    progName = progName ? progName + 1 : tmp;

    /* XXX: 'B' was used in the past but removed in 3.28,
     *      please leave some time before resuing it. */
    optstate = PL_CreateOptState(argc, argv,
                                 "C:DJ:NP:TUV:W:a:c:d:f:gin:op:qst:uvw:z");
    while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
        switch (optstate->option) {
            case 'C':
                cipherString = optstate->value;
                break;

            case 'D':
                NoDelay = PR_TRUE;
                break;

            case 'I': /* reserved for OCSP multi-stapling */
                break;

            case 'J':
                rv = parseSigSchemeList(optstate->value, &enabledSigSchemes, &enabledSigSchemeCount);
                if (rv != SECSuccess) {
                    PL_DestroyOptState(optstate);
                    fprintf(stderr, "Bad signature scheme specified.\n");
                    Usage();
                }
                break;

            case 'N':
                NoReuse = 1;
                break;

            case 'P':
                fullhs = PORT_Atoi(optstate->value);
                break;

            case 'T':
                enableCertStatus = PR_TRUE;
                break;

            case 'U':
                ThrottleUp = PR_TRUE;
                break;

            case 'V':
                if (SECU_ParseSSLVersionRangeString(optstate->value,
                                                    enabledVersions, &enabledVersions) !=
                    SECSuccess) {
                    fprintf(stderr, "Bad version specified.\n");
                    Usage();
                }
                break;

            case 'a':
                sniHostName = PL_strdup(optstate->value);
                break;

            case 'c':
                connections = PORT_Atoi(optstate->value);
                break;

            case 'd':
                dir = optstate->value;
                break;

            case 'f':
                fileName = optstate->value;
                break;

            case 'g':
                enableFalseStart = PR_TRUE;
                break;

            case 'i':
                ignoreErrors = PR_TRUE;
                break;

            case 'n':
                nickName = PL_strdup(optstate->value);
                break;

            case 'o':
                MakeCertOK++;
                break;

            case 'p':
                port = PORT_Atoi(optstate->value);
                break;

            case 'q':
                QuitOnTimeout = PR_TRUE;
                break;

            case 's':
                disableLocking = PR_TRUE;
                break;

            case 't':
                tmpInt = PORT_Atoi(optstate->value);
                if (tmpInt > 0 && tmpInt < MAX_THREADS)
                    max_threads = active_threads = tmpInt;
                break;

            case 'u':
                enableSessionTickets = PR_TRUE;
                break;

            case 'v':
                verbose++;
                break;

            case 'w':
                pwdata.source = PW_PLAINTEXT;
                pwdata.data = PL_strdup(optstate->value);
                break;

            case 'W':
                pwdata.source = PW_FROMFILE;
                pwdata.data = PL_strdup(optstate->value);
                break;

            case 'z':
                enableCompression = PR_TRUE;
                break;

            case 0: /* positional parameter */
                if (hostName) {
                    Usage();
                }
                hostName = PL_strdup(optstate->value);
                break;

            default:
            case '?':
                Usage();
                break;
        }
    }
    PL_DestroyOptState(optstate);

    if (!hostName || status == PL_OPT_BAD)
        Usage();

    if (fullhs != NO_FULLHS_PERCENTAGE && (fullhs < 0 || fullhs > 100 || NoReuse))
        Usage();

    if (port == 0)
        Usage();

    if (fileName)
        readBigFile(fileName);

    PK11_SetPasswordFunc(SECU_GetModulePassword);

    tmp = PR_GetEnvSecure("NSS_DEBUG_TIMEOUT");
    if (tmp && tmp[0]) {
        int sec = PORT_Atoi(tmp);
        if (sec > 0) {
            maxInterval = PR_SecondsToInterval(sec);
        }
    }

    /* Call the NSS initialization routines */
    rv = NSS_Initialize(dir, "", "", SECMOD_DB, NSS_INIT_READONLY);
    if (rv != SECSuccess) {
        fputs("NSS_Init failed.\n", stderr);
        exit(1);
    }
    ssl3stats = SSL_GetStatistics();
    Cert_And_Key.lock = PR_NewLock();
    Cert_And_Key.nickname = nickName;
    Cert_And_Key.wincx = &pwdata;
    Cert_And_Key.cert = NULL;
    Cert_And_Key.key = NULL;

    if (PR_FALSE == FindCertAndKey(&Cert_And_Key)) {

        if (Cert_And_Key.cert == NULL) {
            fprintf(stderr, "strsclnt: Can't find certificate %s\n", Cert_And_Key.nickname);
            exit(1);
        }

        if (Cert_And_Key.key == NULL) {
            fprintf(stderr, "strsclnt: Can't find Private Key for cert %s\n",
                    Cert_And_Key.nickname);
            exit(1);
        }
    }

    client_main(port, connections, &Cert_And_Key, hostName,
                sniHostName);

    /* clean up */
    if (Cert_And_Key.cert) {
        CERT_DestroyCertificate(Cert_And_Key.cert);
    }
    if (Cert_And_Key.key) {
        SECKEY_DestroyPrivateKey(Cert_And_Key.key);
    }

    PR_DestroyLock(Cert_And_Key.lock);

    if (pwdata.data) {
        PL_strfree(pwdata.data);
    }
    if (Cert_And_Key.nickname) {
        PL_strfree(Cert_And_Key.nickname);
    }
    if (sniHostName) {
        PL_strfree(sniHostName);
    }

    PL_strfree(hostName);

    PORT_Free((SSLSignatureScheme *)enabledSigSchemes);

    /* some final stats. */
    printf(
        "strsclnt: %ld cache hits; %ld cache misses, %ld cache not reusable\n"
        "          %ld stateless resumes\n",
        ssl3stats->hsh_sid_cache_hits,
        ssl3stats->hsh_sid_cache_misses,
        ssl3stats->hsh_sid_cache_not_ok,
        ssl3stats->hsh_sid_stateless_resumes);

    if (!NoReuse) {
        if (enableSessionTickets)
            exitVal = (ssl3stats->hsh_sid_stateless_resumes == 0);
        else
            exitVal = (ssl3stats->hsh_sid_cache_misses > 1) ||
                      (ssl3stats->hsh_sid_stateless_resumes != 0);
        if (!exitVal)
            exitVal = (ssl3stats->hsh_sid_cache_not_ok != 0) ||
                      (certsTested > 1);
    } else {
        printf("strsclnt: NoReuse - %d server certificates tested.\n",
               certsTested);
        exitVal = (ssl3stats->hsh_sid_cache_misses != connections) ||
                  (ssl3stats->hsh_sid_stateless_resumes != 0) ||
                  (certsTested != connections);
    }

    exitVal = (exitVal || failed_already);
    SSL_ClearSessionCache();
    if (NSS_Shutdown() != SECSuccess) {
        printf("strsclnt: NSS_Shutdown() failed.\n");
        exit(1);
    }

    PR_Cleanup();
    return exitVal;
}
