/*
 * Basic SSL handshake functions.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nssrenam.h"
#include "cert.h"
#include "secitem.h"
#include "sechash.h"
#include "cryptohi.h" /* for SGN_ funcs */
#include "keyhi.h"    /* for SECKEY_ high level functions. */
#include "ssl.h"
#include "sslimpl.h"
#include "sslproto.h"
#include "ssl3prot.h"
#include "sslerr.h"
#include "pk11func.h"
#include "prinit.h"

/*
** Put a string tag in the library so that we can examine an executable
** and see what kind of security it supports.
*/
const char *ssl_version = "SECURITY_VERSION:"
                          " +us"
                          " +export"
#ifdef TRACE
                          " +trace"
#endif
#ifdef DEBUG
                          " +debug"
#endif
    ;

/***********************************************************************
 * Gathers in and handles records/messages until either the handshake is
 * complete or application data is available.
 *
 * Called from ssl_Do1stHandshake() via function pointer ss->handshake.
 * Caller must hold handshake lock.
 * This function acquires and releases the RecvBufLock.
 *
 * returns SECSuccess for success.
 * returns SECFailure on error, setting PR_WOULD_BLOCK_ERROR if only blocked.
 *
 * The gather functions called by ssl_GatherRecord1stHandshake are expected
 *  to return values interpreted as follows:
 *  1 : the function completed without error.
 *  0 : the function read EOF.
 * -1 : read error, or PR_WOULD_BLOCK_ERROR, or handleRecord error.
 *
 * This code is similar to, and easily confused with, DoRecv() in sslsecur.c
 *
 * This function is called from ssl_Do1stHandshake().
 * The following functions put ssl_GatherRecord1stHandshake into ss->handshake:
 *  ssl_BeginClientHandshake
 *  ssl3_RestartHandshakeAfterCertReq
 *  ssl3_RestartHandshakeAfterServerCert
 *  ssl_BeginServerHandshake
 */
SECStatus
ssl_GatherRecord1stHandshake(sslSocket *ss)
{
    int rv;

    PORT_Assert(ss->opt.noLocks || ssl_Have1stHandshakeLock(ss));

    ssl_GetRecvBufLock(ss);

    /* Wait for handshake to complete, or application data to arrive.  */
    rv = ssl3_GatherCompleteHandshake(ss, 0);
    SSL_TRC(10, ("%d: SSL[%d]: handshake gathering, rv=%d",
                 SSL_GETPID(), ss->fd, rv));

    ssl_ReleaseRecvBufLock(ss);

    if (rv <= 0) {
        if (rv == 0) {
            /* EOF. Loser  */
            PORT_SetError(PR_END_OF_FILE_ERROR);
        }
        if (PORT_GetError() == PR_WOULD_BLOCK_ERROR) {
            SSL_TRC(10, ("%d: SSL[%d]: handshake blocked (need %d)",
                         SSL_GETPID(), ss->fd, ss->gs.remainder));
        }
        return SECFailure; /* rv is < 0 here. */
    }

    ss->handshake = NULL;
    return SECSuccess;
}

/* This function is called at the beginning of a handshake to ensure that at
 * least one SSL/TLS version is enabled. */
static SECStatus
ssl_CheckConfigSanity(sslSocket *ss)
{
    if (SSL_ALL_VERSIONS_DISABLED(&ss->vrange)) {
        SSL_DBG(("%d: SSL[%d]: Can't handshake! all versions disabled.",
                 SSL_GETPID(), ss->fd));
        PORT_SetError(SSL_ERROR_SSL_DISABLED);
        return SECFailure;
    }
    return SECSuccess;
}

/* Sends out the initial client Hello message on the connection.
 * Acquires and releases the socket's xmitBufLock.
 */
