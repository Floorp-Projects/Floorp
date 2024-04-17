/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * CMS public key crypto
 */

#include "cmslocal.h"

#include "cert.h"
#include "keyhi.h"
#include "secasn1.h"
#include "secitem.h"
#include "secoid.h"
#include "pk11func.h"
#include "secerr.h"
#include "secder.h"
#include "prerr.h"

/* ====== RSA ======================================================================= */

/*
 * NSS_CMSUtil_EncryptSymKey_RSA - wrap a symmetric key with RSA
 *
 * this function takes a symmetric key and encrypts it using an RSA public key
 * according to PKCS#1 and RFC2633 (S/MIME)
 */
SECStatus
NSS_CMSUtil_EncryptSymKey_RSA(PLArenaPool *poolp, CERTCertificate *cert,
                              PK11SymKey *bulkkey,
                              SECItem *encKey)
{
    SECStatus rv;
    SECKEYPublicKey *publickey;

    publickey = CERT_ExtractPublicKey(cert);
    if (publickey == NULL)
        return SECFailure;

    rv = NSS_CMSUtil_EncryptSymKey_RSAPubKey(poolp, publickey, bulkkey, encKey);
    SECKEY_DestroyPublicKey(publickey);
    return rv;
}

SECStatus
NSS_CMSUtil_EncryptSymKey_RSAPubKey(PLArenaPool *poolp,
                                    SECKEYPublicKey *publickey,
                                    PK11SymKey *bulkkey, SECItem *encKey)
{
    SECStatus rv;
    int data_len;
    KeyType keyType;
    void *mark = NULL;

    mark = PORT_ArenaMark(poolp);
    if (!mark)
        goto loser;

    /* sanity check */
    keyType = SECKEY_GetPublicKeyType(publickey);
    PORT_Assert(keyType == rsaKey);
    if (keyType != rsaKey) {
        goto loser;
    }
    /* allocate memory for the encrypted key */
    data_len = SECKEY_PublicKeyStrength(publickey); /* block size (assumed to be > keylen) */
    encKey->data = (unsigned char *)PORT_ArenaAlloc(poolp, data_len);
    encKey->len = data_len;
    if (encKey->data == NULL)
        goto loser;

    /* encrypt the key now */
    rv = PK11_PubWrapSymKey(PK11_AlgtagToMechanism(SEC_OID_PKCS1_RSA_ENCRYPTION),
                            publickey, bulkkey, encKey);

    if (rv != SECSuccess)
        goto loser;

    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;

loser:
    if (mark) {
        PORT_ArenaRelease(poolp, mark);
    }
    return SECFailure;
}

/*
 * NSS_CMSUtil_DecryptSymKey_RSA - unwrap a RSA-wrapped symmetric key
 *
 * this function takes an RSA-wrapped symmetric key and unwraps it, returning a symmetric
 * key handle. Please note that the actual unwrapped key data may not be allowed to leave
 * a hardware token...
 */
PK11SymKey *
NSS_CMSUtil_DecryptSymKey_RSA(SECKEYPrivateKey *privkey, SECItem *encKey, SECOidTag bulkalgtag)
{
    /* that's easy */
    CK_MECHANISM_TYPE target;
    PORT_Assert(bulkalgtag != SEC_OID_UNKNOWN);
    target = PK11_AlgtagToMechanism(bulkalgtag);
    if (bulkalgtag == SEC_OID_UNKNOWN || target == CKM_INVALID_MECHANISM) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        return NULL;
    }
    return PK11_PubUnwrapSymKey(privkey, encKey, target, CKA_DECRYPT, 0);
}

/* ====== ESECDH (Ephemeral-Static Elliptic Curve Diffie-Hellman) =========== */

typedef struct ECC_CMS_SharedInfoStr ECC_CMS_SharedInfo;
struct ECC_CMS_SharedInfoStr {
    SECAlgorithmID *keyInfo; /* key-encryption algorithm */
    SECItem entityUInfo;     /* ukm */
    SECItem suppPubInfo;     /* length of KEK in bits as 32-bit number */
};

SEC_ASN1_MKSUB(SECOID_AlgorithmIDTemplate)
SEC_ASN1_MKSUB(SEC_OctetStringTemplate)

