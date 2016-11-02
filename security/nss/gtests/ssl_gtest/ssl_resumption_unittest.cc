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
  ConfigureSessionCache(RESUME_SESSIONID, RESUME_SESSIONID);
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_1);
  SetExpectedVersion(SSL_LIBRARY_VERSION_TLS_1_1);
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
  ConfigureSessionCache(RESUME_NONE, RESUME_NONE);

  server_->SetSniCallback(SwitchCertificates);

  Connect();
  ScopedCERTCertificate cert2(SSL_PeerCertificate(client_->ssl_fd()));
  CheckKeys();
  EXPECT_FALSE(SECITEM_ItemsAreEqual(&cert1->derCert, &cert2->derCert));
}

TEST_P(TlsConnectGeneric, ServerSNICertTypeSwitch) {
  Reset(TlsAgent::kServerEcdsa256);
  Connect();
  ScopedCERTCertificate cert1(SSL_PeerCertificate(client_->ssl_fd()));

  Reset();
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
  CheckKeys();
  TlsServerKeyExchangeEcdhe dhe1;
  EXPECT_TRUE(dhe1.Parse(i1->buffer()));

  // Restart
  Reset();
  TlsInspectorRecordHandshakeMessage* i2 =
      new TlsInspectorRecordHandshakeMessage(kTlsHandshakeServerKeyExchange);
  server_->SetPacketFilter(i2);
  ConfigureSessionCache(RESUME_NONE, RESUME_NONE);
  Connect();
  CheckKeys();

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
  CheckKeys();
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
  CheckKeys();

  TlsServerKeyExchangeEcdhe dhe2;
  EXPECT_TRUE(dhe2.Parse(i2->buffer()));

  // Make sure they are different.
  EXPECT_FALSE((dhe1.public_key_.len() == dhe2.public_key_.len()) &&
               (!memcmp(dhe1.public_key_.data(), dhe2.public_key_.data(),
                        dhe1.public_key_.len())));
}

// Verify that TLS 1.3 reports an accurate group on resumption.
TEST_P(TlsConnectTls13, TestTls13ResumeDifferentGroup) {
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  Connect();
  SendReceive();  // Need to read so that we absorb the session ticket.
  CheckKeys();

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  ExpectResumption(RESUME_TICKET);
  client_->ConfigNamedGroups(kFFDHEGroups);
  server_->ConfigNamedGroups(kFFDHEGroups);
  Connect();
  CheckKeys(ssl_kea_dh, ssl_grp_ffdhe_2048, ssl_auth_rsa_sign, ssl_sig_none);
}

// We need to enable different cipher suites at different times in the following
// tests.  Those cipher suites need to be suited to the version.
static uint16_t ChooseOneCipher(uint16_t version) {
  if (version >= SSL_LIBRARY_VERSION_TLS_1_3) {
    return TLS_AES_128_GCM_SHA256;
  }
  return TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA;
}

static uint16_t ChooseAnotherCipher(uint16_t version) {
  if (version >= SSL_LIBRARY_VERSION_TLS_1_3) {
    return TLS_CHACHA20_POLY1305_SHA256;
  }
  return TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA;
}

// Test that we don't resume when we can't negotiate the same cipher.
TEST_P(TlsConnectGeneric, TestResumeClientDifferentCipher) {
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  client_->EnableSingleCipher(ChooseOneCipher(version_));
  Connect();
  SendReceive();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign);

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  ExpectResumption(RESUME_NONE);
  client_->EnableSingleCipher(ChooseAnotherCipher(version_));
  uint16_t ticket_extension;
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    ticket_extension = ssl_tls13_pre_shared_key_xtn;
  } else {
    ticket_extension = ssl_session_ticket_xtn;
  }
  auto ticket_capture = new TlsExtensionCapture(ticket_extension);
  client_->SetPacketFilter(ticket_capture);
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign);
  EXPECT_EQ(0U, ticket_capture->extension().len());
}

// Test that we don't resume when we can't negotiate the same cipher.
TEST_P(TlsConnectGeneric, TestResumeServerDifferentCipher) {
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  server_->EnableSingleCipher(ChooseOneCipher(version_));
  Connect();
  SendReceive();  // Need to read so that we absorb the session ticket.
  CheckKeys();

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  ExpectResumption(RESUME_NONE);
  server_->EnableSingleCipher(ChooseAnotherCipher(version_));
  Connect();
  CheckKeys();
}

class SelectedCipherSuiteReplacer : public TlsHandshakeFilter {
 public:
  SelectedCipherSuiteReplacer(uint16_t suite) : cipher_suite_(suite) {}

