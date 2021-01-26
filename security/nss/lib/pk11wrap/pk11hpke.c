/*
 * draft-irtf-cfrg-hpke-07
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "keyhi.h"
#include "pkcs11t.h"
#include "pk11func.h"
#include "pk11hpke.h"
#include "pk11pqg.h"
#include "secerr.h"
#include "secitem.h"
#include "secmod.h"
#include "secmodi.h"
#include "secmodti.h"
#include "secutil.h"

#ifndef NSS_ENABLE_DRAFT_HPKE
/* "Not Implemented" stubs to maintain the ABI. */
SECStatus
PK11_HPKE_ValidateParameters(HpkeKemId kemId, HpkeKdfId kdfId, HpkeAeadId aeadId)
{
    PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
    return SECFailure;
}
HpkeContext *
PK11_HPKE_NewContext(HpkeKemId kemId, HpkeKdfId kdfId, HpkeAeadId aeadId,
                     PK11SymKey *psk, const SECItem *pskId)
{

    PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
    return NULL;
}
SECStatus
PK11_HPKE_Deserialize(const HpkeContext *cx, const PRUint8 *enc,
                      unsigned int encLen, SECKEYPublicKey **outPubKey)
{
    PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
    return SECFailure;
}
void
PK11_HPKE_DestroyContext(HpkeContext *cx, PRBool freeit)
{
    PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
}
const SECItem *
PK11_HPKE_GetEncapPubKey(const HpkeContext *cx)
{
    PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
    return NULL;
}
SECStatus
PK11_HPKE_ExportContext(const HpkeContext *cx, PK11SymKey *wrapKey, SECItem **serialized)
{
    PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
    return SECFailure;
}
SECStatus
PK11_HPKE_ExportSecret(const HpkeContext *cx, const SECItem *info,
                       unsigned int L, PK11SymKey **outKey)
{
    PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
    return SECFailure;
}
HpkeContext *
PK11_HPKE_ImportContext(const SECItem *serialized, PK11SymKey *wrapKey)
{
    PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
    return NULL;
}
SECStatus
PK11_HPKE_Open(HpkeContext *cx, const SECItem *aad, const SECItem *ct,
               SECItem **outPt)
{
    PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
    return SECFailure;
}
SECStatus
PK11_HPKE_Seal(HpkeContext *cx, const SECItem *aad, const SECItem *pt, SECItem **outCt)
{
    PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
    return SECFailure;
}
SECStatus
PK11_HPKE_Serialize(const SECKEYPublicKey *pk, PRUint8 *buf, unsigned int *len, unsigned int maxLen)
{
    PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
    return SECFailure;
}
SECStatus
PK11_HPKE_SetupS(HpkeContext *cx, const SECKEYPublicKey *pkE, SECKEYPrivateKey *skE,
                 SECKEYPublicKey *pkR, const SECItem *info)
{
    PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
    return SECFailure;
}
SECStatus
PK11_HPKE_SetupR(HpkeContext *cx, const SECKEYPublicKey *pkR, SECKEYPrivateKey *skR,
                 const SECItem *enc, const SECItem *info)
{
    PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
    return SECFailure;
}

#else
#define SERIALIZATION_VERSION 1

static const char *DRAFT_LABEL = "HPKE-07";
static const char *EXP_LABEL = "exp";
static const char *HPKE_LABEL = "HPKE";
static const char *INFO_LABEL = "info_hash";
static const char *KEM_LABEL = "KEM";
static const char *KEY_LABEL = "key";
static const char *NONCE_LABEL = "base_nonce";
static const char *PSK_ID_LABEL = "psk_id_hash";
static const char *SECRET_LABEL = "secret";
static const char *SEC_LABEL = "sec";
static const char *EAE_PRK_LABEL = "eae_prk";
static const char *SH_SEC_LABEL = "shared_secret";

struct HpkeContextStr {
    const hpkeKemParams *kemParams;
    const hpkeKdfParams *kdfParams;
    const hpkeAeadParams *aeadParams;
    PRUint8 mode;               /* Base and PSK modes supported. */
    SECItem *encapPubKey;       /* Marshalled public key, sent to receiver. */
    SECItem *baseNonce;         /* Deterministic nonce for AEAD. */
    SECItem *pskId;             /* PSK identifier (non-secret). */
    PK11Context *aeadContext;   /* AEAD context used by Seal/Open. */
    PRUint64 sequenceNumber;    /* seqNo for decrypt IV construction. */
    PK11SymKey *sharedSecret;   /* ExtractAndExpand output key. */
    PK11SymKey *key;            /* Key used with the AEAD. */
    PK11SymKey *exporterSecret; /* Derivation key for ExportSecret. */
    PK11SymKey *psk;            /* PSK imported by the application. */
};

static const hpkeKemParams kemParams[] = {
    /* KEM, Nsk, Nsecret, Npk, oidTag, Hash mechanism  */
    { HpkeDhKemX25519Sha256, 32, 32, 32, SEC_OID_CURVE25519, CKM_SHA256 },
};

#define MAX_WRAPPED_EXP_LEN 72   // Largest kdfParams->Nh + 8
static const hpkeKdfParams kdfParams[] = {
    /* KDF, Nh, mechanism  */
    { HpkeKdfHkdfSha256, SHA256_LENGTH, CKM_SHA256 },
    { HpkeKdfHkdfSha384, SHA384_LENGTH, CKM_SHA384 },
    { HpkeKdfHkdfSha512, SHA512_LENGTH, CKM_SHA512 },
};
#define MAX_WRAPPED_KEY_LEN 40   // Largest aeadParams->Nk + 8
static const hpkeAeadParams aeadParams[] = {
    /* AEAD, Nk, Nn, tagLen, mechanism  */
    { HpkeAeadAes128Gcm, 16, 12, 16, CKM_AES_GCM },
    { HpkeAeadChaCha20Poly1305, 32, 12, 16, CKM_CHACHA20_POLY1305 },
};

static inline const hpkeKemParams *
kemId2Params(HpkeKemId kemId)
{
    switch (kemId) {
        case HpkeDhKemX25519Sha256:
            return &kemParams[0];
        default:
            return NULL;
    }
}

static inline const hpkeKdfParams *
kdfId2Params(HpkeKdfId kdfId)
{
    switch (kdfId) {
        case HpkeKdfHkdfSha256:
            return &kdfParams[0];
        case HpkeKdfHkdfSha384:
            return &kdfParams[1];
        case HpkeKdfHkdfSha512:
            return &kdfParams[2];
        default:
            return NULL;
    }
}

static const inline hpkeAeadParams *
aeadId2Params(HpkeAeadId aeadId)
{
    switch (aeadId) {
        case HpkeAeadAes128Gcm:
            return &aeadParams[0];
        case HpkeAeadChaCha20Poly1305:
            return &aeadParams[1];
        default:
            return NULL;
    }
}

static PRUint8 *
encodeNumber(PRUint64 value, PRUint8 *b, size_t count)
{
    PRUint64 encoded;
    PORT_Assert(b && count > 0 && count <= sizeof(encoded));

    encoded = PR_htonll(value);
    PORT_Memcpy(b, ((unsigned char *)(&encoded)) + (sizeof(encoded) - count),
                count);
    return b + count;
}

