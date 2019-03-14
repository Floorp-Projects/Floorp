/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define TLS13_ESNI_VERSION 0xff01

/*
 *  struct {
 *      uint16 version;
 *      uint8 checksum[4];
 *      KeyShareEntry keys<4..2^16-1>;
 *      CipherSuite cipher_suites<2..2^16-2>;
 *      uint16 padded_length;
 *      uint64 not_before;
 *      uint64 not_after;
 *      Extension extensions<0..2^16-1>;
 *  } ESNIKeys;
 */
#include "nss.h"
#include "pk11func.h"
#include "ssl.h"
#include "sslproto.h"
#include "sslimpl.h"
#include "ssl3exthandle.h"
#include "tls13esni.h"
#include "tls13exthandle.h"
#include "tls13hkdf.h"

const char kHkdfPurposeEsniKey[] = "esni key";
const char kHkdfPurposeEsniIv[] = "esni iv";

void
tls13_DestroyESNIKeys(sslEsniKeys *keys)
{
    if (!keys) {
        return;
    }
    SECITEM_FreeItem(&keys->data, PR_FALSE);
    PORT_Free((void *)keys->dummySni);
    tls13_DestroyKeyShares(&keys->keyShares);
    ssl_FreeEphemeralKeyPair(keys->privKey);
    SECITEM_FreeItem(&keys->suites, PR_FALSE);
    PORT_ZFree(keys, sizeof(sslEsniKeys));
}

sslEsniKeys *
tls13_CopyESNIKeys(sslEsniKeys *okeys)
{
    sslEsniKeys *nkeys;
    SECStatus rv;

    PORT_Assert(okeys);

    nkeys = PORT_ZNew(sslEsniKeys);
    if (!nkeys) {
        return NULL;
    }
    PR_INIT_CLIST(&nkeys->keyShares);
    rv = SECITEM_CopyItem(NULL, &nkeys->data, &okeys->data);
    if (rv != SECSuccess) {
        goto loser;
    }
    if (okeys->dummySni) {
        nkeys->dummySni = PORT_Strdup(okeys->dummySni);
        if (!nkeys->dummySni) {
            goto loser;
        }
    }
    for (PRCList *cur_p = PR_LIST_HEAD(&okeys->keyShares);
         cur_p != &okeys->keyShares;
         cur_p = PR_NEXT_LINK(cur_p)) {
        TLS13KeyShareEntry *copy = tls13_CopyKeyShareEntry(
            (TLS13KeyShareEntry *)cur_p);
        if (!copy) {
            goto loser;
        }
        PR_APPEND_LINK(&copy->link, &nkeys->keyShares);
    }
    if (okeys->privKey) {
        nkeys->privKey = ssl_CopyEphemeralKeyPair(okeys->privKey);
        if (!nkeys->privKey) {
            goto loser;
        }
    }
    rv = SECITEM_CopyItem(NULL, &nkeys->suites, &okeys->suites);
    if (rv != SECSuccess) {
        goto loser;
    }
    nkeys->paddedLength = okeys->paddedLength;
    nkeys->notBefore = okeys->notBefore;
    nkeys->notAfter = okeys->notAfter;
    return nkeys;

loser:
    tls13_DestroyESNIKeys(nkeys);
    return NULL;
}

/* Checksum is a 4-byte array. */
static SECStatus
tls13_ComputeESNIKeysChecksum(const PRUint8 *buf, unsigned int len,
                              PRUint8 *checksum)
{
    SECItem copy;
    SECStatus rv;
    PRUint8 sha256[32];

    rv = SECITEM_MakeItem(NULL, &copy, buf, len);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    /* Stomp the checksum. */
    PORT_Memset(copy.data + 2, 0, 4);

    rv = PK11_HashBuf(ssl3_HashTypeToOID(ssl_hash_sha256),
                      sha256,
                      copy.data, copy.len);
    SECITEM_FreeItem(&copy, PR_FALSE);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    PORT_Memcpy(checksum, sha256, 4);
    return SECSuccess;
}

