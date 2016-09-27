/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * SSL server certificate configuration functions.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ssl.h"
#include "sslimpl.h"
#include "secoid.h"   /* for SECOID_GetAlgorithmTag */
#include "pk11func.h" /* for PK11_ReferenceSlot */
#include "nss.h"      /* for NSS_RegisterShutdown */
#include "prinit.h"   /* for PR_CallOnceWithArg */

static const PRCallOnceType pristineCallOnce;
static PRCallOnceType setupServerCAListOnce;

static SECStatus
serverCAListShutdown(void *appData, void *nssData)
{
    PORT_Assert(ssl3_server_ca_list);
    if (ssl3_server_ca_list) {
        CERT_FreeDistNames(ssl3_server_ca_list);
        ssl3_server_ca_list = NULL;
    }
    setupServerCAListOnce = pristineCallOnce;
    return SECSuccess;
}

static PRStatus
serverCAListSetup(void *arg)
{
    CERTCertDBHandle *dbHandle = (CERTCertDBHandle *)arg;
    SECStatus rv = NSS_RegisterShutdown(serverCAListShutdown, NULL);
    PORT_Assert(SECSuccess == rv);
    if (SECSuccess == rv) {
        ssl3_server_ca_list = CERT_GetSSLCACerts(dbHandle);
        return PR_SUCCESS;
    }
    return PR_FAILURE;
}

sslServerCert *
ssl_NewServerCert(const sslServerCertType *certType)
{
    sslServerCert *sc = PORT_ZNew(sslServerCert);
    if (!sc) {
        return NULL;
    }
    memcpy(&sc->certType, certType, sizeof(sc->certType));
    sc->serverCert = NULL;
    sc->serverCertChain = NULL;
    sc->certStatusArray = NULL;
    sc->signedCertTimestamps.len = 0;
    return sc;
}

sslServerCert *
ssl_CopyServerCert(const sslServerCert *oc)
{
    sslServerCert *sc;

    sc = ssl_NewServerCert(&oc->certType);
    if (!sc) {
        return NULL;
    }

    if (oc->serverCert && oc->serverCertChain) {
        sc->serverCert = CERT_DupCertificate(oc->serverCert);
        if (!sc->serverCert)
            goto loser;
        sc->serverCertChain = CERT_DupCertList(oc->serverCertChain);
        if (!sc->serverCertChain)
            goto loser;
    } else {
        sc->serverCert = NULL;
        sc->serverCertChain = NULL;
    }

    if (oc->serverKeyPair) {
        sc->serverKeyPair = ssl_GetKeyPairRef(oc->serverKeyPair);
        if (!sc->serverKeyPair)
            goto loser;
    } else {
        sc->serverKeyPair = NULL;
    }
    sc->serverKeyBits = oc->serverKeyBits;

    if (oc->certStatusArray) {
        sc->certStatusArray = SECITEM_DupArray(NULL, oc->certStatusArray);
        if (!sc->certStatusArray)
            goto loser;
    } else {
        sc->certStatusArray = NULL;
    }

    if (SECITEM_CopyItem(NULL, &sc->signedCertTimestamps,
                         &oc->signedCertTimestamps) != SECSuccess)
        goto loser;
    return sc;
loser:
    ssl_FreeServerCert(sc);
    return NULL;
}

void
ssl_FreeServerCert(sslServerCert *sc)
{
    if (!sc) {
        return;
    }

    if (sc->serverCert) {
        CERT_DestroyCertificate(sc->serverCert);
    }
    if (sc->serverCertChain) {
        CERT_DestroyCertificateList(sc->serverCertChain);
    }
    if (sc->serverKeyPair) {
        ssl_FreeKeyPair(sc->serverKeyPair);
    }
    if (sc->certStatusArray) {
        SECITEM_FreeArray(sc->certStatusArray, PR_TRUE);
    }
    if (sc->signedCertTimestamps.len) {
        SECITEM_FreeItem(&sc->signedCertTimestamps, PR_FALSE);
    }
    PORT_ZFree(sc, sizeof(*sc));
}

