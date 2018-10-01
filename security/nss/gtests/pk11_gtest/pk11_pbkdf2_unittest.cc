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
    const unsigned int iterations = 4096;
    std::string pass("passwordPASSWORDpassword");
    std::string salt("saltSALTsaltSALTsaltSALTsaltSALTsalt");

    // Derivation must succeed with the right values.
    EXPECT_TRUE(DeriveBytes(pass, salt, derived, hash_alg, iterations));

    // Derivation must fail when the password is bogus.
    std::string bogusPass("PasswordPASSWORDpassword");
    EXPECT_FALSE(DeriveBytes(bogusPass, salt, derived, hash_alg, iterations));

    // Derivation must fail when the salt is bogus.
    std::string bogusSalt("SaltSALTsaltSALTsaltSALTsaltSALTsalt");
    EXPECT_FALSE(DeriveBytes(pass, bogusSalt, derived, hash_alg, iterations));

    // Derivation must fail when using the wrong hash function.
    SECOidTag next_hash_alg = static_cast<SECOidTag>(hash_alg + 1);
    EXPECT_FALSE(DeriveBytes(pass, salt, derived, next_hash_alg, iterations));

    // Derivation must fail when using the wrong number of iterations.
    EXPECT_FALSE(DeriveBytes(pass, salt, derived, hash_alg, iterations + 1));
  }

 private:
  bool DeriveBytes(std::string& pass, std::string& salt,
                   std::vector<uint8_t>& derived, SECOidTag hash_alg,
                   unsigned int iterations) {
    SECItem passItem = {siBuffer, ToUcharPtr(pass),
                        static_cast<unsigned int>(pass.length())};
    SECItem saltItem = {siBuffer, ToUcharPtr(salt),
                        static_cast<unsigned int>(salt.length())};

    // Set up PBKDF2 params.
    ScopedSECAlgorithmID alg_id(
        PK11_CreatePBEV2AlgorithmID(SEC_OID_PKCS5_PBKDF2, hash_alg, hash_alg,
                                    derived.size(), iterations, &saltItem));

    // Derive.
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    ScopedPK11SymKey symKey(
        PK11_PBEKeyGen(slot.get(), alg_id.get(), &passItem, false, nullptr));

    SECStatus rv = PK11_ExtractKeyValue(symKey.get());
    EXPECT_EQ(rv, SECSuccess);

    SECItem* keyData = PK11_GetKeyData(symKey.get());
    return !memcmp(&derived[0], keyData->data, keyData->len);
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

}  // namespace nss_test
