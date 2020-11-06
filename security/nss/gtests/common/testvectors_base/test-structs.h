/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This file is generated from sources in nss/gtests/common/wycheproof
 * automatically and should not be touched manually.
 * Generation is trigged by calling ./mach wycheproof */

#ifndef test_structs_h__
#define test_structs_h__

#include <string>
#include <vector>
#include "secoidt.h"
#include "pkcs11t.h"

typedef struct AesCbcTestVectorStr {
  uint32_t id;
  std::string key;
  std::string msg;
  std::string iv;
  std::string ciphertext;
  bool valid;
} AesCbcTestVector;

typedef struct AesCmacTestVectorStr {
  uint32_t id;
  std::string comment;
  std::string key;
  std::string msg;
  std::string tag;
  bool invalid;
} AesCmacTestVector;
typedef AesCmacTestVector HmacTestVector;

typedef struct AesGcmKatValueStr {
  uint32_t id;
  std::string key;
  std::string plaintext;
  std::string additional_data;
  std::string iv;
  std::string hash_key;
  std::string ghash;
  std::string result;
  bool invalid_ct;
  bool invalid_iv;
} AesGcmKatValue;

typedef struct ChaChaTestVectorStr {
  uint32_t id;
  std::vector<uint8_t> plaintext;
  std::vector<uint8_t> aad;
  std::vector<uint8_t> key;
  std::vector<uint8_t> iv;
  std::vector<uint8_t> ciphertext;
  bool invalid_tag;
  bool invalid_iv;
} ChaChaTestVector;

typedef struct EcdsaTestVectorStr {
  SECOidTag hash_oid;
  uint32_t id;
  std::vector<uint8_t> sig;
  std::vector<uint8_t> public_key;
  std::vector<uint8_t> msg;
  bool valid;
} EcdsaTestVector;

typedef EcdsaTestVector DsaTestVector;

typedef struct EcdhTestVectorStr {
  uint32_t id;
  std::vector<uint8_t> private_key;
  std::vector<uint8_t> public_key;
  std::vector<uint8_t> secret;
  bool invalid_asn;
  bool valid;
} EcdhTestVector;

typedef struct HkdfTestVectorStr {
  uint32_t id;
  std::string ikm;
  std::string salt;
  std::string info;
  std::string okm;
  uint32_t size;
  bool valid;
} HkdfTestVector;

typedef struct RsaSignatureTestVectorStr {
  SECOidTag hash_oid;
  uint32_t id;
  std::vector<uint8_t> sig;
  std::vector<uint8_t> public_key;
  std::vector<uint8_t> msg;
  bool valid;
} RsaSignatureTestVector;

typedef struct RsaDecryptTestVectorStr {
  uint32_t id;
  std::vector<uint8_t> msg;
  std::vector<uint8_t> ct;
  std::vector<uint8_t> priv_key;
  bool valid;
} RsaDecryptTestVector;

typedef struct RsaOaepTestVectorStr {
  SECOidTag hash_oid;
  CK_RSA_PKCS_MGF_TYPE mgf_hash;
  uint32_t id;
  std::vector<uint8_t> msg;
  std::vector<uint8_t> ct;
  std::vector<uint8_t> label;
  std::vector<uint8_t> priv_key;
  bool valid;
} RsaOaepTestVector;

typedef struct RsaPssTestVectorStr {
  SECOidTag hash_oid;
  CK_RSA_PKCS_MGF_TYPE mgf_hash;
  uint32_t id;
  unsigned long sLen;
  std::vector<uint8_t> sig;
  std::vector<uint8_t> public_key;
  std::vector<uint8_t> msg;
  bool valid;
} RsaPssTestVector;

#endif  // test_structs_h__
