/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secerr.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"

#include "scoped_ptrs.h"
#include "tls_parser.h"
#include "tls_filter.h"
#include "tls_connect.h"
#include "gtest_utils.h"

namespace nss_test {

// Set the version number in the ClientHello.
class TlsInspectorClientHelloVersionSetter : public TlsHandshakeFilter {
 public:
  TlsInspectorClientHelloVersionSetter(uint16_t version) : version_(version) {}

  virtual PacketFilter::Action FilterHandshake(
      const HandshakeHeader& header,
      const DataBuffer& input, DataBuffer* output) {
    if (header.handshake_type() == kTlsHandshakeClientHello) {
      *output = input;
      output->Write(0, version_, 2);
      return CHANGE;
    }
    return KEEP;
  }

 private:
  uint16_t version_;
};

TEST_P(TlsConnectStream, ServerNegotiateTls10) {
  uint16_t minver, maxver;
  client_->GetVersionRange(&minver, &maxver);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_0,
                           maxver);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_0,
                           SSL_LIBRARY_VERSION_TLS_1_0);
  Connect();
}

TEST_P(TlsConnectGeneric, ServerNegotiateTls11) {
  if (version_ < SSL_LIBRARY_VERSION_TLS_1_1)
    return;

  uint16_t minver, maxver;
  client_->GetVersionRange(&minver, &maxver);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           maxver);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_1);
  Connect();
}

TEST_P(TlsConnectGeneric, ServerNegotiateTls12) {
  if (version_ < SSL_LIBRARY_VERSION_TLS_1_2)
    return;

  uint16_t minver, maxver;
  client_->GetVersionRange(&minver, &maxver);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           maxver);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  Connect();
}

// Test the ServerRandom version hack from
// [draft-ietf-tls-tls13-11 Section 6.3.1.1].
// The first three tests test for active tampering. The next
// two validate that we can also detect fallback using the
// SSL_SetDowngradeCheckVersion() API.
TEST_F(TlsConnectTest, TestDowngradeDetectionToTls11) {
  client_->SetPacketFilter(new TlsInspectorClientHelloVersionSetter
                           (SSL_LIBRARY_VERSION_TLS_1_1));
  ConnectExpectFail();
  ASSERT_EQ(SSL_ERROR_RX_MALFORMED_SERVER_HELLO, client_->error_code());
}

/* Attempt to negotiate the bogus DTLS 1.1 version. */
TEST_F(DtlsConnectTest, TestDtlsVersion11) {
  client_->SetPacketFilter(new TlsInspectorClientHelloVersionSetter(
      ((~0x0101) & 0xffff)));
  ConnectExpectFail();
  // It's kind of surprising that SSL_ERROR_NO_CYPHER_OVERLAP is
  // what is returned here, but this is deliberate in ssl3_HandleAlert().
  EXPECT_EQ(SSL_ERROR_NO_CYPHER_OVERLAP, client_->error_code());
  EXPECT_EQ(SSL_ERROR_UNSUPPORTED_VERSION, server_->error_code());
}

#ifdef NSS_ENABLE_TLS_1_3
TEST_F(TlsConnectTest, TestDowngradeDetectionToTls12) {
  EnsureTlsSetup();
  client_->SetPacketFilter(new TlsInspectorClientHelloVersionSetter
                           (SSL_LIBRARY_VERSION_TLS_1_2));
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  ConnectExpectFail();
  ASSERT_EQ(SSL_ERROR_RX_MALFORMED_SERVER_HELLO, client_->error_code());
}
#endif

// TLS 1.1 clients do not check the random values, so we should
// instead get a handshake failure alert from the server.
TEST_F(TlsConnectTest, TestDowngradeDetectionToTls10) {
  client_->SetPacketFilter(new TlsInspectorClientHelloVersionSetter
                          (SSL_LIBRARY_VERSION_TLS_1_0));
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_0,
                           SSL_LIBRARY_VERSION_TLS_1_1);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_0,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  ConnectExpectFail();
  ASSERT_EQ(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE, server_->error_code());
  ASSERT_EQ(SSL_ERROR_DECRYPT_ERROR_ALERT, client_->error_code());
}

TEST_F(TlsConnectTest, TestFallbackFromTls12) {
  EnsureTlsSetup();
  client_->SetDowngradeCheckVersion(SSL_LIBRARY_VERSION_TLS_1_2);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_1);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  ConnectExpectFail();
  ASSERT_EQ(SSL_ERROR_RX_MALFORMED_SERVER_HELLO, client_->error_code());
}

#ifdef NSS_ENABLE_TLS_1_3
TEST_F(TlsConnectTest, TestFallbackFromTls13) {
  EnsureTlsSetup();
  client_->SetDowngradeCheckVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  ConnectExpectFail();
  ASSERT_EQ(SSL_ERROR_RX_MALFORMED_SERVER_HELLO, client_->error_code());
}

// The TLS v1.3 spec section C.4 states that 'Implementations MUST NOT send or
// accept any records with a version less than { 3, 0 }'. Thus we will not
// allow version ranges including both SSL v3 and TLS v1.3.
TEST_F(TlsConnectTest, DisallowSSLv3HelloWithTLSv13Enabled) {
  SECStatus rv;
  SSLVersionRange vrange = { SSL_LIBRARY_VERSION_3_0,
                             SSL_LIBRARY_VERSION_TLS_1_3 };

  EnsureTlsSetup();
  rv = SSL_VersionRangeSet(client_->ssl_fd(), &vrange);
  EXPECT_EQ(SECFailure, rv);

  rv = SSL_VersionRangeSet(server_->ssl_fd(), &vrange);
  EXPECT_EQ(SECFailure, rv);
}
#endif // NSS_ENABLE_TLS_1_3

} // namespace nss_test
