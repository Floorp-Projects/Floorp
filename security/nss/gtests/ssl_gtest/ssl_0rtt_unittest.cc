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

#include "cpputil.h"
#include "gtest_utils.h"
#include "nss_scoped_ptrs.h"
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

TEST_P(TlsConnectTls13, ZeroRttApplicationReject) {
  SetupForZeroRtt();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);

  auto reject_0rtt = [](PRBool firstHello, const PRUint8* clientToken,
                        unsigned int clientTokenLen, PRUint8* appToken,
                        unsigned int* appTokenLen, unsigned int appTokenMax,
                        void* arg) {
    auto* called = reinterpret_cast<bool*>(arg);
    *called = true;

    EXPECT_TRUE(firstHello);
    EXPECT_EQ(0U, clientTokenLen);
    return ssl_hello_retry_reject_0rtt;
  };

  bool cb_run = false;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(server_->ssl_fd(),
                                                      reject_0rtt, &cb_run));
  ZeroRttSendReceive(true, false);
  Handshake();
  EXPECT_TRUE(cb_run);
  CheckConnected();
  SendReceive();
}

TEST_P(TlsConnectTls13, ZeroRttApparentReplayAfterRestart) {
  // The test fixtures enable anti-replay in SetUp().  This results in 0-RTT
  // being rejected until at least one window passes.  SetupFor0Rtt() forces a
  // rollover of the anti-replay filters, which clears that state and allows
  // 0-RTT to work.  Make the first connection manually to avoid that rollover
  // and cause 0-RTT to be rejected.

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
  void RunTest(bool rollover, const ScopedPK11SymKey& epsk) {
    // Now run a true 0-RTT handshake, but capture the first packet.
    auto first_packet = std::make_shared<SaveFirstPacket>();
    client_->SetFilter(first_packet);
    client_->Set0RttEnabled(true);
    server_->Set0RttEnabled(true);
    ZeroRttSendReceive(true, true);
    Handshake();
    EXPECT_LT(0U, first_packet->packet().len());
    ExpectEarlyDataAccepted(true);
    CheckConnected();
    SendReceive();

    if (rollover) {
      RolloverAntiReplay();
    }

    // Now replay that packet against the server.
    Reset();
    server_->StartConnect();
    server_->Set0RttEnabled(true);
    server_->SetAntiReplayContext(anti_replay_);
    if (epsk) {
      AddPsk(epsk, std::string("foo"), ssl_hash_sha256,
             TLS_CHACHA20_POLY1305_SHA256);
    }

    // Capture the early_data extension, which should not appear.
    auto early_data_ext =
        MakeTlsFilter<TlsExtensionCapture>(server_, ssl_tls13_early_data_xtn);

    // Finally, replay the ClientHello and force the server to consume it.  Stop
    // after the server sends its first flight; the client will not be able to
    // complete this handshake.
    server_->adapter()->PacketReceived(first_packet->packet());
    server_->Handshake();
    EXPECT_FALSE(early_data_ext->captured());
  }

  void RunResPskTest(bool rollover) {
    // Run the initial handshake
    SetupForZeroRtt();
    ExpectResumption(RESUME_TICKET);
    RunTest(rollover, ScopedPK11SymKey(nullptr));
  }

  void RunExtPskTest(bool rollover) {
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    ASSERT_NE(nullptr, slot);

    const std::vector<uint8_t> kPskDummyVal(16, 0xFF);
    SECItem psk_item = {siBuffer, toUcharPtr(kPskDummyVal.data()),
                        static_cast<unsigned int>(kPskDummyVal.size())};
    PK11SymKey* key =
        PK11_ImportSymKey(slot.get(), CKM_HKDF_KEY_GEN, PK11_OriginUnwrap,
                          CKA_DERIVE, &psk_item, NULL);
    ASSERT_NE(nullptr, key);
    ScopedPK11SymKey scoped_psk(key);
    RolloverAntiReplay();
    AddPsk(scoped_psk, std::string("foo"), ssl_hash_sha256,
           TLS_CHACHA20_POLY1305_SHA256);
    StartConnect();
    RunTest(rollover, scoped_psk);
  }
};

TEST_P(TlsZeroRttReplayTest, ResPskZeroRttReplay) { RunResPskTest(false); }

TEST_P(TlsZeroRttReplayTest, ExtPskZeroRttReplay) { RunExtPskTest(false); }

