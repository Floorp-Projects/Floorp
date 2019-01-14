/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <keyhi.h>
#include <keythi.h>
#include <pk11pub.h>
#include <prerror.h>

#include "encrypt.h"
#include "prio/rand.h"
#include "prio/util.h"

// Use curve25519
#define CURVE_OID_TAG SEC_OID_CURVE25519

// Use 96-bit IV
#define GCM_IV_LEN_BYTES 12
// Use 128-bit auth tag
#define GCM_TAG_LEN_BYTES 16

#define PRIO_TAG "PrioPacket"
#define AAD_LEN (strlen(PRIO_TAG) + CURVE25519_KEY_LEN + GCM_IV_LEN_BYTES)

// For an example of NSS curve25519 import/export code, see:
// https://searchfox.org/nss/rev/cfd5fcba7efbfe116e2c08848075240ec3a92718/gtests/pk11_gtest/pk11_curve25519_unittest.cc#66

// The all-zeros curve25519 public key, as DER-encoded SPKI blob.
static const uint8_t curve25519_spki_zeros[] = {
  0x30, 0x39, 0x30, 0x14, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02,
  0x01, 0x06, 0x09, 0x2b, 0x06, 0x01, 0x04, 0x01, 0xda, 0x47, 0x0f, 0x01,
  0x03, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// The all-zeros curve25519 private key, as a PKCS#8 blob.
static const uint8_t curve25519_priv_zeros[] = {
  0x30, 0x67, 0x02, 0x01, 0x00, 0x30, 0x14, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce,
  0x3d, 0x02, 0x01, 0x06, 0x09, 0x2b, 0x06, 0x01, 0x04, 0x01, 0xda, 0x47, 0x0f,
  0x01, 0x04, 0x4c, 0x30, 0x4a, 0x02, 0x01, 0x01, 0x04, 0x20,

  /* Byte index 36:  32 bytes of curve25519 private key. */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

  /* misc type fields */
  0xa1, 0x23, 0x03, 0x21,

  /* Byte index 73:  32 bytes of curve25519 public key. */
  0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Index into `curve25519_priv_zeros` at which the private key begins.
static const size_t curve25519_priv_sk_offset = 36;
// Index into `curve25519_priv_zeros` at which the public key begins.
static const size_t curve25519_priv_pk_offset = 73;

static SECStatus key_from_hex(
  unsigned char key_out[CURVE25519_KEY_LEN],
  const unsigned char hex_in[CURVE25519_KEY_LEN_HEX]);

// Note that we do not use isxdigit because it is locale-dependent
// See: https://github.com/mozilla/libprio/issues/20
static inline char
is_hex_digit(char c)
{
  return ('0' <= c && c <= '9') || ('a' <= c && c <= 'f') ||
         ('A' <= c && c <= 'F');
}

// Note that we do not use toupper because it is locale-dependent
// See: https://github.com/mozilla/libprio/issues/20
static inline char
to_upper(char c)
{
  if (c >= 'a' && c <= 'z') {
    return c - 0x20;
  } else {
    return c;
  }
}

static inline uint8_t
hex_to_int(char h)
{
  return (h > '9') ? to_upper(h) - 'A' + 10 : (h - '0');
}

static inline unsigned char
int_to_hex(uint8_t i)
{
  return (i > 0x09) ? ((i - 10) + 'A') : i + '0';
}

static SECStatus
derive_dh_secret(PK11SymKey** shared_secret, PrivateKey priv, PublicKey pub)
{
  if (priv == NULL)
    return SECFailure;
  if (pub == NULL)
    return SECFailure;
  if (shared_secret == NULL)
    return SECFailure;

  SECStatus rv = SECSuccess;
  *shared_secret = NULL;

  P_CHECKA(*shared_secret = PK11_PubDeriveWithKDF(
             priv, pub, PR_FALSE, NULL, NULL, CKM_ECDH1_DERIVE, CKM_AES_GCM,
             CKA_ENCRYPT | CKA_DECRYPT, 16, CKD_SHA256_KDF, NULL, NULL));

cleanup:
  return rv;
}

SECStatus
PublicKey_import(PublicKey* pk, const unsigned char* data, unsigned int dataLen)
{
  SECStatus rv = SECSuccess;
  CERTSubjectPublicKeyInfo* pkinfo = NULL;
  *pk = NULL;
  unsigned char* key_bytes = NULL;
  uint8_t* spki_data = NULL;

  if (dataLen != CURVE25519_KEY_LEN)
    return SECFailure;

  P_CHECKA(key_bytes = calloc(dataLen, sizeof(unsigned char)));
  memcpy(key_bytes, data, dataLen);

  const int spki_len = sizeof(curve25519_spki_zeros);
  P_CHECKA(spki_data = calloc(spki_len, sizeof(uint8_t)));

  memcpy(spki_data, curve25519_spki_zeros, spki_len);
  SECItem spki_item = { siBuffer, spki_data, spki_len };

  // Import the all-zeros curve25519 public key.
  P_CHECKA(pkinfo = SECKEY_DecodeDERSubjectPublicKeyInfo(&spki_item));
  P_CHECKA(*pk = SECKEY_ExtractPublicKey(pkinfo));

  // Overwrite the all-zeros public key with the 32-byte curve25519 public key
  // given as input.
  memcpy((*pk)->u.ec.publicValue.data, data, CURVE25519_KEY_LEN);

cleanup:
  if (key_bytes)
    free(key_bytes);
  if (spki_data)
    free(spki_data);
  if (pkinfo)
    SECKEY_DestroySubjectPublicKeyInfo(pkinfo);

  if (rv != SECSuccess)
    PublicKey_clear(*pk);
  return rv;
}

SECStatus
PrivateKey_import(PrivateKey* sk, const unsigned char* sk_data,
                  unsigned int sk_data_len, const unsigned char* pk_data,
                  unsigned int pk_data_len)
{
  if (sk_data_len != CURVE25519_KEY_LEN || !sk_data) {
    return SECFailure;
  }

  if (pk_data_len != CURVE25519_KEY_LEN || !pk_data) {
    return SECFailure;
  }

  SECStatus rv = SECSuccess;
  PK11SlotInfo* slot = NULL;
  uint8_t* zero_priv_data = NULL;
  *sk = NULL;
  const int zero_priv_len = sizeof(curve25519_priv_zeros);

  P_CHECKA(slot = PK11_GetInternalSlot());

  P_CHECKA(zero_priv_data = calloc(zero_priv_len, sizeof(uint8_t)));
  SECItem zero_priv_item = { siBuffer, zero_priv_data, zero_priv_len };

  // Copy the PKCS#8-encoded keypair into writable buffer.
  memcpy(zero_priv_data, curve25519_priv_zeros, zero_priv_len);
  // Copy private key into bytes beginning at index `curve25519_priv_sk_offset`.
  memcpy(zero_priv_data + curve25519_priv_sk_offset, sk_data, sk_data_len);
  // Copy private key into bytes beginning at index `curve25519_priv_pk_offset`.
  memcpy(zero_priv_data + curve25519_priv_pk_offset, pk_data, pk_data_len);

  P_CHECKC(PK11_ImportDERPrivateKeyInfoAndReturnKey(
    slot, &zero_priv_item, NULL, NULL, PR_FALSE, PR_FALSE, KU_ALL, sk, NULL));

cleanup:
  if (slot) {
    PK11_FreeSlot(slot);
  }
  if (zero_priv_data) {
    free(zero_priv_data);
  }
  if (rv != SECSuccess) {
    PrivateKey_clear(*sk);
  }
  return rv;
}

SECStatus
PublicKey_import_hex(PublicKey* pk, const unsigned char* hexData,
                     unsigned int dataLen)
{
  unsigned char raw_bytes[CURVE25519_KEY_LEN];

  if (dataLen != CURVE25519_KEY_LEN_HEX || !hexData) {
    return SECFailure;
  }

  if (key_from_hex(raw_bytes, hexData) != SECSuccess) {
    return SECFailure;
  }

  return PublicKey_import(pk, raw_bytes, CURVE25519_KEY_LEN);
}

SECStatus
PrivateKey_import_hex(PrivateKey* sk, const unsigned char* privHexData,
                      unsigned int privDataLen, const unsigned char* pubHexData,
                      unsigned int pubDataLen)
{
  SECStatus rv = SECSuccess;
  unsigned char raw_priv[CURVE25519_KEY_LEN];
  unsigned char raw_pub[CURVE25519_KEY_LEN];

  if (privDataLen != CURVE25519_KEY_LEN_HEX ||
      pubDataLen != CURVE25519_KEY_LEN_HEX) {
    return SECFailure;
  }

  if (!privHexData || !pubHexData) {
    return SECFailure;
  }

  P_CHECK(key_from_hex(raw_priv, privHexData));
  P_CHECK(key_from_hex(raw_pub, pubHexData));

  return PrivateKey_import(sk, raw_priv, CURVE25519_KEY_LEN, raw_pub,
                           CURVE25519_KEY_LEN);
}

SECStatus
PublicKey_export(const_PublicKey pk, unsigned char* data, unsigned int dataLen)
{
  if (pk == NULL || dataLen != CURVE25519_KEY_LEN) {
    return SECFailure;
  }

  const SECItem* key = &pk->u.ec.publicValue;
  if (key->len != CURVE25519_KEY_LEN) {
    return SECFailure;
  }

  memcpy(data, key->data, key->len);
  return SECSuccess;
}

SECStatus
PrivateKey_export(PrivateKey sk, unsigned char* data, unsigned int dataLen)
{
  if (sk == NULL || dataLen != CURVE25519_KEY_LEN) {
    return SECFailure;
  }

  SECStatus rv = SECSuccess;
  SECItem item = { siBuffer, NULL, 0 };

  P_CHECKC(PK11_ReadRawAttribute(PK11_TypePrivKey, sk, CKA_VALUE, &item));

  // If the leading bytes of the key are '\0', then this string can be
  // shorter than `CURVE25519_KEY_LEN` bytes.
  memset(data, 0, CURVE25519_KEY_LEN);
  P_CHECKCB(item.len <= CURVE25519_KEY_LEN);

  // Copy into the low-order bytes of the output.
  const size_t leading_zeros = CURVE25519_KEY_LEN - item.len;
  memcpy(data + leading_zeros, item.data, item.len);

cleanup:
  if (item.data != NULL) {
    SECITEM_ZfreeItem(&item, PR_FALSE);
  }

  return rv;
}

static void
key_to_hex(const unsigned char key_in[CURVE25519_KEY_LEN],
           unsigned char hex_out[(2 * CURVE25519_KEY_LEN) + 1])
{
  const unsigned char* p = key_in;
  for (unsigned int i = 0; i < CURVE25519_KEY_LEN; i++) {
    unsigned char bytel = p[0] & 0x0f;
    unsigned char byteu = (p[0] & 0xf0) >> 4;
    hex_out[2 * i] = int_to_hex(byteu);
    hex_out[2 * i + 1] = int_to_hex(bytel);
    p++;
  }

  hex_out[2 * CURVE25519_KEY_LEN] = '\0';
}

static SECStatus
key_from_hex(unsigned char key_out[CURVE25519_KEY_LEN],
             const unsigned char hex_in[CURVE25519_KEY_LEN_HEX])
{
  for (unsigned int i = 0; i < CURVE25519_KEY_LEN_HEX; i++) {
    if (!is_hex_digit(hex_in[i]))
      return SECFailure;
  }

  const unsigned char* p = hex_in;
  for (unsigned int i = 0; i < CURVE25519_KEY_LEN; i++) {
    uint8_t d0 = hex_to_int(p[0]);
    uint8_t d1 = hex_to_int(p[1]);
    key_out[i] = (d0 << 4) | d1;
    p += 2;
  }

  return SECSuccess;
}

SECStatus
PublicKey_export_hex(const_PublicKey pk, unsigned char* data,
                     unsigned int dataLen)
{
  if (dataLen != CURVE25519_KEY_LEN_HEX + 1) {
    return SECFailure;
  }

  unsigned char raw_data[CURVE25519_KEY_LEN];
  if (PublicKey_export(pk, raw_data, sizeof(raw_data)) != SECSuccess) {
    return SECFailure;
  }

  key_to_hex(raw_data, data);
  return SECSuccess;
}

SECStatus
PrivateKey_export_hex(PrivateKey sk, unsigned char* data, unsigned int dataLen)
{
  if (dataLen != CURVE25519_KEY_LEN_HEX + 1) {
    return SECFailure;
  }

  unsigned char raw_data[CURVE25519_KEY_LEN];
  if (PrivateKey_export(sk, raw_data, sizeof(raw_data)) != SECSuccess) {
    return SECFailure;
  }

  key_to_hex(raw_data, data);
  return SECSuccess;
}

SECStatus
Keypair_new(PrivateKey* pvtkey, PublicKey* pubkey)
{
  if (pvtkey == NULL)
    return SECFailure;
  if (pubkey == NULL)
    return SECFailure;

  SECStatus rv = SECSuccess;
  SECOidData* oid_data = NULL;
  *pubkey = NULL;
  *pvtkey = NULL;

  SECKEYECParams ecp;
  ecp.data = NULL;
  PK11SlotInfo* slot = NULL;

  P_CHECKA(oid_data = SECOID_FindOIDByTag(CURVE_OID_TAG));
  const int oid_struct_len = 2 + oid_data->oid.len;

  P_CHECKA(ecp.data = malloc(oid_struct_len));
  ecp.len = oid_struct_len;

  ecp.type = siDEROID;

  ecp.data[0] = SEC_ASN1_OBJECT_ID;
  ecp.data[1] = oid_data->oid.len;
  memcpy(&ecp.data[2], oid_data->oid.data, oid_data->oid.len);

  P_CHECKA(slot = PK11_GetInternalSlot());
  P_CHECKA(*pvtkey = PK11_GenerateKeyPair(slot, CKM_EC_KEY_PAIR_GEN, &ecp,
                                          (SECKEYPublicKey**)pubkey, PR_FALSE,
                                          PR_FALSE, NULL));
cleanup:
  if (slot) {
    PK11_FreeSlot(slot);
  }
  if (ecp.data) {
    free(ecp.data);
  }
  if (rv != SECSuccess) {
    PublicKey_clear(*pubkey);
    PrivateKey_clear(*pvtkey);
  }
  return rv;
}

void
PublicKey_clear(PublicKey pubkey)
{
  if (pubkey)
    SECKEY_DestroyPublicKey(pubkey);
}

void
PrivateKey_clear(PrivateKey pvtkey)
{
  if (pvtkey)
    SECKEY_DestroyPrivateKey(pvtkey);
}

const SECItem*
PublicKey_toBytes(const_PublicKey pubkey)
{
  return &pubkey->u.ec.publicValue;
}

SECStatus
PublicKey_encryptSize(unsigned int inputLen, unsigned int* outputLen)
{
  if (outputLen == NULL || inputLen >= MAX_ENCRYPT_LEN)
    return SECFailure;

  // public key, IV, tag, and input
  *outputLen =
    CURVE25519_KEY_LEN + GCM_IV_LEN_BYTES + GCM_TAG_LEN_BYTES + inputLen;
  return SECSuccess;
}

static void
set_gcm_params(SECItem* paramItem, CK_GCM_PARAMS* param, unsigned char* nonce,
               const_PublicKey pubkey, unsigned char* aadBuf)
{
  int offset = 0;
  memcpy(aadBuf, PRIO_TAG, strlen(PRIO_TAG));
  offset += strlen(PRIO_TAG);
  memcpy(aadBuf + offset, PublicKey_toBytes(pubkey)->data, CURVE25519_KEY_LEN);
  offset += CURVE25519_KEY_LEN;
  memcpy(aadBuf + offset, nonce, GCM_IV_LEN_BYTES);

  param->pIv = nonce;
  param->ulIvLen = GCM_IV_LEN_BYTES;
  param->pAAD = aadBuf;
  param->ulAADLen = AAD_LEN;
  param->ulTagBits = GCM_TAG_LEN_BYTES * 8;

  paramItem->type = siBuffer;
  paramItem->data = (void*)param;
  paramItem->len = sizeof(*param);
}

SECStatus
PublicKey_encrypt(PublicKey pubkey, unsigned char* output,
                  unsigned int* outputLen, unsigned int maxOutputLen,
                  const unsigned char* input, unsigned int inputLen)
{
  if (pubkey == NULL)
    return SECFailure;

  if (inputLen >= MAX_ENCRYPT_LEN)
    return SECFailure;

  unsigned int needLen;
  if (PublicKey_encryptSize(inputLen, &needLen) != SECSuccess)
    return SECFailure;

  if (maxOutputLen < needLen)
    return SECFailure;

  SECStatus rv = SECSuccess;
  PublicKey eph_pub = NULL;
  PrivateKey eph_priv = NULL;
  PK11SymKey* aes_key = NULL;

  unsigned char nonce[GCM_IV_LEN_BYTES];
  unsigned char aadBuf[AAD_LEN];
  P_CHECKC(rand_bytes(nonce, GCM_IV_LEN_BYTES));

  P_CHECKC(Keypair_new(&eph_priv, &eph_pub));
  P_CHECKC(derive_dh_secret(&aes_key, eph_priv, pubkey));

  CK_GCM_PARAMS param;
  SECItem paramItem;
  set_gcm_params(&paramItem, &param, nonce, eph_pub, aadBuf);

  const SECItem* pk = PublicKey_toBytes(eph_pub);
  P_CHECKCB(pk->len == CURVE25519_KEY_LEN);
  memcpy(output, pk->data, pk->len);
  memcpy(output + CURVE25519_KEY_LEN, param.pIv, param.ulIvLen);

  const int offset = CURVE25519_KEY_LEN + param.ulIvLen;
  P_CHECKC(PK11_Encrypt(aes_key, CKM_AES_GCM, &paramItem, output + offset,
                        outputLen, maxOutputLen - offset, input, inputLen));
  *outputLen = *outputLen + CURVE25519_KEY_LEN + GCM_IV_LEN_BYTES;

cleanup:
  PublicKey_clear(eph_pub);
  PrivateKey_clear(eph_priv);
  if (aes_key)
    PK11_FreeSymKey(aes_key);

  return rv;
}

SECStatus
PrivateKey_decrypt(PrivateKey privkey, unsigned char* output,
                   unsigned int* outputLen, unsigned int maxOutputLen,
                   const unsigned char* input, unsigned int inputLen)
{
  PK11SymKey* aes_key = NULL;
  PublicKey eph_pub = NULL;
  unsigned char aad_buf[AAD_LEN];

  if (privkey == NULL)
    return SECFailure;

  SECStatus rv = SECSuccess;
  unsigned int headerLen;
  if (PublicKey_encryptSize(0, &headerLen) != SECSuccess)
    return SECFailure;

  if (inputLen < headerLen)
    return SECFailure;

  const unsigned int msglen = inputLen - headerLen;
  if (maxOutputLen < msglen || msglen >= MAX_ENCRYPT_LEN)
    return SECFailure;

  P_CHECKC(PublicKey_import(&eph_pub, input, CURVE25519_KEY_LEN));
  unsigned char nonce[GCM_IV_LEN_BYTES];
  memcpy(nonce, input + CURVE25519_KEY_LEN, GCM_IV_LEN_BYTES);

  SECItem paramItem;
  CK_GCM_PARAMS param;
  set_gcm_params(&paramItem, &param, nonce, eph_pub, aad_buf);

  P_CHECKC(derive_dh_secret(&aes_key, privkey, eph_pub));

  const int offset = CURVE25519_KEY_LEN + GCM_IV_LEN_BYTES;
  P_CHECKC(PK11_Decrypt(aes_key, CKM_AES_GCM, &paramItem, output, outputLen,
                        maxOutputLen, input + offset, inputLen - offset));

cleanup:
  PublicKey_clear(eph_pub);
  if (aes_key)
    PK11_FreeSymKey(aes_key);
  return rv;
}
