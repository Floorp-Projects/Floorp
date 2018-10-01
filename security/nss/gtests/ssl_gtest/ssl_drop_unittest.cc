/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secerr.h"
#include "ssl.h"
#include "sslexp.h"

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

TEST_P(TlsConnectDatagramPre13, DropClientFirstFlightOnce) {
  client_->SetFilter(std::make_shared<SelectiveDropFilter>(0x1));
  Connect();
  SendReceive();
}

TEST_P(TlsConnectDatagramPre13, DropServerFirstFlightOnce) {
  server_->SetFilter(std::make_shared<SelectiveDropFilter>(0x1));
  Connect();
  SendReceive();
}

// This drops the first transmission from both the client and server of all
// flights that they send.  Note: In DTLS 1.3, the shorter handshake means that
// this will also drop some application data, so we can't call SendReceive().
TEST_P(TlsConnectDatagramPre13, DropAllFirstTransmissions) {
  client_->SetFilter(std::make_shared<SelectiveDropFilter>(0x15));
  server_->SetFilter(std::make_shared<SelectiveDropFilter>(0x5));
  Connect();
}

// This drops the server's first flight three times.
TEST_P(TlsConnectDatagramPre13, DropServerFirstFlightThrice) {
  server_->SetFilter(std::make_shared<SelectiveDropFilter>(0x7));
  Connect();
}

// This drops the client's second flight once
TEST_P(TlsConnectDatagramPre13, DropClientSecondFlightOnce) {
  client_->SetFilter(std::make_shared<SelectiveDropFilter>(0x2));
  Connect();
}

// This drops the client's second flight three times.
TEST_P(TlsConnectDatagramPre13, DropClientSecondFlightThrice) {
  client_->SetFilter(std::make_shared<SelectiveDropFilter>(0xe));
  Connect();
}

// This drops the server's second flight three times.
TEST_P(TlsConnectDatagramPre13, DropServerSecondFlightThrice) {
  server_->SetFilter(std::make_shared<SelectiveDropFilter>(0xe));
  Connect();
}

class TlsDropDatagram13 : public TlsConnectDatagram13,
                          public ::testing::WithParamInterface<bool> {
 public:
  TlsDropDatagram13()
      : client_filters_(),
        server_filters_(),
        expected_client_acks_(0),
        expected_server_acks_(1) {}

  void SetUp() override {
    TlsConnectDatagram13::SetUp();
    ConfigureSessionCache(RESUME_NONE, RESUME_NONE);
    int short_header = GetParam() ? PR_TRUE : PR_FALSE;
    client_->SetOption(SSL_ENABLE_DTLS_SHORT_HEADER, short_header);
    server_->SetOption(SSL_ENABLE_DTLS_SHORT_HEADER, short_header);
    SetFilters();
  }

  void SetFilters() {
    EnsureTlsSetup();
    client_filters_.Init(client_);
    server_filters_.Init(server_);
  }

  void HandshakeAndAck(const std::shared_ptr<TlsAgent>& agent) {
    agent->Handshake();  // Read flight.
    ShiftDtlsTimers();
    agent->Handshake();  // Generate ACK.
  }

  void ShrinkPostServerHelloMtu() {
    // Abuse the custom extension mechanism to modify the MTU so that the
    // Certificate message is split into two pieces.
    ASSERT_EQ(
        SECSuccess,
        SSL_InstallExtensionHooks(
            server_->ssl_fd(), 1,
            [](PRFileDesc* fd, SSLHandshakeType message, PRUint8* data,
               unsigned int* len, unsigned int maxLen, void* arg) -> PRBool {
              SSLInt_SetMTU(fd, 500);  // Splits the certificate.
              return PR_FALSE;
            },
            nullptr,
            [](PRFileDesc* fd, SSLHandshakeType message, const PRUint8* data,
               unsigned int len, SSLAlertDescription* alert,
               void* arg) -> SECStatus { return SECSuccess; },
            nullptr));
  }

 protected:
  class DropAckChain {
   public:
    DropAckChain()
        : records_(nullptr), ack_(nullptr), drop_(nullptr), chain_(nullptr) {}

    void Init(const std::shared_ptr<TlsAgent>& agent) {
      records_ = std::make_shared<TlsRecordRecorder>(agent);
      ack_ = std::make_shared<TlsRecordRecorder>(agent, ssl_ct_ack);
      ack_->EnableDecryption();
      drop_ = std::make_shared<SelectiveRecordDropFilter>(agent, 0, false);
      chain_ = std::make_shared<ChainedPacketFilter>(
          ChainedPacketFilterInit({records_, ack_, drop_}));
      agent->SetFilter(chain_);
    }

    const TlsRecord& record(size_t i) const { return records_->record(i); }

    std::shared_ptr<TlsRecordRecorder> records_;
    std::shared_ptr<TlsRecordRecorder> ack_;
    std::shared_ptr<SelectiveRecordDropFilter> drop_;
    std::shared_ptr<PacketFilter> chain_;
  };

  void CheckAcks(const DropAckChain& chain, size_t index,
                 std::vector<uint64_t> acks) {
    const DataBuffer& buf = chain.ack_->record(index).buffer;
    size_t offset = 2;
    uint64_t len;

    EXPECT_EQ(2 + acks.size() * 8, buf.len());
    ASSERT_TRUE(buf.Read(0, 2, &len));
    ASSERT_EQ(static_cast<size_t>(len + 2), buf.len());
    if ((2 + acks.size() * 8) != buf.len()) {
      while (offset < buf.len()) {
        uint64_t ack;
        ASSERT_TRUE(buf.Read(offset, 8, &ack));
        offset += 8;
        std::cerr << "Ack=0x" << std::hex << ack << std::dec << std::endl;
      }
      return;
    }

    for (size_t i = 0; i < acks.size(); ++i) {
      uint64_t a = acks[i];
      uint64_t ack;
      ASSERT_TRUE(buf.Read(offset, 8, &ack));
      offset += 8;
      if (a != ack) {
        ADD_FAILURE() << "Wrong ack " << i << " expected=0x" << std::hex << a
                      << " got=0x" << ack << std::dec;
      }
    }
  }

  void CheckedHandshakeSendReceive() {
    Handshake();
    CheckPostHandshake();
  }

  void CheckPostHandshake() {
    CheckConnected();
    SendReceive();
    EXPECT_EQ(expected_client_acks_, client_filters_.ack_->count());
    EXPECT_EQ(expected_server_acks_, server_filters_.ack_->count());
  }

 protected:
  DropAckChain client_filters_;
  DropAckChain server_filters_;
  size_t expected_client_acks_;
  size_t expected_server_acks_;
};

