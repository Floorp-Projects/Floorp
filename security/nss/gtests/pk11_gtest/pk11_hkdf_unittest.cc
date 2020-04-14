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

/* different mechanisms for the tests */
typedef int HkdfTestType;
const int kNSSHkdfLegacy = 0;
const int kPkcs11HkdfDerive = 1;
const int kPkcs11HkdfDeriveDataKey = 2;
const int kPkcs11HkdfSaltDerive = 3;
const int kPkcs11HkdfData = 4;
const int kPKCS11NumTypes = 5;

class Pkcs11HkdfTest : public ::testing::TestWithParam<hkdf_vector> {
 protected:
  CK_MECHANISM_TYPE Pk11HkdfToHash(CK_MECHANISM_TYPE nssHkdfMech) {
    switch (nssHkdfMech) {
      case CKM_NSS_HKDF_SHA1:
        return CKM_SHA_1;
      case CKM_NSS_HKDF_SHA256:
        return CKM_SHA256;
      case CKM_NSS_HKDF_SHA384:
        return CKM_SHA384;
      case CKM_NSS_HKDF_SHA512:
        return CKM_SHA512;
      default:
        break;
    }

    return CKM_INVALID_MECHANISM;
  }

  ScopedPK11SymKey ImportKey(CK_KEY_TYPE keyType, SECItem *ikm_item) {
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    CK_MECHANISM_TYPE mech = CKM_HKDF_KEY_GEN;
    if (!slot) {
      ADD_FAILURE() << "Can't get slot";
      return nullptr;
    }
    switch (keyType) {
      case CKK_GENERIC_SECRET:
        mech = CKM_GENERIC_SECRET_KEY_GEN;
        break;
      case CKK_HKDF:
        mech = CKM_HKDF_KEY_GEN;
        break;
    }

    ScopedPK11SymKey ikm(PK11_ImportSymKey(slot.get(), mech, PK11_OriginUnwrap,
                                           CKA_SIGN, ikm_item, nullptr));

    return ikm;
  }

  void RunTest(hkdf_vector vec, HkdfTestType type, CK_KEY_TYPE keyType) {
    SECItem ikm_item = {siBuffer, vec.ikm.data(),
                        static_cast<unsigned int>(vec.ikm.size())};
    SECItem salt_item = {siBuffer, vec.salt.data(),
                         static_cast<unsigned int>(vec.salt.size())};
    CK_MECHANISM_TYPE derive_mech = vec.mech;
    CK_NSS_HKDFParams nss_hkdf_params = {
        true, vec.salt.data(), static_cast<unsigned int>(vec.salt.size()),
        true, vec.info.data(), static_cast<unsigned int>(vec.info.size())};
    CK_HKDF_PARAMS hkdf_params = {true,
                                  true,
                                  vec.mech,
                                  CKF_HKDF_SALT_DATA,
                                  vec.salt.data(),
                                  static_cast<unsigned int>(vec.salt.size()),
                                  CK_INVALID_HANDLE,
                                  vec.info.data(),
                                  static_cast<unsigned int>(vec.info.size())};
    SECItem params_item = {siBuffer, (unsigned char *)&nss_hkdf_params,
                           sizeof(nss_hkdf_params)};

    ScopedPK11SymKey ikm = ImportKey(keyType, &ikm_item);
    ScopedPK11SymKey salt_key = NULL;
    ASSERT_NE(nullptr, ikm.get());

    switch (type) {
      case kNSSHkdfLegacy:
        printf("kNSSHkdfLegacy\n");
        break; /* already set up */
      case kPkcs11HkdfDeriveDataKey: {
        ScopedPK11SlotInfo slot(PK11_GetSlotFromKey(ikm.get()));
        CK_OBJECT_CLASS ckoData = CKO_DATA;
        CK_OBJECT_HANDLE handle;
        CK_ATTRIBUTE pk11template[2] = {
            {CKA_CLASS, (CK_BYTE_PTR)&ckoData, sizeof(ckoData)},
            {CKA_VALUE, vec.ikm.data(), static_cast<CK_ULONG>(vec.ikm.size())}};

        ScopedPK11GenericObject dataKey(PK11_CreateGenericObject(
            slot.get(), pk11template, PR_ARRAY_SIZE(pk11template), PR_FALSE));
        ASSERT_NE(nullptr, dataKey.get());
        handle = PK11_GetObjectHandle(PK11_TypeGeneric, dataKey.get(), NULL);
        ASSERT_NE((CK_ULONG)CK_INVALID_HANDLE, (CK_ULONG)handle);
        /* replaces old key with our new data key */
        ikm = ScopedPK11SymKey(
            PK11_SymKeyFromHandle(slot.get(), NULL, PK11_OriginUnwrap,
                                  CKM_HKDF_DERIVE, handle, PR_TRUE, NULL));
        ASSERT_NE(nullptr, ikm.get());
        /* generic object is freed, ikm owns the handle */
      }
      /* fall through */
      case kPkcs11HkdfSaltDerive:
      case kPkcs11HkdfDerive:
        if (hkdf_params.ulSaltLen == 0) {
          hkdf_params.ulSaltType = CKF_HKDF_SALT_NULL;
          printf("kPkcs11HkdfNullSaltDerive\n");
        } else if (type == kPkcs11HkdfSaltDerive) {
          salt_key = ImportKey(keyType, &salt_item);
          hkdf_params.ulSaltType = CKF_HKDF_SALT_KEY;
          hkdf_params.ulSaltLen = 0;
          hkdf_params.pSalt = NULL;
          hkdf_params.hSaltKey = PK11_GetSymKeyHandle(salt_key.get());
          printf("kPkcs11HkdfSaltDerive\n");
        } else {
          printf("kPkcs11HkdfDerive%s\n",
                 (type == kPkcs11HkdfDeriveDataKey) ? "DataKey" : "");
        }
        hkdf_params.prfHashMechanism = Pk11HkdfToHash(vec.mech);
        params_item.data = (unsigned char *)&hkdf_params;
        params_item.len = sizeof(hkdf_params);
        derive_mech = CKM_HKDF_DERIVE;
        break;
      case kPkcs11HkdfData:
        printf("kPkcs11HkdfData\n");
        if (hkdf_params.ulSaltLen == 0) {
          hkdf_params.ulSaltType = CKF_HKDF_SALT_NULL;
        }
        hkdf_params.prfHashMechanism = Pk11HkdfToHash(vec.mech);
        params_item.data = (unsigned char *)&hkdf_params;
        params_item.len = sizeof(hkdf_params);
        derive_mech = CKM_HKDF_DATA;
        break;
    }
    ASSERT_NE(derive_mech, CKM_INVALID_MECHANISM);
    ScopedPK11SymKey okm = ScopedPK11SymKey(
        PK11_Derive(ikm.get(), derive_mech, &params_item,
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
  void RunTest(hkdf_vector vec) {
    HkdfTestType test_type;

    for (test_type = kNSSHkdfLegacy; test_type < kPKCS11NumTypes; test_type++) {
      RunTest(vec, test_type, CKK_GENERIC_SECRET);
      if (test_type == kPkcs11HkdfDeriveDataKey) {
        continue;
      }
      RunTest(vec, test_type, CKK_HKDF);
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
