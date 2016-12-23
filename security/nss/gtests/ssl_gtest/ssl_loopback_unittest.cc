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

TEST_P(TlsConnectGeneric, SetupOnly) {}

TEST_P(TlsConnectGeneric, Connect) {
  SetExpectedVersion(std::get<1>(GetParam()));
  Connect();
  CheckKeys();
}

TEST_P(TlsConnectGeneric, ConnectEcdsa) {
  SetExpectedVersion(std::get<1>(GetParam()));
  Reset(TlsAgent::kServerEcdsa256);
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_ecdsa);
}

TEST_P(TlsConnectGenericPre13, CipherSuiteMismatch) {
  EnsureTlsSetup();
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    client_->EnableSingleCipher(TLS_AES_128_GCM_SHA256);
    server_->EnableSingleCipher(TLS_AES_256_GCM_SHA384);
  } else {
    client_->EnableSingleCipher(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA);
    server_->EnableSingleCipher(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA);
  }
  ConnectExpectFail();
  client_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
  server_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
}

TEST_P(TlsConnectGenericPre13, ConnectFalseStart) {
  client_->EnableFalseStart();
  Connect();
  SendReceive();
}

TEST_P(TlsConnectGeneric, ConnectAlpn) {
  EnableAlpn();
  Connect();
  CheckAlpn("a");
}

TEST_P(TlsConnectGeneric, ConnectAlpnClone) {
  EnsureModelSockets();
  client_model_->EnableAlpn(alpn_dummy_val_, sizeof(alpn_dummy_val_));
  server_model_->EnableAlpn(alpn_dummy_val_, sizeof(alpn_dummy_val_));
  Connect();
  CheckAlpn("a");
}

TEST_P(TlsConnectDatagram, ConnectSrtp) {
  EnableSrtp();
  Connect();
  CheckSrtp();
  SendReceive();
}

// 1.3 is disabled in the next few tests because we don't
// presently support resumption in 1.3.
TEST_P(TlsConnectStreamPre13, ConnectAndClientRenegotiate) {
  Connect();
  server_->PrepareForRenegotiate();
  client_->StartRenegotiate();
  Handshake();
  CheckConnected();
}

TEST_P(TlsConnectStreamPre13, ConnectAndServerRenegotiate) {
  Connect();
  client_->PrepareForRenegotiate();
  server_->StartRenegotiate();
  Handshake();
  CheckConnected();
}

TEST_P(TlsConnectGeneric, ConnectSendReceive) {
  Connect();
  SendReceive();
}

// The next two tests takes advantage of the fact that we
// automatically read the first 1024 bytes, so if
// we provide 1200 bytes, they overrun the read buffer
// provided by the calling test.

// DTLS should return an error.
TEST_P(TlsConnectDatagram, ShortRead) {
  Connect();
  client_->ExpectReadWriteError();
  server_->SendData(1200, 1200);
  client_->WaitForErrorCode(SSL_ERROR_RX_SHORT_DTLS_READ, 2000);

  // Now send and receive another packet.
  server_->ResetSentBytes();  // Reset the counter.
  SendReceive();
}

// TLS should get the write in two chunks.
TEST_P(TlsConnectStream, ShortRead) {
  // This test behaves oddly with TLS 1.0 because of 1/n+1 splitting,
  // so skip in that case.
  if (version_ < SSL_LIBRARY_VERSION_TLS_1_1) return;

  Connect();
  server_->SendData(1200, 1200);
  // Read the first tranche.
  WAIT_(client_->received_bytes() == 1024, 2000);
  ASSERT_EQ(1024U, client_->received_bytes());
  // The second tranche should now immediately be available.
  client_->ReadBytes();
  ASSERT_EQ(1200U, client_->received_bytes());
}

TEST_P(TlsConnectGeneric, ConnectWithCompressionMaybe) {
  EnsureTlsSetup();
  client_->EnableCompression();
  server_->EnableCompression();
  Connect();
  EXPECT_EQ(client_->version() < SSL_LIBRARY_VERSION_TLS_1_3 && mode_ != DGRAM,
            client_->is_compressed());
  SendReceive();
}

TEST_P(TlsConnectDatagram, TestDtlsHolddownExpiry) {
  Connect();
  std::cerr << "Expiring holddown timer\n";
  SSLInt_ForceTimerExpiry(client_->ssl_fd());
  SSLInt_ForceTimerExpiry(server_->ssl_fd());
  SendReceive();
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    // One for send, one for receive.
    EXPECT_EQ(2, SSLInt_CountTls13CipherSpecs(client_->ssl_fd()));
  }
}

class TlsPreCCSHeaderInjector : public TlsRecordFilter {
 public:
  TlsPreCCSHeaderInjector() {}
  virtual PacketFilter::Action FilterRecord(
      const TlsRecordHeader& record_header, const DataBuffer& input,
      size_t* offset, DataBuffer* output) override {
    if (record_header.content_type() != kTlsChangeCipherSpecType) return KEEP;

    std::cerr << "Injecting Finished header before CCS\n";
    const uint8_t hhdr[] = {kTlsHandshakeFinished, 0x00, 0x00, 0x0c};
    DataBuffer hhdr_buf(hhdr, sizeof(hhdr));
    TlsRecordHeader nhdr(record_header.version(), kTlsHandshakeType, 0);
    *offset = nhdr.Write(output, *offset, hhdr_buf);
    *offset = record_header.Write(output, *offset, input);
    return CHANGE;
  }
};

