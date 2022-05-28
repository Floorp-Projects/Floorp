/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "pk11pub.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"

extern "C" {
// This is not something that should make you happy.
#include "libssl_internals.h"
}

#include "gtest_utils.h"
#include "tls_connect.h"
#include "tls_filter.h"

namespace nss_test {

// Replaces the client hello with an SSLv2 version once.
class SSLv2ClientHelloFilter : public PacketFilter {
 public:
  SSLv2ClientHelloFilter(const std::shared_ptr<TlsAgent>& client,
                         uint16_t version)
      : replaced_(false),
        client_(client),
        version_(version),
        pad_len_(0),
        reported_pad_len_(0),
        client_random_len_(16),
        ciphers_(0),
        send_escape_(false) {}

  void SetVersion(uint16_t version) { version_ = version; }

  void SetCipherSuites(const std::vector<uint16_t>& ciphers) {
    ciphers_ = ciphers;
  }

  // Set a padding length and announce it correctly.
  void SetPadding(uint8_t pad_len) { SetPadding(pad_len, pad_len); }

  // Set a padding length and allow to lie about its length.
  void SetPadding(uint8_t pad_len, uint8_t reported_pad_len) {
    pad_len_ = pad_len;
    reported_pad_len_ = reported_pad_len;
  }

  void SetClientRandomLength(uint16_t client_random_len) {
    client_random_len_ = client_random_len;
  }

  void SetSendEscape(bool send_escape) { send_escape_ = send_escape; }

 protected:
  virtual PacketFilter::Action Filter(const DataBuffer& input,
                                      DataBuffer* output) {
    if (replaced_) {
      return KEEP;
    }

    // Replace only the very first packet.
    replaced_ = true;

    // The SSLv2 client hello size.
    size_t packet_len = SSL_HL_CLIENT_HELLO_HBYTES + (ciphers_.size() * 3) +
                        client_random_len_ + pad_len_;

    size_t idx = 0;
    *output = input;
    output->Allocate(packet_len);
    output->Truncate(packet_len);

    // Write record length.
    if (pad_len_ > 0) {
      size_t masked_len = 0x3fff & packet_len;
      if (send_escape_) {
        masked_len |= 0x4000;
      }

      idx = output->Write(idx, masked_len, 2);
      idx = output->Write(idx, reported_pad_len_, 1);
    } else {
      PR_ASSERT(!send_escape_);
      idx = output->Write(idx, 0x8000 | packet_len, 2);
    }

    // Remember header length.
    size_t hdr_len = idx;

    // Write client hello.
    idx = output->Write(idx, SSL_MT_CLIENT_HELLO, 1);
    idx = output->Write(idx, version_, 2);

    // Cipher list length.
    idx = output->Write(idx, (ciphers_.size() * 3), 2);

    // Session ID length.
    idx = output->Write(idx, static_cast<uint32_t>(0), 2);

    // ClientRandom length.
    idx = output->Write(idx, client_random_len_, 2);

    // Cipher suites.
    for (auto cipher : ciphers_) {
      idx = output->Write(idx, static_cast<uint32_t>(cipher), 3);
    }

    // Challenge.
    std::vector<uint8_t> challenge(client_random_len_);
    PK11_GenerateRandom(challenge.data(), challenge.size());
    idx = output->Write(idx, challenge.data(), challenge.size());

    // Add padding if any.
    if (pad_len_ > 0) {
      std::vector<uint8_t> pad(pad_len_);
      idx = output->Write(idx, pad.data(), pad.size());
    }

    // Update the client random so that the handshake succeeds.
    SECStatus rv = SSLInt_UpdateSSLv2ClientRandom(
        client_.lock()->ssl_fd(), challenge.data(), challenge.size(),
        output->data() + hdr_len, output->len() - hdr_len);
    EXPECT_EQ(SECSuccess, rv);

    return CHANGE;
  }

 private:
  bool replaced_;
  std::weak_ptr<TlsAgent> client_;
  uint16_t version_;
  uint8_t pad_len_;
  uint8_t reported_pad_len_;
  uint16_t client_random_len_;
  std::vector<uint16_t> ciphers_;
  bool send_escape_;
};

class SSLv2ClientHelloTestF : public TlsConnectTestBase {
 public:
  SSLv2ClientHelloTestF()
      : TlsConnectTestBase(ssl_variant_stream, 0), filter_(nullptr) {}

