/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

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

TEST_P(TlsConnectTls13, ZeroRtt) {
  SetupForZeroRtt();
  client_->SetExpectedAlertSentCount(1);
  server_->SetExpectedAlertReceivedCount(1);
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);
  ZeroRttSendReceive(true, true);
  Handshake();
  ExpectEarlyDataAccepted(true);
  CheckConnected();
  SendReceive();
}

TEST_P(TlsConnectTls13, ZeroRttServerRejectByOption) {
  SetupForZeroRtt();
  client_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);
  ZeroRttSendReceive(true, false);
  Handshake();
  CheckConnected();
  SendReceive();
}

// Test that we don't try to send 0-RTT data when the server sent
// us a ticket without the 0-RTT flags.
TEST_P(TlsConnectTls13, ZeroRttOptionsSetLate) {
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  Connect();
  SendReceive();  // Need to read so that we absorb the session ticket.
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign);
  Reset();
  server_->StartConnect();
  client_->StartConnect();
  // Now turn on 0-RTT but too late for the ticket.
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);
  ZeroRttSendReceive(false, false);
  Handshake();
  CheckConnected();
  SendReceive();
}

TEST_P(TlsConnectTls13, ZeroRttServerForgetTicket) {
  SetupForZeroRtt();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ClearServerCache();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  ExpectResumption(RESUME_NONE);
  ZeroRttSendReceive(true, false);
  Handshake();
  CheckConnected();
  SendReceive();
}

TEST_P(TlsConnectTls13, ZeroRttServerOnly) {
  ExpectResumption(RESUME_NONE);
  server_->Set0RttEnabled(true);
  client_->StartConnect();
  server_->StartConnect();

  // Client sends ordinary ClientHello.
  client_->Handshake();

  // Verify that the server doesn't get data.
  uint8_t buf[100];
  PRInt32 rv = PR_Read(server_->ssl_fd(), buf, sizeof(buf));
  EXPECT_EQ(SECFailure, rv);
  EXPECT_EQ(PR_WOULD_BLOCK_ERROR, PORT_GetError());

  // Now make sure that things complete.
  Handshake();
  CheckConnected();
  SendReceive();
  CheckKeys();
}

TEST_P(TlsConnectTls13, TestTls13ZeroRttAlpn) {
  EnableAlpn();
  SetupForZeroRtt();
  EnableAlpn();
  client_->SetExpectedAlertSentCount(1);
  server_->SetExpectedAlertReceivedCount(1);
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);
  ExpectEarlyDataAccepted(true);
  ZeroRttSendReceive(true, true, [this]() {
    client_->CheckAlpn(SSL_NEXT_PROTO_EARLY_VALUE, "a");
    return true;
  });
  Handshake();
  CheckConnected();
  SendReceive();
  CheckAlpn("a");
}

// Have the server negotiate a different ALPN value, and therefore
// reject 0-RTT.
TEST_P(TlsConnectTls13, TestTls13ZeroRttAlpnChangeServer) {
  EnableAlpn();
  SetupForZeroRtt();
  static const uint8_t client_alpn[] = {0x01, 0x61, 0x01, 0x62};  // "a", "b"
  static const uint8_t server_alpn[] = {0x01, 0x62};              // "b"
  client_->EnableAlpn(client_alpn, sizeof(client_alpn));
  server_->EnableAlpn(server_alpn, sizeof(server_alpn));
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);
  ZeroRttSendReceive(true, false, [this]() {
    client_->CheckAlpn(SSL_NEXT_PROTO_EARLY_VALUE, "a");
    return true;
  });
  Handshake();
  CheckConnected();
  SendReceive();
  CheckAlpn("b");
}

