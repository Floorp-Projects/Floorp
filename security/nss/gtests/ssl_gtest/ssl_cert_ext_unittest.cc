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
  SignedCertificateTimestampsExtractor(TlsAgent* client) {
    client->SetAuthCertificateCallback(
        [&](TlsAgent* agent, bool checksig, bool isServer) -> SECStatus {
          const SECItem* scts = SSL_PeerSignedCertTimestamps(agent->ssl_fd());
          EXPECT_TRUE(scts);
          if (!scts) {
            return SECFailure;
          }
          auth_timestamps_.reset(new DataBuffer(scts->data, scts->len));
          return SECSuccess;
        });
    client->SetHandshakeCallback([&](TlsAgent* agent) {
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
  }

 private:
  std::unique_ptr<DataBuffer> auth_timestamps_;
  std::unique_ptr<DataBuffer> handshake_timestamps_;
};

static const uint8_t kSctValue[] = {0x01, 0x23, 0x45, 0x67, 0x89};
static const SECItem kSctItem = {siBuffer, const_cast<uint8_t*>(kSctValue),
                                 sizeof(kSctValue)};
static const DataBuffer kSctBuffer(kSctValue, sizeof(kSctValue));

// Test timestamps extraction during a successful handshake.
TEST_P(TlsConnectGenericPre13, SignedCertificateTimestampsHandshake) {
  EnsureTlsSetup();
  EXPECT_EQ(SECSuccess, SSL_SetSignedCertTimestamps(server_->ssl_fd(),
                                                    &kSctItem, ssl_kea_rsa));
  EXPECT_EQ(SECSuccess,
            SSL_OptionSet(client_->ssl_fd(), SSL_ENABLE_SIGNED_CERT_TIMESTAMPS,
                          PR_TRUE));
  SignedCertificateTimestampsExtractor timestamps_extractor(client_);

  Connect();

  timestamps_extractor.assertTimestamps(kSctBuffer);
  const SECItem* c_timestamps = SSL_PeerSignedCertTimestamps(client_->ssl_fd());
  EXPECT_EQ(SECEqual, SECITEM_CompareItem(&kSctItem, c_timestamps));
}

TEST_P(TlsConnectGenericPre13, SignedCertificateTimestampsConfig) {
  static const SSLExtraServerCertData kExtraData = {ssl_auth_rsa_sign, nullptr,
                                                    nullptr, &kSctItem};

  EnsureTlsSetup();
  EXPECT_TRUE(
      server_->ConfigServerCert(TlsAgent::kServerRsa, true, &kExtraData));
  EXPECT_EQ(SECSuccess,
            SSL_OptionSet(client_->ssl_fd(), SSL_ENABLE_SIGNED_CERT_TIMESTAMPS,
                          PR_TRUE));
  SignedCertificateTimestampsExtractor timestamps_extractor(client_);

  Connect();

  timestamps_extractor.assertTimestamps(kSctBuffer);
  const SECItem* c_timestamps = SSL_PeerSignedCertTimestamps(client_->ssl_fd());
  EXPECT_EQ(SECEqual, SECITEM_CompareItem(&kSctItem, c_timestamps));
}

// Test SSL_PeerSignedCertTimestamps returning zero-length SECItem
// when the client / the server / both have not enabled the feature.
TEST_P(TlsConnectGenericPre13, SignedCertificateTimestampsInactiveClient) {
  EnsureTlsSetup();
  EXPECT_EQ(SECSuccess, SSL_SetSignedCertTimestamps(server_->ssl_fd(),
                                                    &kSctItem, ssl_kea_rsa));
  SignedCertificateTimestampsExtractor timestamps_extractor(client_);

  Connect();
  timestamps_extractor.assertTimestamps(DataBuffer());
}

TEST_P(TlsConnectGenericPre13, SignedCertificateTimestampsInactiveServer) {
  EnsureTlsSetup();
  EXPECT_EQ(SECSuccess,
            SSL_OptionSet(client_->ssl_fd(), SSL_ENABLE_SIGNED_CERT_TIMESTAMPS,
                          PR_TRUE));
  SignedCertificateTimestampsExtractor timestamps_extractor(client_);

  Connect();
  timestamps_extractor.assertTimestamps(DataBuffer());
}

TEST_P(TlsConnectGenericPre13, SignedCertificateTimestampsInactiveBoth) {
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
    ssl_auth_rsa_sign, nullptr, &kOcspResponses, nullptr};

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
  EXPECT_EQ(SECSuccess, SSL_OptionSet(client_->ssl_fd(),
                                      SSL_ENABLE_OCSP_STAPLING, PR_TRUE));
  client_->SetAuthCertificateCallback(CheckNoOCSP);
  Connect();
}

TEST_P(TlsConnectGenericPre13, OcspMangled) {
  EnsureTlsSetup();
  EXPECT_EQ(SECSuccess, SSL_OptionSet(client_->ssl_fd(),
                                      SSL_ENABLE_OCSP_STAPLING, PR_TRUE));
  EXPECT_TRUE(
      server_->ConfigServerCert(TlsAgent::kServerRsa, true, &kOcspExtraData));

  static const uint8_t val[] = {1};
  auto replacer = new TlsExtensionReplacer(ssl_cert_status_xtn,
                                           DataBuffer(val, sizeof(val)));
  server_->SetPacketFilter(replacer);
  ConnectExpectFail();
  client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_SERVER_HELLO);
  server_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

TEST_P(TlsConnectGeneric, OcspSuccess) {
  EnsureTlsSetup();
  EXPECT_EQ(SECSuccess, SSL_OptionSet(client_->ssl_fd(),
                                      SSL_ENABLE_OCSP_STAPLING, PR_TRUE));
  auto capture_ocsp = new TlsExtensionCapture(ssl_cert_status_xtn);
  server_->SetPacketFilter(capture_ocsp);

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

}  // namespace nspr_test
