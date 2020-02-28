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
  // Should have 4B ctr, 12B nonce for ChaCha, or >=16B ciphertext for AES.
  // Use the same default size for mask output.
  static const int kSampleSize = 16;
  static const int kMaskSize = 16;
  void CreateMask(PRUint16 ciphersuite, SSLProtocolVariant variant,
                  std::string label, const std::vector<uint8_t> &sample,
                  std::vector<uint8_t> *out_mask) {
    ASSERT_NE(nullptr, out_mask);
    SSLMaskingContext *ctx_init = nullptr;
    EXPECT_EQ(SECSuccess,
              SSL_CreateVariantMaskingContext(
                  SSL_LIBRARY_VERSION_TLS_1_3, ciphersuite, variant,
                  secret_.get(), label.c_str(), label.size(), &ctx_init));
    ASSERT_NE(nullptr, ctx_init);
    ScopedSSLMaskingContext ctx(ctx_init);

    EXPECT_EQ(SECSuccess,
              SSL_CreateMask(ctx.get(), sample.data(), sample.size(),
                             out_mask->data(), out_mask->size()));
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

class SuiteTest : public MaskingTest,
                  public ::testing::WithParamInterface<uint16_t> {
 public:
  SuiteTest() : ciphersuite_(GetParam()) {}
  void CreateMask(std::string label, const std::vector<uint8_t> &sample,
                  std::vector<uint8_t> *out_mask) {
    MaskingTest::CreateMask(ciphersuite_, ssl_variant_datagram, label, sample,
                            out_mask);
  }

 protected:
  const uint16_t ciphersuite_;
};

class VariantTest : public MaskingTest,
                    public ::testing::WithParamInterface<SSLProtocolVariant> {
 public:
  VariantTest() : variant_(GetParam()) {}
  void CreateMask(uint16_t ciphersuite, std::string label,
                  const std::vector<uint8_t> &sample,
                  std::vector<uint8_t> *out_mask) {
    MaskingTest::CreateMask(ciphersuite, variant_, label, sample, out_mask);
  }

 protected:
  const SSLProtocolVariant variant_;
};

class VariantSuiteTest : public MaskingTest,
                         public ::testing::WithParamInterface<
                             std::tuple<SSLProtocolVariant, uint16_t>> {
 public:
  VariantSuiteTest()
      : variant_(std::get<0>(GetParam())),
        ciphersuite_(std::get<1>(GetParam())) {}
  void CreateMask(std::string label, const std::vector<uint8_t> &sample,
                  std::vector<uint8_t> *out_mask) {
    MaskingTest::CreateMask(ciphersuite_, variant_, label, sample, out_mask);
  }

 protected:
  const SSLProtocolVariant variant_;
  const uint16_t ciphersuite_;
};

TEST_P(VariantSuiteTest, MaskContextNoLabel) {
  std::vector<uint8_t> sample(kSampleSize);
  std::vector<uint8_t> mask(kMaskSize);
  CreateMask(std::string(""), sample, &mask);
}

TEST_P(VariantSuiteTest, MaskNoSample) {
  std::vector<uint8_t> mask(kMaskSize);
  SSLMaskingContext *ctx_init = nullptr;
  EXPECT_EQ(SECSuccess,
            SSL_CreateVariantMaskingContext(
                SSL_LIBRARY_VERSION_TLS_1_3, ciphersuite_, variant_,
                secret_.get(), kLabel.c_str(), kLabel.size(), &ctx_init));
  ASSERT_NE(nullptr, ctx_init);
  ScopedSSLMaskingContext ctx(ctx_init);

  EXPECT_EQ(SECFailure,
            SSL_CreateMask(ctx.get(), nullptr, 0, mask.data(), mask.size()));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());

  EXPECT_EQ(SECFailure, SSL_CreateMask(ctx.get(), nullptr, mask.size(),
                                       mask.data(), mask.size()));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
}