// All of these tests produce a minimum one ACK, from the server
// to the client upon receiving the client Finished.
// Dropping complete first and second flights does not produce
// ACKs
TEST_P(TlsDropDatagram13, DropClientFirstFlightOnce) {
  client_filters_.drop_->Reset({0});
  StartConnect();
  client_->Handshake();
  server_->Handshake();
  CheckedHandshakeSendReceive();
  CheckAcks(server_filters_, 0, {0x0002000000000000ULL});
}

TEST_P(TlsDropDatagram13, DropServerFirstFlightOnce) {
  server_filters_.drop_->Reset(0xff);
  StartConnect();
  client_->Handshake();
  // Send the first flight, all dropped.
  server_->Handshake();
  server_filters_.drop_->Disable();
  CheckedHandshakeSendReceive();
  CheckAcks(server_filters_, 0, {0x0002000000000000ULL});
}

// Dropping the server's first record also does not produce
// an ACK because the next record is ignored.
// TODO(ekr@rtfm.com): We should generate an empty ACK.
TEST_P(TlsDropDatagram13, DropServerFirstRecordOnce) {
  server_filters_.drop_->Reset({0});
  StartConnect();
  client_->Handshake();
  server_->Handshake();
  Handshake();
  CheckedHandshakeSendReceive();
  CheckAcks(server_filters_, 0, {0x0002000000000000ULL});
}

// Dropping the second packet of the server's flight should
// produce an ACK.
TEST_P(TlsDropDatagram13, DropServerSecondRecordOnce) {
  server_filters_.drop_->Reset({1});
  StartConnect();
  client_->Handshake();
  server_->Handshake();
  HandshakeAndAck(client_);
  expected_client_acks_ = 1;
  CheckedHandshakeSendReceive();
  CheckAcks(client_filters_, 0, {0});  // ServerHello
  CheckAcks(server_filters_, 0, {0x0002000000000000ULL});
}

// Drop the server ACK and verify that the client retransmits
// the ClientHello.
TEST_P(TlsDropDatagram13, DropServerAckOnce) {
  StartConnect();
  client_->Handshake();
  server_->Handshake();
  // At this point the server has sent it's first flight,
  // so make it drop the ACK.
  server_filters_.drop_->Reset({0});
  client_->Handshake();  // Send the client Finished.
  server_->Handshake();  // Receive the Finished and send the ACK.
  EXPECT_EQ(TlsAgent::STATE_CONNECTED, client_->state());
  EXPECT_EQ(TlsAgent::STATE_CONNECTED, server_->state());
  // Wait for the DTLS timeout to make sure we retransmit the
  // Finished.
  ShiftDtlsTimers();
  client_->Handshake();  // Retransmit the Finished.
  server_->Handshake();  // Read the Finished and send an ACK.
  uint8_t buf[1];
  PRInt32 rv = PR_Read(client_->ssl_fd(), buf, sizeof(buf));
  expected_server_acks_ = 2;
  EXPECT_GT(0, rv);
  EXPECT_EQ(PR_WOULD_BLOCK_ERROR, PORT_GetError());
  CheckPostHandshake();
  // There should be two copies of the finished ACK
  CheckAcks(server_filters_, 0, {0x0002000000000000ULL});
  CheckAcks(server_filters_, 1, {0x0002000000000000ULL});
}

