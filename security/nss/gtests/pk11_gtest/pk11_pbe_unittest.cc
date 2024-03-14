/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "nss.h"
#include "pk11pub.h"

#include "gtest/gtest.h"
#include "nss_scoped_ptrs.h"

namespace nss_test {

static unsigned char* ToUcharPtr(std::string& str) {
  return const_cast<unsigned char*>(
      reinterpret_cast<const unsigned char*>(str.c_str()));
}

class Pkcs11PbeTest : public ::testing::Test {
 public:
  void Derive(std::vector<uint8_t>& derived) {
    // Shared between test vectors.
    const unsigned int kIterations = 4096;
    std::string pass("passwordPASSWORDpassword");
    std::string salt("saltSALTsaltSALTsaltSALTsaltSALTsalt");

    // Derivation must succeed with the right values.
    EXPECT_TRUE(DeriveBytes(pass, salt, derived, kIterations));
  }

 private:
  bool DeriveBytes(std::string& pass, std::string& salt,
                   std::vector<uint8_t>& derived, unsigned int kIterations) {
    SECItem pass_item = {siBuffer, ToUcharPtr(pass),
                         static_cast<unsigned int>(pass.length())};
    SECItem salt_item = {siBuffer, ToUcharPtr(salt),
                         static_cast<unsigned int>(salt.length())};

    // Set up PBE params.
    ScopedSECAlgorithmID alg_id(PK11_CreatePBEAlgorithmID(
        SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_2KEY_TRIPLE_DES_CBC, kIterations,
        &salt_item));

    // Derive.
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    ScopedPK11SymKey sym_key(
        PK11_PBEKeyGen(slot.get(), alg_id.get(), &pass_item, false, nullptr));

    SECStatus rv = PK11_ExtractKeyValue(sym_key.get());
    EXPECT_EQ(rv, SECSuccess);

    SECItem* key_data = PK11_GetKeyData(sym_key.get());

    return key_data->len == derived.size() &&
           !memcmp(&derived[0], key_data->data, key_data->len);
  }
};

TEST_F(Pkcs11PbeTest, DeriveKnown) {
  std::vector<uint8_t> derived = {0x86, 0x6b, 0xce, 0xef, 0x26, 0xa4,
                                  0x4f, 0x02, 0x4a, 0x26, 0xcd, 0xd0,
                                  0x4f, 0x7c, 0x19, 0xad};

  Derive(derived);
}

}  // namespace nss_test
