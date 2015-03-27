/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ssl.h"
#include "sslproto.h"

#include <memory>

#include "tls_parser.h"
#include "tls_filter.h"
#include "tls_connect.h"

namespace nss_test {

class TlsServerKeyExchangeECDHE {
 public:
  bool Parse(const DataBuffer& buffer) {
    TlsParser parser(buffer);

    uint8_t curve_type;
    if (!parser.Read(&curve_type)) {
      return false;
    }

    if (curve_type != 3) {  // named_curve
      return false;
    }

    uint32_t named_curve;
    if (!parser.Read(&named_curve, 2)) {
      return false;
    }

    return parser.ReadVariable(&public_key_, 1);
  }

  DataBuffer public_key_;
};

TEST_P(TlsConnectGeneric, SetupOnly) {}

TEST_P(TlsConnectGeneric, Connect) {
  Connect();
  client_->CheckVersion(SSL_LIBRARY_VERSION_TLS_1_2);
}

TEST_P(TlsConnectGeneric, ConnectResumed) {
  ConfigureSessionCache(RESUME_SESSIONID, RESUME_SESSIONID);
  Connect();

  Reset();
  Connect();
  CheckResumption(RESUME_SESSIONID);
}

TEST_P(TlsConnectGeneric, ConnectClientCacheDisabled) {
  ConfigureSessionCache(RESUME_NONE, RESUME_SESSIONID);
  Connect();
  Reset();
  Connect();
  CheckResumption(RESUME_NONE);
}

TEST_P(TlsConnectGeneric, ConnectServerCacheDisabled) {
  ConfigureSessionCache(RESUME_SESSIONID, RESUME_NONE);
  Connect();
  Reset();
  Connect();
  CheckResumption(RESUME_NONE);
}

TEST_P(TlsConnectGeneric, ConnectSessionCacheDisabled) {
  ConfigureSessionCache(RESUME_NONE, RESUME_NONE);
  Connect();
  Reset();
  Connect();
  CheckResumption(RESUME_NONE);
}

TEST_P(TlsConnectGeneric, ConnectResumeSupportBoth) {
  // This prefers tickets.
  ConfigureSessionCache(RESUME_BOTH, RESUME_BOTH);
  Connect();

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_BOTH);
  Connect();
  CheckResumption(RESUME_TICKET);
}

TEST_P(TlsConnectGeneric, ConnectResumeClientTicketServerBoth) {
  // This causes no resumption because the client needs the
  // session cache to resume even with tickets.
  ConfigureSessionCache(RESUME_TICKET, RESUME_BOTH);
  Connect();

  Reset();
  ConfigureSessionCache(RESUME_TICKET, RESUME_BOTH);
  Connect();
  CheckResumption(RESUME_NONE);
}

TEST_P(TlsConnectGeneric, ConnectResumeClientBothTicketServerTicket) {
  // This causes a ticket resumption.
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  Connect();

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  Connect();
  CheckResumption(RESUME_TICKET);
}

TEST_P(TlsConnectGeneric, ConnectClientServerTicketOnly) {
  // This causes no resumption because the client needs the
  // session cache to resume even with tickets.
  ConfigureSessionCache(RESUME_TICKET, RESUME_TICKET);
  Connect();

  Reset();
  ConfigureSessionCache(RESUME_TICKET, RESUME_TICKET);
  Connect();
  CheckResumption(RESUME_NONE);
}

TEST_P(TlsConnectGeneric, ConnectClientBothServerNone) {
  ConfigureSessionCache(RESUME_BOTH, RESUME_NONE);
  Connect();

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_NONE);
  Connect();
  CheckResumption(RESUME_NONE);
}

TEST_P(TlsConnectGeneric, ConnectClientNoneServerBoth) {
  ConfigureSessionCache(RESUME_NONE, RESUME_BOTH);
  Connect();

  Reset();
  ConfigureSessionCache(RESUME_NONE, RESUME_BOTH);
  Connect();
  CheckResumption(RESUME_NONE);
}

TEST_P(TlsConnectGeneric, ConnectAlpn) {
  EnableAlpn();
  Connect();
  client_->CheckAlpn(SSL_NEXT_PROTO_SELECTED, "a");
  server_->CheckAlpn(SSL_NEXT_PROTO_NEGOTIATED, "a");
}

