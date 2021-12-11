/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include <vector>
#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"

#include "gtest_utils.h"
#include "tls_connect.h"
#include "tls_filter.h"
#include "tls_parser.h"

namespace nss_test {

class Tls13CompatTest : public TlsConnectStreamTls13 {
 protected:
  void EnableCompatMode() {
    client_->SetOption(SSL_ENABLE_TLS13_COMPAT_MODE, PR_TRUE);
  }

  void InstallFilters() {
    EnsureTlsSetup();
    client_recorders_.Install(client_);
    server_recorders_.Install(server_);
  }

  void CheckRecordVersions() {
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_0,
              client_recorders_.records_->record(0).header.version());
    CheckRecordsAreTls12("client", client_recorders_.records_, 1);
    CheckRecordsAreTls12("server", server_recorders_.records_, 0);
  }

  void CheckHelloVersions() {
    uint32_t ver;
    ASSERT_TRUE(server_recorders_.hello_->buffer().Read(0, 2, &ver));
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, static_cast<uint16_t>(ver));
    ASSERT_TRUE(client_recorders_.hello_->buffer().Read(0, 2, &ver));
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, static_cast<uint16_t>(ver));
  }

  void CheckForCCS(bool expected_client, bool expected_server) {
    client_recorders_.CheckForCCS(expected_client);
    server_recorders_.CheckForCCS(expected_server);
  }

  void CheckForRegularHandshake() {
    CheckRecordVersions();
    CheckHelloVersions();
    EXPECT_EQ(0U, client_recorders_.session_id_length());
    EXPECT_EQ(0U, server_recorders_.session_id_length());
    CheckForCCS(false, false);
  }

  void CheckForCompatHandshake() {
    CheckRecordVersions();
    CheckHelloVersions();
    EXPECT_EQ(32U, client_recorders_.session_id_length());
    EXPECT_EQ(32U, server_recorders_.session_id_length());
    CheckForCCS(true, true);
  }

 private:
  struct Recorders {
    Recorders() : records_(nullptr), hello_(nullptr) {}

    uint8_t session_id_length() const {
      // session_id is always after version (2) and random (32).
      uint32_t len = 0;
      EXPECT_TRUE(hello_->buffer().Read(2 + 32, 1, &len));
      return static_cast<uint8_t>(len);
    }

    void CheckForCCS(bool expected) const {
      EXPECT_LT(0U, records_->count());
      for (size_t i = 0; i < records_->count(); ++i) {
        // Only the second record can be a CCS.
        bool expected_match = expected && (i == 1);
        EXPECT_EQ(expected_match,
                  ssl_ct_change_cipher_spec ==
                      records_->record(i).header.content_type());
      }
    }

    void Install(std::shared_ptr<TlsAgent>& agent) {
      if (records_ && records_->agent() == agent) {
        // Avoid replacing the filters if they are already installed on this
        // agent. This ensures that InstallFilters() can be used after
        // MakeNewServer() without losing state on the client filters.
        return;
      }
      records_.reset(new TlsRecordRecorder(agent));
      hello_.reset(new TlsHandshakeRecorder(
          agent, std::set<uint8_t>(
                     {kTlsHandshakeClientHello, kTlsHandshakeServerHello})));
      agent->SetFilter(std::make_shared<ChainedPacketFilter>(
          ChainedPacketFilterInit({records_, hello_})));
    }

    std::shared_ptr<TlsRecordRecorder> records_;
    std::shared_ptr<TlsHandshakeRecorder> hello_;
  };

  void CheckRecordsAreTls12(const std::string& agent,
                            const std::shared_ptr<TlsRecordRecorder>& records,
                            size_t start) {
    EXPECT_LE(start, records->count());
    for (size_t i = start; i < records->count(); ++i) {
      EXPECT_EQ(SSL_LIBRARY_VERSION_TLS_1_2,
                records->record(i).header.version())
          << agent << ": record " << i << " has wrong version";
    }
  }

  Recorders client_recorders_;
  Recorders server_recorders_;
};

TEST_F(Tls13CompatTest, Disabled) {
  InstallFilters();
  Connect();
  CheckForRegularHandshake();
}

