/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "nss.h"
#include "pk11pub.h"
#include "secerr.h"
#include "sslproto.h"
#include "sslexp.h"
#include "tls13hkdf.h"

#include "databuffer.h"
#include "gtest_utils.h"
#include "nss_scoped_ptrs.h"

namespace nss_test {

const uint8_t kKey1Data[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
    0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
    0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f};
const DataBuffer kKey1(kKey1Data, sizeof(kKey1Data));

// The same as key1 but with the first byte
// 0x01.
const uint8_t kKey2Data[] = {
    0x01, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
    0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
    0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f};
const DataBuffer kKey2(kKey2Data, sizeof(kKey2Data));

const char kLabelMasterSecret[] = "master secret";

const uint8_t kSessionHash[] = {
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb,
    0xfc, 0xfd, 0xfe, 0xff, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
    0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xd0, 0xd1, 0xd2, 0xd3,
    0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
    0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb,
    0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
    0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, 0xe0, 0xe1, 0xe2, 0xe3,
    0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb,
    0xfc, 0xfd, 0xfe, 0xff,
};

const size_t kHashLength[] = {
    0,  /* ssl_hash_none   */
    16, /* ssl_hash_md5    */
    20, /* ssl_hash_sha1   */
    28, /* ssl_hash_sha224 */
    32, /* ssl_hash_sha256 */
    48, /* ssl_hash_sha384 */
    64, /* ssl_hash_sha512 */
};

size_t GetHashLength(SSLHashType hash) {
  size_t i = static_cast<size_t>(hash);
  if (i < PR_ARRAY_SIZE(kHashLength)) {
    return kHashLength[i];
  }
  ADD_FAILURE() << "Unknown hash: " << hash;
  return 0;
}

CK_MECHANISM_TYPE GetHkdfMech(SSLHashType hash) {
  switch (hash) {
    case ssl_hash_sha256:
      return CKM_NSS_HKDF_SHA256;
    case ssl_hash_sha384:
      return CKM_NSS_HKDF_SHA384;
    default:
      ADD_FAILURE() << "Unknown hash: " << hash;
  }
  return CKM_INVALID_MECHANISM;
}

PRUint16 GetSomeCipherSuiteForHash(SSLHashType hash) {
  switch (hash) {
    case ssl_hash_sha256:
      return TLS_AES_128_GCM_SHA256;
    case ssl_hash_sha384:
      return TLS_AES_256_GCM_SHA384;
    default:
      ADD_FAILURE() << "Unknown hash: " << hash;
  }
  return 0;
}

const std::string kHashName[] = {"None",    "MD5",     "SHA-1",  "SHA-224",
                                 "SHA-256", "SHA-384", "SHA-512"};

static void ImportKey(ScopedPK11SymKey* to, const DataBuffer& key,
                      SSLHashType hash_type, PK11SlotInfo* slot) {
  ASSERT_LT(hash_type, sizeof(kHashLength));
  ASSERT_LE(kHashLength[hash_type], key.len());
  SECItem key_item = {siBuffer, const_cast<uint8_t*>(key.data()),
                      static_cast<unsigned int>(GetHashLength(hash_type))};

  PK11SymKey* inner =
      PK11_ImportSymKey(slot, CKM_SSL3_MASTER_KEY_DERIVE, PK11_OriginUnwrap,
                        CKA_DERIVE, &key_item, NULL);
  ASSERT_NE(nullptr, inner);
  to->reset(inner);
}

static void DumpData(const std::string& label, const uint8_t* buf, size_t len) {
  DataBuffer d(buf, len);

  std::cerr << label << ": " << d << std::endl;
}

void DumpKey(const std::string& label, ScopedPK11SymKey& key) {
  SECStatus rv = PK11_ExtractKeyValue(key.get());
  ASSERT_EQ(SECSuccess, rv);

  SECItem* key_data = PK11_GetKeyData(key.get());
  ASSERT_NE(nullptr, key_data);

  DumpData(label, key_data->data, key_data->len);
}

extern "C" {
extern char ssl_trace;
extern FILE* ssl_trace_iob;
}

class TlsHkdfTest : public ::testing::Test,
                    public ::testing::WithParamInterface<SSLHashType> {
 public:
  TlsHkdfTest()
      : k1_(), k2_(), hash_type_(GetParam()), slot_(PK11_GetInternalSlot()) {
    EXPECT_NE(nullptr, slot_);
    char* ev = getenv("SSLTRACE");
    if (ev && ev[0]) {
      ssl_trace = atoi(ev);
      ssl_trace_iob = stderr;
    }
  }

  void SetUp() {
    ImportKey(&k1_, kKey1, hash_type_, slot_.get());
    ImportKey(&k2_, kKey2, hash_type_, slot_.get());
  }

  void VerifyKey(const ScopedPK11SymKey& key, CK_MECHANISM_TYPE expected_mech,
                 const DataBuffer& expected_value) {
    EXPECT_EQ(expected_mech, PK11_GetMechanism(key.get()));

    SECStatus rv = PK11_ExtractKeyValue(key.get());
    ASSERT_EQ(SECSuccess, rv);

    SECItem* key_data = PK11_GetKeyData(key.get());
    ASSERT_NE(nullptr, key_data);

    EXPECT_EQ(expected_value.len(), key_data->len);
    EXPECT_EQ(
        0, memcmp(expected_value.data(), key_data->data, expected_value.len()));
  }

  void HkdfExtract(const ScopedPK11SymKey& ikmk1, const ScopedPK11SymKey& ikmk2,
                   SSLHashType base_hash, const DataBuffer& expected) {
    std::cerr << "Hash = " << kHashName[base_hash] << std::endl;

    PK11SymKey* prk = nullptr;
    SECStatus rv = tls13_HkdfExtract(ikmk1.get(), ikmk2.get(), base_hash, &prk);
    ASSERT_EQ(SECSuccess, rv);
    ScopedPK11SymKey prkk(prk);

    DumpKey("Output", prkk);
    VerifyKey(prkk, GetHkdfMech(base_hash), expected);

    // Now test the public wrapper.
    PRUint16 cs = GetSomeCipherSuiteForHash(base_hash);
    rv = SSL_HkdfExtract(SSL_LIBRARY_VERSION_TLS_1_3, cs, ikmk1.get(),
                         ikmk2.get(), &prk);
    ASSERT_EQ(SECSuccess, rv);
    ASSERT_NE(nullptr, prk);
    VerifyKey(ScopedPK11SymKey(prk), GetHkdfMech(base_hash), expected);
  }

  void HkdfExpandLabel(ScopedPK11SymKey* prk, SSLHashType base_hash,
                       const uint8_t* session_hash, size_t session_hash_len,
                       const char* label, size_t label_len,
                       const DataBuffer& expected) {
    std::cerr << "Hash = " << kHashName[base_hash] << std::endl;

    std::vector<uint8_t> output(expected.len());

    SECStatus rv = tls13_HkdfExpandLabelRaw(
        prk->get(), base_hash, session_hash, session_hash_len, label, label_len,
        ssl_variant_stream, &output[0], output.size());
    ASSERT_EQ(SECSuccess, rv);
    DumpData("Output", &output[0], output.size());
    EXPECT_EQ(0, memcmp(expected.data(), &output[0], expected.len()));

    // Verify that the public API produces the same result.
    PRUint16 cs = GetSomeCipherSuiteForHash(base_hash);
    PK11SymKey* secret;
    rv = SSL_HkdfExpandLabel(SSL_LIBRARY_VERSION_TLS_1_3, cs, prk->get(),
                             session_hash, session_hash_len, label, label_len,
                             &secret);
    EXPECT_EQ(SECSuccess, rv);
    ASSERT_NE(nullptr, prk);
    VerifyKey(ScopedPK11SymKey(secret), GetHkdfMech(base_hash), expected);

    // Verify that a key can be created with a different key type and size.
    rv = SSL_HkdfExpandLabelWithMech(
        SSL_LIBRARY_VERSION_TLS_1_3, cs, prk->get(), session_hash,
        session_hash_len, label, label_len, CKM_DES3_CBC_PAD, 24, &secret);
    EXPECT_EQ(SECSuccess, rv);
    ASSERT_NE(nullptr, prk);
    ScopedPK11SymKey with_mech(secret);
    EXPECT_EQ(static_cast<CK_MECHANISM_TYPE>(CKM_DES3_CBC_PAD),
              PK11_GetMechanism(with_mech.get()));
    // Just verify that the key is the right size.
    rv = PK11_ExtractKeyValue(with_mech.get());
    ASSERT_EQ(SECSuccess, rv);
    SECItem* key_data = PK11_GetKeyData(with_mech.get());
    ASSERT_NE(nullptr, key_data);
    EXPECT_EQ(24U, key_data->len);
  }

 protected:
  ScopedPK11SymKey k1_;
  ScopedPK11SymKey k2_;
  SSLHashType hash_type_;

 private:
  ScopedPK11SlotInfo slot_;
};

TEST_P(TlsHkdfTest, HkdfNullNull) {
  const uint8_t tv[][48] = {
      {/* ssl_hash_none   */},
      {/* ssl_hash_md5    */},
      {/* ssl_hash_sha1   */},
      {/* ssl_hash_sha224 */},
      {0x33, 0xad, 0x0a, 0x1c, 0x60, 0x7e, 0xc0, 0x3b, 0x09, 0xe6, 0xcd,
       0x98, 0x93, 0x68, 0x0c, 0xe2, 0x10, 0xad, 0xf3, 0x00, 0xaa, 0x1f,
       0x26, 0x60, 0xe1, 0xb2, 0x2e, 0x10, 0xf1, 0x70, 0xf9, 0x2a},
      {0x7e, 0xe8, 0x20, 0x6f, 0x55, 0x70, 0x02, 0x3e, 0x6d, 0xc7, 0x51, 0x9e,
       0xb1, 0x07, 0x3b, 0xc4, 0xe7, 0x91, 0xad, 0x37, 0xb5, 0xc3, 0x82, 0xaa,
       0x10, 0xba, 0x18, 0xe2, 0x35, 0x7e, 0x71, 0x69, 0x71, 0xf9, 0x36, 0x2f,
       0x2c, 0x2f, 0xe2, 0xa7, 0x6b, 0xfd, 0x78, 0xdf, 0xec, 0x4e, 0xa9, 0xb5}};

  const DataBuffer expected_data(tv[hash_type_], GetHashLength(hash_type_));
  HkdfExtract(nullptr, nullptr, hash_type_, expected_data);
}

TEST_P(TlsHkdfTest, HkdfKey1Only) {
  const uint8_t tv[][48] = {
      {/* ssl_hash_none   */},
      {/* ssl_hash_md5    */},
      {/* ssl_hash_sha1   */},
      {/* ssl_hash_sha224 */},
      {0x41, 0x6c, 0x53, 0x92, 0xb9, 0xf3, 0x6d, 0xf1, 0x88, 0xe9, 0x0e,
       0xb1, 0x4d, 0x17, 0xbf, 0x0d, 0xa1, 0x90, 0xbf, 0xdb, 0x7f, 0x1f,
       0x49, 0x56, 0xe6, 0xe5, 0x66, 0xa5, 0x69, 0xc8, 0xb1, 0x5c},
      {0x51, 0xb1, 0xd5, 0xb4, 0x59, 0x79, 0x79, 0x08, 0x4a, 0x15, 0xb2, 0xdb,
       0x84, 0xd3, 0xd6, 0xbc, 0xfc, 0x93, 0x45, 0xd9, 0xdc, 0x74, 0xda, 0x1a,
       0x57, 0xc2, 0x76, 0x9f, 0x3f, 0x83, 0x45, 0x2f, 0xf6, 0xf3, 0x56, 0x1f,
       0x58, 0x63, 0xdb, 0x88, 0xda, 0x40, 0xce, 0x63, 0x7d, 0x24, 0x37, 0xf3}};

  const DataBuffer expected_data(tv[hash_type_], GetHashLength(hash_type_));
  HkdfExtract(k1_, nullptr, hash_type_, expected_data);
}

TEST_P(TlsHkdfTest, HkdfKey2Only) {
  const uint8_t tv[][48] = {
      {/* ssl_hash_none   */},
      {/* ssl_hash_md5    */},
      {/* ssl_hash_sha1   */},
      {/* ssl_hash_sha224 */},
      {0x16, 0xaf, 0x00, 0x54, 0x3a, 0x56, 0xc8, 0x26, 0xa2, 0xa7, 0xfc,
       0xb6, 0x34, 0x66, 0x8a, 0xfd, 0x36, 0xdc, 0x8e, 0xce, 0xc4, 0xd2,
       0x6c, 0x7a, 0xdc, 0xe3, 0x70, 0x36, 0x3d, 0x60, 0xfa, 0x0b},
      {0x7b, 0x40, 0xf9, 0xef, 0x91, 0xff, 0xc9, 0xd1, 0x29, 0x24, 0x5c, 0xbf,
       0xf8, 0x82, 0x76, 0x68, 0xae, 0x4b, 0x63, 0xe8, 0x03, 0xdd, 0x39, 0xa8,
       0xd4, 0x6a, 0xf6, 0xe5, 0xec, 0xea, 0xf8, 0x7d, 0x91, 0x71, 0x81, 0xf1,
       0xdb, 0x3b, 0xaf, 0xbf, 0xde, 0x71, 0x61, 0x15, 0xeb, 0xb5, 0x5f, 0x68}};

  const DataBuffer expected_data(tv[hash_type_], GetHashLength(hash_type_));
  HkdfExtract(nullptr, k2_, hash_type_, expected_data);
}

TEST_P(TlsHkdfTest, HkdfKey1Key2) {
  const uint8_t tv[][48] = {
      {/* ssl_hash_none   */},
      {/* ssl_hash_md5    */},
      {/* ssl_hash_sha1   */},
      {/* ssl_hash_sha224 */},
      {0xa5, 0x68, 0x02, 0x5a, 0x95, 0xc9, 0x7f, 0x55, 0x38, 0xbc, 0xf7,
       0x97, 0xcc, 0x0f, 0xd5, 0xf6, 0xa8, 0x8d, 0x15, 0xbc, 0x0e, 0x85,
       0x74, 0x70, 0x3c, 0xa3, 0x65, 0xbd, 0x76, 0xcf, 0x9f, 0xd3},
      {0x01, 0x93, 0xc0, 0x07, 0x3f, 0x6a, 0x83, 0x0e, 0x2e, 0x4f, 0xb2, 0x58,
       0xe4, 0x00, 0x08, 0x5c, 0x68, 0x9c, 0x37, 0x32, 0x00, 0x37, 0xff, 0xc3,
       0x1c, 0x5b, 0x98, 0x0b, 0x02, 0x92, 0x3f, 0xfd, 0x73, 0x5a, 0x6f, 0x2a,
       0x95, 0xa3, 0xee, 0xf6, 0xd6, 0x8e, 0x6f, 0x86, 0xea, 0x63, 0xf8, 0x33}};

  const DataBuffer expected_data(tv[hash_type_], GetHashLength(hash_type_));
  HkdfExtract(k1_, k2_, hash_type_, expected_data);
}

TEST_P(TlsHkdfTest, HkdfExpandLabel) {
  const uint8_t tv[][48] = {
      {/* ssl_hash_none   */},
      {/* ssl_hash_md5    */},
      {/* ssl_hash_sha1   */},
      {/* ssl_hash_sha224 */},
      {0x3e, 0x4e, 0x6e, 0xd0, 0xbc, 0xc4, 0xf4, 0xff, 0xf0, 0xf5, 0x69,
       0xd0, 0x6c, 0x1e, 0x0e, 0x10, 0x32, 0xaa, 0xd7, 0xa3, 0xef, 0xf6,
       0xa8, 0x65, 0x8e, 0xbe, 0xee, 0xc7, 0x1f, 0x01, 0x6d, 0x3c},
      {0x41, 0xea, 0x77, 0x09, 0x8c, 0x90, 0x04, 0x10, 0xec, 0xbc, 0x37, 0xd8,
       0x5b, 0x54, 0xcd, 0x7b, 0x08, 0x15, 0x13, 0x20, 0xed, 0x1e, 0x3f, 0x54,
       0x74, 0xf7, 0x8b, 0x06, 0x38, 0x28, 0x06, 0x37, 0x75, 0x23, 0xa2, 0xb7,
       0x34, 0xb1, 0x72, 0x2e, 0x59, 0x6d, 0x5a, 0x31, 0xf5, 0x53, 0xab, 0x99}};

  const DataBuffer expected_data(tv[hash_type_], GetHashLength(hash_type_));
  HkdfExpandLabel(&k1_, hash_type_, kSessionHash, GetHashLength(hash_type_),
                  kLabelMasterSecret, strlen(kLabelMasterSecret),
                  expected_data);
}

TEST_P(TlsHkdfTest, HkdfExpandLabelNoHash) {
  const uint8_t tv[][48] = {
      {/* ssl_hash_none   */},
      {/* ssl_hash_md5    */},
      {/* ssl_hash_sha1   */},
      {/* ssl_hash_sha224 */},
      {0xb7, 0x08, 0x00, 0xe3, 0x8e, 0x48, 0x68, 0x91, 0xb1, 0x0f, 0x5e,
       0x6f, 0x22, 0x53, 0x6b, 0x84, 0x69, 0x75, 0xaa, 0xa3, 0x2a, 0xe7,
       0xde, 0xaa, 0xc3, 0xd1, 0xb4, 0x05, 0x22, 0x5c, 0x68, 0xf5},
      {0x13, 0xd3, 0x36, 0x9f, 0x3c, 0x78, 0xa0, 0x32, 0x40, 0xee, 0x16, 0xe9,
       0x11, 0x12, 0x66, 0xc7, 0x51, 0xad, 0xd8, 0x3c, 0xa1, 0xa3, 0x97, 0x74,
       0xd7, 0x45, 0xff, 0xa7, 0x88, 0x9e, 0x52, 0x17, 0x2e, 0xaa, 0x3a, 0xd2,
       0x35, 0xd8, 0xd5, 0x35, 0xfd, 0x65, 0x70, 0x9f, 0xa9, 0xf9, 0xfa, 0x23}};

  const DataBuffer expected_data(tv[hash_type_], GetHashLength(hash_type_));
  HkdfExpandLabel(&k1_, hash_type_, nullptr, 0, kLabelMasterSecret,
                  strlen(kLabelMasterSecret), expected_data);
}

TEST_P(TlsHkdfTest, BadExtractWrapperInput) {
  PK11SymKey* key = nullptr;

  // Bad version.
  EXPECT_EQ(SECFailure,
            SSL_HkdfExtract(SSL_LIBRARY_VERSION_TLS_1_2, TLS_AES_128_GCM_SHA256,
                            k1_.get(), k2_.get(), &key));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());

