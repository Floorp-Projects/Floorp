/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "blapi.h"
#include "gtest/gtest.h"
#include "nss.h"
#include "nss_scoped_ptrs.h"
#include "pk11pub.h"
#include "secerr.h"
#include "sechash.h"
#include "testvectors/hkdf-vectors.h"
#include "util.h"

namespace nss_test {

class Pkcs11HkdfTest : public ::testing::TestWithParam<hkdf_vector> {
 protected:
  ScopedPK11SymKey ImportKey(CK_MECHANISM_TYPE mech, SECItem *ikm_item) {
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    if (!slot) {
      ADD_FAILURE() << "Can't get slot";
      return nullptr;
    }
    ScopedPK11SymKey ikm(
        PK11_ImportSymKey(slot.get(), CKM_GENERIC_SECRET_KEY_GEN,
                          PK11_OriginUnwrap, CKA_SIGN, ikm_item, nullptr));

    return ikm;
  }

  void RunTest(hkdf_vector vec) {
    SECItem ikm_item = {siBuffer, vec.ikm.data(),
                        static_cast<unsigned int>(vec.ikm.size())};

    CK_NSS_HKDFParams hkdf_params = {
        true, vec.salt.data(), static_cast<unsigned int>(vec.salt.size()),
        true, vec.info.data(), static_cast<unsigned int>(vec.info.size())};
    SECItem params_item = {siBuffer, (unsigned char *)&hkdf_params,
                           sizeof(hkdf_params)};

    ScopedPK11SymKey ikm = ImportKey(vec.mech, &ikm_item);
    ASSERT_NE(nullptr, ikm.get());
    ScopedPK11SymKey okm = ScopedPK11SymKey(
        PK11_Derive(ikm.get(), vec.mech, &params_item,
                    CKM_GENERIC_SECRET_KEY_GEN, CKA_DERIVE, vec.l));
    if (vec.res.expect_rv == SECSuccess) {
      ASSERT_NE(nullptr, okm.get());
      if (vec.res.output_match) {
        ASSERT_EQ(SECSuccess, PK11_ExtractKeyValue(okm.get()));
        SECItem *act_okm_item = PK11_GetKeyData(okm.get());
        SECItem vec_okm_item = {siBuffer, vec.okm.data(),
                                static_cast<unsigned int>(vec.okm.size())};
        ASSERT_EQ(0, SECITEM_CompareItem(&vec_okm_item, act_okm_item));
      }
    } else {
      ASSERT_EQ(nullptr, okm.get());
    }
  }
};

TEST_P(Pkcs11HkdfTest, TestVectors) { RunTest(GetParam()); }

INSTANTIATE_TEST_CASE_P(Pkcs11HkdfTests, Pkcs11HkdfTest,
                        ::testing::ValuesIn(kHkdfTestVectors));

TEST_F(Pkcs11HkdfTest, OkmLimits) {
  hkdf_vector vector{
      0,
      CKM_NSS_HKDF_SHA1,
      255 * SHA1_LENGTH /* per rfc5869 */,
      {0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
       0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b},
      {},
      {},
      {},
      {SECSuccess, false} /* Only looking at return value */
  };

  // SHA1 limit
  RunTest(vector);

  // SHA1 limit + 1
  vector.l += 1;
  vector.res.expect_rv = SECFailure;
  RunTest(vector);

  // SHA256 limit
  vector.mech = CKM_NSS_HKDF_SHA256;
  vector.l = 255 * SHA256_LENGTH; /* per rfc5869 */
  vector.res.expect_rv = SECSuccess;
  RunTest(vector);

  // SHA256 limit + 1
  vector.l += 1;
  vector.res.expect_rv = SECFailure;
  RunTest(vector);

  // SHA384 limit
  vector.mech = CKM_NSS_HKDF_SHA384;
  vector.l = 255 * SHA384_LENGTH; /* per rfc5869 */
  vector.res.expect_rv = SECSuccess;
  RunTest(vector);

  // SHA384 limit + 1
  vector.l += 1;
  vector.res.expect_rv = SECFailure;
  RunTest(vector);

  // SHA512 limit
  vector.mech = CKM_NSS_HKDF_SHA512;
  vector.l = 255 * SHA512_LENGTH; /* per rfc5869 */
  vector.res.expect_rv = SECSuccess;
  RunTest(vector);

  // SHA512 limit + 1
  vector.l += 1;
  vector.res.expect_rv = SECFailure;
  RunTest(vector);
}
}  // namespace nss_test
