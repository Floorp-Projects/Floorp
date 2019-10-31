/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "nss.h"
#include "pk11pub.h"
#include "secerr.h"

#include "nss_scoped_ptrs.h"
#include "gtest/gtest.h"

namespace nss_test {

static const uint8_t kInput[99] = {1, 2, 3};
static const uint8_t kKeyData[24] = {'K', 'E', 'Y'};

static SECItem* GetIv() {
  static const uint8_t kIvData[16] = {'I', 'V'};
  static const SECItem kIv = {siBuffer, const_cast<uint8_t*>(kIvData),
                              static_cast<unsigned int>(sizeof(kIvData))};
  return const_cast<SECItem*>(&kIv);
}

class Pkcs11CbcPadTest : public ::testing::TestWithParam<CK_MECHANISM_TYPE> {
 protected:
  bool is_padded() const {
    switch (GetParam()) {
      case CKM_AES_CBC_PAD:
      case CKM_DES3_CBC_PAD:
        return true;

      case CKM_AES_CBC:
      case CKM_DES3_CBC:
        return false;

      default:
        ADD_FAILURE() << "Unknown mechanism " << GetParam();
    }
    return false;
  }

  uint32_t GetUnpaddedMechanism() const {
    switch (GetParam()) {
      case CKM_AES_CBC_PAD:
        return CKM_AES_CBC;
      case CKM_DES3_CBC_PAD:
        return CKM_DES3_CBC;
      default:
        ADD_FAILURE() << "Unknown padded mechanism " << GetParam();
    }
    return 0;
  }

  size_t block_size() const {
    return static_cast<size_t>(PK11_GetBlockSize(GetParam(), nullptr));
  }

  size_t GetInputLen(CK_ATTRIBUTE_TYPE op) const {
    if (is_padded() && op == CKA_ENCRYPT) {
      // Anything goes for encryption when padded.
      return sizeof(kInput);
    }

    // Otherwise, use a strict multiple of the block size.
    size_t block_count = sizeof(kInput) / block_size();
    EXPECT_LT(1U, block_count) << "need 2 blocks for tests";
    return block_count * block_size();
  }

  ScopedPK11SymKey MakeKey(CK_ATTRIBUTE_TYPE op) {
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    EXPECT_NE(nullptr, slot);
    if (!slot) {
      return nullptr;
    }

    unsigned int key_len = 0;
    switch (GetParam()) {
      case CKM_AES_CBC_PAD:
      case CKM_AES_CBC:
        key_len = 16;  // This doesn't do AES-256 to keep it simple.
        break;

      case CKM_DES3_CBC_PAD:
      case CKM_DES3_CBC:
        key_len = 24;
        break;

      default:
        ADD_FAILURE() << "Unknown mechanism " << GetParam();
        return nullptr;
    }

    SECItem key_item = {siBuffer, const_cast<uint8_t*>(kKeyData), key_len};
    PK11SymKey* p = PK11_ImportSymKey(slot.get(), GetParam(), PK11_OriginUnwrap,
                                      op, &key_item, nullptr);
    EXPECT_NE(nullptr, p);
    return ScopedPK11SymKey(p);
  }

