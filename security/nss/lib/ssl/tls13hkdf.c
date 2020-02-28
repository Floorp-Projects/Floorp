/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * TLS 1.3 Protocol
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "keyhi.h"
#include "pk11func.h"
#include "secitem.h"
#include "ssl.h"
#include "sslt.h"
#include "sslerr.h"
#include "sslimpl.h"

/* This table contains the mapping between TLS hash identifiers and the
 * PKCS#11 identifiers */
static const struct {
    SSLHashType hash;
    CK_MECHANISM_TYPE pkcs11Mech;
    unsigned int hashSize;
} kTlsHkdfInfo[] = {
    { ssl_hash_none, 0, 0 },
    { ssl_hash_md5, 0, 0 },
    { ssl_hash_sha1, 0, 0 },
    { ssl_hash_sha224, 0 },
    { ssl_hash_sha256, CKM_NSS_HKDF_SHA256, 32 },
    { ssl_hash_sha384, CKM_NSS_HKDF_SHA384, 48 },
    { ssl_hash_sha512, CKM_NSS_HKDF_SHA512, 64 }
};

SECStatus
tls13_HkdfExtract(PK11SymKey *ikm1, PK11SymKey *ikm2in, SSLHashType baseHash,
                  PK11SymKey **prkp)
{
    CK_NSS_HKDFParams params;
    SECItem paramsi;
    SECStatus rv;
    SECItem *salt;
    PK11SymKey *prk;
    static const PRUint8 zeroKeyBuf[HASH_LENGTH_MAX];
    PK11SymKey *zeroKey = NULL;
    PK11SlotInfo *slot = NULL;
    PK11SymKey *ikm2;

    params.bExtract = CK_TRUE;
    params.bExpand = CK_FALSE;
    params.pInfo = NULL;
    params.ulInfoLen = 0UL;

    if (ikm1) {
        /* TODO(ekr@rtfm.com): This violates the PKCS#11 key boundary
         * but is imposed on us by the present HKDF interface. */
        rv = PK11_ExtractKeyValue(ikm1);
        if (rv != SECSuccess)
            return rv;

        salt = PK11_GetKeyData(ikm1);
        if (!salt)
            return SECFailure;

        params.pSalt = salt->data;
        params.ulSaltLen = salt->len;
        PORT_Assert(salt->len > 0);
    } else {
        /* Per documentation for CKM_NSS_HKDF_*:
         *
         *  If the optional salt is given, it is used; otherwise, the salt is
         *  set to a sequence of zeros equal in length to the HMAC output.
         */
        params.pSalt = NULL;
        params.ulSaltLen = 0UL;
    }
    paramsi.data = (unsigned char *)&params;
    paramsi.len = sizeof(params);

    PORT_Assert(kTlsHkdfInfo[baseHash].pkcs11Mech);
    PORT_Assert(kTlsHkdfInfo[baseHash].hashSize);
    PORT_Assert(kTlsHkdfInfo[baseHash].hash == baseHash);

    /* A zero ikm2 is a key of hash-length 0s. */
    if (!ikm2in) {
        SECItem zeroItem = {
            siBuffer,
            (unsigned char *)zeroKeyBuf,
            kTlsHkdfInfo[baseHash].hashSize
        };
        slot = PK11_GetInternalSlot();
        if (!slot) {
            return SECFailure;
        }
        zeroKey = PK11_ImportSymKey(slot,
                                    kTlsHkdfInfo[baseHash].pkcs11Mech,
                                    PK11_OriginUnwrap,
                                    CKA_DERIVE, &zeroItem, NULL);
        if (!zeroKey)
            return SECFailure;
        ikm2 = zeroKey;
    } else {
        ikm2 = ikm2in;
    }
    PORT_Assert(ikm2);

    PRINT_BUF(50, (NULL, "HKDF Extract: IKM1/Salt", params.pSalt, params.ulSaltLen));
    PRINT_KEY(50, (NULL, "HKDF Extract: IKM2", ikm2));

    prk = PK11_Derive(ikm2, kTlsHkdfInfo[baseHash].pkcs11Mech,
                      &paramsi, kTlsHkdfInfo[baseHash].pkcs11Mech,
                      CKA_DERIVE, kTlsHkdfInfo[baseHash].hashSize);
    if (zeroKey)
        PK11_FreeSymKey(zeroKey);
    if (slot)
        PK11_FreeSlot(slot);
    if (!prk)
        return SECFailure;

    PRINT_KEY(50, (NULL, "HKDF Extract", prk));
    *prkp = prk;

    return SECSuccess;
}

