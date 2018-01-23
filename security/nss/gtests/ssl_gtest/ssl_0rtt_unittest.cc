/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secerr.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslexp.h"
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

TEST_P(TlsConnectTls13, ZeroRttApparentReplayAfterRestart) {
  // The test fixtures call SSL_SetupAntiReplay() in SetUp().  This results in
  // 0-RTT being rejected until at least one window passes.  SetupFor0Rtt()
  // forces a rollover of the anti-replay filters, which clears this state.
  // Here, we do the setup manually here without that forced rollover.

  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  server_->Set0RttEnabled(true);  // So we signal that we allow 0-RTT.
  Connect();
  SendReceive();  // Need to read so that we absorb the session ticket.
  CheckKeys();

  Reset();
  StartConnect();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);
  ZeroRttSendReceive(true, false);
  Handshake();
  CheckConnected();
  SendReceive();
}

class TlsZeroRttReplayTest : public TlsConnectTls13 {
 private:
  class SaveFirstPacket : public PacketFilter {
   public:
    PacketFilter::Action Filter(const DataBuffer& input,
                                DataBuffer* output) override {
      if (!packet_.len() && input.len()) {
        packet_ = input;
      }
      return KEEP;
    }

    const DataBuffer& packet() const { return packet_; }

   private:
    DataBuffer packet_;
  };

 protected:
  void RunTest(bool rollover) {
    // Run the initial handshake
    SetupForZeroRtt();

    // Now run a true 0-RTT handshake, but capture the first packet.
    auto first_packet = std::make_shared<SaveFirstPacket>();
    client_->SetPacketFilter(first_packet);
    client_->Set0RttEnabled(true);
    server_->Set0RttEnabled(true);
    ExpectResumption(RESUME_TICKET);
    ZeroRttSendReceive(true, true);
    Handshake();
    EXPECT_LT(0U, first_packet->packet().len());
    ExpectEarlyDataAccepted(true);
    CheckConnected();
    SendReceive();

    if (rollover) {
      SSLInt_RolloverAntiReplay();
    }

    // Now replay that packet against the server.
    Reset();
    server_->StartConnect();
    server_->Set0RttEnabled(true);

    // Capture the early_data extension, which should not appear.
    auto early_data_ext =
        std::make_shared<TlsExtensionCapture>(ssl_tls13_early_data_xtn);
    server_->SetPacketFilter(early_data_ext);

    // Finally, replay the ClientHello and force the server to consume it.  Stop
    // after the server sends its first flight; the client will not be able to
    // complete this handshake.
    server_->adapter()->PacketReceived(first_packet->packet());
    server_->Handshake();
    EXPECT_FALSE(early_data_ext->captured());
  }
};

TEST_P(TlsZeroRttReplayTest, ZeroRttReplay) { RunTest(false); }

TEST_P(TlsZeroRttReplayTest, ZeroRttReplayAfterRollover) { RunTest(true); }

// Test that we don't try to send 0-RTT data when the server sent
// us a ticket without the 0-RTT flags.
TEST_P(TlsConnectTls13, ZeroRttOptionsSetLate) {
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  Connect();
  SendReceive();  // Need to read so that we absorb the session ticket.
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign);
  Reset();
  StartConnect();
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
  StartConnect();

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

// A small sleep after sending the ClientHello means that the ticket age that
// arrives at the server is too low.  With a small tolerance for variation in
// ticket age (which is determined by the |window| parameter that is passed to
// SSL_SetupAntiReplay()), the server then rejects early data.
TEST_P(TlsConnectTls13, ZeroRttRejectOldTicket) {
  SetupForZeroRtt();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  EXPECT_EQ(SECSuccess, SSL_SetupAntiReplay(1, 1, 3));
  SSLInt_RolloverAntiReplay();  // Make sure to flush replay state.
  SSLInt_RolloverAntiReplay();
  ExpectResumption(RESUME_TICKET);
  ZeroRttSendReceive(true, false, []() {
    PR_Sleep(PR_MillisecondsToInterval(10));
    return true;
  });
  Handshake();
  ExpectEarlyDataAccepted(false);
  CheckConnected();
  SendReceive();
}

