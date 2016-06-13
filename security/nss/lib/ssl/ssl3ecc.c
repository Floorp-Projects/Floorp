/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * SSL3 Protocol
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* ECC code moved here from ssl3con.c */

#include "nss.h"
#include "cert.h"
#include "ssl.h"
#include "cryptohi.h" /* for DSAU_ stuff */
#include "keyhi.h"
#include "secder.h"
#include "secitem.h"

#include "sslimpl.h"
#include "sslproto.h"
#include "sslerr.h"
#include "prtime.h"
#include "prinrval.h"
#include "prerror.h"
#include "pratom.h"
#include "prthread.h"
#include "prinit.h"

#include "pk11func.h"
#include "secmod.h"

#include <stdio.h>

#ifndef PK11_SETATTRS
#define PK11_SETATTRS(x, id, v, l) \
    (x)->type = (id);              \
    (x)->pValue = (v);             \
    (x)->ulValueLen = (l);
#endif

static SECStatus ssl_CreateECDHEphemeralKeys(sslSocket *ss,
                                             const namedGroupDef *ecGroup);

typedef struct ECDHEKeyPairStr {
    sslEphemeralKeyPair *pair;
    int error; /* error code of the call-once function */
    PRCallOnceType once;
} ECDHEKeyPair;

/* arrays of ECDHE KeyPairs */
static PRCallOnceType gECDHEInitOnce;
static int gECDHEInitError;
/* This must be the same as ssl_named_groups_count.  ssl_ECRegister() asserts
 * this at runtime; no compile-time error, sorry. */
static ECDHEKeyPair gECDHEKeyPairs[29];

SECStatus
ssl_NamedGroup2ECParams(PLArenaPool *arena, const namedGroupDef *ecGroup,
                        SECKEYECParams *params)
{
    SECOidData *oidData = NULL;
    PRUint32 policyFlags = 0;
    SECStatus rv;

    if (!ecGroup || ecGroup->type != group_type_ec ||
        (oidData = SECOID_FindOIDByTag(ecGroup->oidTag)) == NULL) {
        PORT_SetError(SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE);
        return SECFailure;
    }

    rv = NSS_GetAlgorithmPolicy(ecGroup->oidTag, &policyFlags);
    if (rv == SECSuccess && !(policyFlags & NSS_USE_ALG_IN_SSL_KX)) {
        PORT_SetError(SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE);
        return SECFailure;
    }

    SECITEM_AllocItem(arena, params, (2 + oidData->oid.len));
    /*
     * params->data needs to contain the ASN encoding of an object ID (OID)
     * representing the named curve. The actual OID is in
     * oidData->oid.data so we simply prepend 0x06 and OID length
     */
    params->data[0] = SEC_ASN1_OBJECT_ID;
    params->data[1] = oidData->oid.len;
    memcpy(params->data + 2, oidData->oid.data, oidData->oid.len);

    return SECSuccess;
}

const namedGroupDef *
ssl_ECPubKey2NamedGroup(const SECKEYPublicKey *pubKey)
{
    SECItem oid = { siBuffer, NULL, 0 };
    SECOidData *oidData = NULL;
    PRUint32 policyFlags = 0;
    unsigned int i;
    const SECKEYECParams *params;

    if (pubKey->keyType != ecKey) {
        PORT_Assert(0);
        return NULL;
    }

    params = &pubKey->u.ec.DEREncodedParams;

    /*
     * params->data needs to contain the ASN encoding of an object ID (OID)
     * representing a named curve. Here, we strip away everything
     * before the actual OID and use the OID to look up a named curve.
     */
    if (params->data[0] != SEC_ASN1_OBJECT_ID)
        return NULL;
    oid.len = params->len - 2;
    oid.data = params->data + 2;
    if ((oidData = SECOID_FindOID(&oid)) == NULL)
        return NULL;
    if ((NSS_GetAlgorithmPolicy(oidData->offset, &policyFlags) ==
         SECSuccess) &&
        !(policyFlags & NSS_USE_ALG_IN_SSL_KX)) {
        return NULL;
    }
    for (i = 0; i < ssl_named_group_count; ++i) {
        if (ssl_named_groups[i].oidTag == oidData->offset) {
            return &ssl_named_groups[i];
        }
    }

    return NULL;
}

/* Caller must set hiLevel error code. */
static SECStatus
ssl3_ComputeECDHKeyHash(SSLHashType hashAlg,
                        SECItem ec_params, SECItem server_ecpoint,
                        SSL3Random *client_rand, SSL3Random *server_rand,
                        SSL3Hashes *hashes, PRBool bypassPKCS11)
{
    PRUint8 *hashBuf;
    PRUint8 *pBuf;
    SECStatus rv = SECSuccess;
    unsigned int bufLen;
    /*
     * XXX For now, we only support named curves (the appropriate
     * checks are made before this method is called) so ec_params
     * takes up only two bytes. ECPoint needs to fit in 256 bytes
     * (because the spec says the length must fit in one byte)
     */
    PRUint8 buf[2 * SSL3_RANDOM_LENGTH + 2 + 1 + 256];

    bufLen = 2 * SSL3_RANDOM_LENGTH + ec_params.len + 1 + server_ecpoint.len;
    if (bufLen <= sizeof buf) {
        hashBuf = buf;
    } else {
        hashBuf = PORT_Alloc(bufLen);
        if (!hashBuf) {
            return SECFailure;
        }
    }

    memcpy(hashBuf, client_rand, SSL3_RANDOM_LENGTH);
    pBuf = hashBuf + SSL3_RANDOM_LENGTH;
    memcpy(pBuf, server_rand, SSL3_RANDOM_LENGTH);
    pBuf += SSL3_RANDOM_LENGTH;
    memcpy(pBuf, ec_params.data, ec_params.len);
    pBuf += ec_params.len;
    pBuf[0] = (PRUint8)(server_ecpoint.len);
    pBuf += 1;
    memcpy(pBuf, server_ecpoint.data, server_ecpoint.len);
    pBuf += server_ecpoint.len;
    PORT_Assert((unsigned int)(pBuf - hashBuf) == bufLen);

    rv = ssl3_ComputeCommonKeyHash(hashAlg, hashBuf, bufLen, hashes,
                                   bypassPKCS11);

    PRINT_BUF(95, (NULL, "ECDHkey hash: ", hashBuf, bufLen));
    PRINT_BUF(95, (NULL, "ECDHkey hash: MD5 result",
                   hashes->u.s.md5, MD5_LENGTH));
    PRINT_BUF(95, (NULL, "ECDHkey hash: SHA1 result",
                   hashes->u.s.sha, SHA1_LENGTH));

    if (hashBuf != buf)
        PORT_Free(hashBuf);
    return rv;
}

