/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "nss.h"
#include "pk11pub.h"

#include "cpputil.h"
#include "nss_scoped_ptrs.h"

#include "testvectors/curve25519-vectors.h"
#include "gtest/gtest.h"

namespace nss_test {

class Pkcs11Curve25519Test
    : public ::testing::TestWithParam<curve25519_testvector> {
 protected:
  void Derive(const uint8_t* pkcs8, size_t pkcs8_len, const uint8_t* spki,
              size_t spki_len, const uint8_t* secret, size_t secret_len,
              bool expect_success) {
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    ASSERT_TRUE(slot);

    SECItem pkcs8Item = {siBuffer, toUcharPtr(pkcs8),
                         static_cast<unsigned int>(pkcs8_len)};

    SECKEYPrivateKey* key = nullptr;
    SECStatus rv = PK11_ImportDERPrivateKeyInfoAndReturnKey(
        slot.get(), &pkcs8Item, nullptr, nullptr, false, false, KU_ALL, &key,
        nullptr);
    EXPECT_EQ(SECSuccess, rv);

    ScopedSECKEYPrivateKey privKey(key);
    ASSERT_TRUE(privKey);

    SECItem spkiItem = {siBuffer, toUcharPtr(spki),
                        static_cast<unsigned int>(spki_len)};

    ScopedCERTSubjectPublicKeyInfo certSpki(
        SECKEY_DecodeDERSubjectPublicKeyInfo(&spkiItem));
    if (!expect_success && !certSpki) {
      return;
    }
    ASSERT_TRUE(certSpki);

    ScopedSECKEYPublicKey pubKey(SECKEY_ExtractPublicKey(certSpki.get()));
    ASSERT_TRUE(pubKey);

    ScopedPK11SymKey symKey(PK11_PubDeriveWithKDF(
        privKey.get(), pubKey.get(), false, nullptr, nullptr, CKM_ECDH1_DERIVE,
        CKM_SHA512_HMAC, CKA_DERIVE, 0, CKD_NULL, nullptr, nullptr));
    ASSERT_EQ(expect_success, !!symKey);

    if (expect_success) {
      rv = PK11_ExtractKeyValue(symKey.get());
      EXPECT_EQ(SECSuccess, rv);

      SECItem* keyData = PK11_GetKeyData(symKey.get());
      EXPECT_EQ(secret_len, keyData->len);
      EXPECT_EQ(memcmp(keyData->data, secret, secret_len), 0);
    }
  };

  void Derive(const curve25519_testvector testvector) {
    Derive(testvector.private_key.data(), testvector.private_key.size(),
           testvector.public_key.data(), testvector.public_key.size(),
           testvector.secret.data(), testvector.secret.size(),
           testvector.valid);
  };
};

TEST_P(Pkcs11Curve25519Test, TestVectors) { Derive(GetParam()); }

INSTANTIATE_TEST_CASE_P(NSSTestVector, Pkcs11Curve25519Test,
                        ::testing::ValuesIn(kCurve25519Vectors));

INSTANTIATE_TEST_CASE_P(WycheproofTestVector, Pkcs11Curve25519Test,
                        ::testing::ValuesIn(kCurve25519WycheproofVectors));

}  // namespace nss_test
