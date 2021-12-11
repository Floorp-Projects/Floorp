/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "cert.h"
#include "secitem.h"
#include "ssl.h"
#include "sslimpl.h"
#include "sslproto.h"
#include "pk11func.h"
#include "ocsp.h"

/* NEED LOCKS IN HERE.  */
CERTCertificate *
SSL_PeerCertificate(PRFileDesc *fd)
{
    sslSocket *ss;

    ss = ssl_FindSocket(fd);
    if (!ss) {
        SSL_DBG(("%d: SSL[%d]: bad socket in PeerCertificate",
                 SSL_GETPID(), fd));
        return 0;
    }
    if (ss->opt.useSecurity && ss->sec.peerCert) {
        return CERT_DupCertificate(ss->sec.peerCert);
    }
    return 0;
}

/* NEED LOCKS IN HERE.  */
CERTCertList *
SSL_PeerCertificateChain(PRFileDesc *fd)
{
    sslSocket *ss;
    CERTCertList *chain = NULL;
    CERTCertificate *cert;
    ssl3CertNode *cur;

    ss = ssl_FindSocket(fd);
    if (!ss) {
        SSL_DBG(("%d: SSL[%d]: bad socket in PeerCertificateChain",
                 SSL_GETPID(), fd));
        return NULL;
    }
    if (!ss->opt.useSecurity || !ss->sec.peerCert) {
        PORT_SetError(SSL_ERROR_NO_CERTIFICATE);
        return NULL;
    }
    chain = CERT_NewCertList();
    if (!chain) {
        return NULL;
    }
    cert = CERT_DupCertificate(ss->sec.peerCert);
    if (CERT_AddCertToListTail(chain, cert) != SECSuccess) {
        goto loser;
    }
    for (cur = ss->ssl3.peerCertChain; cur; cur = cur->next) {
        cert = CERT_DupCertificate(cur->cert);
        if (CERT_AddCertToListTail(chain, cert) != SECSuccess) {
            goto loser;
        }
    }
    return chain;

loser:
    CERT_DestroyCertList(chain);
    return NULL;
}

/* NEED LOCKS IN HERE.  */
CERTCertificate *
SSL_LocalCertificate(PRFileDesc *fd)
{
    sslSocket *ss;

    ss = ssl_FindSocket(fd);
    if (!ss) {
        SSL_DBG(("%d: SSL[%d]: bad socket in PeerCertificate",
                 SSL_GETPID(), fd));
        return NULL;
    }
    if (ss->opt.useSecurity) {
        if (ss->sec.localCert) {
            return CERT_DupCertificate(ss->sec.localCert);
        }
        if (ss->sec.ci.sid && ss->sec.ci.sid->localCert) {
            return CERT_DupCertificate(ss->sec.ci.sid->localCert);
        }
    }
    return NULL;
}

/* NEED LOCKS IN HERE.  */
SECStatus
SSL_SecurityStatus(PRFileDesc *fd, int *op, char **cp, int *kp0, int *kp1,
                   char **ip, char **sp)
{
    sslSocket *ss;

    ss = ssl_FindSocket(fd);
    if (!ss) {
        SSL_DBG(("%d: SSL[%d]: bad socket in SecurityStatus",
                 SSL_GETPID(), fd));
        return SECFailure;
    }

    if (cp)
        *cp = 0;
    if (kp0)
        *kp0 = 0;
    if (kp1)
        *kp1 = 0;
    if (ip)
        *ip = 0;
    if (sp)
        *sp = 0;
    if (op) {
        *op = SSL_SECURITY_STATUS_OFF;
    }

    if (ss->opt.useSecurity && ss->enoughFirstHsDone) {
        const ssl3BulkCipherDef *bulkCipherDef;
        PRBool isDes = PR_FALSE;

        bulkCipherDef = ssl_GetBulkCipherDef(ss->ssl3.hs.suite_def);
        if (cp) {
            *cp = PORT_Strdup(bulkCipherDef->short_name);
        }
        if (PORT_Strstr(bulkCipherDef->short_name, "DES")) {
            isDes = PR_TRUE;
        }

        if (kp0) {
            *kp0 = bulkCipherDef->key_size * 8;
            if (isDes)
                *kp0 = (*kp0 * 7) / 8;
        }
        if (kp1) {
            *kp1 = bulkCipherDef->secret_key_size * 8;
            if (isDes)
                *kp1 = (*kp1 * 7) / 8;
        }
        if (op) {
            if (bulkCipherDef->key_size == 0) {
                *op = SSL_SECURITY_STATUS_OFF;
            } else if (bulkCipherDef->secret_key_size * 8 < 90) {
                *op = SSL_SECURITY_STATUS_ON_LOW;
            } else {
                *op = SSL_SECURITY_STATUS_ON_HIGH;
            }
        }

        if (ip || sp) {
            CERTCertificate *cert;

            cert = ss->sec.peerCert;
            if (cert) {
                if (ip) {
                    *ip = CERT_NameToAscii(&cert->issuer);
                }
                if (sp) {
                    *sp = CERT_NameToAscii(&cert->subject);
                }
            } else {
                if (ip) {
                    *ip = PORT_Strdup("no certificate");
                }
                if (sp) {
                    *sp = PORT_Strdup("no certificate");
                }
            }
        }
    }

    return SECSuccess;
}

