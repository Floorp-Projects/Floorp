/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secerr.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"

// This is internal, just to get TLS_1_3_DRAFT_VERSION.
#include "ssl3prot.h"

#include "gtest_utils.h"
#include "scoped_ptrs.h"
#include "tls_connect.h"
#include "tls_filter.h"
#include "tls_parser.h"

namespace nss_test {

TEST_P(TlsConnectTls13, HelloRetryRequestAbortsZeroRtt) {
  const char* k0RttData = "Such is life";
  const PRInt32 k0RttDataLen = static_cast<PRInt32>(strlen(k0RttData));

  SetupForZeroRtt();  // initial handshake as normal

  static const std::vector<SSLNamedGroup> groups = {ssl_grp_ec_secp384r1,
                                                    ssl_grp_ec_secp521r1};
  server_->ConfigNamedGroups(groups);
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);

  // Send first ClientHello and send 0-RTT data
  auto capture_early_data =
      std::make_shared<TlsExtensionCapture>(ssl_tls13_early_data_xtn);
  client_->SetPacketFilter(capture_early_data);
  client_->Handshake();
  EXPECT_EQ(k0RttDataLen, PR_Write(client_->ssl_fd(), k0RttData,
                                   k0RttDataLen));  // 0-RTT write.
  EXPECT_TRUE(capture_early_data->captured());

  // Send the HelloRetryRequest
  auto hrr_capture = std::make_shared<TlsInspectorRecordHandshakeMessage>(
      kTlsHandshakeHelloRetryRequest);
  server_->SetPacketFilter(hrr_capture);
  server_->Handshake();
  EXPECT_LT(0U, hrr_capture->buffer().len());

  // The server can't read
  std::vector<uint8_t> buf(k0RttDataLen);
  EXPECT_EQ(SECFailure, PR_Read(server_->ssl_fd(), buf.data(), k0RttDataLen));
  EXPECT_EQ(PR_WOULD_BLOCK_ERROR, PORT_GetError());

  // Make a new capture for the early data.
  capture_early_data =
      std::make_shared<TlsExtensionCapture>(ssl_tls13_early_data_xtn);
  client_->SetPacketFilter(capture_early_data);

  // Complete the handshake successfully
  Handshake();
  ExpectEarlyDataAccepted(false);  // The server should reject 0-RTT
  CheckConnected();
  SendReceive();
  EXPECT_FALSE(capture_early_data->captured());
}

// This filter only works for DTLS 1.3 where there is exactly one handshake
// packet. If the record is split into two packets, or there are multiple
// handshake packets, this will break.
class CorrectMessageSeqAfterHrrFilter : public TlsRecordFilter {
 protected:
  PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                    const DataBuffer& record, size_t* offset,
                                    DataBuffer* output) {
    if (filtered_packets() > 0 || header.content_type() != content_handshake) {
      return KEEP;
    }

    DataBuffer buffer(record);
    TlsRecordHeader new_header = {header.version(), header.content_type(),
                                  header.sequence_number() + 1};

    // Correct message_seq.
    buffer.Write(4, 1U, 2);

    *offset = new_header.Write(output, *offset, buffer);
    return CHANGE;
  }
};