TEST_P(TlsConnectDatagram, ConnectSrtp) {
  EnableSrtp();
  Connect();
  CheckSrtp();
}

TEST_P(TlsConnectStream, ConnectECDHE) {
  EnableSomeECDHECiphers();
  Connect();
  client_->CheckKEAType(ssl_kea_ecdh);
}

TEST_P(TlsConnectStream, ConnectECDHETwiceReuseKey) {
  EnableSomeECDHECiphers();
  TlsInspectorRecordHandshakeMessage* i1 =
      new TlsInspectorRecordHandshakeMessage(kTlsHandshakeServerKeyExchange);
  server_->SetPacketFilter(i1);
  Connect();
  client_->CheckKEAType(ssl_kea_ecdh);
  TlsServerKeyExchangeECDHE dhe1;
  ASSERT_TRUE(dhe1.Parse(i1->buffer()));

  // Restart
  Reset();
  TlsInspectorRecordHandshakeMessage* i2 =
      new TlsInspectorRecordHandshakeMessage(kTlsHandshakeServerKeyExchange);
  server_->SetPacketFilter(i2);
  EnableSomeECDHECiphers();
  ConfigureSessionCache(RESUME_NONE, RESUME_NONE);
  Connect();
  client_->CheckKEAType(ssl_kea_ecdh);

  TlsServerKeyExchangeECDHE dhe2;
  ASSERT_TRUE(dhe2.Parse(i2->buffer()));

  // Make sure they are the same.
  ASSERT_EQ(dhe1.public_key_.len(), dhe2.public_key_.len());
  ASSERT_TRUE(!memcmp(dhe1.public_key_.data(), dhe2.public_key_.data(),
                      dhe1.public_key_.len()));
}

TEST_P(TlsConnectStream, ConnectECDHETwiceNewKey) {
  EnableSomeECDHECiphers();
  SECStatus rv =
      SSL_OptionSet(server_->ssl_fd(), SSL_REUSE_SERVER_ECDHE_KEY, PR_FALSE);
  ASSERT_EQ(SECSuccess, rv);
  TlsInspectorRecordHandshakeMessage* i1 =
      new TlsInspectorRecordHandshakeMessage(kTlsHandshakeServerKeyExchange);
  server_->SetPacketFilter(i1);
  Connect();
  client_->CheckKEAType(ssl_kea_ecdh);
  TlsServerKeyExchangeECDHE dhe1;
  ASSERT_TRUE(dhe1.Parse(i1->buffer()));

  // Restart
  Reset();
  EnableSomeECDHECiphers();
  rv = SSL_OptionSet(server_->ssl_fd(), SSL_REUSE_SERVER_ECDHE_KEY, PR_FALSE);
  ASSERT_EQ(SECSuccess, rv);
  TlsInspectorRecordHandshakeMessage* i2 =
      new TlsInspectorRecordHandshakeMessage(kTlsHandshakeServerKeyExchange);
  server_->SetPacketFilter(i2);
  ConfigureSessionCache(RESUME_NONE, RESUME_NONE);
  Connect();
  client_->CheckKEAType(ssl_kea_ecdh);

  TlsServerKeyExchangeECDHE dhe2;
  ASSERT_TRUE(dhe2.Parse(i2->buffer()));

  // Make sure they are different.
  ASSERT_FALSE((dhe1.public_key_.len() == dhe2.public_key_.len()) &&
               (!memcmp(dhe1.public_key_.data(), dhe2.public_key_.data(),
                        dhe1.public_key_.len())));
}

INSTANTIATE_TEST_CASE_P(VariantsStream10, TlsConnectGeneric,
                        ::testing::Combine(
                          TlsConnectTestBase::kTlsModesStream,
                          TlsConnectTestBase::kTlsV10));
INSTANTIATE_TEST_CASE_P(VariantsAll, TlsConnectGeneric,
                        ::testing::Combine(
                          TlsConnectTestBase::kTlsModesAll,
                          TlsConnectTestBase::kTlsV11V12));
INSTANTIATE_TEST_CASE_P(VersionsDatagram, TlsConnectDatagram,
                        TlsConnectTestBase::kTlsV11V12);
INSTANTIATE_TEST_CASE_P(VersionsDatagram, TlsConnectStream,
                        TlsConnectTestBase::kTlsV11V12);

}  // namespace nspr_test