static SECStatus
tls13_DecodeESNIKeys(SECItem *data, sslEsniKeys **keysp)
{
    SECStatus rv;
    sslReadBuffer tmp;
    PRUint64 tmpn;
    sslEsniKeys *keys;
    PRUint8 checksum[4];
    sslReader rdr = SSL_READER(data->data, data->len);

    rv = sslRead_ReadNumber(&rdr, 2, &tmpn);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    if (tmpn != TLS13_ESNI_VERSION) {
        PORT_SetError(SSL_ERROR_UNSUPPORTED_VERSION);
        return SECFailure;
    }
    keys = PORT_ZNew(sslEsniKeys);
    if (!keys) {
        return SECFailure;
    }
    PR_INIT_CLIST(&keys->keyShares);

    /* Make a copy. */
    rv = SECITEM_CopyItem(NULL, &keys->data, data);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = tls13_ComputeESNIKeysChecksum(data->data, data->len, checksum);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Read and check checksum. */
    rv = sslRead_Read(&rdr, 4, &tmp);
    if (rv != SECSuccess) {
        goto loser;
    }

    if (0 != NSS_SecureMemcmp(tmp.buf, checksum, 4)) {
        goto loser;
    }

    /* Parse the key shares. */
    rv = sslRead_ReadVariable(&rdr, 2, &tmp);
    if (rv != SECSuccess) {
        goto loser;
    }

    sslReader rdr2 = SSL_READER(tmp.buf, tmp.len);
    while (SSL_READER_REMAINING(&rdr2)) {
        TLS13KeyShareEntry *ks = NULL;

        rv = tls13_DecodeKeyShareEntry(&rdr2, &ks);
        if (rv != SECSuccess) {
            goto loser;
        }

        if (ks) {
            PR_APPEND_LINK(&ks->link, &keys->keyShares);
        }
    }

    /* Parse cipher suites. */
    rv = sslRead_ReadVariable(&rdr, 2, &tmp);
    if (rv != SECSuccess) {
        goto loser;
    }
    /* This can't be odd. */
    if (tmp.len & 1) {
        goto loser;
    }
    rv = SECITEM_MakeItem(NULL, &keys->suites, (PRUint8 *)tmp.buf, tmp.len);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Padded Length */
    rv = sslRead_ReadNumber(&rdr, 2, &tmpn);
    if (rv != SECSuccess) {
        goto loser;
    }
    keys->paddedLength = (PRUint16)tmpn;

    /* Not Before */
    rv = sslRead_ReadNumber(&rdr, 8, &keys->notBefore);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Not After */
    rv = sslRead_ReadNumber(&rdr, 8, &keys->notAfter);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Extensions, which we ignore. */
    rv = sslRead_ReadVariable(&rdr, 2, &tmp);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Check that this is empty. */
    if (SSL_READER_REMAINING(&rdr) > 0) {
        goto loser;
    }

    *keysp = keys;
    return SECSuccess;

loser:
    tls13_DestroyESNIKeys(keys);
    PORT_SetError(SSL_ERROR_RX_MALFORMED_ESNI_KEYS);

    return SECFailure;
}

/* Encode an ESNI keys structure. We only allow one key
 * share. */
