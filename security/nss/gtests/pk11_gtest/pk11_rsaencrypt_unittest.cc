/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <cstdint>

#include "cpputil.h"
#include "cryptohi.h"
#include "json_reader.h"
#include "gtest/gtest.h"
#include "limits.h"
#include "nss.h"
#include "nss_scoped_ptrs.h"
#include "pk11pub.h"
#include "databuffer.h"

#include "testvectors/rsa_signature-vectors.h"
#include "testvectors/rsaencrypt_bb2048-vectors.h"
#include "testvectors/rsaencrypt_bb3072-vectors.h"

namespace nss_test {

class RsaDecryptWycheproofTest : public ::testing::Test {
 protected:
  void Run(const std::string& name) {
    WycheproofHeader(name, "RSAES-PKCS1-v1_5",
                     "rsaes_pkcs1_decrypt_schema.json",
                     [this](JsonReader& r) { RunGroup(r); });
  }

  void TestDecrypt(const RsaDecryptTestVector& vec) {
    SECItem pkcs8_item = {siBuffer, toUcharPtr(vec.priv_key.data()),
                          static_cast<unsigned int>(vec.priv_key.size())};

    ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
    EXPECT_NE(nullptr, slot);

    SECKEYPrivateKey* key = nullptr;
    SECStatus rv = PK11_ImportDERPrivateKeyInfoAndReturnKey(
        slot.get(), &pkcs8_item, nullptr, nullptr, false, false, KU_ALL, &key,
        nullptr);
    ASSERT_EQ(SECSuccess, rv);
    ASSERT_NE(nullptr, key);
    ScopedSECKEYPrivateKey priv_key(key);

    // Decrypt
    std::vector<uint8_t> decrypted(PR_MAX(1, vec.ct.size()));
    unsigned int decrypted_len = 0;
    rv = PK11_PrivDecryptPKCS1(priv_key.get(), decrypted.data(), &decrypted_len,
                               decrypted.size(), vec.ct.data(), vec.ct.size());

    decrypted.resize(decrypted_len);
    if (vec.valid) {
      ASSERT_EQ(SECSuccess, rv);
      EXPECT_EQ(vec.msg, decrypted);
    } else if (vec.invalid_padding) {
      // If the padding is bad, decryption should succeed and produce
      // (pseudo)random output.
      ASSERT_EQ(SECSuccess, rv);
      ASSERT_NE(vec.msg, decrypted);
    } else {
      ASSERT_EQ(SECFailure, rv)
          << "Returned:" << DataBuffer(decrypted.data(), decrypted.size());
    }
  };

 private:
  void RunGroup(JsonReader& r) {
    std::vector<RsaDecryptTestVector> tests;
    std::vector<uint8_t> private_key;
    while (r.NextItem()) {
      std::string n = r.ReadLabel();
      if (n == "") {
        break;
      }

      if (n == "d" || n == "e" || n == "keysize" || n == "n" ||
          n == "privateKeyJwk" || n == "privateKeyPem") {
        r.SkipValue();
      } else if (n == "privateKeyPkcs8") {
        private_key = r.ReadHex();
      } else if (n == "type") {
        ASSERT_EQ("RsaesPkcs1Decrypt", r.ReadString());
      } else if (n == "tests") {
        WycheproofReadTests(r, &tests, ReadTestAttr, false,
                            [](RsaDecryptTestVector& t, const std::string&,
                               const std::vector<std::string>& flags) {
                              t.invalid_padding =
                                  std::find(flags.begin(), flags.end(),
                                            "InvalidPkcs1Padding") !=
                                  flags.end();
                            });
      } else {
        FAIL() << "unknown label in group: " << n;
      }
    }

    for (auto& t : tests) {
      std::cout << "Running test " << t.id << std::endl;
      t.priv_key = private_key;
      TestDecrypt(t);
    }
  }

