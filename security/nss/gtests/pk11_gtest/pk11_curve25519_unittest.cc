/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <memory>
#include "nss.h"
#include "pk11pub.h"
#include "prerror.h"
#include "cpputil.h"
#include "nss_scoped_ptrs.h"
#include "json_reader.h"

#include "testvectors/curve25519-vectors.h"
#include "gtest/gtest.h"

namespace nss_test {

class Pkcs11Curve25519TestBase {
 protected:
  void Derive(const uint8_t* pkcs8, size_t pkcs8_len, const uint8_t* spki,
              size_t spki_len, const uint8_t* secret, size_t secret_len,
              bool expect_success) {
    ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
    ASSERT_TRUE(slot);

    SECItem pkcs8_item = {siBuffer, toUcharPtr(pkcs8),
                          static_cast<unsigned int>(pkcs8_len)};

    SECKEYPrivateKey* key = nullptr;
    SECStatus rv = PK11_ImportDERPrivateKeyInfoAndReturnKey(
        slot.get(), &pkcs8_item, nullptr, nullptr, false, false, KU_ALL, &key,
        nullptr);
    EXPECT_EQ(SECSuccess, rv);

    ScopedSECKEYPrivateKey priv_key_sess(key);
    ASSERT_TRUE(priv_key_sess);

    SECItem spki_item = {siBuffer, toUcharPtr(spki),
                         static_cast<unsigned int>(spki_len)};

    ScopedCERTSubjectPublicKeyInfo cert_spki(
        SECKEY_DecodeDERSubjectPublicKeyInfo(&spki_item));
    if (!expect_success && !cert_spki) {
      return;
    }
    ASSERT_TRUE(cert_spki);

    ScopedSECKEYPublicKey pub_key_remote(
        SECKEY_ExtractPublicKey(cert_spki.get()));
    ASSERT_TRUE(pub_key_remote);

    // sym_key_sess = ECDH(session_import(private_test), public_test)
    ScopedPK11SymKey sym_key_sess(PK11_PubDeriveWithKDF(
        priv_key_sess.get(), pub_key_remote.get(), false, nullptr, nullptr,
        CKM_ECDH1_DERIVE, CKM_SHA512_HMAC, CKA_DERIVE, 0, CKD_NULL, nullptr,
        nullptr));
    ASSERT_EQ(expect_success, !!sym_key_sess);

    if (expect_success) {
      rv = PK11_ExtractKeyValue(sym_key_sess.get());
      EXPECT_EQ(SECSuccess, rv);

      SECItem* key_data = PK11_GetKeyData(sym_key_sess.get());
      EXPECT_EQ(secret_len, key_data->len);
      EXPECT_EQ(memcmp(key_data->data, secret, secret_len), 0);

      // Perform wrapped export on the imported private, import it as
      // permanent, and verify we derive the same shared secret
      static const uint8_t pw[] = "pw";
      SECItem pwItem = {siBuffer, toUcharPtr(pw), sizeof(pw)};
      ScopedSECKEYEncryptedPrivateKeyInfo epki(PK11_ExportEncryptedPrivKeyInfo(
          slot.get(), SEC_OID_AES_256_CBC, &pwItem, priv_key_sess.get(), 1,
          nullptr));
      ASSERT_NE(nullptr, epki) << "PK11_ExportEncryptedPrivKeyInfo failed: "
                               << PORT_ErrorToName(PORT_GetError());

      ScopedSECKEYPublicKey pub_key_local(
          SECKEY_ConvertToPublicKey(priv_key_sess.get()));

      SECKEYPrivateKey* priv_key_tok = nullptr;
      rv = PK11_ImportEncryptedPrivateKeyInfoAndReturnKey(
          slot.get(), epki.get(), &pwItem, nullptr,
          &pub_key_local->u.ec.publicValue, PR_TRUE, PR_TRUE, ecKey, 0,
          &priv_key_tok, nullptr);
      ASSERT_EQ(SECSuccess, rv) << "PK11_ImportEncryptedPrivateKeyInfo failed "
                                << PORT_ErrorToName(PORT_GetError());
      ASSERT_TRUE(priv_key_tok);

      // sym_key_tok = ECDH(token_import(export(private_test)),
      // public_test)
      ScopedPK11SymKey sym_key_tok(PK11_PubDeriveWithKDF(
          priv_key_tok, pub_key_remote.get(), false, nullptr, nullptr,
          CKM_ECDH1_DERIVE, CKM_SHA512_HMAC, CKA_DERIVE, 0, CKD_NULL, nullptr,
          nullptr));
      EXPECT_TRUE(sym_key_tok);

      if (sym_key_tok) {
        rv = PK11_ExtractKeyValue(sym_key_tok.get());
        EXPECT_EQ(SECSuccess, rv);

        key_data = PK11_GetKeyData(sym_key_tok.get());
        EXPECT_EQ(secret_len, key_data->len);
        EXPECT_EQ(memcmp(key_data->data, secret, secret_len), 0);
      }
      rv = PK11_DeleteTokenPrivateKey(priv_key_tok, true);
      EXPECT_EQ(SECSuccess, rv);
    }
  }

