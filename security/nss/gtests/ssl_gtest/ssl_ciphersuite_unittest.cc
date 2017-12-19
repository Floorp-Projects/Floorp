/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <functional>
#include <memory>
#include "secerr.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"

extern "C" {
// This is not something that should make you happy.
#include "libssl_internals.h"
}

#include "gtest_utils.h"
#include "tls_connect.h"
#include "tls_parser.h"

namespace nss_test {

// variant, version, cipher suite
typedef std::tuple<SSLProtocolVariant, uint16_t, uint16_t, SSLNamedGroup,
                   SSLSignatureScheme>
    CipherSuiteProfile;

class TlsCipherSuiteTestBase : public TlsConnectTestBase {
 public:
  TlsCipherSuiteTestBase(SSLProtocolVariant variant, uint16_t version,
                         uint16_t cipher_suite, SSLNamedGroup group,
                         SSLSignatureScheme sig_scheme)
      : TlsConnectTestBase(variant, version),
        cipher_suite_(cipher_suite),
        group_(group),
        sig_scheme_(sig_scheme),
        csinfo_({0}) {
    SECStatus rv =
        SSL_GetCipherSuiteInfo(cipher_suite_, &csinfo_, sizeof(csinfo_));
    EXPECT_EQ(SECSuccess, rv);
    if (rv == SECSuccess) {
      std::cerr << "Cipher suite: " << csinfo_.cipherSuiteName << std::endl;
    }
    auth_type_ = csinfo_.authType;
    kea_type_ = csinfo_.keaType;
  }

 protected:
  void EnableSingleCipher() {
    EnsureTlsSetup();
    // It doesn't matter which does this, but the test is better if both do it.
    client_->EnableSingleCipher(cipher_suite_);
    server_->EnableSingleCipher(cipher_suite_);

    if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
      std::vector<SSLNamedGroup> groups = {group_};
      client_->ConfigNamedGroups(groups);
      server_->ConfigNamedGroups(groups);
      kea_type_ = SSLInt_GetKEAType(group_);

      client_->SetSignatureSchemes(&sig_scheme_, 1);
      server_->SetSignatureSchemes(&sig_scheme_, 1);
    }
  }