  static void ReadTestAttr(RsaDecryptTestVector& t, const std::string& n,
                           JsonReader& r) {
    if (n == "msg") {
      t.msg = r.ReadHex();
    } else if (n == "ct") {
      t.ct = r.ReadHex();
    } else {
      FAIL() << "unsupported test case field: " << n;
    }
  }
};

TEST_F(RsaDecryptWycheproofTest, Rsa2048) { Run("rsa_pkcs1_2048"); }
TEST_F(RsaDecryptWycheproofTest, Rsa3072) { Run("rsa_pkcs1_3072"); }
TEST_F(RsaDecryptWycheproofTest, Rsa4096) { Run("rsa_pkcs1_4096"); }

TEST_F(RsaDecryptWycheproofTest, Bb2048) {
  for (auto& t : kRsaBb2048Vectors) {
    RsaDecryptTestVector copy = t;
    copy.priv_key = kRsaBb2048;
    TestDecrypt(copy);
  }
}
TEST_F(RsaDecryptWycheproofTest, Bb2049) {
  for (auto& t : kRsaBb2049Vectors) {
    RsaDecryptTestVector copy = t;
    copy.priv_key = kRsaBb2049;
    TestDecrypt(copy);
  }
}
TEST_F(RsaDecryptWycheproofTest, Bb3072) {
  for (auto& t : kRsaBb3072Vectors) {
    RsaDecryptTestVector copy = t;
    copy.priv_key = kRsaBb3072;
    TestDecrypt(copy);
  }
}

TEST(RsaEncryptTest, MessageLengths) {
  const uint8_t spki[] = {
      0x30, 0x81, 0x9f, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7,
      0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x81, 0x8d, 0x00, 0x30, 0x81,
      0x89, 0x02, 0x81, 0x81, 0x00, 0xf8, 0xb8, 0x6c, 0x83, 0xb4, 0xbc, 0xd9,
      0xa8, 0x57, 0xc0, 0xa5, 0xb4, 0x59, 0x76, 0x8c, 0x54, 0x1d, 0x79, 0xeb,
      0x22, 0x52, 0x04, 0x7e, 0xd3, 0x37, 0xeb, 0x41, 0xfd, 0x83, 0xf9, 0xf0,
      0xa6, 0x85, 0x15, 0x34, 0x75, 0x71, 0x5a, 0x84, 0xa8, 0x3c, 0xd2, 0xef,
      0x5a, 0x4e, 0xd3, 0xde, 0x97, 0x8a, 0xdd, 0xff, 0xbb, 0xcf, 0x0a, 0xaa,
      0x86, 0x92, 0xbe, 0xb8, 0x50, 0xe4, 0xcd, 0x6f, 0x80, 0x33, 0x30, 0x76,
      0x13, 0x8f, 0xca, 0x7b, 0xdc, 0xec, 0x5a, 0xca, 0x63, 0xc7, 0x03, 0x25,
      0xef, 0xa8, 0x8a, 0x83, 0x58, 0x76, 0x20, 0xfa, 0x16, 0x77, 0xd7, 0x79,
      0x92, 0x63, 0x01, 0x48, 0x1a, 0xd8, 0x7b, 0x67, 0xf1, 0x52, 0x55, 0x49,
      0x4e, 0xd6, 0x6e, 0x4a, 0x5c, 0xd7, 0x7a, 0x37, 0x36, 0x0c, 0xde, 0xdd,
      0x8f, 0x44, 0xe8, 0xc2, 0xa7, 0x2c, 0x2b, 0xb5, 0xaf, 0x64, 0x4b, 0x61,
      0x07, 0x02, 0x03, 0x01, 0x00, 0x01,
  };

  // Import public key (use pre-generated for performance).
  SECItem spki_item = {siBuffer, toUcharPtr(spki), sizeof(spki)};
  ScopedCERTSubjectPublicKeyInfo cert_spki(
      SECKEY_DecodeDERSubjectPublicKeyInfo(&spki_item));
  ASSERT_TRUE(cert_spki);
  ScopedSECKEYPublicKey pub_key(SECKEY_ExtractPublicKey(cert_spki.get()));
  ASSERT_TRUE(pub_key);

  int mod_len = SECKEY_PublicKeyStrength(pub_key.get());
  ASSERT_TRUE(mod_len > 0);

  std::vector<uint8_t> ctxt(mod_len);
  unsigned int ctxt_len;
  std::vector<uint8_t> msg(mod_len, 0xff);

  // Test with valid inputs
  SECStatus rv =
      PK11_PubEncrypt(pub_key.get(), CKM_RSA_PKCS, nullptr, ctxt.data(),
                      &ctxt_len, mod_len, msg.data(), 1, nullptr);
  ASSERT_EQ(SECSuccess, rv);

  // Maximum message length is mod_len - miniumum padding (8B) - flags (3B)
  unsigned int max_msg_len = mod_len - 8 - 3;
  rv = PK11_PubEncrypt(pub_key.get(), CKM_RSA_PKCS, nullptr, ctxt.data(),
                       &ctxt_len, mod_len, msg.data(), max_msg_len, nullptr);
  ASSERT_EQ(SECSuccess, rv);

  // Test one past maximum length
  rv =
      PK11_PubEncrypt(pub_key.get(), CKM_RSA_PKCS, nullptr, ctxt.data(),
                      &ctxt_len, mod_len, msg.data(), max_msg_len + 1, nullptr);
  ASSERT_EQ(SECFailure, rv);

  // Make sure the the length will not overflow - i.e.
  // (padLen = modulusLen - (UINT_MAX + MINIMUM_PAD_LEN)) may overflow and
  // result in a value that appears valid.
  rv = PK11_PubEncrypt(pub_key.get(), CKM_RSA_PKCS, nullptr, ctxt.data(),
                       &ctxt_len, UINT_MAX, msg.data(), UINT_MAX, nullptr);
  ASSERT_EQ(SECFailure, rv);
}
}  // namespace nss_test