// In this test, we falsely inflate the estimate of the RTT by delaying the
// ServerHello on the first handshake.  This results in the server estimating a
// higher value of the ticket age than the client ultimately provides.  Add a
// small tolerance for variation in ticket age and the ticket will appear to
// arrive prematurely, causing the server to reject early data.
TEST_P(TlsConnectTls13, ZeroRttRejectPrematureTicket) {
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  server_->Set0RttEnabled(true);
  StartConnect();
  client_->Handshake();  // ClientHello
  server_->Handshake();  // ServerHello
  PR_Sleep(PR_MillisecondsToInterval(10));
  Handshake();  // Remainder of handshake
  CheckConnected();
  SendReceive();
  CheckKeys();

  Reset();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  EXPECT_EQ(SECSuccess, SSL_SetupAntiReplay(1, 1, 3));
  SSLInt_RolloverAntiReplay();  // Make sure to flush replay state.
  SSLInt_RolloverAntiReplay();
  ExpectResumption(RESUME_TICKET);
  ExpectEarlyDataAccepted(false);
  StartConnect();
  ZeroRttSendReceive(true, false);
  Handshake();
  CheckConnected();
  SendReceive();
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

// NOTE: In this test and those below, the client always sends
// post-ServerHello alerts with the handshake keys, even if the server
// has accepted 0-RTT.  In some cases, as with errors in
// EncryptedExtensions, the client can't know the server's behavior,
// and in others it's just simpler.  What the server is expecting
// depends on whether it accepted 0-RTT or not. Eventually, we may
// make the server trial decrypt.
//
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
    client_->ExpectSendAlert(kTlsAlertIllegalParameter);
    return true;
  });
  if (variant_ == ssl_variant_stream) {
    server_->ExpectSendAlert(kTlsAlertBadRecordMac);
    Handshake();
    server_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
  } else {
    client_->Handshake();
  }
  client_->CheckErrorCode(SSL_ERROR_NEXT_PROTOCOL_DATA_INVALID);
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
    client_->ExpectSendAlert(kTlsAlertIllegalParameter);
    return true;
  });
  if (variant_ == ssl_variant_stream) {
    server_->ExpectSendAlert(kTlsAlertBadRecordMac);
    Handshake();
    server_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
  } else {
    client_->Handshake();
  }
  client_->CheckErrorCode(SSL_ERROR_NEXT_PROTOCOL_DATA_INVALID);
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
  StartConnect();
  // We will send the early data xtn without sending actual early data. Thus
  // a 1.2 server shouldn't fail until the client sends an alert because the
  // client sends end_of_early_data only after reading the server's flight.
  client_->Set0RttEnabled(true);

  client_->ExpectSendAlert(kTlsAlertIllegalParameter);
  if (variant_ == ssl_variant_stream) {
    server_->ExpectSendAlert(kTlsAlertUnexpectedMessage);
  }
  client_->Handshake();
  server_->Handshake();
  ASSERT_TRUE_WAIT(
      (client_->error_code() == SSL_ERROR_DOWNGRADE_WITH_EARLY_DATA), 2000);

  // DTLS will timeout as we bump the epoch when installing the early app data
  // cipher suite. Thus the encrypted alert will be ignored.
  if (variant_ == ssl_variant_stream) {
    // The client sends an encrypted alert message.
    ASSERT_TRUE_WAIT(
        (server_->error_code() == SSL_ERROR_RX_UNEXPECTED_APPLICATION_DATA),
        2000);
  }
}

// The client should abort the connection when sending a 0-rtt handshake but
// the servers responds with a TLS 1.2 ServerHello. (with app data)
TEST_P(TlsConnectTls13, TestTls13ZeroRttDowngradeEarlyData) {
  const char* k0RttData = "ABCDEF";
  const PRInt32 k0RttDataLen = static_cast<PRInt32>(strlen(k0RttData));

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
  StartConnect();
  // Send the early data xtn in the CH, followed by early app data. The server
  // will fail right after sending its flight, when receiving the early data.
  client_->Set0RttEnabled(true);
  client_->Handshake();  // Send ClientHello.
  PRInt32 rv =
      PR_Write(client_->ssl_fd(), k0RttData, k0RttDataLen);  // 0-RTT write.
  EXPECT_EQ(k0RttDataLen, rv);

  if (variant_ == ssl_variant_stream) {
    // When the server receives the early data, it will fail.
    server_->ExpectSendAlert(kTlsAlertUnexpectedMessage);
    server_->Handshake();  // Consume ClientHello
    EXPECT_EQ(TlsAgent::STATE_ERROR, server_->state());
    server_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_APPLICATION_DATA);
  } else {
    // If it's datagram, we just discard the early data.
    server_->Handshake();  // Consume ClientHello
    EXPECT_EQ(TlsAgent::STATE_CONNECTING, server_->state());
  }

  // The client now reads the ServerHello and fails.
  ASSERT_EQ(TlsAgent::STATE_CONNECTING, client_->state());
  client_->ExpectSendAlert(kTlsAlertIllegalParameter);
  client_->Handshake();
  client_->CheckErrorCode(SSL_ERROR_DOWNGRADE_WITH_EARLY_DATA);
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

  client_->Handshake();
  CheckEarlyDataLimit(client_, short_size);

  PRInt32 sent;
  // Writing more than the limit will succeed in TLS, but fail in DTLS.
  if (variant_ == ssl_variant_stream) {
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

  client_->Handshake();  // Send ClientHello
  CheckEarlyDataLimit(client_, limit);

  server_->Handshake();  // Process ClientHello, send server flight.

  // Lift the limit on the client.
  EXPECT_EQ(SECSuccess,
            SSLInt_SetSocketMaxEarlyDataSize(client_->ssl_fd(), 1000));

  // Send message
  const char* message = "0123456789abcdef";
  const PRInt32 message_len = static_cast<PRInt32>(strlen(message));
  EXPECT_EQ(message_len, PR_Write(client_->ssl_fd(), message, message_len));

  if (variant_ == ssl_variant_stream) {
    // This error isn't fatal for DTLS.
    ExpectAlert(server_, kTlsAlertUnexpectedMessage);
  }

  server_->Handshake();  // This reads the early data and maybe throws an error.
  if (variant_ == ssl_variant_stream) {
    server_->CheckErrorCode(SSL_ERROR_TOO_MUCH_EARLY_DATA);
  } else {
    EXPECT_EQ(TlsAgent::STATE_CONNECTING, server_->state());
  }
  CheckEarlyDataLimit(server_, limit);

  // Attempt to read early data. This will get an error.
  std::vector<uint8_t> buf(strlen(message) + 1);
  EXPECT_GT(0, PR_Read(server_->ssl_fd(), buf.data(), buf.capacity()));
  if (variant_ == ssl_variant_stream) {
    EXPECT_EQ(SSL_ERROR_HANDSHAKE_FAILED, PORT_GetError());
  } else {
    EXPECT_EQ(PR_WOULD_BLOCK_ERROR, PORT_GetError());
  }

  client_->Handshake();  // Process the server's first flight.
  if (variant_ == ssl_variant_stream) {
    client_->Handshake();  // Process the alert.
    client_->CheckErrorCode(SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT);
  } else {
    server_->Handshake();  // Finish connecting.
    EXPECT_EQ(TlsAgent::STATE_CONNECTED, server_->state());
  }
}