static const SEC_ASN1Template ECC_CMS_SharedInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(ECC_CMS_SharedInfo) },
    { SEC_ASN1_XTRN | SEC_ASN1_POINTER,
      offsetof(ECC_CMS_SharedInfo, keyInfo),
      SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED |
          SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 0,
      offsetof(ECC_CMS_SharedInfo, entityUInfo),
      SEC_ASN1_SUB(SEC_OctetStringTemplate) },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED |
          SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 2,
      offsetof(ECC_CMS_SharedInfo, suppPubInfo),
      SEC_ASN1_SUB(SEC_OctetStringTemplate) },
    { 0 }
};

/* Inputs:
 *       keyInfo: key-encryption algorithm
 *       ukm: ukm field of KeyAgreeRecipientInfo
 *       KEKsize: length of KEK in bytes
 *
 * Output: DER encoded ECC-CMS-SharedInfo
 */
static SECItem *
Create_ECC_CMS_SharedInfo(PLArenaPool *poolp,
                          SECAlgorithmID *keyInfo, SECItem *ukm, unsigned int KEKsize)
{
    ECC_CMS_SharedInfo SI;
    unsigned char suppPubInfo[4] = { 0 };

    SI.keyInfo = keyInfo;
    SI.entityUInfo.type = ukm->type;
    SI.entityUInfo.data = ukm->data;
    SI.entityUInfo.len = ukm->len;

    SI.suppPubInfo.type = siBuffer;
    SI.suppPubInfo.data = suppPubInfo;
    SI.suppPubInfo.len = sizeof(suppPubInfo);

    if (KEKsize > 0x1FFFFFFF) { // ensure KEKsize * 8 fits in 4 bytes.
        return NULL;
    }
    KEKsize *= 8;
    suppPubInfo[0] = (KEKsize & 0xFF000000) >> 24;
    suppPubInfo[1] = (KEKsize & 0xFF0000) >> 16;
    suppPubInfo[2] = (KEKsize & 0xFF00) >> 8;
    suppPubInfo[3] = KEKsize & 0xFF;

    return SEC_ASN1EncodeItem(poolp, NULL, &SI, ECC_CMS_SharedInfoTemplate);
}

/* this will generate a key pair, compute the shared secret, */
/* derive a key and ukm for the keyEncAlg out of it, encrypt the bulk key with */
/* the keyEncAlg, set encKey, keyEncAlg, publicKey etc.
 * The ukm is optional per RFC 5753. Pass a NULL value to request an empty ukm.
 * Pass a SECItem with the size set to zero, to request allocating a random
 * ukm of a default size. Or provide an explicit ukm that was defined by the caller.
 */