  // Bad ciphersuite.
  EXPECT_EQ(SECFailure,
            SSL_HkdfExtract(SSL_LIBRARY_VERSION_TLS_1_3, TLS_RSA_WITH_NULL_SHA,
                            k1_.get(), k2_.get(), &key));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());

  // Old ciphersuite.
  EXPECT_EQ(SECFailure, SSL_HkdfExtract(SSL_LIBRARY_VERSION_TLS_1_3,
                                        TLS_RSA_WITH_AES_128_CBC_SHA, k1_.get(),
                                        k2_.get(), &key));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());

  // NULL outparam..
  EXPECT_EQ(SECFailure, SSL_HkdfExtract(SSL_LIBRARY_VERSION_TLS_1_3,
                                        TLS_RSA_WITH_AES_128_CBC_SHA, k1_.get(),
                                        k2_.get(), nullptr));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());

  EXPECT_EQ(nullptr, key);
}

TEST_P(TlsHkdfTest, BadExpandLabelWrapperInput) {
  PK11SymKey* key = nullptr;
  static const char* kLabel = "label";

  // Bad version.
  EXPECT_EQ(
      SECFailure,
      SSL_HkdfExpandLabel(SSL_LIBRARY_VERSION_TLS_1_2, TLS_AES_128_GCM_SHA256,
                          k1_.get(), nullptr, 0, kLabel, strlen(kLabel), &key));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());

  // Bad ciphersuite.
  EXPECT_EQ(
      SECFailure,
      SSL_HkdfExpandLabel(SSL_LIBRARY_VERSION_TLS_1_3, TLS_RSA_WITH_NULL_MD5,
                          k1_.get(), nullptr, 0, kLabel, strlen(kLabel), &key));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());

  // Old ciphersuite.
  EXPECT_EQ(SECFailure,
            SSL_HkdfExpandLabel(SSL_LIBRARY_VERSION_TLS_1_3,
                                TLS_RSA_WITH_AES_128_CBC_SHA, k1_.get(),
                                nullptr, 0, kLabel, strlen(kLabel), &key));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());

  // Null PRK.
  EXPECT_EQ(SECFailure, SSL_HkdfExpandLabel(
                            SSL_LIBRARY_VERSION_TLS_1_2, TLS_AES_128_GCM_SHA256,
                            nullptr, nullptr, 0, kLabel, strlen(kLabel), &key));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());

  // Null, non-zero-length handshake hash.
  EXPECT_EQ(
      SECFailure,
      SSL_HkdfExpandLabel(SSL_LIBRARY_VERSION_TLS_1_2, TLS_AES_128_GCM_SHA256,
                          k1_.get(), nullptr, 2, kLabel, strlen(kLabel), &key));

  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
  // Null, non-zero-length label.
  EXPECT_EQ(SECFailure,
            SSL_HkdfExpandLabel(SSL_LIBRARY_VERSION_TLS_1_3,
                                TLS_AES_128_GCM_SHA256, k1_.get(), nullptr, 0,
                                nullptr, strlen(kLabel), &key));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());

  // Null, empty label.
  EXPECT_EQ(SECFailure, SSL_HkdfExpandLabel(SSL_LIBRARY_VERSION_TLS_1_3,
                                            TLS_AES_128_GCM_SHA256, k1_.get(),
                                            nullptr, 0, nullptr, 0, &key));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());

  // Null key pointer..
  EXPECT_EQ(SECFailure,
            SSL_HkdfExpandLabel(SSL_LIBRARY_VERSION_TLS_1_3,
                                TLS_AES_128_GCM_SHA256, k1_.get(), nullptr, 0,
                                kLabel, strlen(kLabel), nullptr));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());

  EXPECT_EQ(nullptr, key);
}

static const SSLHashType kHashTypes[] = {ssl_hash_sha256, ssl_hash_sha384};
INSTANTIATE_TEST_CASE_P(AllHashFuncs, TlsHkdfTest,
                        ::testing::ValuesIn(kHashTypes));

}  // namespace nss_test