// Drop the client certificate verify.
TEST_P(TlsDropDatagram13, DropClientCertVerify) {
  StartConnect();
  client_->SetupClientAuth();
  server_->RequestClientAuth(true);
  client_->Handshake();
  server_->Handshake();
  // Have the client drop Cert Verify
  client_filters_.drop_->Reset({1});
  expected_server_acks_ = 2;
  CheckedHandshakeSendReceive();
  // Ack of the Cert.
  CheckAcks(server_filters_, 0, {0x0002000000000000ULL});
  // Ack of the whole client handshake.
  CheckAcks(
      server_filters_, 1,
      {0x0002000000000000ULL,    // CH (we drop everything after this on client)
       0x0002000000000003ULL,    // CT (2)
       0x0002000000000004ULL});  // FIN (2)
}

// Shrink the MTU down so that certs get split and drop the first piece.
TEST_P(TlsDropDatagram13, DropFirstHalfOfServerCertificate) {
  server_filters_.drop_->Reset({2});
  StartConnect();
  ShrinkPostServerHelloMtu();
  client_->Handshake();
  server_->Handshake();
  // Check that things got split.
  EXPECT_EQ(6UL,
            server_filters_.records_->count());  // SH, EE, CT1, CT2, CV, FIN
  size_t ct1_size = server_filters_.record(2).buffer.len();
  server_filters_.records_->Clear();
  expected_client_acks_ = 1;
  HandshakeAndAck(client_);
  server_->Handshake();                               // Retransmit
  EXPECT_EQ(3UL, server_filters_.records_->count());  // CT2, CV, FIN
  // Check that the first record is CT1 (which is identical to the same
  // as the previous CT1).
  EXPECT_EQ(ct1_size, server_filters_.record(0).buffer.len());
  CheckedHandshakeSendReceive();
  CheckAcks(client_filters_, 0,
            {0,                        // SH
             0x0002000000000000ULL,    // EE
             0x0002000000000002ULL});  // CT2
  CheckAcks(server_filters_, 0, {0x0002000000000000ULL});
}

// Shrink the MTU down so that certs get split and drop the second piece.
TEST_P(TlsDropDatagram13, DropSecondHalfOfServerCertificate) {
  server_filters_.drop_->Reset({3});
  StartConnect();
  ShrinkPostServerHelloMtu();
  client_->Handshake();
  server_->Handshake();
  // Check that things got split.
  EXPECT_EQ(6UL,
            server_filters_.records_->count());  // SH, EE, CT1, CT2, CV, FIN
  size_t ct1_size = server_filters_.record(3).buffer.len();
  server_filters_.records_->Clear();
  expected_client_acks_ = 1;
  HandshakeAndAck(client_);
  server_->Handshake();                               // Retransmit
  EXPECT_EQ(3UL, server_filters_.records_->count());  // CT1, CV, FIN
  // Check that the first record is CT1
  EXPECT_EQ(ct1_size, server_filters_.record(0).buffer.len());
  CheckedHandshakeSendReceive();
  CheckAcks(client_filters_, 0,
            {
                0,                      // SH
                0x0002000000000000ULL,  // EE
                0x0002000000000001ULL,  // CT1
            });
  CheckAcks(server_filters_, 0, {0x0002000000000000ULL});
}

// In this test, the Certificate message is sent four times, we drop all or part
// of the first three attempts:
// 1. Without fragmentation so that we can see how big it is - we drop that.
// 2. In two pieces - we drop half AND the resulting ACK.
// 3. In three pieces - we drop the middle piece.
//
// After that we let all the ACKs through and allow the handshake to complete
// without further interference.
//
// This allows us to test that ranges of handshake messages are sent correctly
// even when there are overlapping acknowledgments; that ACKs with duplicate or
// overlapping message ranges are handled properly; and that extra
// retransmissions are handled properly.
class TlsFragmentationAndRecoveryTest : public TlsDropDatagram13 {
 public:
  TlsFragmentationAndRecoveryTest() : cert_len_(0) {}