  ScopedPK11Context MakeContext(CK_ATTRIBUTE_TYPE op) {
    ScopedPK11SymKey k = MakeKey(op);
    PK11Context* ctx =
        PK11_CreateContextBySymKey(GetParam(), op, k.get(), GetIv());
    EXPECT_NE(nullptr, ctx);
    return ScopedPK11Context(ctx);
  }
};

TEST_P(Pkcs11CbcPadTest, EncryptDecrypt) {
  uint8_t encrypted[sizeof(kInput) + 64];  // Allow for padding and expansion.
  size_t input_len = GetInputLen(CKA_ENCRYPT);

  ScopedPK11SymKey ek = MakeKey(CKA_ENCRYPT);
  unsigned int encrypted_len = 0;
  SECStatus rv =
      PK11_Encrypt(ek.get(), GetParam(), GetIv(), encrypted, &encrypted_len,
                   sizeof(encrypted), kInput, input_len);
  ASSERT_EQ(SECSuccess, rv);
  EXPECT_LE(input_len, static_cast<size_t>(encrypted_len));

  // Though the decrypted result can't be larger than the input we provided,
  // NSS needs extra space to put the padding in.
  uint8_t decrypted[sizeof(kInput) + 64];
  unsigned int decrypted_len = 0;
  ScopedPK11SymKey dk = MakeKey(CKA_DECRYPT);
  rv = PK11_Decrypt(dk.get(), GetParam(), GetIv(), decrypted, &decrypted_len,
                    sizeof(decrypted), encrypted, encrypted_len);
  ASSERT_EQ(SECSuccess, rv);
  EXPECT_EQ(input_len, static_cast<size_t>(decrypted_len));
  EXPECT_EQ(0, memcmp(kInput, decrypted, input_len));
}

TEST_P(Pkcs11CbcPadTest, ContextEncryptDecrypt) {
  uint8_t encrypted[sizeof(kInput) + 64];  // Allow for padding and expansion.
  size_t input_len = GetInputLen(CKA_ENCRYPT);

  ScopedPK11Context ectx = MakeContext(CKA_ENCRYPT);
  int encrypted_len = 0;
  SECStatus rv = PK11_CipherOp(ectx.get(), encrypted, &encrypted_len,
                               sizeof(encrypted), kInput, input_len);
  ASSERT_EQ(SECSuccess, rv);
  EXPECT_LE(0, encrypted_len);  // Stupid signed parameters.

  unsigned int final_len = 0;
  rv = PK11_CipherFinal(ectx.get(), encrypted + encrypted_len, &final_len,
                        sizeof(encrypted) - encrypted_len);
  ASSERT_EQ(SECSuccess, rv);
  encrypted_len += final_len;
  EXPECT_LE(input_len, static_cast<size_t>(encrypted_len));

  uint8_t decrypted[sizeof(kInput) + 64];
  int decrypted_len = 0;
  ScopedPK11Context dctx = MakeContext(CKA_DECRYPT);
  rv = PK11_CipherOp(dctx.get(), decrypted, &decrypted_len, sizeof(decrypted),
                     encrypted, encrypted_len);
  ASSERT_EQ(SECSuccess, rv);
  EXPECT_LE(0, decrypted_len);

  rv = PK11_CipherFinal(dctx.get(), decrypted + decrypted_len, &final_len,
                        sizeof(decrypted) - decrypted_len);
  ASSERT_EQ(SECSuccess, rv);
  decrypted_len += final_len;
  EXPECT_EQ(input_len, static_cast<size_t>(decrypted_len));
  EXPECT_EQ(0, memcmp(kInput, decrypted, input_len));
}

TEST_P(Pkcs11CbcPadTest, ContextEncryptDecryptTwoParts) {
  uint8_t encrypted[sizeof(kInput) + 64];
  size_t input_len = GetInputLen(CKA_ENCRYPT);

  ScopedPK11Context ectx = MakeContext(CKA_ENCRYPT);
  int first_len = 0;
  SECStatus rv = PK11_CipherOp(ectx.get(), encrypted, &first_len,
                               sizeof(encrypted), kInput, block_size());
  ASSERT_EQ(SECSuccess, rv);
  ASSERT_LE(0, first_len);

  int second_len = 0;
  rv = PK11_CipherOp(ectx.get(), encrypted + first_len, &second_len,
                     sizeof(encrypted) - first_len, kInput + block_size(),
                     input_len - block_size());
  ASSERT_EQ(SECSuccess, rv);
  ASSERT_LE(0, second_len);

  unsigned int final_len = 0;
  rv = PK11_CipherFinal(ectx.get(), encrypted + first_len + second_len,
                        &final_len, sizeof(encrypted) - first_len - second_len);
  ASSERT_EQ(SECSuccess, rv);
  unsigned int encrypted_len = first_len + second_len + final_len;
  ASSERT_LE(input_len, static_cast<size_t>(encrypted_len));

  // Now decrypt this in a similar fashion.
  uint8_t decrypted[sizeof(kInput) + 64];
  ScopedPK11Context dctx = MakeContext(CKA_DECRYPT);
  rv = PK11_CipherOp(dctx.get(), decrypted, &first_len, sizeof(decrypted),
                     encrypted, block_size());
  ASSERT_EQ(SECSuccess, rv);
  EXPECT_LE(0, first_len);

  rv = PK11_CipherOp(dctx.get(), decrypted + first_len, &second_len,
                     sizeof(decrypted) - first_len, encrypted + block_size(),
                     encrypted_len - block_size());
  ASSERT_EQ(SECSuccess, rv);
  EXPECT_LE(0, second_len);

  unsigned int decrypted_len = 0;
  rv = PK11_CipherFinal(dctx.get(), decrypted + first_len + second_len,
                        &decrypted_len,
                        sizeof(decrypted) - first_len - second_len);
  ASSERT_EQ(SECSuccess, rv);
  decrypted_len += first_len + second_len;
  EXPECT_EQ(input_len, static_cast<size_t>(decrypted_len));
  EXPECT_EQ(0, memcmp(kInput, decrypted, input_len));
}

TEST_P(Pkcs11CbcPadTest, FailDecryptSimple) {
  ScopedPK11SymKey dk = MakeKey(CKA_DECRYPT);
  uint8_t output[sizeof(kInput) + 64];
  unsigned int output_len = 999;
  SECStatus rv =
      PK11_Decrypt(dk.get(), GetParam(), GetIv(), output, &output_len,
                   sizeof(output), kInput, GetInputLen(CKA_DECRYPT));
  if (is_padded()) {
    EXPECT_EQ(SECFailure, rv);
    EXPECT_EQ(999U, output_len);
  } else {
    // Unpadded decryption can't really fail.
    EXPECT_EQ(SECSuccess, rv);
  }
}

TEST_P(Pkcs11CbcPadTest, FailEncryptSimple) {
  ScopedPK11SymKey ek = MakeKey(CKA_ENCRYPT);
  uint8_t output[3];  // Too small for anything.
  unsigned int output_len = 333;

  SECStatus rv =
      PK11_Encrypt(ek.get(), GetParam(), GetIv(), output, &output_len,
                   sizeof(output), kInput, GetInputLen(CKA_ENCRYPT));
  EXPECT_EQ(SECFailure, rv);
  EXPECT_EQ(333U, output_len);
}

// It's a bit of a lie to put this in pk11_cbc_unittest, since we
// also test bounds checking in other modes. There doesn't seem
// to be an appropriately-generic place elsewhere.
TEST_F(Pkcs11CbcPadTest, FailEncryptShortParam) {
  SECStatus rv = SECFailure;
  uint8_t encrypted[sizeof(kInput)];
  unsigned int encrypted_len = 0;
  size_t input_len = AES_BLOCK_SIZE;

  // CK_GCM_PARAMS is the largest param struct used across AES modes
  uint8_t param_buf[sizeof(CK_GCM_PARAMS)];
  SECItem param = {siBuffer, param_buf, sizeof(param_buf)};
  SECItem key_item = {siBuffer, const_cast<uint8_t*>(kKeyData), 16};

  // Setup (we use the ECB key for other modes)
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_NE(nullptr, slot);
  ScopedPK11SymKey key(PK11_ImportSymKey(slot.get(), CKM_AES_ECB,
                                         PK11_OriginUnwrap, CKA_ENCRYPT,
                                         &key_item, nullptr));
  ASSERT_TRUE(key.get());

  // CTR should have a CK_AES_CTR_PARAMS
  param.len = sizeof(CK_AES_CTR_PARAMS) - 1;
  rv = PK11_Encrypt(key.get(), CKM_AES_CTR, &param, encrypted, &encrypted_len,
                    sizeof(encrypted), kInput, input_len);
  EXPECT_EQ(SECFailure, rv);

  param.len++;
  reinterpret_cast<CK_AES_CTR_PARAMS*>(param.data)->ulCounterBits = 32;
  rv = PK11_Encrypt(key.get(), CKM_AES_CTR, &param, encrypted, &encrypted_len,
                    sizeof(encrypted), kInput, input_len);
  EXPECT_EQ(SECSuccess, rv);

  // GCM should have a CK_GCM_PARAMS
  param.len = sizeof(CK_GCM_PARAMS) - 1;
  rv = PK11_Encrypt(key.get(), CKM_AES_GCM, &param, encrypted, &encrypted_len,
                    sizeof(encrypted), kInput, input_len);
  EXPECT_EQ(SECFailure, rv);

  param.len++;
  reinterpret_cast<CK_GCM_PARAMS*>(param.data)->pIv = param_buf;
  reinterpret_cast<CK_GCM_PARAMS*>(param.data)->ulIvLen = 12;
  reinterpret_cast<CK_GCM_PARAMS*>(param.data)->pAAD = nullptr;
  reinterpret_cast<CK_GCM_PARAMS*>(param.data)->ulAADLen = 0;
  reinterpret_cast<CK_GCM_PARAMS*>(param.data)->ulTagBits = 128;
  rv = PK11_Encrypt(key.get(), CKM_AES_GCM, &param, encrypted, &encrypted_len,
                    sizeof(encrypted), kInput, input_len);
  EXPECT_EQ(SECSuccess, rv);

  // CBC should have a 16B IV
  param.len = AES_BLOCK_SIZE - 1;
  rv = PK11_Encrypt(key.get(), CKM_AES_CBC, &param, encrypted, &encrypted_len,
                    sizeof(encrypted), kInput, input_len);
  EXPECT_EQ(SECFailure, rv);

  param.len++;
  rv = PK11_Encrypt(key.get(), CKM_AES_CBC, &param, encrypted, &encrypted_len,
                    sizeof(encrypted), kInput, input_len);
  EXPECT_EQ(SECSuccess, rv);

  // CTS
  param.len = AES_BLOCK_SIZE - 1;
  rv = PK11_Encrypt(key.get(), CKM_AES_CTS, &param, encrypted, &encrypted_len,
                    sizeof(encrypted), kInput, input_len);
  EXPECT_EQ(SECFailure, rv);

  param.len++;
  rv = PK11_Encrypt(key.get(), CKM_AES_CTS, &param, encrypted, &encrypted_len,
                    sizeof(encrypted), kInput, input_len);
  EXPECT_EQ(SECSuccess, rv);
}

TEST_P(Pkcs11CbcPadTest, ContextFailDecryptSimple) {
  ScopedPK11Context dctx = MakeContext(CKA_DECRYPT);
  uint8_t output[sizeof(kInput) + 64];
  int output_len = 77;

  SECStatus rv = PK11_CipherOp(dctx.get(), output, &output_len, sizeof(output),
                               kInput, GetInputLen(CKA_DECRYPT));
  EXPECT_EQ(SECSuccess, rv);
  EXPECT_LE(0, output_len) << "this is not an AEAD, so content leaks";

  unsigned int final_len = 88;
  rv = PK11_CipherFinal(dctx.get(), output, &final_len, sizeof(output));
  if (is_padded()) {
    EXPECT_EQ(SECFailure, rv);
    ASSERT_EQ(88U, final_len) << "final_len should be untouched";
  } else {
    // Unpadded decryption can't really fail.
    EXPECT_EQ(SECSuccess, rv);
  }
}

TEST_P(Pkcs11CbcPadTest, ContextFailDecryptInvalidBlockSize) {
  ScopedPK11Context dctx = MakeContext(CKA_DECRYPT);
  uint8_t output[sizeof(kInput) + 64];
  int output_len = 888;

  SECStatus rv = PK11_CipherOp(dctx.get(), output, &output_len, sizeof(output),
                               kInput, GetInputLen(CKA_DECRYPT) - 1);
  EXPECT_EQ(SECFailure, rv);
  // Because PK11_CipherOp is partial, it can return data on failure.
  // This means that it needs to reset its output length to 0 when it starts.
  EXPECT_EQ(0, output_len) << "output_len is reset";
}

TEST_P(Pkcs11CbcPadTest, EncryptDecrypt_PaddingTooLong) {
  if (!is_padded()) {
    return;
  }

  // Padding that's over the block size
  const std::vector<uint8_t> input = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20,
      0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
      0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
      0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};
  std::vector<uint8_t> encrypted(input.size());
  uint32_t encrypted_len = 0;

