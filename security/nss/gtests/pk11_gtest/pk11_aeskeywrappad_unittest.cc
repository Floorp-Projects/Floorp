/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "gtest/gtest.h"
#include "nss.h"
#include "nss_scoped_ptrs.h"
#include "pk11pub.h"

namespace nss_test {

class Pkcs11AESKeyWrapPadTest : public ::testing::Test {};

// Encrypt an ephemeral EC key (U2F use case)
TEST_F(Pkcs11AESKeyWrapPadTest, WrapUnwrapECKey) {
  const uint32_t kwrappedBufLen = 256;
  const uint32_t kPublicKeyLen = 65;
  const uint32_t kOidLen = 65;
  unsigned char param_buf[kOidLen];
  unsigned char unwrap_buf[kPublicKeyLen];

  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_NE(nullptr, slot);

  SECItem ecdsa_params = {siBuffer, param_buf, sizeof(param_buf)};
  SECOidData* oid_data = SECOID_FindOIDByTag(SEC_OID_SECG_EC_SECP256R1);
  ASSERT_NE(oid_data, nullptr);
  ecdsa_params.data[0] = SEC_ASN1_OBJECT_ID;
  ecdsa_params.data[1] = oid_data->oid.len;
  memcpy(ecdsa_params.data + 2, oid_data->oid.data, oid_data->oid.len);
  ecdsa_params.len = oid_data->oid.len + 2;

  SECKEYPublicKey* pub_tmp;
  ScopedSECKEYPublicKey pub_key;
  ScopedSECKEYPrivateKey priv_key(
      PK11_GenerateKeyPair(slot.get(), CKM_EC_KEY_PAIR_GEN, &ecdsa_params,
                           &pub_tmp, PR_FALSE, PR_TRUE, nullptr));
  ASSERT_NE(nullptr, priv_key);
  ASSERT_NE(nullptr, pub_tmp);
  pub_key.reset(pub_tmp);

  // Generate a KEK.
  ScopedPK11SymKey kek(
      PK11_KeyGen(slot.get(), CKM_AES_CBC, nullptr, 16, nullptr));
  ASSERT_NE(nullptr, kek);

  // Wrap the key
  ScopedSECItem wrapped(::SECITEM_AllocItem(nullptr, nullptr, kwrappedBufLen));
  ScopedSECItem param(PK11_ParamFromIV(CKM_NSS_AES_KEY_WRAP_PAD, nullptr));

  SECStatus rv = PK11_WrapPrivKey(slot.get(), kek.get(), priv_key.get(),
                                  CKM_NSS_AES_KEY_WRAP_PAD, param.get(),
                                  wrapped.get(), nullptr);
  ASSERT_EQ(rv, SECSuccess);

  SECItem pubKey = {siBuffer, unwrap_buf, kPublicKeyLen};
  CK_ATTRIBUTE_TYPE usages[] = {CKA_SIGN};
  int usageCount = 1;

  ScopedSECKEYPrivateKey unwrapped(
      PK11_UnwrapPrivKey(slot.get(), kek.get(), CKM_NSS_AES_KEY_WRAP_PAD,
                         param.get(), wrapped.get(), nullptr, &pubKey, false,
                         true, CKK_EC, usages, usageCount, nullptr));
  ASSERT_EQ(0, PORT_GetError());
  ASSERT_TRUE(!!unwrapped);

  // Try it with internal params allocation.
  SECKEYPrivateKey* tmp = PK11_UnwrapPrivKey(
      slot.get(), kek.get(), CKM_NSS_AES_KEY_WRAP_PAD, nullptr, wrapped.get(),
      nullptr, &pubKey, false, true, CKK_EC, usages, usageCount, nullptr);
  ASSERT_EQ(0, PORT_GetError());
  ASSERT_NE(nullptr, tmp);
  unwrapped.reset(tmp);
}

// Encrypt an ephemeral RSA key
TEST_F(Pkcs11AESKeyWrapPadTest, WrapUnwrapRsaKey) {
  const uint32_t kwrappedBufLen = 648;
  unsigned char unwrap_buf[kwrappedBufLen];

  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_NE(nullptr, slot);

  PK11RSAGenParams rsa_param;
  rsa_param.keySizeInBits = 1024;
  rsa_param.pe = 65537L;

  SECKEYPublicKey* pub_tmp;
  ScopedSECKEYPublicKey pub_key;
  ScopedSECKEYPrivateKey priv_key(
      PK11_GenerateKeyPair(slot.get(), CKM_RSA_PKCS_KEY_PAIR_GEN, &rsa_param,
                           &pub_tmp, PR_FALSE, PR_FALSE, nullptr));
  ASSERT_NE(nullptr, priv_key);
  ASSERT_NE(nullptr, pub_tmp);
  pub_key.reset(pub_tmp);

  // Generate a KEK.
  ScopedPK11SymKey kek(
      PK11_KeyGen(slot.get(), CKM_AES_CBC, nullptr, 16, nullptr));
  ASSERT_NE(nullptr, kek);

  // Wrap the key
  ScopedSECItem wrapped(::SECITEM_AllocItem(nullptr, nullptr, kwrappedBufLen));
  ScopedSECItem param(PK11_ParamFromIV(CKM_NSS_AES_KEY_WRAP_PAD, nullptr));

  SECStatus rv = PK11_WrapPrivKey(slot.get(), kek.get(), priv_key.get(),
                                  CKM_NSS_AES_KEY_WRAP_PAD, param.get(),
                                  wrapped.get(), nullptr);
  ASSERT_EQ(rv, SECSuccess);

  SECItem pubKey = {siBuffer, unwrap_buf, kwrappedBufLen};
  CK_ATTRIBUTE_TYPE usages[] = {CKA_SIGN};
  int usageCount = 1;

  ScopedSECKEYPrivateKey unwrapped(
      PK11_UnwrapPrivKey(slot.get(), kek.get(), CKM_NSS_AES_KEY_WRAP_PAD,
                         param.get(), wrapped.get(), nullptr, &pubKey, false,
                         false, CKK_EC, usages, usageCount, nullptr));
  ASSERT_EQ(0, PORT_GetError());
  ASSERT_TRUE(!!unwrapped);

  ScopedSECItem priv_key_data(
      PK11_ExportDERPrivateKeyInfo(priv_key.get(), nullptr));
  ScopedSECItem unwrapped_data(
      PK11_ExportDERPrivateKeyInfo(unwrapped.get(), nullptr));
  EXPECT_TRUE(!!priv_key_data);
  EXPECT_TRUE(!!unwrapped_data);
  ASSERT_EQ(priv_key_data->len, unwrapped_data->len);
  ASSERT_EQ(
      0, memcmp(priv_key_data->data, unwrapped_data->data, priv_key_data->len));
}

// Wrap a random that's a multiple of the block size, and compare the unwrap
// result.
TEST_F(Pkcs11AESKeyWrapPadTest, WrapUnwrapRandom_EvenBlock) {
  const uint32_t kInputKeyLen = 128;
  uint32_t out_len = 0;
  std::vector<unsigned char> input_key(kInputKeyLen);
  std::vector<unsigned char> wrapped_key(
      kInputKeyLen + AES_BLOCK_SIZE);  // One block of padding
  std::vector<unsigned char> unwrapped_key(
      kInputKeyLen + AES_BLOCK_SIZE);  // One block of padding

  // Generate input key material
  SECStatus rv = PK11_GenerateRandom(input_key.data(), input_key.size());
  EXPECT_EQ(SECSuccess, rv);

  // Generate a KEK.
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_NE(nullptr, slot);
  ScopedPK11SymKey kek(
      PK11_KeyGen(slot.get(), CKM_AES_CBC, nullptr, 16, nullptr));
  ASSERT_NE(nullptr, kek);

  // Wrap the key
  rv = PK11_Encrypt(kek.get(), CKM_NSS_AES_KEY_WRAP_PAD, /* param */ nullptr,
                    wrapped_key.data(), &out_len,
                    static_cast<unsigned int>(wrapped_key.size()),
                    input_key.data(),
                    static_cast<unsigned int>(input_key.size()));
  ASSERT_EQ(SECSuccess, rv);

  rv = PK11_Decrypt(kek.get(), CKM_NSS_AES_KEY_WRAP_PAD, /* param */ nullptr,
                    unwrapped_key.data(), &out_len,
                    static_cast<unsigned int>(unwrapped_key.size()),
                    wrapped_key.data(), out_len);
  ASSERT_EQ(SECSuccess, rv);
  ASSERT_EQ(input_key.size(), out_len);
  ASSERT_EQ(0, memcmp(input_key.data(), unwrapped_key.data(), out_len));
}

// Wrap a random that's NOT a multiple of the block size, and compare the unwrap
// result.
TEST_F(Pkcs11AESKeyWrapPadTest, WrapUnwrapRandom_OddBlock1) {
  const uint32_t kInputKeyLen = 65;
  uint32_t out_len = 0;
  std::vector<unsigned char> input_key(kInputKeyLen);
  std::vector<unsigned char> wrapped_key(
      kInputKeyLen + AES_BLOCK_SIZE);  // One block of padding
  std::vector<unsigned char> unwrapped_key(
      kInputKeyLen + AES_BLOCK_SIZE);  // One block of padding

  // Generate input key material
  SECStatus rv = PK11_GenerateRandom(input_key.data(), input_key.size());
  EXPECT_EQ(SECSuccess, rv);

  // Generate a KEK.
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_NE(nullptr, slot);
  ScopedPK11SymKey kek(
      PK11_KeyGen(slot.get(), CKM_AES_CBC, nullptr, 16, nullptr));
  ASSERT_NE(nullptr, kek);

  // Wrap the key
  rv = PK11_Encrypt(kek.get(), CKM_NSS_AES_KEY_WRAP_PAD, /* param */ nullptr,
                    wrapped_key.data(), &out_len,
                    static_cast<unsigned int>(wrapped_key.size()),
                    input_key.data(),
                    static_cast<unsigned int>(input_key.size()));
  ASSERT_EQ(SECSuccess, rv);

  rv = PK11_Decrypt(kek.get(), CKM_NSS_AES_KEY_WRAP_PAD, /* param */ nullptr,
                    unwrapped_key.data(), &out_len,
                    static_cast<unsigned int>(unwrapped_key.size()),
                    wrapped_key.data(), out_len);
  ASSERT_EQ(SECSuccess, rv);
  ASSERT_EQ(input_key.size(), out_len);
  ASSERT_EQ(0, memcmp(input_key.data(), unwrapped_key.data(), out_len));
}

// Wrap a random that's NOT a multiple of the block size, and compare the unwrap
// result.
TEST_F(Pkcs11AESKeyWrapPadTest, WrapUnwrapRandom_OddBlock2) {
  const uint32_t kInputKeyLen = 63;
  uint32_t out_len = 0;
  std::vector<unsigned char> input_key(kInputKeyLen);
  std::vector<unsigned char> wrapped_key(
      kInputKeyLen + AES_BLOCK_SIZE);  // One block of padding
  std::vector<unsigned char> unwrapped_key(
      kInputKeyLen + AES_BLOCK_SIZE);  // One block of padding

  // Generate input key material
  SECStatus rv = PK11_GenerateRandom(input_key.data(), input_key.size());
  EXPECT_EQ(SECSuccess, rv);

  // Generate a KEK.
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_NE(nullptr, slot);
  ScopedPK11SymKey kek(
      PK11_KeyGen(slot.get(), CKM_AES_CBC, nullptr, 16, nullptr));
  ASSERT_NE(nullptr, kek);

  // Wrap the key
  rv = PK11_Encrypt(kek.get(), CKM_NSS_AES_KEY_WRAP_PAD, /* param */ nullptr,
                    wrapped_key.data(), &out_len, wrapped_key.size(),
                    input_key.data(), input_key.size());
  ASSERT_EQ(SECSuccess, rv);

  rv = PK11_Decrypt(kek.get(), CKM_NSS_AES_KEY_WRAP_PAD, /* param */ nullptr,
                    unwrapped_key.data(), &out_len,
                    static_cast<unsigned int>(unwrapped_key.size()),
                    wrapped_key.data(), out_len);
  ASSERT_EQ(SECSuccess, rv);
  ASSERT_EQ(input_key.size(), out_len);
  ASSERT_EQ(0, memcmp(input_key.data(), unwrapped_key.data(), out_len));
}

// Invalid long padding (over the block size, but otherwise valid)
TEST_F(Pkcs11AESKeyWrapPadTest, WrapUnwrapRandom_PaddingTooLong) {
  const uint32_t kInputKeyLen = 32;
  uint32_t out_len = 0;

  // Apply our own padding
  const unsigned char buf[32] = {
      0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
      0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
      0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};
  std::vector<unsigned char> wrapped_key(kInputKeyLen + AES_BLOCK_SIZE);
  std::vector<unsigned char> unwrapped_key(kInputKeyLen + AES_BLOCK_SIZE);

  // Generate a KEK.
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_NE(nullptr, slot);
  ScopedPK11SymKey kek(
      PK11_KeyGen(slot.get(), CKM_AES_CBC, nullptr, 16, nullptr));
  ASSERT_NE(nullptr, kek);

  // Wrap the key
  SECStatus rv =
      PK11_Encrypt(kek.get(), CKM_NSS_AES_KEY_WRAP,  // Don't apply more padding
                   /* param */ nullptr, wrapped_key.data(), &out_len,
                   wrapped_key.size(), buf, sizeof(buf));
  ASSERT_EQ(SECSuccess, rv);

  rv = PK11_Decrypt(kek.get(), CKM_NSS_AES_KEY_WRAP_PAD, /* param */ nullptr,
                    unwrapped_key.data(), &out_len,
                    static_cast<unsigned int>(unwrapped_key.size()),
                    wrapped_key.data(), out_len);
  ASSERT_EQ(SECFailure, rv);
}

// Invalid 0-length padding (there should be a full block if the message doesn't
// need to be padded)
TEST_F(Pkcs11AESKeyWrapPadTest, WrapUnwrapRandom_NoPadding) {
  const uint32_t kInputKeyLen = 32;
  uint32_t out_len = 0;

  // Apply our own padding
  const unsigned char buf[32] = {0};
  std::vector<unsigned char> wrapped_key(kInputKeyLen + AES_BLOCK_SIZE);
  std::vector<unsigned char> unwrapped_key(kInputKeyLen + AES_BLOCK_SIZE);

  // Generate a KEK.
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_NE(nullptr, slot);
  ScopedPK11SymKey kek(
      PK11_KeyGen(slot.get(), CKM_AES_CBC, nullptr, 16, nullptr));
  ASSERT_NE(nullptr, kek);

  // Wrap the key
  SECStatus rv =
      PK11_Encrypt(kek.get(), CKM_NSS_AES_KEY_WRAP,  // Don't apply more padding
                   /* param */ nullptr, wrapped_key.data(), &out_len,
                   wrapped_key.size(), buf, sizeof(buf));
  ASSERT_EQ(SECSuccess, rv);

  rv = PK11_Decrypt(kek.get(), CKM_NSS_AES_KEY_WRAP_PAD, /* param */ nullptr,
                    unwrapped_key.data(), &out_len,
                    static_cast<unsigned int>(unwrapped_key.size()),
                    wrapped_key.data(), out_len);
  ASSERT_EQ(SECFailure, rv);
}

// Invalid padding
TEST_F(Pkcs11AESKeyWrapPadTest, WrapUnwrapRandom_BadPadding1) {
  const uint32_t kInputKeyLen = 32;
  uint32_t out_len = 0;

  // Apply our own padding
  const unsigned char buf[32] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08,
                                 0x08, 0x08, 0x08, 0x08};  // Check all 8 bytes
  std::vector<unsigned char> wrapped_key(kInputKeyLen + AES_BLOCK_SIZE);
  std::vector<unsigned char> unwrapped_key(kInputKeyLen + AES_BLOCK_SIZE);