TEST_F(Tls13CompatTest, Enabled) {
  EnableCompatMode();
  InstallFilters();
  Connect();
  CheckForCompatHandshake();
}

TEST_F(Tls13CompatTest, EnabledZeroRtt) {
  SetupForZeroRtt();
  EnableCompatMode();
  InstallFilters();

  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);
  ZeroRttSendReceive(true, true);
  CheckForCCS(true, true);
  Handshake();
  ExpectEarlyDataAccepted(true);
  CheckConnected();

  CheckForCompatHandshake();
}

TEST_F(Tls13CompatTest, EnabledHrr) {
  EnableCompatMode();
  InstallFilters();

  // Force a HelloRetryRequest.  The server sends CCS immediately.
  server_->ConfigNamedGroups({ssl_grp_ec_secp384r1});
  client_->StartConnect();
  server_->StartConnect();
  client_->Handshake();
  server_->Handshake();
  CheckForCCS(false, true);

  Handshake();
  CheckConnected();
  CheckForCompatHandshake();
}

TEST_F(Tls13CompatTest, EnabledStatelessHrr) {
  EnableCompatMode();
  InstallFilters();

  // Force a HelloRetryRequest
  server_->ConfigNamedGroups({ssl_grp_ec_secp384r1});
  client_->StartConnect();
  server_->StartConnect();
  client_->Handshake();
  server_->Handshake();

  // The server should send CCS before HRR.
  CheckForCCS(false, true);

  // A new server should complete the handshake, and not send CCS.
  MakeNewServer();
  InstallFilters();
  server_->ConfigNamedGroups({ssl_grp_ec_secp384r1});

  Handshake();
  CheckConnected();
  CheckRecordVersions();
  CheckHelloVersions();
  CheckForCCS(true, false);
}

TEST_F(Tls13CompatTest, EnabledHrrZeroRtt) {
  SetupForZeroRtt();
  EnableCompatMode();
  InstallFilters();
  server_->ConfigNamedGroups({ssl_grp_ec_secp384r1});

  // With 0-RTT, the client sends CCS immediately.  With HRR, the server sends
  // CCS immediately too.
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);
  ZeroRttSendReceive(true, false);
  CheckForCCS(true, true);

  Handshake();
  ExpectEarlyDataAccepted(false);
  CheckConnected();
  CheckForCompatHandshake();
}

TEST_F(Tls13CompatTest, EnabledAcceptedEch) {
  EnsureTlsSetup();
  SetupEch(client_, server_);
  EnableCompatMode();
  InstallFilters();
  Connect();
  CheckForCompatHandshake();
}

TEST_F(Tls13CompatTest, EnabledRejectedEch) {
  EnsureTlsSetup();
  // Configure ECH on the client only, and expect CCS.
  SetupEch(client_, server_, HpkeDhKemX25519Sha256, false, true, false);
  EnableCompatMode();
  InstallFilters();
  ExpectAlert(client_, kTlsAlertEchRequired);
  ConnectExpectFailOneSide(TlsAgent::CLIENT);
  client_->CheckErrorCode(SSL_ERROR_ECH_RETRY_WITHOUT_ECH);
  CheckForCompatHandshake();
  // Reset expectations for the TlsAgent dtor.
  server_->ExpectReceiveAlert(kTlsAlertCloseNotify, kTlsAlertWarning);
}

class TlsSessionIDEchoFilter : public TlsHandshakeFilter {
 public:
  TlsSessionIDEchoFilter(const std::shared_ptr<TlsAgent>& a)
      : TlsHandshakeFilter(
            a, {kTlsHandshakeClientHello, kTlsHandshakeServerHello}) {}

 protected:
  virtual PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                               const DataBuffer& input,
                                               DataBuffer* output) {
    TlsParser parser(input);

    // Skip version + random.
    EXPECT_TRUE(parser.Skip(2 + 32));

    // Capture CH.legacy_session_id.
    if (header.handshake_type() == kTlsHandshakeClientHello) {
      EXPECT_TRUE(parser.ReadVariable(&sid_, 1));
      return KEEP;
    }

    // Check that server sends one too.
    uint32_t sid_len = 0;
    EXPECT_TRUE(parser.Read(&sid_len, 1));
    EXPECT_EQ(sid_len, sid_.len());

    // Echo the one we captured.
    *output = input;
    output->Write(parser.consumed(), sid_.data(), sid_.len());

    return CHANGE;
  }

 private:
  DataBuffer sid_;
};

