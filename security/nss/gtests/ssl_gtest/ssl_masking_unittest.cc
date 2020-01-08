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

const std::string kLabel = "sn";

class MaskingTest : public ::testing::Test {
 public:
  MaskingTest() : slot_(PK11_GetInternalSlot()) {}

  void InitSecret(SSLHashType hash_type) {
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    PK11SymKey *s = PK11_KeyGen(slot_.get(), CKM_GENERIC_SECRET_KEY_GEN,
                                nullptr, AES_128_KEY_LENGTH, nullptr);
    ASSERT_NE(nullptr, s);
    secret_.reset(s);
  }

  void SetUp() override {
    InitSecret(ssl_hash_sha256);
    PORT_SetError(0);
  }

 protected:
  ScopedPK11SymKey secret_;
  ScopedPK11SlotInfo slot_;
  void CreateMask(PRUint16 ciphersuite, std::string label,
                  const std::vector<uint8_t> &sample,
                  std::vector<uint8_t> *out_mask) {
    ASSERT_NE(nullptr, out_mask);
    SSLMaskingContext *ctx_init = nullptr;
    EXPECT_EQ(SECSuccess,
              SSL_CreateMaskingContext(SSL_LIBRARY_VERSION_TLS_1_3, ciphersuite,
                                       secret_.get(), label.c_str(),
                                       label.size(), &ctx_init));
    EXPECT_EQ(0, PORT_GetError());
    ASSERT_NE(nullptr, ctx_init);
    ScopedSSLMaskingContext ctx(ctx_init);

    EXPECT_EQ(SECSuccess,
              SSL_CreateMask(ctx.get(), sample.data(), sample.size(),
                             out_mask->data(), out_mask->size()));
    EXPECT_EQ(0, PORT_GetError());
    bool all_zeros = std::all_of(out_mask->begin(), out_mask->end(),
                                 [](uint8_t v) { return v == 0; });

    // If out_mask is short, |all_zeros| will be (expectedly) true often enough
    // to fail tests.
    // In this case, just retry to make sure we're not outputting zeros
    // continuously.
    if (all_zeros && out_mask->size() < 3) {
      unsigned int tries = 2;
      std::vector<uint8_t> tmp_sample = sample;
      std::vector<uint8_t> tmp_mask(out_mask->size());
      while (tries--) {
        tmp_sample.data()[0]++;  // Tweak something to get a new mask.
        EXPECT_EQ(SECSuccess, SSL_CreateMask(ctx.get(), tmp_sample.data(),
                                             tmp_sample.size(), tmp_mask.data(),
                                             tmp_mask.size()));
        EXPECT_EQ(0, PORT_GetError());
        bool retry_zero = std::all_of(tmp_mask.begin(), tmp_mask.end(),
                                      [](uint8_t v) { return v == 0; });
        if (!retry_zero) {
          all_zeros = false;
          break;
        }
      }
    }
    EXPECT_FALSE(all_zeros);
  }
};

TEST_F(MaskingTest, MaskContextNoLabel) {
  std::vector<uint8_t> sample(AES_BLOCK_SIZE);
  std::vector<uint8_t> mask(AES_BLOCK_SIZE);
  CreateMask(TLS_AES_128_GCM_SHA256, std::string(""), sample, &mask);
}

TEST_F(MaskingTest, MaskContextUnsupportedMech) {
  std::vector<uint8_t> sample(AES_BLOCK_SIZE);
  std::vector<uint8_t> mask(AES_BLOCK_SIZE);
  SSLMaskingContext *ctx_init = nullptr;
  EXPECT_EQ(SECFailure,
            SSL_CreateMaskingContext(SSL_LIBRARY_VERSION_TLS_1_3,
                                     TLS_RSA_WITH_AES_128_CBC_SHA256,
                                     secret_.get(), nullptr, 0, &ctx_init));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
  EXPECT_EQ(nullptr, ctx_init);
}

