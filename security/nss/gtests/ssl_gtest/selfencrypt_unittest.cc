/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <functional>
#include <memory>
#include "nss.h"
#include "pk11pub.h"
#include "prerror.h"
#include "secerr.h"
#include "ssl.h"
#include "sslerr.h"
extern "C" {
#include "sslimpl.h"
#include "selfencrypt.h"
}

#include "databuffer.h"
#include "gtest_utils.h"
#include "scoped_ptrs.h"

namespace nss_test {

static const uint8_t kAesKey1Buf[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                                      0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
                                      0x0c, 0x0d, 0x0e, 0x0f};
static const DataBuffer kAesKey1(kAesKey1Buf, sizeof(kAesKey1Buf));

static const uint8_t kAesKey2Buf[] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
                                      0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b,
                                      0x1c, 0x1d, 0x1e, 0x1f};
static const DataBuffer kAesKey2(kAesKey2Buf, sizeof(kAesKey2Buf));

static const uint8_t kHmacKey1Buf[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
    0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};
static const DataBuffer kHmacKey1(kHmacKey1Buf, sizeof(kHmacKey1Buf));

static const uint8_t kHmacKey2Buf[] = {
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a,
    0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
    0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f};
static const DataBuffer kHmacKey2(kHmacKey2Buf, sizeof(kHmacKey2Buf));

static const uint8_t* kKeyName1 =
    reinterpret_cast<const unsigned char*>("KEY1KEY1KEY1KEY1");
static const uint8_t* kKeyName2 =
    reinterpret_cast<const uint8_t*>("KEY2KEY2KEY2KEY2");

static void ImportKey(const DataBuffer& key, PK11SlotInfo* slot,
                      CK_MECHANISM_TYPE mech, CK_ATTRIBUTE_TYPE cka,
                      ScopedPK11SymKey* to) {
  SECItem key_item = {siBuffer, const_cast<uint8_t*>(key.data()),
                      static_cast<unsigned int>(key.len())};

  PK11SymKey* inner =
      PK11_ImportSymKey(slot, mech, PK11_OriginUnwrap, cka, &key_item, nullptr);
  ASSERT_NE(nullptr, inner);
  to->reset(inner);
}

extern "C" {
extern char ssl_trace;
extern FILE* ssl_trace_iob;
}

class SelfEncryptTestBase : public ::testing::Test {
 public:
  SelfEncryptTestBase(size_t message_size)
      : aes1_(),
        aes2_(),
        hmac1_(),
        hmac2_(),
        message_(),
        slot_(PK11_GetInternalSlot()) {
    EXPECT_NE(nullptr, slot_);
    char* ev = getenv("SSLTRACE");
    if (ev && ev[0]) {
      ssl_trace = atoi(ev);
      ssl_trace_iob = stderr;
    }
    message_.Allocate(message_size);
    for (size_t i = 0; i < message_.len(); ++i) {
      message_.data()[i] = i;
    }
  }

  void SetUp() {
    message_.Allocate(100);
    for (size_t i = 0; i < 100; ++i) {
      message_.data()[i] = i;
    }
    ImportKey(kAesKey1, slot_.get(), CKM_AES_CBC, CKA_ENCRYPT, &aes1_);
    ImportKey(kAesKey2, slot_.get(), CKM_AES_CBC, CKA_ENCRYPT, &aes2_);
    ImportKey(kHmacKey1, slot_.get(), CKM_SHA256_HMAC, CKA_SIGN, &hmac1_);
    ImportKey(kHmacKey2, slot_.get(), CKM_SHA256_HMAC, CKA_SIGN, &hmac2_);
  }

