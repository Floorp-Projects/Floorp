/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secerr.h"
#include "ssl.h"
#include "ssl3prot.h"
#include "sslerr.h"
#include "sslproto.h"

#include "gtest_utils.h"
#include "scoped_ptrs.h"
#include "tls_connect.h"
#include "tls_filter.h"
#include "tls_parser.h"

namespace nss_test {

TEST_P(TlsConnectStream, ServerNegotiateTls10) {
  uint16_t minver, maxver;
  client_->GetVersionRange(&minver, &maxver);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_0, maxver);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_0,
                           SSL_LIBRARY_VERSION_TLS_1_0);
  Connect();
}

TEST_P(TlsConnectGeneric, ServerNegotiateTls11) {
  if (version_ < SSL_LIBRARY_VERSION_TLS_1_1) return;

  uint16_t minver, maxver;
  client_->GetVersionRange(&minver, &maxver);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1, maxver);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_1);
  Connect();
}

TEST_P(TlsConnectGeneric, ServerNegotiateTls12) {
  if (version_ < SSL_LIBRARY_VERSION_TLS_1_2) return;

  uint16_t minver, maxver;
  client_->GetVersionRange(&minver, &maxver);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2, maxver);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  Connect();
}
#ifndef TLS_1_3_DRAFT_VERSION

// Test the ServerRandom version hack from
// [draft-ietf-tls-tls13-11 Section 6.3.1.1].
// The first three tests test for active tampering. The next
// two validate that we can also detect fallback using the
// SSL_SetDowngradeCheckVersion() API.
TEST_F(TlsConnectTest, TestDowngradeDetectionToTls11) {
  client_->SetPacketFilter(
      std::make_shared<TlsInspectorClientHelloVersionSetter>(
          SSL_LIBRARY_VERSION_TLS_1_1));
  ConnectExpectFail();
  ASSERT_EQ(SSL_ERROR_RX_MALFORMED_SERVER_HELLO, client_->error_code());
}

/* Attempt to negotiate the bogus DTLS 1.1 version. */
TEST_F(DtlsConnectTest, TestDtlsVersion11) {
  client_->SetPacketFilter(
      std::make_shared<TlsInspectorClientHelloVersionSetter>(
          ((~0x0101) & 0xffff)));
  ConnectExpectFail();
  // It's kind of surprising that SSL_ERROR_NO_CYPHER_OVERLAP is
  // what is returned here, but this is deliberate in ssl3_HandleAlert().
  EXPECT_EQ(SSL_ERROR_NO_CYPHER_OVERLAP, client_->error_code());
  EXPECT_EQ(SSL_ERROR_UNSUPPORTED_VERSION, server_->error_code());
}

// Disabled as long as we have draft version.
TEST_F(TlsConnectTest, TestDowngradeDetectionToTls12) {
  EnsureTlsSetup();
  client_->SetPacketFilter(
      std::make_shared<TlsInspectorClientHelloVersionSetter>(
          SSL_LIBRARY_VERSION_TLS_1_2));
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  ConnectExpectFail();
  ASSERT_EQ(SSL_ERROR_RX_MALFORMED_SERVER_HELLO, client_->error_code());
}

// TLS 1.1 clients do not check the random values, so we should
// instead get a handshake failure alert from the server.
TEST_F(TlsConnectTest, TestDowngradeDetectionToTls10) {
  client_->SetPacketFilter(
      std::make_shared<TlsInspectorClientHelloVersionSetter>(
          SSL_LIBRARY_VERSION_TLS_1_0));
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_0,
                           SSL_LIBRARY_VERSION_TLS_1_1);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_0,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  ConnectExpectFail();
  ASSERT_EQ(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE, server_->error_code());
  ASSERT_EQ(SSL_ERROR_DECRYPT_ERROR_ALERT, client_->error_code());
}

TEST_F(TlsConnectTest, TestFallbackFromTls12) {
  EnsureTlsSetup();
  client_->SetDowngradeCheckVersion(SSL_LIBRARY_VERSION_TLS_1_2);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_1);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  ConnectExpectFail();
  ASSERT_EQ(SSL_ERROR_RX_MALFORMED_SERVER_HELLO, client_->error_code());
}

TEST_F(TlsConnectTest, TestFallbackFromTls13) {
  EnsureTlsSetup();
  client_->SetDowngradeCheckVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  ConnectExpectFail();
  ASSERT_EQ(SSL_ERROR_RX_MALFORMED_SERVER_HELLO, client_->error_code());
}
#endif