sslServerCert *
ssl_FindServerCert(const sslSocket *ss,
                   const sslServerCertType *certType)
{
    PRCList *cursor;

    for (cursor = PR_NEXT_LINK(&ss->serverCerts);
         cursor != &ss->serverCerts;
         cursor = PR_NEXT_LINK(cursor)) {
        sslServerCert *cert = (sslServerCert *)cursor;
        if (cert->certType.authType != certType->authType) {
            continue;
        }
        switch (cert->certType.authType) {
            case ssl_auth_ecdsa:
            case ssl_auth_ecdh_rsa:
            case ssl_auth_ecdh_ecdsa:
                /* Note: For deprecated APIs, we need to be able to find and
                   match a slot with any named curve. */
                if (certType->namedCurve &&
                    cert->certType.namedCurve != certType->namedCurve) {
                    continue;
                }
                break;
            default:
                break;
        }
        return cert;
    }
    return NULL;
}

sslServerCert *
ssl_FindServerCertByAuthType(const sslSocket *ss, SSLAuthType authType)
{
    sslServerCertType certType;
    certType.authType = authType;
    /* Setting the named curve to NULL ensures that all EC certificates
     * are matched when searching for this slot. */
    certType.namedCurve = NULL;
    return ssl_FindServerCert(ss, &certType);
}

SECStatus
ssl_OneTimeCertSetup(sslSocket *ss, const sslServerCert *sc)
{
    if (PR_SUCCESS != PR_CallOnceWithArg(&setupServerCAListOnce,
                                         &serverCAListSetup,
                                         (void *)(ss->dbHandle))) {
        return SECFailure;
    }
    return SECSuccess;
}

/* Determine which slot a certificate fits into.  SSLAuthType is known, but
 * extra information needs to be worked out from the cert and key. */
static void
ssl_PopulateCertType(sslServerCertType *certType, SSLAuthType authType,
                     CERTCertificate *cert, sslKeyPair *keyPair)
{
    certType->authType = authType;
    switch (authType) {
        case ssl_auth_ecdsa:
        case ssl_auth_ecdh_rsa:
        case ssl_auth_ecdh_ecdsa:
            certType->namedCurve = ssl_ECPubKey2NamedGroup(keyPair->pubKey);
            break;
        default:
            break;
    }
}

static SECStatus
ssl_PopulateServerCert(sslServerCert *sc, CERTCertificate *cert,
                       const CERTCertificateList *certChain)
{
    if (sc->serverCert) {
        CERT_DestroyCertificate(sc->serverCert);
    }
    if (sc->serverCertChain) {
        CERT_DestroyCertificateList(sc->serverCertChain);
    }

    if (!cert) {
        sc->serverCert = NULL;
        sc->serverCertChain = NULL;
        return SECSuccess;
    }

    sc->serverCert = CERT_DupCertificate(cert);
    if (certChain) {
        sc->serverCertChain = CERT_DupCertList(certChain);
    } else {
        sc->serverCertChain =
            CERT_CertChainFromCert(sc->serverCert, certUsageSSLServer,
                                   PR_TRUE);
    }
    return sc->serverCertChain ? SECSuccess : SECFailure;
}

static SECStatus
ssl_PopulateKeyPair(sslServerCert *sc, sslKeyPair *keyPair)
{
    /* Copy over the key pair. */
    if (sc->serverKeyPair) {
        ssl_FreeKeyPair(sc->serverKeyPair);
    }
    if (keyPair) {
        /* Get the size of the cert's public key, and remember it. */
        sc->serverKeyBits = SECKEY_PublicKeyStrengthInBits(keyPair->pubKey);
        if (sc->serverKeyBits == 0) {
            return SECFailure;
        }

        SECKEY_CacheStaticFlags(keyPair->privKey);
        sc->serverKeyPair = ssl_GetKeyPairRef(keyPair);
    } else {
        sc->serverKeyPair = NULL;
    }
    return SECSuccess;
}

static SECStatus
ssl_PopulateOCSPResponses(sslServerCert *sc,
                          const SECItemArray *stapledOCSPResponses)
{
    if (sc->certStatusArray) {
        SECITEM_FreeArray(sc->certStatusArray, PR_TRUE);
    }
    if (stapledOCSPResponses) {
        sc->certStatusArray = SECITEM_DupArray(NULL, stapledOCSPResponses);
        return sc->certStatusArray ? SECSuccess : SECFailure;
    } else {
        sc->certStatusArray = NULL;
    }
    return SECSuccess;
}