/* Called from ssl3_SendClientKeyExchange(). */
SECStatus
ssl3_SendECDHClientKeyExchange(sslSocket *ss, SECKEYPublicKey *svrPubKey)
{
    PK11SymKey *pms = NULL;
    SECStatus rv = SECFailure;
    PRBool isTLS, isTLS12;
    CK_MECHANISM_TYPE target;
    const namedGroupDef *groupDef;
    sslEphemeralKeyPair *keyPair = NULL;
    SECKEYPublicKey *pubKey;

    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));

    isTLS = (PRBool)(ss->ssl3.pwSpec->version > SSL_LIBRARY_VERSION_3_0);
    isTLS12 = (PRBool)(ss->ssl3.pwSpec->version >= SSL_LIBRARY_VERSION_TLS_1_2);

    /* Generate ephemeral EC keypair */
    if (svrPubKey->keyType != ecKey) {
        PORT_SetError(SEC_ERROR_BAD_KEY);
        goto loser;
    }
    groupDef = ssl_ECPubKey2NamedGroup(svrPubKey);
    if (!groupDef) {
        PORT_SetError(SEC_ERROR_BAD_KEY);
        goto loser;
    }
    rv = ssl_CreateECDHEphemeralKeyPair(groupDef, &keyPair);
    if (rv != SECSuccess) {
        ssl_MapLowLevelError(SEC_ERROR_KEYGEN_FAIL);
        goto loser;
    }

    pubKey = keyPair->keys->pubKey;
    PRINT_BUF(50, (ss, "ECDH public value:",
                   pubKey->u.ec.publicValue.data,
                   pubKey->u.ec.publicValue.len));

    if (isTLS12) {
        target = CKM_TLS12_MASTER_KEY_DERIVE_DH;
    } else if (isTLS) {
        target = CKM_TLS_MASTER_KEY_DERIVE_DH;
    } else {
        target = CKM_SSL3_MASTER_KEY_DERIVE_DH;
    }

    /* Determine the PMS */
    pms = PK11_PubDeriveWithKDF(keyPair->keys->privKey, svrPubKey,
                                PR_FALSE, NULL, NULL, CKM_ECDH1_DERIVE, target,
                                CKA_DERIVE, 0, CKD_NULL, NULL, NULL);

    if (pms == NULL) {
        (void)SSL3_SendAlert(ss, alert_fatal, illegal_parameter);
        ssl_MapLowLevelError(SSL_ERROR_CLIENT_KEY_EXCHANGE_FAILURE);
        goto loser;
    }

    rv = ssl3_AppendHandshakeHeader(ss, client_key_exchange,
                                    pubKey->u.ec.publicValue.len + 1);
    if (rv != SECSuccess) {
        goto loser; /* err set by ssl3_AppendHandshake* */
    }

    rv = ssl3_AppendHandshakeVariable(ss, pubKey->u.ec.publicValue.data,
                                      pubKey->u.ec.publicValue.len, 1);

    if (rv != SECSuccess) {
        goto loser; /* err set by ssl3_AppendHandshake* */
    }

    rv = ssl3_InitPendingCipherSpec(ss, pms);
    if (rv != SECSuccess) {
        ssl_MapLowLevelError(SSL_ERROR_CLIENT_KEY_EXCHANGE_FAILURE);
        goto loser;
    }

    PK11_FreeSymKey(pms);
    ssl_FreeEphemeralKeyPair(keyPair);
    return SECSuccess;

loser:
    if (pms)
        PK11_FreeSymKey(pms);
    if (keyPair)
        ssl_FreeEphemeralKeyPair(keyPair);
    return SECFailure;
}

/* This function returns the size of the key_exchange field in
 * the KeyShareEntry structure, i.e.:
 *     opaque point <1..2^8-1>; */
unsigned int
tls13_SizeOfECDHEKeyShareKEX(const SECKEYPublicKey *pubKey)
{
    PORT_Assert(pubKey->keyType == ecKey);
    return 1 + pubKey->u.ec.publicValue.len;
}

/* This function encodes the key_exchange field in
 * the KeyShareEntry structure. */
SECStatus
tls13_EncodeECDHEKeyShareKEX(sslSocket *ss, const SECKEYPublicKey *pubKey)
{
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveXmitBufLock(ss));
    PORT_Assert(pubKey->keyType == ecKey);

    return ssl3_AppendHandshakeVariable(ss, pubKey->u.ec.publicValue.data,
                                        pubKey->u.ec.publicValue.len, 1);
}