  void SelfTest(
      const uint8_t* writeKeyName, const ScopedPK11SymKey& writeAes,
      const ScopedPK11SymKey& writeHmac, const uint8_t* readKeyName,
      const ScopedPK11SymKey& readAes, const ScopedPK11SymKey& readHmac,
      PRErrorCode protect_error_code = 0, PRErrorCode unprotect_error_code = 0,
      std::function<void(uint8_t* ciphertext, unsigned int* ciphertext_len)>
          mutate = nullptr) {
    uint8_t ciphertext[1000];
    unsigned int ciphertext_len;
    uint8_t plaintext[1000];
    unsigned int plaintext_len;

    SECStatus rv = ssl_SelfEncryptProtectInt(
        writeAes.get(), writeHmac.get(), writeKeyName, message_.data(),
        message_.len(), ciphertext, &ciphertext_len, sizeof(ciphertext));
    if (rv != SECSuccess) {
      std::cerr << "Error: " << PORT_ErrorToName(PORT_GetError()) << std::endl;
    }
    if (protect_error_code) {
      ASSERT_EQ(protect_error_code, PORT_GetError());
      return;
    }
    ASSERT_EQ(SECSuccess, rv);

    if (mutate) {
      mutate(ciphertext, &ciphertext_len);
    }
    rv = ssl_SelfEncryptUnprotectInt(readAes.get(), readHmac.get(), readKeyName,
                                     ciphertext, ciphertext_len, plaintext,
                                     &plaintext_len, sizeof(plaintext));
    if (rv != SECSuccess) {
      std::cerr << "Error: " << PORT_ErrorToName(PORT_GetError()) << std::endl;
    }
    if (!unprotect_error_code) {
      ASSERT_EQ(SECSuccess, rv);
      EXPECT_EQ(message_.len(), plaintext_len);
      EXPECT_EQ(0, memcmp(message_.data(), plaintext, message_.len()));
    } else {
      ASSERT_EQ(SECFailure, rv);
      EXPECT_EQ(unprotect_error_code, PORT_GetError());
    }
  }

 protected:
  ScopedPK11SymKey aes1_;
  ScopedPK11SymKey aes2_;
  ScopedPK11SymKey hmac1_;
  ScopedPK11SymKey hmac2_;
  DataBuffer message_;

 private:
  ScopedPK11SlotInfo slot_;
};

class SelfEncryptTestVariable : public SelfEncryptTestBase,
                                public ::testing::WithParamInterface<size_t> {
 public:
  SelfEncryptTestVariable() : SelfEncryptTestBase(GetParam()) {}
};

class SelfEncryptTest128 : public SelfEncryptTestBase {
 public:
  SelfEncryptTest128() : SelfEncryptTestBase(128) {}
};

TEST_P(SelfEncryptTestVariable, SuccessCase) {
  SelfTest(kKeyName1, aes1_, hmac1_, kKeyName1, aes1_, hmac1_);
}

TEST_P(SelfEncryptTestVariable, WrongMacKey) {
  SelfTest(kKeyName1, aes1_, hmac1_, kKeyName1, aes1_, hmac2_, 0,
           SEC_ERROR_BAD_DATA);
}

TEST_P(SelfEncryptTestVariable, WrongKeyName) {
  SelfTest(kKeyName1, aes1_, hmac1_, kKeyName2, aes1_, hmac1_, 0,
           SEC_ERROR_NOT_A_RECIPIENT);
}

TEST_P(SelfEncryptTestVariable, AddAByte) {
  SelfTest(kKeyName1, aes1_, hmac1_, kKeyName1, aes1_, hmac1_, 0,
           SEC_ERROR_BAD_DATA,
           [](uint8_t* ciphertext, unsigned int* ciphertext_len) {
             (*ciphertext_len)++;
           });
}

TEST_P(SelfEncryptTestVariable, SubtractAByte) {
  SelfTest(kKeyName1, aes1_, hmac1_, kKeyName1, aes1_, hmac1_, 0,
           SEC_ERROR_BAD_DATA,
           [](uint8_t* ciphertext, unsigned int* ciphertext_len) {
             (*ciphertext_len)--;
           });
}