SECStatus
NSS_CMSUtil_EncryptSymKey_ESECDH(PLArenaPool *poolp, CERTCertificate *cert,
                                 PK11SymKey *bulkkey, SECItem *encKey,
                                 PRBool genUkm, SECItem *ukm,
                                 SECAlgorithmID *keyEncAlg, SECItem *pubKey,
                                 void *wincx)
{
    SECOidTag certalgtag; /* the certificate's encryption algorithm */
    SECStatus rv;
    SECStatus err;
    PK11SymKey *kek;
    SECKEYPublicKey *publickey = NULL;
    SECKEYPublicKey *ourPubKey = NULL;
    SECKEYPrivateKey *ourPrivKey = NULL;
    unsigned int bulkkey_size, kek_size;
    SECAlgorithmID keyWrapAlg;
    SECOidTag keyEncAlgtag;
    SECItem keyWrapAlg_params, *keyEncAlg_params, *SharedInfo;
    CK_MECHANISM_TYPE keyDerivationType, keyWrapMech;
    CK_ULONG kdf;

    if (genUkm && (ukm->len != 0 || ukm->data != NULL)) {
        PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
        return SECFailure;
    }

    certalgtag = SECOID_GetAlgorithmTag(&(cert->subjectPublicKeyInfo.algorithm));
    PORT_Assert(certalgtag == SEC_OID_ANSIX962_EC_PUBLIC_KEY);
    if (certalgtag != SEC_OID_ANSIX962_EC_PUBLIC_KEY) {
        return SECFailure;
    }

    /* Get the public key of the recipient. */
    publickey = CERT_ExtractPublicKey(cert);
    if (publickey == NULL)
        goto loser;

    ourPrivKey = SECKEY_CreateECPrivateKey(&publickey->u.ec.DEREncodedParams,
                                           &ourPubKey, wincx);
    if (!ourPrivKey || !ourPubKey) {
        goto loser;
    }

    rv = SECITEM_CopyItem(poolp, pubKey, &ourPubKey->u.ec.publicValue);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* pubKey will be encoded as a BIT STRING, so pubKey->len must be length in bits
     * rather than bytes */
    pubKey->len = pubKey->len * 8;

    SECKEY_DestroyPublicKey(ourPubKey); /* we only need the private key from now on */
    ourPubKey = NULL;

    bulkkey_size = PK11_GetKeyLength(bulkkey);
    if (bulkkey_size == 0) {
        goto loser;
    }

    /* Parameters are supposed to be absent for AES key wrap algorithms.
     * However, Microsoft Outlook cannot decrypt message unless
     * parameters field is NULL. */
    keyWrapAlg_params.len = 2;
    keyWrapAlg_params.data = (unsigned char *)PORT_ArenaAlloc(poolp, keyWrapAlg_params.len);
    if (keyWrapAlg_params.data == NULL)
        goto loser;

    keyWrapAlg_params.data[0] = SEC_ASN1_NULL;
    keyWrapAlg_params.data[1] = 0;

    /* RFC5753 specifies id-aes128-wrap as the mandatory to support algorithm.
     * So, use id-aes128-wrap unless bulkkey provides more than 128 bits of
     * security. If bulkkey provides more than 128 bits of security, use
     * the algorithms from KE Set 2 in RFC 6318. */

    /* TODO: NIST Special Publication SP 800-56A requires the use of
     * Cofactor ECDH. However, RFC 6318 specifies the use of
     * dhSinglePass-stdDH-sha256kdf-scheme or dhSinglePass-stdDH-sha384kdf-scheme
     * for Suite B. The reason for this is that all of the NIST recommended
     * prime curves in FIPS 186-3 (including the Suite B curves) have a cofactor
     * of 1, and so for these curves standard and cofactor ECDH are the same.
     * This code should really look at the key's parameters and choose cofactor
     * ECDH if the curve has a cofactor other than 1. */
    if ((PK11_GetMechanism(bulkkey) == CKM_AES_CBC) && (bulkkey_size > 16)) {
        rv = SECOID_SetAlgorithmID(poolp, &keyWrapAlg, SEC_OID_AES_256_KEY_WRAP, &keyWrapAlg_params);
        kek_size = 32;
        keyWrapMech = CKM_NSS_AES_KEY_WRAP;
        kdf = CKD_SHA384_KDF;
        keyEncAlgtag = SEC_OID_DHSINGLEPASS_STDDH_SHA384KDF_SCHEME;
        keyDerivationType = CKM_ECDH1_DERIVE;
    } else {
        rv = SECOID_SetAlgorithmID(poolp, &keyWrapAlg, SEC_OID_AES_128_KEY_WRAP, &keyWrapAlg_params);
        kek_size = 16;
        keyWrapMech = CKM_NSS_AES_KEY_WRAP;
        kdf = CKD_SHA256_KDF;
        keyEncAlgtag = SEC_OID_DHSINGLEPASS_STDDH_SHA256KDF_SCHEME;
        keyDerivationType = CKM_ECDH1_DERIVE;
    }
    if (rv != SECSuccess) {
        goto loser;
    }

    /* ukm is optional, but RFC 5753 says that originators SHOULD include the ukm.
     * I made ukm 64 bytes, since RFC 2631 states that UserKeyingMaterial must
     * contain 512 bits for Diffie-Hellman key agreement. */

    if (genUkm) {
        ukm->type = siBuffer;
        ukm->len = 64;
        ukm->data = (unsigned char *)PORT_ArenaAlloc(poolp, ukm->len);

        if (ukm->data == NULL) {
            goto loser;
        }
        rv = PK11_GenerateRandom(ukm->data, ukm->len);
        if (rv != SECSuccess) {
            goto loser;
        }
    }

    SharedInfo = Create_ECC_CMS_SharedInfo(poolp, &keyWrapAlg,
                                           ukm,
                                           kek_size);
    if (!SharedInfo) {
        goto loser;
    }

    /* Generate the KEK (key exchange key) according to RFC5753 which we use
     * to wrap the bulk encryption key. */
    kek = PK11_PubDeriveWithKDF(ourPrivKey, publickey, PR_TRUE,
                                NULL, NULL,
                                keyDerivationType, keyWrapMech,
                                CKA_WRAP, kek_size, kdf, SharedInfo, NULL);
    if (!kek) {
        goto loser;
    }

    SECKEY_DestroyPublicKey(publickey);
    SECKEY_DestroyPrivateKey(ourPrivKey);
    publickey = NULL;
    ourPrivKey = NULL;

    /* allocate space for the encrypted CEK (bulk key) */
    /* AES key wrap produces an output 64-bits longer than
     * the input AES CEK (RFC 3565, Section 2.3.2) */
    encKey->len = bulkkey_size + 8;
    encKey->data = (unsigned char *)PORT_ArenaAlloc(poolp, encKey->len);

    if (encKey->data == NULL) {
        PK11_FreeSymKey(kek);
        goto loser;
    }

    err = PK11_WrapSymKey(keyWrapMech, NULL, kek, bulkkey, encKey);

    PK11_FreeSymKey(kek); /* we do not need the KEK anymore */
    if (err != SECSuccess) {
        goto loser;
    }

    keyEncAlg_params = SEC_ASN1EncodeItem(poolp, NULL, &keyWrapAlg, SEC_ASN1_GET(SECOID_AlgorithmIDTemplate));
    if (keyEncAlg_params == NULL)
        goto loser;
    keyEncAlg_params->type = siBuffer;

    /* now set keyEncAlg */
    rv = SECOID_SetAlgorithmID(poolp, keyEncAlg, keyEncAlgtag, keyEncAlg_params);
    if (rv != SECSuccess) {
        goto loser;
    }

    return SECSuccess;

loser:
    if (publickey) {
        SECKEY_DestroyPublicKey(publickey);
    }
    if (ourPubKey) {
        SECKEY_DestroyPublicKey(ourPubKey);
    }
    if (ourPrivKey) {
        SECKEY_DestroyPrivateKey(ourPrivKey);
    }
    return SECFailure;
}