/*
** Called from ssl3_HandleClientKeyExchange()
*/
SECStatus
ssl3_HandleECDHClientKeyExchange(sslSocket *ss, SSL3Opaque *b,
                                 PRUint32 length,
                                 sslKeyPair *serverKeyPair)
{
    PK11SymKey *pms;
    SECStatus rv;
    SECKEYPublicKey clntPubKey;
    CK_MECHANISM_TYPE target;
    PRBool isTLS, isTLS12;
    int errCode = SSL_ERROR_RX_MALFORMED_CLIENT_KEY_EXCH;

    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    clntPubKey.keyType = ecKey;
    clntPubKey.u.ec.DEREncodedParams.len =
        serverKeyPair->pubKey->u.ec.DEREncodedParams.len;
    clntPubKey.u.ec.DEREncodedParams.data =
        serverKeyPair->pubKey->u.ec.DEREncodedParams.data;

    rv = ssl3_ConsumeHandshakeVariable(ss, &clntPubKey.u.ec.publicValue,
                                       1, &b, &length);
    if (rv != SECSuccess) {
        PORT_SetError(errCode);
        return SECFailure;
    }

    /* we have to catch the case when the client's public key has length 0. */
    if (!clntPubKey.u.ec.publicValue.len) {
        (void)SSL3_SendAlert(ss, alert_fatal, illegal_parameter);
        PORT_SetError(errCode);
        return SECFailure;
    }

    isTLS = (PRBool)(ss->ssl3.prSpec->version > SSL_LIBRARY_VERSION_3_0);
    isTLS12 = (PRBool)(ss->ssl3.prSpec->version >= SSL_LIBRARY_VERSION_TLS_1_2);

    if (isTLS12) {
        target = CKM_TLS12_MASTER_KEY_DERIVE_DH;
    } else if (isTLS) {
        target = CKM_TLS_MASTER_KEY_DERIVE_DH;
    } else {
        target = CKM_SSL3_MASTER_KEY_DERIVE_DH;
    }

    /*  Determine the PMS */
    pms = PK11_PubDeriveWithKDF(serverKeyPair->privKey, &clntPubKey,
                                PR_FALSE, NULL, NULL,
                                CKM_ECDH1_DERIVE, target, CKA_DERIVE, 0,
                                CKD_NULL, NULL, NULL);

    if (pms == NULL) {
        /* last gasp.  */
        errCode = ssl_MapLowLevelError(SSL_ERROR_CLIENT_KEY_EXCHANGE_FAILURE);
        PORT_SetError(errCode);
        return SECFailure;
    }

    rv = ssl3_InitPendingCipherSpec(ss, pms);
    PK11_FreeSymKey(pms);
    if (rv != SECSuccess) {
        /* error code set by ssl3_InitPendingCipherSpec */
        return SECFailure;
    }
    return SECSuccess;
}

/*
** Take an encoded key share and make a public key out of it.
** returns NULL on error.
*/
SECStatus
tls13_ImportECDHKeyShare(sslSocket *ss, SECKEYPublicKey *peerKey,
                         SSL3Opaque *b, PRUint32 length,
                         const namedGroupDef *ecGroup)
{
    SECStatus rv;
    SECItem ecPoint = { siBuffer, NULL, 0 };

    PORT_Assert(ss->opt.noLocks || ssl_HaveRecvBufLock(ss));
    PORT_Assert(ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss));

    rv = ssl3_ConsumeHandshakeVariable(ss, &ecPoint, 1, &b, &length);
    if (rv != SECSuccess) {
        PORT_SetError(SSL_ERROR_RX_MALFORMED_ECDHE_KEY_SHARE);
        return SECFailure;
    }
    if (length || !ecPoint.len) {
        PORT_SetError(SSL_ERROR_RX_MALFORMED_ECDHE_KEY_SHARE);
        return SECFailure;
    }

    /* Fail if the ec point uses compressed representation */
    if (ecPoint.data[0] != EC_POINT_FORM_UNCOMPRESSED) {
        PORT_SetError(SEC_ERROR_UNSUPPORTED_EC_POINT_FORM);
        return SECFailure;
    }

    peerKey->keyType = ecKey;
    /* Set up the encoded params */
    rv = ssl_NamedGroup2ECParams(peerKey->arena, ecGroup,
                                 &peerKey->u.ec.DEREncodedParams);
    if (rv != SECSuccess) {
        ssl_MapLowLevelError(SSL_ERROR_RX_MALFORMED_ECDHE_KEY_SHARE);
        return SECFailure;
    }

    /* copy publicValue in peerKey */
    rv = SECITEM_CopyItem(peerKey->arena, &peerKey->u.ec.publicValue, &ecPoint);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    return SECSuccess;
}

const namedGroupDef *
ssl_GetECGroupWithStrength(PRUint32 curvemsk, int requiredECCbits)
{
    int i;

    for (i = 0; i < ssl_named_group_count; i++) {
        if (ssl_named_groups[i].type != group_type_ec ||
            ssl_named_groups[i].bits < requiredECCbits) {
            continue;
        }
        if ((curvemsk & (1U << i))) {
            return &ssl_named_groups[i];
        }
    }
    PORT_SetError(SSL_ERROR_NO_CYPHER_OVERLAP);
    return NULL;
}

/* Find the "weakest link".  Get the strength of the signature and symmetric
 * keys and choose a curve based on the weakest of those two. */