TEST_P(TlsConnectTls13, SecondClientHelloRejectEarlyDataXtn) {
  static const std::vector<SSLNamedGroup> groups = {ssl_grp_ec_secp384r1,
                                                    ssl_grp_ec_secp521r1};

  SetupForZeroRtt();
  ExpectResumption(RESUME_TICKET);

  client_->ConfigNamedGroups(groups);
  server_->ConfigNamedGroups(groups);
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);

  // A new client that tries to resume with 0-RTT but doesn't send the
  // correct key share(s). The server will respond with an HRR.
  auto orig_client =
      std::make_shared<TlsAgent>(client_->name(), TlsAgent::CLIENT, mode_);
  client_.swap(orig_client);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  client_->ConfigureSessionCache(RESUME_BOTH);
  client_->Set0RttEnabled(true);
  client_->StartConnect();

  // Swap in the new client.
  client_->SetPeer(server_);
  server_->SetPeer(client_);

  // Send the ClientHello.
  client_->Handshake();
  // Process the CH, send an HRR.
  server_->Handshake();

  // Swap the client we created manually with the one that successfully
  // received a PSK, and try to resume with 0-RTT. The client doesn't know
  // about the HRR so it will send the early_data xtn as well as 0-RTT data.
  client_.swap(orig_client);
  orig_client.reset();

  // Correct the DTLS message sequence number after an HRR.
  if (mode_ == DGRAM) {
    client_->SetPacketFilter(
        std::make_shared<CorrectMessageSeqAfterHrrFilter>());
  }

  server_->SetPeer(client_);
  client_->Handshake();

  // Send 0-RTT data.
  const char* k0RttData = "ABCDEF";
  const PRInt32 k0RttDataLen = static_cast<PRInt32>(strlen(k0RttData));
  PRInt32 rv = PR_Write(client_->ssl_fd(), k0RttData, k0RttDataLen);
  EXPECT_EQ(k0RttDataLen, rv);

  ExpectAlert(server_, kTlsAlertUnsupportedExtension);
  Handshake();
  client_->CheckErrorCode(SSL_ERROR_UNSUPPORTED_EXTENSION_ALERT);
}

class KeyShareReplayer : public TlsExtensionFilter {
 public:
  KeyShareReplayer() {}

  virtual PacketFilter::Action FilterExtension(uint16_t extension_type,
                                               const DataBuffer& input,
                                               DataBuffer* output) {
    if (extension_type != ssl_tls13_key_share_xtn) {
      return KEEP;
    }

    if (!data_.len()) {
      data_ = input;
      return KEEP;
    }

    *output = data_;
    return CHANGE;
  }

 private:
  DataBuffer data_;
};

// This forces a HelloRetryRequest by disabling P-256 on the server.  However,
// the second ClientHello is modified so that it omits the requested share.  The
// server should reject this.
TEST_P(TlsConnectTls13, RetryWithSameKeyShare) {
  EnsureTlsSetup();
  client_->SetPacketFilter(std::make_shared<KeyShareReplayer>());
  static const std::vector<SSLNamedGroup> groups = {ssl_grp_ec_secp384r1,
                                                    ssl_grp_ec_secp521r1};
  server_->ConfigNamedGroups(groups);
  ConnectExpectAlert(server_, kTlsAlertIllegalParameter);
  EXPECT_EQ(SSL_ERROR_BAD_2ND_CLIENT_HELLO, server_->error_code());
  EXPECT_EQ(SSL_ERROR_ILLEGAL_PARAMETER_ALERT, client_->error_code());
}

// This tests that the second attempt at sending a ClientHello (after receiving
// a HelloRetryRequest) is correctly retransmitted.
TEST_F(TlsConnectDatagram13, DropClientSecondFlightWithHelloRetry) {
  static const std::vector<SSLNamedGroup> groups = {ssl_grp_ec_secp384r1,
                                                    ssl_grp_ec_secp521r1};
  server_->ConfigNamedGroups(groups);
  server_->SetPacketFilter(std::make_shared<SelectiveDropFilter>(0x2));
  Connect();
}

class TlsKeyExchange13 : public TlsKeyExchangeTest {};

// This should work, with an HRR, because the server prefers x25519 and the
// client generates a share for P-384 on the initial ClientHello.
TEST_P(TlsKeyExchange13, ConnectEcdhePreferenceMismatchHrr) {
  EnsureKeyShareSetup();
  static const std::vector<SSLNamedGroup> client_groups = {
      ssl_grp_ec_secp384r1, ssl_grp_ec_curve25519};
  static const std::vector<SSLNamedGroup> server_groups = {
      ssl_grp_ec_curve25519, ssl_grp_ec_secp384r1};
  client_->ConfigNamedGroups(client_groups);
  server_->ConfigNamedGroups(server_groups);
  Connect();
  CheckKeys();
  static const std::vector<SSLNamedGroup> expectedShares = {
      ssl_grp_ec_secp384r1};
  CheckKEXDetails(client_groups, expectedShares, ssl_grp_ec_curve25519);
}