static PRUint8 *
decodeNumber(PRUint64 *value, PRUint8 *b, size_t count)
{
    unsigned int i;
    PRUint64 number = 0;
    PORT_Assert(b && value && count <= sizeof(*value));

    for (i = 0; i < count; i++) {
        number = (number << 8) + b[i];
    }
    *value = number;
    return b + count;
}

SECStatus
PK11_HPKE_ValidateParameters(HpkeKemId kemId, HpkeKdfId kdfId, HpkeAeadId aeadId)
{
    /* If more variants are added, ensure the combination is also
     * legal. For now it is, since only the AEAD may vary. */
    const hpkeKemParams *kem = kemId2Params(kemId);
    const hpkeKdfParams *kdf = kdfId2Params(kdfId);
    const hpkeAeadParams *aead = aeadId2Params(aeadId);
    if (!kem || !kdf || !aead) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    return SECSuccess;
}

HpkeContext *
PK11_HPKE_NewContext(HpkeKemId kemId, HpkeKdfId kdfId, HpkeAeadId aeadId,
                     PK11SymKey *psk, const SECItem *pskId)
{
    SECStatus rv = SECSuccess;
    PK11SlotInfo *slot = NULL;
    HpkeContext *cx = NULL;
    /* Both the PSK and the PSK ID default to empty. */
    SECItem emptyItem = { siBuffer, NULL, 0 };

    cx = PORT_ZNew(HpkeContext);
    if (!cx) {
        return NULL;
    }
    cx->mode = psk ? HpkeModePsk : HpkeModeBase;
    cx->kemParams = kemId2Params(kemId);
    cx->kdfParams = kdfId2Params(kdfId);
    cx->aeadParams = aeadId2Params(aeadId);
    CHECK_FAIL_ERR((!!psk != !!pskId), SEC_ERROR_INVALID_ARGS);
    CHECK_FAIL_ERR(!cx->kemParams || !cx->kdfParams || !cx->aeadParams,
                   SEC_ERROR_INVALID_ARGS);

    /* Import the provided PSK or the default. */
    slot = PK11_GetBestSlot(CKM_EC_KEY_PAIR_GEN, NULL);
    CHECK_FAIL(!slot);
    if (psk) {
        cx->psk = PK11_ReferenceSymKey(psk);
        cx->pskId = SECITEM_DupItem(pskId);
    } else {
        cx->psk = PK11_ImportDataKey(slot, CKM_HKDF_DATA, PK11_OriginUnwrap,
                                     CKA_DERIVE, &emptyItem, NULL);
        cx->pskId = SECITEM_DupItem(&emptyItem);
    }
    CHECK_FAIL(!cx->psk);
    CHECK_FAIL(!cx->pskId);

CLEANUP:
    if (rv != SECSuccess) {
        PK11_FreeSymKey(cx->psk);
        SECITEM_FreeItem(cx->pskId, PR_TRUE);
        cx->pskId = NULL;
        cx->psk = NULL;
        PORT_Free(cx);
        cx = NULL;
    }
    if (slot) {
        PK11_FreeSlot(slot);
    }
    return cx;
}

void
PK11_HPKE_DestroyContext(HpkeContext *cx, PRBool freeit)
{
    if (!cx) {
        return;
    }

    if (cx->aeadContext) {
        PK11_DestroyContext((PK11Context *)cx->aeadContext, PR_TRUE);
        cx->aeadContext = NULL;
    }
    PK11_FreeSymKey(cx->exporterSecret);
    PK11_FreeSymKey(cx->sharedSecret);
    PK11_FreeSymKey(cx->key);
    PK11_FreeSymKey(cx->psk);
    SECITEM_FreeItem(cx->pskId, PR_TRUE);
    SECITEM_FreeItem(cx->baseNonce, PR_TRUE);
    SECITEM_FreeItem(cx->encapPubKey, PR_TRUE);
    cx->exporterSecret = NULL;
    cx->sharedSecret = NULL;
    cx->key = NULL;
    cx->psk = NULL;
    cx->pskId = NULL;
    cx->baseNonce = NULL;
    cx->encapPubKey = NULL;
    if (freeit) {
        PORT_ZFree(cx, sizeof(HpkeContext));
    }
}

/* Export Format:
    struct {
        uint8 serilizationVersion;
        uint8 hpkeVersion;
        uint16 kemId;
        uint16 kdfId;
        uint16 aeadId;
        uint16 modeId;
        uint64 sequenceNumber;
        opaque senderPubKey<1..2^16-1>;
        opaque baseNonce<1..2^16-1>;
        opaque key<1..2^16-1>;
        opaque exporterSecret<1..2^16-1>;
    } HpkeSerializedContext
*/
#define EXPORTED_CTX_BASE_LEN 26 /* Fixed size plus 2B for each variable. */
#define REMAINING_BYTES(walker, buf) \
    buf->len - (walker - buf->data)
SECStatus
PK11_HPKE_ExportContext(const HpkeContext *cx, PK11SymKey *wrapKey, SECItem **serialized)
{
    SECStatus rv;
    size_t allocLen;
    PRUint8 *walker;
    SECItem *keyBytes = NULL;      // Maybe wrapped
    SECItem *exporterBytes = NULL; // Maybe wrapped
    SECItem *serializedCx = NULL;
    PRUint8 wrappedKeyBytes[MAX_WRAPPED_KEY_LEN] = { 0 };
    PRUint8 wrappedExpBytes[MAX_WRAPPED_EXP_LEN] = { 0 };
    SECItem wrappedKey = { siBuffer, wrappedKeyBytes, sizeof(wrappedKeyBytes) };
    SECItem wrappedExp = { siBuffer, wrappedExpBytes, sizeof(wrappedExpBytes) };

    CHECK_FAIL_ERR((!cx || !cx->aeadContext || !serialized), SEC_ERROR_INVALID_ARGS);
    CHECK_FAIL_ERR((cx->aeadContext->operation != (CKA_NSS_MESSAGE | CKA_DECRYPT)),
                   SEC_ERROR_NOT_A_RECIPIENT);

    /* If a wrapping key was provided, do the wrap first
     * so that we know what size to allocate. */
    if (wrapKey) {
        rv = PK11_WrapSymKey(CKM_AES_KEY_WRAP_KWP, NULL, wrapKey,
                             cx->key, &wrappedKey);
        CHECK_RV(rv);
        rv = PK11_WrapSymKey(CKM_AES_KEY_WRAP_KWP, NULL, wrapKey,
                             cx->exporterSecret, &wrappedExp);
        CHECK_RV(rv);

        keyBytes = &wrappedKey;
        exporterBytes = &wrappedExp;
    } else {
        rv = PK11_ExtractKeyValue(cx->key);
        CHECK_RV(rv);
        keyBytes = PK11_GetKeyData(cx->key);
        CHECK_FAIL(!keyBytes);
        PORT_Assert(keyBytes->len == cx->aeadParams->Nk);

        rv = PK11_ExtractKeyValue(cx->exporterSecret);
        CHECK_RV(rv);
        exporterBytes = PK11_GetKeyData(cx->exporterSecret);
        CHECK_FAIL(!exporterBytes);
        PORT_Assert(exporterBytes->len == cx->kdfParams->Nh);
    }

    allocLen = EXPORTED_CTX_BASE_LEN + cx->baseNonce->len + cx->encapPubKey->len;
    allocLen += wrapKey ? wrappedKey.len : cx->aeadParams->Nk;
    allocLen += wrapKey ? wrappedExp.len : cx->kdfParams->Nh;

    serializedCx = SECITEM_AllocItem(NULL, NULL, allocLen);
    CHECK_FAIL(!serializedCx);

    walker = &serializedCx->data[0];
    *(walker)++ = (PRUint8)SERIALIZATION_VERSION;
    *(walker)++ = (PRUint8)HPKE_DRAFT_VERSION;

    walker = encodeNumber(cx->kemParams->id, walker, 2);
    walker = encodeNumber(cx->kdfParams->id, walker, 2);
    walker = encodeNumber(cx->aeadParams->id, walker, 2);
    walker = encodeNumber(cx->mode, walker, 2);
    walker = encodeNumber(cx->sequenceNumber, walker, 8);

    /* sender public key, serialized. */
    walker = encodeNumber(cx->encapPubKey->len, walker, 2);
    PORT_Memcpy(walker, cx->encapPubKey->data, cx->encapPubKey->len);
    walker += cx->encapPubKey->len;

    /* base nonce */
    walker = encodeNumber(cx->baseNonce->len, walker, 2);
    PORT_Memcpy(walker, cx->baseNonce->data, cx->baseNonce->len);
    walker += cx->baseNonce->len;

    /* key. */
    walker = encodeNumber(keyBytes->len, walker, 2);
    PORT_Memcpy(walker, keyBytes->data, keyBytes->len);
    walker += keyBytes->len;

    /* exporter_secret. */
    walker = encodeNumber(exporterBytes->len, walker, 2);
    PORT_Memcpy(walker, exporterBytes->data, exporterBytes->len);
    walker += exporterBytes->len;

    CHECK_FAIL_ERR(REMAINING_BYTES(walker, serializedCx) != 0,
                   SEC_ERROR_LIBRARY_FAILURE);
    *serialized = serializedCx;

CLEANUP:
    if (rv != SECSuccess) {
        SECITEM_ZfreeItem(serializedCx, PR_TRUE);
    }
    return rv;
}

