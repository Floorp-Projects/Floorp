/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ssl.h"
#include <functional>
#include <memory>
#include "secerr.h"
#include "sslerr.h"
#include "sslproto.h"

#include "gtest_utils.h"
#include "tls_connect.h"

namespace nss_test {

// mode, version, cipher suite
typedef std::tuple<std::string, uint16_t, uint16_t> CipherSuiteProfile;

class TlsCipherSuiteTestBase : public TlsConnectTestBase {
 public:
  TlsCipherSuiteTestBase(std::string mode, uint16_t version,
                         uint16_t cipher_suite)
      : TlsConnectTestBase(TlsConnectTestBase::ToMode(mode), version),
        cipher_suite_(cipher_suite) {}

 protected:
  uint16_t cipher_suite_;

  void SetupCertificate() {
    SSLCipherSuiteInfo csinfo;
    EXPECT_EQ(SECSuccess,
              SSL_GetCipherSuiteInfo(cipher_suite_, &csinfo, sizeof(csinfo)));
    std::cerr << "Cipher suite: " << csinfo.cipherSuiteName << std::endl;
    switch (csinfo.authType) {
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

  void EnableSingleCipher() {
    EnsureTlsSetup();
    // It doesn't matter which does this, but the test is better if both do it.
    client_->EnableSingleCipher(cipher_suite_);
    server_->EnableSingleCipher(cipher_suite_);
  }

  void ConnectAndCheckCipherSuite() {
    Connect();
    SendReceive();

    // Check that we used the right cipher suite.
    uint16_t actual;
    EXPECT_TRUE(client_->cipher_suite(&actual) && actual == cipher_suite_);
    EXPECT_TRUE(server_->cipher_suite(&actual) && actual == cipher_suite_);
  }
};

class TlsCipherSuiteTest
    : public TlsCipherSuiteTestBase,
      public ::testing::WithParamInterface<CipherSuiteProfile> {
 public:
  TlsCipherSuiteTest()
      : TlsCipherSuiteTestBase(std::get<0>(GetParam()), std::get<1>(GetParam()),
                               std::get<2>(GetParam())) {}
};

TEST_P(TlsCipherSuiteTest, SingleCipherSuite) {
  SetupCertificate();
  EnableSingleCipher();
  ConnectAndCheckCipherSuite();
}

class TlsResumptionTest
    : public TlsCipherSuiteTestBase,
      public ::testing::WithParamInterface<CipherSuiteProfile> {
 public:
  TlsResumptionTest()
      : TlsCipherSuiteTestBase(std::get<0>(GetParam()), std::get<1>(GetParam()),
                               std::get<2>(GetParam())) {}

  bool SkipIfCipherSuiteIsDSA() {
    SSLCipherSuiteInfo csinfo;
    SECStatus rv =
        SSL_GetCipherSuiteInfo(cipher_suite_, &csinfo, sizeof(csinfo));
    if (rv != SECSuccess) {
      EXPECT_TRUE(false) << "Can't get cipher suite info";
      return false;
    }
    bool isDSA = csinfo.authType == ssl_auth_dsa;
    if (isDSA) {
      std::cerr << "Skipping DSA suite: " << csinfo.cipherSuiteName
                << std::endl;
    }
    return isDSA;
  }

  void EnablePskCipherSuite() {
    SSLCipherSuiteInfo targetInfo;
    ASSERT_EQ(SECSuccess, SSL_GetCipherSuiteInfo(cipher_suite_, &targetInfo,
                                                 sizeof(targetInfo)));
    SSLKEAType targetKea;
    switch (targetInfo.keaType) {
      case ssl_kea_ecdh:
        targetKea = ssl_kea_ecdh_psk;
        break;
      case ssl_kea_dh:
        targetKea = ssl_kea_dh_psk;
        break;
      default:
        EXPECT_TRUE(false) << "Unsupported KEA type for "
                           << targetInfo.cipherSuiteName;
        return;
    }

    size_t count = SSL_GetNumImplementedCiphers();
    const uint16_t *ciphers = SSL_GetImplementedCiphers();
    bool found = false;
    for (size_t i = 0; i < count; ++i) {
      SSLCipherSuiteInfo candidateInfo;
      ASSERT_EQ(SECSuccess, SSL_GetCipherSuiteInfo(ciphers[i], &candidateInfo,
                                                   sizeof(candidateInfo)));
      if (candidateInfo.authType == ssl_auth_psk &&
          candidateInfo.keaType == targetKea &&
          candidateInfo.symCipher == targetInfo.symCipher &&
          candidateInfo.macAlgorithm == targetInfo.macAlgorithm) {
        // We aren't able to check that the PRF hash is the same.  This is OK
        // because there are (currently) no suites that have different PRF
        // hashes but also use the same symmetric cipher.
        EXPECT_EQ(SECSuccess,
                  SSL_CipherPrefSet(client_->ssl_fd(), ciphers[i], PR_TRUE));
        EXPECT_EQ(SECSuccess,
                  SSL_CipherPrefSet(server_->ssl_fd(), ciphers[i], PR_TRUE));
        found = true;
      }
    }
    EXPECT_TRUE(found) << "Can't find matching PSK cipher for "
                       << targetInfo.cipherSuiteName;
  }
};

TEST_P(TlsResumptionTest, ResumeCipherSuite) {
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
  // Enable a PSK cipher suite, since EnableSingleCipher() disabled all of them.
  // On resumption in TLS 1.3, even though a PSK cipher suite is negotiated, the
  // original cipher suite is reported through the API.  That is what makes this
  // test work without more tweaks.
  if (version_ == SSL_LIBRARY_VERSION_TLS_1_3) {
    EnablePskCipherSuite();
  }

  ExpectResumption(RESUME_TICKET);
  ConnectAndCheckCipherSuite();
}

// This awful macro makes the test instantiations easier to read.
#define INSTANTIATE_CIPHER_TEST_P(name, modes, versions, ...)      \
  static const uint16_t k##name##CiphersArr[] = {__VA_ARGS__};     \
  static const ::testing::internal::ParamGenerator<uint16_t>       \
      k##name##Ciphers = ::testing::ValuesIn(k##name##CiphersArr); \
  INSTANTIATE_TEST_CASE_P(                                         \
      CipherSuite##name, TlsCipherSuiteTest,                       \
      ::testing::Combine(TlsConnectTestBase::kTlsModes##modes,     \
                         TlsConnectTestBase::kTls##versions,       \
                         k##name##Ciphers));                       \
  INSTANTIATE_TEST_CASE_P(                                         \
      Resume##name, TlsResumptionTest,                             \
      ::testing::Combine(TlsConnectTestBase::kTlsModes##modes,     \
                         TlsConnectTestBase::kTls##versions,       \
                         k##name##Ciphers))

INSTANTIATE_CIPHER_TEST_P(RC4, Stream, V10ToV12, TLS_RSA_WITH_RC4_128_SHA,
                          TLS_ECDH_ECDSA_WITH_RC4_128_SHA,
                          TLS_ECDHE_ECDSA_WITH_RC4_128_SHA,
                          TLS_ECDH_RSA_WITH_RC4_128_SHA,
                          TLS_ECDHE_RSA_WITH_RC4_128_SHA);
INSTANTIATE_CIPHER_TEST_P(AEAD12, All, V12, TLS_RSA_WITH_AES_128_GCM_SHA256,
                          TLS_RSA_WITH_AES_256_GCM_SHA384,
                          TLS_DHE_DSS_WITH_AES_128_GCM_SHA256,
                          TLS_DHE_DSS_WITH_AES_256_GCM_SHA384,
                          TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
                          TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384);
INSTANTIATE_CIPHER_TEST_P(AEAD, All, V12Plus,
                          TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
                          TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
                          TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
                          TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
                          TLS_DHE_RSA_WITH_AES_128_GCM_SHA256,
                          TLS_DHE_RSA_WITH_AES_256_GCM_SHA384,
                          TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
                          TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
                          TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256);
INSTANTIATE_CIPHER_TEST_P(CBC12, All, V12, TLS_DHE_RSA_WITH_AES_256_CBC_SHA256,
                          TLS_RSA_WITH_AES_256_CBC_SHA256,
                          TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
                          TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
                          TLS_DHE_RSA_WITH_AES_128_CBC_SHA256,
                          TLS_RSA_WITH_AES_128_CBC_SHA256,
                          TLS_DHE_DSS_WITH_AES_128_CBC_SHA256,
                          TLS_DHE_DSS_WITH_AES_256_CBC_SHA256);
INSTANTIATE_CIPHER_TEST_P(
    CBCStream, Stream, V10ToV12, TLS_ECDH_ECDSA_WITH_NULL_SHA,
    TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA, TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA, TLS_ECDHE_ECDSA_WITH_NULL_SHA,
    TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA, TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA, TLS_ECDH_RSA_WITH_NULL_SHA,
    TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA, TLS_ECDH_RSA_WITH_AES_128_CBC_SHA,
    TLS_ECDH_RSA_WITH_AES_256_CBC_SHA, TLS_ECDHE_RSA_WITH_NULL_SHA,
    TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA, TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA);
INSTANTIATE_CIPHER_TEST_P(
    CBCDatagram, Datagram, V11V12, TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA, TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA, TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA, TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDH_RSA_WITH_AES_128_CBC_SHA, TLS_ECDH_RSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA, TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA);

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
      : TlsCipherSuiteTestBase("TLS", GetParam().version,
                               GetParam().cipher_suite) {}
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