 protected:
  PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                       const DataBuffer& input,
                                       DataBuffer* output) override {
    if (header.handshake_type() != kTlsHandshakeServerHello) {
      return KEEP;
    }

    *output = input;
    uint32_t temp = 0;
    EXPECT_TRUE(input.Read(0, 2, &temp));
    // Cipher suite is after version(2) and random(32).
    size_t pos = 34;
    if (temp < SSL_LIBRARY_VERSION_TLS_1_3) {
      // In old versions, we have to skip a session_id too.
      EXPECT_TRUE(input.Read(pos, 1, &temp));
      pos += 1 + temp;
    }
    output->Write(pos, static_cast<uint32_t>(cipher_suite_), 2);
    return CHANGE;
  }

 private:
  uint16_t cipher_suite_;
};

// Test that the client doesn't tolerate the server picking a different cipher
// suite for resumption.
TEST_P(TlsConnectStream, TestResumptionOverrideCipher) {
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  server_->EnableSingleCipher(ChooseOneCipher(version_));
  Connect();
  SendReceive();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign);

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  server_->SetPacketFilter(
      new SelectedCipherSuiteReplacer(ChooseAnotherCipher(version_)));

  ConnectExpectFail();
  client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_SERVER_HELLO);
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    // The reason this test is stream only: the server is unable to decrypt
    // the alert that the client sends, see bug 1304603.
    server_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
  } else {
    server_->CheckErrorCode(SSL_ERROR_HANDSHAKE_FAILURE_ALERT);
  }
}

class SelectedVersionReplacer : public TlsHandshakeFilter {
 public:
  SelectedVersionReplacer(uint16_t version) : version_(version) {}

 protected:
  PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                       const DataBuffer& input,
                                       DataBuffer* output) override {
    if (header.handshake_type() != kTlsHandshakeServerHello) {
      return KEEP;
    }

    *output = input;
    output->Write(0, static_cast<uint32_t>(version_), 2);
    return CHANGE;
  }

 private:
  uint16_t version_;
};

// Test how the client handles the case where the server picks a
// lower version number on resumption.
TEST_P(TlsConnectGenericPre13, TestResumptionOverrideVersion) {
  uint16_t override_version = 0;
  if (mode_ == STREAM) {
    switch (version_) {
      case SSL_LIBRARY_VERSION_TLS_1_0:
        return;  // Skip the test.
      case SSL_LIBRARY_VERSION_TLS_1_1:
        override_version = SSL_LIBRARY_VERSION_TLS_1_0;
        break;
      case SSL_LIBRARY_VERSION_TLS_1_2:
        override_version = SSL_LIBRARY_VERSION_TLS_1_1;
        break;
      default:
        ASSERT_TRUE(false) << "unknown version";
    }
  } else {
    if (version_ == SSL_LIBRARY_VERSION_TLS_1_2) {
      override_version = SSL_LIBRARY_VERSION_DTLS_1_0_WIRE;
    } else {
      ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_1, version_);
      return;  // Skip the test.
    }
  }

  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  // Need to use a cipher that is plausible for the lower version.
  server_->EnableSingleCipher(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA);
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign);

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  // Enable the lower version on the client.
  client_->SetVersionRange(version_ - 1, version_);
  server_->EnableSingleCipher(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA);
  server_->SetPacketFilter(new SelectedVersionReplacer(override_version));

  ConnectExpectFail();
  client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_SERVER_HELLO);
  server_->CheckErrorCode(SSL_ERROR_HANDSHAKE_FAILURE_ALERT);
}

// Test that two TLS resumptions work and produce the same ticket.
// This will change after bug 1257047 is fixed.
TEST_F(TlsConnectTest, TestTls13ResumptionTwice) {
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);

  Connect();
  SendReceive();  // Need to read so that we absorb the session ticket.
  CheckKeys();
  uint16_t original_suite;
  EXPECT_TRUE(client_->cipher_suite(&original_suite));

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  ExpectResumption(RESUME_TICKET);
  TlsExtensionCapture* c1 =
      new TlsExtensionCapture(ssl_tls13_pre_shared_key_xtn);
  client_->SetPacketFilter(c1);
  Connect();
  SendReceive();
  CheckKeys(ssl_kea_ecdh, ssl_grp_ec_curve25519, ssl_auth_rsa_sign,
            ssl_sig_none);
  // The filter will go away when we reset, so save the captured extension.
  DataBuffer initialTicket(c1->extension());
  ASSERT_LT(0U, initialTicket.len());

  ScopedCERTCertificate cert1(SSL_PeerCertificate(client_->ssl_fd()));
  ASSERT_TRUE(!!cert1.get());

  Reset();
  ClearStats();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  TlsExtensionCapture* c2 =
      new TlsExtensionCapture(ssl_tls13_pre_shared_key_xtn);
  client_->SetPacketFilter(c2);
  ExpectResumption(RESUME_TICKET);
  Connect();
  SendReceive();
  CheckKeys(ssl_kea_ecdh, ssl_grp_ec_curve25519, ssl_auth_rsa_sign,
            ssl_sig_none);
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

  ASSERT_NE(initialTicket, c2->extension());
}

}  // namespace nss_test