  void Derive(const EcdhTestVector& testvector) {
    std::cout << "Running test: " << testvector.id << std::endl;

    Derive(testvector.private_key.data(), testvector.private_key.size(),
           testvector.public_key.data(), testvector.public_key.size(),
           testvector.secret.data(), testvector.secret.size(),
           testvector.valid);
  }
};

class Pkcs11Curve25519Wycheproof : public Pkcs11Curve25519TestBase,
                                   public ::testing::Test {
 protected:
  void RunGroup(JsonReader& r) {
    std::vector<EcdhTestVector> tests;
    while (r.NextItem()) {
      std::string n = r.ReadLabel();
      if (n == "") {
        break;
      }
      if (n == "curve") {
        ASSERT_EQ("curve25519", r.ReadString());
      } else if (n == "type") {
        ASSERT_EQ("XdhComp", r.ReadString());
      } else if (n == "tests") {
        WycheproofReadTests(r, &tests, ReadTestAttr, true,
                            Pkcs11Curve25519Wycheproof::FilterInvalid);
      } else {
        FAIL() << "unknown group label: " << n;
      }
    }

    for (auto& t : tests) {
      Derive(t);
    }
  }

 private:
  static void FilterInvalid(EcdhTestVector& t, const std::string& result,
                            const std::vector<std::string>& flags) {
    static const std::vector<uint8_t> kNonCanonPublic1 = {
        0x30, 0x39, 0x30, 0x14, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02,
        0x01, 0x06, 0x09, 0x2b, 0x06, 0x01, 0x04, 0x01, 0xda, 0x47, 0x0f, 0x01,
        0x03, 0x21, 0x00, 0xda, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    };
    static const std::vector<uint8_t> kNonCanonPublic2 = {
        0x30, 0x39, 0x30, 0x14, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02,
        0x01, 0x06, 0x09, 0x2b, 0x06, 0x01, 0x04, 0x01, 0xda, 0x47, 0x0f, 0x01,
        0x03, 0x21, 0x00, 0xdb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    };

    if (result == "acceptable" &&
        (std::find_if(flags.begin(), flags.end(),
                      [](const std::string& flag) {
                        return flag == "SmallPublicKey" ||
                               flag == "ZeroSharedSecret";
                      }) != flags.end() ||
         t.public_key == kNonCanonPublic1 ||
         t.public_key == kNonCanonPublic2)) {
      t.valid = false;
    }
  }

  static void ReadTestAttr(EcdhTestVector& t, const std::string& n,
                           JsonReader& r) {
    // Static PKCS#8 and SPKI wrappers for the raw keys from Wycheproof.
    static const std::vector<uint8_t> kPrivatePrefix = {
        0x30, 0x67, 0x02, 0x01, 0x00, 0x30, 0x14, 0x06, 0x07, 0x2a, 0x86, 0x48,
        0xce, 0x3d, 0x02, 0x01, 0x06, 0x09, 0x2b, 0x06, 0x01, 0x04, 0x01, 0xda,
        0x47, 0x0f, 0x01, 0x04, 0x4c, 0x30, 0x4a, 0x02, 0x01, 0x01, 0x04, 0x20};
    // The public key section of the PKCS#8 wrapper is filled up with 0's, which
    // is not correct, but acceptable for the tests at this moment because
    // validity of the public key is not checked.
    // It's still necessary because of
    // https://searchfox.org/nss/rev/7bc70a3317b800aac07bad83e74b6c79a9ec5bff/lib/pk11wrap/pk11pk12.c#171
    static const std::vector<uint8_t> kPrivateSuffix = {
        0xa1, 0x23, 0x03, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    static const std::vector<uint8_t> kPublicPrefix = {
        0x30, 0x39, 0x30, 0x14, 0x06, 0x07, 0x2a, 0x86, 0x48,
        0xce, 0x3d, 0x02, 0x01, 0x06, 0x09, 0x2b, 0x06, 0x01,
        0x04, 0x01, 0xda, 0x47, 0x0f, 0x01, 0x03, 0x21, 0x00};

    if (n == "public") {
      t.public_key = kPublicPrefix;
      std::vector<uint8_t> pub = r.ReadHex();
      t.public_key.insert(t.public_key.end(), pub.begin(), pub.end());
    } else if (n == "private") {
      t.private_key = kPrivatePrefix;
      std::vector<uint8_t> priv = r.ReadHex();
      t.private_key.insert(t.private_key.end(), priv.begin(), priv.end());
      t.private_key.insert(t.private_key.end(), kPrivateSuffix.begin(),
                           kPrivateSuffix.end());
    } else if (n == "shared") {
      t.secret = r.ReadHex();
    } else {
      FAIL() << "unsupported test case field: " << n;
    }
  }
};

TEST_F(Pkcs11Curve25519Wycheproof, Run) {
  WycheproofHeader("x25519", "XDH", "xdh_comp_schema.json",
                   [this](JsonReader& r) { RunGroup(r); });
}

class Pkcs11Curve25519ParamTest
    : public Pkcs11Curve25519TestBase,
      public ::testing::TestWithParam<EcdhTestVector> {};

TEST_P(Pkcs11Curve25519ParamTest, TestVectors) { Derive(GetParam()); }

INSTANTIATE_TEST_SUITE_P(NSSTestVector, Pkcs11Curve25519ParamTest,
                         ::testing::ValuesIn(kCurve25519Vectors));

}  // namespace nss_test