static SECStatus
ssl_PopulateSignedCertTimestamps(sslServerCert *sc,
                                 const SECItem *signedCertTimestamps)
{
    if (sc->signedCertTimestamps.len) {
        SECITEM_FreeItem(&sc->signedCertTimestamps, PR_FALSE);
    }
    if (signedCertTimestamps) {
        return SECITEM_CopyItem(NULL, &sc->signedCertTimestamps,
                                signedCertTimestamps);
    }
    return SECSuccess;
}

static SECStatus
ssl_ConfigCert(sslSocket *ss, CERTCertificate *cert,
               sslKeyPair *keyPair, const SSLExtraServerCertData *data)
{
    sslServerCert *oldsc;
    sslServerCertType certType;
    SECStatus rv;
    sslServerCert *sc = NULL;
    int error_code = SEC_ERROR_NO_MEMORY;

    PORT_Assert(cert);
    PORT_Assert(keyPair);
    PORT_Assert(data);
    PORT_Assert(data->authType != ssl_auth_null);

    if (!cert || !keyPair || !data || data->authType == ssl_auth_null) {
        error_code = SEC_ERROR_INVALID_ARGS;
        goto loser;
    }

    ssl_PopulateCertType(&certType, data->authType, cert, keyPair);

    /* Delete any existing certificate that matches this one, since we can only
     * use one certificate of a given type. */
    oldsc = ssl_FindServerCert(ss, &certType);
    if (oldsc) {
        PR_REMOVE_LINK(&oldsc->link);
        ssl_FreeServerCert(oldsc);
    }
    sc = ssl_NewServerCert(&certType);
    if (!sc) {
        goto loser;
    }

    rv = ssl_PopulateServerCert(sc, cert, data->certChain);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = ssl_PopulateKeyPair(sc, keyPair);
    if (rv != SECSuccess) {
        error_code = SEC_ERROR_INVALID_ARGS;
        goto loser;
    }
    rv = ssl_PopulateOCSPResponses(sc, data->stapledOCSPResponses);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = ssl_PopulateSignedCertTimestamps(sc, data->signedCertTimestamps);
    if (rv != SECSuccess) {
        goto loser;
    }
    PR_APPEND_LINK(&sc->link, &ss->serverCerts);

    /* This one-time setup depends on having the certificate in place. */
    rv = ssl_OneTimeCertSetup(ss, sc);
    if (rv != SECSuccess) {
        PR_REMOVE_LINK(&sc->link);
        error_code = PORT_GetError();
        goto loser;
    }
    return SECSuccess;

loser:
    if (sc) {
        ssl_FreeServerCert(sc);
    }
    /* This is the only way any of the calls above can fail, except the one time
     * setup, which doesn't land here. */
    PORT_SetError(error_code);
    return SECFailure;
}

static SSLAuthType
ssl_GetEcdhAuthType(CERTCertificate *cert)
{
    SECOidTag sigTag = SECOID_GetAlgorithmTag(&cert->signature);
    switch (sigTag) {
        case SEC_OID_PKCS1_RSA_ENCRYPTION:
        case SEC_OID_PKCS1_RSA_PSS_SIGNATURE:
        case SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION:
        case SEC_OID_PKCS1_MD4_WITH_RSA_ENCRYPTION:
        case SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION:
        case SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION:
        case SEC_OID_PKCS1_SHA224_WITH_RSA_ENCRYPTION:
        case SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION:
        case SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION:
        case SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION:
            return ssl_auth_ecdh_rsa;
        case SEC_OID_ANSIX962_ECDSA_SHA1_SIGNATURE:
        case SEC_OID_ANSIX962_ECDSA_SHA224_SIGNATURE:
        case SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE:
        case SEC_OID_ANSIX962_ECDSA_SHA384_SIGNATURE:
        case SEC_OID_ANSIX962_ECDSA_SHA512_SIGNATURE:
        case SEC_OID_ANSIX962_ECDSA_SIGNATURE_RECOMMENDED_DIGEST:
        case SEC_OID_ANSIX962_ECDSA_SIGNATURE_SPECIFIED_DIGEST:
            return ssl_auth_ecdh_ecdsa;
        default:
            return ssl_auth_null;
    }
}

