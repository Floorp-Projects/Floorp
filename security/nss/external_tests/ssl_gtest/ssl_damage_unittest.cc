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
#include "scoped_ptrs.h"
#include "tls_connect.h"
#include "tls_filter.h"
#include "tls_parser.h"

namespace nss_test {

TEST_F(TlsConnectTest, DamageSecretHandleClientFinished) {
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->StartConnect();
  client_->StartConnect();
  client_->Handshake();
  server_->Handshake();
  std::cerr << "Damaging HS secret\n";
  SSLInt_DamageHsTrafficSecret(server_->ssl_fd());
  client_->Handshake();
  server_->Handshake();
  // The client thinks it has connected.
  EXPECT_EQ(TlsAgent::STATE_CONNECTED, client_->state());
  server_->CheckErrorCode(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE);
  client_->Handshake();
  client_->CheckErrorCode(SSL_ERROR_DECRYPT_ERROR_ALERT);
}

TEST_F(TlsConnectTest, DamageSecretHandleServerFinished) {
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetPacketFilter(new AfterRecordN(
      server_, client_,
      0,  // ServerHello.
      [this]() { SSLInt_DamageHsTrafficSecret(client_->ssl_fd()); }));
  ConnectExpectFail();
  client_->CheckErrorCode(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE);
  server_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
}

}  // namespace nspr_test