/* TODO: Move to pk11wrap and export? */
static int
cms_GetKekSizeFromKeyWrapAlgTag(SECOidTag keyWrapAlgtag)
{
    switch (keyWrapAlgtag) {
        case SEC_OID_AES_128_KEY_WRAP:
            return 16;
        case SEC_OID_AES_192_KEY_WRAP:
            return 24;
        case SEC_OID_AES_256_KEY_WRAP:
            return 32;
        default:
            break;
    }
    return 0;
}

/* TODO: Move to smimeutil and export? */
static CK_ULONG
cms_GetKdfFromKeyEncAlgTag(SECOidTag keyEncAlgtag)
{
    switch (keyEncAlgtag) {
        case SEC_OID_DHSINGLEPASS_STDDH_SHA1KDF_SCHEME:
        case SEC_OID_DHSINGLEPASS_COFACTORDH_SHA1KDF_SCHEME:
            return CKD_SHA1_KDF;
        case SEC_OID_DHSINGLEPASS_STDDH_SHA224KDF_SCHEME:
        case SEC_OID_DHSINGLEPASS_COFACTORDH_SHA224KDF_SCHEME:
            return CKD_SHA224_KDF;
        case SEC_OID_DHSINGLEPASS_STDDH_SHA256KDF_SCHEME:
        case SEC_OID_DHSINGLEPASS_COFACTORDH_SHA256KDF_SCHEME:
            return CKD_SHA256_KDF;
        case SEC_OID_DHSINGLEPASS_STDDH_SHA384KDF_SCHEME:
        case SEC_OID_DHSINGLEPASS_COFACTORDH_SHA384KDF_SCHEME:
            return CKD_SHA384_KDF;
        case SEC_OID_DHSINGLEPASS_STDDH_SHA512KDF_SCHEME:
        case SEC_OID_DHSINGLEPASS_COFACTORDH_SHA512KDF_SCHEME:
            return CKD_SHA512_KDF;
        default:
            break;
    }
    return 0;
}

