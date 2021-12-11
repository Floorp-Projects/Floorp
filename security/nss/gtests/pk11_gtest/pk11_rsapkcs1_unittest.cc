/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>
#include <memory>
#include "cryptohi.h"
#include "cpputil.h"
#include "databuffer.h"
#include "gtest/gtest.h"
#include "nss.h"
#include "nss_scoped_ptrs.h"
#include "pk11pub.h"
#include "secerr.h"
#include "sechash.h"
#include "pk11_signature_test.h"

#include "testvectors/rsa_signature_2048_sha224-vectors.h"
#include "testvectors/rsa_signature_2048_sha256-vectors.h"
#include "testvectors/rsa_signature_2048_sha512-vectors.h"
#include "testvectors/rsa_signature_3072_sha256-vectors.h"
#include "testvectors/rsa_signature_3072_sha384-vectors.h"
#include "testvectors/rsa_signature_3072_sha512-vectors.h"
#include "testvectors/rsa_signature_4096_sha384-vectors.h"
#include "testvectors/rsa_signature_4096_sha512-vectors.h"
#include "testvectors/rsa_signature-vectors.h"

namespace nss_test {

CK_MECHANISM_TYPE RsaHashToComboMech(SECOidTag hash) {
  switch (hash) {
    case SEC_OID_SHA1:
      return CKM_SHA1_RSA_PKCS;
    case SEC_OID_SHA224:
      return CKM_SHA224_RSA_PKCS;
    case SEC_OID_SHA256:
      return CKM_SHA256_RSA_PKCS;
    case SEC_OID_SHA384:
      return CKM_SHA384_RSA_PKCS;
    case SEC_OID_SHA512:
      return CKM_SHA512_RSA_PKCS;
    default:
      break;
  }
  return CKM_INVALID_MECHANISM;
}

class Pkcs11RsaBaseTest : public Pk11SignatureTest {
 protected:
  Pkcs11RsaBaseTest(SECOidTag hashOid)
      : Pk11SignatureTest(CKM_RSA_PKCS, hashOid, RsaHashToComboMech(hashOid)) {}