const namedGroupDef *
ssl_GetECGroupForServerSocket(sslSocket *ss)
{
    const sslServerCert *cert = ss->sec.serverCert;
    int certKeySize;
    int requiredECCbits = ss->sec.secretKeyBits * 2;

    PORT_Assert(cert);
    if (!cert || !cert->serverKeyPair || !cert->serverKeyPair->pubKey) {
        PORT_SetError(SSL_ERROR_NO_CYPHER_OVERLAP);
        return NULL;
    }

    if (cert->certType.authType == ssl_auth_rsa_sign) {
        certKeySize = SECKEY_PublicKeyStrengthInBits(cert->serverKeyPair->pubKey);
        certKeySize =
            SSL_RSASTRENGTH_TO_ECSTRENGTH(certKeySize);
    } else if (cert->certType.authType == ssl_auth_ecdsa ||
               cert->certType.authType == ssl_auth_ecdh_rsa ||
               cert->certType.authType == ssl_auth_ecdh_ecdsa) {
        const namedGroupDef *groupDef = cert->certType.namedCurve;

        /* We won't select a certificate unless the named curve has been
         * negotiated (or supported_curves was absent), double check that. */
        PORT_Assert(groupDef->type == group_type_ec);
        PORT_Assert(ssl_NamedGroupEnabled(ss, groupDef));
        if (!ssl_NamedGroupEnabled(ss, groupDef)) {
            return NULL;
        }
        certKeySize = groupDef->bits;
    } else {
        PORT_Assert(0);
        return NULL;
    }
    if (requiredECCbits > certKeySize) {
        requiredECCbits = certKeySize;
    }

    return ssl_GetECGroupWithStrength(ss->namedGroups, requiredECCbits);
}

/* function to clear out the lists */
static SECStatus
ssl_ShutdownECDHECurves(void *appData, void *nssData)
{
    int i;

    for (i = 0; i < PR_ARRAY_SIZE(gECDHEKeyPairs); i++) {
        if (gECDHEKeyPairs[i].pair) {
            ssl_FreeEphemeralKeyPair(gECDHEKeyPairs[i].pair);
        }
    }
    memset(gECDHEKeyPairs, 0, sizeof(gECDHEKeyPairs));
    return SECSuccess;
}

static PRStatus
ssl_ECRegister(void)
{
    SECStatus rv;
    PORT_Assert(PR_ARRAY_SIZE(gECDHEKeyPairs) == ssl_named_group_count);
    rv = NSS_RegisterShutdown(ssl_ShutdownECDHECurves, gECDHEKeyPairs);
    if (rv != SECSuccess) {
        gECDHEInitError = PORT_GetError();
    }
    return (PRStatus)rv;
}

/* Create an ECDHE key pair for a given curve */
SECStatus
ssl_CreateECDHEphemeralKeyPair(const namedGroupDef *ecGroup,
                               sslEphemeralKeyPair **keyPair)
{
    SECKEYPrivateKey *privKey = NULL;
    SECKEYPublicKey *pubKey = NULL;
    SECKEYECParams ecParams = { siBuffer, NULL, 0 };
    sslEphemeralKeyPair *pair;

    if (ssl_NamedGroup2ECParams(NULL, ecGroup, &ecParams) != SECSuccess) {
        return SECFailure;
    }
    privKey = SECKEY_CreateECPrivateKey(&ecParams, &pubKey, NULL);
    SECITEM_FreeItem(&ecParams, PR_FALSE);

    if (!privKey || !pubKey ||
        !(pair = ssl_NewEphemeralKeyPair(ecGroup, privKey, pubKey))) {
        if (privKey) {
            SECKEY_DestroyPrivateKey(privKey);
        }
        if (pubKey) {
            SECKEY_DestroyPublicKey(pubKey);
        }
        ssl_MapLowLevelError(SEC_ERROR_KEYGEN_FAIL);
        return SECFailure;
    }

    *keyPair = pair;
    return SECSuccess;
}

/* CallOnce function, called once for each named curve. */
static PRStatus
ssl_CreateECDHEphemeralKeyPairOnce(void *arg)
{
    const namedGroupDef *groupDef = (const namedGroupDef *)arg;
    sslEphemeralKeyPair *keyPair = NULL;
    SECStatus rv;

    PORT_Assert(groupDef->type == group_type_ec);
    PORT_Assert(gECDHEKeyPairs[groupDef->index].pair == NULL);

    /* ok, no one has generated a global key for this curve yet, do so */
    rv = ssl_CreateECDHEphemeralKeyPair(groupDef, &keyPair);
    if (rv != SECSuccess) {
        gECDHEKeyPairs[groupDef->index].error = PORT_GetError();
        return PR_FAILURE;
    }

    gECDHEKeyPairs[groupDef->index].pair = keyPair;
    return PR_SUCCESS;
}

/*
 * Creates the ephemeral public and private ECDH keys used by
 * server in ECDHE_RSA and ECDHE_ECDSA handshakes.
 * For now, the elliptic curve is chosen to be the same
 * strength as the signing certificate (ECC or RSA).
 * We need an API to specify the curve. This won't be a real
 * issue until we further develop server-side support for ECC
 * cipher suites.
 */
static SECStatus
ssl_CreateECDHEphemeralKeys(sslSocket *ss, const namedGroupDef *ecGroup)
{
    sslEphemeralKeyPair *keyPair = NULL;

    /* if there's no global key for this curve, make one. */
    if (gECDHEKeyPairs[ecGroup->index].pair == NULL) {
        PRStatus status;

        status = PR_CallOnce(&gECDHEInitOnce, ssl_ECRegister);
        if (status != PR_SUCCESS) {
            PORT_SetError(gECDHEInitError);
            return SECFailure;
        }
        status = PR_CallOnceWithArg(&gECDHEKeyPairs[ecGroup->index].once,
                                    ssl_CreateECDHEphemeralKeyPairOnce,
                                    (void *)ecGroup);
        if (status != PR_SUCCESS) {
            PORT_SetError(gECDHEKeyPairs[ecGroup->index].error);
            return SECFailure;
        }
    }

    keyPair = ssl_CopyEphemeralKeyPair(gECDHEKeyPairs[ecGroup->index].pair);
    PORT_Assert(keyPair != NULL);
    if (!keyPair)
        return SECFailure;

    PORT_Assert(PR_CLIST_IS_EMPTY(&ss->ephemeralKeyPairs));
    PR_APPEND_LINK(&keyPair->link, &ss->ephemeralKeyPairs);
    return SECSuccess;
}