PK11SymKey *
NSS_CMSUtil_DecryptSymKey_ECDH(SECKEYPrivateKey *privkey, SECItem *encKey,
                               SECAlgorithmID *keyEncAlg, SECOidTag bulkalgtag,
                               SECItem *ukm, NSSCMSOriginatorIdentifierOrKey *oiok,
                               void *wincx)
{
    SECAlgorithmID keyWrapAlg;
    SECOidTag keyEncAlgtag, keyWrapAlgtag;
    CK_MECHANISM_TYPE target, keyDerivationType, keyWrapMech;
    CK_ULONG kdf;
    PK11SymKey *kek = NULL, *bulkkey = NULL;
    int kek_size;
    SECKEYPublicKey originatorpublickey;
    SECItem *oiok_publicKey, *SharedInfo = NULL;
    SECStatus rv;

    PORT_Memset(&keyWrapAlg, 0, sizeof(SECAlgorithmID));

    PORT_Assert(bulkalgtag != SEC_OID_UNKNOWN);
    target = PK11_AlgtagToMechanism(bulkalgtag);
    if (bulkalgtag == SEC_OID_UNKNOWN || target == CKM_INVALID_MECHANISM) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        goto loser;
    }

    keyEncAlgtag = SECOID_GetAlgorithmTag(keyEncAlg);
    keyDerivationType = PK11_AlgtagToMechanism(keyEncAlgtag);
    if ((keyEncAlgtag == SEC_OID_UNKNOWN) ||
        (keyDerivationType == CKM_INVALID_MECHANISM)) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        goto loser;
    }

    rv = SEC_ASN1DecodeItem(NULL, &keyWrapAlg,
                            SEC_ASN1_GET(SECOID_AlgorithmIDTemplate), &(keyEncAlg->parameters));
    if (rv != SECSuccess) {
        goto loser;
    }

    keyWrapAlgtag = SECOID_GetAlgorithmTag(&keyWrapAlg);
    keyWrapMech = PK11_AlgtagToMechanism(keyWrapAlgtag);
    if ((keyWrapAlgtag == SEC_OID_UNKNOWN) ||
        (keyWrapMech == CKM_INVALID_MECHANISM)) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        goto loser;
    }

    kek_size = cms_GetKekSizeFromKeyWrapAlgTag(keyWrapAlgtag);
    if (!kek_size) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        goto loser;
    }

    kdf = cms_GetKdfFromKeyEncAlgTag(keyEncAlgtag);
    if (!kdf) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        goto loser;
    }

    /* Get originator's public key */
    /* TODO: Add support for static-static ECDH */
    if (oiok->identifierType != NSSCMSOriginatorIDOrKey_OriginatorPublicKey) {
        PORT_SetError(SEC_ERROR_UNSUPPORTED_MESSAGE_TYPE);
        goto loser;
    }

    /* PK11_PubDeriveWithKDF only uses the keyType u.ec.publicValue.data
     * and u.ec.publicValue.len from the originator's public key. */
    oiok_publicKey = &(oiok->id.originatorPublicKey.publicKey);
    originatorpublickey.keyType = ecKey;
    originatorpublickey.u.ec.publicValue.data = oiok_publicKey->data;
    originatorpublickey.u.ec.publicValue.len = oiok_publicKey->len / 8;

    SharedInfo = Create_ECC_CMS_SharedInfo(NULL, &keyWrapAlg, ukm, kek_size);
    if (!SharedInfo) {
        goto loser;
    }

    /* Generate the KEK (key exchange key) according to RFC5753 which we use
     * to wrap the bulk encryption key. */
    kek = PK11_PubDeriveWithKDF(privkey, &originatorpublickey, PR_TRUE,
                                NULL, NULL,
                                keyDerivationType, keyWrapMech,
                                CKA_WRAP, kek_size, kdf, SharedInfo, wincx);

    SECITEM_FreeItem(SharedInfo, PR_TRUE);

    if (kek == NULL) {
        goto loser;
    }

    bulkkey = PK11_UnwrapSymKey(kek, keyWrapMech, NULL, encKey, target, CKA_UNWRAP, 0);
    PK11_FreeSymKey(kek); /* we do not need the KEK anymore */