TEST_F(TlsConnectTest, EchoTLS13CompatibilitySessionID) {
  ConfigureSessionCache(RESUME_SESSIONID, RESUME_SESSIONID);

  client_->SetOption(SSL_ENABLE_TLS13_COMPAT_MODE, PR_TRUE);

  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);

  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_2);

  server_->SetFilter(MakeTlsFilter<TlsSessionIDEchoFilter>(client_));
  ConnectExpectAlert(client_, kTlsAlertIllegalParameter);

  client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_SERVER_HELLO);
  server_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

class TlsSessionIDInjectFilter : public TlsHandshakeFilter {
 public:
  TlsSessionIDInjectFilter(const std::shared_ptr<TlsAgent>& a)
      : TlsHandshakeFilter(a, {kTlsHandshakeServerHello}) {}

 protected:
  virtual PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                               const DataBuffer& input,
                                               DataBuffer* output) {
    TlsParser parser(input);

    // Skip version + random.
    EXPECT_TRUE(parser.Skip(2 + 32));

    *output = input;

    // Inject a Session ID.
    const uint8_t fake_sid[SSL3_SESSIONID_BYTES] = {0xff};
    output->Write(parser.consumed(), sizeof(fake_sid), 1);
    output->Splice(fake_sid, sizeof(fake_sid), parser.consumed() + 1, 0);

    return CHANGE;
  }
};

TEST_F(TlsConnectTest, TLS13NonCompatModeSessionID) {
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);

  MakeTlsFilter<TlsSessionIDInjectFilter>(server_);
  client_->ExpectSendAlert(kTlsAlertIllegalParameter);
  server_->ExpectSendAlert(kTlsAlertUnexpectedMessage);
  ConnectExpectFail();

  client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_SERVER_HELLO);
  server_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_RECORD_TYPE);
}

static const uint8_t kCannedCcs[] = {
    ssl_ct_change_cipher_spec,
    SSL_LIBRARY_VERSION_TLS_1_2 >> 8,
    SSL_LIBRARY_VERSION_TLS_1_2 & 0xff,
    0,
    1,  // length
    1   // change_cipher_spec_choice
};

// A ChangeCipherSpec is ignored by a server because we have to tolerate it for
// compatibility mode.  That doesn't mean that we have to tolerate it
// unconditionally.  If we negotiate 1.3, we expect to see a cookie extension.
TEST_F(TlsConnectStreamTls13, ChangeCipherSpecBeforeClientHello13) {
  EnsureTlsSetup();
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  // Client sends CCS before starting the handshake.
  client_->SendDirect(DataBuffer(kCannedCcs, sizeof(kCannedCcs)));
  ConnectExpectAlert(server_, kTlsAlertUnexpectedMessage);
  server_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_CHANGE_CIPHER);
  client_->CheckErrorCode(SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT);
}

// A ChangeCipherSpec is ignored by a server because we have to tolerate it for
// compatibility mode.  That doesn't mean that we have to tolerate it
// unconditionally.  If we negotiate 1.3, we expect to see a cookie extension.
TEST_F(TlsConnectStreamTls13, ChangeCipherSpecBeforeClientHelloTwice) {
  EnsureTlsSetup();
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  // Client sends CCS before starting the handshake.
  client_->SendDirect(DataBuffer(kCannedCcs, sizeof(kCannedCcs)));
  client_->SendDirect(DataBuffer(kCannedCcs, sizeof(kCannedCcs)));
  ConnectExpectAlert(server_, kTlsAlertUnexpectedMessage);
  server_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_CHANGE_CIPHER);
  client_->CheckErrorCode(SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT);
}

