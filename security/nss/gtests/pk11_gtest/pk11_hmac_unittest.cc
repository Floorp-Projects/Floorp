/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include <tuple>

#include "nss.h"
#include "pk11pub.h"
#include "secerr.h"
#include "sechash.h"
#include "blapi.h"
#include "gtest/gtest.h"
#include "nss_scoped_ptrs.h"
#include "testvectors/hmac-sha256-vectors.h"
#include "testvectors/hmac-sha384-vectors.h"
#include "testvectors/hmac-sha512-vectors.h"
#include "util.h"

namespace nss_test {

class Pkcs11HmacTest : public ::testing::TestWithParam<
                           std::tuple<HmacTestVector, CK_MECHANISM_TYPE>> {
 protected:
  void RunTestVector(const HmacTestVector &vec, CK_MECHANISM_TYPE mech) {
    std::string err = "Test #" + std::to_string(vec.id) + " failed";
    std::vector<uint8_t> vec_key = hex_string_to_bytes(vec.key);
    std::vector<uint8_t> vec_mac = hex_string_to_bytes(vec.tag);
    std::vector<uint8_t> vec_msg = hex_string_to_bytes(vec.msg);
    std::vector<uint8_t> output(vec_mac.size());

    // Don't provide a null pointer, even if the input is empty.
    uint8_t tmp;
    SECItem key = {siBuffer, vec_key.data() ? vec_key.data() : &tmp,
                   static_cast<unsigned int>(vec_key.size())};
    SECItem mac = {siBuffer, vec_mac.data() ? vec_mac.data() : &tmp,
                   static_cast<unsigned int>(vec_mac.size())};
    SECItem msg = {siBuffer, vec_msg.data() ? vec_msg.data() : &tmp,
                   static_cast<unsigned int>(vec_msg.size())};
    SECItem out = {siBuffer, output.data() ? output.data() : &tmp,
                   static_cast<unsigned int>(output.size())};

    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    ASSERT_NE(nullptr, slot) << err;

    ScopedPK11SymKey p11_key(PK11_ImportSymKey(
        slot.get(), mech, PK11_OriginUnwrap, CKA_SIGN, &key, nullptr));
    ASSERT_NE(nullptr, p11_key.get()) << err;

    SECStatus rv = PK11_SignWithSymKey(p11_key.get(), mech, NULL, &out, &msg);
    EXPECT_EQ(SECSuccess, rv) << err;
    EXPECT_EQ(!vec.invalid, 0 == SECITEM_CompareItem(&out, &mac)) << err;
  }
};

TEST_P(Pkcs11HmacTest, WycheproofVectors) {
  RunTestVector(std::get<0>(GetParam()), std::get<1>(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    HmacSha256, Pkcs11HmacTest,
    ::testing::Combine(::testing::ValuesIn(kHmacSha256WycheproofVectors),
                       ::testing::Values(CKM_SHA256_HMAC)));
INSTANTIATE_TEST_SUITE_P(
    HmacSha384, Pkcs11HmacTest,
    ::testing::Combine(::testing::ValuesIn(kHmacSha384WycheproofVectors),
                       ::testing::Values(CKM_SHA384_HMAC)));
INSTANTIATE_TEST_SUITE_P(
    HmacSha512, Pkcs11HmacTest,
    ::testing::Combine(::testing::ValuesIn(kHmacSha512WycheproofVectors),
                       ::testing::Values(CKM_SHA512_HMAC)));
}  // namespace nss_test