// Check that the client validates the ALPN selection of the server.
// Stomp the ALPN on the client after sending the ClientHello so
// that the server selection appears to be incorrect. The client
// should then fail the connection.
TEST_P(TlsConnectTls13, TestTls13ZeroRttNoAlpnServer) {
  EnableAlpn();
  SetupForZeroRtt();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  EnableAlpn();
  ExpectResumption(RESUME_TICKET);
  ZeroRttSendReceive(true, true, [this]() {
    PRUint8 b[] = {'b'};
    client_->CheckAlpn(SSL_NEXT_PROTO_EARLY_VALUE, "a");
    EXPECT_EQ(SECSuccess, SSLInt_Set0RttAlpn(client_->ssl_fd(), b, sizeof(b)));
    client_->CheckAlpn(SSL_NEXT_PROTO_EARLY_VALUE, "b");
    return true;
  });
  Handshake();
  client_->CheckErrorCode(SSL_ERROR_NEXT_PROTOCOL_DATA_INVALID);
  server_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

// Set up with no ALPN and then set the client so it thinks it has ALPN.
// The server responds without the extension and the client returns an
// error.
TEST_P(TlsConnectTls13, TestTls13ZeroRttNoAlpnClient) {
  SetupForZeroRtt();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);
  ZeroRttSendReceive(true, true, [this]() {
    PRUint8 b[] = {'b'};
    EXPECT_EQ(SECSuccess, SSLInt_Set0RttAlpn(client_->ssl_fd(), b, 1));
    client_->CheckAlpn(SSL_NEXT_PROTO_EARLY_VALUE, "b");
    return true;
  });
  Handshake();
  client_->CheckErrorCode(SSL_ERROR_NEXT_PROTOCOL_DATA_INVALID);
  server_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

// Remove the old ALPN value and so the client will not offer early data.
TEST_P(TlsConnectTls13, TestTls13ZeroRttAlpnChangeBoth) {
  EnableAlpn();
  SetupForZeroRtt();
  static const uint8_t alpn[] = {0x01, 0x62};  // "b"
  EnableAlpn(alpn, sizeof(alpn));
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);
  ZeroRttSendReceive(true, false, [this]() {
    client_->CheckAlpn(SSL_NEXT_PROTO_NO_SUPPORT);
    return false;
  });
  Handshake();
  CheckConnected();
  SendReceive();
  CheckAlpn("b");
}

// The client should abort the connection when sending a 0-rtt handshake but
// the servers responds with a TLS 1.2 ServerHello. (no app data sent)
TEST_P(TlsConnectTls13, TestTls13ZeroRttDowngrade) {
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  server_->Set0RttEnabled(true);  // set ticket_allow_early_data
  Connect();

  SendReceive();  // Need to read so that we absorb the session tickets.
  CheckKeys();

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  client_->StartConnect();
  server_->StartConnect();

  // We will send the early data xtn without sending actual early data. Thus
  // a 1.2 server shouldn't fail until the client sends an alert because the
  // client sends end_of_early_data only after reading the server's flight.
  client_->Set0RttEnabled(true);

  client_->Handshake();
  server_->Handshake();
  ASSERT_TRUE_WAIT(
      (client_->error_code() == SSL_ERROR_DOWNGRADE_WITH_EARLY_DATA), 2000);

  // DTLS will timeout as we bump the epoch when installing the early app data
  // cipher suite. Thus the encrypted alert will be ignored.
  if (mode_ == STREAM) {
    // The client sends an encrypted alert message.
    ASSERT_TRUE_WAIT(
        (server_->error_code() == SSL_ERROR_RX_UNEXPECTED_APPLICATION_DATA),
        2000);
  }
}

// The client should abort the connection when sending a 0-rtt handshake but
// the servers responds with a TLS 1.2 ServerHello. (with app data)
TEST_P(TlsConnectTls13, TestTls13ZeroRttDowngradeEarlyData) {
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  server_->Set0RttEnabled(true);  // set ticket_allow_early_data
  Connect();

  SendReceive();  // Need to read so that we absorb the session tickets.
  CheckKeys();

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  client_->StartConnect();
  server_->StartConnect();

  // Send the early data xtn in the CH, followed by early app data. The server
  // will fail right after sending its flight, when receiving the early data.
  client_->Set0RttEnabled(true);
  ZeroRttSendReceive(true, false);

  client_->Handshake();
  server_->Handshake();
  ASSERT_TRUE_WAIT(
      (client_->error_code() == SSL_ERROR_DOWNGRADE_WITH_EARLY_DATA), 2000);

  // DTLS will timeout as we bump the epoch when installing the early app data
  // cipher suite. Thus the encrypted alert will be ignored.
  if (mode_ == STREAM) {
    // The server sends an alert when receiving the early app data record.
    ASSERT_TRUE_WAIT(
        (server_->error_code() == SSL_ERROR_RX_UNEXPECTED_APPLICATION_DATA),
        2000);
  }
}

}  // namespace nss_test