  virtual void SetupCertificate() {
    if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
      switch (sig_scheme_) {
        case ssl_sig_rsa_pkcs1_sha256:
        case ssl_sig_rsa_pkcs1_sha384:
        case ssl_sig_rsa_pkcs1_sha512:
          Reset(TlsAgent::kServerRsaSign);
          auth_type_ = ssl_auth_rsa_sign;
          break;
        case ssl_sig_rsa_pss_sha256:
        case ssl_sig_rsa_pss_sha384:
          Reset(TlsAgent::kServerRsaSign);
          auth_type_ = ssl_auth_rsa_sign;
          break;
        case ssl_sig_rsa_pss_sha512:
          // You can't fit SHA-512 PSS in a 1024-bit key.
          Reset(TlsAgent::kRsa2048);
          auth_type_ = ssl_auth_rsa_sign;
          break;
        case ssl_sig_ecdsa_secp256r1_sha256:
          Reset(TlsAgent::kServerEcdsa256);
          auth_type_ = ssl_auth_ecdsa;
          break;
        case ssl_sig_ecdsa_secp384r1_sha384:
          Reset(TlsAgent::kServerEcdsa384);
          auth_type_ = ssl_auth_ecdsa;
          break;
        default:
          ADD_FAILURE() << "Unsupported signature scheme: " << sig_scheme_;
          break;
      }
    } else {
      switch (csinfo_.authType) {
        case ssl_auth_rsa_sign:
          Reset(TlsAgent::kServerRsaSign);
          break;
        case ssl_auth_rsa_decrypt:
          Reset(TlsAgent::kServerRsaDecrypt);
          break;
        case ssl_auth_ecdsa:
          Reset(TlsAgent::kServerEcdsa256);
          break;
        case ssl_auth_ecdh_ecdsa:
          Reset(TlsAgent::kServerEcdhEcdsa);
          break;
        case ssl_auth_ecdh_rsa:
          Reset(TlsAgent::kServerEcdhRsa);
          break;
        case ssl_auth_dsa:
          Reset(TlsAgent::kServerDsa);
          break;
        default:
          ASSERT_TRUE(false) << "Unsupported cipher suite: " << cipher_suite_;
          break;
      }
    }
  }

  void ConnectAndCheckCipherSuite() {
    Connect();
    SendReceive();

    // Check that we used the right cipher suite, auth type and kea type.
    uint16_t actual;
    EXPECT_TRUE(client_->cipher_suite(&actual));
    EXPECT_EQ(cipher_suite_, actual);
    EXPECT_TRUE(server_->cipher_suite(&actual));
    EXPECT_EQ(cipher_suite_, actual);
    SSLAuthType auth;
    EXPECT_TRUE(client_->auth_type(&auth));
    EXPECT_EQ(auth_type_, auth);
    EXPECT_TRUE(server_->auth_type(&auth));
    EXPECT_EQ(auth_type_, auth);
    SSLKEAType kea;
    EXPECT_TRUE(client_->kea_type(&kea));
    EXPECT_EQ(kea_type_, kea);
    EXPECT_TRUE(server_->kea_type(&kea));
    EXPECT_EQ(kea_type_, kea);
  }

  // Get the expected limit on the number of records that can be sent for the
  // cipher suite.
  uint64_t record_limit() const {
    switch (csinfo_.symCipher) {
      case ssl_calg_rc4:
      case ssl_calg_3des:
        return 1ULL << 20;
      case ssl_calg_aes:
      case ssl_calg_aes_gcm:
        return 0x5aULL << 28;
      case ssl_calg_null:
      case ssl_calg_chacha20:
        return (1ULL << 48) - 1;
      case ssl_calg_rc2:
      case ssl_calg_des:
      case ssl_calg_idea:
      case ssl_calg_fortezza:
      case ssl_calg_camellia:
      case ssl_calg_seed:
        break;
    }
    EXPECT_TRUE(false) << "No limit for " << csinfo_.cipherSuiteName;
    return 1ULL < 48;
  }

  uint64_t last_safe_write() const {
    uint64_t limit = record_limit() - 1;
    if (version_ < SSL_LIBRARY_VERSION_TLS_1_1 &&
        (csinfo_.symCipher == ssl_calg_3des ||
         csinfo_.symCipher == ssl_calg_aes)) {
      // 1/n-1 record splitting needs space for two records.
      limit--;
    }
    return limit;
  }

 protected:
  uint16_t cipher_suite_;
  SSLAuthType auth_type_;
  SSLKEAType kea_type_;
  SSLNamedGroup group_;
  SSLSignatureScheme sig_scheme_;
  SSLCipherSuiteInfo csinfo_;
};

class TlsCipherSuiteTest
    : public TlsCipherSuiteTestBase,
      public ::testing::WithParamInterface<CipherSuiteProfile> {
 public:
  TlsCipherSuiteTest()
      : TlsCipherSuiteTestBase(std::get<0>(GetParam()), std::get<1>(GetParam()),
                               std::get<2>(GetParam()), std::get<3>(GetParam()),
                               std::get<4>(GetParam())) {}

 protected:
  bool SkipIfCipherSuiteIsDSA() {
    bool isDSA = csinfo_.authType == ssl_auth_dsa;
    if (isDSA) {
      std::cerr << "Skipping DSA suite: " << csinfo_.cipherSuiteName
                << std::endl;
    }
    return isDSA;
  }
};

TEST_P(TlsCipherSuiteTest, SingleCipherSuite) {
  SetupCertificate();
  EnableSingleCipher();
  ConnectAndCheckCipherSuite();
}