class PacketCoalesceFilter : public PacketFilter {
 public:
  PacketCoalesceFilter() : packet_data_() {}

  void SendCoalesced(std::shared_ptr<TlsAgent> agent) {
    agent->SendDirect(packet_data_);
  }

 protected:
  PacketFilter::Action Filter(const DataBuffer& input,
                              DataBuffer* output) override {
    packet_data_.Write(packet_data_.len(), input);
    return DROP;
  }

 private:
  DataBuffer packet_data_;
};

TEST_P(TlsConnectTls13, ZeroRttOrdering) {
  SetupForZeroRtt();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);

  // Send out the ClientHello.
  client_->Handshake();

  // Now, coalesce the next three things from the client: early data, second
  // flight and 1-RTT data.
  auto coalesce = std::make_shared<PacketCoalesceFilter>();
  client_->SetPacketFilter(coalesce);

  // Send (and hold) early data.
  static const std::vector<uint8_t> early_data = {3, 2, 1};
  EXPECT_EQ(static_cast<PRInt32>(early_data.size()),
            PR_Write(client_->ssl_fd(), early_data.data(), early_data.size()));

  // Send (and hold) the second client handshake flight.
  // The client sends EndOfEarlyData after seeing the server Finished.
  server_->Handshake();
  client_->Handshake();

  // Send (and hold) 1-RTT data.
  static const std::vector<uint8_t> late_data = {7, 8, 9, 10};
  EXPECT_EQ(static_cast<PRInt32>(late_data.size()),
            PR_Write(client_->ssl_fd(), late_data.data(), late_data.size()));

  // Now release them all at once.
  coalesce->SendCoalesced(client_);

  // Now ensure that the three steps are exposed in the right order on the
  // server: delivery of early data, handshake callback, delivery of 1-RTT.
  size_t step = 0;
  server_->SetHandshakeCallback([&step](TlsAgent*) {
    EXPECT_EQ(1U, step);
    ++step;
  });

  std::vector<uint8_t> buf(10);
  PRInt32 read = PR_Read(server_->ssl_fd(), buf.data(), buf.size());
  ASSERT_EQ(static_cast<PRInt32>(early_data.size()), read);
  buf.resize(read);
  EXPECT_EQ(early_data, buf);
  EXPECT_EQ(0U, step);
  ++step;

  // The third read should be after the handshake callback and should return the
  // data that was sent after the handshake completed.
  buf.resize(10);
  read = PR_Read(server_->ssl_fd(), buf.data(), buf.size());
  ASSERT_EQ(static_cast<PRInt32>(late_data.size()), read);
  buf.resize(read);
  EXPECT_EQ(late_data, buf);
  EXPECT_EQ(2U, step);
}

#ifndef NSS_DISABLE_TLS_1_3
INSTANTIATE_TEST_CASE_P(Tls13ZeroRttReplayTest, TlsZeroRttReplayTest,
                        TlsConnectTestBase::kTlsVariantsAll);
#endif

}  // namespace nss_test