SECStatus
ssl3_HandleECDHServerKeyExchange(sslSocket *ss, SSL3Opaque *b, PRUint32 length)
{
    PLArenaPool *arena = NULL;
    SECKEYPublicKey *peerKey = NULL;
    PRBool isTLS;
    SECStatus rv;
    int errCode = SSL_ERROR_RX_MALFORMED_SERVER_KEY_EXCH;
    SSL3AlertDescription desc = illegal_parameter;
    SSL3Hashes hashes;
    SECItem signature = { siBuffer, NULL, 0 };
    SSLHashType hashAlg = ssl_hash_none;

    SECItem ec_params = { siBuffer, NULL, 0 };
    SECItem ec_point = { siBuffer, NULL, 0 };
    unsigned char paramBuf[3]; /* only for curve_type == named_curve */
    const namedGroupDef *ecGroup;

    isTLS = (PRBool)(ss->ssl3.prSpec->version > SSL_LIBRARY_VERSION_3_0);

    ec_params.len = sizeof paramBuf;
    ec_params.data = paramBuf;
    rv = ssl3_ConsumeHandshake(ss, ec_params.data, ec_params.len, &b, &length);
    if (rv != SECSuccess) {
        goto loser; /* malformed. */
    }

    /* Fail if the curve is not a named curve */
    if (ec_params.data[0] != ec_type_named) {
        errCode = SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE;
        desc = handshake_failure;
        goto alert_loser;
    }
    ecGroup = ssl_LookupNamedGroup(ec_params.data[1] << 8 | ec_params.data[2]);
    if (!ecGroup || ecGroup->type != group_type_ec) {
        errCode = SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE;
        desc = handshake_failure;
        goto alert_loser;
    }

    rv = ssl3_ConsumeHandshakeVariable(ss, &ec_point, 1, &b, &length);
    if (rv != SECSuccess) {
        goto loser; /* malformed. */
    }

    /* Fail if the provided point has length 0. */
    if (!ec_point.len) {
        /* desc and errCode are initialized already */
        goto alert_loser;
    }

    /* Fail if the ec point uses compressed representation. */
    if (ec_point.data[0] != EC_POINT_FORM_UNCOMPRESSED) {
        errCode = SEC_ERROR_UNSUPPORTED_EC_POINT_FORM;
        desc = handshake_failure;
        goto alert_loser;
    }

    if (ss->ssl3.prSpec->version >= SSL_LIBRARY_VERSION_TLS_1_2) {
        SSLSignatureAndHashAlg sigAndHash;
        rv = ssl3_ConsumeSignatureAndHashAlgorithm(ss, &b, &length,
                                                   &sigAndHash);
        if (rv != SECSuccess) {
            goto loser; /* malformed or unsupported. */
        }
        rv = ssl3_CheckSignatureAndHashAlgorithmConsistency(
            ss, &sigAndHash, ss->sec.peerCert);
        if (rv != SECSuccess) {
            goto loser;
        }
        hashAlg = sigAndHash.hashAlg;
    }

    rv = ssl3_ConsumeHandshakeVariable(ss, &signature, 2, &b, &length);
    if (rv != SECSuccess) {
        goto loser; /* malformed. */
    }

    if (length != 0) {
        if (isTLS)
            desc = decode_error;
        goto alert_loser; /* malformed. */
    }

    PRINT_BUF(60, (NULL, "Server EC params", ec_params.data,
                   ec_params.len));
    PRINT_BUF(60, (NULL, "Server EC point", ec_point.data, ec_point.len));

    /* failures after this point are not malformed handshakes. */
    /* TLS: send decrypt_error if signature failed. */
    desc = isTLS ? decrypt_error : handshake_failure;

    /*
     *  check to make sure the hash is signed by right guy
     */
    rv = ssl3_ComputeECDHKeyHash(hashAlg, ec_params, ec_point,
                                 &ss->ssl3.hs.client_random,
                                 &ss->ssl3.hs.server_random,
                                 &hashes, ss->opt.bypassPKCS11);

    if (rv != SECSuccess) {
        errCode =
            ssl_MapLowLevelError(SSL_ERROR_SERVER_KEY_EXCHANGE_FAILURE);
        goto alert_loser;
    }
    rv = ssl3_VerifySignedHashes(&hashes, ss->sec.peerCert, &signature,
                                 isTLS, ss->pkcs11PinArg);
    if (rv != SECSuccess) {
        errCode =
            ssl_MapLowLevelError(SSL_ERROR_SERVER_KEY_EXCHANGE_FAILURE);
        goto alert_loser;
    }

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
        errCode = SEC_ERROR_NO_MEMORY;
        goto loser;
    }

    peerKey = PORT_ArenaZNew(arena, SECKEYPublicKey);
    if (peerKey == NULL) {
        errCode = SEC_ERROR_NO_MEMORY;
        goto loser;
    }

    peerKey->arena = arena;
    peerKey->keyType = ecKey;

    /* set up EC parameters in peerKey */
    rv = ssl_NamedGroup2ECParams(arena, ecGroup,
                                 &peerKey->u.ec.DEREncodedParams);
    if (rv != SECSuccess) {
        /* we should never get here since we already
         * checked that we are dealing with a supported curve
         */
        errCode = SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE;
        goto alert_loser;
    }

    /* copy publicValue in peerKey */
    if (SECITEM_CopyItem(arena, &peerKey->u.ec.publicValue, &ec_point)) {
        errCode = SEC_ERROR_NO_MEMORY;
        goto loser;
    }
    peerKey->pkcs11Slot = NULL;
    peerKey->pkcs11ID = CK_INVALID_HANDLE;

    ss->sec.peerKey = peerKey;
    return SECSuccess;