TEST_F(MaskingTest, MaskNullSample) {
  std::vector<uint8_t> mask(AES_BLOCK_SIZE);
  SSLMaskingContext *ctx_init = nullptr;
  EXPECT_EQ(SECSuccess,
            SSL_CreateMaskingContext(SSL_LIBRARY_VERSION_TLS_1_3,
                                     TLS_AES_128_GCM_SHA256, secret_.get(),
                                     kLabel.c_str(), kLabel.size(), &ctx_init));
  EXPECT_EQ(0, PORT_GetError());
  ASSERT_NE(nullptr, ctx_init);
  ScopedSSLMaskingContext ctx(ctx_init);

  EXPECT_EQ(SECFailure,
            SSL_CreateMask(ctx.get(), nullptr, 0, mask.data(), mask.size()));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());

  EXPECT_EQ(SECFailure, SSL_CreateMask(ctx.get(), nullptr, mask.size(),
                                       mask.data(), mask.size()));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
}

TEST_F(MaskingTest, MaskContextUnsupportedVersion) {
  std::vector<uint8_t> sample(AES_BLOCK_SIZE);
  std::vector<uint8_t> mask(AES_BLOCK_SIZE);
  SSLMaskingContext *ctx_init = nullptr;
  EXPECT_EQ(SECFailure, SSL_CreateMaskingContext(
                            SSL_LIBRARY_VERSION_TLS_1_2, TLS_AES_128_GCM_SHA256,
                            secret_.get(), nullptr, 0, &ctx_init));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
  EXPECT_EQ(nullptr, ctx_init);
}

TEST_F(MaskingTest, MaskTooMuchOutput) {
  // Max internally-supported length for AES
  std::vector<uint8_t> sample(AES_BLOCK_SIZE);
  std::vector<uint8_t> mask(AES_BLOCK_SIZE + 1);
  SSLMaskingContext *ctx_init = nullptr;
  EXPECT_EQ(SECSuccess,
            SSL_CreateMaskingContext(SSL_LIBRARY_VERSION_TLS_1_3,
                                     TLS_AES_128_GCM_SHA256, secret_.get(),
                                     kLabel.c_str(), kLabel.size(), &ctx_init));
  EXPECT_EQ(0, PORT_GetError());
  ASSERT_NE(nullptr, ctx_init);
  ScopedSSLMaskingContext ctx(ctx_init);

  EXPECT_EQ(SECFailure, SSL_CreateMask(ctx.get(), sample.data(), sample.size(),
                                       mask.data(), mask.size()));
  EXPECT_EQ(SEC_ERROR_OUTPUT_LEN, PORT_GetError());
}

TEST_F(MaskingTest, MaskShortOutput) {
  std::vector<uint8_t> sample(16);
  std::vector<uint8_t> mask(16);  // Don't pass a null

  SSLMaskingContext *ctx_init = nullptr;
  EXPECT_EQ(SECSuccess,
            SSL_CreateMaskingContext(SSL_LIBRARY_VERSION_TLS_1_3,
                                     TLS_AES_128_GCM_SHA256, secret_.get(),
                                     kLabel.c_str(), kLabel.size(), &ctx_init));
  EXPECT_EQ(0, PORT_GetError());
  ASSERT_NE(nullptr, ctx_init);
  ScopedSSLMaskingContext ctx(ctx_init);
  EXPECT_EQ(SECFailure, SSL_CreateMask(ctx.get(), sample.data(), sample.size(),
                                       mask.data(), 0));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
}

TEST_F(MaskingTest, MaskRotateLabel) {
  std::vector<uint8_t> sample(AES_BLOCK_SIZE);
  std::vector<uint8_t> mask1(AES_BLOCK_SIZE);
  std::vector<uint8_t> mask2(AES_BLOCK_SIZE);
  EXPECT_EQ(SECSuccess, PK11_GenerateRandomOnSlot(slot_.get(), sample.data(),
                                                  sample.size()));

  CreateMask(TLS_AES_128_GCM_SHA256, kLabel, sample, &mask1);
  CreateMask(TLS_AES_128_GCM_SHA256, std::string("sn1"), sample, &mask2);
  EXPECT_FALSE(mask1 == mask2);
}

