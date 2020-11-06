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
#include "util.h"

#include "testvectors/hkdf-sha1-vectors.h"
#include "testvectors/hkdf-sha256-vectors.h"
#include "testvectors/hkdf-sha384-vectors.h"
#include "testvectors/hkdf-sha512-vectors.h"

namespace nss_test {

enum class HkdfTestType {
  legacy,        /* CKM_NSS_HKDF_SHA... */
  derive,        /* CKM_HKDF_DERIVE, ikm as secret key, salt as data. */
  deriveDataKey, /* CKM_HKDF_DERIVE, ikm as data, salt as data. */
  saltDerive,    /* CKM_HKDF_DERIVE, [ikm, salt] as secret key, salt as key. */
  saltDeriveDataKey, /* CKM_HKDF_DERIVE, [ikm, salt] as data, salt as key. */
  hkdfData           /* CKM_HKDF_DATA, ikm as data, salt as data. */
};
static const HkdfTestType kHkdfTestTypesAll[] = {
    HkdfTestType::legacy,
    HkdfTestType::derive,
    HkdfTestType::deriveDataKey,
    HkdfTestType::saltDerive,
    HkdfTestType::saltDeriveDataKey,
    HkdfTestType::hkdfData,
};

class Pkcs11HkdfTest
    : public ::testing::TestWithParam<
          std::tuple<HkdfTestVector, HkdfTestType, CK_MECHANISM_TYPE>> {
 protected:
  CK_MECHANISM_TYPE Pk11MechToVendorMech(CK_MECHANISM_TYPE pk11_mech) {
    switch (pk11_mech) {
      case CKM_SHA_1:
        return CKM_NSS_HKDF_SHA1;
      case CKM_SHA256:
        return CKM_NSS_HKDF_SHA256;
      case CKM_SHA384:
        return CKM_NSS_HKDF_SHA384;
      case CKM_SHA512:
        return CKM_NSS_HKDF_SHA512;
      default:
        ADD_FAILURE() << "Unknown hash mech";
        return CKM_INVALID_MECHANISM;
    }
  }

  ScopedPK11SymKey ImportKey(SECItem &ikm_item, bool import_as_data) {
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    if (!slot) {
      ADD_FAILURE() << "Can't get slot";
      return nullptr;
    }

    ScopedPK11SymKey ikm;
    if (import_as_data) {
      ikm.reset(PK11_ImportDataKey(slot.get(), CKM_HKDF_KEY_GEN,
                                   PK11_OriginUnwrap, CKA_SIGN, &ikm_item,
                                   nullptr));
    } else {
      ikm.reset(PK11_ImportSymKey(slot.get(), CKM_GENERIC_SECRET_KEY_GEN,
                                  PK11_OriginUnwrap, CKA_SIGN, &ikm_item,
                                  nullptr));
    }
    return ikm;
  }

  void RunWycheproofTest(const HkdfTestVector &vec, HkdfTestType test_type,
                         CK_MECHANISM_TYPE hash_mech) {
    std::string msg = "Test #" + std::to_string(vec.id) + " failed";
    std::vector<uint8_t> vec_ikm = hex_string_to_bytes(vec.ikm);
    std::vector<uint8_t> vec_okm = hex_string_to_bytes(vec.okm);
    std::vector<uint8_t> vec_info = hex_string_to_bytes(vec.info);
    std::vector<uint8_t> vec_salt = hex_string_to_bytes(vec.salt);
    SECItem ikm_item = {siBuffer, vec_ikm.data(),
                        static_cast<unsigned int>(vec_ikm.size())};
    SECItem okm_item = {siBuffer, vec_okm.data(),
                        static_cast<unsigned int>(vec_okm.size())};
    SECItem salt_item = {siBuffer, vec_salt.data(),
                         static_cast<unsigned int>(vec_salt.size())};
    CK_MECHANISM_TYPE derive_mech = CKM_HKDF_DERIVE;
    ScopedPK11SymKey salt_key = nullptr;
    ScopedPK11SymKey ikm = nullptr;

    // Legacy vendor mech params
    CK_NSS_HKDFParams nss_hkdf_params = {
        true, vec_salt.data(), static_cast<unsigned int>(vec_salt.size()),
        true, vec_info.data(), static_cast<unsigned int>(vec_info.size())};

    // PKCS #11 v3.0
    CK_HKDF_PARAMS hkdf_params = {
        true,
        true,
        hash_mech,
        vec_salt.size() ? CKF_HKDF_SALT_DATA : CKF_HKDF_SALT_NULL,
        vec_salt.size() ? vec_salt.data() : nullptr,
        static_cast<unsigned int>(vec_salt.size()),
        CK_INVALID_HANDLE,
        vec_info.data(),
        static_cast<unsigned int>(vec_info.size())};
    SECItem params_item = {siBuffer, (unsigned char *)&hkdf_params,
                           sizeof(hkdf_params)};

    switch (test_type) {
      case HkdfTestType::legacy:
        derive_mech = Pk11MechToVendorMech(hash_mech);
        params_item.data = (uint8_t *)&nss_hkdf_params;
        params_item.len = sizeof(nss_hkdf_params);
        ikm = ImportKey(ikm_item, false);
        break;
      case HkdfTestType::derive:
        ikm = ImportKey(ikm_item, false);
        break;
      case HkdfTestType::deriveDataKey:
        ikm = ImportKey(ikm_item, true);
        break;
      case HkdfTestType::saltDerive:
        ikm = ImportKey(ikm_item, false);
        salt_key = ImportKey(salt_item, false);
        break;
      case HkdfTestType::saltDeriveDataKey:
        ikm = ImportKey(ikm_item, true);
        salt_key = ImportKey(salt_item, true);
        break;
      case HkdfTestType::hkdfData:
        derive_mech = CKM_HKDF_DATA;
        ikm = ImportKey(ikm_item, true);
        break;
      default:
        ADD_FAILURE() << msg;
        return;
    }
    ASSERT_NE(nullptr, ikm) << msg;

    if (test_type == HkdfTestType::saltDerive ||
        test_type == HkdfTestType::saltDeriveDataKey) {
      ASSERT_NE(nullptr, salt_key) << msg;
      hkdf_params.ulSaltType = CKF_HKDF_SALT_KEY;
      hkdf_params.ulSaltLen = 0;
      hkdf_params.pSalt = NULL;
      hkdf_params.hSaltKey = PK11_GetSymKeyHandle(salt_key.get());
    }

    ScopedPK11SymKey okm = ScopedPK11SymKey(
        PK11_Derive(ikm.get(), derive_mech, &params_item,
                    CKM_GENERIC_SECRET_KEY_GEN, CKA_DERIVE, vec.size));
    if (vec.valid) {
      ASSERT_NE(nullptr, okm.get()) << msg;
      ASSERT_EQ(SECSuccess, PK11_ExtractKeyValue(okm.get())) << msg;
      ASSERT_EQ(0, SECITEM_CompareItem(&okm_item, PK11_GetKeyData(okm.get())))
          << msg;
    } else {
      ASSERT_EQ(nullptr, okm.get()) << msg;
    }
  }
};

TEST_P(Pkcs11HkdfTest, WycheproofVectors) {
  RunWycheproofTest(std::get<0>(GetParam()), std::get<1>(GetParam()),
                    std::get<2>(GetParam()));
}

INSTANTIATE_TEST_CASE_P(
    HkdfSha1, Pkcs11HkdfTest,
    ::testing::Combine(::testing::ValuesIn(kHkdfSha1WycheproofVectors),
                       ::testing::ValuesIn(kHkdfTestTypesAll),
                       ::testing::Values(CKM_SHA_1)));

INSTANTIATE_TEST_CASE_P(
    HkdfSha256, Pkcs11HkdfTest,
    ::testing::Combine(::testing::ValuesIn(kHkdfSha256WycheproofVectors),
                       ::testing::ValuesIn(kHkdfTestTypesAll),
                       ::testing::Values(CKM_SHA256)));

INSTANTIATE_TEST_CASE_P(
    HkdfSha384, Pkcs11HkdfTest,
    ::testing::Combine(::testing::ValuesIn(kHkdfSha384WycheproofVectors),
                       ::testing::ValuesIn(kHkdfTestTypesAll),
                       ::testing::Values(CKM_SHA384)));

INSTANTIATE_TEST_CASE_P(
    HkdfSha512, Pkcs11HkdfTest,
    ::testing::Combine(::testing::ValuesIn(kHkdfSha512WycheproofVectors),
                       ::testing::ValuesIn(kHkdfTestTypesAll),
                       ::testing::Values(CKM_SHA512)));
}  // namespace nss_test