SECStatus
SSLExp_EncodeESNIKeys(PRUint16 *cipherSuites, unsigned int cipherSuiteCount,
                      SSLNamedGroup group, SECKEYPublicKey *pubKey,
                      PRUint16 pad, PRUint64 notBefore, PRUint64 notAfter,
                      PRUint8 *out, unsigned int *outlen, unsigned int maxlen)
{
    unsigned int savedOffset;
    SECStatus rv;
    sslBuffer b = SSL_BUFFER_EMPTY;

    rv = sslBuffer_AppendNumber(&b, TLS13_ESNI_VERSION, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = sslBuffer_Skip(&b, 4, &savedOffset);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Length of vector. */
    rv = sslBuffer_AppendNumber(
        &b, tls13_SizeOfKeyShareEntry(pubKey), 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Our one key share. */
    rv = tls13_EncodeKeyShareEntry(&b, group, pubKey);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Cipher suites. */
    rv = sslBuffer_AppendNumber(&b, cipherSuiteCount * 2, 2);
    if (rv != SECSuccess) {
        goto loser;
    }
    for (unsigned int i = 0; i < cipherSuiteCount; i++) {
        rv = sslBuffer_AppendNumber(&b, cipherSuites[i], 2);
        if (rv != SECSuccess) {
            goto loser;
        }
    }

    /* Padding Length. Fixed for now. */
    rv = sslBuffer_AppendNumber(&b, pad, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Start time. */
    rv = sslBuffer_AppendNumber(&b, notBefore, 8);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* End time. */
    rv = sslBuffer_AppendNumber(&b, notAfter, 8);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* No extensions. */
    rv = sslBuffer_AppendNumber(&b, 0, 2);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = tls13_ComputeESNIKeysChecksum(SSL_BUFFER_BASE(&b),
                                       SSL_BUFFER_LEN(&b),
                                       SSL_BUFFER_BASE(&b) + 2);
    if (rv != SECSuccess) {
        PORT_Assert(PR_FALSE);
        goto loser;
    }

    if (SSL_BUFFER_LEN(&b) > maxlen) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        goto loser;
    }
    PORT_Memcpy(out, SSL_BUFFER_BASE(&b), SSL_BUFFER_LEN(&b));
    *outlen = SSL_BUFFER_LEN(&b);

    sslBuffer_Clear(&b);
    return SECSuccess;
loser:
    sslBuffer_Clear(&b);
    return SECFailure;
}

SECStatus
SSLExp_SetESNIKeyPair(PRFileDesc *fd,
                      SECKEYPrivateKey *privKey,
                      const PRUint8 *record, unsigned int recordLen)
{
    sslSocket *ss;
    SECStatus rv;
    sslEsniKeys *keys = NULL;
    SECKEYPublicKey *pubKey = NULL;
    SECItem data = { siBuffer, CONST_CAST(PRUint8, record), recordLen };
    PLArenaPool *arena = NULL;

    ss = ssl_FindSocket(fd);
    if (!ss) {
        SSL_DBG(("%d: SSL[%d]: bad socket in %s",
                 SSL_GETPID(), fd, __FUNCTION__));
        return SECFailure;
    }

    rv = tls13_DecodeESNIKeys(&data, &keys);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    /* Check the cipher suites. */
    (void)ssl3_config_match_init(ss);
    /* Make sure the cipher suite is OK. */
    SSLVersionRange vrange = { SSL_LIBRARY_VERSION_TLS_1_3,
                               SSL_LIBRARY_VERSION_TLS_1_3 };

    sslReader csrdr = SSL_READER(keys->suites.data,
                                 keys->suites.len);
    while (SSL_READER_REMAINING(&csrdr)) {
        PRUint64 asuite;

        rv = sslRead_ReadNumber(&csrdr, 2, &asuite);
        if (rv != SECSuccess) {
            goto loser;
        }
        const ssl3CipherSuiteCfg *suiteCfg =
            ssl_LookupCipherSuiteCfg(asuite, ss->cipherSuites);
        if (!ssl3_config_match(suiteCfg, ss->ssl3.policy, &vrange, ss)) {
            /* Illegal suite. */
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            goto loser;
        }
    }

    if (PR_CLIST_IS_EMPTY(&keys->keyShares)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        goto loser;
    }
    if (PR_PREV_LINK(&keys->keyShares) != PR_NEXT_LINK(&keys->keyShares)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        goto loser;
    }
    TLS13KeyShareEntry *entry = (TLS13KeyShareEntry *)PR_LIST_HEAD(
        &keys->keyShares);
    if (entry->group->keaType != ssl_kea_ecdh) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        goto loser;
    }
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena) {
        goto loser;
    }
    pubKey = PORT_ArenaZNew(arena, SECKEYPublicKey);
    if (!pubKey) {
        goto loser;
    }
    pubKey->arena = arena;
    arena = NULL; /* From here, this will be destroyed with the pubkey. */
    /* Dummy PKCS11 values because this key isn't on a slot. */
    pubKey->pkcs11Slot = NULL;
    pubKey->pkcs11ID = CK_INVALID_HANDLE;
    rv = ssl_ImportECDHKeyShare(pubKey,
                                entry->key_exchange.data,
                                entry->key_exchange.len,
                                entry->group);
    if (rv != SECSuccess) {
        goto loser;
    }

    privKey = SECKEY_CopyPrivateKey(privKey);
    if (!privKey) {
        goto loser;
    }
    keys->privKey = ssl_NewEphemeralKeyPair(entry->group, privKey, pubKey);
    if (!keys->privKey) {
        goto loser;
    }
    pubKey = NULL;
    ss->esniKeys = keys;
    return SECSuccess;

loser:
    PORT_FreeArena(arena, PR_FALSE);
    SECKEY_DestroyPublicKey(pubKey);
    tls13_DestroyESNIKeys(keys);
    return SECFailure;
}

