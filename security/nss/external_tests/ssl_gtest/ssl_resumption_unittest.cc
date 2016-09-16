/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ssl.h"
#include <functional>
#include <memory>
#include "secerr.h"
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

class TlsServerKeyExchangeEcdhe {
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

TEST_P(TlsConnectGenericPre13, ConnectResumed) {
  ConfigureSessionCache(RESUME_SESSIONID, RESUME_SESSIONID);
  Connect();

  Reset();
  ExpectResumption(RESUME_SESSIONID);
  Connect();
}

TEST_P(TlsConnectGeneric, ConnectClientCacheDisabled) {
  ConfigureSessionCache(RESUME_NONE, RESUME_SESSIONID);
  Connect();
  SendReceive();

  Reset();
  ExpectResumption(RESUME_NONE);
  Connect();
  SendReceive();
}

TEST_P(TlsConnectGeneric, ConnectServerCacheDisabled) {
  ConfigureSessionCache(RESUME_SESSIONID, RESUME_NONE);
  Connect();
  SendReceive();

  Reset();
  ExpectResumption(RESUME_NONE);
  Connect();
  SendReceive();
}

TEST_P(TlsConnectGeneric, ConnectSessionCacheDisabled) {
  ConfigureSessionCache(RESUME_NONE, RESUME_NONE);
  Connect();
  SendReceive();

  Reset();
  ExpectResumption(RESUME_NONE);
  Connect();
  SendReceive();
}

TEST_P(TlsConnectGeneric, ConnectResumeSupportBoth) {
  // This prefers tickets.
  ConfigureSessionCache(RESUME_BOTH, RESUME_BOTH);
  Connect();
  SendReceive();

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_BOTH);
  ExpectResumption(RESUME_TICKET);
  Connect();
  SendReceive();
}

TEST_P(TlsConnectGeneric, ConnectResumeClientTicketServerBoth) {
  // This causes no resumption because the client needs the
  // session cache to resume even with tickets.
  ConfigureSessionCache(RESUME_TICKET, RESUME_BOTH);
  Connect();
  SendReceive();

  Reset();
  ConfigureSessionCache(RESUME_TICKET, RESUME_BOTH);
  ExpectResumption(RESUME_NONE);
  Connect();
  SendReceive();
}

TEST_P(TlsConnectGeneric, ConnectResumeClientBothTicketServerTicket) {
  // This causes a ticket resumption.
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  Connect();
  SendReceive();

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  ExpectResumption(RESUME_TICKET);
  Connect();
  SendReceive();
}

TEST_P(TlsConnectGeneric, ConnectResumeClientServerTicketOnly) {
  // This causes no resumption because the client needs the
  // session cache to resume even with tickets.
  ConfigureSessionCache(RESUME_TICKET, RESUME_TICKET);
  Connect();
  SendReceive();

  Reset();
  ConfigureSessionCache(RESUME_TICKET, RESUME_TICKET);
  ExpectResumption(RESUME_NONE);
  Connect();
  SendReceive();
}

TEST_P(TlsConnectGeneric, ConnectResumeClientBothServerNone) {
  ConfigureSessionCache(RESUME_BOTH, RESUME_NONE);
  Connect();
  SendReceive();

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_NONE);
  ExpectResumption(RESUME_NONE);
  Connect();
  SendReceive();
}

TEST_P(TlsConnectGeneric, ConnectResumeClientNoneServerBoth) {
  ConfigureSessionCache(RESUME_NONE, RESUME_BOTH);
  Connect();
  SendReceive();

  Reset();
  ConfigureSessionCache(RESUME_NONE, RESUME_BOTH);
  ExpectResumption(RESUME_NONE);
  Connect();
  SendReceive();
}

TEST_P(TlsConnectGenericPre13, ConnectResumeWithHigherVersion) {
  EnsureTlsSetup();
  SetExpectedVersion(SSL_LIBRARY_VERSION_TLS_1_1);
  ConfigureSessionCache(RESUME_SESSIONID, RESUME_SESSIONID);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_1);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_1);
  Connect();

  Reset();
  EnsureTlsSetup();
  SetExpectedVersion(SSL_LIBRARY_VERSION_TLS_1_2);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  ExpectResumption(RESUME_NONE);
  Connect();
}

TEST_P(TlsConnectGeneric, ConnectResumeClientBothTicketServerTicketForget) {
  // This causes a ticket resumption.
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  Connect();
  SendReceive();

  Reset();
  ClearServerCache();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  ExpectResumption(RESUME_NONE);
  Connect();
  SendReceive();
}