  ScopedPK11SymKey ek = MakeKey(CKA_ENCRYPT);
  SECStatus rv = PK11_Encrypt(ek.get(), GetUnpaddedMechanism(), GetIv(),
                              encrypted.data(), &encrypted_len,
                              encrypted.size(), input.data(), input.size());
  ASSERT_EQ(SECSuccess, rv);
  EXPECT_EQ(input.size(), encrypted_len);

  std::vector<uint8_t> decrypted(input.size());
  uint32_t decrypted_len = 0;
  ScopedPK11SymKey dk = MakeKey(CKA_DECRYPT);
  rv = PK11_Decrypt(dk.get(), GetParam(), GetIv(), decrypted.data(),
                    &decrypted_len, decrypted.size(), encrypted.data(),
                    encrypted_len);
  EXPECT_EQ(SECFailure, rv);
  EXPECT_EQ(0U, decrypted_len);
}

TEST_P(Pkcs11CbcPadTest, EncryptDecrypt_ShortPadding1) {
  if (!is_padded()) {
    return;
  }

  // Padding that's one byte short
  const std::vector<uint8_t> input = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08};
  std::vector<uint8_t> encrypted(input.size());
  uint32_t encrypted_len = 0;

  ScopedPK11SymKey ek = MakeKey(CKA_ENCRYPT);
  SECStatus rv = PK11_Encrypt(ek.get(), GetUnpaddedMechanism(), GetIv(),
                              encrypted.data(), &encrypted_len,
                              encrypted.size(), input.data(), input.size());
  ASSERT_EQ(SECSuccess, rv);
  EXPECT_EQ(input.size(), encrypted_len);

  std::vector<uint8_t> decrypted(input.size());
  uint32_t decrypted_len = 0;
  ScopedPK11SymKey dk = MakeKey(CKA_DECRYPT);
  rv = PK11_Decrypt(dk.get(), GetParam(), GetIv(), decrypted.data(),
                    &decrypted_len, decrypted.size(), encrypted.data(),
                    encrypted_len);
  EXPECT_EQ(SECFailure, rv);
  EXPECT_EQ(0U, decrypted_len);
}

