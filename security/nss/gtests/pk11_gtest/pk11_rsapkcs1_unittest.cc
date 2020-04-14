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
#include "sechash.h"

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

class Pkcs11RsaPkcs1WycheproofTest
    : public ::testing::TestWithParam<RsaSignatureTestVector> {
 protected:
  void Derive(const RsaSignatureTestVector vec) {
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

TEST_P(Pkcs11RsaPkcs1WycheproofTest, Verify) { Derive(GetParam()); }

INSTANTIATE_TEST_CASE_P(
    Wycheproof2048RsaSignatureSha224Test, Pkcs11RsaPkcs1WycheproofTest,
    ::testing::ValuesIn(kRsaSignature2048Sha224WycheproofVectors));

INSTANTIATE_TEST_CASE_P(
    Wycheproof2048RsaSignatureSha256Test, Pkcs11RsaPkcs1WycheproofTest,
    ::testing::ValuesIn(kRsaSignature2048Sha256WycheproofVectors));

INSTANTIATE_TEST_CASE_P(
    Wycheproof2048RsaSignatureSha512Test, Pkcs11RsaPkcs1WycheproofTest,
    ::testing::ValuesIn(kRsaSignature2048Sha512WycheproofVectors));

INSTANTIATE_TEST_CASE_P(
    Wycheproof3072RsaSignatureSha256Test, Pkcs11RsaPkcs1WycheproofTest,
    ::testing::ValuesIn(kRsaSignature3072Sha256WycheproofVectors));

INSTANTIATE_TEST_CASE_P(
    Wycheproof3072RsaSignatureSha384Test, Pkcs11RsaPkcs1WycheproofTest,
    ::testing::ValuesIn(kRsaSignature3072Sha384WycheproofVectors));

INSTANTIATE_TEST_CASE_P(
    Wycheproof3072RsaSignatureSha512Test, Pkcs11RsaPkcs1WycheproofTest,
    ::testing::ValuesIn(kRsaSignature3072Sha512WycheproofVectors));

INSTANTIATE_TEST_CASE_P(
    Wycheproof4096RsaSignatureSha384Test, Pkcs11RsaPkcs1WycheproofTest,
    ::testing::ValuesIn(kRsaSignature4096Sha384WycheproofVectors));

INSTANTIATE_TEST_CASE_P(
    Wycheproof4096RsaSignatureSha512Test, Pkcs11RsaPkcs1WycheproofTest,
    ::testing::ValuesIn(kRsaSignature4096Sha512WycheproofVectors));

INSTANTIATE_TEST_CASE_P(WycheproofRsaSignatureTest,
                        Pkcs11RsaPkcs1WycheproofTest,
                        ::testing::ValuesIn(kRsaSignatureWycheproofVectors));

}  // namespace nss_test