// This callback switches out the "server" cert used on the server with
// the "client" certificate, which should be the same type.
static int32_t SwitchCertificates(TlsAgent* agent, const SECItem* srvNameArr,
                                  uint32_t srvNameArrSize) {
  bool ok = agent->ConfigServerCert("client");
  if (!ok) return SSL_SNI_SEND_ALERT;

  return 0;  // first config
};

TEST_P(TlsConnectGeneric, ServerSNICertSwitch) {
  Connect();
  ScopedCERTCertificate cert1(SSL_PeerCertificate(client_->ssl_fd()));

  Reset();
  EnsureTlsSetup();
  ConfigureSessionCache(RESUME_NONE, RESUME_NONE);

  server_->SetSniCallback(SwitchCertificates);

  Connect();
  ScopedCERTCertificate cert2(SSL_PeerCertificate(client_->ssl_fd()));
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign);
  EXPECT_FALSE(SECITEM_ItemsAreEqual(&cert1->derCert, &cert2->derCert));
}

TEST_P(TlsConnectGeneric, ServerSNICertTypeSwitch) {
  Reset(TlsAgent::kServerEcdsa256);
  Connect();
  ScopedCERTCertificate cert1(SSL_PeerCertificate(client_->ssl_fd()));

  Reset();
  EnsureTlsSetup();
  ConfigureSessionCache(RESUME_NONE, RESUME_NONE);

  // Because we configure an RSA certificate here, it only adds a second, unused
  // certificate, which has no effect on what the server uses.
  server_->SetSniCallback(SwitchCertificates);

  Connect();
  ScopedCERTCertificate cert2(SSL_PeerCertificate(client_->ssl_fd()));
  CheckKeys(ssl_kea_ecdh, ssl_auth_ecdsa);
  EXPECT_TRUE(SECITEM_ItemsAreEqual(&cert1->derCert, &cert2->derCert));
}

// Prior to TLS 1.3, we were not fully ephemeral; though 1.3 fixes that
TEST_P(TlsConnectGenericPre13, ConnectEcdheTwiceReuseKey) {
  TlsInspectorRecordHandshakeMessage* i1 =
      new TlsInspectorRecordHandshakeMessage(kTlsHandshakeServerKeyExchange);
  server_->SetPacketFilter(i1);
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign);
  TlsServerKeyExchangeEcdhe dhe1;
  EXPECT_TRUE(dhe1.Parse(i1->buffer()));

  // Restart
  Reset();
  TlsInspectorRecordHandshakeMessage* i2 =
      new TlsInspectorRecordHandshakeMessage(kTlsHandshakeServerKeyExchange);
  server_->SetPacketFilter(i2);
  ConfigureSessionCache(RESUME_NONE, RESUME_NONE);
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign);

  TlsServerKeyExchangeEcdhe dhe2;
  EXPECT_TRUE(dhe2.Parse(i2->buffer()));

  // Make sure they are the same.
  EXPECT_EQ(dhe1.public_key_.len(), dhe2.public_key_.len());
  EXPECT_TRUE(!memcmp(dhe1.public_key_.data(), dhe2.public_key_.data(),
                      dhe1.public_key_.len()));
}

// This test parses the ServerKeyExchange, which isn't in 1.3
TEST_P(TlsConnectGenericPre13, ConnectEcdheTwiceNewKey) {
  server_->EnsureTlsSetup();
  SECStatus rv =
      SSL_OptionSet(server_->ssl_fd(), SSL_REUSE_SERVER_ECDHE_KEY, PR_FALSE);
  EXPECT_EQ(SECSuccess, rv);
  TlsInspectorRecordHandshakeMessage* i1 =
      new TlsInspectorRecordHandshakeMessage(kTlsHandshakeServerKeyExchange);
  server_->SetPacketFilter(i1);
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign);
  TlsServerKeyExchangeEcdhe dhe1;
  EXPECT_TRUE(dhe1.Parse(i1->buffer()));

  // Restart
  Reset();
  server_->EnsureTlsSetup();
  rv = SSL_OptionSet(server_->ssl_fd(), SSL_REUSE_SERVER_ECDHE_KEY, PR_FALSE);
  EXPECT_EQ(SECSuccess, rv);
  TlsInspectorRecordHandshakeMessage* i2 =
      new TlsInspectorRecordHandshakeMessage(kTlsHandshakeServerKeyExchange);
  server_->SetPacketFilter(i2);
  ConfigureSessionCache(RESUME_NONE, RESUME_NONE);
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign);

  TlsServerKeyExchangeEcdhe dhe2;
  EXPECT_TRUE(dhe2.Parse(i2->buffer()));

  // Make sure they are different.
  EXPECT_FALSE((dhe1.public_key_.len() == dhe2.public_key_.len()) &&
               (!memcmp(dhe1.public_key_.data(), dhe2.public_key_.data(),
                        dhe1.public_key_.len())));
}