// This should work, but not use HRR because the key share for x25519 was
// pre-generated by the client.
TEST_P(TlsKeyExchange13, ConnectEcdhePreferenceMismatchHrrExtraShares) {
  EnsureKeyShareSetup();
  static const std::vector<SSLNamedGroup> client_groups = {
      ssl_grp_ec_secp384r1, ssl_grp_ec_curve25519};
  static const std::vector<SSLNamedGroup> server_groups = {
      ssl_grp_ec_curve25519, ssl_grp_ec_secp384r1};
  client_->ConfigNamedGroups(client_groups);
  server_->ConfigNamedGroups(server_groups);
  EXPECT_EQ(SECSuccess, SSL_SendAdditionalKeyShares(client_->ssl_fd(), 1));

  Connect();
  CheckKeys();
  CheckKEXDetails(client_groups, client_groups);
}

TEST_F(TlsConnectTest, Select12AfterHelloRetryRequest) {
  EnsureTlsSetup();
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  static const std::vector<SSLNamedGroup> client_groups = {
      ssl_grp_ec_secp256r1, ssl_grp_ec_secp521r1};
  client_->ConfigNamedGroups(client_groups);
  static const std::vector<SSLNamedGroup> server_groups = {
      ssl_grp_ec_secp384r1, ssl_grp_ec_secp521r1};
  server_->ConfigNamedGroups(server_groups);
  client_->StartConnect();
  server_->StartConnect();

  client_->Handshake();
  server_->Handshake();

  // Here we replace the TLS server with one that does TLS 1.2 only.
  // This will happily send the client a TLS 1.2 ServerHello.
  server_.reset(new TlsAgent(server_->name(), TlsAgent::SERVER, mode_));
  client_->SetPeer(server_);
  server_->SetPeer(client_);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  server_->StartConnect();
  ExpectAlert(client_, kTlsAlertIllegalParameter);
  Handshake();
  EXPECT_EQ(SSL_ERROR_ILLEGAL_PARAMETER_ALERT, server_->error_code());
  EXPECT_EQ(SSL_ERROR_RX_MALFORMED_SERVER_HELLO, client_->error_code());
}

class HelloRetryRequestAgentTest : public TlsAgentTestClient {
 protected:
  void SetUp() override {
    TlsAgentTestClient::SetUp();
    EnsureInit();
    agent_->StartConnect();
  }

  void MakeCannedHrr(const uint8_t* body, size_t len, DataBuffer* hrr_record,
                     uint32_t seq_num = 0) const {
    DataBuffer hrr_data;
    hrr_data.Allocate(len + 4);
    size_t i = 0;
    i = hrr_data.Write(i, 0x7f00 | TLS_1_3_DRAFT_VERSION, 2);
    i = hrr_data.Write(i, static_cast<uint32_t>(len), 2);
    if (len) {
      hrr_data.Write(i, body, len);
    }
    DataBuffer hrr;
    MakeHandshakeMessage(kTlsHandshakeHelloRetryRequest, hrr_data.data(),
                         hrr_data.len(), &hrr, seq_num);
    MakeRecord(kTlsHandshakeType, SSL_LIBRARY_VERSION_TLS_1_3, hrr.data(),
               hrr.len(), hrr_record, seq_num);
  }

