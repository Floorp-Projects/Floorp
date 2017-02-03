/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "nss.h"
#include "pk11pub.h"
#include "sechash.h"

#include "gtest/gtest.h"
#include "scoped_ptrs.h"

#include "pk11_ecdsa_vectors.h"
#include "pk11_signature_test.h"

namespace nss_test {

class Pkcs11EcdsaTest : public Pk11SignatureTest {
 protected:
  CK_MECHANISM_TYPE mechanism() { return CKM_ECDSA; }
  SECItem* parameters() { return nullptr; }
};

class Pkcs11EcdsaSha256Test : public Pkcs11EcdsaTest {
 protected:
  SECOidTag hashOID() { return SEC_OID_SHA256; }
};

class Pkcs11EcdsaSha384Test : public Pkcs11EcdsaTest {
 protected:
  SECOidTag hashOID() { return SEC_OID_SHA384; }
};

class Pkcs11EcdsaSha512Test : public Pkcs11EcdsaTest {
 protected:
  SECOidTag hashOID() { return SEC_OID_SHA512; }
};

TEST_F(Pkcs11EcdsaSha256Test, VerifyP256) {
  SIG_TEST_VECTOR_VERIFY(kP256Spki, kP256Data, kP256Signature)
}
TEST_F(Pkcs11EcdsaSha256Test, SignAndVerifyP256) {
  SIG_TEST_VECTOR_SIGN_VERIFY(kP256Pkcs8, kP256Spki, kP256Data)
}

TEST_F(Pkcs11EcdsaSha384Test, VerifyP384) {
  SIG_TEST_VECTOR_VERIFY(kP384Spki, kP384Data, kP384Signature)
}
TEST_F(Pkcs11EcdsaSha384Test, SignAndVerifyP384) {
  SIG_TEST_VECTOR_SIGN_VERIFY(kP384Pkcs8, kP384Spki, kP384Data)
}

TEST_F(Pkcs11EcdsaSha512Test, VerifyP521) {
  SIG_TEST_VECTOR_VERIFY(kP521Spki, kP521Data, kP521Signature)
}
TEST_F(Pkcs11EcdsaSha512Test, SignAndVerifyP521) {
  SIG_TEST_VECTOR_SIGN_VERIFY(kP521Pkcs8, kP521Spki, kP521Data)
}

// Importing a private key in PKCS#8 format must fail when the outer AlgID
// struct contains neither id-ecPublicKey nor a namedCurve parameter.
TEST_F(Pkcs11EcdsaSha256Test, ImportNoCurveOIDOrAlgorithmParams) {
  EXPECT_FALSE(ImportPrivateKey(kP256Pkcs8NoCurveOIDOrAlgorithmParams,
                                sizeof(kP256Pkcs8NoCurveOIDOrAlgorithmParams)));
};

// Importing a private key in PKCS#8 format must succeed when only the outer
// AlgID struct contains the namedCurve parameters.
TEST_F(Pkcs11EcdsaSha256Test, ImportOnlyAlgorithmParams) {
  EXPECT_TRUE(ImportPrivateKeyAndSignHashedData(
      kP256Pkcs8OnlyAlgorithmParams, sizeof(kP256Pkcs8OnlyAlgorithmParams),
      kP256Data, sizeof(kP256Data)));
};

// Importing a private key in PKCS#8 format must succeed when the outer AlgID
// struct and the inner ECPrivateKey contain the same namedCurve parameters.
// The inner curveOID is always ignored, so only the outer one will be used.
TEST_F(Pkcs11EcdsaSha256Test, ImportMatchingCurveOIDAndAlgorithmParams) {
  EXPECT_TRUE(ImportPrivateKeyAndSignHashedData(
      kP256Pkcs8MatchingCurveOIDAndAlgorithmParams,
      sizeof(kP256Pkcs8MatchingCurveOIDAndAlgorithmParams), kP256Data,
      sizeof(kP256Data)));
};

// Importing a private key in PKCS#8 format must succeed when the outer AlgID
// struct and the inner ECPrivateKey contain dissimilar namedCurve parameters.
// The inner curveOID is always ignored, so only the outer one will be used.
TEST_F(Pkcs11EcdsaSha256Test, ImportDissimilarCurveOIDAndAlgorithmParams) {
  EXPECT_TRUE(ImportPrivateKeyAndSignHashedData(
      kP256Pkcs8DissimilarCurveOIDAndAlgorithmParams,
      sizeof(kP256Pkcs8DissimilarCurveOIDAndAlgorithmParams), kP256Data,
      sizeof(kP256Data)));
};

// Importing a private key in PKCS#8 format must fail when the outer ASN.1
// AlgorithmID struct contains only id-ecPublicKey but no namedCurve parameter.
TEST_F(Pkcs11EcdsaSha256Test, ImportNoAlgorithmParams) {
  EXPECT_FALSE(ImportPrivateKey(kP256Pkcs8NoAlgorithmParams,
                                sizeof(kP256Pkcs8NoAlgorithmParams)));
};

// Importing a private key in PKCS#8 format must fail when id-ecPublicKey is
// given (so we know it's an EC key) but the namedCurve parameter is unknown.
TEST_F(Pkcs11EcdsaSha256Test, ImportInvalidAlgorithmParams) {
  EXPECT_FALSE(ImportPrivateKey(kP256Pkcs8InvalidAlgorithmParams,
                                sizeof(kP256Pkcs8InvalidAlgorithmParams)));
};

// Importing a private key in PKCS#8 format with a point not on the curve will
// succeed. Using the contained public key however will fail when trying to
// import it before using it for any operation.
TEST_F(Pkcs11EcdsaSha256Test, ImportPointNotOnCurve) {
  ScopedSECKEYPrivateKey privKey(ImportPrivateKey(
      kP256Pkcs8PointNotOnCurve, sizeof(kP256Pkcs8PointNotOnCurve)));
  ASSERT_TRUE(privKey);

  ScopedSECKEYPublicKey pubKey(SECKEY_ConvertToPublicKey(privKey.get()));
  ASSERT_TRUE(pubKey);

  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_TRUE(slot);

  auto handle = PK11_ImportPublicKey(slot.get(), pubKey.get(), false);
  EXPECT_EQ(handle, static_cast<decltype(handle)>(CK_INVALID_HANDLE));
};

// Importing a private key in PKCS#8 format must fail when no point is given.
// PK11 currently offers no APIs to derive raw public keys from private values.
TEST_F(Pkcs11EcdsaSha256Test, ImportNoPublicKey) {
  EXPECT_FALSE(
      ImportPrivateKey(kP256Pkcs8NoPublicKey, sizeof(kP256Pkcs8NoPublicKey)));
};

// Importing a public key in SPKI format must fail when id-ecPublicKey is
// given (so we know it's an EC key) but the namedCurve parameter is missing.
TEST_F(Pkcs11EcdsaSha256Test, ImportSpkiNoAlgorithmParams) {
  EXPECT_FALSE(ImportPublicKey(kP256SpkiNoAlgorithmParams,
                               sizeof(kP256SpkiNoAlgorithmParams)));
}

// Importing a public key in SPKI format with a point not on the curve will
// succeed. Using the public key however will fail when trying to import
// it before using it for any operation.
TEST_F(Pkcs11EcdsaSha256Test, ImportSpkiPointNotOnCurve) {
  ScopedSECKEYPublicKey pubKey(ImportPublicKey(
      kP256SpkiPointNotOnCurve, sizeof(kP256SpkiPointNotOnCurve)));
  ASSERT_TRUE(pubKey);

  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_TRUE(slot);

  auto handle = PK11_ImportPublicKey(slot.get(), pubKey.get(), false);
  EXPECT_EQ(handle, static_cast<decltype(handle)>(CK_INVALID_HANDLE));
}

}  // namespace nss_test
