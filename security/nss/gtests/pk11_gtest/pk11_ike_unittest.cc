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
#include "databuffer.h"

#include "testvectors/ike-sha1-vectors.h"
#include "testvectors/ike-sha256-vectors.h"
#include "testvectors/ike-sha384-vectors.h"
#include "testvectors/ike-sha512-vectors.h"
#include "testvectors/ike-aesxcbc-vectors.h"

namespace nss_test {

class Pkcs11IkeTest : public ::testing::TestWithParam<
                          std::tuple<IkeTestVector, CK_MECHANISM_TYPE>> {
 protected:
  ScopedPK11SymKey ImportKey(SECItem &ikm_item) {
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    if (!slot) {
      ADD_FAILURE() << "Can't get slot";
      return nullptr;
    }
    ScopedPK11SymKey ikm(
        PK11_ImportSymKey(slot.get(), CKM_GENERIC_SECRET_KEY_GEN,
                          PK11_OriginUnwrap, CKA_DERIVE, &ikm_item, nullptr));
    return ikm;
  }

  void RunVectorTest(const IkeTestVector &vec, CK_MECHANISM_TYPE prf_mech) {
    std::string msg = "Test #" + std::to_string(vec.id) + " failed";
    std::vector<uint8_t> vec_ikm = hex_string_to_bytes(vec.ikm);
    std::vector<uint8_t> vec_okm = hex_string_to_bytes(vec.okm);
    std::vector<uint8_t> vec_gxykm = hex_string_to_bytes(vec.gxykm);
    std::vector<uint8_t> vec_prevkm = hex_string_to_bytes(vec.prevkm);
    std::vector<uint8_t> vec_Ni = hex_string_to_bytes(vec.Ni);
    std::vector<uint8_t> vec_Nr = hex_string_to_bytes(vec.Nr);
    std::vector<uint8_t> vec_seed_data = hex_string_to_bytes(vec.seed_data);
    SECItem ikm_item = {siBuffer, vec_ikm.data(),
                        static_cast<unsigned int>(vec_ikm.size())};
    SECItem okm_item = {siBuffer, vec_okm.data(),
                        static_cast<unsigned int>(vec_okm.size())};
    SECItem prevkm_item = {siBuffer, vec_prevkm.data(),
                           static_cast<unsigned int>(vec_prevkm.size())};
    SECItem gxykm_item = {siBuffer, vec_gxykm.data(),
                          static_cast<unsigned int>(vec_gxykm.size())};
    CK_MECHANISM_TYPE derive_mech = CKM_NSS_IKE_PRF_DERIVE;
    ScopedPK11SymKey gxy_key = nullptr;
    ScopedPK11SymKey prev_key = nullptr;
    ScopedPK11SymKey ikm = ImportKey(ikm_item);

    // IKE_PRF structure (used in cases 1, 2 and 3)
    CK_NSS_IKE_PRF_DERIVE_PARAMS nss_ike_prf_params = {
        prf_mech,
        CK_FALSE,
        CK_FALSE,
        vec_Ni.data(),
        static_cast<CK_ULONG>(vec_Ni.size()),
        vec_Nr.data(),
        static_cast<CK_ULONG>(vec_Nr.size()),
        CK_INVALID_HANDLE};

    // IKE_V1_PRF, used to derive session keys.
    CK_NSS_IKE1_PRF_DERIVE_PARAMS nss_ike_v1_prf_params = {
        prf_mech,          false,
        CK_INVALID_HANDLE, CK_INVALID_HANDLE,
        vec_Ni.data(),     static_cast<CK_ULONG>(vec_Ni.size()),
        vec_Nr.data(),     static_cast<CK_ULONG>(vec_Nr.size()),
        vec.key_number};

    // IKE_V1_APP_B, do quick mode (all session keys in one call).
    CK_NSS_IKE1_APP_B_PRF_DERIVE_PARAMS nss_ike_app_b_prf_params_quick = {
        prf_mech, CK_FALSE, CK_INVALID_HANDLE, vec_seed_data.data(),
        static_cast<CK_ULONG>(vec_seed_data.size())};

    // IKE_V1_APP_B, used for long session keys in ike_v1
    CK_MECHANISM_TYPE nss_ike_app_b_prf_params = prf_mech;

    // IKE_PRF_PLUS, used to generate session keys in ike v2
    CK_NSS_IKE_PRF_PLUS_DERIVE_PARAMS nss_ike_prf_plus_params = {
        prf_mech, CK_FALSE, CK_INVALID_HANDLE, vec_seed_data.data(),
        static_cast<CK_ULONG>(vec_seed_data.size())};

    SECItem params_item = {siBuffer, (unsigned char *)&nss_ike_prf_params,
                           sizeof(nss_ike_prf_params)};

    switch (vec.test_type) {
      case IkeTestType::ikeGxy:
        nss_ike_prf_params.bDataAsKey = true;
        break;
      case IkeTestType::ikeV1Psk:
        break;
      case IkeTestType::ikeV2Rekey:
        nss_ike_prf_params.bRekey = true;
        gxy_key = ImportKey(gxykm_item);
        nss_ike_prf_params.hNewKey = PK11_GetSymKeyHandle(gxy_key.get());
        break;
      case IkeTestType::ikeV1:
        derive_mech = CKM_NSS_IKE1_PRF_DERIVE;
        params_item.data = (unsigned char *)&nss_ike_v1_prf_params;
        params_item.len = sizeof(nss_ike_v1_prf_params);
        gxy_key = ImportKey(gxykm_item);
        nss_ike_v1_prf_params.hKeygxy = PK11_GetSymKeyHandle(gxy_key.get());
        if (prevkm_item.len != 0) {
          prev_key = ImportKey(prevkm_item);
          nss_ike_v1_prf_params.bHasPrevKey = true;
          nss_ike_v1_prf_params.hPrevKey = PK11_GetSymKeyHandle(prev_key.get());
        }
        break;
      case IkeTestType::ikeV1AppB:
        derive_mech = CKM_NSS_IKE1_APP_B_PRF_DERIVE;
        params_item.data = (unsigned char *)&nss_ike_app_b_prf_params;
        params_item.len = sizeof(nss_ike_app_b_prf_params);
        break;
      case IkeTestType::ikeV1AppBQuick:
        derive_mech = CKM_NSS_IKE1_APP_B_PRF_DERIVE;
        params_item.data = (unsigned char *)&nss_ike_app_b_prf_params_quick;
        params_item.len = sizeof(nss_ike_app_b_prf_params_quick);
        if (gxykm_item.len != 0) {
          gxy_key = ImportKey(gxykm_item);
          nss_ike_app_b_prf_params_quick.bHasKeygxy = true;
          nss_ike_app_b_prf_params_quick.hKeygxy =
              PK11_GetSymKeyHandle(gxy_key.get());
        }
        break;
      case IkeTestType::ikePlus:
        derive_mech = CKM_NSS_IKE_PRF_PLUS_DERIVE;
        params_item.data = (unsigned char *)&nss_ike_prf_plus_params;
        params_item.len = sizeof(nss_ike_prf_plus_params);
        break;
      default:
        ADD_FAILURE() << msg;
        return;
    }
    ASSERT_NE(nullptr, ikm) << msg;

    ScopedPK11SymKey okm = ScopedPK11SymKey(
        PK11_Derive(ikm.get(), derive_mech, &params_item,
                    CKM_GENERIC_SECRET_KEY_GEN, CKA_DERIVE, vec.size));
    if (vec.valid) {
      ASSERT_NE(nullptr, okm.get()) << msg;
      ASSERT_EQ(SECSuccess, PK11_ExtractKeyValue(okm.get())) << msg;
      SECItem *outItem = PK11_GetKeyData(okm.get());
      SECItem nullItem = {siBuffer, NULL, 0};
      if (outItem == NULL) {
        outItem = &nullItem;
      }
      ASSERT_EQ(0, SECITEM_CompareItem(&okm_item, PK11_GetKeyData(okm.get())))
          << msg << std::endl
          << " expect:" << DataBuffer(okm_item.data, okm_item.len) << std::endl
          << " calc'd:" << DataBuffer(outItem->data, outItem->len) << std::endl;
    } else {
      ASSERT_EQ(nullptr, okm.get()) << msg;
    }
  }
};

TEST_P(Pkcs11IkeTest, IkeproofVectors) {
  RunVectorTest(std::get<0>(GetParam()), std::get<1>(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    IkeSha1, Pkcs11IkeTest,
    ::testing::Combine(::testing::ValuesIn(kIkeSha1ProofVectors),
                       ::testing::Values(CKM_SHA_1_HMAC)));
INSTANTIATE_TEST_SUITE_P(
    IkeSha256, Pkcs11IkeTest,
    ::testing::Combine(::testing::ValuesIn(kIkeSha256ProofVectors),
                       ::testing::Values(CKM_SHA256_HMAC)));

INSTANTIATE_TEST_SUITE_P(
    IkeSha384, Pkcs11IkeTest,
    ::testing::Combine(::testing::ValuesIn(kIkeSha384ProofVectors),
                       ::testing::Values(CKM_SHA384_HMAC)));

INSTANTIATE_TEST_SUITE_P(
    IkeSha512, Pkcs11IkeTest,
    ::testing::Combine(::testing::ValuesIn(kIkeSha512ProofVectors),
                       ::testing::Values(CKM_SHA512_HMAC)));

INSTANTIATE_TEST_SUITE_P(
    IkeAESXCBC, Pkcs11IkeTest,
    ::testing::Combine(::testing::ValuesIn(kIkeAesXcbcProofVectors),
                       ::testing::Values(CKM_AES_XCBC_MAC)));

}  // namespace nss_test