// The server accepts a ChangeCipherSpec even if the client advertises
// an empty session ID.
TEST_F(TlsConnectStreamTls13, ChangeCipherSpecAfterClientHelloEmptySid) {
  EnsureTlsSetup();
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);

  StartConnect();
  client_->Handshake();  // Send ClientHello
  client_->SendDirect(DataBuffer(kCannedCcs, sizeof(kCannedCcs)));  // Send CCS

  Handshake();
  CheckConnected();
}

// The server rejects multiple ChangeCipherSpec even if the client
// indicates compatibility mode with non-empty session ID.
TEST_F(Tls13CompatTest, ChangeCipherSpecAfterClientHelloTwice) {
  EnsureTlsSetup();
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  EnableCompatMode();

  StartConnect();
  client_->Handshake();  // Send ClientHello
  // Send CCS twice in a row
  client_->SendDirect(DataBuffer(kCannedCcs, sizeof(kCannedCcs)));
  client_->SendDirect(DataBuffer(kCannedCcs, sizeof(kCannedCcs)));

  server_->ExpectSendAlert(kTlsAlertUnexpectedMessage);
  server_->Handshake();  // Consume ClientHello and CCS.
  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CHANGE_CIPHER);
}

// The client accepts a ChangeCipherSpec even if it advertises an empty
// session ID.
TEST_F(TlsConnectStreamTls13, ChangeCipherSpecAfterServerHelloEmptySid) {
  EnsureTlsSetup();
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);

  // To replace Finished with a CCS below
  auto filter = MakeTlsFilter<TlsHandshakeDropper>(server_);
  filter->SetHandshakeTypes({kTlsHandshakeFinished});
  filter->EnableDecryption();

  StartConnect();
  client_->Handshake();  // Send ClientHello
  server_->Handshake();  // Consume ClientHello, and
                         // send ServerHello..CertificateVerify
  // Send CCS
  server_->SendDirect(DataBuffer(kCannedCcs, sizeof(kCannedCcs)));

  // No alert is sent from the client. As Finished is dropped, we
  // can't use Handshake() and CheckConnected().
  client_->Handshake();
}

// The client rejects multiple ChangeCipherSpec in a row even if the
// client indicates compatibility mode with non-empty session ID.
TEST_F(Tls13CompatTest, ChangeCipherSpecAfterServerHelloTwice) {
  EnsureTlsSetup();
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  EnableCompatMode();

  // To replace Finished with a CCS below
  auto filter = MakeTlsFilter<TlsHandshakeDropper>(server_);
  filter->SetHandshakeTypes({kTlsHandshakeFinished});
  filter->EnableDecryption();

  StartConnect();
  client_->Handshake();  // Send ClientHello
  server_->Handshake();  // Consume ClientHello, and
                         // send ServerHello..CertificateVerify
                         // the ServerHello is followed by CCS
  // Send another CCS
  server_->SendDirect(DataBuffer(kCannedCcs, sizeof(kCannedCcs)));
  client_->ExpectSendAlert(kTlsAlertUnexpectedMessage);
  client_->Handshake();  // Consume ClientHello and CCS
  client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CHANGE_CIPHER);
}

// If we negotiate 1.2, we abort.
TEST_F(TlsConnectStreamTls13, ChangeCipherSpecBeforeClientHello12) {
  EnsureTlsSetup();
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  // Client sends CCS before starting the handshake.
  client_->SendDirect(DataBuffer(kCannedCcs, sizeof(kCannedCcs)));
  ConnectExpectAlert(server_, kTlsAlertUnexpectedMessage);
  server_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_CHANGE_CIPHER);
  client_->CheckErrorCode(SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT);
}

TEST_F(TlsConnectStreamTls13, ChangeCipherSpecAfterFinished13) {
  EnsureTlsSetup();
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  Connect();
  SendReceive(10);
  // Client sends CCS after the handshake.
  client_->SendDirect(DataBuffer(kCannedCcs, sizeof(kCannedCcs)));
  server_->ExpectSendAlert(kTlsAlertUnexpectedMessage);
  server_->ExpectReadWriteError();
  server_->ReadBytes();
  EXPECT_EQ(SSL_ERROR_RX_UNEXPECTED_RECORD_TYPE, server_->error_code());
}