loser:
    SECOID_DestroyAlgorithmID(&keyWrapAlg, PR_FALSE);
    return bulkkey;
}

/* ====== ESDH (Ephemeral-Static Diffie-Hellman) ==================================== */

SECStatus
NSS_CMSUtil_EncryptSymKey_ESDH(PLArenaPool *poolp, CERTCertificate *cert, PK11SymKey *key,
                               SECItem *encKey, SECItem *ukm, SECAlgorithmID *keyEncAlg,
                               SECItem *pubKey)
{
#if 0 /* not yet done */
    SECOidTag certalgtag;       /* the certificate's encryption algorithm */
    SECOidTag encalgtag;        /* the algorithm used for key exchange/agreement */
    SECStatus rv;
    SECItem *params = NULL;
    int data_len;
    SECStatus err;
    PK11SymKey *tek;
    CERTCertificate *ourCert;
    SECKEYPublicKey *ourPubKey;
    NSSCMSKEATemplateSelector whichKEA = NSSCMSKEAInvalid;

    certalgtag = SECOID_GetAlgorithmTag(&(cert->subjectPublicKeyInfo.algorithm));
    PORT_Assert(certalgtag == SEC_OID_X942_DIFFIE_HELMAN_KEY);

    /* We really want to show our KEA tag as the key exchange algorithm tag. */
    encalgtag = SEC_OID_CMS_EPHEMERAL_STATIC_DIFFIE_HELLMAN;

    /* Get the public key of the recipient. */
    publickey = CERT_ExtractPublicKey(cert);
    if (publickey == NULL) goto loser;

    /* XXXX generate a DH key pair on a PKCS11 module (XXX which parameters?) */
    /* XXXX */ourCert = PK11_FindBestKEAMatch(cert, wincx);
    if (ourCert == NULL) goto loser;

    arena = PORT_NewArena(1024);
    if (arena == NULL) goto loser;

    /* While we're here, extract the key pair's public key data and copy it into */
    /* the outgoing parameters. */
    /* XXXX */ourPubKey = CERT_ExtractPublicKey(ourCert);
    if (ourPubKey == NULL)
    {
        goto loser;
    }
    SECITEM_CopyItem(arena, pubKey, /* XXX */&(ourPubKey->u.fortezza.KEAKey));
    SECKEY_DestroyPublicKey(ourPubKey); /* we only need the private key from now on */
    ourPubKey = NULL;

    /* Extract our private key in order to derive the KEA key. */
    ourPrivKey = PK11_FindKeyByAnyCert(ourCert,wincx);
    CERT_DestroyCertificate(ourCert); /* we're done with this */
    if (!ourPrivKey) goto loser;

    /* If ukm desired, prepare it - allocate enough space (filled with zeros). */
    if (ukm) {
        ukm->data = (unsigned char*)PORT_ArenaZAlloc(arena,/* XXXX */);
        ukm->len = /* XXXX */;
    }

    /* Generate the KEK (key exchange key) according to RFC2631 which we use
     * to wrap the bulk encryption key. */
    kek = PK11_PubDerive(ourPrivKey, publickey, PR_TRUE,
                         ukm, NULL,
                         /* XXXX */CKM_KEA_KEY_DERIVE, /* XXXX */CKM_SKIPJACK_WRAP,
                         CKA_WRAP, 0, wincx);

    SECKEY_DestroyPublicKey(publickey);
    SECKEY_DestroyPrivateKey(ourPrivKey);
    publickey = NULL;
    ourPrivKey = NULL;

    if (!kek)
        goto loser;

    /* allocate space for the encrypted CEK (bulk key) */
    encKey->data = (unsigned char*)PORT_ArenaAlloc(poolp, SMIME_FORTEZZA_MAX_KEY_SIZE);
    encKey->len = SMIME_FORTEZZA_MAX_KEY_SIZE;

    if (encKey->data == NULL)
    {
        PK11_FreeSymKey(kek);
        goto loser;
    }


    /* Wrap the bulk key using CMSRC2WRAP or CMS3DESWRAP, depending on the */
    /* bulk encryption algorithm */
    switch (/* XXXX */PK11_AlgtagToMechanism(enccinfo->encalg))
    {
    case /* XXXX */CKM_SKIPJACK_CFB8:
        err = PK11_WrapSymKey(/* XXXX */CKM_CMS3DES_WRAP, NULL, kek, bulkkey, encKey);
        whichKEA = NSSCMSKEAUsesSkipjack;
        break;
    case /* XXXX */CKM_SKIPJACK_CFB8:
        err = PK11_WrapSymKey(/* XXXX */CKM_CMSRC2_WRAP, NULL, kek, bulkkey, encKey);
        whichKEA = NSSCMSKEAUsesSkipjack;
        break;
    default:
        /* XXXX what do we do here? Neither RC2 nor 3DES... */
        err = SECFailure;
        /* set error */
        break;
    }

    PK11_FreeSymKey(kek);       /* we do not need the KEK anymore */
    if (err != SECSuccess)
        goto loser;

    PORT_Assert(whichKEA != NSSCMSKEAInvalid);

    /* see RFC2630 12.3.1.1 "keyEncryptionAlgorithm must be ..." */
    /* params is the DER encoded key wrap algorithm (with parameters!) (XXX) */
    params = SEC_ASN1EncodeItem(arena, NULL, &keaParams, sec_pkcs7_get_kea_template(whichKEA));
    if (params == NULL)
        goto loser;

    /* now set keyEncAlg */
    rv = SECOID_SetAlgorithmID(poolp, keyEncAlg, SEC_OID_CMS_EPHEMERAL_STATIC_DIFFIE_HELLMAN, params);
    if (rv != SECSuccess)
        goto loser;

    /* XXXXXXX this is not right yet */
loser:
    if (arena) {
        PORT_FreeArena(arena, PR_FALSE);
    }
    if (publickey) {
        SECKEY_DestroyPublicKey(publickey);
    }
    if (ourPrivKey) {
        SECKEY_DestroyPrivateKey(ourPrivKey);
    }
#endif
    return SECFailure;
}