TEST_F(MaskingTest, MaskRotateSample) {
  std::vector<uint8_t> sample(AES_BLOCK_SIZE);
  std::vector<uint8_t> mask1(AES_BLOCK_SIZE);
  std::vector<uint8_t> mask2(AES_BLOCK_SIZE);

  EXPECT_EQ(SECSuccess, PK11_GenerateRandomOnSlot(slot_.get(), sample.data(),
                                                  sample.size()));
  CreateMask(TLS_AES_128_GCM_SHA256, kLabel, sample, &mask1);

  EXPECT_EQ(SECSuccess, PK11_GenerateRandomOnSlot(slot_.get(), sample.data(),
                                                  sample.size()));
  CreateMask(TLS_AES_128_GCM_SHA256, kLabel, sample, &mask2);
  EXPECT_FALSE(mask1 == mask2);
}

TEST_F(MaskingTest, MaskAesRederive) {
  std::vector<uint8_t> sample(AES_BLOCK_SIZE);
  std::vector<uint8_t> mask1(AES_BLOCK_SIZE);
  std::vector<uint8_t> mask2(AES_BLOCK_SIZE);

  SECStatus rv =
      PK11_GenerateRandomOnSlot(slot_.get(), sample.data(), sample.size());
  EXPECT_EQ(SECSuccess, rv);

  // Check that re-using inputs with a new context produces the same mask.
  CreateMask(TLS_AES_128_GCM_SHA256, kLabel, sample, &mask1);
  CreateMask(TLS_AES_128_GCM_SHA256, kLabel, sample, &mask2);
  EXPECT_TRUE(mask1 == mask2);
}

TEST_F(MaskingTest, MaskAesTooLong) {
  std::vector<uint8_t> sample(AES_BLOCK_SIZE + 1);
  std::vector<uint8_t> mask(AES_BLOCK_SIZE + 1);
  SSLMaskingContext *ctx_init = nullptr;
  EXPECT_EQ(SECSuccess,
            SSL_CreateMaskingContext(SSL_LIBRARY_VERSION_TLS_1_3,
                                     TLS_AES_128_GCM_SHA256, secret_.get(),
                                     kLabel.c_str(), kLabel.size(), &ctx_init));
  EXPECT_EQ(0, PORT_GetError());
  ASSERT_NE(nullptr, ctx_init);
  ScopedSSLMaskingContext ctx(ctx_init);
  EXPECT_EQ(SECFailure, SSL_CreateMask(ctx.get(), sample.data(), sample.size(),
                                       mask.data(), mask.size()));
  EXPECT_EQ(SEC_ERROR_OUTPUT_LEN, PORT_GetError());
}

TEST_F(MaskingTest, MaskAesShortSample) {
  std::vector<uint8_t> sample(AES_BLOCK_SIZE - 1);
  std::vector<uint8_t> mask(AES_BLOCK_SIZE - 1);
  SSLMaskingContext *ctx_init = nullptr;
  EXPECT_EQ(SECSuccess,
            SSL_CreateMaskingContext(SSL_LIBRARY_VERSION_TLS_1_3,
                                     TLS_AES_128_GCM_SHA256, secret_.get(),
                                     kLabel.c_str(), kLabel.size(), &ctx_init));
  EXPECT_EQ(0, PORT_GetError());
  ASSERT_NE(nullptr, ctx_init);
  ScopedSSLMaskingContext ctx(ctx_init);

  EXPECT_EQ(SECFailure, SSL_CreateMask(ctx.get(), sample.data(), sample.size(),
                                       mask.data(), mask.size()));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
}

TEST_F(MaskingTest, MaskAesShortValid) {
  std::vector<uint8_t> sample(AES_BLOCK_SIZE);
  std::vector<uint8_t> mask(1);
  EXPECT_EQ(SECSuccess, PK11_GenerateRandomOnSlot(slot_.get(), sample.data(),
                                                  sample.size()));
  CreateMask(TLS_AES_128_GCM_SHA256, kLabel, sample, &mask);
}

