/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "nss.h"
#include "pk11pub.h"

#include "cpputil.h"
#include "nss_scoped_ptrs.h"

#include "testvectors/p256ecdh-vectors.h"
#include "testvectors/p384ecdh-vectors.h"
#include "testvectors/p521ecdh-vectors.h"
#include "gtest/gtest.h"

namespace nss_test {

class Pkcs11EcdhTest : public ::testing::TestWithParam<EcdhTestVector> {
 protected:
  void Derive(const EcdhTestVector vec) {
    std::string err = "Test #" + std::to_string(vec.id) + " failed";

    SECItem expect_item = {siBuffer, toUcharPtr(vec.secret.data()),
                           static_cast<unsigned int>(vec.secret.size())};

    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    ASSERT_TRUE(slot);

    SECItem priv_item = {siBuffer, toUcharPtr(vec.private_key.data()),
                         static_cast<unsigned int>(vec.private_key.size())};
    SECKEYPrivateKey* key = nullptr;
    SECStatus rv = PK11_ImportDERPrivateKeyInfoAndReturnKey(
        slot.get(), &priv_item, nullptr, nullptr, false, false, KU_ALL, &key,
        nullptr);
    EXPECT_EQ(SECSuccess, rv) << err;

    ScopedSECKEYPrivateKey priv_key(key);
    ASSERT_TRUE(priv_key) << err;

    SECItem spki_item = {siBuffer, toUcharPtr(vec.public_key.data()),
                         static_cast<unsigned int>(vec.public_key.size())};

    ScopedCERTSubjectPublicKeyInfo cert_spki(
        SECKEY_DecodeDERSubjectPublicKeyInfo(&spki_item));
    if (vec.valid) {
      ASSERT_TRUE(!!cert_spki) << err;
    } else if (!cert_spki) {
      ASSERT_TRUE(vec.invalid_asn) << err;
      return;
    }

    ScopedSECKEYPublicKey pub_key(SECKEY_ExtractPublicKey(cert_spki.get()));
    if (vec.valid) {
      ASSERT_TRUE(!!pub_key) << err;
    } else if (!pub_key) {
      ASSERT_FALSE(vec.valid) << err;
      return;
    }

    ScopedPK11SymKey sym_key(
        PK11_PubDeriveWithKDF(priv_key.get(), pub_key.get(), false, nullptr,
                              nullptr, CKM_ECDH1_DERIVE, CKM_SHA512_HMAC,
                              CKA_DERIVE, 0, CKD_NULL, nullptr, nullptr));
    ASSERT_EQ(vec.valid, !!sym_key) << err;

    if (vec.valid) {
      rv = PK11_ExtractKeyValue(sym_key.get());
      EXPECT_EQ(SECSuccess, rv) << err;

      SECItem* derived_key = PK11_GetKeyData(sym_key.get());
      EXPECT_EQ(0, SECITEM_CompareItem(derived_key, &expect_item)) << err;
    }
  };
};

TEST_P(Pkcs11EcdhTest, TestVectors) { Derive(GetParam()); }

INSTANTIATE_TEST_CASE_P(WycheproofP256EcdhTest, Pkcs11EcdhTest,
                        ::testing::ValuesIn(kP256EcdhWycheproofVectors));
INSTANTIATE_TEST_CASE_P(WycheproofP384EcdhTest, Pkcs11EcdhTest,
                        ::testing::ValuesIn(kP384EcdhWycheproofVectors));
INSTANTIATE_TEST_CASE_P(WycheproofP521EcdhTest, Pkcs11EcdhTest,
                        ::testing::ValuesIn(kP521EcdhWycheproofVectors));

}  // namespace nss_test