TEST_P(TlsConnectGeneric, TestFallbackSCSVVersionMatch) {
  client_->SetFallbackSCSVEnabled(true);
  Connect();
}

TEST_P(TlsConnectGenericPre13, TestFallbackSCSVVersionMismatch) {
  client_->SetFallbackSCSVEnabled(true);
  server_->SetVersionRange(version_, version_ + 1);
  ConnectExpectFail();
  client_->CheckErrorCode(SSL_ERROR_INAPPROPRIATE_FALLBACK_ALERT);
}

// The TLS v1.3 spec section C.4 states that 'Implementations MUST NOT send or
// accept any records with a version less than { 3, 0 }'. Thus we will not
// allow version ranges including both SSL v3 and TLS v1.3.
TEST_F(TlsConnectTest, DisallowSSLv3HelloWithTLSv13Enabled) {
  SECStatus rv;
  SSLVersionRange vrange = {SSL_LIBRARY_VERSION_3_0,
                            SSL_LIBRARY_VERSION_TLS_1_3};

  EnsureTlsSetup();
  rv = SSL_VersionRangeSet(client_->ssl_fd(), &vrange);
  EXPECT_EQ(SECFailure, rv);

  rv = SSL_VersionRangeSet(server_->ssl_fd(), &vrange);
  EXPECT_EQ(SECFailure, rv);
}

TEST_P(TlsConnectStream, ConnectTls10AndServerRenegotiateHigher) {
  if (version_ == SSL_LIBRARY_VERSION_TLS_1_0) {
    return;
  }
  // Set the client so it will accept any version from 1.0
  // to |version_|.
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_0, version_);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_0,
                           SSL_LIBRARY_VERSION_TLS_1_0);
  // Reset version so that the checks succeed.
  uint16_t test_version = version_;
  version_ = SSL_LIBRARY_VERSION_TLS_1_0;
  Connect();

  // Now renegotiate, with the server being set to do
  // |version_|.
  client_->PrepareForRenegotiate();
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_0, test_version);
  // Reset version and cipher suite so that the preinfo callback
  // doesn't fail.
  server_->ResetPreliminaryInfo();
  server_->StartRenegotiate();
  Handshake();
  if (test_version >= SSL_LIBRARY_VERSION_TLS_1_3) {
    // In TLS 1.3, the server detects this problem.
    client_->CheckErrorCode(SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT);
    server_->CheckErrorCode(SSL_ERROR_RENEGOTIATION_NOT_ALLOWED);
  } else {
    client_->CheckErrorCode(SSL_ERROR_UNSUPPORTED_VERSION);
    server_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
  }
}

TEST_P(TlsConnectStream, ConnectTls10AndClientRenegotiateHigher) {
  if (version_ == SSL_LIBRARY_VERSION_TLS_1_0) {
    return;
  }
  // Set the client so it will accept any version from 1.0
  // to |version_|.
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_0, version_);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_0,
                           SSL_LIBRARY_VERSION_TLS_1_0);
  // Reset version so that the checks succeed.
  uint16_t test_version = version_;
  version_ = SSL_LIBRARY_VERSION_TLS_1_0;
  Connect();

  // Now renegotiate, with the server being set to do
  // |version_|.
  server_->PrepareForRenegotiate();
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_0, test_version);
  // Reset version and cipher suite so that the preinfo callback
  // doesn't fail.
  server_->ResetPreliminaryInfo();
  client_->StartRenegotiate();
  Handshake();
  if (test_version >= SSL_LIBRARY_VERSION_TLS_1_3) {
    // In TLS 1.3, the server detects this problem.
    client_->CheckErrorCode(SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT);
    server_->CheckErrorCode(SSL_ERROR_RENEGOTIATION_NOT_ALLOWED);
  } else {
    client_->CheckErrorCode(SSL_ERROR_UNSUPPORTED_VERSION);
    server_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
  }
}

TEST_F(TlsConnectTest, Tls13RejectsRehandshakeClient) {
  EnsureTlsSetup();
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  Connect();
  SECStatus rv = SSL_ReHandshake(client_->ssl_fd(), PR_TRUE);
  EXPECT_EQ(SECFailure, rv);
  EXPECT_EQ(SSL_ERROR_RENEGOTIATION_NOT_ALLOWED, PORT_GetError());
}