TEST_F(MaskingTest, MaskChaChaRederive) {
  // Block-aligned.
  std::vector<uint8_t> sample(32);
  std::vector<uint8_t> mask1(32);
  std::vector<uint8_t> mask2(32);
  EXPECT_EQ(SECSuccess, PK11_GenerateRandomOnSlot(slot_.get(), sample.data(),
                                                  sample.size()));
  CreateMask(TLS_CHACHA20_POLY1305_SHA256, kLabel, sample, &mask1);
  CreateMask(TLS_CHACHA20_POLY1305_SHA256, kLabel, sample, &mask2);
  EXPECT_TRUE(mask1 == mask2);
}

TEST_F(MaskingTest, MaskChaChaRederiveOddSizes) {
  // Non-block-aligned.
  std::vector<uint8_t> sample(27);
  std::vector<uint8_t> mask1(26);
  std::vector<uint8_t> mask2(25);
  EXPECT_EQ(SECSuccess, PK11_GenerateRandomOnSlot(slot_.get(), sample.data(),
                                                  sample.size()));
  CreateMask(TLS_CHACHA20_POLY1305_SHA256, kLabel, sample, &mask1);
  CreateMask(TLS_CHACHA20_POLY1305_SHA256, kLabel, sample, &mask2);
  mask1.pop_back();
  EXPECT_TRUE(mask1 == mask2);
}

TEST_F(MaskingTest, MaskChaChaLongValid) {
  // Max internally-supported length for ChaCha
  std::vector<uint8_t> sample(128);
  std::vector<uint8_t> mask(128);
  EXPECT_EQ(SECSuccess, PK11_GenerateRandomOnSlot(slot_.get(), sample.data(),
                                                  sample.size()));
  CreateMask(TLS_CHACHA20_POLY1305_SHA256, kLabel, sample, &mask);
}

TEST_F(MaskingTest, MaskChaChaTooLong) {
  // Max internally-supported length for ChaCha
  std::vector<uint8_t> sample(128 + 1);
  std::vector<uint8_t> mask(128 + 1);
  SSLMaskingContext *ctx_init = nullptr;
  EXPECT_EQ(SECSuccess, SSL_CreateMaskingContext(SSL_LIBRARY_VERSION_TLS_1_3,
                                                 TLS_CHACHA20_POLY1305_SHA256,
                                                 secret_.get(), kLabel.c_str(),
                                                 kLabel.size(), &ctx_init));
  EXPECT_EQ(0, PORT_GetError());
  ASSERT_NE(nullptr, ctx_init);
  ScopedSSLMaskingContext ctx(ctx_init);
  EXPECT_EQ(SECFailure, SSL_CreateMask(ctx.get(), sample.data(), sample.size(),
                                       mask.data(), mask.size()));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
}

TEST_F(MaskingTest, MaskChaChaShortSample) {
  std::vector<uint8_t> sample(15);  // Should have 4B ctr, 12B nonce.
  std::vector<uint8_t> mask(15);
  SSLMaskingContext *ctx_init = nullptr;
  EXPECT_EQ(SECSuccess, SSL_CreateMaskingContext(SSL_LIBRARY_VERSION_TLS_1_3,
                                                 TLS_CHACHA20_POLY1305_SHA256,
                                                 secret_.get(), kLabel.c_str(),
                                                 kLabel.size(), &ctx_init));
  EXPECT_EQ(0, PORT_GetError());
  ASSERT_NE(nullptr, ctx_init);
  ScopedSSLMaskingContext ctx(ctx_init);
  EXPECT_EQ(SECFailure, SSL_CreateMask(ctx.get(), sample.data(), sample.size(),
                                       mask.data(), mask.size()));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
}

TEST_F(MaskingTest, MaskChaChaShortValid) {
  std::vector<uint8_t> sample(16);
  std::vector<uint8_t> mask(1);
  EXPECT_EQ(SECSuccess, PK11_GenerateRandomOnSlot(slot_.get(), sample.data(),
                                                  sample.size()));
  CreateMask(TLS_CHACHA20_POLY1305_SHA256, kLabel, sample, &mask);
}

}  // namespace nss_test