/* This function examines the key usages of the given RSA-PKCS1 certificate
 * and configures one or multiple server certificates based on that data.
 *
 * If the data argument contains an authType value other than ssl_auth_null,
 * then only that slot will be used.  If that choice is invalid,
 * then this will fail. */
static SECStatus
ssl_ConfigRsaPkcs1CertByUsage(sslSocket *ss, CERTCertificate *cert,
                              sslKeyPair *keyPair,
                              SSLExtraServerCertData *data)
{
    SECStatus rv = SECFailure;

    PRBool ku_sig = (PRBool)(cert->keyUsage & KU_DIGITAL_SIGNATURE);
    PRBool ku_enc = (PRBool)(cert->keyUsage & KU_KEY_ENCIPHERMENT);

    if ((data->authType == ssl_auth_rsa_sign && ku_sig) ||
        (data->authType == ssl_auth_rsa_pss && ku_sig) ||
        (data->authType == ssl_auth_rsa_decrypt && ku_enc)) {
        return ssl_ConfigCert(ss, cert, keyPair, data);
    }

    if (data->authType != ssl_auth_null || !(ku_sig || ku_enc)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (ku_sig) {
        data->authType = ssl_auth_rsa_sign;
        rv = ssl_ConfigCert(ss, cert, keyPair, data);
        if (rv != SECSuccess) {
            return rv;
        }

        /* This certificate is RSA, assume that it's also PSS. */
        data->authType = ssl_auth_rsa_pss;
        rv = ssl_ConfigCert(ss, cert, keyPair, data);
        if (rv != SECSuccess) {
            return rv;
        }
    }

    if (ku_enc) {
        /* If ku_sig=true we configure signature and encryption slots with the
         * same cert. This is bad form, but there are enough dual-usage RSA
         * certs that we can't really break by limiting this to one type. */
        data->authType = ssl_auth_rsa_decrypt;
        rv = ssl_ConfigCert(ss, cert, keyPair, data);
        if (rv != SECSuccess) {
            return rv;
        }
    }

    return rv;
}

/* This function examines the type of certificate and its key usage and
 * configures a certificate based on that information.  For some certificates
 * this can mean that multiple server certificates are configured.
 *
 * If the data argument contains an authType value other than ssl_auth_null,
 * then only that slot will be used.  If that choice is invalid,
 * then this will fail. */
static SECStatus
ssl_ConfigCertByUsage(sslSocket *ss, CERTCertificate *cert,
                      sslKeyPair *keyPair, const SSLExtraServerCertData *data)
{
    SECStatus rv = SECFailure;
    SSLExtraServerCertData arg;
    SECOidTag tag;

    PORT_Assert(data);
    /* Take a (shallow) copy so that we can play with it */
    memcpy(&arg, data, sizeof(arg));

    tag = SECOID_GetAlgorithmTag(&cert->subjectPublicKeyInfo.algorithm);
    switch (tag) {
        case SEC_OID_X500_RSA_ENCRYPTION:
        case SEC_OID_PKCS1_RSA_ENCRYPTION:
            return ssl_ConfigRsaPkcs1CertByUsage(ss, cert, keyPair, &arg);

        case SEC_OID_PKCS1_RSA_PSS_SIGNATURE:
            if (cert->keyUsage & KU_DIGITAL_SIGNATURE) {
                arg.authType = ssl_auth_rsa_pss;
            }
            break;

        case SEC_OID_ANSIX9_DSA_SIGNATURE:
            if (cert->keyUsage & KU_DIGITAL_SIGNATURE) {
                arg.authType = ssl_auth_dsa;
            }
            break;

        case SEC_OID_ANSIX962_EC_PUBLIC_KEY:
            if (cert->keyUsage & KU_KEY_ENCIPHERMENT) {
                if ((cert->keyUsage & KU_DIGITAL_SIGNATURE) &&
                    arg.authType == ssl_auth_null) {
                    /* See above regarding bad practice. */
                    arg.authType = ssl_auth_ecdsa;
                    rv = ssl_ConfigCert(ss, cert, keyPair, &arg);
                    if (rv != SECSuccess) {
                        return rv;
                    }
                }

                arg.authType = ssl_GetEcdhAuthType(cert);
            } else if (cert->keyUsage & KU_DIGITAL_SIGNATURE) {
                arg.authType = ssl_auth_ecdsa;
            }
            break;

        default:
            break;
    }

    /* Check that we successfully picked an authType */
    if (arg.authType == ssl_auth_null) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    /* |data->authType| has to either agree or be ssl_auth_null. */
    if (data && data->authType != ssl_auth_null &&
        data->authType != arg.authType) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    return ssl_ConfigCert(ss, cert, keyPair, &arg);
}

/* This function adopts pubKey and destroys it if things go wrong. */
static sslKeyPair *
ssl_MakeKeyPairForCert(SECKEYPrivateKey *key, SECKEYPublicKey *pubKey)
{
    sslKeyPair *keyPair = NULL;
    SECKEYPrivateKey *privKeyCopy = NULL;
    PK11SlotInfo *bestSlot;

    if (key->pkcs11Slot) {
        bestSlot = PK11_ReferenceSlot(key->pkcs11Slot);
        if (bestSlot) {
            privKeyCopy = PK11_CopyTokenPrivKeyToSessionPrivKey(bestSlot, key);
            PK11_FreeSlot(bestSlot);
        }
    }
    if (!privKeyCopy) {
        CK_MECHANISM_TYPE keyMech = PK11_MapSignKeyType(key->keyType);
        /* XXX Maybe should be bestSlotMultiple? */
        bestSlot = PK11_GetBestSlot(keyMech, NULL /* wincx */);
        if (bestSlot) {
            privKeyCopy = PK11_CopyTokenPrivKeyToSessionPrivKey(bestSlot, key);
            PK11_FreeSlot(bestSlot);
        }
    }
    if (!privKeyCopy) {
        privKeyCopy = SECKEY_CopyPrivateKey(key);
    }
    if (privKeyCopy) {
        keyPair = ssl_NewKeyPair(privKeyCopy, pubKey);
    }
    if (!keyPair) {
        if (privKeyCopy) {
            SECKEY_DestroyPrivateKey(privKeyCopy);
        }
        /* We adopted the public key, so we're responsible. */
        if (pubKey) {
            SECKEY_DestroyPublicKey(pubKey);
        }
    }
    return keyPair;
}

/* Configure a certificate and private key.
 *
 * This function examines the certificate and key to determine which slot (or
 * slots) to place the information in.  As long as certificates are different
 * (based on having different values of sslServerCertType), then this function
 * can be called multiple times and the certificates will all be remembered.
 */
SECStatus
SSL_ConfigServerCert(PRFileDesc *fd, CERTCertificate *cert,
                     SECKEYPrivateKey *key,
                     const SSLExtraServerCertData *data, unsigned int data_len)
{
    sslSocket *ss;
    SECKEYPublicKey *pubKey;
    sslKeyPair *keyPair;
    SECStatus rv;
    SSLExtraServerCertData dataCopy = {
        ssl_auth_null, NULL, NULL, NULL
    };

    ss = ssl_FindSocket(fd);
    if (!ss) {
        return SECFailure;
    }

    if (!cert || !key) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (data) {
        if (data_len > sizeof(dataCopy)) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return SECFailure;
        }
        PORT_Memcpy(&dataCopy, data, data_len);
    }

    pubKey = CERT_ExtractPublicKey(cert);
    if (!pubKey) {
        return SECFailure;
    }

    keyPair = ssl_MakeKeyPairForCert(key, pubKey);
    if (!keyPair) {
        /* pubKey is adopted by ssl_MakeKeyPairForCert() */
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }

    rv = ssl_ConfigCertByUsage(ss, cert, keyPair, &dataCopy);
    ssl_FreeKeyPair(keyPair);
    return rv;
}