  // Generate a KEK.
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_NE(nullptr, slot);
  ScopedPK11SymKey kek(
      PK11_KeyGen(slot.get(), CKM_AES_CBC, nullptr, 16, nullptr));
  ASSERT_NE(nullptr, kek);

  // Wrap the key
  SECStatus rv =
      PK11_Encrypt(kek.get(), CKM_NSS_AES_KEY_WRAP,  // Don't apply more padding
                   /* param */ nullptr, wrapped_key.data(), &out_len,
                   wrapped_key.size(), buf, sizeof(buf));
  ASSERT_EQ(SECSuccess, rv);

  rv = PK11_Decrypt(kek.get(), CKM_NSS_AES_KEY_WRAP_PAD, /* param */ nullptr,
                    unwrapped_key.data(), &out_len,
                    static_cast<unsigned int>(unwrapped_key.size()),
                    wrapped_key.data(), out_len);
  ASSERT_EQ(SECFailure, rv);
}

// Invalid padding
TEST_F(Pkcs11AESKeyWrapPadTest, WrapUnwrapRandom_BadPadding2) {
  const uint32_t kInputKeyLen = 32;
  uint32_t out_len = 0;

  // Apply our own padding
  const unsigned char
      buf[32] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x01, 0x02};  // Check first loop repeat
  std::vector<unsigned char> wrapped_key(kInputKeyLen + AES_BLOCK_SIZE);
  std::vector<unsigned char> unwrapped_key(kInputKeyLen + AES_BLOCK_SIZE);

  // Generate a KEK.
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_NE(nullptr, slot);
  ScopedPK11SymKey kek(
      PK11_KeyGen(slot.get(), CKM_AES_CBC, nullptr, 16, nullptr));
  ASSERT_NE(nullptr, kek);

  // Wrap the key
  SECStatus rv =
      PK11_Encrypt(kek.get(), CKM_NSS_AES_KEY_WRAP,  // Don't apply more padding
                   /* param */ nullptr, wrapped_key.data(), &out_len,
                   wrapped_key.size(), buf, sizeof(buf));
  ASSERT_EQ(SECSuccess, rv);

  rv = PK11_Decrypt(kek.get(), CKM_NSS_AES_KEY_WRAP_PAD, /* param */ nullptr,
                    unwrapped_key.data(), &out_len,
                    static_cast<unsigned int>(unwrapped_key.size()),
                    wrapped_key.data(), out_len);
  ASSERT_EQ(SECFailure, rv);
}