  void MakeGroupHrr(SSLNamedGroup group, DataBuffer* hrr_record,
                    uint32_t seq_num = 0) const {
    const uint8_t group_hrr[] = {
        static_cast<uint8_t>(ssl_tls13_key_share_xtn >> 8),
        static_cast<uint8_t>(ssl_tls13_key_share_xtn),
        0,
        2,  // length of key share extension
        static_cast<uint8_t>(group >> 8),
        static_cast<uint8_t>(group)};
    MakeCannedHrr(group_hrr, sizeof(group_hrr), hrr_record, seq_num);
  }
};

// Send two HelloRetryRequest messages in response to the ClientHello.  The are
// constructed to appear legitimate by asking for a new share in each, so that
// the client has to count to work out that the server is being unreasonable.
TEST_P(HelloRetryRequestAgentTest, SendSecondHelloRetryRequest) {
  DataBuffer hrr;
  MakeGroupHrr(ssl_grp_ec_secp384r1, &hrr, 0);
  ProcessMessage(hrr, TlsAgent::STATE_CONNECTING);
  MakeGroupHrr(ssl_grp_ec_secp521r1, &hrr, 1);
  ExpectAlert(kTlsAlertUnexpectedMessage);
  ProcessMessage(hrr, TlsAgent::STATE_ERROR,
                 SSL_ERROR_RX_UNEXPECTED_HELLO_RETRY_REQUEST);
}

// Here the client receives a HelloRetryRequest with a group that they already
// provided a share for.
TEST_P(HelloRetryRequestAgentTest, HandleBogusHelloRetryRequest) {
  DataBuffer hrr;
  MakeGroupHrr(ssl_grp_ec_curve25519, &hrr);
  ExpectAlert(kTlsAlertIllegalParameter);
  ProcessMessage(hrr, TlsAgent::STATE_ERROR,
                 SSL_ERROR_RX_MALFORMED_HELLO_RETRY_REQUEST);
}

TEST_P(HelloRetryRequestAgentTest, HandleNoopHelloRetryRequest) {
  DataBuffer hrr;
  MakeCannedHrr(nullptr, 0U, &hrr);
  ExpectAlert(kTlsAlertDecodeError);
  ProcessMessage(hrr, TlsAgent::STATE_ERROR,
                 SSL_ERROR_RX_MALFORMED_HELLO_RETRY_REQUEST);
}

TEST_P(HelloRetryRequestAgentTest, HandleHelloRetryRequestCookie) {
  const uint8_t canned_cookie_hrr[] = {
      static_cast<uint8_t>(ssl_tls13_cookie_xtn >> 8),
      static_cast<uint8_t>(ssl_tls13_cookie_xtn),
      0,
      5,  // length of cookie extension
      0,
      3,  // cookie value length
      0xc0,
      0x0c,
      0x13};
  DataBuffer hrr;
  MakeCannedHrr(canned_cookie_hrr, sizeof(canned_cookie_hrr), &hrr);
  auto capture = std::make_shared<TlsExtensionCapture>(ssl_tls13_cookie_xtn);
  agent_->SetPacketFilter(capture);
  ProcessMessage(hrr, TlsAgent::STATE_CONNECTING);
  const size_t cookie_pos = 2 + 2;  // cookie_xtn, extension len
  DataBuffer cookie(canned_cookie_hrr + cookie_pos,
                    sizeof(canned_cookie_hrr) - cookie_pos);
  EXPECT_EQ(cookie, capture->extension());
}

INSTANTIATE_TEST_CASE_P(HelloRetryRequestAgentTests, HelloRetryRequestAgentTest,
                        ::testing::Combine(TlsConnectTestBase::kTlsModesAll,
                                           TlsConnectTestBase::kTlsV13));
#ifndef NSS_DISABLE_TLS_1_3
INSTANTIATE_TEST_CASE_P(HelloRetryRequestKeyExchangeTests, TlsKeyExchange13,
                        ::testing::Combine(TlsConnectTestBase::kTlsModesAll,
                                           TlsConnectTestBase::kTlsV13));
#endif

}  // namespace nss_test