TEST_P(VariantSuiteTest, MaskShortSample) {
  std::vector<uint8_t> sample(kSampleSize);
  std::vector<uint8_t> mask(kMaskSize);
  SSLMaskingContext *ctx_init = nullptr;
  EXPECT_EQ(SECSuccess,
            SSL_CreateVariantMaskingContext(
                SSL_LIBRARY_VERSION_TLS_1_3, ciphersuite_, variant_,
                secret_.get(), kLabel.c_str(), kLabel.size(), &ctx_init));
  ASSERT_NE(nullptr, ctx_init);
  ScopedSSLMaskingContext ctx(ctx_init);

  EXPECT_EQ(SECFailure,
            SSL_CreateMask(ctx.get(), sample.data(), sample.size() - 1,
                           mask.data(), mask.size()));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
}

TEST_P(VariantSuiteTest, MaskContextUnsupportedMech) {
  std::vector<uint8_t> sample(kSampleSize);
  std::vector<uint8_t> mask(kMaskSize);
  SSLMaskingContext *ctx_init = nullptr;
  EXPECT_EQ(SECFailure,
            SSL_CreateVariantMaskingContext(
                SSL_LIBRARY_VERSION_TLS_1_3, TLS_RSA_WITH_AES_128_CBC_SHA256,
                variant_, secret_.get(), nullptr, 0, &ctx_init));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
  EXPECT_EQ(nullptr, ctx_init);
}

TEST_P(VariantSuiteTest, MaskContextUnsupportedVersion) {
  std::vector<uint8_t> sample(kSampleSize);
  std::vector<uint8_t> mask(kMaskSize);
  SSLMaskingContext *ctx_init = nullptr;
  EXPECT_EQ(SECFailure, SSL_CreateVariantMaskingContext(
                            SSL_LIBRARY_VERSION_TLS_1_2, ciphersuite_, variant_,
                            secret_.get(), nullptr, 0, &ctx_init));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
  EXPECT_EQ(nullptr, ctx_init);
}

TEST_P(VariantSuiteTest, MaskMaxLength) {
  uint32_t max_mask_len = kMaskSize;
  if (ciphersuite_ == TLS_CHACHA20_POLY1305_SHA256) {
    // Internal limitation for ChaCha20 masks.
    max_mask_len = 128;
  }

  std::vector<uint8_t> sample(kSampleSize);
  std::vector<uint8_t> mask(max_mask_len + 1);
  SSLMaskingContext *ctx_init = nullptr;
  EXPECT_EQ(SECSuccess,
            SSL_CreateVariantMaskingContext(
                SSL_LIBRARY_VERSION_TLS_1_3, ciphersuite_, variant_,
                secret_.get(), kLabel.c_str(), kLabel.size(), &ctx_init));
  ASSERT_NE(nullptr, ctx_init);
  ScopedSSLMaskingContext ctx(ctx_init);

  EXPECT_EQ(SECSuccess, SSL_CreateMask(ctx.get(), sample.data(), sample.size(),
                                       mask.data(), mask.size() - 1));
  EXPECT_EQ(SECFailure, SSL_CreateMask(ctx.get(), sample.data(), sample.size(),
                                       mask.data(), mask.size()));
  EXPECT_EQ(SEC_ERROR_OUTPUT_LEN, PORT_GetError());
}

TEST_P(VariantSuiteTest, MaskMinLength) {
  std::vector<uint8_t> sample(kSampleSize);
  std::vector<uint8_t> mask(1);  // Don't pass a null

  SSLMaskingContext *ctx_init = nullptr;
  EXPECT_EQ(SECSuccess,
            SSL_CreateVariantMaskingContext(
                SSL_LIBRARY_VERSION_TLS_1_3, ciphersuite_, variant_,
                secret_.get(), kLabel.c_str(), kLabel.size(), &ctx_init));
  ASSERT_NE(nullptr, ctx_init);
  ScopedSSLMaskingContext ctx(ctx_init);
  EXPECT_EQ(SECFailure, SSL_CreateMask(ctx.get(), sample.data(), sample.size(),
                                       mask.data(), 0));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
  EXPECT_EQ(SECSuccess, SSL_CreateMask(ctx.get(), sample.data(), sample.size(),
                                       mask.data(), 1));
}

TEST_P(VariantSuiteTest, MaskRotateLabel) {
  std::vector<uint8_t> sample(kSampleSize);
  std::vector<uint8_t> mask1(kMaskSize);
  std::vector<uint8_t> mask2(kMaskSize);
  EXPECT_EQ(SECSuccess, PK11_GenerateRandomOnSlot(slot_.get(), sample.data(),
                                                  sample.size()));

  CreateMask(kLabel, sample, &mask1);
  CreateMask(std::string("sn1"), sample, &mask2);
  EXPECT_FALSE(mask1 == mask2);
}