TEST_P(TlsCipherSuiteTest, ResumeCipherSuite) {
  if (SkipIfCipherSuiteIsDSA()) {
    return;  // Tickets don't work with DSA (bug 1174677).
  }

  SetupCertificate();  // This is only needed once.

  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  EnableSingleCipher();

  ConnectAndCheckCipherSuite();

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  EnableSingleCipher();
  ExpectResumption(RESUME_TICKET);
  ConnectAndCheckCipherSuite();
}

TEST_P(TlsCipherSuiteTest, ReadLimit) {
  SetupCertificate();
  EnableSingleCipher();
  ConnectAndCheckCipherSuite();
  if (version_ < SSL_LIBRARY_VERSION_TLS_1_3) {
    uint64_t last = last_safe_write();
    EXPECT_EQ(SECSuccess, SSLInt_AdvanceWriteSeqNum(client_->ssl_fd(), last));
    EXPECT_EQ(SECSuccess, SSLInt_AdvanceReadSeqNum(server_->ssl_fd(), last));

    client_->SendData(10, 10);
    server_->ReadBytes();  // This should be OK.
  } else {
    // In TLS 1.3, reading or writing triggers a KeyUpdate.  That would mean
    // that the sequence numbers would reset and we wouldn't hit the limit.  So
    // we move the sequence number to one less than the limit directly and don't
    // test sending and receiving just before the limit.
    uint64_t last = record_limit() - 1;
    EXPECT_EQ(SECSuccess, SSLInt_AdvanceReadSeqNum(server_->ssl_fd(), last));
  }

  // The payload needs to be big enough to pass for encrypted.  The code checks
  // the limit before it tries to decrypt.
  static const uint8_t payload[32] = {6};
  DataBuffer record;
  uint64_t epoch;
  if (variant_ == ssl_variant_datagram) {
    if (version_ == SSL_LIBRARY_VERSION_TLS_1_3) {
      epoch = 3;  // Application traffic keys.
    } else {
      epoch = 1;
    }
  } else {
    epoch = 0;
  }
  TlsAgentTestBase::MakeRecord(variant_, kTlsApplicationDataType, version_,
                               payload, sizeof(payload), &record,
                               (epoch << 48) | record_limit());
  client_->SendDirect(record);
  server_->ExpectReadWriteError();
  server_->ReadBytes();
  EXPECT_EQ(SSL_ERROR_TOO_MANY_RECORDS, server_->error_code());
}

TEST_P(TlsCipherSuiteTest, WriteLimit) {
  // This asserts in TLS 1.3 because we expect an automatic update.
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    return;
  }
  SetupCertificate();
  EnableSingleCipher();
  ConnectAndCheckCipherSuite();
  EXPECT_EQ(SECSuccess,
            SSLInt_AdvanceWriteSeqNum(client_->ssl_fd(), last_safe_write()));
  client_->SendData(10, 10);
  client_->ExpectReadWriteError();
  client_->SendData(10, 10);
  EXPECT_EQ(SSL_ERROR_TOO_MANY_RECORDS, client_->error_code());
}

// This awful macro makes the test instantiations easier to read.
#define INSTANTIATE_CIPHER_TEST_P(name, modes, versions, groups, sigalgs, ...) \
  static const uint16_t k##name##CiphersArr[] = {__VA_ARGS__};                 \
  static const ::testing::internal::ParamGenerator<uint16_t>                   \
      k##name##Ciphers = ::testing::ValuesIn(k##name##CiphersArr);             \
  INSTANTIATE_TEST_CASE_P(                                                     \
      CipherSuite##name, TlsCipherSuiteTest,                                   \
      ::testing::Combine(TlsConnectTestBase::kTlsVariants##modes,              \
                         TlsConnectTestBase::kTls##versions, k##name##Ciphers, \
                         groups, sigalgs));

static const auto kDummyNamedGroupParams = ::testing::Values(ssl_grp_none);
static const auto kDummySignatureSchemesParams =
    ::testing::Values(ssl_sig_none);

