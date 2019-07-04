/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"

#include <memory>

#include "tls_connect.h"
#include "tls_filter.h"
#include "tls_parser.h"

namespace nss_test {

// Tests for Certificate Transparency (RFC 6962)
// These don't work with TLS 1.3: see bug 1252745.

// Helper class - stores signed certificate timestamps as provided
// by the relevant callbacks on the client.
class SignedCertificateTimestampsExtractor {
 public:
  SignedCertificateTimestampsExtractor(std::shared_ptr<TlsAgent>& client)
      : client_(client) {
    client->SetAuthCertificateCallback(
        [this](TlsAgent* agent, bool checksig, bool isServer) -> SECStatus {
          const SECItem* scts = SSL_PeerSignedCertTimestamps(agent->ssl_fd());
          EXPECT_TRUE(scts);
          if (!scts) {
            return SECFailure;
          }
          auth_timestamps_.reset(new DataBuffer(scts->data, scts->len));
          return SECSuccess;
        });
    client->SetHandshakeCallback([this](TlsAgent* agent) {
      const SECItem* scts = SSL_PeerSignedCertTimestamps(agent->ssl_fd());
      ASSERT_TRUE(scts);
      handshake_timestamps_.reset(new DataBuffer(scts->data, scts->len));
    });
  }

  void assertTimestamps(const DataBuffer& timestamps) {
    EXPECT_TRUE(auth_timestamps_);
    EXPECT_EQ(timestamps, *auth_timestamps_);

    EXPECT_TRUE(handshake_timestamps_);
    EXPECT_EQ(timestamps, *handshake_timestamps_);

    const SECItem* current =
        SSL_PeerSignedCertTimestamps(client_.lock()->ssl_fd());
    EXPECT_EQ(timestamps, DataBuffer(current->data, current->len));
  }