 protected:
  void RunTest(size_t dropped_half) {
    FirstFlightDropCertificate();

    SecondAttemptDropHalf(dropped_half);
    size_t dropped_half_size = server_record_len(dropped_half);
    size_t second_flight_count = server_filters_.records_->count();

    ThirdAttemptDropMiddle();
    size_t repaired_third_size = server_record_len((dropped_half == 0) ? 0 : 2);
    size_t third_flight_count = server_filters_.records_->count();

    AckAndCompleteRetransmission();
    size_t final_server_flight_count = server_filters_.records_->count();
    EXPECT_LE(3U, final_server_flight_count);  // CT(sixth), CV, Fin
    CheckSizeOfSixth(dropped_half_size, repaired_third_size);

    SendDelayedAck();
    // Same number of messages as the last flight.
    EXPECT_EQ(final_server_flight_count, server_filters_.records_->count());
    // Double check that the Certificate size is still correct.
    CheckSizeOfSixth(dropped_half_size, repaired_third_size);

    CompleteHandshake(final_server_flight_count);

    // This is the ACK for the first attempt to send a whole certificate.
    std::vector<uint64_t> client_acks = {
        0,                     // SH
        0x0002000000000000ULL  // EE
    };
    CheckAcks(client_filters_, 0, client_acks);
    // And from the second attempt for the half was kept (we delayed this ACK).
    client_acks.push_back(0x0002000000000000ULL + second_flight_count +
                          ~dropped_half % 2);
    CheckAcks(client_filters_, 1, client_acks);
    // And the third attempt where the first and last thirds got through.
    client_acks.push_back(0x0002000000000000ULL + second_flight_count +
                          third_flight_count - 1);
    client_acks.push_back(0x0002000000000000ULL + second_flight_count +
                          third_flight_count + 1);
    CheckAcks(client_filters_, 2, client_acks);
    CheckAcks(server_filters_, 0, {0x0002000000000000ULL});
  }

 private:
  void FirstFlightDropCertificate() {
    StartConnect();
    client_->Handshake();

    // Note: 1 << N is the Nth packet, starting from zero.
    server_filters_.drop_->Reset(1 << 2);  // Drop Cert0.
    server_->Handshake();
    EXPECT_EQ(5U, server_filters_.records_->count());  // SH, EE, CT, CV, Fin
    cert_len_ = server_filters_.records_->record(2).buffer.len();

    HandshakeAndAck(client_);
    EXPECT_EQ(2U, client_filters_.records_->count());
  }

  // Lower the MTU so that the server has to split the certificate in two
  // pieces.  The server resends Certificate (in two), plus CV and Fin.
  void SecondAttemptDropHalf(size_t dropped_half) {
    ASSERT_LE(0U, dropped_half);
    ASSERT_GT(2U, dropped_half);
    server_filters_.records_->Clear();
    server_filters_.drop_->Reset({dropped_half});  // Drop Cert1[half]
    SplitServerMtu(2);
    server_->Handshake();
    EXPECT_LE(4U, server_filters_.records_->count());  // CT x2, CV, Fin

    // Generate and capture the ACK from the client.
    client_filters_.drop_->Reset({0});
    HandshakeAndAck(client_);
    EXPECT_EQ(3U, client_filters_.records_->count());
  }

  // Lower the MTU again so that the server sends Certificate cut into three
  // pieces.  Drop the middle piece.
  void ThirdAttemptDropMiddle() {
    server_filters_.records_->Clear();
    server_filters_.drop_->Reset({1});  // Drop Cert2[1] (of 3)
    SplitServerMtu(3);
    // Because we dropped the client ACK, the server retransmits on a timer.
    ShiftDtlsTimers();
    server_->Handshake();
    EXPECT_LE(5U, server_filters_.records_->count());  // CT x3, CV, Fin
  }

  void AckAndCompleteRetransmission() {
    // Generate ACKs.
    HandshakeAndAck(client_);
    // The server should send the final sixth of the certificate: the client has
    // acknowledged the first half and the last third.  Also send CV and Fin.
    server_filters_.records_->Clear();
    server_->Handshake();
  }

  void CheckSizeOfSixth(size_t size_of_half, size_t size_of_third) {
    // Work out if the final sixth is the right size.  We get the records with
    // overheads added, which obscures the length of the payload.  We want to
    // ensure that the server only sent the missing sixth of the Certificate.
    //
    // We captured |size_of_half + overhead| and |size_of_third + overhead| and
    // want to calculate |size_of_third - size_of_third + overhead|.  We can't
    // calculate |overhead|, but it is is (currently) always a handshake message
    // header, a content type, and an authentication tag:
    static const size_t record_overhead = 12 + 1 + 16;
    EXPECT_EQ(size_of_half - size_of_third + record_overhead,
              server_filters_.records_->record(0).buffer.len());
  }

  void SendDelayedAck() {
    // Send the ACK we held back.  The reordered ACK doesn't add new
    // information,
    // but triggers an extra retransmission of the missing records again (even
    // though the client has all that it needs).
    client_->SendRecordDirect(client_filters_.records_->record(2));
    server_filters_.records_->Clear();
    server_->Handshake();
  }