SECStatus
tls13_HkdfExpandLabel(PK11SymKey *prk, SSLHashType baseHash,
                      const PRUint8 *handshakeHash, unsigned int handshakeHashLen,
                      const char *label, unsigned int labelLen,
                      CK_MECHANISM_TYPE algorithm, unsigned int keySize,
                      SSLProtocolVariant variant, PK11SymKey **keyp)
{
    CK_NSS_HKDFParams params;
    SECItem paramsi = { siBuffer, NULL, 0 };
    /* Size of info array needs to be big enough to hold the maximum Prefix,
     * Label, plus HandshakeHash. If it's ever to small, the code will abort.
     */
    PRUint8 info[256];
    sslBuffer infoBuf = SSL_BUFFER(info);
    PK11SymKey *derived;
    SECStatus rv;
    const char *kLabelPrefixTls = "tls13 ";
    const char *kLabelPrefixDtls = "dtls13";
    const unsigned int kLabelPrefixLen =
        (variant == ssl_variant_stream) ? strlen(kLabelPrefixTls) : strlen(kLabelPrefixDtls);
    const char *kLabelPrefix =
        (variant == ssl_variant_stream) ? kLabelPrefixTls : kLabelPrefixDtls;

    PORT_Assert(prk);
    PORT_Assert(keyp);
    if ((handshakeHashLen > 255) ||
        (handshakeHash == NULL && handshakeHashLen > 0) ||
        (labelLen + kLabelPrefixLen > 255)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /*
     *  [draft-ietf-tls-tls13-11] Section 7.1:
     *
     *  HKDF-Expand-Label(Secret, Label, HashValue, Length) =
     *       HKDF-Expand(Secret, HkdfLabel, Length)
     *
     *  Where HkdfLabel is specified as:
     *
     *  struct HkdfLabel {
     *    uint16 length;
     *    opaque label<9..255>;
     *    opaque hash_value<0..255>;
     *  };
     *
     *  Where:
     *  - HkdfLabel.length is Length
     *  - HkdfLabel.hash_value is HashValue.
     *  - HkdfLabel.label is "TLS 1.3, " + Label
     *
     */
    rv = sslBuffer_AppendNumber(&infoBuf, keySize, 2);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    rv = sslBuffer_AppendNumber(&infoBuf, labelLen + kLabelPrefixLen, 1);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    rv = sslBuffer_Append(&infoBuf, kLabelPrefix, kLabelPrefixLen);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    rv = sslBuffer_Append(&infoBuf, label, labelLen);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    rv = sslBuffer_AppendVariable(&infoBuf, handshakeHash, handshakeHashLen, 1);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    params.bExtract = CK_FALSE;
    params.bExpand = CK_TRUE;
    params.pInfo = SSL_BUFFER_BASE(&infoBuf);
    params.ulInfoLen = SSL_BUFFER_LEN(&infoBuf);
    paramsi.data = (unsigned char *)&params;
    paramsi.len = sizeof(params);

    derived = PK11_DeriveWithFlags(prk, kTlsHkdfInfo[baseHash].pkcs11Mech,
                                   &paramsi, algorithm,
                                   CKA_DERIVE, keySize,
                                   CKF_SIGN | CKF_VERIFY);
    if (!derived)
        return SECFailure;

    *keyp = derived;

#ifdef TRACE
    if (ssl_trace >= 50) {
        /* Make sure the label is null terminated. */
        char labelStr[100];
        PORT_Memcpy(labelStr, label, labelLen);
        labelStr[labelLen] = 0;
        SSL_TRC(50, ("HKDF Expand: label='tls13 %s',requested length=%d",
                     labelStr, keySize));
    }
    PRINT_KEY(50, (NULL, "PRK", prk));
    PRINT_BUF(50, (NULL, "Hash", handshakeHash, handshakeHashLen));
    PRINT_BUF(50, (NULL, "Info", SSL_BUFFER_BASE(&infoBuf),
                   SSL_BUFFER_LEN(&infoBuf)));
    PRINT_KEY(50, (NULL, "Derived key", derived));
#endif

    return SECSuccess;
}

SECStatus
tls13_HkdfExpandLabelRaw(PK11SymKey *prk, SSLHashType baseHash,
                         const PRUint8 *handshakeHash, unsigned int handshakeHashLen,
                         const char *label, unsigned int labelLen,
                         SSLProtocolVariant variant, unsigned char *output,
                         unsigned int outputLen)
{
    PK11SymKey *derived = NULL;
    SECItem *rawkey;
    SECStatus rv;

    rv = tls13_HkdfExpandLabel(prk, baseHash, handshakeHash, handshakeHashLen,
                               label, labelLen,
                               kTlsHkdfInfo[baseHash].pkcs11Mech, outputLen,
                               variant, &derived);
    if (rv != SECSuccess || !derived) {
        goto abort;
    }

    rv = PK11_ExtractKeyValue(derived);
    if (rv != SECSuccess) {
        goto abort;
    }

    rawkey = PK11_GetKeyData(derived);
    if (!rawkey) {
        goto abort;
    }

    PORT_Assert(rawkey->len == outputLen);
    memcpy(output, rawkey->data, outputLen);
    PK11_FreeSymKey(derived);

    return SECSuccess;

abort:
    if (derived) {
        PK11_FreeSymKey(derived);
    }
    PORT_SetError(SSL_ERROR_SYM_KEY_CONTEXT_FAILURE);
    return SECFailure;
}