SECStatus
SSLExp_EnableESNI(PRFileDesc *fd,
                  const PRUint8 *esniKeys,
                  unsigned int esniKeysLen,
                  const char *dummySNI)
{
    sslSocket *ss;
    sslEsniKeys *keys = NULL;
    SECItem data = { siBuffer, CONST_CAST(PRUint8, esniKeys), esniKeysLen };
    SECStatus rv;

    ss = ssl_FindSocket(fd);
    if (!ss) {
        SSL_DBG(("%d: SSL[%d]: bad socket in %s",
                 SSL_GETPID(), fd, __FUNCTION__));
        return SECFailure;
    }

    rv = tls13_DecodeESNIKeys(&data, &keys);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    if (dummySNI) {
        keys->dummySni = PORT_Strdup(dummySNI);
        if (!keys->dummySni) {
            tls13_DestroyESNIKeys(keys);
            return SECFailure;
        }
    }

    /* Delete in case it was set before. */
    tls13_DestroyESNIKeys(ss->esniKeys);
    ss->esniKeys = keys;

    return SECSuccess;
}

/*
 * struct {
 *     opaque record_digest<0..2^16-1>;
 *     KeyShareEntry esni_key_share;
 *     Random client_hello_random;
 * } ESNIContents;
 */
SECStatus
tls13_ComputeESNIKeys(const sslSocket *ss,
                      TLS13KeyShareEntry *entry,
                      sslKeyPair *keyPair,
                      const ssl3CipherSuiteDef *suite,
                      const PRUint8 *esniKeysHash,
                      const PRUint8 *keyShareBuf,
                      unsigned int keyShareBufLen,
                      const PRUint8 *clientRandom,
                      ssl3KeyMaterial *keyMat)
{
    PK11SymKey *Z = NULL;
    PK11SymKey *Zx = NULL;
    SECStatus ret = SECFailure;
    PRUint8 esniContentsBuf[256]; /* Just big enough. */
    sslBuffer esniContents = SSL_BUFFER(esniContentsBuf);
    PRUint8 hash[64];
    const ssl3BulkCipherDef *cipherDef = ssl_GetBulkCipherDef(suite);
    size_t keySize = cipherDef->key_size;
    size_t ivSize = cipherDef->iv_size +
                    cipherDef->explicit_nonce_size; /* This isn't always going to
                                                     * work, but it does for
                                                     * AES-GCM */
    unsigned int hashSize = tls13_GetHashSizeForHash(suite->prf_hash);
    SECStatus rv;

    rv = tls13_HandleKeyShare(CONST_CAST(sslSocket, ss), entry, keyPair,
                              suite->prf_hash, &Z);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = tls13_HkdfExtract(NULL, Z, suite->prf_hash, &Zx);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Encode ESNIContents. */
    rv = sslBuffer_AppendVariable(&esniContents,
                                  esniKeysHash, hashSize, 2);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = sslBuffer_Append(&esniContents, keyShareBuf, keyShareBufLen);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = sslBuffer_Append(&esniContents, clientRandom, SSL3_RANDOM_LENGTH);
    if (rv != SECSuccess) {
        goto loser;
    }

    PORT_Assert(hashSize <= sizeof(hash));
    rv = PK11_HashBuf(ssl3_HashTypeToOID(suite->prf_hash),
                      hash,
                      SSL_BUFFER_BASE(&esniContents),
                      SSL_BUFFER_LEN(&esniContents));
    ;
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = tls13_HkdfExpandLabel(Zx, suite->prf_hash,
                               hash, hashSize,
                               kHkdfPurposeEsniKey, strlen(kHkdfPurposeEsniKey),
                               ssl3_Alg2Mech(cipherDef->calg),
                               keySize,
                               &keyMat->key);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = tls13_HkdfExpandLabelRaw(Zx, suite->prf_hash,
                                  hash, hashSize,
                                  kHkdfPurposeEsniIv, strlen(kHkdfPurposeEsniIv),
                                  keyMat->iv, ivSize);
    if (rv != SECSuccess) {
        goto loser;
    }

    ret = SECSuccess;

loser:
    PK11_FreeSymKey(Z);
    PK11_FreeSymKey(Zx);
    return ret;
}

