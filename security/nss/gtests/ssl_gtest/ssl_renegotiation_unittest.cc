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

#include "gtest_utils.h"
#include "tls_connect.h"

namespace nss_test {

// 1.3 is disabled in the next few tests because we don't
// presently support resumption in 1.3.
TEST_P(TlsConnectStreamPre13, RenegotiateClient) {
  Connect();
  server_->PrepareForRenegotiate();
  client_->StartRenegotiate();
  Handshake();
  CheckConnected();
}

TEST_P(TlsConnectStreamPre13, RenegotiateServer) {
  Connect();
  client_->PrepareForRenegotiate();
  server_->StartRenegotiate();
  Handshake();
  CheckConnected();
}

// The renegotiation options shouldn't cause an error if TLS 1.3 is chosen.
TEST_F(TlsConnectTest, RenegotiationConfigTls13) {
  EnsureTlsSetup();
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetOption(SSL_ENABLE_RENEGOTIATION, SSL_RENEGOTIATE_UNRESTRICTED);
  server_->SetOption(SSL_REQUIRE_SAFE_NEGOTIATION, PR_TRUE);
  Connect();
  SendReceive();
  CheckKeys();
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

  if (test_version >= SSL_LIBRARY_VERSION_TLS_1_3) {
    ExpectAlert(server_, kTlsAlertUnexpectedMessage);
  } else {
    ExpectAlert(client_, kTlsAlertIllegalParameter);
  }

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
  if (test_version >= SSL_LIBRARY_VERSION_TLS_1_3) {
    ExpectAlert(server_, kTlsAlertUnexpectedMessage);
  } else {
    ExpectAlert(client_, kTlsAlertIllegalParameter);
  }
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

}  // namespace nss_test