#ifndef NSS_DISABLE_TLS_1_3
static SSLSignatureScheme kSignatureSchemesParamsArr[] = {
    ssl_sig_rsa_pkcs1_sha256,       ssl_sig_rsa_pkcs1_sha384,
    ssl_sig_rsa_pkcs1_sha512,       ssl_sig_ecdsa_secp256r1_sha256,
    ssl_sig_ecdsa_secp384r1_sha384, ssl_sig_rsa_pss_sha256,
    ssl_sig_rsa_pss_sha384,         ssl_sig_rsa_pss_sha512,
};
#endif

INSTANTIATE_CIPHER_TEST_P(RC4, Stream, V10ToV12, kDummyNamedGroupParams,
                          kDummySignatureSchemesParams,
                          TLS_RSA_WITH_RC4_128_SHA,
                          TLS_ECDH_ECDSA_WITH_RC4_128_SHA,
                          TLS_ECDHE_ECDSA_WITH_RC4_128_SHA,
                          TLS_ECDH_RSA_WITH_RC4_128_SHA,
                          TLS_ECDHE_RSA_WITH_RC4_128_SHA);
INSTANTIATE_CIPHER_TEST_P(AEAD12, All, V12, kDummyNamedGroupParams,
                          kDummySignatureSchemesParams,
                          TLS_RSA_WITH_AES_128_GCM_SHA256,
                          TLS_RSA_WITH_AES_256_GCM_SHA384,
                          TLS_DHE_DSS_WITH_AES_128_GCM_SHA256,
                          TLS_DHE_DSS_WITH_AES_256_GCM_SHA384,
                          TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
                          TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384);
INSTANTIATE_CIPHER_TEST_P(AEAD, All, V12, kDummyNamedGroupParams,
                          kDummySignatureSchemesParams,
                          TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
                          TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
                          TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
                          TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
                          TLS_DHE_RSA_WITH_AES_128_GCM_SHA256,
                          TLS_DHE_RSA_WITH_AES_256_GCM_SHA384,
                          TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
                          TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
                          TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256);
INSTANTIATE_CIPHER_TEST_P(
    CBC12, All, V12, kDummyNamedGroupParams, kDummySignatureSchemesParams,
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA256, TLS_RSA_WITH_AES_256_CBC_SHA256,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256, TLS_DHE_RSA_WITH_AES_128_CBC_SHA256,
    TLS_RSA_WITH_AES_128_CBC_SHA256, TLS_DHE_DSS_WITH_AES_128_CBC_SHA256,
    TLS_DHE_DSS_WITH_AES_256_CBC_SHA256);
INSTANTIATE_CIPHER_TEST_P(
    CBCStream, Stream, V10ToV12, kDummyNamedGroupParams,
    kDummySignatureSchemesParams, TLS_ECDH_ECDSA_WITH_NULL_SHA,
    TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA, TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA, TLS_ECDHE_ECDSA_WITH_NULL_SHA,
    TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA, TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA, TLS_ECDH_RSA_WITH_NULL_SHA,
    TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA, TLS_ECDH_RSA_WITH_AES_128_CBC_SHA,
    TLS_ECDH_RSA_WITH_AES_256_CBC_SHA, TLS_ECDHE_RSA_WITH_NULL_SHA,
    TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA, TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA);
INSTANTIATE_CIPHER_TEST_P(
    CBCDatagram, Datagram, V11V12, kDummyNamedGroupParams,
    kDummySignatureSchemesParams, TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA, TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA, TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA, TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDH_RSA_WITH_AES_128_CBC_SHA, TLS_ECDH_RSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA, TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA);
#ifndef NSS_DISABLE_TLS_1_3
INSTANTIATE_CIPHER_TEST_P(TLS13, All, V13,
                          ::testing::ValuesIn(kFasterDHEGroups),
                          ::testing::ValuesIn(kSignatureSchemesParamsArr),
                          TLS_AES_128_GCM_SHA256, TLS_CHACHA20_POLY1305_SHA256,
                          TLS_AES_256_GCM_SHA384);
INSTANTIATE_CIPHER_TEST_P(TLS13AllGroups, All, V13,
                          ::testing::ValuesIn(kAllDHEGroups),
                          ::testing::Values(ssl_sig_ecdsa_secp384r1_sha384),
                          TLS_AES_256_GCM_SHA384);