HpkeContext *
PK11_HPKE_ImportContext(const SECItem *serialized, PK11SymKey *wrapKey)
{
    SECStatus rv = SECSuccess;
    HpkeContext *cx = NULL;
    PRUint8 *walker;
    PRUint64 tmpn;
    PRUint8 tmp8;
    HpkeKemId kem;
    HpkeKdfId kdf;
    HpkeAeadId aead;
    PK11SlotInfo *slot = NULL;
    PK11SymKey *tmpKey = NULL;
    SECItem tmpItem = { siBuffer, NULL, 0 };
    SECItem emptyItem = { siBuffer, NULL, 0 };

    CHECK_FAIL_ERR((!serialized || !serialized->data || serialized->len == 0),
                   SEC_ERROR_INVALID_ARGS);
    CHECK_FAIL_ERR((serialized->len < EXPORTED_CTX_BASE_LEN), SEC_ERROR_BAD_DATA);

    walker = serialized->data;

    tmp8 = *(walker++);
    CHECK_FAIL_ERR((tmp8 != SERIALIZATION_VERSION), SEC_ERROR_BAD_DATA);
    tmp8 = *(walker++);
    CHECK_FAIL_ERR((tmp8 != HPKE_DRAFT_VERSION), SEC_ERROR_INVALID_ALGORITHM);

    walker = decodeNumber(&tmpn, walker, 2);
    kem = (HpkeKemId)tmpn;

    walker = decodeNumber(&tmpn, walker, 2);
    kdf = (HpkeKdfId)tmpn;

    walker = decodeNumber(&tmpn, walker, 2);
    aead = (HpkeAeadId)tmpn;

    /* Create context. We'll manually set the mode, though we
     * no longer have the PSK and have no need for it. */
    cx = PK11_HPKE_NewContext(kem, kdf, aead, NULL, NULL);
    CHECK_FAIL(!cx);

    walker = decodeNumber(&tmpn, walker, 2);
    CHECK_FAIL_ERR((tmpn != HpkeModeBase && tmpn != HpkeModePsk),
                   SEC_ERROR_BAD_DATA);
    cx->mode = (HpkeModeId)tmpn;

    walker = decodeNumber(&cx->sequenceNumber, walker, 8);
    slot = PK11_GetBestSlot(CKM_HKDF_DERIVE, NULL);
    CHECK_FAIL(!slot);

    /* Import sender public key (serialized). */
    walker = decodeNumber(&tmpn, walker, 2);
    CHECK_FAIL_ERR(tmpn >= REMAINING_BYTES(walker, serialized),
                   SEC_ERROR_BAD_DATA);
    tmpItem.data = walker;
    tmpItem.len = tmpn;
    cx->encapPubKey = SECITEM_DupItem(&tmpItem);
    CHECK_FAIL(!cx->encapPubKey);
    walker += tmpItem.len;

    /* Import base_nonce. */
    walker = decodeNumber(&tmpn, walker, 2);
    CHECK_FAIL_ERR(tmpn != cx->aeadParams->Nn, SEC_ERROR_BAD_DATA);
    CHECK_FAIL_ERR(tmpn >= REMAINING_BYTES(walker, serialized),
                   SEC_ERROR_BAD_DATA);
    tmpItem.data = walker;
    tmpItem.len = tmpn;
    cx->baseNonce = SECITEM_DupItem(&tmpItem);
    CHECK_FAIL(!cx->baseNonce);
    walker += tmpItem.len;

    /* Import key */
    walker = decodeNumber(&tmpn, walker, 2);
    CHECK_FAIL_ERR(tmpn >= REMAINING_BYTES(walker, serialized),
                   SEC_ERROR_BAD_DATA);
    tmpItem.data = walker;
    tmpItem.len = tmpn;
    walker += tmpItem.len;
    if (wrapKey) {
        cx->key = PK11_UnwrapSymKey(wrapKey, CKM_AES_KEY_WRAP_KWP,
                                    NULL, &tmpItem, cx->aeadParams->mech,
                                    CKA_NSS_MESSAGE | CKA_DECRYPT, 0);
        CHECK_FAIL(!cx->key);
    } else {
        CHECK_FAIL_ERR(tmpn != cx->aeadParams->Nk, SEC_ERROR_BAD_DATA);
        tmpKey = PK11_ImportSymKey(slot, cx->aeadParams->mech,
                                   PK11_OriginUnwrap, CKA_NSS_MESSAGE | CKA_DECRYPT,
                                   &tmpItem, NULL);
        CHECK_FAIL(!tmpKey);
        cx->key = tmpKey;
    }

    /* Import exporter_secret. */
    walker = decodeNumber(&tmpn, walker, 2);
    CHECK_FAIL_ERR(tmpn != REMAINING_BYTES(walker, serialized),
                   SEC_ERROR_BAD_DATA);
    tmpItem.data = walker;
    tmpItem.len = tmpn;
    walker += tmpItem.len;

    if (wrapKey) {
        cx->exporterSecret = PK11_UnwrapSymKey(wrapKey, CKM_AES_KEY_WRAP_KWP,
                                               NULL, &tmpItem, cx->kdfParams->mech,
                                               CKM_HKDF_DERIVE, 0);
        CHECK_FAIL(!cx->exporterSecret);
    } else {
        CHECK_FAIL_ERR(tmpn != cx->kdfParams->Nh, SEC_ERROR_BAD_DATA);
        tmpKey = PK11_ImportSymKey(slot, CKM_HKDF_DERIVE, PK11_OriginUnwrap,
                                   CKA_DERIVE, &tmpItem, NULL);
        CHECK_FAIL(!tmpKey);
        cx->exporterSecret = tmpKey;
    }

    cx->aeadContext = PK11_CreateContextBySymKey(cx->aeadParams->mech,
                                                 CKA_NSS_MESSAGE | CKA_DECRYPT,
                                                 cx->key, &emptyItem);

CLEANUP:
    if (rv != SECSuccess) {
        PK11_FreeSymKey(tmpKey);
        PK11_HPKE_DestroyContext(cx, PR_TRUE);
        cx = NULL;
    }
    if (slot) {
        PK11_FreeSlot(slot);
    }

    return cx;
}