/* Set up ESNI. This generates a private key as a side effect. */
SECStatus
tls13_ClientSetupESNI(sslSocket *ss)
{
    ssl3CipherSuite suite;
    sslEphemeralKeyPair *keyPair;
    size_t i;
    PRCList *cur;
    SECStatus rv;
    TLS13KeyShareEntry *share;
    const sslNamedGroupDef *group = NULL;
    PRTime now = PR_Now() / PR_USEC_PER_SEC;

    PORT_Assert(!ss->xtnData.esniPrivateKey);

    if (!ss->esniKeys) {
        return SECSuccess;
    }

    if ((ss->esniKeys->notBefore > now) || (ss->esniKeys->notAfter < now)) {
        return SECSuccess;
    }

    /* If we're not sending SNI, don't send ESNI. */
    if (!ssl_ShouldSendSNIExtension(ss, ss->url)) {
        return SECSuccess;
    }

    /* Pick the group. */
    for (i = 0; i < SSL_NAMED_GROUP_COUNT; ++i) {
        for (cur = PR_NEXT_LINK(&ss->esniKeys->keyShares);
             cur != &ss->esniKeys->keyShares;
             cur = PR_NEXT_LINK(cur)) {
            if (!ss->namedGroupPreferences[i]) {
                continue;
            }
            share = (TLS13KeyShareEntry *)cur;
            if (share->group->name == ss->namedGroupPreferences[i]->name) {
                group = ss->namedGroupPreferences[i];
                break;
            }
        }
    }

    if (!group) {
        /* No compatible group. */
        return SECSuccess;
    }

    rv = ssl3_NegotiateCipherSuiteInner(ss, &ss->esniKeys->suites,
                                        SSL_LIBRARY_VERSION_TLS_1_3, &suite);
    if (rv != SECSuccess) {
        return SECSuccess;
    }

    rv = tls13_CreateKeyShare(ss, group, &keyPair);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    ss->xtnData.esniPrivateKey = keyPair;
    ss->xtnData.esniSuite = suite;
    ss->xtnData.peerEsniShare = share;

    return SECSuccess;
}

/*
 * struct {
 *     CipherSuite suite;
 *     KeyShareEntry key_share;
 *     opaque record_digest<0..2^16-1>;
 *     opaque encrypted_sni<0..2^16-1>;
 * } ClientEncryptedSNI;
 *
 * struct {
 *     ServerNameList sni;
 *     opaque zeros[ESNIKeys.padded_length - length(sni)];
 * } PaddedServerNameList;
 *
 * struct {
 *     uint8 nonce[16];
 *     PaddedServerNameList realSNI;
 * } ClientESNIInner;
 */
SECStatus
tls13_FormatEsniAADInput(sslBuffer *aadInput,
                         PRUint8 *keyShare, unsigned int keyShareLen)
{
    SECStatus rv;

    /* 8 bytes of 0 for the sequence number. */
    rv = sslBuffer_AppendNumber(aadInput, 0, 8);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    /* Key share. */
    PORT_Assert(keyShareLen > 0);
    rv = sslBuffer_Append(aadInput, keyShare, keyShareLen);
    if (rv != SECSuccess) {
        return SECFailure;
    }

    return SECSuccess;
}

static SECStatus
tls13_ServerGetEsniAEAD(const sslSocket *ss, PRUint64 suite,
                        const ssl3CipherSuiteDef **suiteDefp,
                        SSLAEADCipher *aeadp)
{
    SECStatus rv;
    const ssl3CipherSuiteDef *suiteDef;
    SSLAEADCipher aead;

    /* Check against the suite list for ESNI */
    PRBool csMatch = PR_FALSE;
    sslReader csrdr = SSL_READER(ss->esniKeys->suites.data,
                                 ss->esniKeys->suites.len);
    while (SSL_READER_REMAINING(&csrdr)) {
        PRUint64 asuite;

        rv = sslRead_ReadNumber(&csrdr, 2, &asuite);
        if (rv != SECSuccess) {
            return SECFailure;
        }
        if (asuite == suite) {
            csMatch = PR_TRUE;
            break;
        }
    }
    if (!csMatch) {
        return SECFailure;
    }

    suiteDef = ssl_LookupCipherSuiteDef(suite);
    PORT_Assert(suiteDef);
    if (!suiteDef) {
        return SECFailure;
    }
    aead = tls13_GetAead(ssl_GetBulkCipherDef(suiteDef));
    if (!aead) {
        return SECFailure;
    }

    *suiteDefp = suiteDef;
    *aeadp = aead;
    return SECSuccess;
}

