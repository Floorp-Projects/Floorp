/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "nss.h"
#include "pk11pub.h"
#include "sechash.h"

#include "gtest/gtest.h"
#include "nss_scoped_ptrs.h"

#include "pk11_ecdsa_vectors.h"
#include "pk11_signature_test.h"

namespace nss_test {

class Pkcs11EcdsaTestBase : public Pk11SignatureTest {
 protected:
  Pkcs11EcdsaTestBase(SECOidTag hash_oid)
      : Pk11SignatureTest(CKM_ECDSA, hash_oid) {}
};

struct Pkcs11EcdsaTestParams {
  SECOidTag hash_oid_;
  Pkcs11SignatureTestParams sig_params_;
};

class Pkcs11EcdsaTest
    : public Pkcs11EcdsaTestBase,
      public ::testing::WithParamInterface<Pkcs11EcdsaTestParams> {
 public:
  Pkcs11EcdsaTest() : Pkcs11EcdsaTestBase(GetParam().hash_oid_) {}
};

TEST_P(Pkcs11EcdsaTest, Verify) { Verify(GetParam().sig_params_); }

TEST_P(Pkcs11EcdsaTest, SignAndVerify) {
  SignAndVerify(GetParam().sig_params_);
}

static const Pkcs11EcdsaTestParams kEcdsaVectors[] = {
    {SEC_OID_SHA256,
     {DataBuffer(kP256Pkcs8, sizeof(kP256Pkcs8)),
      DataBuffer(kP256Spki, sizeof(kP256Spki)),
      DataBuffer(kP256Data, sizeof(kP256Data)),
      DataBuffer(kP256Signature, sizeof(kP256Signature))}},
    {SEC_OID_SHA256,
     {DataBuffer(kP256Pkcs8ZeroPad, sizeof(kP256Pkcs8ZeroPad)),
      DataBuffer(kP256SpkiZeroPad, sizeof(kP256SpkiZeroPad)),
      DataBuffer(kP256DataZeroPad, sizeof(kP256DataZeroPad)),
      DataBuffer(kP256SignatureZeroPad, sizeof(kP256SignatureZeroPad))}},
    {SEC_OID_SHA384,
     {DataBuffer(kP384Pkcs8, sizeof(kP384Pkcs8)),
      DataBuffer(kP384Spki, sizeof(kP384Spki)),
      DataBuffer(kP384Data, sizeof(kP384Data)),
      DataBuffer(kP384Signature, sizeof(kP384Signature))}},
    {SEC_OID_SHA512,
     {DataBuffer(kP521Pkcs8, sizeof(kP521Pkcs8)),
      DataBuffer(kP521Spki, sizeof(kP521Spki)),
      DataBuffer(kP521Data, sizeof(kP521Data)),
      DataBuffer(kP521Signature, sizeof(kP521Signature))}}};

INSTANTIATE_TEST_CASE_P(EcdsaSignVerify, Pkcs11EcdsaTest,
                        ::testing::ValuesIn(kEcdsaVectors));

class Pkcs11EcdsaSha256Test : public Pkcs11EcdsaTestBase {
 public:
  Pkcs11EcdsaSha256Test() : Pkcs11EcdsaTestBase(SEC_OID_SHA256) {}
};

// Importing a private key in PKCS#8 format must fail when the outer AlgID
// struct contains neither id-ecPublicKey nor a namedCurve parameter.
TEST_F(Pkcs11EcdsaSha256Test, ImportNoCurveOIDOrAlgorithmParams) {
  DataBuffer k(kP256Pkcs8NoCurveOIDOrAlgorithmParams,
               sizeof(kP256Pkcs8NoCurveOIDOrAlgorithmParams));
  EXPECT_FALSE(ImportPrivateKey(k));
};

// Importing a private key in PKCS#8 format must succeed when only the outer
// AlgID struct contains the namedCurve parameters.
TEST_F(Pkcs11EcdsaSha256Test, ImportOnlyAlgorithmParams) {
  DataBuffer k(kP256Pkcs8OnlyAlgorithmParams,
               sizeof(kP256Pkcs8OnlyAlgorithmParams));
  DataBuffer data(kP256Data, sizeof(kP256Data));
  DataBuffer sig;
  EXPECT_TRUE(ImportPrivateKeyAndSignHashedData(k, data, &sig));
};

// Importing a private key in PKCS#8 format must succeed when the outer AlgID
// struct and the inner ECPrivateKey contain the same namedCurve parameters.
// The inner curveOID is always ignored, so only the outer one will be used.
TEST_F(Pkcs11EcdsaSha256Test, ImportMatchingCurveOIDAndAlgorithmParams) {
  DataBuffer k(kP256Pkcs8MatchingCurveOIDAndAlgorithmParams,
               sizeof(kP256Pkcs8MatchingCurveOIDAndAlgorithmParams));
  DataBuffer data(kP256Data, sizeof(kP256Data));
  DataBuffer sig;
  EXPECT_TRUE(ImportPrivateKeyAndSignHashedData(k, data, &sig));
};

// Importing a private key in PKCS#8 format must succeed when the outer AlgID
// struct and the inner ECPrivateKey contain dissimilar namedCurve parameters.
// The inner curveOID is always ignored, so only the outer one will be used.
TEST_F(Pkcs11EcdsaSha256Test, ImportDissimilarCurveOIDAndAlgorithmParams) {
  DataBuffer k(kP256Pkcs8DissimilarCurveOIDAndAlgorithmParams,
               sizeof(kP256Pkcs8DissimilarCurveOIDAndAlgorithmParams));
  DataBuffer data(kP256Data, sizeof(kP256Data));
  DataBuffer sig;
  EXPECT_TRUE(ImportPrivateKeyAndSignHashedData(k, data, &sig));
};

// Importing a private key in PKCS#8 format must fail when the outer ASN.1
// AlgorithmID struct contains only id-ecPublicKey but no namedCurve parameter.
TEST_F(Pkcs11EcdsaSha256Test, ImportNoAlgorithmParams) {
  DataBuffer k(kP256Pkcs8NoAlgorithmParams,
               sizeof(kP256Pkcs8NoAlgorithmParams));
  EXPECT_FALSE(ImportPrivateKey(k));
};

// Importing a private key in PKCS#8 format must fail when id-ecPublicKey is
// given (so we know it's an EC key) but the namedCurve parameter is unknown.
TEST_F(Pkcs11EcdsaSha256Test, ImportInvalidAlgorithmParams) {
  DataBuffer k(kP256Pkcs8InvalidAlgorithmParams,
               sizeof(kP256Pkcs8InvalidAlgorithmParams));
  EXPECT_FALSE(ImportPrivateKey(k));
};

// Importing a private key in PKCS#8 format with a point not on the curve will
// succeed. Using the contained public key however will fail when trying to
// import it before using it for any operation.
TEST_F(Pkcs11EcdsaSha256Test, ImportPointNotOnCurve) {
  DataBuffer k(kP256Pkcs8PointNotOnCurve, sizeof(kP256Pkcs8PointNotOnCurve));
  ScopedSECKEYPrivateKey privKey(ImportPrivateKey(k));
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
  DataBuffer k(kP256Pkcs8NoPublicKey, sizeof(kP256Pkcs8NoPublicKey));
  EXPECT_FALSE(ImportPrivateKey(k));
};

// Importing a public key in SPKI format must fail when id-ecPublicKey is
// given (so we know it's an EC key) but the namedCurve parameter is missing.
TEST_F(Pkcs11EcdsaSha256Test, ImportSpkiNoAlgorithmParams) {
  DataBuffer k(kP256SpkiNoAlgorithmParams, sizeof(kP256SpkiNoAlgorithmParams));
  EXPECT_FALSE(ImportPublicKey(k));
}

// Importing a public key in SPKI format with a point not on the curve will
// succeed. Using the public key however will fail when trying to import
// it before using it for any operation.
TEST_F(Pkcs11EcdsaSha256Test, ImportSpkiPointNotOnCurve) {
  DataBuffer k(kP256SpkiPointNotOnCurve, sizeof(kP256SpkiPointNotOnCurve));
  ScopedSECKEYPublicKey pubKey(ImportPublicKey(k));
  ASSERT_TRUE(pubKey);

  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_TRUE(slot);

  auto handle = PK11_ImportPublicKey(slot.get(), pubKey.get(), false);
  EXPECT_EQ(handle, static_cast<decltype(handle)>(CK_INVALID_HANDLE));
}

}  // namespace nss_test