SECStatus
PK11_HPKE_Serialize(const SECKEYPublicKey *pk, PRUint8 *buf, unsigned int *len, unsigned int maxLen)
{
    if (!pk || !len || pk->keyType != ecKey) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* If no buffer provided, return the length required for
     * the serialized public key. */
    if (!buf) {
        *len = pk->u.ec.publicValue.len;
        return SECSuccess;
    }

    if (maxLen < pk->u.ec.publicValue.len) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return SECFailure;
    }

    PORT_Memcpy(buf, pk->u.ec.publicValue.data, pk->u.ec.publicValue.len);
    *len = pk->u.ec.publicValue.len;
    return SECSuccess;
};

SECStatus
PK11_HPKE_Deserialize(const HpkeContext *cx, const PRUint8 *enc,
                      unsigned int encLen, SECKEYPublicKey **outPubKey)
{
    SECStatus rv;
    SECKEYPublicKey *pubKey = NULL;
    SECOidData *oidData = NULL;
    PLArenaPool *arena;

    if (!cx || !enc || encLen == 0 || !outPubKey) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    CHECK_FAIL(!arena);
    pubKey = PORT_ArenaZNew(arena, SECKEYPublicKey);
    CHECK_FAIL(!pubKey);

    pubKey->arena = arena;
    pubKey->keyType = ecKey;
    pubKey->pkcs11Slot = NULL;
    pubKey->pkcs11ID = CK_INVALID_HANDLE;

    rv = SECITEM_MakeItem(pubKey->arena, &pubKey->u.ec.publicValue,
                          enc, encLen);
    CHECK_RV(rv);
    pubKey->u.ec.encoding = ECPoint_Undefined;
    pubKey->u.ec.size = 0;

    oidData = SECOID_FindOIDByTag(cx->kemParams->oidTag);
    CHECK_FAIL_ERR(!oidData, SEC_ERROR_INVALID_ALGORITHM);

    // Create parameters.
    CHECK_FAIL(!SECITEM_AllocItem(pubKey->arena, &pubKey->u.ec.DEREncodedParams,
                                  2 + oidData->oid.len));

    // Set parameters.
    pubKey->u.ec.DEREncodedParams.data[0] = SEC_ASN1_OBJECT_ID;
    pubKey->u.ec.DEREncodedParams.data[1] = oidData->oid.len;
    PORT_Memcpy(pubKey->u.ec.DEREncodedParams.data + 2, oidData->oid.data, oidData->oid.len);
    *outPubKey = pubKey;

CLEANUP:
    if (rv != SECSuccess) {
        SECKEY_DestroyPublicKey(pubKey);
    }
    return rv;
};

static SECStatus
pk11_hpke_CheckKeys(const HpkeContext *cx, const SECKEYPublicKey *pk,
                    const SECKEYPrivateKey *sk)
{
    SECOidTag pkTag;
    unsigned int i;
    if (pk->keyType != ecKey || (sk && sk->keyType != ecKey)) {
        PORT_SetError(SEC_ERROR_BAD_KEY);
        return SECFailure;
    }
    pkTag = SECKEY_GetECCOid(&pk->u.ec.DEREncodedParams);
    if (pkTag != cx->kemParams->oidTag) {
        PORT_SetError(SEC_ERROR_BAD_KEY);
        return SECFailure;
    }
    for (i = 0; i < PR_ARRAY_SIZE(kemParams); i++) {
        if (cx->kemParams->oidTag == kemParams[i].oidTag) {
            return SECSuccess;
        }
    }

    return SECFailure;
}

static SECStatus
pk11_hpke_GenerateKeyPair(const HpkeContext *cx, SECKEYPublicKey **pkE,
                          SECKEYPrivateKey **skE)
{
    SECStatus rv = SECSuccess;
    SECKEYPrivateKey *privKey = NULL;
    SECKEYPublicKey *pubKey = NULL;
    SECOidData *oidData = NULL;
    SECKEYECParams ecp;
    PK11SlotInfo *slot = NULL;
    ecp.data = NULL;
    PORT_Assert(cx && skE && pkE);

    oidData = SECOID_FindOIDByTag(cx->kemParams->oidTag);
    CHECK_FAIL_ERR(!oidData, SEC_ERROR_INVALID_ALGORITHM);
    ecp.data = PORT_Alloc(2 + oidData->oid.len);
    CHECK_FAIL(!ecp.data);

    ecp.len = 2 + oidData->oid.len;
    ecp.type = siDEROID;
    ecp.data[0] = SEC_ASN1_OBJECT_ID;
    ecp.data[1] = oidData->oid.len;
    PORT_Memcpy(&ecp.data[2], oidData->oid.data, oidData->oid.len);

    slot = PK11_GetBestSlot(CKM_EC_KEY_PAIR_GEN, NULL);
    CHECK_FAIL(!slot);

    privKey = PK11_GenerateKeyPair(slot, CKM_EC_KEY_PAIR_GEN, &ecp, &pubKey,
                                   PR_FALSE, PR_TRUE, NULL);
    CHECK_FAIL_ERR((!privKey || !pubKey), SEC_ERROR_KEYGEN_FAIL);
    PORT_Assert(rv == SECSuccess);
    *skE = privKey;
    *pkE = pubKey;

CLEANUP:
    if (rv != SECSuccess) {
        SECKEY_DestroyPrivateKey(privKey);
        SECKEY_DestroyPublicKey(pubKey);
    }
    if (slot) {
        PK11_FreeSlot(slot);
    }
    PORT_Free(ecp.data);
    return rv;
}

static inline SECItem *
pk11_hpke_MakeExtractLabel(const char *prefix, unsigned int prefixLen,
                           const char *label, unsigned int labelLen,
                           const SECItem *suiteId, const SECItem *ikm)
{
    SECItem *out = NULL;
    PRUint8 *walker;
    out = SECITEM_AllocItem(NULL, NULL, prefixLen + labelLen + suiteId->len + (ikm ? ikm->len : 0));
    if (!out) {
        return NULL;
    }

    walker = out->data;
    PORT_Memcpy(walker, prefix, prefixLen);
    walker += prefixLen;
    PORT_Memcpy(walker, suiteId->data, suiteId->len);
    walker += suiteId->len;
    PORT_Memcpy(walker, label, labelLen);
    walker += labelLen;
    if (ikm && ikm->data) {
        PORT_Memcpy(walker, ikm->data, ikm->len);
    }

    return out;
}

