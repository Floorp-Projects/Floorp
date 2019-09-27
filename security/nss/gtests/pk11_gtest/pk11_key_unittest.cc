/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "nss.h"
#include "pk11pub.h"
#include "pk11pqg.h"
#include "prerror.h"
#include "secoid.h"

#include "gtest/gtest.h"
#include "nss_scoped_ptrs.h"
#include "pk11_keygen.h"

namespace nss_test {

class Pkcs11NullKeyTestBase : public ::testing::Test {
 protected:
  // This constructs a key pair, then erases the public value from the public
  // key.  NSS should reject this.
  void Test(const Pkcs11KeyPairGenerator& generator,
            CK_MECHANISM_TYPE dh_mech) {
    ScopedSECKEYPrivateKey priv;
    ScopedSECKEYPublicKey pub;
    generator.GenerateKey(&priv, &pub);
    ASSERT_TRUE(priv);

    // These don't leak because they are allocated to the arena associated with
    // the public key.
    SECItem* pub_val = nullptr;
    switch (SECKEY_GetPublicKeyType(pub.get())) {
      case rsaKey:
        pub_val = &pub->u.rsa.modulus;
        break;

      case dsaKey:
        pub_val = &pub->u.dsa.publicValue;
        break;

      case dhKey:
        pub_val = &pub->u.dh.publicValue;
        break;

      case ecKey:
        pub_val = &pub->u.ec.publicValue;
        break;

      default:
        FAIL() << "Unknown key type " << SECKEY_GetPublicKeyType(pub.get());
    }
    pub_val->data = nullptr;
    pub_val->len = 0;

    ScopedPK11SymKey symKey(PK11_PubDeriveWithKDF(
        priv.get(), pub.get(), false, nullptr, nullptr, dh_mech,
        CKM_SHA512_HMAC, CKA_DERIVE, 0, CKD_NULL, nullptr, nullptr));
    ASSERT_FALSE(symKey);
  }
};

class Pkcs11DhNullKeyTest : public Pkcs11NullKeyTestBase {};
TEST_F(Pkcs11DhNullKeyTest, UseNullPublicValue) {
  Test(Pkcs11KeyPairGenerator(CKM_DH_PKCS_KEY_PAIR_GEN), CKM_DH_PKCS_DERIVE);
}

class Pkcs11EcdhNullKeyTest : public Pkcs11NullKeyTestBase,
                              public ::testing::WithParamInterface<SECOidTag> {
};
TEST_P(Pkcs11EcdhNullKeyTest, UseNullPublicValue) {
  Test(Pkcs11KeyPairGenerator(CKM_EC_KEY_PAIR_GEN, GetParam()),
       CKM_ECDH1_DERIVE);
}
INSTANTIATE_TEST_CASE_P(Pkcs11EcdhNullKeyTest, Pkcs11EcdhNullKeyTest,
                        ::testing::Values(SEC_OID_SECG_EC_SECP256R1,
                                          SEC_OID_SECG_EC_SECP384R1,
                                          SEC_OID_SECG_EC_SECP521R1,
                                          SEC_OID_CURVE25519));

}  // namespace nss_test