  void CompleteHandshake(size_t extra_retransmissions) {
    // All this messing around shouldn't cause a failure...
    Handshake();
    // ...but it leaves a mess.  Add an extra few calls to Handshake() for the
    // client so that it absorbs the extra retransmissions.
    for (size_t i = 0; i < extra_retransmissions; ++i) {
      client_->Handshake();
    }
    CheckConnected();
  }

  // Split the server MTU so that the Certificate is split into |count| pieces.
  // The calculation doesn't need to be perfect as long as the Certificate
  // message is split into the right number of pieces.
  void SplitServerMtu(size_t count) {
    // Set the MTU based on the formula:
    // bare_size = cert_len_ - actual_overhead
    // MTU = ceil(bare_size / count) + pessimistic_overhead
    //
    // actual_overhead is the amount of actual overhead on the record we
    // captured, which is (note that our length doesn't include the header):
    static const size_t actual_overhead = 12 +  // handshake message header
                                          1 +   // content type
                                          16;   // authentication tag
    size_t bare_size = cert_len_ - actual_overhead;

    // pessimistic_overhead is the amount of expansion that NSS assumes will be
    // added to each handshake record.  Right now, that is DTLS_MIN_FRAGMENT:
    static const size_t pessimistic_overhead =
        12 +  // handshake message header
        1 +   // content type
        13 +  // record header length
        64;   // maximum record expansion: IV, MAC and block cipher expansion

    size_t mtu = (bare_size + count - 1) / count + pessimistic_overhead;
    if (g_ssl_gtest_verbose) {
      std::cerr << "server: set MTU to " << mtu << std::endl;
    }
    EXPECT_EQ(SECSuccess, SSLInt_SetMTU(server_->ssl_fd(), mtu));
  }

  size_t server_record_len(size_t index) const {
    return server_filters_.records_->record(index).buffer.len();
  }

  size_t cert_len_;
};

TEST_P(TlsFragmentationAndRecoveryTest, DropFirstHalf) { RunTest(0); }

TEST_P(TlsFragmentationAndRecoveryTest, DropSecondHalf) { RunTest(1); }

TEST_P(TlsDropDatagram13, NoDropsDuringZeroRtt) {
  SetupForZeroRtt();
  SetFilters();
  std::cerr << "Starting second handshake" << std::endl;
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);
  ZeroRttSendReceive(true, true);
  Handshake();
  ExpectEarlyDataAccepted(true);
  CheckConnected();
  SendReceive();
  EXPECT_EQ(0U, client_filters_.ack_->count());
  CheckAcks(server_filters_, 0,
            {0x0001000000000001ULL,    // EOED
             0x0002000000000000ULL});  // Finished
}

TEST_P(TlsDropDatagram13, DropEEDuringZeroRtt) {
  SetupForZeroRtt();
  SetFilters();
  std::cerr << "Starting second handshake" << std::endl;
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);
  server_filters_.drop_->Reset({1});
  ZeroRttSendReceive(true, true);
  HandshakeAndAck(client_);
  Handshake();
  ExpectEarlyDataAccepted(true);
  CheckConnected();
  SendReceive();
  CheckAcks(client_filters_, 0, {0});
  CheckAcks(server_filters_, 0,
            {0x0001000000000002ULL,    // EOED
             0x0002000000000000ULL});  // Finished
}

class TlsReorderDatagram13 : public TlsDropDatagram13 {
 public:
  TlsReorderDatagram13() {}

  // Send records from the records buffer in the given order.
  void ReSend(TlsAgent::Role side, std::vector<size_t> indices) {
    std::shared_ptr<TlsAgent> agent;
    std::shared_ptr<TlsRecordRecorder> records;

    if (side == TlsAgent::CLIENT) {
      agent = client_;
      records = client_filters_.records_;
    } else {
      agent = server_;
      records = server_filters_.records_;
    }

    for (auto i : indices) {
      agent->SendRecordDirect(records->record(i));
    }
  }
};

// Reorder the server records so that EE comes at the end
// of the flight and will still produce an ACK.
TEST_P(TlsDropDatagram13, ReorderServerEE) {
  server_filters_.drop_->Reset({1});
  StartConnect();
  client_->Handshake();
  server_->Handshake();
  // We dropped EE, now reinject.
  server_->SendRecordDirect(server_filters_.record(1));
  expected_client_acks_ = 1;
  HandshakeAndAck(client_);
  CheckedHandshakeSendReceive();
  CheckAcks(client_filters_, 0,
            {
                0,                   // SH
                0x0002000000000000,  // EE
            });
  CheckAcks(server_filters_, 0, {0x0002000000000000ULL});
}