TEST_P(TlsConnectStreamPre13, ClientFinishedHeaderBeforeCCS) {
  client_->SetPacketFilter(new TlsPreCCSHeaderInjector());
  ConnectExpectFail();
  client_->CheckErrorCode(SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT);
  server_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_CHANGE_CIPHER);
}

TEST_P(TlsConnectStreamPre13, ServerFinishedHeaderBeforeCCS) {
  server_->SetPacketFilter(new TlsPreCCSHeaderInjector());
  client_->StartConnect();
  server_->StartConnect();
  Handshake();
  EXPECT_EQ(TlsAgent::STATE_ERROR, client_->state());
  client_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_CHANGE_CIPHER);
  EXPECT_EQ(TlsAgent::STATE_CONNECTED, server_->state());
}

TEST_P(TlsConnectTls13, UnknownAlert) {
  Connect();
  SSLInt_SendAlert(server_->ssl_fd(), kTlsAlertWarning,
                   0xff);  // Unknown value.
  client_->ExpectReadWriteError();
  client_->WaitForErrorCode(SSL_ERROR_RX_UNKNOWN_ALERT, 2000);
}

TEST_P(TlsConnectTls13, AlertWrongLevel) {
  Connect();
  SSLInt_SendAlert(server_->ssl_fd(), kTlsAlertWarning,
                   kTlsAlertUnexpectedMessage);
  client_->ExpectReadWriteError();
  client_->WaitForErrorCode(SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT, 2000);
}

TEST_F(TlsConnectStreamTls13, Tls13FailedWriteSecondFlight) {
  EnsureTlsSetup();
  client_->StartConnect();
  server_->StartConnect();
  client_->Handshake();
  server_->Handshake();  // Send first flight.
  client_->adapter()->CloseWrites();
  client_->Handshake();  // This will get an error, but shouldn't crash.
  client_->CheckErrorCode(SSL_ERROR_SOCKET_WRITE_FAILURE);
}

TEST_F(TlsConnectStreamTls13, NegotiateShortHeaders) {
  client_->SetShortHeadersEnabled();
  server_->SetShortHeadersEnabled();
  client_->ExpectShortHeaders();
  server_->ExpectShortHeaders();
  Connect();
}

INSTANTIATE_TEST_CASE_P(GenericStream, TlsConnectGeneric,
                        ::testing::Combine(TlsConnectTestBase::kTlsModesStream,
                                           TlsConnectTestBase::kTlsVAll));
INSTANTIATE_TEST_CASE_P(
    GenericDatagram, TlsConnectGeneric,
    ::testing::Combine(TlsConnectTestBase::kTlsModesDatagram,
                       TlsConnectTestBase::kTlsV11Plus));

INSTANTIATE_TEST_CASE_P(StreamOnly, TlsConnectStream,
                        TlsConnectTestBase::kTlsVAll);
INSTANTIATE_TEST_CASE_P(DatagramOnly, TlsConnectDatagram,
                        TlsConnectTestBase::kTlsV11Plus);

INSTANTIATE_TEST_CASE_P(Pre12Stream, TlsConnectPre12,
                        ::testing::Combine(TlsConnectTestBase::kTlsModesStream,
                                           TlsConnectTestBase::kTlsV10V11));
INSTANTIATE_TEST_CASE_P(
    Pre12Datagram, TlsConnectPre12,
    ::testing::Combine(TlsConnectTestBase::kTlsModesDatagram,
                       TlsConnectTestBase::kTlsV11));

INSTANTIATE_TEST_CASE_P(Version12Only, TlsConnectTls12,
                        TlsConnectTestBase::kTlsModesAll);
#ifndef NSS_DISABLE_TLS_1_3
INSTANTIATE_TEST_CASE_P(Version13Only, TlsConnectTls13,
                        TlsConnectTestBase::kTlsModesAll);
#endif

INSTANTIATE_TEST_CASE_P(Pre13Stream, TlsConnectGenericPre13,
                        ::testing::Combine(TlsConnectTestBase::kTlsModesStream,
                                           TlsConnectTestBase::kTlsV10ToV12));
INSTANTIATE_TEST_CASE_P(
    Pre13Datagram, TlsConnectGenericPre13,
    ::testing::Combine(TlsConnectTestBase::kTlsModesDatagram,
                       TlsConnectTestBase::kTlsV11V12));
INSTANTIATE_TEST_CASE_P(Pre13StreamOnly, TlsConnectStreamPre13,
                        TlsConnectTestBase::kTlsV10ToV12);

INSTANTIATE_TEST_CASE_P(Version12Plus, TlsConnectTls12Plus,
                        ::testing::Combine(TlsConnectTestBase::kTlsModesAll,
                                           TlsConnectTestBase::kTlsV12Plus));

}  // namespace nspr_test