TEST_P(TlsZeroRttReplayTest, ZeroRttReplayAfterRollover) {
  RunResPskTest(true);
}

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

// Advancing time after sending the ClientHello means that the ticket age that
// arrives at the server is too low.  The server then rejects early data if this
// delay exceeds half the anti-replay window.
TEST_P(TlsConnectTls13, ZeroRttRejectOldTicket) {
  static const PRTime kWindow = 10 * PR_USEC_PER_SEC;
  ResetAntiReplay(kWindow);
  SetupForZeroRtt();

  Reset();
  StartConnect();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);
  ZeroRttSendReceive(true, false, [this]() {
    AdvanceTime(1 + kWindow / 2);
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
  static const PRTime kWindow = 10 * PR_USEC_PER_SEC;
  ResetAntiReplay(kWindow);
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  server_->Set0RttEnabled(true);
  StartConnect();
  client_->Handshake();  // ClientHello
  server_->Handshake();  // ServerHello
  AdvanceTime(1 + kWindow / 2);
  Handshake();  // Remainder of handshake
  CheckConnected();
  SendReceive();
  CheckKeys();

  Reset();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
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
  static const std::vector<uint8_t> alpn({0x01, 0x62});  // "b"
  EnableAlpn(alpn);
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

TEST_P(TlsConnectTls13, SendTooMuchEarlyData) {
  EnsureTlsSetup();
  const char* big_message = "0123456789abcdef";
  const size_t short_size = strlen(big_message) - 1;
  const PRInt32 short_length = static_cast<PRInt32>(short_size);
  EXPECT_EQ(SECSuccess,
            SSL_SetMaxEarlyDataSize(server_->ssl_fd(),
                                    static_cast<PRUint32>(short_size)));
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
  EnsureTlsSetup();

  const size_t limit = 5;
  EXPECT_EQ(SECSuccess, SSL_SetMaxEarlyDataSize(server_->ssl_fd(), limit));
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
  client_->SetFilter(coalesce);

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

// Early data remains available after the handshake completes for TLS.
TEST_F(TlsConnectStreamTls13, ZeroRttLateReadTls) {
  SetupForZeroRtt();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);
  client_->Handshake();  // ClientHello

  // Write some early data.
  const uint8_t data[] = {1, 2, 3, 4, 5, 6, 7, 8};
  PRInt32 rv = PR_Write(client_->ssl_fd(), data, sizeof(data));
  EXPECT_EQ(static_cast<PRInt32>(sizeof(data)), rv);

  // Consume the ClientHello and generate ServerHello..Finished.
  server_->Handshake();

  // Read some of the data.
  std::vector<uint8_t> small_buffer(1 + sizeof(data) / 2);
  rv = PR_Read(server_->ssl_fd(), small_buffer.data(), small_buffer.size());
  EXPECT_EQ(static_cast<PRInt32>(small_buffer.size()), rv);
  EXPECT_EQ(0, memcmp(data, small_buffer.data(), small_buffer.size()));

  Handshake();  // Complete the handshake.
  ExpectEarlyDataAccepted(true);
  CheckConnected();

  // After the handshake, it should be possible to read the remainder.
  uint8_t big_buf[100];
  rv = PR_Read(server_->ssl_fd(), big_buf, sizeof(big_buf));
  EXPECT_EQ(static_cast<PRInt32>(sizeof(data) - small_buffer.size()), rv);
  EXPECT_EQ(0, memcmp(&data[small_buffer.size()], big_buf,
                      sizeof(data) - small_buffer.size()));

  // And that's all there is to read.
  rv = PR_Read(server_->ssl_fd(), big_buf, sizeof(big_buf));
  EXPECT_GT(0, rv);
  EXPECT_EQ(PR_WOULD_BLOCK_ERROR, PORT_GetError());
}

// Early data that arrives before the handshake can be read after the handshake
// is complete.
TEST_F(TlsConnectDatagram13, ZeroRttLateReadDtls) {
  SetupForZeroRtt();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);
  client_->Handshake();  // ClientHello

  // Write some early data.
  const uint8_t data[] = {1, 2, 3};
  PRInt32 written = PR_Write(client_->ssl_fd(), data, sizeof(data));
  EXPECT_EQ(static_cast<PRInt32>(sizeof(data)), written);

  Handshake();  // Complete the handshake.
  ExpectEarlyDataAccepted(true);
  CheckConnected();

  // Reading at the server should return the early data, which was buffered.
  uint8_t buf[sizeof(data) + 1] = {0};
  PRInt32 read = PR_Read(server_->ssl_fd(), buf, sizeof(buf));
  EXPECT_EQ(static_cast<PRInt32>(sizeof(data)), read);
  EXPECT_EQ(0, memcmp(data, buf, sizeof(data)));
}

class PacketHolder : public PacketFilter {
 public:
  PacketHolder() = default;

  virtual Action Filter(const DataBuffer& input, DataBuffer* output) {
    packet_ = input;
    Disable();
    return DROP;
  }

  const DataBuffer& packet() const { return packet_; }

 private:
  DataBuffer packet_;
};

// Early data that arrives late is discarded for DTLS.
TEST_F(TlsConnectDatagram13, ZeroRttLateArrivalDtls) {
  SetupForZeroRtt();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);
  client_->Handshake();  // ClientHello

  // Write some early data.  Twice, so that we can read bits of it.
  const uint8_t data[] = {1, 2, 3};
  PRInt32 written = PR_Write(client_->ssl_fd(), data, sizeof(data));
  EXPECT_EQ(static_cast<PRInt32>(sizeof(data)), written);

  // Block and capture the next packet.
  auto holder = std::make_shared<PacketHolder>();
  client_->SetFilter(holder);
  written = PR_Write(client_->ssl_fd(), data, sizeof(data));
  EXPECT_EQ(static_cast<PRInt32>(sizeof(data)), written);
  EXPECT_FALSE(holder->enabled()) << "the filter should disable itself";

  // Consume the ClientHello and generate ServerHello..Finished.
  server_->Handshake();

  // Read some of the data.
  std::vector<uint8_t> small_buffer(sizeof(data));
  PRInt32 read =
      PR_Read(server_->ssl_fd(), small_buffer.data(), small_buffer.size());

  EXPECT_EQ(static_cast<PRInt32>(small_buffer.size()), read);
  EXPECT_EQ(0, memcmp(data, small_buffer.data(), small_buffer.size()));

  Handshake();  // Complete the handshake.
  ExpectEarlyDataAccepted(true);
  CheckConnected();

  server_->SendDirect(holder->packet());

  // Reading now should return nothing, even though a valid packet was
  // delivered.
  read = PR_Read(server_->ssl_fd(), small_buffer.data(), small_buffer.size());
  EXPECT_GT(0, read);
  EXPECT_EQ(PR_WOULD_BLOCK_ERROR, PORT_GetError());
}

