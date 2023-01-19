/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>

#include "cpputil.h"
#include "cryptohi.h"
#include "json_reader.h"
#include "gtest/gtest.h"
#include "limits.h"
#include "nss.h"
#include "nss_scoped_ptrs.h"
#include "pk11pub.h"
#include "testvectors_base/test-structs.h"

namespace nss_test {

struct RsaOaepTestVector {
  uint32_t id;
  std::vector<uint8_t> msg;
  std::vector<uint8_t> ct;
  std::vector<uint8_t> label;
  bool valid;
};

class RsaOaepWycheproofTest : public ::testing::Test {
 protected:
  void Run(const std::string& file) {
    WycheproofHeader(file, "RSAES-OAEP", "rsaes_oaep_decrypt_schema.json",
                     [this](JsonReader& r) { RunGroup(r); });
  }

  void TestDecrypt(ScopedSECKEYPrivateKey& priv_key, SECOidTag hash_oid,
                   CK_RSA_PKCS_MGF_TYPE mgf_hash,
                   const RsaOaepTestVector& vec) {
    // Set up the OAEP parameters.
    CK_RSA_PKCS_OAEP_PARAMS oaepParams;
    oaepParams.source = CKZ_DATA_SPECIFIED;
    oaepParams.pSourceData = const_cast<unsigned char*>(vec.label.data());
    oaepParams.ulSourceDataLen = vec.label.size();
    oaepParams.mgf = mgf_hash;
    oaepParams.hashAlg = HashOidToHashMech(hash_oid);
    SECItem params_item = {siBuffer,
                           toUcharPtr(reinterpret_cast<uint8_t*>(&oaepParams)),
                           static_cast<unsigned int>(sizeof(oaepParams))};
    // Decrypt.
    std::vector<uint8_t> decrypted(PR_MAX(1, vec.ct.size()));
    unsigned int decrypted_len = 0;
    SECStatus rv = PK11_PrivDecrypt(
        priv_key.get(), CKM_RSA_PKCS_OAEP, &params_item, decrypted.data(),
        &decrypted_len, decrypted.size(), vec.ct.data(), vec.ct.size());

    if (vec.valid) {
      EXPECT_EQ(SECSuccess, rv);
      decrypted.resize(decrypted_len);
      EXPECT_EQ(vec.msg, decrypted);
    } else {
      EXPECT_EQ(SECFailure, rv);
    }
  };

 private:
  void RunGroup(JsonReader& r) {
    std::vector<RsaOaepTestVector> tests;
    ScopedSECKEYPrivateKey private_key;
    CK_MECHANISM_TYPE mgf_hash = CKM_INVALID_MECHANISM;
    SECOidTag hash_oid = SEC_OID_UNKNOWN;

    while (r.NextItem()) {
      std::string n = r.ReadLabel();
      if (n == "") {
        break;
      }

      if (n == "d" || n == "e" || n == "keysize" || n == "n" ||
          n == "privateKeyJwk" || n == "privateKeyPem") {
        r.SkipValue();
      } else if (n == "privateKeyPkcs8") {
        std::vector<uint8_t> priv_key = r.ReadHex();
        private_key = LoadPrivateKey(priv_key);
      } else if (n == "mgf") {
        ASSERT_EQ("MGF1", r.ReadString());
      } else if (n == "mgfSha") {
        mgf_hash = HashOidToHashMech(r.ReadHash());
      } else if (n == "sha") {
        hash_oid = r.ReadHash();
      } else if (n == "type") {
        ASSERT_EQ("RsaesOaepDecrypt", r.ReadString());
      } else if (n == "tests") {
        WycheproofReadTests(r, &tests, ReadTestAttr);
      } else {
        FAIL() << "unknown label in group: " << n;
      }
    }

    for (auto& t : tests) {
      TestDecrypt(private_key, hash_oid, mgf_hash, t);
    }
  }