// Minimum valid padding
TEST_F(Pkcs11AESKeyWrapPadTest, WrapUnwrapRandom_ShortValidPadding) {
  const uint32_t kInputKeyLen = 32;
  uint32_t out_len = 0;

  // Apply our own padding
  const unsigned char buf[kInputKeyLen] = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};  // Minimum
  std::vector<unsigned char> wrapped_key(kInputKeyLen + AES_BLOCK_SIZE);
  std::vector<unsigned char> unwrapped_key(kInputKeyLen + AES_BLOCK_SIZE);

  // Generate a KEK.
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_NE(nullptr, slot);
  ScopedPK11SymKey kek(
      PK11_KeyGen(slot.get(), CKM_AES_CBC, nullptr, 16, nullptr));
  ASSERT_NE(nullptr, kek);

  // Wrap the key
  SECStatus rv =
      PK11_Encrypt(kek.get(), CKM_NSS_AES_KEY_WRAP,  // Don't apply more padding
                   /* param */ nullptr, wrapped_key.data(), &out_len,
                   wrapped_key.size(), buf, sizeof(buf));
  ASSERT_EQ(SECSuccess, rv);

  rv = PK11_Decrypt(kek.get(), CKM_NSS_AES_KEY_WRAP_PAD, /* param */ nullptr,
                    unwrapped_key.data(), &out_len,
                    static_cast<unsigned int>(unwrapped_key.size()),
                    wrapped_key.data(), out_len);
  ASSERT_EQ(SECSuccess, rv);
  ASSERT_EQ(kInputKeyLen - 1, out_len);
  ASSERT_EQ(0, memcmp(buf, unwrapped_key.data(), out_len));
}

}  // namespace nss_test
