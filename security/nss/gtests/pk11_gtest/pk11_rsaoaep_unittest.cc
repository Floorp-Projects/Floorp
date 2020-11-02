/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>

#include "cpputil.h"
#include "cryptohi.h"
#include "gtest/gtest.h"
#include "limits.h"
#include "nss.h"
#include "nss_scoped_ptrs.h"
#include "pk11pub.h"

#include "testvectors/rsa_oaep_2048_sha1_mgf1sha1-vectors.h"
#include "testvectors/rsa_oaep_2048_sha256_mgf1sha1-vectors.h"
#include "testvectors/rsa_oaep_2048_sha256_mgf1sha256-vectors.h"
#include "testvectors/rsa_oaep_2048_sha384_mgf1sha1-vectors.h"
#include "testvectors/rsa_oaep_2048_sha384_mgf1sha384-vectors.h"
#include "testvectors/rsa_oaep_2048_sha512_mgf1sha1-vectors.h"
#include "testvectors/rsa_oaep_2048_sha512_mgf1sha512-vectors.h"

namespace nss_test {

class RsaOaepWycheproofTest
    : public ::testing::TestWithParam<RsaOaepTestVectorStr> {
 protected:
  void TestDecrypt(const RsaOaepTestVectorStr vec) {
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

    // Set up the OAEP parameters.
    CK_RSA_PKCS_OAEP_PARAMS oaepParams;
    oaepParams.source = CKZ_DATA_SPECIFIED;
    oaepParams.pSourceData = const_cast<unsigned char*>(vec.label.data());
    oaepParams.ulSourceDataLen = vec.label.size();
    oaepParams.mgf = vec.mgf_hash;
    oaepParams.hashAlg = HashOidToHashMech(vec.hash_oid);
    SECItem params_item = {siBuffer,
                           toUcharPtr(reinterpret_cast<uint8_t*>(&oaepParams)),
                           static_cast<unsigned int>(sizeof(oaepParams))};
    // Decrypt.
    std::vector<uint8_t> decrypted(PR_MAX(1, vec.ct.size()));
    unsigned int decrypted_len = 0;
    rv = PK11_PrivDecrypt(priv_key.get(), CKM_RSA_PKCS_OAEP, &params_item,
                          decrypted.data(), &decrypted_len, decrypted.size(),
                          vec.ct.data(), vec.ct.size());

    if (vec.valid) {
      EXPECT_EQ(SECSuccess, rv);
      decrypted.resize(decrypted_len);
      EXPECT_EQ(vec.msg, decrypted);
    } else {
      EXPECT_EQ(SECFailure, rv);
    }
  };

 private:
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

TEST_P(RsaOaepWycheproofTest, OaepDecrypt) { TestDecrypt(GetParam()); }

INSTANTIATE_TEST_CASE_P(WycheproofRsa2048Sha1OaepTest, RsaOaepWycheproofTest,
                        ::testing::ValuesIn(kRsaOaep2048Sha1WycheproofVectors));

INSTANTIATE_TEST_CASE_P(
    WycheproofOaep2048Sha256Sha1Test, RsaOaepWycheproofTest,
    ::testing::ValuesIn(kRsaOaep2048Sha256Mgf1Sha1WycheproofVectors));

INSTANTIATE_TEST_CASE_P(
    WycheproofOaep2048Sha256Sha256Test, RsaOaepWycheproofTest,
    ::testing::ValuesIn(kRsaOaep2048Sha256Mgf1Sha256WycheproofVectors));

INSTANTIATE_TEST_CASE_P(
    WycheproofOaep2048Sha384Sha1Test, RsaOaepWycheproofTest,
    ::testing::ValuesIn(kRsaOaep2048Sha384Mgf1Sha1WycheproofVectors));

INSTANTIATE_TEST_CASE_P(
    WycheproofOaep2048Sha384Sha384Test, RsaOaepWycheproofTest,
    ::testing::ValuesIn(kRsaOaep2048Sha384Mgf1Sha384WycheproofVectors));

INSTANTIATE_TEST_CASE_P(
    WycheproofOaep2048Sha512Sha1Test, RsaOaepWycheproofTest,
    ::testing::ValuesIn(kRsaOaep2048Sha512Mgf1Sha1WycheproofVectors));

INSTANTIATE_TEST_CASE_P(
    WycheproofOaep2048Sha512Sha512Test, RsaOaepWycheproofTest,
    ::testing::ValuesIn(kRsaOaep2048Sha512Mgf1Sha512WycheproofVectors));

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

  // This fails because this method is broken and assumes CKM_RSA_PKCS and
  // doesn't understand OAEP.
  p_unwrapped_tmp = PK11_PubUnwrapSymKey(priv.get(), wrapped.get(), CKM_AES_CBC,
                                         CKA_DECRYPT, 16);
  ASSERT_EQ(p_unwrapped_tmp, nullptr);

  ScopedPK11SymKey unwrapped;
  p_unwrapped_tmp = PK11_PubUnwrapSymKeyWithMechanism(
      priv.get(), CKM_RSA_PKCS_OAEP, &param, wrapped.get(), CKM_AES_CBC,
      CKA_DECRYPT, 16);
  ASSERT_NE(p_unwrapped_tmp, nullptr);

  unwrapped.reset(p_unwrapped_tmp);

  // Extract key's value in order to validate decryption worked.
  rv = PK11_ExtractKeyValue(to_wrap.get());
  ASSERT_EQ(rv, SECSuccess);

  rv = PK11_ExtractKeyValue(unwrapped.get());
  ASSERT_EQ(rv, SECSuccess);

  // References owned by PKCS#11 layer; no need to scope and free.
  SECItem* expectedItem = PK11_GetKeyData(to_wrap.get());
  SECItem* actualItem = PK11_GetKeyData(unwrapped.get());

  ASSERT_EQ(SECITEM_CompareItem(actualItem, expectedItem), 0);
}
}  // namespace nss_test