alert_loser:
    (void)SSL3_SendAlert(ss, alert_fatal, desc);
loser:
    if (arena) {
        PORT_FreeArena(arena, PR_FALSE);
    }
    PORT_SetError(errCode);
    return SECFailure;
}

SECStatus
ssl3_SendECDHServerKeyExchange(
    sslSocket *ss,
    const SSLSignatureAndHashAlg *sigAndHash)
{
    SECStatus rv = SECFailure;
    int length;
    PRBool isTLS, isTLS12;
    SECItem signed_hash = { siBuffer, NULL, 0 };
    SSL3Hashes hashes;

    SECItem ec_params = { siBuffer, NULL, 0 };
    unsigned char paramBuf[3];
    const namedGroupDef *ecGroup;
    sslEphemeralKeyPair *keyPair;
    SECKEYPublicKey *pubKey;

    /* Generate ephemeral ECDH key pair and send the public key */
    ecGroup = ssl_GetECGroupForServerSocket(ss);
    if (!ecGroup) {
        goto loser;
    }

    PORT_Assert(PR_CLIST_IS_EMPTY(&ss->ephemeralKeyPairs));
    if (ss->opt.reuseServerECDHEKey) {
        rv = ssl_CreateECDHEphemeralKeys(ss, ecGroup);
        if (rv != SECSuccess) {
            goto loser;
        }
        keyPair = (sslEphemeralKeyPair *)PR_NEXT_LINK(&ss->ephemeralKeyPairs);
    } else {
        rv = ssl_CreateECDHEphemeralKeyPair(ecGroup, &keyPair);
        if (rv != SECSuccess) {
            goto loser;
        }
        PR_APPEND_LINK(&keyPair->link, &ss->ephemeralKeyPairs);
    }
    if (rv != SECSuccess) {
        goto loser;
    }

    PORT_Assert(keyPair);
    if (!keyPair) {
        PORT_SetError(SSL_ERROR_SERVER_KEY_EXCHANGE_FAILURE);
        return SECFailure;
    }

    ec_params.len = sizeof(paramBuf);
    ec_params.data = paramBuf;
    PORT_Assert(keyPair->group);
    PORT_Assert(keyPair->group->type == group_type_ec);
    ec_params.data[0] = ec_type_named;
    ec_params.data[1] = keyPair->group->name >> 8;
    ec_params.data[2] = keyPair->group->name & 0xff;

    pubKey = keyPair->keys->pubKey;
    rv = ssl3_ComputeECDHKeyHash(sigAndHash->hashAlg,
                                 ec_params,
                                 pubKey->u.ec.publicValue,
                                 &ss->ssl3.hs.client_random,
                                 &ss->ssl3.hs.server_random,
                                 &hashes, ss->opt.bypassPKCS11);
    if (rv != SECSuccess) {
        ssl_MapLowLevelError(SSL_ERROR_SERVER_KEY_EXCHANGE_FAILURE);
        goto loser;
    }

    isTLS = (PRBool)(ss->ssl3.pwSpec->version > SSL_LIBRARY_VERSION_3_0);
    isTLS12 = (PRBool)(ss->ssl3.pwSpec->version >= SSL_LIBRARY_VERSION_TLS_1_2);

    rv = ssl3_SignHashes(&hashes, ss->sec.serverCert->serverKeyPair->privKey,
                         &signed_hash, isTLS);
    if (rv != SECSuccess) {
        goto loser; /* ssl3_SignHashes has set err. */
    }
    if (signed_hash.data == NULL) {
        /* how can this happen and rv == SECSuccess ?? */
        PORT_SetError(SSL_ERROR_SERVER_KEY_EXCHANGE_FAILURE);
        goto loser;
    }

    length = ec_params.len +
             1 + pubKey->u.ec.publicValue.len +
             (isTLS12 ? 2 : 0) + 2 + signed_hash.len;

    rv = ssl3_AppendHandshakeHeader(ss, server_key_exchange, length);
    if (rv != SECSuccess) {
        goto loser; /* err set by AppendHandshake. */
    }

    rv = ssl3_AppendHandshake(ss, ec_params.data, ec_params.len);
    if (rv != SECSuccess) {
        goto loser; /* err set by AppendHandshake. */
    }

    rv = ssl3_AppendHandshakeVariable(ss, pubKey->u.ec.publicValue.data,
                                      pubKey->u.ec.publicValue.len, 1);
    if (rv != SECSuccess) {
        goto loser; /* err set by AppendHandshake. */
    }

    if (isTLS12) {
        rv = ssl3_AppendSignatureAndHashAlgorithm(ss, sigAndHash);
        if (rv != SECSuccess) {
            goto loser; /* err set by AppendHandshake. */
        }
    }

    rv = ssl3_AppendHandshakeVariable(ss, signed_hash.data,
                                      signed_hash.len, 2);
    if (rv != SECSuccess) {
        goto loser; /* err set by AppendHandshake. */
    }

    PORT_Free(signed_hash.data);
    return SECSuccess;

loser:
    if (signed_hash.data != NULL)
        PORT_Free(signed_hash.data);
    return SECFailure;
}

/* List of all ECC cipher suites */
static const ssl3CipherSuite ssl_all_ec_suites[] = {
    TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
    TLS_ECDHE_ECDSA_WITH_NULL_SHA,
    TLS_ECDHE_ECDSA_WITH_RC4_128_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
    TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
    TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
    TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
    TLS_ECDHE_RSA_WITH_NULL_SHA,
    TLS_ECDHE_RSA_WITH_RC4_128_SHA,
    TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA,
    TLS_ECDH_ECDSA_WITH_NULL_SHA,
    TLS_ECDH_ECDSA_WITH_RC4_128_SHA,
    TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDH_RSA_WITH_AES_128_CBC_SHA,
    TLS_ECDH_RSA_WITH_AES_256_CBC_SHA,
    TLS_ECDH_RSA_WITH_NULL_SHA,
    TLS_ECDH_RSA_WITH_RC4_128_SHA,
    0 /* end of list marker */
};