/*******************************************************************/
/* Deprecated functions.
 *
 * The remainder of this file contains deprecated functions for server
 * certificate configuration.  These configure certificates incorrectly, but in
 * a way that allows old code to continue working without change.  All these
 * functions create certificate slots based on SSLKEAType values.  Some values
 * of SSLKEAType cause multiple certificates to be configured.
 */

SECStatus
SSL_ConfigSecureServer(PRFileDesc *fd, CERTCertificate *cert,
                       SECKEYPrivateKey *key, SSLKEAType kea)
{
    return SSL_ConfigSecureServerWithCertChain(fd, cert, NULL, key, kea);
}

/* This implements a limited check that is consistent with the checks performed
 * by older versions of NSS.  This is less rigorous than the checks in
 * ssl_ConfigCertByUsage(), only checking against the type of key and ignoring
 * things like usage. */
static PRBool
ssl_CertSuitableForAuthType(CERTCertificate *cert, SSLAuthType authType)
{
    SECOidTag tag = SECOID_GetAlgorithmTag(&cert->subjectPublicKeyInfo.algorithm);
    switch (authType) {
        case ssl_auth_rsa_decrypt:
        case ssl_auth_rsa_sign:
            return tag == SEC_OID_X500_RSA_ENCRYPTION ||
                   tag == SEC_OID_PKCS1_RSA_ENCRYPTION;
        case ssl_auth_dsa:
            return tag == SEC_OID_ANSIX9_DSA_SIGNATURE;
        case ssl_auth_ecdsa:
        case ssl_auth_ecdh_rsa:
        case ssl_auth_ecdh_ecdsa:
            return tag == SEC_OID_ANSIX962_EC_PUBLIC_KEY;
        case ssl_auth_null:
        case ssl_auth_kea:
        case ssl_auth_rsa_pss: /* not supported with deprecated APIs */
            return PR_FALSE;
        default:
            PORT_Assert(0);
            return PR_FALSE;
    }
}