 private:
  std::weak_ptr<TlsAgent> client_;
  std::unique_ptr<DataBuffer> auth_timestamps_;
  std::unique_ptr<DataBuffer> handshake_timestamps_;
};

static const uint8_t kSctValue[] = {0x01, 0x23, 0x45, 0x67, 0x89};
static const SECItem kSctItem = {siBuffer, const_cast<uint8_t*>(kSctValue),
                                 sizeof(kSctValue)};
static const DataBuffer kSctBuffer(kSctValue, sizeof(kSctValue));
static const SSLExtraServerCertData kExtraSctData = {
    ssl_auth_null, nullptr, nullptr, &kSctItem, nullptr, nullptr};

// Test timestamps extraction during a successful handshake.
TEST_P(TlsConnectGenericPre13, SignedCertificateTimestampsLegacy) {
  EnsureTlsSetup();

  // We have to use the legacy API consistently here for configuring certs.
  // Also, this doesn't work in TLS 1.3 because this only configures the SCT for
  // RSA decrypt and PKCS#1 signing, not PSS.
  ScopedCERTCertificate cert;
  ScopedSECKEYPrivateKey priv;
  ASSERT_TRUE(TlsAgent::LoadCertificate(TlsAgent::kServerRsa, &cert, &priv));
  EXPECT_EQ(SECSuccess, SSL_ConfigSecureServerWithCertChain(
                            server_->ssl_fd(), cert.get(), nullptr, priv.get(),
                            ssl_kea_rsa));
  EXPECT_EQ(SECSuccess, SSL_SetSignedCertTimestamps(server_->ssl_fd(),
                                                    &kSctItem, ssl_kea_rsa));

  client_->SetOption(SSL_ENABLE_SIGNED_CERT_TIMESTAMPS, PR_TRUE);
  SignedCertificateTimestampsExtractor timestamps_extractor(client_);

  Connect();

  timestamps_extractor.assertTimestamps(kSctBuffer);
}

TEST_P(TlsConnectGeneric, SignedCertificateTimestampsSuccess) {
  EnsureTlsSetup();
  EXPECT_TRUE(
      server_->ConfigServerCert(TlsAgent::kServerRsa, true, &kExtraSctData));
  client_->SetOption(SSL_ENABLE_SIGNED_CERT_TIMESTAMPS, PR_TRUE);
  SignedCertificateTimestampsExtractor timestamps_extractor(client_);

  Connect();

  timestamps_extractor.assertTimestamps(kSctBuffer);
}

// Test SSL_PeerSignedCertTimestamps returning zero-length SECItem
// when the client / the server / both have not enabled the feature.
TEST_P(TlsConnectGeneric, SignedCertificateTimestampsInactiveClient) {
  EnsureTlsSetup();
  EXPECT_TRUE(
      server_->ConfigServerCert(TlsAgent::kServerRsa, true, &kExtraSctData));
  SignedCertificateTimestampsExtractor timestamps_extractor(client_);

  Connect();
  timestamps_extractor.assertTimestamps(DataBuffer());
}

TEST_P(TlsConnectGeneric, SignedCertificateTimestampsInactiveServer) {
  EnsureTlsSetup();
  client_->SetOption(SSL_ENABLE_SIGNED_CERT_TIMESTAMPS, PR_TRUE);
  SignedCertificateTimestampsExtractor timestamps_extractor(client_);

  Connect();
  timestamps_extractor.assertTimestamps(DataBuffer());
}

TEST_P(TlsConnectGeneric, SignedCertificateTimestampsInactiveBoth) {
  EnsureTlsSetup();
  SignedCertificateTimestampsExtractor timestamps_extractor(client_);

  Connect();
  timestamps_extractor.assertTimestamps(DataBuffer());
}

// Check that the given agent doesn't have an OCSP response for its peer.
static SECStatus CheckNoOCSP(TlsAgent* agent, bool checksig, bool isServer) {
  const SECItemArray* ocsp = SSL_PeerStapledOCSPResponses(agent->ssl_fd());
  EXPECT_TRUE(ocsp);
  EXPECT_EQ(0U, ocsp->len);
  return SECSuccess;
}

static const uint8_t kOcspValue1[] = {1, 2, 3, 4, 5, 6};
static const uint8_t kOcspValue2[] = {7, 8, 9};
static const SECItem kOcspItems[] = {
    {siBuffer, const_cast<uint8_t*>(kOcspValue1), sizeof(kOcspValue1)},
    {siBuffer, const_cast<uint8_t*>(kOcspValue2), sizeof(kOcspValue2)}};
static const SECItemArray kOcspResponses = {const_cast<SECItem*>(kOcspItems),
                                            PR_ARRAY_SIZE(kOcspItems)};
const static SSLExtraServerCertData kOcspExtraData = {
    ssl_auth_null, nullptr, &kOcspResponses, nullptr, nullptr, nullptr};

TEST_P(TlsConnectGeneric, NoOcsp) {
  EnsureTlsSetup();
  client_->SetAuthCertificateCallback(CheckNoOCSP);
  Connect();
}

// The client doesn't get OCSP stapling unless it asks.
TEST_P(TlsConnectGeneric, OcspNotRequested) {
  EnsureTlsSetup();
  client_->SetAuthCertificateCallback(CheckNoOCSP);
  EXPECT_TRUE(
      server_->ConfigServerCert(TlsAgent::kServerRsa, true, &kOcspExtraData));
  Connect();
}

// Even if the client asks, the server has nothing unless it is configured.
TEST_P(TlsConnectGeneric, OcspNotProvided) {
  EnsureTlsSetup();
  client_->SetOption(SSL_ENABLE_OCSP_STAPLING, PR_TRUE);
  client_->SetAuthCertificateCallback(CheckNoOCSP);
  Connect();
}

TEST_P(TlsConnectGenericPre13, OcspMangled) {
  EnsureTlsSetup();
  client_->SetOption(SSL_ENABLE_OCSP_STAPLING, PR_TRUE);
  EXPECT_TRUE(
      server_->ConfigServerCert(TlsAgent::kServerRsa, true, &kOcspExtraData));

  static const uint8_t val[] = {1};
  auto replacer = MakeTlsFilter<TlsExtensionReplacer>(
      server_, ssl_cert_status_xtn, DataBuffer(val, sizeof(val)));
  ConnectExpectAlert(client_, kTlsAlertIllegalParameter);
  client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_SERVER_HELLO);
  server_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

TEST_P(TlsConnectGeneric, OcspSuccess) {
  EnsureTlsSetup();
  client_->SetOption(SSL_ENABLE_OCSP_STAPLING, PR_TRUE);
  auto capture_ocsp =
      MakeTlsFilter<TlsExtensionCapture>(server_, ssl_cert_status_xtn);

  // The value should be available during the AuthCertificateCallback
  client_->SetAuthCertificateCallback([](TlsAgent* agent, bool checksig,
                                         bool isServer) -> SECStatus {
    const SECItemArray* ocsp = SSL_PeerStapledOCSPResponses(agent->ssl_fd());
    if (!ocsp) {
      return SECFailure;
    }
    EXPECT_EQ(1U, ocsp->len) << "We only provide the first item";
    EXPECT_EQ(0, SECITEM_CompareItem(&kOcspItems[0], &ocsp->items[0]));
    return SECSuccess;
  });
  EXPECT_TRUE(
      server_->ConfigServerCert(TlsAgent::kServerRsa, true, &kOcspExtraData));

  Connect();
  // In TLS 1.3, the server doesn't provide a visible ServerHello extension.
  // For earlier versions, the extension is just empty.
  EXPECT_EQ(0U, capture_ocsp->extension().len());
}

TEST_P(TlsConnectGeneric, OcspHugeSuccess) {
  EnsureTlsSetup();
  client_->SetOption(SSL_ENABLE_OCSP_STAPLING, PR_TRUE);

  uint8_t hugeOcspValue[16385];
  memset(hugeOcspValue, 0xa1, sizeof(hugeOcspValue));
  const SECItem hugeOcspItems[] = {
      {siBuffer, const_cast<uint8_t*>(hugeOcspValue), sizeof(hugeOcspValue)}};
  const SECItemArray hugeOcspResponses = {const_cast<SECItem*>(hugeOcspItems),
                                          PR_ARRAY_SIZE(hugeOcspItems)};
  const SSLExtraServerCertData hugeOcspExtraData = {
      ssl_auth_null, nullptr, &hugeOcspResponses, nullptr, nullptr, nullptr};

  // The value should be available during the AuthCertificateCallback
  client_->SetAuthCertificateCallback([&](TlsAgent* agent, bool checksig,
                                          bool isServer) -> SECStatus {
    const SECItemArray* ocsp = SSL_PeerStapledOCSPResponses(agent->ssl_fd());
    if (!ocsp) {
      return SECFailure;
    }
    EXPECT_EQ(1U, ocsp->len) << "We only provide the first item";
    EXPECT_EQ(0, SECITEM_CompareItem(&hugeOcspItems[0], &ocsp->items[0]));
    return SECSuccess;
  });
  EXPECT_TRUE(server_->ConfigServerCert(TlsAgent::kServerRsa, true,
                                        &hugeOcspExtraData));

  Connect();
}

}  // namespace nss_test