static SECStatus
pk11_hpke_LabeledExtractData(const HpkeContext *cx, SECItem *salt,
                             const SECItem *suiteId, const char *label,
                             unsigned int labelLen, const SECItem *ikm, SECItem **out)
{
    SECStatus rv;
    CK_HKDF_PARAMS params = { 0 };
    PK11SymKey *importedIkm = NULL;
    PK11SymKey *prk = NULL;
    PK11SlotInfo *slot = NULL;
    SECItem *borrowed;
    SECItem *outDerived = NULL;
    SECItem *labeledIkm;
    SECItem paramsItem = { siBuffer, (unsigned char *)&params,
                           sizeof(params) };
    PORT_Assert(cx && ikm && label && labelLen && out && suiteId);

    labeledIkm = pk11_hpke_MakeExtractLabel(DRAFT_LABEL, strlen(DRAFT_LABEL), label, labelLen, suiteId, ikm);
    CHECK_FAIL(!labeledIkm);
    params.bExtract = CK_TRUE;
    params.bExpand = CK_FALSE;
    params.prfHashMechanism = cx->kdfParams->mech;
    params.ulSaltType = salt ? CKF_HKDF_SALT_DATA : CKF_HKDF_SALT_NULL;
    params.pSalt = salt ? (CK_BYTE_PTR)salt->data : NULL;
    params.ulSaltLen = salt ? salt->len : 0;
    params.pInfo = labeledIkm->data;
    params.ulInfoLen = labeledIkm->len;

    slot = PK11_GetBestSlot(CKM_EC_KEY_PAIR_GEN, NULL);
    CHECK_FAIL(!slot);

    importedIkm = PK11_ImportDataKey(slot, CKM_HKDF_DATA, PK11_OriginUnwrap,
                                     CKA_DERIVE, labeledIkm, NULL);
    CHECK_FAIL(!importedIkm);
    prk = PK11_Derive(importedIkm, CKM_HKDF_DATA, &paramsItem,
                      CKM_HKDF_DERIVE, CKA_DERIVE, 0);
    CHECK_FAIL(!prk);
    rv = PK11_ExtractKeyValue(prk);
    CHECK_RV(rv);
    borrowed = PK11_GetKeyData(prk);
    CHECK_FAIL(!borrowed);
    outDerived = SECITEM_DupItem(borrowed);
    CHECK_FAIL(!outDerived);

    *out = outDerived;

CLEANUP:
    PK11_FreeSymKey(importedIkm);
    PK11_FreeSymKey(prk);
    SECITEM_FreeItem(labeledIkm, PR_TRUE);
    if (slot) {
        PK11_FreeSlot(slot);
    }
    return rv;
}

static SECStatus
pk11_hpke_LabeledExtract(const HpkeContext *cx, PK11SymKey *salt,
                         const SECItem *suiteId, const char *label, CK_MECHANISM_TYPE hashMech,
                         unsigned int labelLen, PK11SymKey *ikm, PK11SymKey **out)
{
    SECStatus rv = SECSuccess;
    SECItem *innerLabel = NULL;
    PK11SymKey *labeledIkm = NULL;
    PK11SymKey *prk = NULL;
    CK_HKDF_PARAMS params = { 0 };
    CK_KEY_DERIVATION_STRING_DATA labelData;
    SECItem labelDataItem = { siBuffer, NULL, 0 };
    SECItem paramsItem = { siBuffer, (unsigned char *)&params,
                           sizeof(params) };
    PORT_Assert(cx && ikm && label && labelLen && out && suiteId);

    innerLabel = pk11_hpke_MakeExtractLabel(DRAFT_LABEL, strlen(DRAFT_LABEL), label, labelLen, suiteId, NULL);
    CHECK_FAIL(!innerLabel);
    labelData.pData = innerLabel->data;
    labelData.ulLen = innerLabel->len;
    labelDataItem.data = (PRUint8 *)&labelData;
    labelDataItem.len = sizeof(labelData);
    labeledIkm = PK11_Derive(ikm, CKM_CONCATENATE_DATA_AND_BASE,
                             &labelDataItem, CKM_GENERIC_SECRET_KEY_GEN, CKA_DERIVE, 0);
    CHECK_FAIL(!labeledIkm);

    params.bExtract = CK_TRUE;
    params.bExpand = CK_FALSE;
    params.prfHashMechanism = hashMech;
    params.ulSaltType = salt ? CKF_HKDF_SALT_KEY : CKF_HKDF_SALT_NULL;
    params.hSaltKey = salt ? PK11_GetSymKeyHandle(salt) : CK_INVALID_HANDLE;

    prk = PK11_Derive(labeledIkm, CKM_HKDF_DERIVE, &paramsItem,
                      CKM_HKDF_DERIVE, CKA_DERIVE, 0);
    CHECK_FAIL(!prk);
    *out = prk;

CLEANUP:
    PK11_FreeSymKey(labeledIkm);
    SECITEM_ZfreeItem(innerLabel, PR_TRUE);
    return rv;
}

static SECStatus
pk11_hpke_LabeledExpand(const HpkeContext *cx, PK11SymKey *prk, const SECItem *suiteId,
                        const char *label, unsigned int labelLen, const SECItem *info,
                        unsigned int L, CK_MECHANISM_TYPE hashMech, PK11SymKey **outKey,
                        SECItem **outItem)
{
    SECStatus rv = SECSuccess;
    CK_MECHANISM_TYPE keyMech;
    CK_MECHANISM_TYPE deriveMech;
    CK_HKDF_PARAMS params = { 0 };
    PK11SymKey *derivedKey = NULL;
    SECItem *labeledInfoItem = NULL;
    SECItem paramsItem = { siBuffer, (unsigned char *)&params,
                           sizeof(params) };
    SECItem *derivedKeyData;
    PRUint8 encodedL[2];
    PRUint8 *walker = encodedL;
    size_t len;
    PORT_Assert(cx && prk && label && (!!outKey != !!outItem));

    walker = encodeNumber(L, walker, 2);
    len = info ? info->len : 0;
    len += sizeof(encodedL) + strlen(DRAFT_LABEL) + suiteId->len + labelLen;
    labeledInfoItem = SECITEM_AllocItem(NULL, NULL, len);
    CHECK_FAIL(!labeledInfoItem);

    walker = labeledInfoItem->data;
    PORT_Memcpy(walker, encodedL, sizeof(encodedL));
    walker += sizeof(encodedL);
    PORT_Memcpy(walker, DRAFT_LABEL, strlen(DRAFT_LABEL));
    walker += strlen(DRAFT_LABEL);
    PORT_Memcpy(walker, suiteId->data, suiteId->len);
    walker += suiteId->len;
    PORT_Memcpy(walker, label, labelLen);
    walker += labelLen;
    if (info) {
        PORT_Memcpy(walker, info->data, info->len);
    }

    params.bExtract = CK_FALSE;
    params.bExpand = CK_TRUE;
    params.prfHashMechanism = hashMech;
    params.ulSaltType = CKF_HKDF_SALT_NULL;
    params.pInfo = labeledInfoItem->data;
    params.ulInfoLen = labeledInfoItem->len;
    deriveMech = outItem ? CKM_HKDF_DATA : CKM_HKDF_DERIVE;
    /* If we're expanding to the encryption key use the appropriate mechanism. */
    keyMech = (label && !strcmp(KEY_LABEL, label)) ? cx->aeadParams->mech : CKM_HKDF_DERIVE;

    derivedKey = PK11_Derive(prk, deriveMech, &paramsItem, keyMech, CKA_DERIVE, L);
    CHECK_FAIL(!derivedKey);

    if (outItem) {
        /* Don't allow export of real keys. */
        CHECK_FAIL_ERR(deriveMech != CKM_HKDF_DATA, SEC_ERROR_LIBRARY_FAILURE);
        rv = PK11_ExtractKeyValue(derivedKey);
        CHECK_RV(rv);
        derivedKeyData = PK11_GetKeyData(derivedKey);
        CHECK_FAIL_ERR((!derivedKeyData), SEC_ERROR_NO_KEY);
        *outItem = SECITEM_DupItem(derivedKeyData);
        CHECK_FAIL(!*outItem);
        PK11_FreeSymKey(derivedKey);
    } else {
        *outKey = derivedKey;
    }

CLEANUP:
    if (rv != SECSuccess) {
        PK11_FreeSymKey(derivedKey);
    }
    SECITEM_ZfreeItem(labeledInfoItem, PR_TRUE);
    return rv;
}