static const ssl3CipherSuite ssl_dhe_suites[] = {
    TLS_DHE_RSA_WITH_AES_128_GCM_SHA256,
    TLS_DHE_RSA_WITH_AES_256_GCM_SHA384,
    TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
    TLS_DHE_DSS_WITH_AES_128_GCM_SHA256,
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA,
    TLS_DHE_DSS_WITH_AES_128_CBC_SHA,
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA256,
    TLS_DHE_DSS_WITH_AES_128_CBC_SHA256,
    TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA,
    TLS_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA,
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA,
    TLS_DHE_DSS_WITH_AES_256_CBC_SHA,
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA256,
    TLS_DHE_DSS_WITH_AES_256_CBC_SHA256,
    TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA,
    TLS_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA,
    TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA,
    TLS_DHE_DSS_WITH_RC4_128_SHA,
    TLS_DHE_RSA_WITH_DES_CBC_SHA,
    TLS_DHE_DSS_WITH_DES_CBC_SHA,
    0
};

/* Order(N^2).  Yuk. */
static PRBool
ssl_IsSuiteEnabled(sslSocket *ss, const ssl3CipherSuite *list)
{
    const ssl3CipherSuite *suite;

    for (suite = list; *suite; ++suite) {
        PRBool enabled = PR_FALSE;
        SECStatus rv = ssl3_CipherPrefGet(ss, *suite, &enabled);

        PORT_Assert(rv == SECSuccess); /* else is coding error */
        if (rv == SECSuccess && enabled)
            return PR_TRUE;
    }
    return PR_FALSE;
}

/* Ask: is ANY ECC cipher suite enabled on this socket? */
static PRBool
ssl_IsECCEnabled(sslSocket *ss)
{
    PK11SlotInfo *slot;

    /* make sure we can do ECC */
    slot = PK11_GetBestSlot(CKM_ECDH1_DERIVE, ss->pkcs11PinArg);
    if (!slot) {
        return PR_FALSE;
    }
    PK11_FreeSlot(slot);

    /* make sure an ECC cipher is enabled */
    return ssl_IsSuiteEnabled(ss, ssl_all_ec_suites);
}

/* This function already presumes we can do ECC, ssl_IsECCEnabled must be
 * called before this function. It looks to see if we have a token which
 * is capable of doing smaller than SuiteB curves. If the token can, we
 * presume the token can do the whole SSL suite of curves. If it can't we
 * presume the token that allowed ECC to be enabled can only do suite B
 * curves. */
PRBool
ssl_SuiteBOnly(sslSocket *ss)
{
    /* See if we can support small curves (like 163). If not, assume we can
     * only support Suite-B curves (P-256, P-384, P-521). */
    PK11SlotInfo *slot =
        PK11_GetBestSlotWithAttributes(CKM_ECDH1_DERIVE, 0, 163,
                                       ss ? ss->pkcs11PinArg : NULL);

    if (!slot) {
        /* nope, presume we can only do suite B */
        return PR_TRUE;
    }
    /* we can, presume we can do all curves */
    PK11_FreeSlot(slot);
    return PR_FALSE;
}

/* Send our Supported Groups extension. */
PRInt32
ssl_SendSupportedGroupsXtn(sslSocket *ss, PRBool append, PRUint32 maxBytes)
{
    PRInt32 extension_length;
    unsigned char enabledGroups[64];
    unsigned int enabledGroupsLen = 0;
    unsigned int i;
    PRBool ec;
    PRBool ff = PR_FALSE;

    if (!ss)
        return 0;

    ec = ssl_IsECCEnabled(ss);
    /* We only send FF supported groups if we require DH named groups or if TLS
     * 1.3 is a possibility. */
    if (ss->opt.requireDHENamedGroups ||
        ss->vrange.max >= SSL_LIBRARY_VERSION_TLS_1_3) {
        ff = ssl_IsSuiteEnabled(ss, ssl_dhe_suites);
    }
    if (!ec && !ff) {
        return 0;
    }

    PORT_Assert(sizeof(enabledGroups) > ssl_named_group_count * 2);
    for (i = 0; i < ssl_named_group_count; ++i) {
        if (ssl_named_groups[i].type == group_type_ec && !ec) {
            continue;
        }
        if (ssl_named_groups[i].type == group_type_ff && !ff) {
            continue;
        }
        if (!ssl_NamedGroupEnabled(ss, &ssl_named_groups[i])) {
            continue;
        }

        if (append) {
            enabledGroups[enabledGroupsLen++] = ssl_named_groups[i].name >> 8;
            enabledGroups[enabledGroupsLen++] = ssl_named_groups[i].name & 0xff;
        } else {
            enabledGroupsLen += 2;
        }
    }

    extension_length =
        2 /* extension type */ +
        2 /* extension length */ +
        2 /* enabled groups length */ +
        enabledGroupsLen;

    if (maxBytes < (PRUint32)extension_length) {
        return 0;
    }

    if (append) {
        SECStatus rv;
        rv = ssl3_AppendHandshakeNumber(ss, ssl_supported_groups_xtn, 2);
        if (rv != SECSuccess)
            return -1;
        rv = ssl3_AppendHandshakeNumber(ss, extension_length - 4, 2);
        if (rv != SECSuccess)
            return -1;
        rv = ssl3_AppendHandshakeVariable(ss, enabledGroups,
                                          enabledGroupsLen, 2);
        if (rv != SECSuccess)
            return -1;
        if (!ss->sec.isServer) {
            TLSExtensionData *xtnData = &ss->xtnData;
            xtnData->advertised[xtnData->numAdvertised++] =
                ssl_supported_groups_xtn;
        }
    }
    return extension_length;
}