  ScopedSECKEYPrivateKey LoadPrivateKey(const std::vector<uint8_t>& priv_key) {
    SECItem pkcs8_item = {siBuffer, toUcharPtr(priv_key.data()),
                          static_cast<unsigned int>(priv_key.size())};

    ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
    EXPECT_NE(nullptr, slot);

    SECKEYPrivateKey* key = nullptr;
    SECStatus rv = PK11_ImportDERPrivateKeyInfoAndReturnKey(
        slot.get(), &pkcs8_item, nullptr, nullptr, false, false, KU_ALL, &key,
        nullptr);
    EXPECT_EQ(SECSuccess, rv);
    EXPECT_NE(nullptr, key);

    return ScopedSECKEYPrivateKey(key);
  }

  static void ReadTestAttr(RsaOaepTestVector& t, const std::string& n,
                           JsonReader& r) {
    if (n == "msg") {
      t.msg = r.ReadHex();
    } else if (n == "ct") {
      t.ct = r.ReadHex();
    } else if (n == "label") {
      t.label = r.ReadHex();
    } else {
      FAIL() << "unsupported test case field: " << n;
    }
  }

  inline CK_MECHANISM_TYPE HashOidToHashMech(SECOidTag hash_oid) {
    switch (hash_oid) {
      case SEC_OID_SHA1:
        return CKM_SHA_1;
      case SEC_OID_SHA224:
        return CKM_SHA224;
      case SEC_OID_SHA256:
        return CKM_SHA256;
      case SEC_OID_SHA384:
        return CKM_SHA384;
      case SEC_OID_SHA512:
        return CKM_SHA512;
      default:
        ADD_FAILURE();
    }
    return CKM_INVALID_MECHANISM;
  }
};

TEST_F(RsaOaepWycheproofTest, RsaOaep2048Sha1) {
  Run("rsa_oaep_2048_sha1_mgf1sha1");
}
TEST_F(RsaOaepWycheproofTest, RsaOaep2048Sha256MgfSha1) {
  Run("rsa_oaep_2048_sha256_mgf1sha1");
}
TEST_F(RsaOaepWycheproofTest, RsaOaep2048Sha256) {
  Run("rsa_oaep_2048_sha256_mgf1sha256");
}
TEST_F(RsaOaepWycheproofTest, RsaOaep2048Sha384MgfSha1) {
  Run("rsa_oaep_2048_sha384_mgf1sha1");
}
TEST_F(RsaOaepWycheproofTest, RsaOaep2048Sha384) {
  Run("rsa_oaep_2048_sha384_mgf1sha384");
}
TEST_F(RsaOaepWycheproofTest, RsaOaep2048Sha512MgfSha1) {
  Run("rsa_oaep_2048_sha512_mgf1sha1");
}
TEST_F(RsaOaepWycheproofTest, RsaOaep2048Sha512) {
  Run("rsa_oaep_2048_sha512_mgf1sha512");
}

TEST_F(RsaOaepWycheproofTest, RsaOaep3072Sha256MgfSha1) {
  Run("rsa_oaep_3072_sha256_mgf1sha1");
}
TEST_F(RsaOaepWycheproofTest, RsaOaep3072Sha256) {
  Run("rsa_oaep_3072_sha256_mgf1sha256");
}
TEST_F(RsaOaepWycheproofTest, RsaOaep3072Sha512MgfSha1) {
  Run("rsa_oaep_3072_sha512_mgf1sha1");
}
TEST_F(RsaOaepWycheproofTest, RsaOaep3072Sha512) {
  Run("rsa_oaep_3072_sha512_mgf1sha512");
}

TEST_F(RsaOaepWycheproofTest, RsaOaep4096Sha256MgfSha1) {
  Run("rsa_oaep_4096_sha256_mgf1sha1");
}
TEST_F(RsaOaepWycheproofTest, RsaOaep4096Sha256) {
  Run("rsa_oaep_4096_sha256_mgf1sha256");
}
TEST_F(RsaOaepWycheproofTest, RsaOaep4096Sha512MgfSha1) {
  Run("rsa_oaep_4096_sha512_mgf1sha1");
}
TEST_F(RsaOaepWycheproofTest, RsaOaep4096Sha512) {
  Run("rsa_oaep_4096_sha512_mgf1sha512");
}

TEST_F(RsaOaepWycheproofTest, RsaOaepMisc) { Run("rsa_oaep_misc"); }

TEST(Pkcs11RsaOaepTest, TestOaepWrapUnwrap) {
  const size_t kRsaKeyBits = 2048;
  const size_t kwrappedBufLen = 4096;

  SECStatus rv = SECFailure;

  ScopedSECKEYPrivateKey priv;
  ScopedSECKEYPublicKey pub;
  PK11RSAGenParams rsa_params;
  rsa_params.keySizeInBits = kRsaKeyBits;
  rsa_params.pe = 65537;

  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_NE(slot, nullptr);

  SECKEYPublicKey* p_pub_tmp = nullptr;
  priv.reset(PK11_GenerateKeyPair(slot.get(), CKM_RSA_PKCS_KEY_PAIR_GEN,
                                  &rsa_params, &p_pub_tmp, false, false,
                                  nullptr));
  pub.reset(p_pub_tmp);

  ASSERT_NE(priv.get(), nullptr);
  ASSERT_NE(pub.get(), nullptr);

  ScopedPK11SymKey to_wrap(
      PK11_KeyGen(slot.get(), CKM_AES_CBC, nullptr, 16, nullptr));

  CK_RSA_PKCS_OAEP_PARAMS oaep_params = {CKM_SHA256, CKG_MGF1_SHA256,
                                         CKZ_DATA_SPECIFIED, NULL, 0};

  SECItem param = {siBuffer, (unsigned char*)&oaep_params, sizeof(oaep_params)};

  ScopedSECItem wrapped(SECITEM_AllocItem(nullptr, nullptr, kwrappedBufLen));
  rv = PK11_PubWrapSymKeyWithMechanism(pub.get(), CKM_RSA_PKCS_OAEP, &param,
                                       to_wrap.get(), wrapped.get());
  ASSERT_EQ(rv, SECSuccess);

  PK11SymKey* p_unwrapped_tmp = nullptr;

  // Extract key's value in order to validate decryption worked.
  rv = PK11_ExtractKeyValue(to_wrap.get());
  ASSERT_EQ(rv, SECSuccess);

  // References owned by PKCS#11 layer; no need to scope and free.
  SECItem* expectedItem = PK11_GetKeyData(to_wrap.get());

  // This assumes CKM_RSA_PKCS and doesn't understand OAEP.
  // CKM_RSA_PKCS cannot safely return errors, however, as it can lead
  // to Bleichenbacher-like attacks. To solve this there's a new definition
  // that generates fake key material based on the message and private key.
  // This returned key material will not be the key we were expecting, so
  // make sure that's the case:
  p_unwrapped_tmp = PK11_PubUnwrapSymKey(priv.get(), wrapped.get(), CKM_AES_CBC,
                                         CKA_DECRYPT, 16);
  // As long as the wrapped data is the same length as the key
  // (which it should be), then CKM_RSA_PKCS should not fail.
  ASSERT_NE(p_unwrapped_tmp, nullptr);
  ScopedPK11SymKey fakeUnwrapped;
  fakeUnwrapped.reset(p_unwrapped_tmp);
  rv = PK11_ExtractKeyValue(fakeUnwrapped.get());
  ASSERT_EQ(rv, SECSuccess);

  // References owned by PKCS#11 layer; no need to scope and free.
  SECItem* fakeItem = PK11_GetKeyData(fakeUnwrapped.get());
  ASSERT_NE(SECITEM_CompareItem(fakeItem, expectedItem), 0);

  ScopedPK11SymKey unwrapped;
  p_unwrapped_tmp = PK11_PubUnwrapSymKeyWithMechanism(
      priv.get(), CKM_RSA_PKCS_OAEP, &param, wrapped.get(), CKM_AES_CBC,
      CKA_DECRYPT, 16);
  ASSERT_NE(p_unwrapped_tmp, nullptr);

  unwrapped.reset(p_unwrapped_tmp);

  rv = PK11_ExtractKeyValue(unwrapped.get());
  ASSERT_EQ(rv, SECSuccess);

  // References owned by PKCS#11 layer; no need to scope and free.
  SECItem* actualItem = PK11_GetKeyData(unwrapped.get());

  ASSERT_EQ(SECITEM_CompareItem(actualItem, expectedItem), 0);
}
}  // namespace nss_test
