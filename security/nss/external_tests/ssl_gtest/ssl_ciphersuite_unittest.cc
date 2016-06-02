/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secerr.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"
#include <memory>
#include <functional>

#include "tls_connect.h"
#include "gtest_utils.h"

namespace nss_test {

// mode, version, cipher suite
typedef std::tuple<std::string, uint16_t, uint16_t> CipherSuiteProfile;

class TlsCipherSuiteTest
    : public TlsConnectTestBase,
      public ::testing::WithParamInterface<CipherSuiteProfile> {
 public:

  TlsCipherSuiteTest()
      : TlsConnectTestBase(TlsConnectTestBase::ToMode(std::get<0>(GetParam())),
                           std::get<1>(GetParam())),
        cipher_suite_(std::get<2>(GetParam()))
      {}

 protected:
  uint16_t cipher_suite_;

  void SetupCertificate() {
    SSLCipherSuiteInfo csinfo;
    EXPECT_EQ(SECSuccess, SSL_GetCipherSuiteInfo(cipher_suite_,
                                                 &csinfo, sizeof(csinfo)));
    std::cerr << "Cipher suite: " << csinfo.cipherSuiteName << std::endl;
    switch (csinfo.authType) {
      case ssl_auth_rsa_sign:
        Reset(TlsAgent::kServerRsaSign);
        break;
      case ssl_auth_rsa_decrypt:
        Reset(TlsAgent::kServerRsaDecrypt);
        break;
      case ssl_auth_ecdsa:
        Reset(TlsAgent::kServerEcdsa);
        break;
      case ssl_auth_ecdh_ecdsa:
        Reset(TlsAgent::kServerEcdhEcdsa);
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
};


TEST_P(TlsCipherSuiteTest, ConnectWithCipherSuite) {
  SetupCertificate();
  EnableSingleCipher();

  Connect();
  SendReceive();

  // Check that we used the right cipher suite.
  uint16_t actual;
  EXPECT_TRUE(client_->cipher_suite(&actual) && actual == cipher_suite_);
  EXPECT_TRUE(server_->cipher_suite(&actual) && actual == cipher_suite_);
}

// This awful macro makes the test instantiations easier to read.
#define INSTANTIATE_CIPHER_TEST_P(name, modes, versions, ...)           \
  static const uint16_t k##name##CiphersArr[] = { __VA_ARGS__ };        \
  static const ::testing::internal::ParamGenerator<uint16_t>            \
    k##name##Ciphers = ::testing::ValuesIn(k##name##CiphersArr);        \
  INSTANTIATE_TEST_CASE_P(CipherSuite##name, TlsCipherSuiteTest,        \
                          ::testing::Combine(                           \
                               TlsConnectTestBase::kTlsModes##modes,    \
                               TlsConnectTestBase::kTls##versions,      \
                               k##name##Ciphers))

// The cipher suites of type ECDH_RSA are disabled because we don't have
// a compatible certificate in the test fixture.
INSTANTIATE_CIPHER_TEST_P(RC4, Stream, V10ToV12,
                          TLS_RSA_WITH_RC4_128_SHA,
                          TLS_ECDH_ECDSA_WITH_RC4_128_SHA,
                          TLS_ECDHE_ECDSA_WITH_RC4_128_SHA,
                          // TLS_ECDH_RSA_WITH_RC4_128_SHA,
                          TLS_ECDHE_RSA_WITH_RC4_128_SHA);
// TODO - Bug 1251136 move DHE_RSA suites to V12Plus
INSTANTIATE_CIPHER_TEST_P(AEAD12, All, V12,
                          TLS_RSA_WITH_AES_128_GCM_SHA256,
                          TLS_DHE_DSS_WITH_AES_128_GCM_SHA256,
                          TLS_DHE_DSS_WITH_AES_256_GCM_SHA384,
                          TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
                          TLS_RSA_WITH_AES_256_GCM_SHA384,
                          TLS_DHE_RSA_WITH_AES_256_GCM_SHA384,
                          TLS_DHE_RSA_WITH_AES_128_GCM_SHA256,
                          TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
                          TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384);
INSTANTIATE_CIPHER_TEST_P(AEAD, All, V12Plus,
                          TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
                          TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
                          TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
                          TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
                          TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
                          TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256);
INSTANTIATE_CIPHER_TEST_P(CBC12, All, V12,
                          TLS_DHE_RSA_WITH_AES_256_CBC_SHA256,
                          TLS_RSA_WITH_AES_256_CBC_SHA256,
                          TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
                          TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
                          TLS_DHE_RSA_WITH_AES_128_CBC_SHA256,
                          TLS_RSA_WITH_AES_128_CBC_SHA256,
                          TLS_DHE_DSS_WITH_AES_128_CBC_SHA256,
                          TLS_DHE_DSS_WITH_AES_256_CBC_SHA256,
                          TLS_RSA_WITH_NULL_SHA256);
INSTANTIATE_CIPHER_TEST_P(CBCStream, Stream, V10ToV12,
                          TLS_ECDH_ECDSA_WITH_NULL_SHA,
                          TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA,
                          TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA,
                          TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA,
                          TLS_ECDHE_ECDSA_WITH_NULL_SHA,
                          TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,
                          TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
                          TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
                          // TLS_ECDH_RSA_WITH_NULL_SHA,
                          // TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA,
                          // TLS_ECDH_RSA_WITH_AES_128_CBC_SHA,
                          // TLS_ECDH_RSA_WITH_AES_256_CBC_SHA,
                          TLS_ECDHE_RSA_WITH_NULL_SHA,
                          TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,
                          TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
                          TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA);
INSTANTIATE_CIPHER_TEST_P(CBCDatagram, Datagram, V11V12,
                          TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA,
                          TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA,
                          TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA,
                          TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,
                          TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
                          TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
                          // TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA,
                          // TLS_ECDH_RSA_WITH_AES_128_CBC_SHA,
                          // TLS_ECDH_RSA_WITH_AES_256_CBC_SHA,
                          TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,
                          TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
                          TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA);

}  // namespace nspr_test
