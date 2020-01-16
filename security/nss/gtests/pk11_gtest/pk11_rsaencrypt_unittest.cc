/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>

#include "cpputil.h"
#include "cryptohi.h"
#include "gtest/gtest.h"
#include "limits.h"
#include "nss.h"
#include "nss_scoped_ptrs.h"
#include "pk11pub.h"

namespace nss_test {

TEST(RsaEncryptTest, MessageLengths) {
  const uint8_t spki[] = {
      0x30, 0x81, 0x9f, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7,
      0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x81, 0x8d, 0x00, 0x30, 0x81,
      0x89, 0x02, 0x81, 0x81, 0x00, 0xf8, 0xb8, 0x6c, 0x83, 0xb4, 0xbc, 0xd9,
      0xa8, 0x57, 0xc0, 0xa5, 0xb4, 0x59, 0x76, 0x8c, 0x54, 0x1d, 0x79, 0xeb,
      0x22, 0x52, 0x04, 0x7e, 0xd3, 0x37, 0xeb, 0x41, 0xfd, 0x83, 0xf9, 0xf0,
      0xa6, 0x85, 0x15, 0x34, 0x75, 0x71, 0x5a, 0x84, 0xa8, 0x3c, 0xd2, 0xef,
      0x5a, 0x4e, 0xd3, 0xde, 0x97, 0x8a, 0xdd, 0xff, 0xbb, 0xcf, 0x0a, 0xaa,
      0x86, 0x92, 0xbe, 0xb8, 0x50, 0xe4, 0xcd, 0x6f, 0x80, 0x33, 0x30, 0x76,
      0x13, 0x8f, 0xca, 0x7b, 0xdc, 0xec, 0x5a, 0xca, 0x63, 0xc7, 0x03, 0x25,
      0xef, 0xa8, 0x8a, 0x83, 0x58, 0x76, 0x20, 0xfa, 0x16, 0x77, 0xd7, 0x79,
      0x92, 0x63, 0x01, 0x48, 0x1a, 0xd8, 0x7b, 0x67, 0xf1, 0x52, 0x55, 0x49,
      0x4e, 0xd6, 0x6e, 0x4a, 0x5c, 0xd7, 0x7a, 0x37, 0x36, 0x0c, 0xde, 0xdd,
      0x8f, 0x44, 0xe8, 0xc2, 0xa7, 0x2c, 0x2b, 0xb5, 0xaf, 0x64, 0x4b, 0x61,
      0x07, 0x02, 0x03, 0x01, 0x00, 0x01,
  };

  // Import public key (use pre-generated for performance).
  SECItem spki_item = {siBuffer, toUcharPtr(spki), sizeof(spki)};
  ScopedCERTSubjectPublicKeyInfo cert_spki(
      SECKEY_DecodeDERSubjectPublicKeyInfo(&spki_item));
  ASSERT_TRUE(cert_spki);
  ScopedSECKEYPublicKey pub_key(SECKEY_ExtractPublicKey(cert_spki.get()));
  ASSERT_TRUE(pub_key);

  int mod_len = SECKEY_PublicKeyStrength(pub_key.get());
  ASSERT_TRUE(mod_len > 0);

  std::vector<uint8_t> ctxt(mod_len);
  unsigned int ctxt_len;
  std::vector<uint8_t> msg(mod_len, 0xff);

  // Test with valid inputs
  SECStatus rv =
      PK11_PubEncrypt(pub_key.get(), CKM_RSA_PKCS, nullptr, ctxt.data(),
                      &ctxt_len, mod_len, msg.data(), 1, nullptr);
  ASSERT_EQ(SECSuccess, rv);

  // Maximum message length is mod_len - miniumum padding (8B) - flags (3B)
  unsigned int max_msg_len = mod_len - 8 - 3;
  rv = PK11_PubEncrypt(pub_key.get(), CKM_RSA_PKCS, nullptr, ctxt.data(),
                       &ctxt_len, mod_len, msg.data(), max_msg_len, nullptr);
  ASSERT_EQ(SECSuccess, rv);

  // Test one past maximum length
  rv =
      PK11_PubEncrypt(pub_key.get(), CKM_RSA_PKCS, nullptr, ctxt.data(),
                      &ctxt_len, mod_len, msg.data(), max_msg_len + 1, nullptr);
  ASSERT_EQ(SECFailure, rv);

  // Make sure the the length will not overflow - i.e.
  // (padLen = modulusLen - (UINT_MAX + MINIMUM_PAD_LEN)) may overflow and
  // result in a value that appears valid.
  rv = PK11_PubEncrypt(pub_key.get(), CKM_RSA_PKCS, nullptr, ctxt.data(),
                       &ctxt_len, UINT_MAX, msg.data(), UINT_MAX, nullptr);
  ASSERT_EQ(SECFailure, rv);
}
}