/* Send our "canned" (precompiled) Supported Point Formats extension,
 * which says that we only support uncompressed points.
 */
PRInt32
ssl3_SendSupportedPointFormatsXtn(
    sslSocket *ss,
    PRBool append,
    PRUint32 maxBytes)
{
    static const PRUint8 ecPtFmt[6] = {
        0, 11, /* Extension type */
        0, 2,  /* octets that follow */
        1,     /* octets that follow */
        0      /* uncompressed type only */
    };

    /* No point in doing this unless we have a socket that supports ECC.
     * Similarly, no point if we are going to do TLS 1.3 only or we have already
     * picked TLS 1.3 (server) given that it doesn't use point formats. */
    if (!ss || !ssl_IsECCEnabled(ss) ||
        ss->vrange.min >= SSL_LIBRARY_VERSION_TLS_1_3 ||
        (ss->sec.isServer && ss->version >= SSL_LIBRARY_VERSION_TLS_1_3))
        return 0;
    if (append && maxBytes >= (sizeof ecPtFmt)) {
        SECStatus rv = ssl3_AppendHandshake(ss, ecPtFmt, (sizeof ecPtFmt));
        if (rv != SECSuccess)
            return -1;
        if (!ss->sec.isServer) {
            TLSExtensionData *xtnData = &ss->xtnData;
            xtnData->advertised[xtnData->numAdvertised++] =
                ssl_ec_point_formats_xtn;
        }
    }
    return sizeof(ecPtFmt);
}

/* Just make sure that the remote client supports uncompressed points,
 * Since that is all we support.  Disable ECC cipher suites if it doesn't.
 */
SECStatus
ssl3_HandleSupportedPointFormatsXtn(sslSocket *ss, PRUint16 ex_type,
                                    SECItem *data)
{
    int i;

    if (data->len < 2 || data->len > 255 || !data->data ||
        data->len != (unsigned int)data->data[0] + 1) {
        return ssl3_DecodeError(ss);
    }
    for (i = data->len; --i > 0;) {
        if (data->data[i] == 0) {
            /* indicate that we should send a reply */
            SECStatus rv;
            rv = ssl3_RegisterServerHelloExtensionSender(ss, ex_type,
                                                         &ssl3_SendSupportedPointFormatsXtn);
            return rv;
        }
    }

    /* Poor client doesn't support uncompressed points. */
    PORT_SetError(SSL_ERROR_RX_MALFORMED_HANDSHAKE);
    return SECFailure;
}

static SECStatus
ssl_UpdateSupportedGroups(sslSocket *ss, SECItem *data)
{
    PRInt32 list_len;
    PRUint32 peerGroups = 0;

    if (!data->data || data->len < 4) {
        (void)ssl3_DecodeError(ss);
        return SECFailure;
    }

    /* get the length of elliptic_curve_list */
    list_len = ssl3_ConsumeHandshakeNumber(ss, 2, &data->data, &data->len);
    if (list_len < 0 || data->len != list_len || (data->len % 2) != 0) {
        (void)ssl3_DecodeError(ss);
        return SECFailure;
    }

    /* build bit vector of peer's supported groups */
    while (data->len) {
        const namedGroupDef *group;
        PRInt32 curve_name =
            ssl3_ConsumeHandshakeNumber(ss, 2, &data->data, &data->len);
        if (curve_name < 0) {
            return SECFailure; /* fatal alert already sent */
        }
        group = ssl_LookupNamedGroup(curve_name);
        if (group) {
            peerGroups |= (1U << group->index);
        }

        /* "Codepoints in the NamedCurve registry with a high byte of 0x01 (that
         * is, between 256 and 511 inclusive) are set aside for FFDHE groups,"
         * -- https://tools.ietf.org/html/draft-ietf-tls-negotiated-ff-dhe-10
         */
        if ((curve_name & 0xff00) == 0x0100) {
            ss->ssl3.hs.peerSupportsFfdheGroups = PR_TRUE;
        }
    }
    /* Note: if ss->opt.requireDHENamedGroups is set, we disable DHE cipher
     * suites, but we do that in ssl3_config_match(). */
    if (!ss->opt.requireDHENamedGroups && !ss->ssl3.hs.peerSupportsFfdheGroups) {
        /* If we don't require that DHE use named groups, and no FFDHE was
         * included, we pretend that they support all the FFDHE groups we do. */
        unsigned int i;
        for (i = 0; i < ssl_named_group_count; ++i) {
            if (ssl_named_groups[i].type == group_type_ff) {
                peerGroups |= (1U << ssl_named_groups[i].index);
            }
        }
    }

    /* What curves do we support in common? */
    ss->namedGroups &= peerGroups;
    return SECSuccess;
}

/* Ensure that the curve in our server cert is one of the ones supported
 * by the remote client, and disable all ECC cipher suites if not.
 */
SECStatus
ssl_HandleSupportedGroupsXtn(sslSocket *ss, PRUint16 ex_type, SECItem *data)
{
    SECStatus rv;

    rv = ssl_UpdateSupportedGroups(ss, data);
    if (rv != SECSuccess)
        return SECFailure;

    /* TLS 1.3 permits the server to send this extension so make it so. */
    if (ss->sec.isServer && ss->version >= SSL_LIBRARY_VERSION_TLS_1_3) {
        rv = ssl3_RegisterServerHelloExtensionSender(ss, ex_type,
                                                     &ssl_SendSupportedGroupsXtn);
        if (rv != SECSuccess) {
            return SECFailure; /* error already set. */
        }
    }

    return SECSuccess;
}