TEST_F(TlsConnectDatagram13, CompatModeDtlsClient) {
  EnsureTlsSetup();
  client_->SetOption(SSL_ENABLE_TLS13_COMPAT_MODE, PR_TRUE);
  auto client_records = MakeTlsFilter<TlsRecordRecorder>(client_);
  auto server_records = MakeTlsFilter<TlsRecordRecorder>(server_);
  Connect();

  ASSERT_EQ(2U, client_records->count());  // CH, Fin
  EXPECT_EQ(ssl_ct_handshake, client_records->record(0).header.content_type());
  EXPECT_EQ(kCtDtlsCiphertext,
            (client_records->record(1).header.content_type() &
             kCtDtlsCiphertextMask));

  ASSERT_EQ(6U, server_records->count());  // SH, EE, CT, CV, Fin, Ack
  EXPECT_EQ(ssl_ct_handshake, server_records->record(0).header.content_type());
  for (size_t i = 1; i < server_records->count(); ++i) {
    EXPECT_EQ(kCtDtlsCiphertext,
              (server_records->record(i).header.content_type() &
               kCtDtlsCiphertextMask));
  }
}

class AddSessionIdFilter : public TlsHandshakeFilter {
 public:
  AddSessionIdFilter(const std::shared_ptr<TlsAgent>& client)
      : TlsHandshakeFilter(client, {ssl_hs_client_hello}) {}

 protected:
  PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                       const DataBuffer& input,
                                       DataBuffer* output) override {
    uint32_t session_id_len = 0;
    EXPECT_TRUE(input.Read(2 + 32, 1, &session_id_len));
    EXPECT_EQ(0U, session_id_len);
    uint8_t session_id[33] = {32};  // 32 for length, the rest zero.
    *output = input;
    output->Splice(session_id, sizeof(session_id), 34, 1);
    return CHANGE;
  }
};

// Adding a session ID to a DTLS ClientHello should not trigger compatibility
// mode.  It should be ignored instead.
TEST_F(TlsConnectDatagram13, CompatModeDtlsServer) {
  EnsureTlsSetup();
  auto client_records = std::make_shared<TlsRecordRecorder>(client_);
  client_->SetFilter(
      std::make_shared<ChainedPacketFilter>(ChainedPacketFilterInit(
          {client_records, std::make_shared<AddSessionIdFilter>(client_)})));
  auto server_hello =
      std::make_shared<TlsHandshakeRecorder>(server_, kTlsHandshakeServerHello);
  auto server_records = std::make_shared<TlsRecordRecorder>(server_);
  server_->SetFilter(std::make_shared<ChainedPacketFilter>(
      ChainedPacketFilterInit({server_records, server_hello})));
  StartConnect();
  client_->Handshake();
  server_->Handshake();
  // The client will consume the ServerHello, but discard everything else
  // because it doesn't decrypt.  And don't wait around for the client to ACK.
  client_->Handshake();

  ASSERT_EQ(1U, client_records->count());
  EXPECT_EQ(ssl_ct_handshake, client_records->record(0).header.content_type());

  ASSERT_EQ(5U, server_records->count());  // SH, EE, CT, CV, Fin
  EXPECT_EQ(ssl_ct_handshake, server_records->record(0).header.content_type());
  for (size_t i = 1; i < server_records->count(); ++i) {
    EXPECT_EQ(kCtDtlsCiphertext,
              (server_records->record(i).header.content_type() &
               kCtDtlsCiphertextMask));
  }

  uint32_t session_id_len = 0;
  EXPECT_TRUE(server_hello->buffer().Read(2 + 32, 1, &session_id_len));
  EXPECT_EQ(0U, session_id_len);
}

TEST_F(Tls13CompatTest, ConnectWith12ThenAttemptToResume13CompatMode) {
  ConfigureSessionCache(RESUME_SESSIONID, RESUME_SESSIONID);
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_2);
  Connect();

  Reset();
  ExpectResumption(RESUME_NONE);
  version_ = SSL_LIBRARY_VERSION_TLS_1_3;
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  EnableCompatMode();
  Connect();
}

}  // namespace nss_test