// The client sends an out of order non-handshake message
// but with the handshake key.
class TlsSendCipherSpecCapturer {
 public:
  TlsSendCipherSpecCapturer(std::shared_ptr<TlsAgent>& agent)
      : send_cipher_specs_() {
    SSLInt_SetCipherSpecChangeFunc(agent->ssl_fd(), CipherSpecChanged,
                                   (void*)this);
  }

  std::shared_ptr<TlsCipherSpec> spec(size_t i) {
    if (i >= send_cipher_specs_.size()) {
      return nullptr;
    }
    return send_cipher_specs_[i];
  }

 private:
  static void CipherSpecChanged(void* arg, PRBool sending,
                                ssl3CipherSpec* newSpec) {
    if (!sending) {
      return;
    }

    auto self = static_cast<TlsSendCipherSpecCapturer*>(arg);

    auto spec = std::make_shared<TlsCipherSpec>();
    bool ret = spec->Init(SSLInt_CipherSpecToEpoch(newSpec),
                          SSLInt_CipherSpecToAlgorithm(newSpec),
                          SSLInt_CipherSpecToKey(newSpec),
                          SSLInt_CipherSpecToIv(newSpec));
    EXPECT_EQ(true, ret);
    self->send_cipher_specs_.push_back(spec);
  }

  std::vector<std::shared_ptr<TlsCipherSpec>> send_cipher_specs_;
};

TEST_P(TlsDropDatagram13, SendOutOfOrderAppWithHandshakeKey) {
  StartConnect();
  TlsSendCipherSpecCapturer capturer(client_);
  client_->Handshake();
  server_->Handshake();
  client_->Handshake();
  EXPECT_EQ(TlsAgent::STATE_CONNECTED, client_->state());
  server_->Handshake();
  EXPECT_EQ(TlsAgent::STATE_CONNECTED, server_->state());
  // After the client sends Finished, inject an app data record
  // with the handshake key. This should produce an alert.
  uint8_t buf[] = {'a', 'b', 'c'};
  auto spec = capturer.spec(0);
  ASSERT_NE(nullptr, spec.get());
  ASSERT_EQ(2, spec->epoch());
  ASSERT_TRUE(client_->SendEncryptedRecord(spec, 0x0002000000000002,
                                           ssl_ct_application_data,
                                           DataBuffer(buf, sizeof(buf))));

  // Now have the server consume the bogus message.
  server_->ExpectSendAlert(illegal_parameter, kTlsAlertFatal);
  server_->Handshake();
  EXPECT_EQ(TlsAgent::STATE_ERROR, server_->state());
  EXPECT_EQ(SSL_ERROR_RX_UNKNOWN_RECORD_TYPE, PORT_GetError());
}

TEST_P(TlsDropDatagram13, SendOutOfOrderHsNonsenseWithHandshakeKey) {
  StartConnect();
  TlsSendCipherSpecCapturer capturer(client_);
  client_->Handshake();
  server_->Handshake();
  client_->Handshake();
  EXPECT_EQ(TlsAgent::STATE_CONNECTED, client_->state());
  server_->Handshake();
  EXPECT_EQ(TlsAgent::STATE_CONNECTED, server_->state());
  // Inject a new bogus handshake record, which the server responds
  // to by just ACKing the original one (we ignore the contents).
  uint8_t buf[] = {'a', 'b', 'c'};
  auto spec = capturer.spec(0);
  ASSERT_NE(nullptr, spec.get());
  ASSERT_EQ(2, spec->epoch());
  ASSERT_TRUE(client_->SendEncryptedRecord(spec, 0x0002000000000002,
                                           ssl_ct_handshake,
                                           DataBuffer(buf, sizeof(buf))));
  server_->Handshake();
  EXPECT_EQ(2UL, server_filters_.ack_->count());
  // The server acknowledges client Finished twice.
  CheckAcks(server_filters_, 0, {0x0002000000000000ULL});
  CheckAcks(server_filters_, 1, {0x0002000000000000ULL});
}

// Shrink the MTU down so that certs get split and then swap the first and
// second pieces of the server certificate.
TEST_P(TlsReorderDatagram13, ReorderServerCertificate) {
  StartConnect();
  ShrinkPostServerHelloMtu();
  client_->Handshake();
  // Drop the entire handshake flight so we can reorder.
  server_filters_.drop_->Reset(0xff);
  server_->Handshake();
  // Check that things got split.
  EXPECT_EQ(6UL,
            server_filters_.records_->count());  // CH, EE, CT1, CT2, CV, FIN
  // Now re-send things in a different order.
  ReSend(TlsAgent::SERVER, std::vector<size_t>{0, 1, 3, 2, 4, 5});
  // Clear.
  server_filters_.drop_->Disable();
  server_filters_.records_->Clear();
  // Wait for client to send ACK.
  ShiftDtlsTimers();
  CheckedHandshakeSendReceive();
  EXPECT_EQ(2UL, server_filters_.records_->count());  // ACK + Data
  CheckAcks(server_filters_, 0, {0x0002000000000000ULL});
}