  SSLv2ClientHelloTestF(SSLProtocolVariant variant, uint16_t version)
      : TlsConnectTestBase(variant, version), filter_(nullptr) {}

  void SetUp() override {
    TlsConnectTestBase::SetUp();
    filter_ = MakeTlsFilter<SSLv2ClientHelloFilter>(client_, version_);
    server_->SetOption(SSL_ENABLE_V2_COMPATIBLE_HELLO, PR_TRUE);
  }

  void SetExpectedVersion(uint16_t version) {
    TlsConnectTestBase::SetExpectedVersion(version);
    filter_->SetVersion(version);
  }

  void SetAvailableCipherSuite(uint16_t cipher) {
    filter_->SetCipherSuites(std::vector<uint16_t>(1, cipher));
  }

  void SetAvailableCipherSuites(const std::vector<uint16_t>& ciphers) {
    filter_->SetCipherSuites(ciphers);
  }

  void SetPadding(uint8_t pad_len) { filter_->SetPadding(pad_len); }

  void SetPadding(uint8_t pad_len, uint8_t reported_pad_len) {
    filter_->SetPadding(pad_len, reported_pad_len);
  }

  void SetClientRandomLength(uint16_t client_random_len) {
    filter_->SetClientRandomLength(client_random_len);
  }

  void SetSendEscape(bool send_escape) { filter_->SetSendEscape(send_escape); }

 private:
  std::shared_ptr<SSLv2ClientHelloFilter> filter_;
};

// Parameterized version of SSLv2ClientHelloTestF we can
// use with TEST_P to test multiple TLS versions easily.
class SSLv2ClientHelloTest : public SSLv2ClientHelloTestF,
                             public ::testing::WithParamInterface<uint16_t> {
 public:
  SSLv2ClientHelloTest()
      : SSLv2ClientHelloTestF(ssl_variant_stream, GetParam()) {}
};

// Test negotiating TLS 1.0 - 1.2.
TEST_P(SSLv2ClientHelloTest, Connect) {
  SetAvailableCipherSuite(TLS_DHE_RSA_WITH_AES_128_CBC_SHA);
  Connect();
}

TEST_P(SSLv2ClientHelloTest, ConnectDisabled) {
  server_->SetOption(SSL_ENABLE_V2_COMPATIBLE_HELLO, PR_FALSE);
  SetAvailableCipherSuite(TLS_DHE_RSA_WITH_AES_128_CBC_SHA);

  StartConnect();
  client_->Handshake();  // Send the modified ClientHello.
  server_->Handshake();  // Read some.
  // The problem here is that the v2 ClientHello puts the version where the v3
  // ClientHello puts a version number.  So the version number (0x0301+) appears
  // to be a length and server blocks waiting for that much data.
  EXPECT_EQ(PR_WOULD_BLOCK_ERROR, PORT_GetError());

  // This is usually what happens with v2-compatible: the server hangs.
  // But to be certain, feed in more data to see if an error comes out.
  uint8_t zeros[SSL_LIBRARY_VERSION_TLS_1_2] = {0};
  client_->SendDirect(DataBuffer(zeros, sizeof(zeros)));
  ExpectAlert(server_, kTlsAlertUnexpectedMessage);
  server_->Handshake();
  client_->Handshake();
}

// Sending a v2 ClientHello after a no-op v3 record must fail.
TEST_P(SSLv2ClientHelloTest, ConnectAfterEmptyV3Record) {
  DataBuffer buffer;

  size_t idx = 0;
  idx = buffer.Write(idx, 0x16, 1);    // handshake
  idx = buffer.Write(idx, 0x0301, 2);  // record_version
  (void)buffer.Write(idx, 0U, 2);      // length=0

  SetAvailableCipherSuite(TLS_DHE_RSA_WITH_AES_128_CBC_SHA);
  EnsureTlsSetup();
  client_->SendDirect(buffer);

  // Need padding so the connection doesn't just time out. With a v2
  // ClientHello parsed as a v3 record we will use the record version
  // as the record length.
  SetPadding(255);

  ConnectExpectAlert(server_, kTlsAlertUnexpectedMessage);
  EXPECT_EQ(SSL_ERROR_RX_UNKNOWN_RECORD_TYPE, server_->error_code());
}

// Test negotiating TLS 1.3.
TEST_F(SSLv2ClientHelloTestF, Connect13) {
  EnsureTlsSetup();
  SetExpectedVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);

