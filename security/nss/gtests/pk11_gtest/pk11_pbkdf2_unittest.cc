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

class Pkcs11Pbkdf2Test : public ::testing::Test {
 public:
  void Derive(std::vector<uint8_t>& derived, SECOidTag hash_alg) {
    // Shared between test vectors.
    const unsigned int kIterations = 4096;
    std::string pass("passwordPASSWORDpassword");
    std::string salt("saltSALTsaltSALTsaltSALTsaltSALTsalt");

    // Derivation must succeed with the right values.
    EXPECT_TRUE(DeriveBytes(pass, salt, derived, hash_alg, kIterations));

    // Derivation must fail when the password is bogus.
    std::string bogus_pass("PasswordPASSWORDpassword");
    EXPECT_FALSE(DeriveBytes(bogus_pass, salt, derived, hash_alg, kIterations));

    // Derivation must fail when the salt is bogus.
    std::string bogus_salt("SaltSALTsaltSALTsaltSALTsaltSALTsalt");
    EXPECT_FALSE(DeriveBytes(pass, bogus_salt, derived, hash_alg, kIterations));

    // Derivation must fail when using the wrong hash function.
    SECOidTag next_hash_alg = static_cast<SECOidTag>(hash_alg + 1);
    EXPECT_FALSE(DeriveBytes(pass, salt, derived, next_hash_alg, kIterations));

    // Derivation must fail when using the wrong number of kIterations.
    EXPECT_FALSE(DeriveBytes(pass, salt, derived, hash_alg, kIterations + 1));
  }

  void KeySizes(SECOidTag hash_alg) {
    // These tests will only validate the controls around the key sizes.
    // The resulting key is tested above, with valid key sizes.
    const unsigned int kIterations = 10;
    std::string pass("passwordPASSWORDpassword");
    std::string salt("saltSALTsaltSALTsaltSALTsaltSALTsalt");

    // Derivation must fail when using key sizes bigger than MAX_KEY_LEN.
    const int big_key_size = 768;
    EXPECT_FALSE(KeySizeParam(pass, salt, big_key_size, hash_alg, kIterations));

    // Zero is acceptable as key size and will be managed internally.
    const int zero_key_size = 0;
    EXPECT_TRUE(KeySizeParam(pass, salt, zero_key_size, hash_alg, kIterations));

    // -1 will be set to 0 internally and this means that the key size will be
    // obtained from the template. If the template doesn't have this defined,
    // it must fail.
    const int minus_key_size = -1;
    EXPECT_FALSE(
        KeySizeParam(pass, salt, minus_key_size, hash_alg, kIterations));

    // Lower than -1 is not allowed, as -1 means no keyLen defined.
    const int negative_key_size = -10;
    EXPECT_FALSE(
        KeySizeParam(pass, salt, negative_key_size, hash_alg, kIterations));
  }

 private:
  bool DeriveBytes(std::string& pass, std::string& salt,
                   std::vector<uint8_t>& derived, SECOidTag hash_alg,
                   unsigned int kIterations) {
    SECItem pass_item = {siBuffer, ToUcharPtr(pass),
                         static_cast<unsigned int>(pass.length())};
    SECItem salt_item = {siBuffer, ToUcharPtr(salt),
                         static_cast<unsigned int>(salt.length())};

    // Set up PBKDF2 params.
    ScopedSECAlgorithmID alg_id(
        PK11_CreatePBEV2AlgorithmID(SEC_OID_PKCS5_PBKDF2, hash_alg, hash_alg,
                                    derived.size(), kIterations, &salt_item));

    // Derive.
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    ScopedPK11SymKey sym_key(
        PK11_PBEKeyGen(slot.get(), alg_id.get(), &pass_item, false, nullptr));

    SECStatus rv = PK11_ExtractKeyValue(sym_key.get());
    EXPECT_EQ(rv, SECSuccess);

    SECItem* key_data = PK11_GetKeyData(sym_key.get());
    return !memcmp(&derived[0], key_data->data, key_data->len);
  }

  bool KeySizeParam(std::string& pass, std::string& salt, const int key_size,
                    SECOidTag hash_alg, unsigned int kIterations) {
    SECItem pass_item = {siBuffer, ToUcharPtr(pass),
                         static_cast<unsigned int>(pass.length())};
    SECItem salt_item = {siBuffer, ToUcharPtr(salt),
                         static_cast<unsigned int>(salt.length())};

    // Set up PBKDF2 params.
    ScopedSECAlgorithmID alg_id(
        PK11_CreatePBEV2AlgorithmID(SEC_OID_PKCS5_PBKDF2, hash_alg, hash_alg,
                                    key_size, kIterations, &salt_item));

    // Try to generate a key with the defined params.
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    ScopedPK11SymKey sym_key(
        PK11_PBEKeyGen(slot.get(), alg_id.get(), &pass_item, false, nullptr));

    // Should be nullptr if fail.
    return sym_key.get();
  }
};

// RFC 6070 <http://tools.ietf.org/html/rfc6070>
TEST_F(Pkcs11Pbkdf2Test, DeriveKnown1) {
  std::vector<uint8_t> derived = {0x3d, 0x2e, 0xec, 0x4f, 0xe4, 0x1c, 0x84,
                                  0x9b, 0x80, 0xc8, 0xd8, 0x36, 0x62, 0xc0,
                                  0xe4, 0x4a, 0x8b, 0x29, 0x1a, 0x96, 0x4c,
                                  0xf2, 0xf0, 0x70, 0x38};

  Derive(derived, SEC_OID_HMAC_SHA1);
}

// https://stackoverflow.com/questions/5130513/pbkdf2-hmac-sha2-test-vectors
TEST_F(Pkcs11Pbkdf2Test, DeriveKnown2) {
  std::vector<uint8_t> derived = {
      0x34, 0x8c, 0x89, 0xdb, 0xcb, 0xd3, 0x2b, 0x2f, 0x32, 0xd8,
      0x14, 0xb8, 0x11, 0x6e, 0x84, 0xcf, 0x2b, 0x17, 0x34, 0x7e,
      0xbc, 0x18, 0x00, 0x18, 0x1c, 0x4e, 0x2a, 0x1f, 0xb8, 0xdd,
      0x53, 0xe1, 0xc6, 0x35, 0x51, 0x8c, 0x7d, 0xac, 0x47, 0xe9};

  Derive(derived, SEC_OID_HMAC_SHA256);
}

TEST_F(Pkcs11Pbkdf2Test, KeyLenSizes) {
  // The size controls are regardless of the algorithms.
  KeySizes(SEC_OID_HMAC_SHA256);
}

}  // namespace nss_test
