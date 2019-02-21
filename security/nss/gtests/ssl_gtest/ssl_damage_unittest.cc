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
#include "nss_scoped_ptrs.h"
#include "tls_connect.h"
#include "tls_filter.h"
#include "tls_parser.h"

namespace nss_test {

TEST_F(TlsConnectTest, DamageSecretHandleClientFinished) {
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  StartConnect();
  client_->Handshake();
  server_->Handshake();
  std::cerr << "Damaging HS secret" << std::endl;
  SSLInt_DamageClientHsTrafficSecret(server_->ssl_fd());
  client_->Handshake();
  // The client thinks it has connected.
  EXPECT_EQ(TlsAgent::STATE_CONNECTED, client_->state());

  ExpectAlert(server_, kTlsAlertDecryptError);
  server_->Handshake();
  server_->CheckErrorCode(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE);
  client_->Handshake();
  client_->CheckErrorCode(SSL_ERROR_DECRYPT_ERROR_ALERT);
}

TEST_F(TlsConnectTest, DamageSecretHandleServerFinished) {
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  MakeTlsFilter<AfterRecordN>(
      server_, client_,
      0,  // ServerHello.
      [this]() { SSLInt_DamageServerHsTrafficSecret(client_->ssl_fd()); });
  ConnectExpectAlert(client_, kTlsAlertDecryptError);
  client_->CheckErrorCode(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE);
}

TEST_P(TlsConnectGenericPre13, DamageServerSignature) {
  EnsureTlsSetup();
  auto filter = MakeTlsFilter<TlsLastByteDamager>(
      server_, kTlsHandshakeServerKeyExchange);
  ExpectAlert(client_, kTlsAlertDecryptError);
  ConnectExpectFail();
  client_->CheckErrorCode(SEC_ERROR_BAD_SIGNATURE);
  server_->CheckErrorCode(SSL_ERROR_DECRYPT_ERROR_ALERT);
}

TEST_P(TlsConnectTls13, DamageServerSignature) {
  EnsureTlsSetup();
  auto filter = MakeTlsFilter<TlsLastByteDamager>(
      server_, kTlsHandshakeCertificateVerify);
  filter->EnableDecryption();
  ConnectExpectAlert(client_, kTlsAlertDecryptError);
  client_->CheckErrorCode(SEC_ERROR_BAD_SIGNATURE);
}

TEST_P(TlsConnectGeneric, DamageClientSignature) {
  EnsureTlsSetup();
  client_->SetupClientAuth();
  server_->RequestClientAuth(true);
  auto filter = MakeTlsFilter<TlsLastByteDamager>(
      client_, kTlsHandshakeCertificateVerify);
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    filter->EnableDecryption();
  }
  server_->ExpectSendAlert(kTlsAlertDecryptError);
  // Do these handshakes by hand to avoid race condition on
  // the client processing the server's alert.
  StartConnect();
  client_->Handshake();
  server_->Handshake();
  client_->Handshake();
  server_->Handshake();
  EXPECT_EQ(version_ >= SSL_LIBRARY_VERSION_TLS_1_3
                ? TlsAgent::STATE_CONNECTED
                : TlsAgent::STATE_CONNECTING,
            client_->state());
  server_->CheckErrorCode(SEC_ERROR_BAD_SIGNATURE);
}

}  // namespace nss_test