/* This finds an existing server cert slot and unlinks it, or it makes a new
 * server cert slot of the right type. */
static sslServerCert *
ssl_FindOrMakeCertType(sslSocket *ss, SSLAuthType authType)
{
    sslServerCert *sc;
    sslServerCertType certType;

    certType.authType = authType;
    /* Setting the named curve to NULL ensures that all EC certificates
     * are matched when searching for this slot. */
    certType.namedCurve = NULL;
    sc = ssl_FindServerCert(ss, &certType);
    if (sc) {
        PR_REMOVE_LINK(&sc->link);
        return sc;
    }

    return ssl_NewServerCert(&certType);
}

static void
ssl_RemoveCertAndKeyByAuthType(sslSocket *ss, SSLAuthType authType)
{
    sslServerCert *sc;

    sc = ssl_FindServerCertByAuthType(ss, authType);
    if (sc) {
        (void)ssl_PopulateServerCert(sc, NULL, NULL);
        (void)ssl_PopulateKeyPair(sc, NULL);
        /* Leave the entry linked here because the old API expects that.  There
         * might be OCSP stapling values or signed certificate timestamps still
         * present that will subsequently be used. */
        /* For ECC certificates, also leave the namedCurve parameter on the slot
         * unchanged; the value will be updated when a key is added. */
    }
}

static SECStatus
ssl_AddCertAndKeyByAuthType(sslSocket *ss, SSLAuthType authType,
                            CERTCertificate *cert,
                            const CERTCertificateList *certChainOpt,
                            sslKeyPair *keyPair)
{
    sslServerCert *sc;
    SECStatus rv;

    if (!ssl_CertSuitableForAuthType(cert, authType)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    sc = ssl_FindOrMakeCertType(ss, authType);
    if (!sc) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }
    rv = ssl_PopulateKeyPair(sc, keyPair);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        goto loser;
    }
    /* Now that we have a key pair, update the details of the slot. Many of the
     * legacy functions create a slot with a namedCurve of NULL, which
     * makes the slot unusable; this corrects that. */
    ssl_PopulateCertType(&sc->certType, authType, cert, keyPair);
    rv = ssl_PopulateServerCert(sc, cert, certChainOpt);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        goto loser;
    }
    PR_APPEND_LINK(&sc->link, &ss->serverCerts);
    return ssl_OneTimeCertSetup(ss, sc);
loser:
    ssl_FreeServerCert(sc);
    return SECFailure;
}

