/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "nss.h"
#include "pk11pub.h"
#include "sechash.h"
#include "cryptohi.h"

#include "cpputil.h"
#include "databuffer.h"

#include "gtest/gtest.h"
#include "nss_scoped_ptrs.h"

#include "testvectors/dsa-vectors.h"

namespace nss_test {

class Pkcs11DsaTest : public ::testing::TestWithParam<DsaTestVector> {
 protected:
  void Derive(const uint8_t* sig, size_t sig_len, const uint8_t* spki,
              size_t spki_len, const uint8_t* data, size_t data_len,
              bool expect_success, const uint32_t test_id,
              const SECOidTag hash_oid) {
    std::stringstream s;
    s << "Test with original ID #" << test_id << " failed.\n";
    s << "Expected Success: " << expect_success << "\n";
    std::string msg = s.str();

    SECItem spki_item = {siBuffer, toUcharPtr(spki),
                         static_cast<unsigned int>(spki_len)};

    ScopedCERTSubjectPublicKeyInfo cert_spki(
        SECKEY_DecodeDERSubjectPublicKeyInfo(&spki_item));
    ASSERT_TRUE(cert_spki) << msg;

    ScopedSECKEYPublicKey pub_key(SECKEY_ExtractPublicKey(cert_spki.get()));
    ASSERT_TRUE(pub_key) << msg;

    SECItem sig_item = {siBuffer, toUcharPtr(sig),
                        static_cast<unsigned int>(sig_len)};
    ScopedSECItem decoded_sig_item(
        DSAU_DecodeDerSigToLen(&sig_item, SECKEY_SignatureLen(pub_key.get())));
    if (!decoded_sig_item) {
      ASSERT_FALSE(expect_success) << msg;
      return;
    }

    DataBuffer hash;
    hash.Allocate(static_cast<size_t>(HASH_ResultLenByOidTag(hash_oid)));
    SECStatus rv = PK11_HashBuf(hash_oid, toUcharPtr(hash.data()),
                                toUcharPtr(data), data_len);
    ASSERT_EQ(SECSuccess, rv) << msg;

    // Verify.
    SECItem hash_item = {siBuffer, toUcharPtr(hash.data()),
                         static_cast<unsigned int>(hash.len())};
    rv = PK11_VerifyWithMechanism(pub_key.get(), CKM_DSA, nullptr,
                                  decoded_sig_item.get(), &hash_item, nullptr);
    EXPECT_EQ(expect_success ? SECSuccess : SECFailure, rv);
  };

  void Derive(const DsaTestVector vector) {
    Derive(vector.sig.data(), vector.sig.size(), vector.public_key.data(),
           vector.public_key.size(), vector.msg.data(), vector.msg.size(),
           vector.valid, vector.id, vector.hash_oid);
  };
};

TEST_P(Pkcs11DsaTest, WycheproofVectors) { Derive(GetParam()); }

INSTANTIATE_TEST_SUITE_P(DsaTest, Pkcs11DsaTest,
                         ::testing::ValuesIn(kDsaWycheproofVectors));

}  // namespace nss_test