TEST_P(VariantSuiteTest, MaskRotateSample) {
  std::vector<uint8_t> sample(kSampleSize);
  std::vector<uint8_t> mask1(kMaskSize);
  std::vector<uint8_t> mask2(kMaskSize);

  EXPECT_EQ(SECSuccess, PK11_GenerateRandomOnSlot(slot_.get(), sample.data(),
                                                  sample.size()));
  CreateMask(kLabel, sample, &mask1);

  EXPECT_EQ(SECSuccess, PK11_GenerateRandomOnSlot(slot_.get(), sample.data(),
                                                  sample.size()));
  CreateMask(kLabel, sample, &mask2);
  EXPECT_FALSE(mask1 == mask2);
}

TEST_P(VariantSuiteTest, MaskRederive) {
  std::vector<uint8_t> sample(kSampleSize);
  std::vector<uint8_t> mask1(kMaskSize);
  std::vector<uint8_t> mask2(kMaskSize);

  SECStatus rv =
      PK11_GenerateRandomOnSlot(slot_.get(), sample.data(), sample.size());
  EXPECT_EQ(SECSuccess, rv);

  // Check that re-using inputs with a new context produces the same mask.
  CreateMask(kLabel, sample, &mask1);
  CreateMask(kLabel, sample, &mask2);
  EXPECT_TRUE(mask1 == mask2);
}

TEST_P(SuiteTest, MaskTlsVariantKeySeparation) {
  std::vector<uint8_t> sample(kSampleSize);
  std::vector<uint8_t> tls_mask(kMaskSize);
  std::vector<uint8_t> dtls_mask(kMaskSize);
  SSLMaskingContext *stream_ctx_init = nullptr;
  SSLMaskingContext *datagram_ctx_init = nullptr;

  // Init
  EXPECT_EQ(SECSuccess, SSL_CreateVariantMaskingContext(
                            SSL_LIBRARY_VERSION_TLS_1_3, ciphersuite_,
                            ssl_variant_stream, secret_.get(), kLabel.c_str(),
                            kLabel.size(), &stream_ctx_init));
  ASSERT_NE(nullptr, stream_ctx_init);
  EXPECT_EQ(SECSuccess, SSL_CreateVariantMaskingContext(
                            SSL_LIBRARY_VERSION_TLS_1_3, ciphersuite_,
                            ssl_variant_datagram, secret_.get(), kLabel.c_str(),
                            kLabel.size(), &datagram_ctx_init));
  ASSERT_NE(nullptr, datagram_ctx_init);
  ScopedSSLMaskingContext tls_ctx(stream_ctx_init);
  ScopedSSLMaskingContext dtls_ctx(datagram_ctx_init);

  // Derive
  EXPECT_EQ(SECSuccess,
            SSL_CreateMask(tls_ctx.get(), sample.data(), sample.size(),
                           tls_mask.data(), tls_mask.size()));

  EXPECT_EQ(SECSuccess,
            SSL_CreateMask(dtls_ctx.get(), sample.data(), sample.size(),
                           dtls_mask.data(), dtls_mask.size()));
  EXPECT_NE(tls_mask, dtls_mask);
}

TEST_P(VariantTest, MaskChaChaRederiveOddSizes) {
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

static const uint16_t kMaskingCiphersuites[] = {TLS_CHACHA20_POLY1305_SHA256,
                                                TLS_AES_128_GCM_SHA256,
                                                TLS_AES_256_GCM_SHA384};
::testing::internal::ParamGenerator<uint16_t> kMaskingCiphersuiteParams =
    ::testing::ValuesIn(kMaskingCiphersuites);

INSTANTIATE_TEST_CASE_P(GenericMasking, SuiteTest, kMaskingCiphersuiteParams);

INSTANTIATE_TEST_CASE_P(GenericMasking, VariantTest,
                        TlsConnectTestBase::kTlsVariantsAll);

INSTANTIATE_TEST_CASE_P(GenericMasking, VariantSuiteTest,
                        ::testing::Combine(TlsConnectTestBase::kTlsVariantsAll,
                                           kMaskingCiphersuiteParams));

}  // namespace nss_test