SECStatus
ssl_BeginClientHandshake(sslSocket *ss)
{
    sslSessionID *sid = NULL;
    SECStatus rv;

    PORT_Assert(ss->opt.noLocks || ssl_Have1stHandshakeLock(ss));

    ss->sec.isServer = PR_FALSE;

    rv = ssl_CheckConfigSanity(ss);
    if (rv != SECSuccess)
        goto loser;

    /* Get peer name of server */
    rv = ssl_GetPeerInfo(ss);
    if (rv < 0) {
#ifdef HPUX11
        /*
         * On some HP-UX B.11.00 systems, getpeername() occasionally
         * fails with ENOTCONN after a successful completion of
         * non-blocking connect.  I found that if we do a write()
         * and then retry getpeername(), it will work.
         */
        if (PR_GetError() == PR_NOT_CONNECTED_ERROR) {
            char dummy;
            (void)PR_Write(ss->fd->lower, &dummy, 0);
            rv = ssl_GetPeerInfo(ss);
            if (rv < 0) {
                goto loser;
            }
        }
#else
        goto loser;
#endif
    }

    SSL_TRC(3, ("%d: SSL[%d]: sending client-hello", SSL_GETPID(), ss->fd));

    /* If there's an sid set from an external cache, use it. */
    if (ss->sec.ci.sid && ss->sec.ci.sid->cached == in_external_cache) {
        sid = ss->sec.ci.sid;
        SSL_TRC(3, ("%d: SSL[%d]: using external token", SSL_GETPID(), ss->fd));
    } else if (!ss->opt.noCache) {
        /* Try to find server in our session-id cache */
        sid = ssl_LookupSID(ssl_Time(ss), &ss->sec.ci.peer,
                            ss->sec.ci.port, ss->peerID, ss->url);
    }

    if (sid) {
        if (sid->version >= ss->vrange.min && sid->version <= ss->vrange.max) {
            PORT_Assert(!ss->sec.localCert);
            ss->sec.localCert = CERT_DupCertificate(sid->localCert);
        } else {
            ssl_UncacheSessionID(ss);
            ssl_FreeSID(sid);
            sid = NULL;
        }
    }
    if (!sid) {
        sid = ssl3_NewSessionID(ss, PR_FALSE);
        if (!sid) {
            goto loser;
        }
        /* This session is a dummy, which we don't want to resume. */
        sid->u.ssl3.keys.resumable = PR_FALSE;
    }
    ss->sec.ci.sid = sid;

    ss->gs.state = GS_INIT;
    ss->handshake = ssl_GatherRecord1stHandshake;

    /* ssl3_SendClientHello will override this if it succeeds. */
    ss->version = SSL_LIBRARY_VERSION_3_0;

    ssl_GetSSL3HandshakeLock(ss);
    ssl_GetXmitBufLock(ss);
    rv = ssl3_SendClientHello(ss, client_hello_initial);
    ssl_ReleaseXmitBufLock(ss);
    ssl_ReleaseSSL3HandshakeLock(ss);

    return rv;

loser:
    return SECFailure;
}

SECStatus
ssl_BeginServerHandshake(sslSocket *ss)
{
    SECStatus rv;

    ss->sec.isServer = PR_TRUE;
    ss->ssl3.hs.ws = wait_client_hello;

    rv = ssl_CheckConfigSanity(ss);
    if (rv != SECSuccess)
        goto loser;

    ss->handshake = ssl_GatherRecord1stHandshake;
    return SECSuccess;

loser:
    return SECFailure;
}

/* This function doesn't really belong in this file.
** It's here to keep AIX compilers from optimizing it away,
** and not including it in the DSO.
*/

#include "nss.h"
extern const char __nss_ssl_version[];

PRBool
NSSSSL_VersionCheck(const char *importedVersion)
{
#define NSS_VERSION_VARIABLE __nss_ssl_version
#include "verref.h"

    /*
     * This is the secret handshake algorithm.
     *
     * This release has a simple version compatibility
     * check algorithm.  This release is not backward
     * compatible with previous major releases.  It is
     * not compatible with future major, minor, or
     * patch releases.
     */
    return NSS_VersionCheck(importedVersion);
}

const char *
NSSSSL_GetVersion(void)
{
    return NSS_VERSION;
}