TEST_P(TlsReorderDatagram13, DataAfterEOEDDuringZeroRtt) {
  SetupForZeroRtt();
  SetFilters();
  std::cerr << "Starting second handshake" << std::endl;
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);
  // Send the client's first flight of zero RTT data.
  ZeroRttSendReceive(true, true);
  // Now send another client application data record but
  // capture it.
  client_filters_.records_->Clear();
  client_filters_.drop_->Reset(0xff);
  const char* k0RttData = "123456";
  const PRInt32 k0RttDataLen = static_cast<PRInt32>(strlen(k0RttData));
  PRInt32 rv =
      PR_Write(client_->ssl_fd(), k0RttData, k0RttDataLen);  // 0-RTT write.
  EXPECT_EQ(k0RttDataLen, rv);
  EXPECT_EQ(1UL, client_filters_.records_->count());  // data
  server_->Handshake();
  client_->Handshake();
  ExpectEarlyDataAccepted(true);
  // The server still hasn't received anything at this point.
  EXPECT_EQ(3UL, client_filters_.records_->count());  // data, EOED, FIN
  EXPECT_EQ(TlsAgent::STATE_CONNECTED, client_->state());
  EXPECT_EQ(TlsAgent::STATE_CONNECTING, server_->state());
  // Now re-send the client's messages: EOED, data, FIN
  ReSend(TlsAgent::CLIENT, std::vector<size_t>({1, 0, 2}));
  server_->Handshake();
  CheckConnected();
  EXPECT_EQ(0U, client_filters_.ack_->count());
  // Acknowledgements for EOED and Finished.
  CheckAcks(server_filters_, 0, {0x0001000000000002ULL, 0x0002000000000000ULL});
  uint8_t buf[8];
  rv = PR_Read(server_->ssl_fd(), buf, sizeof(buf));
  EXPECT_EQ(-1, rv);
  EXPECT_EQ(PR_WOULD_BLOCK_ERROR, PORT_GetError());
}

TEST_P(TlsReorderDatagram13, DataAfterFinDuringZeroRtt) {
  SetupForZeroRtt();
  SetFilters();
  std::cerr << "Starting second handshake" << std::endl;
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);
  // Send the client's first flight of zero RTT data.
  ZeroRttSendReceive(true, true);
  // Now send another client application data record but
  // capture it.
  client_filters_.records_->Clear();
  client_filters_.drop_->Reset(0xff);
  const char* k0RttData = "123456";
  const PRInt32 k0RttDataLen = static_cast<PRInt32>(strlen(k0RttData));
  PRInt32 rv =
      PR_Write(client_->ssl_fd(), k0RttData, k0RttDataLen);  // 0-RTT write.
  EXPECT_EQ(k0RttDataLen, rv);
  EXPECT_EQ(1UL, client_filters_.records_->count());  // data
  server_->Handshake();
  client_->Handshake();
  ExpectEarlyDataAccepted(true);
  // The server still hasn't received anything at this point.
  EXPECT_EQ(3UL, client_filters_.records_->count());  // EOED, FIN, Data
  EXPECT_EQ(TlsAgent::STATE_CONNECTED, client_->state());
  EXPECT_EQ(TlsAgent::STATE_CONNECTING, server_->state());
  // Now re-send the client's messages: EOED, FIN, Data
  ReSend(TlsAgent::CLIENT, std::vector<size_t>({1, 2, 0}));
  server_->Handshake();
  CheckConnected();
  EXPECT_EQ(0U, client_filters_.ack_->count());
  // Acknowledgements for EOED and Finished.
  CheckAcks(server_filters_, 0, {0x0001000000000002ULL, 0x0002000000000000ULL});
  uint8_t buf[8];
  rv = PR_Read(server_->ssl_fd(), buf, sizeof(buf));
  EXPECT_EQ(-1, rv);
  EXPECT_EQ(PR_WOULD_BLOCK_ERROR, PORT_GetError());
}

static void GetCipherAndLimit(uint16_t version, uint16_t* cipher,
                              uint64_t* limit = nullptr) {
  uint64_t l;
  if (!limit) limit = &l;

  if (version < SSL_LIBRARY_VERSION_TLS_1_2) {
    *cipher = TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA;
    *limit = 0x5aULL << 28;
  } else if (version == SSL_LIBRARY_VERSION_TLS_1_2) {
    *cipher = TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256;
    *limit = (1ULL << 48) - 1;
  } else {
    // This test probably isn't especially useful for TLS 1.3, which has a much
    // shorter sequence number encoding.  That space can probably be searched in
    // a reasonable amount of time.
    *cipher = TLS_CHACHA20_POLY1305_SHA256;
    // Assume that we are starting with an expected sequence number of 0.
    *limit = (1ULL << 29) - 1;
  }
}