#endif

// Fields are: version, cipher suite, bulk cipher name, secretKeySize
struct SecStatusParams {
  uint16_t version;
  uint16_t cipher_suite;
  std::string name;
  int keySize;
};

inline std::ostream &operator<<(std::ostream &stream,
                                const SecStatusParams &vals) {
  SSLCipherSuiteInfo csinfo;
  SECStatus rv =
      SSL_GetCipherSuiteInfo(vals.cipher_suite, &csinfo, sizeof(csinfo));
  if (rv != SECSuccess) {
    return stream << "Error invoking SSL_GetCipherSuiteInfo()";
  }

  return stream << "TLS " << VersionString(vals.version) << ", "
                << csinfo.cipherSuiteName << ", name = \"" << vals.name
                << "\", key size = " << vals.keySize;
}

class SecurityStatusTest
    : public TlsCipherSuiteTestBase,
      public ::testing::WithParamInterface<SecStatusParams> {
 public:
  SecurityStatusTest()
      : TlsCipherSuiteTestBase(ssl_variant_stream, GetParam().version,
                               GetParam().cipher_suite, ssl_grp_none,
                               ssl_sig_none) {}
};

// SSL_SecurityStatus produces fairly useless output when compared to
// SSL_GetCipherSuiteInfo and SSL_GetChannelInfo, but we can't break it, so we
// need to check it.
TEST_P(SecurityStatusTest, CheckSecurityStatus) {
  SetupCertificate();
  EnableSingleCipher();
  ConnectAndCheckCipherSuite();

  int on;
  char *cipher;
  int keySize;
  int secretKeySize;
  char *issuer;
  char *subject;
  EXPECT_EQ(SECSuccess,
            SSL_SecurityStatus(client_->ssl_fd(), &on, &cipher, &keySize,
                               &secretKeySize, &issuer, &subject));
  if (std::string(cipher) == "NULL") {
    EXPECT_EQ(0, on);
  } else {
    EXPECT_NE(0, on);
  }
  EXPECT_EQ(GetParam().name, std::string(cipher));
  // All the ciphers we support have secret key size == key size.
  EXPECT_EQ(GetParam().keySize, keySize);
  EXPECT_EQ(GetParam().keySize, secretKeySize);
  EXPECT_LT(0U, strlen(issuer));
  EXPECT_LT(0U, strlen(subject));

  PORT_Free(cipher);
  PORT_Free(issuer);
  PORT_Free(subject);
}

static const SecStatusParams kSecStatusTestValuesArr[] = {
    {SSL_LIBRARY_VERSION_TLS_1_0, TLS_ECDHE_RSA_WITH_NULL_SHA, "NULL", 0},
    {SSL_LIBRARY_VERSION_TLS_1_0, TLS_RSA_WITH_RC4_128_SHA, "RC4", 128},
    {SSL_LIBRARY_VERSION_TLS_1_0, TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,
     "3DES-EDE-CBC", 168},
    {SSL_LIBRARY_VERSION_TLS_1_0, TLS_RSA_WITH_AES_128_CBC_SHA, "AES-128", 128},
    {SSL_LIBRARY_VERSION_TLS_1_2, TLS_RSA_WITH_AES_256_CBC_SHA256, "AES-256",
     256},
    {SSL_LIBRARY_VERSION_TLS_1_2, TLS_RSA_WITH_AES_128_GCM_SHA256,
     "AES-128-GCM", 128},
    {SSL_LIBRARY_VERSION_TLS_1_2, TLS_RSA_WITH_AES_256_GCM_SHA384,
     "AES-256-GCM", 256},
    {SSL_LIBRARY_VERSION_TLS_1_2, TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
     "ChaCha20-Poly1305", 256}};
INSTANTIATE_TEST_CASE_P(TestSecurityStatus, SecurityStatusTest,
                        ::testing::ValuesIn(kSecStatusTestValuesArr));

}  // namespace nspr_test
