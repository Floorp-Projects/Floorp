/*
 * Accessor functions for SSLSocket private members.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cert.h"
#include "ssl.h"
#include "certt.h"
#include "sslimpl.h"

/* given PRFileDesc, returns a copy of certificate associated with the socket
 * the caller should delete the cert when done with SSL_DestroyCertificate
 */
CERTCertificate *
SSL_RevealCert(PRFileDesc *fd)
{
    CERTCertificate *cert = NULL;
    sslSocket *sslsocket = NULL;

    sslsocket = ssl_FindSocket(fd);

    /* CERT_DupCertificate increases reference count and returns pointer to
     * the same cert
     */
    if (sslsocket && sslsocket->sec.peerCert)
        cert = CERT_DupCertificate(sslsocket->sec.peerCert);

    return cert;
}

/* given PRFileDesc, returns a pointer to PinArg associated with the socket
 */
void *
SSL_RevealPinArg(PRFileDesc *fd)
{
    sslSocket *sslsocket = NULL;
    void *PinArg = NULL;

    sslsocket = ssl_FindSocket(fd);

    /* is pkcs11PinArg part of the sslSocket or sslSecurityInfo ? */
    if (sslsocket)
        PinArg = sslsocket->pkcs11PinArg;

    return PinArg;
}

/* given PRFileDesc, returns a pointer to the URL associated with the socket
 * the caller should free url when done
 */
char *
SSL_RevealURL(PRFileDesc *fd)
{
    sslSocket *sslsocket = NULL;
    char *url = NULL;

    sslsocket = ssl_FindSocket(fd);

    if (sslsocket && sslsocket->url)
        url = PL_strdup(sslsocket->url);

    return url;
}

/* given PRFileDesc, returns status information related to extensions
 * negotiated with peer during the handshake.
 */

SECStatus
SSL_HandshakeNegotiatedExtension(PRFileDesc *socket,
                                 SSLExtensionType extId,
                                 PRBool *pYes)
{
    /* some decisions derived from SSL_GetChannelInfo */
    sslSocket *sslsocket = NULL;

    if (!pYes) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    sslsocket = ssl_FindSocket(socket);
    if (!sslsocket) {
        SSL_DBG(("%d: SSL[%d]: bad socket in HandshakeNegotiatedExtension",
                 SSL_GETPID(), socket));
        return SECFailure;
    }

    *pYes = PR_FALSE;

    /* according to public API SSL_GetChannelInfo, this doesn't need a lock */
    if (sslsocket->opt.useSecurity) {
        /* now we know this socket went through ssl3_InitState() and
         * ss->xtnData got initialized, which is the only member accessed by
         * ssl3_ExtensionNegotiated();
         * Member xtnData appears to get accessed in functions that handle
         * the handshake (hello messages and extension sending),
         * therefore the handshake lock should be sufficient.
         */
        ssl_GetSSL3HandshakeLock(sslsocket);
        *pYes = ssl3_ExtensionNegotiated(sslsocket, extId);
        ssl_ReleaseSSL3HandshakeLock(sslsocket);
    }

    return SECSuccess;
}