// Early data reads in TLS should be coalesced.
TEST_F(TlsConnectStreamTls13, ZeroRttCoalesceReadTls) {
  SetupForZeroRtt();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);
  client_->Handshake();  // ClientHello

  // Write some early data.  In two writes.
  const uint8_t data[] = {1, 2, 3, 4, 5, 6};
  PRInt32 written = PR_Write(client_->ssl_fd(), data, 1);
  EXPECT_EQ(1, written);

  written = PR_Write(client_->ssl_fd(), data + 1, sizeof(data) - 1);
  EXPECT_EQ(static_cast<PRInt32>(sizeof(data) - 1), written);

  // Consume the ClientHello and generate ServerHello..Finished.
  server_->Handshake();

  // Read all of the data.
  std::vector<uint8_t> buffer(sizeof(data));
  PRInt32 read = PR_Read(server_->ssl_fd(), buffer.data(), buffer.size());
  EXPECT_EQ(static_cast<PRInt32>(sizeof(data)), read);
  EXPECT_EQ(0, memcmp(data, buffer.data(), sizeof(data)));

  Handshake();  // Complete the handshake.
  ExpectEarlyDataAccepted(true);
  CheckConnected();
}

// Early data reads in DTLS should not be coalesced.
TEST_F(TlsConnectDatagram13, ZeroRttNoCoalesceReadDtls) {
  SetupForZeroRtt();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);
  client_->Handshake();  // ClientHello

  // Write some early data.  In two writes.
  const uint8_t data[] = {1, 2, 3, 4, 5, 6};
  PRInt32 written = PR_Write(client_->ssl_fd(), data, 1);
  EXPECT_EQ(1, written);

  written = PR_Write(client_->ssl_fd(), data + 1, sizeof(data) - 1);
  EXPECT_EQ(static_cast<PRInt32>(sizeof(data) - 1), written);

  // Consume the ClientHello and generate ServerHello..Finished.
  server_->Handshake();

  // Try to read all of the data.
  std::vector<uint8_t> buffer(sizeof(data));
  PRInt32 read = PR_Read(server_->ssl_fd(), buffer.data(), buffer.size());
  EXPECT_EQ(1, read);
  EXPECT_EQ(0, memcmp(data, buffer.data(), 1));

  // Read the remainder.
  read = PR_Read(server_->ssl_fd(), buffer.data(), buffer.size());
  EXPECT_EQ(static_cast<PRInt32>(sizeof(data) - 1), read);
  EXPECT_EQ(0, memcmp(data + 1, buffer.data(), sizeof(data) - 1));

  Handshake();  // Complete the handshake.
  ExpectEarlyDataAccepted(true);
  CheckConnected();
}