static SECStatus
ssl_AddCertsByKEA(sslSocket *ss, CERTCertificate *cert,
                  const CERTCertificateList *certChainOpt,
                  SECKEYPrivateKey *key, SSLKEAType certType)
{
    SECKEYPublicKey *pubKey;
    sslKeyPair *keyPair;
    SECStatus rv;

    pubKey = CERT_ExtractPublicKey(cert);
    if (!pubKey) {
        return SECFailure;
    }

    keyPair = ssl_MakeKeyPairForCert(key, pubKey);
    if (!keyPair) {
        /* Note: pubKey is adopted or freed by ssl_MakeKeyPairForCert()
         * depending on whether it succeeds or not. */
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }

    switch (certType) {
        case ssl_kea_rsa:
            rv = ssl_AddCertAndKeyByAuthType(ss, ssl_auth_rsa_decrypt,
                                             cert, certChainOpt, keyPair);
            if (rv != SECSuccess) {
                return SECFailure;
            }
            rv = ssl_AddCertAndKeyByAuthType(ss, ssl_auth_rsa_sign,
                                             cert, certChainOpt, keyPair);
            break;

        case ssl_kea_dh:
            rv = ssl_AddCertAndKeyByAuthType(ss, ssl_auth_dsa,
                                             cert, certChainOpt, keyPair);
            break;

        case ssl_kea_ecdh:
            rv = ssl_AddCertAndKeyByAuthType(ss, ssl_auth_ecdsa,
                                             cert, certChainOpt, keyPair);
            if (rv != SECSuccess) {
                return SECFailure;
            }
            rv = ssl_AddCertAndKeyByAuthType(ss, ssl_GetEcdhAuthType(cert),
                                             cert, certChainOpt, keyPair);
            break;

        default:
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            rv = SECFailure;
            break;
    }

    ssl_FreeKeyPair(keyPair);
    return rv;
}

/* Public deprecated function */
SECStatus
SSL_ConfigSecureServerWithCertChain(PRFileDesc *fd, CERTCertificate *cert,
                                    const CERTCertificateList *certChainOpt,
                                    SECKEYPrivateKey *key, SSLKEAType certType)
{
    sslSocket *ss;

    ss = ssl_FindSocket(fd);
    if (!ss) {
        return SECFailure;
    }

    if (!cert != !key) { /* Configure both, or neither */
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (!cert) {
        switch (certType) {
            case ssl_kea_rsa:
                ssl_RemoveCertAndKeyByAuthType(ss, ssl_auth_rsa_decrypt);
                ssl_RemoveCertAndKeyByAuthType(ss, ssl_auth_rsa_sign);
                break;

            case ssl_kea_dh:
                ssl_RemoveCertAndKeyByAuthType(ss, ssl_auth_dsa);
                break;

            case ssl_kea_ecdh:
                ssl_RemoveCertAndKeyByAuthType(ss, ssl_auth_ecdsa);
                ssl_RemoveCertAndKeyByAuthType(ss, ssl_auth_ecdh_rsa);
                ssl_RemoveCertAndKeyByAuthType(ss, ssl_auth_ecdh_ecdsa);
                break;

            default:
                PORT_SetError(SEC_ERROR_INVALID_ARGS);
                return SECFailure;
        }
        return SECSuccess;
    }

    return ssl_AddCertsByKEA(ss, cert, certChainOpt, key, certType);
}

static SECStatus
ssl_SetOCSPResponsesInSlot(sslSocket *ss, SSLAuthType authType,
                           const SECItemArray *responses)
{
    sslServerCert *sc;
    SECStatus rv;

    sc = ssl_FindOrMakeCertType(ss, authType);
    if (!sc) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }
    rv = ssl_PopulateOCSPResponses(sc, responses);
    if (rv == SECSuccess) {
        PR_APPEND_LINK(&sc->link, &ss->serverCerts);
    } else {
        ssl_FreeServerCert(sc);
    }
    return rv;
}