TEST_F(TlsConnectTest, Tls13RejectsRehandshakeServer) {
  EnsureTlsSetup();
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  Connect();
  SECStatus rv = SSL_ReHandshake(server_->ssl_fd(), PR_TRUE);
  EXPECT_EQ(SECFailure, rv);
  EXPECT_EQ(SSL_ERROR_RENEGOTIATION_NOT_ALLOWED, PORT_GetError());
}

TEST_P(TlsConnectGeneric, AlertBeforeServerHello) {
  EnsureTlsSetup();
  client_->SetExpectedAlertReceivedCount(1);
  client_->StartConnect();
  server_->StartConnect();
  client_->Handshake();  // Send ClientHello.
  static const uint8_t kWarningAlert[] = {kTlsAlertWarning,
                                          kTlsAlertUnrecognizedName};
  DataBuffer alert;
  TlsAgentTestBase::MakeRecord(mode_, kTlsAlertType,
                               SSL_LIBRARY_VERSION_TLS_1_0, kWarningAlert,
                               PR_ARRAY_SIZE(kWarningAlert), &alert);
  client_->adapter()->PacketReceived(alert);
  Handshake();
  CheckConnected();
}

class Tls13NoSupportedVersions : public TlsConnectStreamTls12 {
 protected:
  void Run(uint16_t overwritten_client_version, uint16_t max_server_version) {
    client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                             SSL_LIBRARY_VERSION_TLS_1_2);
    server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2, max_server_version);
    client_->SetPacketFilter(
        std::make_shared<TlsInspectorClientHelloVersionSetter>(
            overwritten_client_version));
    auto capture = std::make_shared<TlsInspectorRecordHandshakeMessage>(
        kTlsHandshakeServerHello);
    server_->SetPacketFilter(capture);
    ConnectExpectFail();
    client_->CheckErrorCode(SSL_ERROR_DECRYPT_ERROR_ALERT);
    server_->CheckErrorCode(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE);
    const DataBuffer& server_hello = capture->buffer();
    ASSERT_GT(server_hello.len(), 2U);
    uint32_t ver;
    ASSERT_TRUE(server_hello.Read(0, 2, &ver));
    ASSERT_EQ(static_cast<uint32_t>(SSL_LIBRARY_VERSION_TLS_1_2), ver);
  }
};

// If we offer a 1.3 ClientHello w/o supported_versions, the server should
// negotiate 1.2.
TEST_F(Tls13NoSupportedVersions,
       Tls13ClientHelloWithoutSupportedVersionsServer12) {
  Run(SSL_LIBRARY_VERSION_TLS_1_3, SSL_LIBRARY_VERSION_TLS_1_2);
}

TEST_F(Tls13NoSupportedVersions,
       Tls13ClientHelloWithoutSupportedVersionsServer13) {
  Run(SSL_LIBRARY_VERSION_TLS_1_3, SSL_LIBRARY_VERSION_TLS_1_3);
}

TEST_F(Tls13NoSupportedVersions,
       Tls14ClientHelloWithoutSupportedVersionsServer13) {
  Run(SSL_LIBRARY_VERSION_TLS_1_3 + 1, SSL_LIBRARY_VERSION_TLS_1_3);
}

// Offer 1.3 but with ClientHello.legacy_version == TLS 1.4. This
// causes a bad MAC error when we read EncryptedExtensions.
TEST_F(TlsConnectStreamTls13, Tls14ClientHelloWithSupportedVersions) {
  client_->SetPacketFilter(
      std::make_shared<TlsInspectorClientHelloVersionSetter>(
          SSL_LIBRARY_VERSION_TLS_1_3 + 1));
  auto capture = std::make_shared<TlsInspectorRecordHandshakeMessage>(
      kTlsHandshakeServerHello);
  server_->SetPacketFilter(capture);
  ConnectExpectFail();
  client_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
  server_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
  const DataBuffer& server_hello = capture->buffer();
  ASSERT_GT(server_hello.len(), 2U);
  uint32_t ver;
  ASSERT_TRUE(server_hello.Read(0, 2, &ver));
  // This way we don't need to change with new draft version.
  ASSERT_LT(static_cast<uint32_t>(SSL_LIBRARY_VERSION_TLS_1_2), ver);
}

}  // namespace nss_test