  std::vector<uint16_t> cipher_suites = {TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256};
  SetAvailableCipherSuites(cipher_suites);

  ConnectExpectAlert(server_, kTlsAlertIllegalParameter);
  EXPECT_EQ(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO, server_->error_code());
}

// Test negotiating an EC suite.
TEST_P(SSLv2ClientHelloTest, NegotiateECSuite) {
  SetAvailableCipherSuite(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA);
  Connect();
}

// Test negotiating TLS 1.0 - 1.2 with a padded client hello.
TEST_P(SSLv2ClientHelloTest, AddPadding) {
  SetAvailableCipherSuite(TLS_DHE_RSA_WITH_AES_128_CBC_SHA);
  SetPadding(255);
  Connect();
}

// Test that sending a security escape fails the handshake.
TEST_P(SSLv2ClientHelloTest, SendSecurityEscape) {
  SetAvailableCipherSuite(TLS_DHE_RSA_WITH_AES_128_CBC_SHA);

  // Send a security escape.
  SetSendEscape(true);

  // Set a big padding so that the server fails instead of timing out.
  SetPadding(255);

  ConnectExpectAlert(server_, kTlsAlertUnexpectedMessage);
}

// Invalid SSLv2 client hello padding must fail the handshake.
TEST_P(SSLv2ClientHelloTest, AddErroneousPadding) {
  SetAvailableCipherSuite(TLS_DHE_RSA_WITH_AES_128_CBC_SHA);

  // Append 5 bytes of padding but say it's only 4.
  SetPadding(5, 4);

  ConnectExpectAlert(server_, kTlsAlertIllegalParameter);
  EXPECT_EQ(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO, server_->error_code());
}

// Invalid SSLv2 client hello padding must fail the handshake.
TEST_P(SSLv2ClientHelloTest, AddErroneousPadding2) {
  SetAvailableCipherSuite(TLS_DHE_RSA_WITH_AES_128_CBC_SHA);

  // Append 5 bytes of padding but say it's 6.
  SetPadding(5, 6);

  ConnectExpectAlert(server_, kTlsAlertIllegalParameter);
  EXPECT_EQ(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO, server_->error_code());
}

// Wrong amount of bytes for the ClientRandom must fail the handshake.
TEST_P(SSLv2ClientHelloTest, SmallClientRandom) {
  SetAvailableCipherSuite(TLS_DHE_RSA_WITH_AES_128_CBC_SHA);

  // Send a ClientRandom that's too small.
  SetClientRandomLength(15);

  ConnectExpectAlert(server_, kTlsAlertIllegalParameter);
  EXPECT_EQ(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO, server_->error_code());
}

// Test sending the maximum accepted number of ClientRandom bytes.
TEST_P(SSLv2ClientHelloTest, MaxClientRandom) {
  SetAvailableCipherSuite(TLS_DHE_RSA_WITH_AES_128_CBC_SHA);
  SetClientRandomLength(32);
  Connect();
}

// Wrong amount of bytes for the ClientRandom must fail the handshake.
TEST_P(SSLv2ClientHelloTest, BigClientRandom) {
  SetAvailableCipherSuite(TLS_DHE_RSA_WITH_AES_128_CBC_SHA);

  // Send a ClientRandom that's too big.
  SetClientRandomLength(33);

  ConnectExpectAlert(server_, kTlsAlertIllegalParameter);
  EXPECT_EQ(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO, server_->error_code());
}

