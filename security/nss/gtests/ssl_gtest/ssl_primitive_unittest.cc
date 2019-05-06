/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>

#include "keyhi.h"
#include "pk11pub.h"
#include "secerr.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslexp.h"
#include "sslproto.h"

#include "gtest_utils.h"
#include "nss_scoped_ptrs.h"
#include "scoped_ptrs_ssl.h"
#include "tls_connect.h"

namespace nss_test {

// From tls_hkdf_unittest.cc:
extern size_t GetHashLength(SSLHashType ht);

class AeadTest : public ::testing::Test {
 public:
  AeadTest() : slot_(PK11_GetInternalSlot()) {}

  void InitSecret(SSLHashType hash_type) {
    static const uint8_t kData[64] = {'s', 'e', 'c', 'r', 'e', 't'};
    SECItem key_item = {siBuffer, const_cast<uint8_t *>(kData),
                        static_cast<unsigned int>(GetHashLength(hash_type))};
    PK11SymKey *s =
        PK11_ImportSymKey(slot_.get(), CKM_SSL3_MASTER_KEY_DERIVE,
                          PK11_OriginUnwrap, CKA_DERIVE, &key_item, NULL);
    ASSERT_NE(nullptr, s);
    secret_.reset(s);
  }

  void SetUp() override {
    InitSecret(ssl_hash_sha256);
    PORT_SetError(0);
  }

 protected:
  static void EncryptDecrypt(const ScopedSSLAeadContext &ctx,
                             const uint8_t *ciphertext, size_t ciphertext_len) {
    static const uint8_t kAad[] = {'a', 'a', 'd'};
    static const uint8_t kPlaintext[] = {'t', 'e', 'x', 't'};
    static const size_t kMaxSize = 32;

    ASSERT_GE(kMaxSize, ciphertext_len);
    ASSERT_LT(0U, ciphertext_len);

    uint8_t output[kMaxSize];
    unsigned int output_len = 0;
    EXPECT_EQ(SECSuccess, SSL_AeadEncrypt(ctx.get(), 0, kAad, sizeof(kAad),
                                          kPlaintext, sizeof(kPlaintext),
                                          output, &output_len, sizeof(output)));
    ASSERT_EQ(ciphertext_len, static_cast<size_t>(output_len));
    EXPECT_EQ(0, memcmp(ciphertext, output, ciphertext_len));

    memset(output, 0, sizeof(output));
    EXPECT_EQ(SECSuccess, SSL_AeadDecrypt(ctx.get(), 0, kAad, sizeof(kAad),
                                          ciphertext, ciphertext_len, output,
                                          &output_len, sizeof(output)));
    ASSERT_EQ(sizeof(kPlaintext), static_cast<size_t>(output_len));
    EXPECT_EQ(0, memcmp(kPlaintext, output, sizeof(kPlaintext)));

    // Now for some tests of decryption failure.
    // Truncate the input.
    EXPECT_EQ(SECFailure, SSL_AeadDecrypt(ctx.get(), 0, kAad, sizeof(kAad),
                                          ciphertext, ciphertext_len - 1,
                                          output, &output_len, sizeof(output)));
    EXPECT_EQ(SEC_ERROR_BAD_DATA, PORT_GetError());

    // Skip the first byte of the AAD.
    EXPECT_EQ(
        SECFailure,
        SSL_AeadDecrypt(ctx.get(), 0, kAad + 1, sizeof(kAad) - 1, ciphertext,
                        ciphertext_len, output, &output_len, sizeof(output)));
    EXPECT_EQ(SEC_ERROR_BAD_DATA, PORT_GetError());

    uint8_t input[kMaxSize] = {0};
    // Toggle a byte of the input.
    memcpy(input, ciphertext, ciphertext_len);
    input[0] ^= 9;
    EXPECT_EQ(SECFailure, SSL_AeadDecrypt(ctx.get(), 0, kAad, sizeof(kAad),
                                          input, ciphertext_len, output,
                                          &output_len, sizeof(output)));
    EXPECT_EQ(SEC_ERROR_BAD_DATA, PORT_GetError());

    // Toggle the last byte (the auth tag).
    memcpy(input, ciphertext, ciphertext_len);
    input[ciphertext_len - 1] ^= 77;
    EXPECT_EQ(SECFailure, SSL_AeadDecrypt(ctx.get(), 0, kAad, sizeof(kAad),
                                          input, ciphertext_len, output,
                                          &output_len, sizeof(output)));
    EXPECT_EQ(SEC_ERROR_BAD_DATA, PORT_GetError());

    // Toggle some of the AAD.
    memcpy(input, kAad, sizeof(kAad));
    input[1] ^= 23;
    EXPECT_EQ(SECFailure, SSL_AeadDecrypt(ctx.get(), 0, input, sizeof(kAad),
                                          ciphertext, ciphertext_len, output,
                                          &output_len, sizeof(output)));
    EXPECT_EQ(SEC_ERROR_BAD_DATA, PORT_GetError());
  }

 protected:
  ScopedPK11SymKey secret_;

