/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "nss.h"
#include "prerror.h"
#include "pk11pub.h"
#include "sechash.h"
#include "cryptohi.h"

#include "cpputil.h"
#include "databuffer.h"
#include "pk11_signature_test.h"

#include "gtest/gtest.h"
#include "nss_scoped_ptrs.h"

#include "testvectors/dsa-vectors.h"

namespace nss_test {
CK_MECHANISM_TYPE
DsaHashToComboMech(SECOidTag hash) {
  switch (hash) {
    case SEC_OID_SHA1:
      return CKM_DSA_SHA1;
    case SEC_OID_SHA224:
      return CKM_DSA_SHA224;
    case SEC_OID_SHA256:
      return CKM_DSA_SHA256;
    case SEC_OID_SHA384:
      return CKM_DSA_SHA384;
    case SEC_OID_SHA512:
      return CKM_DSA_SHA512;
    default:
      break;
  }
  return CKM_INVALID_MECHANISM;
}

class Pkcs11DsaTestBase : public Pk11SignatureTest {
 protected:
  Pkcs11DsaTestBase(SECOidTag hashOid)
      : Pk11SignatureTest(CKM_DSA, hashOid, DsaHashToComboMech(hashOid)) {}

  void Verify(const DsaTestVector vec) {
    /* DSA vectors encode the signature in DER, we need to unwrap it before
     * we can send the raw signatures to PKCS #11. */
    DataBuffer pubKeyBuffer(vec.public_key.data(), vec.public_key.size());
    ScopedSECKEYPublicKey nssPubKey(ImportPublicKey(pubKeyBuffer));
    SECItem sigItem = {siBuffer, toUcharPtr(vec.sig.data()),
                       static_cast<unsigned int>(vec.sig.size())};
    ScopedSECItem decodedSigItem(
        DSAU_DecodeDerSigToLen(&sigItem, SECKEY_SignatureLen(nssPubKey.get())));
    if (!decodedSigItem) {
      ASSERT_FALSE(vec.valid) << "Failed to decode DSA signature Error: "
                              << PORT_ErrorToString(PORT_GetError()) << "\n";
      return;
    }

    Pkcs11SignatureTestParams params = {
        DataBuffer(), pubKeyBuffer, DataBuffer(vec.msg.data(), vec.msg.size()),
        DataBuffer(decodedSigItem.get()->data, decodedSigItem.get()->len)};
    Pk11SignatureTest::Verify(params, (bool)vec.valid);
  }
};

class Pkcs11DsaTest : public Pkcs11DsaTestBase,
                      public ::testing::WithParamInterface<DsaTestVector> {
 public:
  Pkcs11DsaTest() : Pkcs11DsaTestBase(GetParam().hash_oid) {}
};

TEST_P(Pkcs11DsaTest, WycheproofVectors) { Verify(GetParam()); }

INSTANTIATE_TEST_SUITE_P(DsaTest, Pkcs11DsaTest,
                         ::testing::ValuesIn(kDsaWycheproofVectors));

}  // namespace nss_test
