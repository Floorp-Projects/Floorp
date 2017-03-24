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
    ExpectAlert(client_, kTlsAlertIllegalParameter);
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
    ExpectAlert(client_, kTlsAlertIllegalParameter);
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

  client_->ExpectSendAlert(kTlsAlertIllegalParameter);
  if (mode_ == STREAM) {
    server_->ExpectSendAlert(kTlsAlertUnexpectedMessage);
  }
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
  ZeroRttSendReceive(true, false, [this]() {
    client_->ExpectSendAlert(kTlsAlertIllegalParameter);
    if (mode_ == STREAM) {
      server_->ExpectSendAlert(kTlsAlertUnexpectedMessage);
    }
    return true;
  });

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

static void CheckEarlyDataLimit(const std::shared_ptr<TlsAgent>& agent,
                                size_t expected_size) {
  SSLPreliminaryChannelInfo preinfo;
  SECStatus rv =
      SSL_GetPreliminaryChannelInfo(agent->ssl_fd(), &preinfo, sizeof(preinfo));
  EXPECT_EQ(SECSuccess, rv);
  EXPECT_EQ(expected_size, static_cast<size_t>(preinfo.maxEarlyDataSize));
}

TEST_P(TlsConnectTls13, SendTooMuchEarlyData) {
  const char* big_message = "0123456789abcdef";
  const size_t short_size = strlen(big_message) - 1;
  const PRInt32 short_length = static_cast<PRInt32>(short_size);
  SSLInt_SetMaxEarlyDataSize(static_cast<PRUint32>(short_size));
  SetupForZeroRtt();

  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);

  ExpectAlert(client_, kTlsAlertEndOfEarlyData);
  client_->Handshake();
  CheckEarlyDataLimit(client_, short_size);

  PRInt32 sent;
  // Writing more than the limit will succeed in TLS, but fail in DTLS.
  if (mode_ == STREAM) {
    sent = PR_Write(client_->ssl_fd(), big_message,
                    static_cast<PRInt32>(strlen(big_message)));
  } else {
    sent = PR_Write(client_->ssl_fd(), big_message,
                    static_cast<PRInt32>(strlen(big_message)));
    EXPECT_GE(0, sent);
    EXPECT_EQ(PR_WOULD_BLOCK_ERROR, PORT_GetError());

    // Try an exact-sized write now.
    sent = PR_Write(client_->ssl_fd(), big_message, short_length);
  }
  EXPECT_EQ(short_length, sent);

  // Even a single octet write should now fail.
  sent = PR_Write(client_->ssl_fd(), big_message, 1);
  EXPECT_GE(0, sent);
  EXPECT_EQ(PR_WOULD_BLOCK_ERROR, PORT_GetError());

  // Process the ClientHello and read 0-RTT.
  server_->Handshake();
  CheckEarlyDataLimit(server_, short_size);

  std::vector<uint8_t> buf(short_size + 1);
  PRInt32 read = PR_Read(server_->ssl_fd(), buf.data(), buf.capacity());
  EXPECT_EQ(short_length, read);
  EXPECT_EQ(0, memcmp(big_message, buf.data(), short_size));

  // Second read fails.
  read = PR_Read(server_->ssl_fd(), buf.data(), buf.capacity());
  EXPECT_EQ(SECFailure, read);
  EXPECT_EQ(PR_WOULD_BLOCK_ERROR, PORT_GetError());

  Handshake();
  ExpectEarlyDataAccepted(true);
  CheckConnected();
  SendReceive();
}

TEST_P(TlsConnectTls13, ReceiveTooMuchEarlyData) {
  const size_t limit = 5;
  SSLInt_SetMaxEarlyDataSize(limit);
  SetupForZeroRtt();

  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);

  client_->ExpectSendAlert(kTlsAlertEndOfEarlyData);
  client_->Handshake();  // Send ClientHello
  CheckEarlyDataLimit(client_, limit);

  // Lift the limit on the client.
  EXPECT_EQ(SECSuccess,
            SSLInt_SetSocketMaxEarlyDataSize(client_->ssl_fd(), 1000));

  // Send message
  const char* message = "0123456789abcdef";
  const PRInt32 message_len = static_cast<PRInt32>(strlen(message));
  EXPECT_EQ(message_len, PR_Write(client_->ssl_fd(), message, message_len));

  if (mode_ == STREAM) {
    // This error isn't fatal for DTLS.
    ExpectAlert(server_, kTlsAlertUnexpectedMessage);
  }
  server_->Handshake();  // Process ClientHello, send server flight.
  server_->Handshake();  // Just to make sure that we don't read ahead.
  CheckEarlyDataLimit(server_, limit);

  // Attempt to read early data.
  std::vector<uint8_t> buf(strlen(message) + 1);
  EXPECT_GT(0, PR_Read(server_->ssl_fd(), buf.data(), buf.capacity()));
  if (mode_ == STREAM) {
    server_->CheckErrorCode(SSL_ERROR_TOO_MUCH_EARLY_DATA);
  }

  client_->Handshake();  // Process the handshake.
  client_->Handshake();  // Process the alert.
  if (mode_ == STREAM) {
    client_->CheckErrorCode(SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT);
  }
}

}  // namespace nss_test