 private:
  ScopedPK11SlotInfo slot_;
};

// These tests all use fixed inputs: a fixed secret, a fixed label, and fixed
// inputs.  So they have fixed outputs.
static const char *kLabel = "test ";
static const uint8_t kCiphertextAes128Gcm[] = {
    0x11, 0x14, 0xfc, 0x58, 0x4f, 0x44, 0xff, 0x8c, 0xb6, 0xd8,
    0x20, 0xb3, 0xfb, 0x50, 0xd9, 0x3b, 0xd4, 0xc6, 0xe1, 0x14};
static const uint8_t kCiphertextAes256Gcm[] = {
    0xf7, 0x27, 0x35, 0x80, 0x88, 0xaf, 0x99, 0x85, 0xf2, 0x83,
    0xca, 0xbb, 0x95, 0x42, 0x09, 0x3f, 0x9c, 0xf3, 0x29, 0xf0};
static const uint8_t kCiphertextChaCha20Poly1305[] = {
    0x4e, 0x89, 0x2c, 0xfa, 0xfc, 0x8c, 0x40, 0x55, 0x6d, 0x7e,
    0x99, 0xac, 0x8e, 0x54, 0x58, 0xb1, 0x18, 0xd2, 0x66, 0x22};

TEST_F(AeadTest, AeadBadVersion) {
  SSLAeadContext *ctx = nullptr;
  ASSERT_EQ(SECFailure,
            SSL_MakeAead(SSL_LIBRARY_VERSION_TLS_1_2, TLS_AES_128_GCM_SHA256,
                         secret_.get(), kLabel, strlen(kLabel), &ctx));
  EXPECT_EQ(nullptr, ctx);
}

TEST_F(AeadTest, AeadUnsupportedCipher) {
  SSLAeadContext *ctx = nullptr;
  ASSERT_EQ(SECFailure,
            SSL_MakeAead(SSL_LIBRARY_VERSION_TLS_1_3, TLS_RSA_WITH_NULL_MD5,
                         secret_.get(), kLabel, strlen(kLabel), &ctx));
  EXPECT_EQ(nullptr, ctx);
}

TEST_F(AeadTest, AeadOlderCipher) {
  SSLAeadContext *ctx = nullptr;
  ASSERT_EQ(
      SECFailure,
      SSL_MakeAead(SSL_LIBRARY_VERSION_TLS_1_3, TLS_RSA_WITH_AES_128_CBC_SHA,
                   secret_.get(), kLabel, strlen(kLabel), &ctx));
  EXPECT_EQ(nullptr, ctx);
}

TEST_F(AeadTest, AeadNoLabel) {
  SSLAeadContext *ctx = nullptr;
  ASSERT_EQ(SECFailure,
            SSL_MakeAead(SSL_LIBRARY_VERSION_TLS_1_3, TLS_AES_128_GCM_SHA256,
                         secret_.get(), nullptr, 12, &ctx));
  EXPECT_EQ(nullptr, ctx);
}

TEST_F(AeadTest, AeadLongLabel) {
  SSLAeadContext *ctx = nullptr;
  ASSERT_EQ(SECFailure,
            SSL_MakeAead(SSL_LIBRARY_VERSION_TLS_1_3, TLS_AES_128_GCM_SHA256,
                         secret_.get(), "", 254, &ctx));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
  EXPECT_EQ(nullptr, ctx);
}

TEST_F(AeadTest, AeadNoPointer) {
  SSLAeadContext *ctx = nullptr;
  ASSERT_EQ(SECFailure,
            SSL_MakeAead(SSL_LIBRARY_VERSION_TLS_1_3, TLS_AES_128_GCM_SHA256,
                         secret_.get(), kLabel, strlen(kLabel), nullptr));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
  EXPECT_EQ(nullptr, ctx);
}

TEST_F(AeadTest, AeadAes128Gcm) {
  SSLAeadContext *ctxInit;
  ASSERT_EQ(SECSuccess,
            SSL_MakeAead(SSL_LIBRARY_VERSION_TLS_1_3, TLS_AES_128_GCM_SHA256,
                         secret_.get(), kLabel, strlen(kLabel), &ctxInit));
  ScopedSSLAeadContext ctx(ctxInit);
  EXPECT_NE(nullptr, ctx);

  EncryptDecrypt(ctx, kCiphertextAes128Gcm, sizeof(kCiphertextAes128Gcm));
}

TEST_F(AeadTest, AeadAes256Gcm) {
  SSLAeadContext *ctxInit = nullptr;
  ASSERT_EQ(SECSuccess,
            SSL_MakeAead(SSL_LIBRARY_VERSION_TLS_1_3, TLS_AES_256_GCM_SHA384,
                         secret_.get(), kLabel, strlen(kLabel), &ctxInit));
  ScopedSSLAeadContext ctx(ctxInit);
  EXPECT_NE(nullptr, ctx);

  EncryptDecrypt(ctx, kCiphertextAes256Gcm, sizeof(kCiphertextAes256Gcm));
}

TEST_F(AeadTest, AeadChaCha20Poly1305) {
  SSLAeadContext *ctxInit;
  ASSERT_EQ(
      SECSuccess,
      SSL_MakeAead(SSL_LIBRARY_VERSION_TLS_1_3, TLS_CHACHA20_POLY1305_SHA256,
                   secret_.get(), kLabel, strlen(kLabel), &ctxInit));
  ScopedSSLAeadContext ctx(ctxInit);
  EXPECT_NE(nullptr, ctx);

  EncryptDecrypt(ctx, kCiphertextChaCha20Poly1305,
                 sizeof(kCiphertextChaCha20Poly1305));
}

}  // namespace nss_test