/* Public deprecated function */
SECStatus
SSL_SetStapledOCSPResponses(PRFileDesc *fd, const SECItemArray *responses,
                            SSLKEAType certType)
{
    sslSocket *ss;
    SECStatus rv;

    ss = ssl_FindSocket(fd);
    if (!ss) {
        SSL_DBG(("%d: SSL[%d]: bad socket in SSL_SetStapledOCSPResponses",
                 SSL_GETPID(), fd));
        return SECFailure;
    }

    switch (certType) {
        case ssl_kea_rsa:
            rv = ssl_SetOCSPResponsesInSlot(ss, ssl_auth_rsa_decrypt, responses);
            if (rv != SECSuccess) {
                return SECFailure;
            }
            return ssl_SetOCSPResponsesInSlot(ss, ssl_auth_rsa_sign, responses);

        case ssl_kea_dh:
            return ssl_SetOCSPResponsesInSlot(ss, ssl_auth_dsa, responses);

        case ssl_kea_ecdh:
            rv = ssl_SetOCSPResponsesInSlot(ss, ssl_auth_ecdsa, responses);
            if (rv != SECSuccess) {
                return SECFailure;
            }
            rv = ssl_SetOCSPResponsesInSlot(ss, ssl_auth_ecdh_rsa, responses);
            if (rv != SECSuccess) {
                return SECFailure;
            }
            return ssl_SetOCSPResponsesInSlot(ss, ssl_auth_ecdh_ecdsa, responses);

        default:
            SSL_DBG(("%d: SSL[%d]: invalid cert type in SSL_SetStapledOCSPResponses",
                     SSL_GETPID(), fd));
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return SECFailure;
    }
}

static SECStatus
ssl_SetSignedTimestampsInSlot(sslSocket *ss, SSLAuthType authType,
                              const SECItem *scts)
{
    sslServerCert *sc;
    SECStatus rv;

    sc = ssl_FindOrMakeCertType(ss, authType);
    if (!sc) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }
    rv = ssl_PopulateSignedCertTimestamps(sc, scts);
    if (rv == SECSuccess) {
        PR_APPEND_LINK(&sc->link, &ss->serverCerts);
    } else {
        ssl_FreeServerCert(sc);
    }
    return rv;
}

/* Public deprecated function */
SECStatus
SSL_SetSignedCertTimestamps(PRFileDesc *fd, const SECItem *scts,
                            SSLKEAType certType)
{
    sslSocket *ss;
    SECStatus rv;

    ss = ssl_FindSocket(fd);
    if (!ss) {
        SSL_DBG(("%d: SSL[%d]: bad socket in SSL_SetSignedCertTimestamps",
                 SSL_GETPID(), fd));
        return SECFailure;
    }

    switch (certType) {
        case ssl_kea_rsa:
            rv = ssl_SetSignedTimestampsInSlot(ss, ssl_auth_rsa_decrypt, scts);
            if (rv != SECSuccess) {
                return SECFailure;
            }
            return ssl_SetSignedTimestampsInSlot(ss, ssl_auth_rsa_sign, scts);

        case ssl_kea_dh:
            return ssl_SetSignedTimestampsInSlot(ss, ssl_auth_dsa, scts);

        case ssl_kea_ecdh:
            rv = ssl_SetSignedTimestampsInSlot(ss, ssl_auth_ecdsa, scts);
            if (rv != SECSuccess) {
                return SECFailure;
            }
            rv = ssl_SetSignedTimestampsInSlot(ss, ssl_auth_ecdh_rsa, scts);
            if (rv != SECSuccess) {
                return SECFailure;
            }
            return ssl_SetSignedTimestampsInSlot(ss, ssl_auth_ecdh_ecdsa, scts);

        default:
            SSL_DBG(("%d: SSL[%d]: invalid cert type in SSL_SetSignedCertTimestamps",
                     SSL_GETPID(), fd));
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return SECFailure;
    }
}

/* Public deprecated function. */
SSLKEAType
NSS_FindCertKEAType(CERTCertificate *cert)
{
    int tag;

    if (!cert)
        return ssl_kea_null;

    tag = SECOID_GetAlgorithmTag(&(cert->subjectPublicKeyInfo.algorithm));
    switch (tag) {
        case SEC_OID_X500_RSA_ENCRYPTION:
        case SEC_OID_PKCS1_RSA_ENCRYPTION:
            return ssl_kea_rsa;
        case SEC_OID_ANSIX9_DSA_SIGNATURE: /* hah, signature, not a key? */
        case SEC_OID_X942_DIFFIE_HELMAN_KEY:
            return ssl_kea_dh;
        case SEC_OID_ANSIX962_EC_PUBLIC_KEY:
            return ssl_kea_ecdh;
        default:
            return ssl_kea_null;
    }
}
