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

/* convert a CERTDistNameStr to an array ascii strings.
 * we ignore caNames which we can't convert, so n could be less than nnames
 * n is always set, even on failure.
 * This function allows us to use the existing CERT_FilterCertListByCANames. */
static char **
ssl_DistNamesToStrings(struct CERTDistNamesStr *caNames, int *n)
{
    char **names;
    int i;
    SECStatus rv;
    PLArenaPool *arena;

    *n = 0;
    names = PORT_ZNewArray(char *, caNames->nnames);
    if (names == NULL) {
        return NULL;
    }
    arena = PORT_NewArena(2048);
    if (arena == NULL) {
        PORT_Free(names);
        return NULL;
    }
    for (i = 0; i < caNames->nnames; ++i) {
        CERTName dn;
        rv = SEC_QuickDERDecodeItem(arena, &dn, SEC_ASN1_GET(CERT_NameTemplate),
                                    caNames->names + i);
        if (rv != SECSuccess) {
            continue;
        }
        names[*n] = CERT_NameToAscii(&dn);
        if (names[*n])
            (*n)++;
    }
    PORT_FreeArena(arena, PR_FALSE);
    return names;
}

/* free the dist names we allocated in the above function. n must be the
 * returned n from that function. */
static void
ssl_FreeDistNamesStrings(char **strings, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        PORT_Free(strings[i]);
    }
    PORT_Free(strings);
}

PRBool
ssl_CertIsUsable(sslSocket *ss, CERTCertificate *cert)
{
    SECStatus rv;
    SSLSignatureScheme scheme;

    if ((ss == NULL) || (cert == NULL)) {
        return PR_FALSE;
    }
    /* There are two ways of handling the old style handshake:
     * 1) check the actual record we are using and return true,
     *   if (!ss->ssl3.hs.hashType == handshake_hash_record  &&
     *           ss->ssl3.hs.hashType == handshake_hash_single) {
     *   return PR_TRUE;
     * 2) assume if ss->peerSignatureSchemesCount == 0 we are using the
     *    old handshake.
     * There is one case where using 2 will be wrong: we somehow call this
     * function outside the case where of out GetClientAuthData context.
     * In that case we don't know that the 'real' peerScheme list is, so the
     * best we can do is either always assume good or always assume bad.
     * I think the best results is to always assume good, so we use
     * option 2 here to handle that case as well.*/
    if (ss->peerSignatureSchemeCount == 0) {
        return PR_TRUE;
    }
    if (ss->peerSignatureSchemes == NULL) {
        return PR_FALSE; /* should this really be an assert? */
    }
    rv = ssl_PickClientSignatureScheme(ss, cert, NULL,
                                       ss->peerSignatureSchemes,
                                       ss->peerSignatureSchemeCount,
                                       &scheme);
    if (rv != SECSuccess) {
        return PR_FALSE;
    }
    return PR_TRUE;
}

SECStatus
ssl_FilterClientCertListBySSLSocket(sslSocket *ss, CERTCertList *certList)
{
    CERTCertListNode *node;
    CERTCertificate *cert;

    if (!certList) {
        return SECFailure;
    }

    node = CERT_LIST_HEAD(certList);

    while (!CERT_LIST_END(node, certList)) {
        cert = node->cert;
        if (PR_TRUE != ssl_CertIsUsable(ss, cert)) {
            /* cert doesn't match the socket criteria, remove it */
            CERTCertListNode *freenode = node;
            node = CERT_LIST_NEXT(node);
            CERT_RemoveCertListNode(freenode);
        } else {
            /* this cert is good, go to the next cert */
            node = CERT_LIST_NEXT(node);
        }
    }

    return (SECSuccess);
}

/* This function can be called by the application's custom GetClientAuthHook
 * to filter out any certs in the cert list that doesn't match the negotiated
 * requirements of the current SSL connection.
 */