static SECStatus
pk11_hpke_ExtractAndExpand(const HpkeContext *cx, PK11SymKey *ikm,
                           const SECItem *kemContext, PK11SymKey **out)
{
    SECStatus rv;
    PK11SymKey *eaePrk = NULL;
    PK11SymKey *sharedSecret = NULL;
    PRUint8 suiteIdBuf[5];
    PRUint8 *walker;
    PORT_Memcpy(suiteIdBuf, KEM_LABEL, strlen(KEM_LABEL));
    SECItem suiteIdItem = { siBuffer, suiteIdBuf, sizeof(suiteIdBuf) };
    PORT_Assert(cx && ikm && kemContext && out);

    walker = &suiteIdBuf[3];
    walker = encodeNumber(cx->kemParams->id, walker, 2);

    rv = pk11_hpke_LabeledExtract(cx, NULL, &suiteIdItem, EAE_PRK_LABEL,
                                  cx->kemParams->hashMech, strlen(EAE_PRK_LABEL),
                                  ikm, &eaePrk);
    CHECK_RV(rv);

    rv = pk11_hpke_LabeledExpand(cx, eaePrk, &suiteIdItem, SH_SEC_LABEL, strlen(SH_SEC_LABEL),
                                 kemContext, cx->kemParams->Nsecret, cx->kemParams->hashMech,
                                 &sharedSecret, NULL);
    CHECK_RV(rv);
    *out = sharedSecret;

CLEANUP:
    if (rv != SECSuccess) {
        PK11_FreeSymKey(sharedSecret);
    }
    PK11_FreeSymKey(eaePrk);
    return rv;
}

static SECStatus
pk11_hpke_Encap(HpkeContext *cx, const SECKEYPublicKey *pkE, SECKEYPrivateKey *skE,
                SECKEYPublicKey *pkR)
{
    SECStatus rv;
    PK11SymKey *dh = NULL;
    SECItem *kemContext = NULL;
    SECItem *encPkR = NULL;
    unsigned int tmpLen;

    PORT_Assert(cx && skE && pkE && pkR);

    rv = pk11_hpke_CheckKeys(cx, pkE, skE);
    CHECK_RV(rv);
    rv = pk11_hpke_CheckKeys(cx, pkR, NULL);
    CHECK_RV(rv);

    dh = PK11_PubDeriveWithKDF(skE, pkR, PR_FALSE, NULL, NULL, CKM_ECDH1_DERIVE,
                               CKM_SHA512_HMAC /* unused */, CKA_DERIVE, 0,
                               CKD_NULL, NULL, NULL);
    CHECK_FAIL(!dh);

    /* Encapsulate our sender public key. Many use cases
     * (including ECH) require that the application fetch
     * this value, so do it once and store into the cx. */
    rv = PK11_HPKE_Serialize(pkE, NULL, &tmpLen, 0);
    CHECK_RV(rv);
    cx->encapPubKey = SECITEM_AllocItem(NULL, NULL, tmpLen);
    CHECK_FAIL(!cx->encapPubKey);
    rv = PK11_HPKE_Serialize(pkE, cx->encapPubKey->data,
                             &cx->encapPubKey->len, cx->encapPubKey->len);
    CHECK_RV(rv);

    rv = PK11_HPKE_Serialize(pkR, NULL, &tmpLen, 0);
    CHECK_RV(rv);

    kemContext = SECITEM_AllocItem(NULL, NULL, cx->encapPubKey->len + tmpLen);
    CHECK_FAIL(!kemContext);

    PORT_Memcpy(kemContext->data, cx->encapPubKey->data, cx->encapPubKey->len);
    rv = PK11_HPKE_Serialize(pkR, &kemContext->data[cx->encapPubKey->len], &tmpLen, tmpLen);
    CHECK_RV(rv);

    rv = pk11_hpke_ExtractAndExpand(cx, dh, kemContext, &cx->sharedSecret);
    CHECK_RV(rv);

CLEANUP:
    if (rv != SECSuccess) {
        PK11_FreeSymKey(cx->sharedSecret);
        cx->sharedSecret = NULL;
    }
    SECITEM_FreeItem(encPkR, PR_TRUE);
    SECITEM_FreeItem(kemContext, PR_TRUE);
    PK11_FreeSymKey(dh);
    return rv;
}

