/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secerr.h"
#include "ssl.h"

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

TEST_P(TlsConnectDatagram, DropClientFirstFlightOnce) {
  client_->SetPacketFilter(new SelectiveDropFilter(0x1));
  Connect();
  SendReceive();
}

TEST_P(TlsConnectDatagram, DropServerFirstFlightOnce) {
  server_->SetPacketFilter(new SelectiveDropFilter(0x1));
  Connect();
  SendReceive();
}

// This drops the first transmission from both the client and server of all
// flights that they send.  Note: In DTLS 1.3, the shorter handshake means that
// this will also drop some application data, so we can't call SendReceive().
TEST_P(TlsConnectDatagram, DropAllFirstTransmissions) {
  client_->SetPacketFilter(new SelectiveDropFilter(0x15));
  server_->SetPacketFilter(new SelectiveDropFilter(0x5));
  Connect();
}

// This drops the server's first flight three times.
TEST_P(TlsConnectDatagram, DropServerFirstFlightThrice) {
  server_->SetPacketFilter(new SelectiveDropFilter(0x7));
  Connect();
}

// This drops the client's second flight once
TEST_P(TlsConnectDatagram, DropClientSecondFlightOnce) {
  client_->SetPacketFilter(new SelectiveDropFilter(0x2));
  Connect();
}

// This drops the client's second flight three times.
TEST_P(TlsConnectDatagram, DropClientSecondFlightThrice) {
  client_->SetPacketFilter(new SelectiveDropFilter(0xe));
  Connect();
}

// This drops the server's second flight three times.
TEST_P(TlsConnectDatagram, DropServerSecondFlightThrice) {
  server_->SetPacketFilter(new SelectiveDropFilter(0xe));
  Connect();
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
    *cipher = TLS_CHACHA20_POLY1305_SHA256;
    *limit = (1ULL << 48) - 1;
  }
}

// This simulates a huge number of drops on one side.
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

INSTANTIATE_TEST_CASE_P(Datagram12Plus, TlsConnectDatagram12Plus,
                        TlsConnectTestBase::kTlsV12Plus);

}  // namespace nss_test