PK11SymKey *
NSS_CMSUtil_DecryptSymKey_ESDH(SECKEYPrivateKey *privkey, SECItem *encKey,
                               SECAlgorithmID *keyEncAlg, SECOidTag bulkalgtag,
                               void *pwfn_arg)
{
#if 0 /* not yet done */
    SECStatus err;
    CK_MECHANISM_TYPE bulkType;
    PK11SymKey *tek;
    SECKEYPublicKey *originatorPubKey;
    NSSCMSSMIMEKEAParameters keaParams;

   /* XXXX get originator's public key */
   originatorPubKey = PK11_MakeKEAPubKey(keaParams.originatorKEAKey.data,
                           keaParams.originatorKEAKey.len);
   if (originatorPubKey == NULL)
      goto loser;

   /* Generate the TEK (token exchange key) which we use to unwrap the bulk encryption key.
      The Derive function generates a shared secret and combines it with the originatorRA
      data to come up with an unique session key */
   tek = PK11_PubDerive(privkey, originatorPubKey, PR_FALSE,
                         &keaParams.originatorRA, NULL,
                         CKM_KEA_KEY_DERIVE, CKM_SKIPJACK_WRAP,
                         CKA_WRAP, 0, pwfn_arg);
   SECKEY_DestroyPublicKey(originatorPubKey);   /* not needed anymore */
   if (tek == NULL)
        goto loser;

    /* Now that we have the TEK, unwrap the bulk key
       with which to decrypt the message. */
    /* Skipjack is being used as the bulk encryption algorithm.*/
    /* Unwrap the bulk key. */
    bulkkey = PK11_UnwrapSymKey(tek, CKM_SKIPJACK_WRAP, NULL,
                                encKey, CKM_SKIPJACK_CBC64, CKA_DECRYPT, 0);

    return bulkkey;

loser:
#endif
    return NULL;
}