TEST_P(Pkcs11CbcPadTest, EncryptDecrypt_ShortPadding2) {
  if (!is_padded()) {
    return;
  }

  // Padding that's one byte short
  const std::vector<uint8_t> input = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02};
  std::vector<uint8_t> encrypted(input.size());
  uint32_t encrypted_len = 0;

  ScopedPK11SymKey ek = MakeKey(CKA_ENCRYPT);
  SECStatus rv = PK11_Encrypt(ek.get(), GetUnpaddedMechanism(), GetIv(),
                              encrypted.data(), &encrypted_len,
                              encrypted.size(), input.data(), input.size());
  ASSERT_EQ(SECSuccess, rv);
  EXPECT_EQ(input.size(), encrypted_len);

  std::vector<uint8_t> decrypted(input.size());
  uint32_t decrypted_len = 0;
  ScopedPK11SymKey dk = MakeKey(CKA_DECRYPT);
  rv = PK11_Decrypt(dk.get(), GetParam(), GetIv(), decrypted.data(),
                    &decrypted_len, decrypted.size(), encrypted.data(),
                    encrypted_len);
  EXPECT_EQ(SECFailure, rv);
  EXPECT_EQ(0U, decrypted_len);
}

TEST_P(Pkcs11CbcPadTest, EncryptDecrypt_ZeroLengthPadding) {
  if (!is_padded()) {
    return;
  }

  // Padding of length zero
  const std::vector<uint8_t> input = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  std::vector<uint8_t> encrypted(input.size());
  uint32_t encrypted_len = 0;

  ScopedPK11SymKey ek = MakeKey(CKA_ENCRYPT);
  SECStatus rv = PK11_Encrypt(ek.get(), GetUnpaddedMechanism(), GetIv(),
                              encrypted.data(), &encrypted_len,
                              encrypted.size(), input.data(), input.size());
  ASSERT_EQ(SECSuccess, rv);
  EXPECT_EQ(input.size(), encrypted_len);

  std::vector<uint8_t> decrypted(input.size());
  uint32_t decrypted_len = 0;
  ScopedPK11SymKey dk = MakeKey(CKA_DECRYPT);
  rv = PK11_Decrypt(dk.get(), GetParam(), GetIv(), decrypted.data(),
                    &decrypted_len, decrypted.size(), encrypted.data(),
                    encrypted_len);
  EXPECT_EQ(SECFailure, rv);
  EXPECT_EQ(0U, decrypted_len);
}