// Early data reads in DTLS should fail if the buffer is too small.
TEST_F(TlsConnectDatagram13, ZeroRttShortReadDtls) {
  SetupForZeroRtt();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);
  client_->Handshake();  // ClientHello

  // Write some early data.  In two writes.
  const uint8_t data[] = {1, 2, 3, 4, 5, 6};
  PRInt32 written = PR_Write(client_->ssl_fd(), data, sizeof(data));
  EXPECT_EQ(static_cast<PRInt32>(sizeof(data)), written);

  // Consume the ClientHello and generate ServerHello..Finished.
  server_->Handshake();

  // Try to read all of the data into a small buffer.
  std::vector<uint8_t> buffer(sizeof(data));
  PRInt32 read = PR_Read(server_->ssl_fd(), buffer.data(), 1);
  EXPECT_GT(0, read);
  EXPECT_EQ(SSL_ERROR_RX_SHORT_DTLS_READ, PORT_GetError());

  // Read again with more space.
  read = PR_Read(server_->ssl_fd(), buffer.data(), buffer.size());
  EXPECT_EQ(static_cast<PRInt32>(sizeof(data)), read);
  EXPECT_EQ(0, memcmp(data, buffer.data(), sizeof(data)));

  Handshake();  // Complete the handshake.
  ExpectEarlyDataAccepted(true);
  CheckConnected();
}

// There are few ways in which TLS uses the clock and most of those operate on
// timescales that would be ridiculous to wait for in a test.  This is the one
// test we have that uses the real clock.  It tests that time passes by checking
// that a small sleep results in rejection of early data. 0-RTT has a
// configurable timer, which makes it ideal for this.
TEST_F(TlsConnectStreamTls13, TimePassesByDefault) {
  // Calling EnsureTlsSetup() replaces the time function on client and server,
  // and sets up anti-replay, which we don't want, so initialize each directly.
  client_->EnsureTlsSetup();
  server_->EnsureTlsSetup();
  // StartConnect() calls EnsureTlsSetup(), so avoid that too.
  client_->StartConnect();
  server_->StartConnect();

  // Set a tiny anti-replay window.  This has to be at least 2 milliseconds to
  // have any chance of being relevant as that is the smallest window that we
  // can detect.  Anything smaller rounds to zero.
  static const unsigned int kTinyWindowMs = 5;
  ResetAntiReplay(static_cast<PRTime>(kTinyWindowMs * PR_USEC_PER_MSEC));
  server_->SetAntiReplayContext(anti_replay_);

  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  server_->Set0RttEnabled(true);
  Handshake();
  CheckConnected();
  SendReceive();  // Absorb a session ticket.
  CheckKeys();

  // Clear the first window.
  PR_Sleep(PR_MillisecondsToInterval(kTinyWindowMs));

  Reset();
  client_->EnsureTlsSetup();
  server_->EnsureTlsSetup();
  client_->StartConnect();
  server_->StartConnect();

  // Early data is rejected by the server only if time passes for it as well.
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);
  ZeroRttSendReceive(true, false, []() {
    // Sleep long enough that we minimize the risk of our RTT estimation being
    // duped by stutters in test execution.  This is very long to allow for
    // flaky and low-end hardware, especially what our CI runs on.
    PR_Sleep(PR_MillisecondsToInterval(1000));
    return true;
  });
  Handshake();
  ExpectEarlyDataAccepted(false);
  CheckConnected();
}