SECStatus
SSL_FilterClientCertListBySocket(PRFileDesc *fd, CERTCertList *certList)
{
    sslSocket *ss = ssl_FindSocket(fd);
    if (ss == NULL) {
        return SECFailure;
    }
    return ssl_FilterClientCertListBySSLSocket(ss, certList);
}

/* This function can be called by the application's custom GetClientAuthHook
 * to determine if a single certificate matches the negotiated requirements of
 * the current SSL connection.
 */
PRBool
SSL_CertIsUsable(PRFileDesc *fd, CERTCertificate *cert)
{
    sslSocket *ss = ssl_FindSocket(fd);
    if (ss == NULL) {
        return PR_FALSE;
    }
    return ssl_CertIsUsable(ss, cert);
}

/*
 * This callback used by SSL to pull client certificate upon
 * server request
 */
SECStatus
NSS_GetClientAuthData(void *arg,
                      PRFileDesc *fd,
                      struct CERTDistNamesStr *caNames,
                      struct CERTCertificateStr **pRetCert,
                      struct SECKEYPrivateKeyStr **pRetKey)
{
    CERTCertificate *cert = NULL;
    CERTCertList *certList = NULL;
    SECKEYPrivateKey *privkey = NULL;
    char *chosenNickName = (char *)arg; /* CONST */
    SECStatus rv = SECFailure;

    sslSocket *ss = ssl_FindSocket(fd);
    if (!ss) {
        return SECFailure;
    }
    void *pw_arg = SSL_RevealPinArg(fd);

    /* first, handle any token authentication that may be needed */
    if (chosenNickName && pw_arg) {
        certList = PK11_FindCertsFromNickname(chosenNickName, pw_arg);
        if (certList) {
            CERT_FilterCertListForUserCerts(certList);
            rv = CERT_FilterCertListByUsage(certList, certUsageSSLClient,
                                            PR_FALSE);
            if ((rv != SECSuccess) || CERT_LIST_EMPTY(certList)) {
                CERT_DestroyCertList(certList);
                certList = NULL;
            }
        }
    }

    /* otherwise look through the cache based on usage
     * if chosenNickname is set, we ignore the expiration date */
    if (certList == NULL) {
        certList = CERT_FindUserCertsByUsage(CERT_GetDefaultCertDB(),
                                             certUsageSSLClient,
                                             PR_FALSE, chosenNickName == NULL,
                                             pw_arg);
        /* filter only the certs that meet the nickname requirements */
        if (chosenNickName) {
            rv = CERT_FilterCertListByNickname(certList, chosenNickName,
                                               pw_arg);
        } else {
            int nnames = 0;
            char **names = ssl_DistNamesToStrings(caNames, &nnames);
            rv = CERT_FilterCertListByCANames(certList, nnames, names,
                                              certUsageSSLClient);
            ssl_FreeDistNamesStrings(names, nnames);
        }
        if ((rv != SECSuccess) || CERT_LIST_EMPTY(certList)) {
            CERT_DestroyCertList(certList);
            certList = NULL;
        }
    }
    if (certList == NULL) {
        /* no user certs meeting the nickname/usage requirements found */
        return SECFailure;
    }
    /* now remove any certs that can't meet the connection requirements */
    rv = ssl_FilterClientCertListBySSLSocket(ss, certList);
    if ((rv != SECSuccess) || CERT_LIST_EMPTY(certList)) {
        // no certs left.
        CERT_DestroyCertList(certList);
        return SECFailure;
    }

    /* now return the top cert in the list. We've strived to make the
     * list ordered by the most likely usable cert, so it should be the best
     * match. */
    cert = CERT_DupCertificate(CERT_LIST_HEAD(certList)->cert);
    CERT_DestroyCertList(certList);
    privkey = PK11_FindKeyByAnyCert(cert, pw_arg);
    if (privkey == NULL) {
        CERT_DestroyCertificate(cert);
        return SECFailure;
    }
    *pRetCert = cert;
    *pRetKey = privkey;
    return SECSuccess;
}
