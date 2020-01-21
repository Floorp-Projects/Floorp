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

typedef struct EcdhTestVectorStr {
  uint32_t id;
  std::vector<uint8_t> private_key;
  std::vector<uint8_t> public_key;
  std::vector<uint8_t> secret;
  bool invalid_asn;
  bool valid;
} EcdhTestVector;

#endif  // test_structs_h__