// Test that SSL_CreateAntiReplayContext doesn't pass bad inputs.
TEST_F(TlsConnectStreamTls13, BadAntiReplayArgs) {
  SSLAntiReplayContext* p;
  // Zero or negative window.
  EXPECT_EQ(SECFailure, SSL_CreateAntiReplayContext(0, -1, 1, 1, &p));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
  EXPECT_EQ(SECFailure, SSL_CreateAntiReplayContext(0, 0, 1, 1, &p));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
  // Zero k.
  EXPECT_EQ(SECFailure, SSL_CreateAntiReplayContext(0, 1, 0, 1, &p));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
  // Zero bits.
  EXPECT_EQ(SECFailure, SSL_CreateAntiReplayContext(0, 1, 1, 0, &p));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
  EXPECT_EQ(SECFailure, SSL_CreateAntiReplayContext(0, 1, 1, 1, nullptr));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());

  // Prove that these parameters do work, even if they are useless..
  EXPECT_EQ(SECSuccess, SSL_CreateAntiReplayContext(0, 1, 1, 1, &p));
  ASSERT_NE(nullptr, p);
  ScopedSSLAntiReplayContext ctx(p);

  // The socket isn't a client or server until later, so configuring a client
  // should work OK.
  client_->EnsureTlsSetup();
  EXPECT_EQ(SECSuccess, SSL_SetAntiReplayContext(client_->ssl_fd(), ctx.get()));
  EXPECT_EQ(SECSuccess, SSL_SetAntiReplayContext(client_->ssl_fd(), nullptr));
}

// See also TlsConnectGenericResumption.ResumeServerIncompatibleCipher
TEST_P(TlsConnectTls13, ZeroRttDifferentCompatibleCipher) {
  EnsureTlsSetup();
  server_->EnableSingleCipher(TLS_AES_128_GCM_SHA256);
  SetupForZeroRtt();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  // Change the ciphersuite.  Resumption is OK because the hash is the same, but
  // early data will be rejected.
  server_->EnableSingleCipher(TLS_CHACHA20_POLY1305_SHA256);
  ExpectResumption(RESUME_TICKET);

  StartConnect();
  ZeroRttSendReceive(true, false);

  Handshake();
  ExpectEarlyDataAccepted(false);
  CheckConnected();
  SendReceive();
}

// See also TlsConnectGenericResumption.ResumeServerIncompatibleCipher
TEST_P(TlsConnectTls13, ZeroRttDifferentIncompatibleCipher) {
  EnsureTlsSetup();
  server_->EnableSingleCipher(TLS_AES_256_GCM_SHA384);
  SetupForZeroRtt();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  // Resumption is rejected because the hash is different.
  server_->EnableSingleCipher(TLS_CHACHA20_POLY1305_SHA256);
  ExpectResumption(RESUME_NONE);

  StartConnect();
  ZeroRttSendReceive(true, false);

  Handshake();
  ExpectEarlyDataAccepted(false);
  CheckConnected();
  SendReceive();
}

// The client failing to provide EndOfEarlyData results in failure.
// After 0-RTT working perfectly, things fall apart later.
// The server is unable to detect the change in keys, so it fails decryption.
// The client thinks everything has worked until it gets the alert.
TEST_F(TlsConnectStreamTls13, SuppressEndOfEarlyDataClientOnly) {
  SetupForZeroRtt();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  client_->SetOption(SSL_SUPPRESS_END_OF_EARLY_DATA, true);
  ExpectResumption(RESUME_TICKET);
  ZeroRttSendReceive(true, true);
  ExpectAlert(server_, kTlsAlertBadRecordMac);
  Handshake();
  EXPECT_EQ(TlsAgent::STATE_CONNECTED, client_->state());
  EXPECT_EQ(TlsAgent::STATE_ERROR, server_->state());
  server_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
  client_->Handshake();
  EXPECT_EQ(TlsAgent::STATE_ERROR, client_->state());
  client_->CheckErrorCode(SSL_ERROR_BAD_MAC_ALERT);
}

TEST_P(TlsConnectGeneric, SuppressEndOfEarlyDataNoZeroRtt) {
  EnsureTlsSetup();
  client_->SetOption(SSL_SUPPRESS_END_OF_EARLY_DATA, true);
  server_->SetOption(SSL_SUPPRESS_END_OF_EARLY_DATA, true);
  Connect();
  SendReceive();
}

#ifndef NSS_DISABLE_TLS_1_3
INSTANTIATE_TEST_CASE_P(Tls13ZeroRttReplayTest, TlsZeroRttReplayTest,
                        TlsConnectTestBase::kTlsVariantsAll);
#endif

}  // namespace nss_test