TEST_P(Pkcs11CbcPadTest, EncryptDecrypt_OverflowPadding) {
  if (!is_padded()) {
    return;
  }

  // Padding that's much longer than block size
  const std::vector<uint8_t> input = {
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  std::vector<uint8_t> encrypted(input.size());
  uint32_t encrypted_len = 0;

  ScopedPK11SymKey ek = MakeKey(CKA_ENCRYPT);
  SECStatus rv = PK11_Encrypt(ek.get(), GetUnpaddedMechanism(), GetIv(),
                              encrypted.data(), &encrypted_len,
                              encrypted.size(), input.data(), input.size());
  ASSERT_EQ(SECSuccess, rv);
  EXPECT_EQ(input.size(), encrypted_len);

  std::vector<uint8_t> decrypted(input.size());
  uint32_t decrypted_len = 0;
  ScopedPK11SymKey dk = MakeKey(CKA_DECRYPT);
  rv = PK11_Decrypt(dk.get(), GetParam(), GetIv(), decrypted.data(),
                    &decrypted_len, decrypted.size(), encrypted.data(),
                    encrypted_len);
  EXPECT_EQ(SECFailure, rv);
  EXPECT_EQ(0U, decrypted_len);
}

TEST_P(Pkcs11CbcPadTest, EncryptDecrypt_ShortValidPadding) {
  if (!is_padded()) {
    return;
  }

  // Minimal valid padding
  const std::vector<uint8_t> input = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
  std::vector<uint8_t> encrypted(input.size());
  uint32_t encrypted_len = 0;

  ScopedPK11SymKey ek = MakeKey(CKA_ENCRYPT);
  SECStatus rv = PK11_Encrypt(ek.get(), GetUnpaddedMechanism(), GetIv(),
                              encrypted.data(), &encrypted_len,
                              encrypted.size(), input.data(), input.size());
  ASSERT_EQ(SECSuccess, rv);
  EXPECT_EQ(input.size(), encrypted_len);

  std::vector<uint8_t> decrypted(input.size());
  uint32_t decrypted_len = 0;
  ScopedPK11SymKey dk = MakeKey(CKA_DECRYPT);
  rv = PK11_Decrypt(dk.get(), GetParam(), GetIv(), decrypted.data(),
                    &decrypted_len, decrypted.size(), encrypted.data(),
                    encrypted_len);
  EXPECT_EQ(SECSuccess, rv);
  EXPECT_EQ(input.size() - 1, decrypted_len);
  EXPECT_EQ(0, memcmp(decrypted.data(), input.data(), decrypted_len));
}

INSTANTIATE_TEST_CASE_P(EncryptDecrypt, Pkcs11CbcPadTest,
                        ::testing::Values(CKM_AES_CBC_PAD, CKM_AES_CBC,
                                          CKM_DES3_CBC_PAD, CKM_DES3_CBC));

}  // namespace nss_test