// Connection must fail if we require safe renegotiation but the client doesn't
// include TLS_EMPTY_RENEGOTIATION_INFO_SCSV in the list of cipher suites.
TEST_P(SSLv2ClientHelloTest, RequireSafeRenegotiation) {
  server_->SetOption(SSL_REQUIRE_SAFE_NEGOTIATION, PR_TRUE);
  SetAvailableCipherSuite(TLS_DHE_RSA_WITH_AES_128_CBC_SHA);
  ConnectExpectAlert(server_, kTlsAlertHandshakeFailure);
  EXPECT_EQ(SSL_ERROR_UNSAFE_NEGOTIATION, server_->error_code());
}

// Connection must succeed when requiring safe renegotiation and the client
// includes TLS_EMPTY_RENEGOTIATION_INFO_SCSV in the list of cipher suites.
TEST_P(SSLv2ClientHelloTest, RequireSafeRenegotiationWithSCSV) {
  server_->SetOption(SSL_REQUIRE_SAFE_NEGOTIATION, PR_TRUE);
  std::vector<uint16_t> cipher_suites = {TLS_DHE_RSA_WITH_AES_128_CBC_SHA,
                                         TLS_EMPTY_RENEGOTIATION_INFO_SCSV};
  SetAvailableCipherSuites(cipher_suites);
  Connect();
}

TEST_P(SSLv2ClientHelloTest, CheckServerRandom) {
  ConfigureSessionCache(RESUME_NONE, RESUME_NONE);
  SetAvailableCipherSuite(TLS_DHE_RSA_WITH_AES_128_CBC_SHA);

  static const size_t random_len = 32;
  uint8_t srandom1[random_len];
  uint8_t z[random_len] = {0};

  auto sh = MakeTlsFilter<TlsHandshakeRecorder>(server_, ssl_hs_server_hello);
  Connect();
  ASSERT_TRUE(sh->buffer().len() > (random_len + 2));
  memcpy(srandom1, sh->buffer().data() + 2, random_len);
  EXPECT_NE(0, memcmp(srandom1, z, random_len));

  Reset();
  sh = MakeTlsFilter<TlsHandshakeRecorder>(server_, ssl_hs_server_hello);
  Connect();
  ASSERT_TRUE(sh->buffer().len() > (random_len + 2));
  const uint8_t* srandom2 = sh->buffer().data() + 2;

  EXPECT_NE(0, memcmp(srandom2, z, random_len));
  EXPECT_NE(0, memcmp(srandom1, srandom2, random_len));
}

// Connect to the server with TLS 1.1, signalling that this is a fallback from
// a higher version. As the server doesn't support anything higher than TLS 1.1
// it must accept the connection.
TEST_F(SSLv2ClientHelloTestF, FallbackSCSV) {
  EnsureTlsSetup();
  SetExpectedVersion(SSL_LIBRARY_VERSION_TLS_1_1);
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_1);

  std::vector<uint16_t> cipher_suites = {TLS_DHE_RSA_WITH_AES_128_CBC_SHA,
                                         TLS_FALLBACK_SCSV};
  SetAvailableCipherSuites(cipher_suites);
  Connect();
}

// Connect to the server with TLS 1.1, signalling that this is a fallback from
// a higher version. As the server supports TLS 1.2 though it must reject the
// connection due to a possible downgrade attack.
TEST_F(SSLv2ClientHelloTestF, InappropriateFallbackSCSV) {
  SetExpectedVersion(SSL_LIBRARY_VERSION_TLS_1_1);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_1);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_2);

  std::vector<uint16_t> cipher_suites = {TLS_DHE_RSA_WITH_AES_128_CBC_SHA,
                                         TLS_FALLBACK_SCSV};
  SetAvailableCipherSuites(cipher_suites);

  ConnectExpectAlert(server_, kTlsAlertInappropriateFallback);
  EXPECT_EQ(SSL_ERROR_INAPPROPRIATE_FALLBACK_ALERT, server_->error_code());
}

INSTANTIATE_TEST_SUITE_P(VersionsStream10Pre13, SSLv2ClientHelloTest,
                         TlsConnectTestBase::kTlsV10);
INSTANTIATE_TEST_SUITE_P(VersionsStreamPre13, SSLv2ClientHelloTest,
                         TlsConnectTestBase::kTlsV11V12);

}  // namespace nss_test