/************************************************************************/

/* NEED LOCKS IN HERE.  */
SECStatus
SSL_AuthCertificateHook(PRFileDesc *s, SSLAuthCertificate func, void *arg)
{
    sslSocket *ss;

    ss = ssl_FindSocket(s);
    if (!ss) {
        SSL_DBG(("%d: SSL[%d]: bad socket in AuthCertificateHook",
                 SSL_GETPID(), s));
        return SECFailure;
    }

    ss->authCertificate = func;
    ss->authCertificateArg = arg;

    return SECSuccess;
}

/* NEED LOCKS IN HERE.  */
SECStatus
SSL_GetClientAuthDataHook(PRFileDesc *s, SSLGetClientAuthData func,
                          void *arg)
{
    sslSocket *ss;

    ss = ssl_FindSocket(s);
    if (!ss) {
        SSL_DBG(("%d: SSL[%d]: bad socket in GetClientAuthDataHook",
                 SSL_GETPID(), s));
        return SECFailure;
    }

    ss->getClientAuthData = func;
    ss->getClientAuthDataArg = arg;
    return SECSuccess;
}

/* NEED LOCKS IN HERE.  */
SECStatus
SSL_SetPKCS11PinArg(PRFileDesc *s, void *arg)
{
    sslSocket *ss;

    ss = ssl_FindSocket(s);
    if (!ss) {
        SSL_DBG(("%d: SSL[%d]: bad socket in GetClientAuthDataHook",
                 SSL_GETPID(), s));
        return SECFailure;
    }

    ss->pkcs11PinArg = arg;
    return SECSuccess;
}

/* This is the "default" authCert callback function.  It is called when a
 * certificate message is received from the peer and the local application
 * has not registered an authCert callback function.
 */
SECStatus
SSL_AuthCertificate(void *arg, PRFileDesc *fd, PRBool checkSig, PRBool isServer)
{
    SECStatus rv;
    CERTCertDBHandle *handle;
    sslSocket *ss;
    SECCertUsage certUsage;
    const char *hostname = NULL;
    SECItemArray *certStatusArray;

    ss = ssl_FindSocket(fd);
    PORT_Assert(ss != NULL);
    if (!ss) {
        return SECFailure;
    }

    handle = (CERTCertDBHandle *)arg;
    certStatusArray = &ss->sec.ci.sid->peerCertStatus;

    PRTime now = ssl_Time(ss);
    if (certStatusArray->len) {
        PORT_SetError(0);
        if (CERT_CacheOCSPResponseFromSideChannel(handle, ss->sec.peerCert, now,
                                                  &certStatusArray->items[0],
                                                  ss->pkcs11PinArg) !=
            SECSuccess) {
            PORT_Assert(PR_GetError() != 0);
        }
    }

    /* this may seem backwards, but isn't. */
    certUsage = isServer ? certUsageSSLClient : certUsageSSLServer;

    rv = CERT_VerifyCert(handle, ss->sec.peerCert, checkSig, certUsage,
                         now, ss->pkcs11PinArg, NULL);

    if (rv != SECSuccess || isServer)
        return rv;

    /* cert is OK.  This is the client side of an SSL connection.
     * Now check the name field in the cert against the desired hostname.
     * NB: This is our only defense against Man-In-The-Middle (MITM) attacks!
     */
    hostname = ss->url;
    if (hostname && hostname[0])
        rv = CERT_VerifyCertName(ss->sec.peerCert, hostname);
    else
        rv = SECFailure;
    if (rv != SECSuccess)
        PORT_SetError(SSL_ERROR_BAD_CERT_DOMAIN);

    return rv;
}