  void Verify(const RsaSignatureTestVector vec) {
    Pkcs11SignatureTestParams params = {
        DataBuffer(), DataBuffer(vec.public_key.data(), vec.public_key.size()),
        DataBuffer(vec.msg.data(), vec.msg.size()),
        DataBuffer(vec.sig.data(), vec.sig.size())};
    Pk11SignatureTest::Verify(params, (bool)vec.valid);
  }
};

class Pkcs11RsaPkcs1WycheproofTest
    : public Pkcs11RsaBaseTest,
      public ::testing::WithParamInterface<RsaSignatureTestVector> {
 public:
  Pkcs11RsaPkcs1WycheproofTest() : Pkcs11RsaBaseTest(GetParam().hash_oid) {}

 protected:
  void Verify1(const RsaSignatureTestVector vec) {
    SECItem spki_item = {siBuffer, toUcharPtr(vec.public_key.data()),
                         static_cast<unsigned int>(vec.public_key.size())};

    ScopedCERTSubjectPublicKeyInfo cert_spki(
        SECKEY_DecodeDERSubjectPublicKeyInfo(&spki_item));
    ASSERT_TRUE(cert_spki);

    ScopedSECKEYPublicKey pub_key(SECKEY_ExtractPublicKey(cert_spki.get()));
    ASSERT_TRUE(pub_key);

    DataBuffer hash;
    hash.Allocate(static_cast<size_t>(HASH_ResultLenByOidTag(vec.hash_oid)));
    SECStatus rv = PK11_HashBuf(vec.hash_oid, toUcharPtr(hash.data()),
                                toUcharPtr(vec.msg.data()), vec.msg.size());
    ASSERT_EQ(rv, SECSuccess);

    // Verify.
    SECItem hash_item = {siBuffer, toUcharPtr(hash.data()),
                         static_cast<unsigned int>(hash.len())};
    SECItem sig_item = {siBuffer, toUcharPtr(vec.sig.data()),
                        static_cast<unsigned int>(vec.sig.size())};

    rv = VFY_VerifyDigestDirect(&hash_item, pub_key.get(), &sig_item,
                                SEC_OID_PKCS1_RSA_ENCRYPTION, vec.hash_oid,
                                nullptr);
    EXPECT_EQ(rv, vec.valid ? SECSuccess : SECFailure);
  };
};

/* Test that PKCS #1 v1.5 verification requires a minimum of 8B
 * of padding, per-RFC3447. The padding formula is
 * `pad_len = em_len - t_len - 3`, where em_len is the octet length
 * of the RSA modulus and t_len is the length of the `DigestInfo ||
 * Hash(message)` sequence. For SHA512, t_len is 83. We'll tweak the
 * modulus size to test with a pad_len of 8 (valid) and 6 (invalid):
 *   em_len = `8 + 83 + 3` = `94*8` = 752b
 *   em_len = `6 + 83 + 3` = `92*8` = 736b
 * Use 6 as the invalid value since modLen % 16 must be zero.
 */
TEST(RsaPkcs1Test, Pkcs1MinimumPadding) {
  const size_t kRsaShortKeyBits = 736;
  const size_t kRsaKeyBits = 752;
  static const std::vector<uint8_t> kMsg{'T', 'E', 'S', 'T'};
  static const std::vector<uint8_t> kSha512DigestInfo{
      0x30, 0x51, 0x30, 0x0D, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
      0x65, 0x03, 0x04, 0x02, 0x03, 0x05, 0x00, 0x04, 0x40};
  static const std::vector<uint8_t> kMsgSha512{
      0x7B, 0xFA, 0x95, 0xA6, 0x88, 0x92, 0x4C, 0x47, 0xC7, 0xD2, 0x23,
      0x81, 0xF2, 0x0C, 0xC9, 0x26, 0xF5, 0x24, 0xBE, 0xAC, 0xB1, 0x3F,
      0x84, 0xE2, 0x03, 0xD4, 0xBD, 0x8C, 0xB6, 0xBA, 0x2F, 0xCE, 0x81,
      0xC5, 0x7A, 0x5F, 0x05, 0x9B, 0xF3, 0xD5, 0x09, 0x92, 0x64, 0x87,
      0xBD, 0xE9, 0x25, 0xB3, 0xBC, 0xEE, 0x06, 0x35, 0xE4, 0xF7, 0xBA,
      0xEB, 0xA0, 0x54, 0xE5, 0xDB, 0xA6, 0x96, 0xB2, 0xBF};

  ScopedSECKEYPrivateKey short_priv, good_priv;
  ScopedSECKEYPublicKey short_pub, good_pub;
  PK11RSAGenParams rsa_params;
  rsa_params.keySizeInBits = kRsaShortKeyBits;
  rsa_params.pe = 65537;

  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_TRUE(slot);
  SECKEYPublicKey* p_pub_tmp = nullptr;
  short_priv.reset(PK11_GenerateKeyPair(slot.get(), CKM_RSA_PKCS_KEY_PAIR_GEN,
                                        &rsa_params, &p_pub_tmp, false, false,
                                        nullptr));
  short_pub.reset(p_pub_tmp);

  rsa_params.keySizeInBits = kRsaKeyBits;
  good_priv.reset(PK11_GenerateKeyPair(slot.get(), CKM_RSA_PKCS_KEY_PAIR_GEN,
                                       &rsa_params, &p_pub_tmp, false, false,
                                       nullptr));
  good_pub.reset(p_pub_tmp);

  size_t em_len = kRsaShortKeyBits / 8;
  size_t t_len = kSha512DigestInfo.size() + kMsgSha512.size();
  size_t pad_len = em_len - t_len - 3;
  ASSERT_EQ(6U, pad_len);

  std::vector<uint8_t> invalid_pkcs;
  invalid_pkcs.push_back(0x00);
  invalid_pkcs.push_back(0x01);
  invalid_pkcs.insert(invalid_pkcs.end(), pad_len, 0xff);
  invalid_pkcs.insert(invalid_pkcs.end(), 1, 0x00);
  invalid_pkcs.insert(invalid_pkcs.end(), kSha512DigestInfo.begin(),
                      kSha512DigestInfo.end());
  invalid_pkcs.insert(invalid_pkcs.end(), kMsgSha512.begin(), kMsgSha512.end());
  ASSERT_EQ(em_len, invalid_pkcs.size());

  // Sign it indirectly. Signing functions check for a proper pad_len.
  std::vector<uint8_t> sig(em_len);
  uint32_t sig_len;
  SECStatus rv =
      PK11_PubDecryptRaw(short_priv.get(), sig.data(), &sig_len, sig.size(),
                         invalid_pkcs.data(), invalid_pkcs.size());
  EXPECT_EQ(SECSuccess, rv);

  // Verify it.
  DataBuffer hash;
  hash.Allocate(static_cast<size_t>(HASH_ResultLenByOidTag(SEC_OID_SHA512)));
  rv = PK11_HashBuf(SEC_OID_SHA512, toUcharPtr(hash.data()),
                    toUcharPtr(kMsg.data()), kMsg.size());
  ASSERT_EQ(rv, SECSuccess);
  SECItem hash_item = {siBuffer, toUcharPtr(hash.data()),
                       static_cast<unsigned int>(hash.len())};
  SECItem sig_item = {siBuffer, toUcharPtr(sig.data()), sig_len};
  rv = VFY_VerifyDigestDirect(&hash_item, short_pub.get(), &sig_item,
                              SEC_OID_PKCS1_RSA_ENCRYPTION, SEC_OID_SHA512,
                              nullptr);
  EXPECT_EQ(SECFailure, rv);
  EXPECT_EQ(SEC_ERROR_BAD_SIGNATURE, PORT_GetError());

  // Repeat the test with the sufficiently-long key.
  em_len = kRsaKeyBits / 8;
  t_len = kSha512DigestInfo.size() + kMsgSha512.size();
  pad_len = em_len - t_len - 3;
  ASSERT_EQ(8U, pad_len);

  std::vector<uint8_t> valid_pkcs;
  valid_pkcs.push_back(0x00);
  valid_pkcs.push_back(0x01);
  valid_pkcs.insert(valid_pkcs.end(), pad_len, 0xff);
  valid_pkcs.insert(valid_pkcs.end(), 1, 0x00);
  valid_pkcs.insert(valid_pkcs.end(), kSha512DigestInfo.begin(),
                    kSha512DigestInfo.end());
  valid_pkcs.insert(valid_pkcs.end(), kMsgSha512.begin(), kMsgSha512.end());
  ASSERT_EQ(em_len, valid_pkcs.size());

  // Sign it the same way as above (even though we could use sign APIs now).
  sig.resize(em_len);
  rv = PK11_PubDecryptRaw(good_priv.get(), sig.data(), &sig_len, sig.size(),
                          valid_pkcs.data(), valid_pkcs.size());
  EXPECT_EQ(SECSuccess, rv);

  // Verify it.
  sig_item = {siBuffer, toUcharPtr(sig.data()), sig_len};
  rv = VFY_VerifyDigestDirect(&hash_item, good_pub.get(), &sig_item,
                              SEC_OID_PKCS1_RSA_ENCRYPTION, SEC_OID_SHA512,
                              nullptr);
  EXPECT_EQ(SECSuccess, rv);
}

TEST(RsaPkcs1Test, RequireNullParameter) {
  // The test vectors may be verified with:
  //
  // openssl rsautl -keyform der -pubin -inkey spki.bin -in sig.bin | der2ascii
  // openssl rsautl -keyform der -pubin -inkey spki.bin -in sig2.bin | der2ascii

  // Import public key.
  SECItem spki_item = {siBuffer, toUcharPtr(kSpki), sizeof(kSpki)};
  ScopedCERTSubjectPublicKeyInfo cert_spki(
      SECKEY_DecodeDERSubjectPublicKeyInfo(&spki_item));
  ASSERT_TRUE(cert_spki);
  ScopedSECKEYPublicKey pub_key(SECKEY_ExtractPublicKey(cert_spki.get()));
  ASSERT_TRUE(pub_key);

  SECItem hash = {siBuffer, toUcharPtr(kHash), sizeof(kHash)};

  // kSignature is a valid signature.
  SECItem sig_item = {siBuffer, toUcharPtr(kSignature), sizeof(kSignature)};
  SECStatus rv = VFY_VerifyDigestDirect(&hash, pub_key.get(), &sig_item,
                                        SEC_OID_PKCS1_RSA_ENCRYPTION,
                                        SEC_OID_SHA256, nullptr);
  EXPECT_EQ(SECSuccess, rv);

  // kSignatureInvalid is not.
  sig_item = {siBuffer, toUcharPtr(kSignatureInvalid),
              sizeof(kSignatureInvalid)};
  rv = VFY_VerifyDigestDirect(&hash, pub_key.get(), &sig_item,
                              SEC_OID_PKCS1_RSA_ENCRYPTION, SEC_OID_SHA256,
                              nullptr);
#ifdef NSS_PKCS1_AllowMissingParameters
  EXPECT_EQ(SECSuccess, rv);
#else
  EXPECT_EQ(SECFailure, rv);
#endif
}

TEST_P(Pkcs11RsaPkcs1WycheproofTest, Verify) {
  /* Using VFY_ interface */
  Verify1(GetParam());
  /* Using PKCS #11 interface */
  setSkipRaw(true);
  Verify(GetParam());
}

INSTANTIATE_TEST_SUITE_P(
    Wycheproof2048RsaSignatureSha224Test, Pkcs11RsaPkcs1WycheproofTest,
    ::testing::ValuesIn(kRsaSignature2048Sha224WycheproofVectors));

INSTANTIATE_TEST_SUITE_P(
    Wycheproof2048RsaSignatureSha256Test, Pkcs11RsaPkcs1WycheproofTest,
    ::testing::ValuesIn(kRsaSignature2048Sha256WycheproofVectors));

INSTANTIATE_TEST_SUITE_P(
    Wycheproof2048RsaSignatureSha512Test, Pkcs11RsaPkcs1WycheproofTest,
    ::testing::ValuesIn(kRsaSignature2048Sha512WycheproofVectors));

INSTANTIATE_TEST_SUITE_P(
    Wycheproof3072RsaSignatureSha256Test, Pkcs11RsaPkcs1WycheproofTest,
    ::testing::ValuesIn(kRsaSignature3072Sha256WycheproofVectors));

INSTANTIATE_TEST_SUITE_P(
    Wycheproof3072RsaSignatureSha384Test, Pkcs11RsaPkcs1WycheproofTest,
    ::testing::ValuesIn(kRsaSignature3072Sha384WycheproofVectors));

INSTANTIATE_TEST_SUITE_P(
    Wycheproof3072RsaSignatureSha512Test, Pkcs11RsaPkcs1WycheproofTest,
    ::testing::ValuesIn(kRsaSignature3072Sha512WycheproofVectors));

INSTANTIATE_TEST_SUITE_P(
    Wycheproof4096RsaSignatureSha384Test, Pkcs11RsaPkcs1WycheproofTest,
    ::testing::ValuesIn(kRsaSignature4096Sha384WycheproofVectors));

INSTANTIATE_TEST_SUITE_P(
    Wycheproof4096RsaSignatureSha512Test, Pkcs11RsaPkcs1WycheproofTest,
    ::testing::ValuesIn(kRsaSignature4096Sha512WycheproofVectors));

INSTANTIATE_TEST_SUITE_P(WycheproofRsaSignatureTest,
                         Pkcs11RsaPkcs1WycheproofTest,
                         ::testing::ValuesIn(kRsaSignatureWycheproofVectors));

}  // namespace nss_test
