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

// TODO(ekr@rtfm.com): Export this separately.
unsigned char *tls13_EncodeUintX(PRUint32 value, unsigned int bytes, unsigned char *to);

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
tls13_HkdfExtract(PK11SymKey *ikm1, PK11SymKey *ikm2, SSLHashType baseHash,
                  PK11SymKey **prkp)
{
    CK_NSS_HKDFParams params;
    SECItem paramsi;
    SECStatus rv;
    SECItem *salt;
    PK11SymKey *prk;

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
    prk = PK11_Derive(ikm2, kTlsHkdfInfo[baseHash].pkcs11Mech,
                      &paramsi, kTlsHkdfInfo[baseHash].pkcs11Mech,
                      CKA_DERIVE, kTlsHkdfInfo[baseHash].hashSize);
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
                      PK11SymKey **keyp)
{
    CK_NSS_HKDFParams params;
    SECItem paramsi = { siBuffer, NULL, 0 };
    /* Size of info array needs to be big enough to hold the maximum Prefix,
     * Label, plus HandshakeHash. If it's ever to small, the code will abort.
     */
    PRUint8 info[110];
    PRUint8 *ptr = info;
    unsigned int infoLen;
    PK11SymKey *derived;
    const char *kLabelPrefix = "TLS 1.3, ";
    const unsigned int kLabelPrefixLen = strlen(kLabelPrefix);

    if (handshakeHash) {
        PORT_Assert(handshakeHashLen == kTlsHkdfInfo[baseHash].hashSize);
    } else {
        PORT_Assert(!handshakeHashLen);
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
    infoLen = 2 + 1 + kLabelPrefixLen + labelLen + 1 + handshakeHashLen;
    if (infoLen > sizeof(info)) {
        PORT_Assert(0);
        goto abort;
    }

    ptr = tls13_EncodeUintX(keySize, 2, ptr);
    ptr = tls13_EncodeUintX(labelLen + kLabelPrefixLen, 1, ptr);
    PORT_Memcpy(ptr, kLabelPrefix, kLabelPrefixLen);
    ptr += kLabelPrefixLen;
    PORT_Memcpy(ptr, label, labelLen);
    ptr += labelLen;
    ptr = tls13_EncodeUintX(handshakeHashLen, 1, ptr);
    if (handshakeHash) {
        PORT_Memcpy(ptr, handshakeHash, handshakeHashLen);
        ptr += handshakeHashLen;
    }
    PORT_Assert((ptr - info) == infoLen);

    params.bExtract = CK_FALSE;
    params.bExpand = CK_TRUE;
    params.pInfo = info;
    params.ulInfoLen = infoLen;
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
    if (ssl_trace >= 10) {
        /* Make sure the label is null terminated. */
        char labelStr[100];
        PORT_Memcpy(labelStr, label, labelLen);
        labelStr[labelLen] = 0;
        SSL_TRC(50, ("HKDF Expand: label=[TLS 1.3, ] + '%s',requested length=%d",
                     labelStr, keySize));
    }
    PRINT_KEY(50, (NULL, "PRK", prk));
    PRINT_BUF(50, (NULL, "Hash", handshakeHash, handshakeHashLen));
    PRINT_BUF(50, (NULL, "Info", info, infoLen));
    PRINT_KEY(50, (NULL, "Derived key", derived));
#endif

    return SECSuccess;

abort:
    PORT_SetError(SSL_ERROR_SYM_KEY_CONTEXT_FAILURE);
    return SECFailure;
}

SECStatus
tls13_HkdfExpandLabelRaw(PK11SymKey *prk, SSLHashType baseHash,
                         const PRUint8 *handshakeHash, unsigned int handshakeHashLen,
                         const char *label, unsigned int labelLen,
                         unsigned char *output, unsigned int outputLen)
{
    PK11SymKey *derived = NULL;
    SECItem *rawkey;
    SECStatus rv;

    rv = tls13_HkdfExpandLabel(prk, baseHash, handshakeHash, handshakeHashLen,
                               label, labelLen,
                               kTlsHkdfInfo[baseHash].pkcs11Mech, outputLen,
                               &derived);
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