SECStatus
tls13_ServerDecryptEsniXtn(const sslSocket *ss, const PRUint8 *in, unsigned int inLen,
                           PRUint8 *out, unsigned int *outLen, unsigned int maxLen)
{
    sslReader rdr = SSL_READER(in, inLen);
    PRUint64 suite;
    const ssl3CipherSuiteDef *suiteDef;
    SSLAEADCipher aead = NULL;
    TLSExtension *keyShareExtension;
    TLS13KeyShareEntry *entry = NULL;
    ssl3KeyMaterial keyMat = { NULL };

    sslBuffer aadInput = SSL_BUFFER_EMPTY;
    const PRUint8 *keyShareBuf;
    sslReadBuffer buf;
    unsigned int keyShareBufLen;
    PRUint8 hash[64];
    SECStatus rv;

    /* Read the cipher suite. */
    rv = sslRead_ReadNumber(&rdr, 2, &suite);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Find the AEAD */
    rv = tls13_ServerGetEsniAEAD(ss, suite, &suiteDef, &aead);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Note where the KeyShare starts. */
    keyShareBuf = SSL_READER_CURRENT(&rdr);
    rv = tls13_DecodeKeyShareEntry(&rdr, &entry);
    if (rv != SECSuccess) {
        goto loser;
    }
    keyShareBufLen = SSL_READER_CURRENT(&rdr) - keyShareBuf;
    if (!entry || entry->group->name != ss->esniKeys->privKey->group->name) {
        goto loser;
    }

    /* The hash of the ESNIKeys structure. */
    rv = sslRead_ReadVariable(&rdr, 2, &buf);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Check that the hash matches. */
    unsigned int hashLen = tls13_GetHashSizeForHash(suiteDef->prf_hash);
    PORT_Assert(hashLen <= sizeof(hash));
    rv = PK11_HashBuf(ssl3_HashTypeToOID(suiteDef->prf_hash),
                      hash,
                      ss->esniKeys->data.data, ss->esniKeys->data.len);
    if (rv != SECSuccess) {
        goto loser;
    }

    if (buf.len != hashLen) {
        /* This is malformed. */
        goto loser;
    }
    if (0 != NSS_SecureMemcmp(hash, buf.buf, hashLen)) {
        goto loser;
    }

    rv = tls13_ComputeESNIKeys(ss, entry,
                               ss->esniKeys->privKey->keys,
                               suiteDef,
                               hash, keyShareBuf, keyShareBufLen,
                               ((sslSocket *)ss)->ssl3.hs.client_random,
                               &keyMat);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Read the ciphertext. */
    rv = sslRead_ReadVariable(&rdr, 2, &buf);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* Check that this is empty. */
    if (SSL_READER_REMAINING(&rdr) > 0) {
        goto loser;
    }

    /* Find the key share extension. */
    keyShareExtension = ssl3_FindExtension(CONST_CAST(sslSocket, ss),
                                           ssl_tls13_key_share_xtn);
    if (!keyShareExtension) {
        goto loser;
    }
    rv = tls13_FormatEsniAADInput(&aadInput,
                                  keyShareExtension->data.data,
                                  keyShareExtension->data.len);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = aead(&keyMat, PR_TRUE /* Decrypt */,
              out, outLen, maxLen,
              buf.buf, buf.len,
              SSL_BUFFER_BASE(&aadInput),
              SSL_BUFFER_LEN(&aadInput));
    sslBuffer_Clear(&aadInput);
    if (rv != SECSuccess) {
        goto loser;
    }

    ssl_DestroyKeyMaterial(&keyMat);
    tls13_DestroyKeyShareEntry(entry);
    return SECSuccess;

loser:
    FATAL_ERROR(CONST_CAST(sslSocket, ss), SSL_ERROR_RX_MALFORMED_ESNI_EXTENSION, illegal_parameter);
    ssl_DestroyKeyMaterial(&keyMat); /* Safe because zeroed. */
    if (entry) {
        tls13_DestroyKeyShareEntry(entry);
    }
    return SECFailure;
}