// This simulates a huge number of drops on one side.
// See Bug 12965514 where a large gap was handled very inefficiently.
TEST_P(TlsConnectDatagram, MissLotsOfPackets) {
  uint16_t cipher;
  uint64_t limit;

  GetCipherAndLimit(version_, &cipher, &limit);

  EnsureTlsSetup();
  server_->EnableSingleCipher(cipher);
  Connect();

  // Note that the limit for ChaCha is 2^48-1.
  EXPECT_EQ(SECSuccess,
            SSLInt_AdvanceWriteSeqNum(client_->ssl_fd(), limit - 10));
  SendReceive();
}

// Send a sequence number of 0xfffffffd and it should be interpreted as that
// (and not -3 or UINT64_MAX - 2).
TEST_F(TlsConnectDatagram13, UnderflowSequenceNumber) {
  Connect();
  // This is only valid if short headers are disabled.
  client_->SetOption(SSL_ENABLE_DTLS_SHORT_HEADER, PR_FALSE);
  EXPECT_EQ(SECSuccess,
            SSLInt_AdvanceWriteSeqNum(client_->ssl_fd(), (1ULL << 30) - 3));
  SendReceive();
}

class TlsConnectDatagram12Plus : public TlsConnectDatagram {
 public:
  TlsConnectDatagram12Plus() : TlsConnectDatagram() {}
};

// This simulates missing a window's worth of packets.
TEST_P(TlsConnectDatagram12Plus, MissAWindow) {
  EnsureTlsSetup();
  uint16_t cipher;
  GetCipherAndLimit(version_, &cipher);
  server_->EnableSingleCipher(cipher);
  Connect();
  EXPECT_EQ(SECSuccess, SSLInt_AdvanceWriteSeqByAWindow(client_->ssl_fd(), 0));
  SendReceive();
}

TEST_P(TlsConnectDatagram12Plus, MissAWindowAndOne) {
  EnsureTlsSetup();
  uint16_t cipher;
  GetCipherAndLimit(version_, &cipher);
  server_->EnableSingleCipher(cipher);
  Connect();

  EXPECT_EQ(SECSuccess, SSLInt_AdvanceWriteSeqByAWindow(client_->ssl_fd(), 1));
  SendReceive();
}

// This filter replaces the first record it sees with junk application data.
class TlsReplaceFirstRecordWithJunk : public TlsRecordFilter {
 public:
  TlsReplaceFirstRecordWithJunk(const std::shared_ptr<TlsAgent>& a)
      : TlsRecordFilter(a), replaced_(false) {}

 protected:
  PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                    const DataBuffer& record, size_t* offset,
                                    DataBuffer* output) override {
    if (replaced_) {
      return KEEP;
    }
    replaced_ = true;
    TlsRecordHeader out_header(header.variant(), header.version(),
                               ssl_ct_application_data,
                               header.sequence_number());

    static const uint8_t junk[] = {1, 2, 3, 4};
    *offset = out_header.Write(output, *offset, DataBuffer(junk, sizeof(junk)));
    return CHANGE;
  }

 private:
  bool replaced_;
};

// DTLS needs to discard application_data that it receives prior to handshake
// completion, not generate an error.
TEST_P(TlsConnectDatagram, ReplaceFirstServerRecordWithApplicationData) {
  MakeTlsFilter<TlsReplaceFirstRecordWithJunk>(server_);
  Connect();
}

TEST_P(TlsConnectDatagram, ReplaceFirstClientRecordWithApplicationData) {
  MakeTlsFilter<TlsReplaceFirstRecordWithJunk>(client_);
  Connect();
}

INSTANTIATE_TEST_CASE_P(Datagram12Plus, TlsConnectDatagram12Plus,
                        TlsConnectTestBase::kTlsV12Plus);
INSTANTIATE_TEST_CASE_P(DatagramPre13, TlsConnectDatagramPre13,
                        TlsConnectTestBase::kTlsV11V12);
INSTANTIATE_TEST_CASE_P(DatagramDrop13, TlsDropDatagram13,
                        ::testing::Values(true, false));
INSTANTIATE_TEST_CASE_P(DatagramReorder13, TlsReorderDatagram13,
                        ::testing::Values(true, false));
INSTANTIATE_TEST_CASE_P(DatagramFragment13, TlsFragmentationAndRecoveryTest,
                        ::testing::Values(true, false));

}  // namespace nss_test