SECStatus
PK11_HPKE_ExportSecret(const HpkeContext *cx, const SECItem *info, unsigned int L,
                       PK11SymKey **out)
{
    SECStatus rv;
    PK11SymKey *exported;
    PRUint8 suiteIdBuf[10];
    PRUint8 *walker;
    PORT_Memcpy(suiteIdBuf, HPKE_LABEL, strlen(HPKE_LABEL));
    SECItem suiteIdItem = { siBuffer, suiteIdBuf, sizeof(suiteIdBuf) };

    /* Arbitrary info length limit well under the specified max. */
    if (!cx || !info || (!info->data && info->len) || info->len > 0xFFFF ||
        !L || (L > 255 * cx->kdfParams->Nh)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    walker = &suiteIdBuf[4];
    walker = encodeNumber(cx->kemParams->id, walker, 2);
    walker = encodeNumber(cx->kdfParams->id, walker, 2);
    walker = encodeNumber(cx->aeadParams->id, walker, 2);

    rv = pk11_hpke_LabeledExpand(cx, cx->exporterSecret, &suiteIdItem, SEC_LABEL,
                                 strlen(SEC_LABEL), info, L, cx->kdfParams->mech,
                                 &exported, NULL);
    CHECK_RV(rv);
    *out = exported;

CLEANUP:
    return rv;
}

static SECStatus
pk11_hpke_Decap(HpkeContext *cx, const SECKEYPublicKey *pkR, SECKEYPrivateKey *skR,
                const SECItem *encS)
{
    SECStatus rv;
    PK11SymKey *dh = NULL;
    SECItem *encR = NULL;
    SECItem *kemContext = NULL;
    SECKEYPublicKey *pkS = NULL;
    unsigned int tmpLen;

    if (!cx || !skR || !pkR || !encS || !encS->data || !encS->len) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    rv = PK11_HPKE_Deserialize(cx, encS->data, encS->len, &pkS);
    CHECK_RV(rv);

    rv = pk11_hpke_CheckKeys(cx, pkR, skR);
    CHECK_RV(rv);
    rv = pk11_hpke_CheckKeys(cx, pkS, NULL);
    CHECK_RV(rv);

    dh = PK11_PubDeriveWithKDF(skR, pkS, PR_FALSE, NULL, NULL, CKM_ECDH1_DERIVE,
                               CKM_SHA512_HMAC /* unused */, CKA_DERIVE, 0,
                               CKD_NULL, NULL, NULL);
    CHECK_FAIL(!dh);

    /* kem_context = concat(enc, pkRm) */
    rv = PK11_HPKE_Serialize(pkR, NULL, &tmpLen, 0);
    CHECK_RV(rv);

    kemContext = SECITEM_AllocItem(NULL, NULL, encS->len + tmpLen);
    CHECK_FAIL(!kemContext);

    PORT_Memcpy(kemContext->data, encS->data, encS->len);
    rv = PK11_HPKE_Serialize(pkR, &kemContext->data[encS->len], &tmpLen,
                             kemContext->len - encS->len);
    CHECK_RV(rv);
    rv = pk11_hpke_ExtractAndExpand(cx, dh, kemContext, &cx->sharedSecret);
    CHECK_RV(rv);

    /* Store the sender serialized public key, which
     * may be required by application use cases. */
    cx->encapPubKey = SECITEM_DupItem(encS);
    CHECK_FAIL(!cx->encapPubKey);

CLEANUP:
    if (rv != SECSuccess) {
        PK11_FreeSymKey(cx->sharedSecret);
        cx->sharedSecret = NULL;
    }
    PK11_FreeSymKey(dh);
    SECKEY_DestroyPublicKey(pkS);
    SECITEM_FreeItem(encR, PR_TRUE);
    SECITEM_ZfreeItem(kemContext, PR_TRUE);
    return rv;
}

const SECItem *
PK11_HPKE_GetEncapPubKey(const HpkeContext *cx)
{
    if (!cx) {
        return NULL;
    }
    return cx->encapPubKey;
}

static SECStatus
pk11_hpke_KeySchedule(HpkeContext *cx, const SECItem *info)
{
    SECStatus rv;
    SECItem contextItem = { siBuffer, NULL, 0 };
    unsigned int len;
    unsigned int off;
    PK11SymKey *secret = NULL;
    SECItem *pskIdHash = NULL;
    SECItem *infoHash = NULL;
    PRUint8 suiteIdBuf[10];
    PRUint8 *walker;
    PORT_Memcpy(suiteIdBuf, HPKE_LABEL, strlen(HPKE_LABEL));
    SECItem suiteIdItem = { siBuffer, suiteIdBuf, sizeof(suiteIdBuf) };
    PORT_Assert(cx && info && cx->psk && cx->pskId);

    walker = &suiteIdBuf[4];
    walker = encodeNumber(cx->kemParams->id, walker, 2);
    walker = encodeNumber(cx->kdfParams->id, walker, 2);
    walker = encodeNumber(cx->aeadParams->id, walker, 2);

    rv = pk11_hpke_LabeledExtractData(cx, NULL, &suiteIdItem, PSK_ID_LABEL,
                                      strlen(PSK_ID_LABEL), cx->pskId, &pskIdHash);
    CHECK_RV(rv);
    rv = pk11_hpke_LabeledExtractData(cx, NULL, &suiteIdItem, INFO_LABEL,
                                      strlen(INFO_LABEL), info, &infoHash);
    CHECK_RV(rv);

    // Make the context string
    len = sizeof(cx->mode) + pskIdHash->len + infoHash->len;
    CHECK_FAIL(!SECITEM_AllocItem(NULL, &contextItem, len));
    off = 0;
    PORT_Memcpy(&contextItem.data[off], &cx->mode, sizeof(cx->mode));
    off += sizeof(cx->mode);
    PORT_Memcpy(&contextItem.data[off], pskIdHash->data, pskIdHash->len);
    off += pskIdHash->len;
    PORT_Memcpy(&contextItem.data[off], infoHash->data, infoHash->len);
    off += infoHash->len;

    // Compute the keys
    rv = pk11_hpke_LabeledExtract(cx, cx->sharedSecret, &suiteIdItem, SECRET_LABEL,
                                  cx->kdfParams->mech, strlen(SECRET_LABEL),
                                  cx->psk, &secret);
    CHECK_RV(rv);
    rv = pk11_hpke_LabeledExpand(cx, secret, &suiteIdItem, KEY_LABEL, strlen(KEY_LABEL),
                                 &contextItem, cx->aeadParams->Nk, cx->kdfParams->mech,
                                 &cx->key, NULL);
    CHECK_RV(rv);
    rv = pk11_hpke_LabeledExpand(cx, secret, &suiteIdItem, NONCE_LABEL, strlen(NONCE_LABEL),
                                 &contextItem, cx->aeadParams->Nn, cx->kdfParams->mech,
                                 NULL, &cx->baseNonce);
    CHECK_RV(rv);
    rv = pk11_hpke_LabeledExpand(cx, secret, &suiteIdItem, EXP_LABEL, strlen(EXP_LABEL),
                                 &contextItem, cx->kdfParams->Nh, cx->kdfParams->mech,
                                 &cx->exporterSecret, NULL);
    CHECK_RV(rv);

CLEANUP:
    /* If !SECSuccess, callers will tear down the context. */
    PK11_FreeSymKey(secret);
    SECITEM_FreeItem(&contextItem, PR_FALSE);
    SECITEM_FreeItem(infoHash, PR_TRUE);
    SECITEM_FreeItem(pskIdHash, PR_TRUE);
    return rv;
}

SECStatus
PK11_HPKE_SetupR(HpkeContext *cx, const SECKEYPublicKey *pkR, SECKEYPrivateKey *skR,
                 const SECItem *enc, const SECItem *info)
{
    SECStatus rv;
    SECItem empty = { siBuffer, NULL, 0 };

    CHECK_FAIL_ERR((!cx || !skR || !info || !enc || !enc->data || !enc->len),
                   SEC_ERROR_INVALID_ARGS);
    /* Already setup */
    CHECK_FAIL_ERR((cx->aeadContext), SEC_ERROR_INVALID_STATE);

    rv = pk11_hpke_Decap(cx, pkR, skR, enc);
    CHECK_RV(rv);
    rv = pk11_hpke_KeySchedule(cx, info);
    CHECK_RV(rv);

    /* Store the key context for subsequent calls to Open().
     * PK11_CreateContextBySymKey refs the key internally. */
    PORT_Assert(cx->key);
    cx->aeadContext = PK11_CreateContextBySymKey(cx->aeadParams->mech,
                                                 CKA_NSS_MESSAGE | CKA_DECRYPT,
                                                 cx->key, &empty);
    CHECK_FAIL_ERR((!cx->aeadContext), SEC_ERROR_LIBRARY_FAILURE);

CLEANUP:
    if (rv != SECSuccess) {
        /* Clear everything past NewContext. */
        PK11_HPKE_DestroyContext(cx, PR_FALSE);
    }
    return rv;
}

SECStatus
PK11_HPKE_SetupS(HpkeContext *cx, const SECKEYPublicKey *pkE, SECKEYPrivateKey *skE,
                 SECKEYPublicKey *pkR, const SECItem *info)
{
    SECStatus rv;
    SECItem empty = { siBuffer, NULL, 0 };
    SECKEYPublicKey *tmpPkE = NULL;
    SECKEYPrivateKey *tmpSkE = NULL;
    CHECK_FAIL_ERR((!cx || !pkR || !info || (!!skE != !!pkE)), SEC_ERROR_INVALID_ARGS);
    /* Already setup */
    CHECK_FAIL_ERR((cx->aeadContext), SEC_ERROR_INVALID_STATE);

    /* If NULL was passed for the local keypair, generate one. */
    if (skE == NULL) {
        rv = pk11_hpke_GenerateKeyPair(cx, &tmpPkE, &tmpSkE);
        if (rv != SECSuccess) {
            /* Code set */
            return SECFailure;
        }
        rv = pk11_hpke_Encap(cx, tmpPkE, tmpSkE, pkR);
    } else {
        rv = pk11_hpke_Encap(cx, pkE, skE, pkR);
    }
    CHECK_RV(rv);

    SECItem defaultInfo = { siBuffer, NULL, 0 };
    if (!info || !info->data) {
        info = &defaultInfo;
    }
    rv = pk11_hpke_KeySchedule(cx, info);
    CHECK_RV(rv);

    PORT_Assert(cx->key);
    cx->aeadContext = PK11_CreateContextBySymKey(cx->aeadParams->mech,
                                                 CKA_NSS_MESSAGE | CKA_ENCRYPT,
                                                 cx->key, &empty);
    CHECK_FAIL_ERR((!cx->aeadContext), SEC_ERROR_LIBRARY_FAILURE);

CLEANUP:
    if (rv != SECSuccess) {
        /* Clear everything past NewContext. */
        PK11_HPKE_DestroyContext(cx, PR_FALSE);
    }
    SECKEY_DestroyPrivateKey(tmpSkE);
    SECKEY_DestroyPublicKey(tmpPkE);
    return rv;
}

SECStatus
PK11_HPKE_Seal(HpkeContext *cx, const SECItem *aad, const SECItem *pt,
               SECItem **out)
{
    SECStatus rv;
    PRUint8 ivOut[12] = { 0 };
    SECItem *ct = NULL;
    size_t maxOut;
    unsigned char tagBuf[HASH_LENGTH_MAX];
    size_t tagLen;
    unsigned int fixedBits;
    PORT_Assert(cx->baseNonce->len == sizeof(ivOut));
    PORT_Memcpy(ivOut, cx->baseNonce->data, cx->baseNonce->len);

    /* aad may be NULL, PT may be zero-length but not NULL. */
    if (!cx || !cx->aeadContext ||
        (aad && aad->len && !aad->data) ||
        !pt || (pt->len && !pt->data) ||
        !out) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    tagLen = cx->aeadParams->tagLen;
    maxOut = pt->len + tagLen;
    fixedBits = (cx->baseNonce->len - 8) * 8;
    ct = SECITEM_AllocItem(NULL, NULL, maxOut);
    CHECK_FAIL(!ct);

    rv = PK11_AEADOp(cx->aeadContext,
                     CKG_GENERATE_COUNTER_XOR, fixedBits,
                     ivOut, sizeof(ivOut),
                     aad ? aad->data : NULL,
                     aad ? aad->len : 0,
                     ct->data, (int *)&ct->len, maxOut,
                     tagBuf, tagLen,
                     pt->data, pt->len);
    CHECK_RV(rv);
    CHECK_FAIL_ERR((ct->len > maxOut - tagLen), SEC_ERROR_LIBRARY_FAILURE);

    /* Append the tag to the ciphertext. */
    PORT_Memcpy(&ct->data[ct->len], tagBuf, tagLen);
    ct->len += tagLen;
    *out = ct;

CLEANUP:
    if (rv != SECSuccess) {
        SECITEM_ZfreeItem(ct, PR_TRUE);
    }
    return rv;
}

/* PKCS #11 defines the IV generator function to be ignored on
 * decrypt (i.e. it uses the nonce input, as provided, as the IV).
 * The sequence number is kept independently on each endpoint and
 * the XORed IV is not transmitted, so we have to do our own IV
 * construction XOR outside of the token. */
static SECStatus
pk11_hpke_makeIv(HpkeContext *cx, PRUint8 *iv, size_t ivLen)
{
    unsigned int counterLen = sizeof(cx->sequenceNumber);
    PORT_Assert(cx->baseNonce->len == ivLen);
    PORT_Assert(counterLen == 8);
    if (cx->sequenceNumber == PR_UINT64(0xffffffffffffffff)) {
        /* Overflow */
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        return SECFailure;
    }

    PORT_Memcpy(iv, cx->baseNonce->data, cx->baseNonce->len);
    for (size_t i = 0; i < counterLen; i++) {
        iv[cx->baseNonce->len - 1 - i] ^=
            PORT_GET_BYTE_BE(cx->sequenceNumber,
                             counterLen - 1 - i, counterLen);
    }
    return SECSuccess;
}

SECStatus
PK11_HPKE_Open(HpkeContext *cx, const SECItem *aad,
               const SECItem *ct, SECItem **out)
{
    SECStatus rv;
    PRUint8 constructedNonce[12] = { 0 };
    unsigned int tagLen;
    SECItem *pt = NULL;

    /* aad may be NULL, CT may be zero-length but not NULL. */
    if ((!cx || !cx->aeadContext || !ct || !out) ||
        (aad && aad->len && !aad->data) ||
        (!ct->data || (ct->data && !ct->len))) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    tagLen = cx->aeadParams->tagLen;
    CHECK_FAIL_ERR((ct->len < tagLen), SEC_ERROR_INVALID_ARGS);

    pt = SECITEM_AllocItem(NULL, NULL, ct->len);
    CHECK_FAIL(!pt);

    rv = pk11_hpke_makeIv(cx, constructedNonce, sizeof(constructedNonce));
    CHECK_RV(rv);

    rv = PK11_AEADOp(cx->aeadContext, CKG_NO_GENERATE, 0,
                     constructedNonce, sizeof(constructedNonce),
                     aad ? aad->data : NULL,
                     aad ? aad->len : 0,
                     pt->data, (int *)&pt->len, pt->len,
                     &ct->data[ct->len - tagLen], tagLen,
                     ct->data, ct->len - tagLen);
    CHECK_RV(rv);
    cx->sequenceNumber++;
    *out = pt;

CLEANUP:
    if (rv != SECSuccess) {
        SECITEM_ZfreeItem(pt, PR_TRUE);
    }
    return rv;
}

#endif // NSS_ENABLE_DRAFT_HPKE
