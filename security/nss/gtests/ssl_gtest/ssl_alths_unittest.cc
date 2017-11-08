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

static const uint32_t kServerHelloVersionAlt = SSL_LIBRARY_VERSION_TLS_1_2;
static const uint16_t kServerHelloVersionRegular =
    0x7f00 | TLS_1_3_DRAFT_VERSION;

class AltHandshakeTest : public TlsConnectStreamTls13 {
 protected:
  void SetUp() {
    TlsConnectStreamTls13::SetUp();
    client_ccs_recorder_ =
        std::make_shared<TlsRecordRecorder>(kTlsChangeCipherSpecType);
    server_handshake_recorder_ =
        std::make_shared<TlsRecordRecorder>(kTlsHandshakeType);
    server_ccs_recorder_ =
        std::make_shared<TlsRecordRecorder>(kTlsChangeCipherSpecType);
    server_hello_recorder_ =
        std::make_shared<TlsInspectorRecordHandshakeMessage>(
            kTlsHandshakeServerHello);
  }

  void SetAltHandshakeTypeEnabled() {
    client_->SetAltHandshakeTypeEnabled();
    server_->SetAltHandshakeTypeEnabled();
  }

  void InstallFilters() {
    client_->SetPacketFilter(client_ccs_recorder_);
    auto chain = std::make_shared<ChainedPacketFilter>(ChainedPacketFilterInit(
        {server_handshake_recorder_, server_ccs_recorder_,
         server_hello_recorder_}));
    server_->SetPacketFilter(chain);
  }

  void CheckServerHelloRecordVersion(uint16_t record_version) {
    ASSERT_EQ(record_version,
              server_handshake_recorder_->record(0).header.version());
  }

  void CheckServerHelloVersion(uint16_t server_hello_version) {
    uint32_t ver;
    ASSERT_TRUE(server_hello_recorder_->buffer().Read(0, 2, &ver));
    ASSERT_EQ(server_hello_version, ver);
  }

  void CheckForRegularHandshake() {
    EXPECT_EQ(0U, client_ccs_recorder_->count());
    EXPECT_EQ(0U, server_ccs_recorder_->count());
    CheckServerHelloVersion(kServerHelloVersionRegular);
    CheckServerHelloRecordVersion(SSL_LIBRARY_VERSION_TLS_1_0);
  }

  void CheckForAltHandshake() {
    EXPECT_EQ(1U, client_ccs_recorder_->count());
    EXPECT_EQ(1U, server_ccs_recorder_->count());
    CheckServerHelloVersion(kServerHelloVersionAlt);
    CheckServerHelloRecordVersion(SSL_LIBRARY_VERSION_TLS_1_2);
  }

  std::shared_ptr<TlsRecordRecorder> client_ccs_recorder_;
  std::shared_ptr<TlsRecordRecorder> server_handshake_recorder_;
  std::shared_ptr<TlsRecordRecorder> server_ccs_recorder_;
  std::shared_ptr<TlsInspectorRecordHandshakeMessage> server_hello_recorder_;
};

TEST_F(AltHandshakeTest, ClientOnly) {
  client_->SetAltHandshakeTypeEnabled();
  InstallFilters();
  Connect();
  CheckForRegularHandshake();
}

TEST_F(AltHandshakeTest, ServerOnly) {
  server_->SetAltHandshakeTypeEnabled();
  InstallFilters();
  Connect();
  CheckForRegularHandshake();
}

TEST_F(AltHandshakeTest, Enabled) {
  SetAltHandshakeTypeEnabled();
  InstallFilters();
  Connect();
  CheckForAltHandshake();
}

TEST_F(AltHandshakeTest, ZeroRtt) {
  SetAltHandshakeTypeEnabled();
  SetupForZeroRtt();
  SetAltHandshakeTypeEnabled();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);

  InstallFilters();

  ExpectResumption(RESUME_TICKET);
  ZeroRttSendReceive(true, true);
  Handshake();
  ExpectEarlyDataAccepted(true);
  CheckConnected();

  CheckForAltHandshake();
}

// Neither client nor server has the extension prior to resumption, so the
// client doesn't send a CCS before its 0-RTT data.
TEST_F(AltHandshakeTest, DisabledBeforeZeroRtt) {
  SetupForZeroRtt();
  SetAltHandshakeTypeEnabled();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);

  InstallFilters();

  ExpectResumption(RESUME_TICKET);
  ZeroRttSendReceive(true, true);
  Handshake();
  ExpectEarlyDataAccepted(true);
  CheckConnected();

  EXPECT_EQ(0U, client_ccs_recorder_->count());
  EXPECT_EQ(1U, server_ccs_recorder_->count());
  CheckServerHelloVersion(kServerHelloVersionAlt);
}

// Both use the alternative in the initial handshake but only the server enables
// it on resumption.
TEST_F(AltHandshakeTest, ClientDisabledAfterZeroRtt) {
  SetAltHandshakeTypeEnabled();
  SetupForZeroRtt();
  server_->SetAltHandshakeTypeEnabled();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);

  InstallFilters();

  ExpectResumption(RESUME_TICKET);
  ZeroRttSendReceive(true, true);
  Handshake();
  ExpectEarlyDataAccepted(true);
  CheckConnected();

  CheckForRegularHandshake();
}

// If the alternative handshake isn't negotiated after 0-RTT, and the client has
// it enabled, it will send a ChangeCipherSpec.  The server chokes on it if it
// hasn't negotiated the alternative handshake.
TEST_F(AltHandshakeTest, ServerDisabledAfterZeroRtt) {
  SetAltHandshakeTypeEnabled();
  SetupForZeroRtt();
  client_->SetAltHandshakeTypeEnabled();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);

  client_->ExpectSendAlert(kTlsAlertEndOfEarlyData);
  client_->Handshake();  // Send ClientHello (and CCS)

  server_->Handshake();  // Consume the ClientHello, which is OK.
  client_->ExpectResumption();
  client_->Handshake();  // Read the server handshake.
  EXPECT_EQ(TlsAgent::STATE_CONNECTED, client_->state());

  // Now the server reads the CCS instead of more handshake messages.
  ExpectAlert(server_, kTlsAlertBadRecordMac);
  server_->Handshake();
  EXPECT_EQ(TlsAgent::STATE_ERROR, server_->state());
  client_->Handshake();  // Consume the alert.
  EXPECT_EQ(TlsAgent::STATE_ERROR, client_->state());
}

}  // nss_test