// Test that two TLS resumptions work and produce the same ticket.
// This will change after bug 1257047 is fixed.
TEST_F(TlsConnectTest, TestTls13ResumptionTwice) {
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  Connect();
  SendReceive();  // Need to read so that we absorb the session ticket.
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign);
  uint16_t original_suite;
  EXPECT_TRUE(client_->cipher_suite(&original_suite));

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  TlsExtensionCapture* c1 = new TlsExtensionCapture(kTlsExtensionPreSharedKey);
  client_->SetPacketFilter(c1);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  ExpectResumption(RESUME_TICKET);
  Connect();
  SendReceive();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign);
  // The filter will go away when we reset, so save the captured extension.
  DataBuffer initialTicket(c1->extension());
  ASSERT_LT(0U, initialTicket.len());

  ScopedCERTCertificate cert1(SSL_PeerCertificate(client_->ssl_fd()));
  ASSERT_TRUE(!!cert1.get());

  Reset();
  ClearStats();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  TlsExtensionCapture* c2 = new TlsExtensionCapture(kTlsExtensionPreSharedKey);
  client_->SetPacketFilter(c2);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  ExpectResumption(RESUME_TICKET);
  Connect();
  SendReceive();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign);
  ASSERT_LT(0U, c2->extension().len());

  ScopedCERTCertificate cert2(SSL_PeerCertificate(client_->ssl_fd()));
  ASSERT_TRUE(!!cert2.get());

  // Check that the cipher suite is reported the same on both sides, though in
  // TLS 1.3 resumption actually negotiates a different cipher suite.
  uint16_t resumed_suite;
  EXPECT_TRUE(server_->cipher_suite(&resumed_suite));
  EXPECT_EQ(original_suite, resumed_suite);
  EXPECT_TRUE(client_->cipher_suite(&resumed_suite));
  EXPECT_EQ(original_suite, resumed_suite);

  // TODO(ekr@rtfm.com): This will change when we fix bug 1257047.
  ASSERT_EQ(initialTicket, c2->extension());
}

TEST_F(TlsConnectTest, DisableClientPSKAndFailToResume) {
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  Connect();
  SendReceive();  // Need to read so that we absorb the session ticket.
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign);

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  TlsExtensionCapture* capture =
      new TlsExtensionCapture(kTlsExtensionPreSharedKey);
  client_->SetPacketFilter(capture);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  // We need to disable ALL PSK cipher suites with the same symmetric cipher and
  // PRF hash.  Otherwise the server will just use a different key exchange.
  client_->DisableAllCiphers();
  client_->EnableCiphersByAuthType(ssl_auth_rsa_sign);
  ExpectResumption(RESUME_NONE);
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign);
  EXPECT_EQ(0U, capture->extension().len());
}

TEST_F(TlsConnectTest, DisableServerPSKAndFailToResume) {
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  Connect();
  SendReceive();  // Need to read so that we absorb the session ticket.
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign);

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  TlsExtensionCapture* clientCapture =
      new TlsExtensionCapture(kTlsExtensionPreSharedKey);
  client_->SetPacketFilter(clientCapture);
  TlsExtensionCapture* serverCapture =
      new TlsExtensionCapture(kTlsExtensionPreSharedKey);
  server_->SetPacketFilter(serverCapture);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  // We need to disable ALL PSK cipher suites with the same symmetric cipher and
  // PRF hash.  Otherwise the server will just use a different key exchange.
  server_->DisableAllCiphers();
  server_->EnableCiphersByAuthType(ssl_auth_rsa_sign);
  ExpectResumption(RESUME_NONE);
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign);
  // The client should have the extension, but the server should not.
  EXPECT_LT(0U, clientCapture->extension().len());
  EXPECT_EQ(0U, serverCapture->extension().len());
}

}  // namespace nss_test