TEST_P(SelfEncryptTestVariable, BogusIv) {
  SelfTest(kKeyName1, aes1_, hmac1_, kKeyName1, aes1_, hmac1_, 0,
           SEC_ERROR_BAD_DATA,
           [](uint8_t* ciphertext, unsigned int* ciphertext_len) {
             ciphertext[16]++;
           });
}

TEST_P(SelfEncryptTestVariable, BogusCiphertext) {
  SelfTest(kKeyName1, aes1_, hmac1_, kKeyName1, aes1_, hmac1_, 0,
           SEC_ERROR_BAD_DATA,
           [](uint8_t* ciphertext, unsigned int* ciphertext_len) {
             ciphertext[32]++;
           });
}

TEST_P(SelfEncryptTestVariable, BadMac) {
  SelfTest(kKeyName1, aes1_, hmac1_, kKeyName1, aes1_, hmac1_, 0,
           SEC_ERROR_BAD_DATA,
           [](uint8_t* ciphertext, unsigned int* ciphertext_len) {
             ciphertext[*ciphertext_len - 1]++;
           });
}

TEST_F(SelfEncryptTest128, DISABLED_BadPadding) {
  SelfTest(kKeyName1, aes1_, hmac1_, kKeyName1, aes2_, hmac1_, 0,
           SEC_ERROR_BAD_DATA);
}

TEST_F(SelfEncryptTest128, ShortKeyName) {
  SelfTest(kKeyName1, aes1_, hmac1_, kKeyName1, aes1_, hmac1_, 0,
           SEC_ERROR_BAD_DATA,
           [](uint8_t* ciphertext, unsigned int* ciphertext_len) {
             *ciphertext_len = 15;
           });
}

TEST_F(SelfEncryptTest128, ShortIv) {
  SelfTest(kKeyName1, aes1_, hmac1_, kKeyName1, aes1_, hmac1_, 0,
           SEC_ERROR_BAD_DATA,
           [](uint8_t* ciphertext, unsigned int* ciphertext_len) {
             *ciphertext_len = 31;
           });
}

TEST_F(SelfEncryptTest128, ShortCiphertextLen) {
  SelfTest(kKeyName1, aes1_, hmac1_, kKeyName1, aes1_, hmac1_, 0,
           SEC_ERROR_BAD_DATA,
           [](uint8_t* ciphertext, unsigned int* ciphertext_len) {
             *ciphertext_len = 32;
           });
}

TEST_F(SelfEncryptTest128, ShortCiphertext) {
  SelfTest(kKeyName1, aes1_, hmac1_, kKeyName1, aes1_, hmac1_, 0,
           SEC_ERROR_BAD_DATA,
           [](uint8_t* ciphertext, unsigned int* ciphertext_len) {
             *ciphertext_len -= 17;
           });
}

TEST_F(SelfEncryptTest128, MacWithAESKeyEncrypt) {
  SelfTest(kKeyName1, aes1_, aes1_, kKeyName1, aes1_, hmac1_,
           SEC_ERROR_LIBRARY_FAILURE);
}

TEST_F(SelfEncryptTest128, AESWithMacKeyEncrypt) {
  SelfTest(kKeyName1, hmac1_, hmac1_, kKeyName1, aes1_, hmac1_,
           SEC_ERROR_INVALID_KEY);
}

TEST_F(SelfEncryptTest128, MacWithAESKeyDecrypt) {
  SelfTest(kKeyName1, aes1_, hmac1_, kKeyName1, aes1_, aes1_, 0,
           SEC_ERROR_LIBRARY_FAILURE);
}

TEST_F(SelfEncryptTest128, AESWithMacKeyDecrypt) {
  SelfTest(kKeyName1, aes1_, hmac1_, kKeyName1, hmac1_, hmac1_, 0,
           SEC_ERROR_INVALID_KEY);
}

INSTANTIATE_TEST_CASE_P(VariousSizes, SelfEncryptTestVariable,
                        ::testing::Values(0, 15, 16, 31, 255, 256, 257));

}  // namespace nss_test
