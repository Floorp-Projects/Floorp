/*
 * NSS utility functions
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <string.h>
#include "prerror.h"
#include "secitem.h"
#include "prnetdb.h"
#include "cert.h"
#include "nspr.h"
#include "secder.h"
#include "keyhi.h"
#include "nss.h"
#include "ssl.h"
#include "pk11func.h" /* for PK11_ function calls */
#include "sslimpl.h"

/*
 * This callback used by SSL to pull client sertificate upon
 * server request
 */
SECStatus
NSS_GetClientAuthData(void *arg,
                      PRFileDesc *socket,
                      struct CERTDistNamesStr *caNames,
                      struct CERTCertificateStr **pRetCert,
                      struct SECKEYPrivateKeyStr **pRetKey)
{
    CERTCertificate *cert = NULL;
    SECKEYPrivateKey *privkey = NULL;
    char *chosenNickName = (char *)arg; /* CONST */
    void *proto_win = NULL;
    SECStatus rv = SECFailure;

    proto_win = SSL_RevealPinArg(socket);

    if (chosenNickName) {
        cert = CERT_FindUserCertByUsage(CERT_GetDefaultCertDB(),
                                        chosenNickName, certUsageSSLClient,
                                        PR_FALSE, proto_win);
        if (cert) {
            privkey = PK11_FindKeyByAnyCert(cert, proto_win);
            if (privkey) {
                rv = SECSuccess;
            } else {
                CERT_DestroyCertificate(cert);
            }
        }
    } else { /* no name given, automatically find the right cert. */
        CERTCertNicknames *names;
        int i;

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
                if (CERT_CheckCertValidTimes(cert, ssl_TimeUsec(), PR_TRUE) !=
                    secCertTimeValid) {
                    CERT_DestroyCertificate(cert);
                    continue;
                }
                rv = NSS_CmpCertChainWCANames(cert, caNames);
                if (rv == SECSuccess) {
                    privkey =
                        PK11_FindKeyByAnyCert(cert, proto_win);
                    if (privkey)
                        break;
                }
                rv = SECFailure;
                CERT_DestroyCertificate(cert);
            }
            CERT_FreeNicknames(names);
        }
    }
    if (rv == SECSuccess) {
        *pRetCert = cert;
        *pRetKey = privkey;
    }
    return rv;
}
