/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secerr.h"
#include "ssl.h"
#include "ssl3prot.h"
#include "sslerr.h"
#include "sslproto.h"

#include "gtest_utils.h"
#include "nss_scoped_ptrs.h"
#include "tls_connect.h"
#include "tls_filter.h"
#include "tls_parser.h"

namespace nss_test {

TEST_P(TlsConnectStream, ServerNegotiateTls10) {
  uint16_t minver, maxver;
  client_->GetVersionRange(&minver, &maxver);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_0, maxver);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_0,
                           SSL_LIBRARY_VERSION_TLS_1_0);
  Connect();
}

TEST_P(TlsConnectGeneric, ServerNegotiateTls11) {
  if (version_ < SSL_LIBRARY_VERSION_TLS_1_1) return;

  uint16_t minver, maxver;
  client_->GetVersionRange(&minver, &maxver);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1, maxver);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_1);
  Connect();
}

TEST_P(TlsConnectGeneric, ServerNegotiateTls12) {
  if (version_ < SSL_LIBRARY_VERSION_TLS_1_2) return;

  uint16_t minver, maxver;
  client_->GetVersionRange(&minver, &maxver);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2, maxver);
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
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_0,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_0,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  client_->SetOption(SSL_ENABLE_HELLO_DOWNGRADE_CHECK, PR_TRUE);
  MakeTlsFilter<TlsClientHelloVersionSetter>(client_,
                                             SSL_LIBRARY_VERSION_TLS_1_1);
  ConnectExpectAlert(client_, kTlsAlertIllegalParameter);
  client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_SERVER_HELLO);
  server_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

// Attempt to negotiate the bogus DTLS 1.1 version.
TEST_F(DtlsConnectTest, TestDtlsVersion11) {
  MakeTlsFilter<TlsClientHelloVersionSetter>(client_, ((~0x0101) & 0xffff));
  ConnectExpectAlert(server_, kTlsAlertHandshakeFailure);
  // It's kind of surprising that SSL_ERROR_NO_CYPHER_OVERLAP is
  // what is returned here, but this is deliberate in ssl3_HandleAlert().
  client_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
  server_->CheckErrorCode(SSL_ERROR_UNSUPPORTED_VERSION);
}

TEST_F(TlsConnectTest, TestDowngradeDetectionToTls12) {
  client_->SetOption(SSL_ENABLE_HELLO_DOWNGRADE_CHECK, PR_TRUE);
  MakeTlsFilter<TlsExtensionDropper>(client_, ssl_tls13_supported_versions_xtn);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  ConnectExpectAlert(client_, kTlsAlertIllegalParameter);
  client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_SERVER_HELLO);
  server_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

// Disabling downgrade checks will be caught when the Finished MAC check fails.
TEST_F(TlsConnectTest, TestDisableDowngradeDetection) {
  client_->SetOption(SSL_ENABLE_HELLO_DOWNGRADE_CHECK, PR_FALSE);
  MakeTlsFilter<TlsExtensionDropper>(client_, ssl_tls13_supported_versions_xtn);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  ConnectExpectAlert(server_, kTlsAlertDecryptError);
  client_->CheckErrorCode(SSL_ERROR_DECRYPT_ERROR_ALERT);
  server_->CheckErrorCode(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE);
}

typedef std::tuple<SSLProtocolVariant,
                   uint16_t,  // client version
                   uint16_t>  // server version
    TlsDowngradeProfile;

class TlsDowngradeTest
    : public TlsConnectTestBase,
      public ::testing::WithParamInterface<TlsDowngradeProfile> {
 public:
  TlsDowngradeTest()
      : TlsConnectTestBase(std::get<0>(GetParam()), std::get<1>(GetParam())),
        c_ver(std::get<1>(GetParam())),
        s_ver(std::get<2>(GetParam())) {}

 protected:
  const uint16_t c_ver;
  const uint16_t s_ver;
};

TEST_P(TlsDowngradeTest, TlsDowngradeSentinelTest) {
  static const uint8_t tls12_downgrade_random[] = {0x44, 0x4F, 0x57, 0x4E,
                                                   0x47, 0x52, 0x44, 0x01};
  static const uint8_t tls1_downgrade_random[] = {0x44, 0x4F, 0x57, 0x4E,
                                                  0x47, 0x52, 0x44, 0x00};
  static const size_t kRandomLen = 32;

  if (c_ver > s_ver) {
    return;
  }

  client_->SetVersionRange(c_ver, c_ver);
  server_->SetVersionRange(c_ver, s_ver);

  auto sh = MakeTlsFilter<TlsHandshakeRecorder>(server_, ssl_hs_server_hello);
  Connect();
  ASSERT_TRUE(sh->buffer().len() > (kRandomLen + 2));

  const uint8_t* downgrade_sentinel =
      sh->buffer().data() + 2 + kRandomLen - sizeof(tls1_downgrade_random);
  if (c_ver < s_ver) {
    if (c_ver == SSL_LIBRARY_VERSION_TLS_1_2) {
      EXPECT_EQ(0, memcmp(downgrade_sentinel, tls12_downgrade_random,
                          sizeof(tls12_downgrade_random)));
    } else {
      EXPECT_EQ(0, memcmp(downgrade_sentinel, tls1_downgrade_random,
                          sizeof(tls1_downgrade_random)));
    }
  } else {
    EXPECT_NE(0, memcmp(downgrade_sentinel, tls12_downgrade_random,
                        sizeof(tls12_downgrade_random)));
    EXPECT_NE(0, memcmp(downgrade_sentinel, tls1_downgrade_random,
                        sizeof(tls1_downgrade_random)));
  }
}

// TLS 1.1 clients do not check the random values, so we should
// instead get a handshake failure alert from the server.
TEST_F(TlsConnectTest, TestDowngradeDetectionToTls10) {
  // Setting the option here has no effect.
  client_->SetOption(SSL_ENABLE_HELLO_DOWNGRADE_CHECK, PR_TRUE);
  MakeTlsFilter<TlsClientHelloVersionSetter>(client_,
                                             SSL_LIBRARY_VERSION_TLS_1_0);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_0,
                           SSL_LIBRARY_VERSION_TLS_1_1);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_0,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  ConnectExpectAlert(server_, kTlsAlertDecryptError);
  server_->CheckErrorCode(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE);
  client_->CheckErrorCode(SSL_ERROR_DECRYPT_ERROR_ALERT);
}

TEST_F(TlsConnectTest, TestFallbackFromTls12) {
  client_->SetOption(SSL_ENABLE_HELLO_DOWNGRADE_CHECK, PR_TRUE);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_1);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  client_->SetDowngradeCheckVersion(SSL_LIBRARY_VERSION_TLS_1_2);
  ConnectExpectAlert(client_, kTlsAlertIllegalParameter);
  client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_SERVER_HELLO);
  server_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

static SECStatus AllowFalseStart(PRFileDesc* fd, void* arg,
                                 PRBool* can_false_start) {
  bool* false_start_attempted = reinterpret_cast<bool*>(arg);
  *false_start_attempted = true;
  *can_false_start = PR_TRUE;
  return SECSuccess;
}

// If we disable the downgrade check, the sentinel is still generated, and we
// disable false start instead.
TEST_F(TlsConnectTest, DisableFalseStartOnFallback) {
  // Don't call client_->EnableFalseStart(), because that sets the client up for
  // success, and we want false start to fail.
  client_->SetOption(SSL_ENABLE_FALSE_START, PR_TRUE);
  bool false_start_attempted = false;
  EXPECT_EQ(SECSuccess,
            SSL_SetCanFalseStartCallback(client_->ssl_fd(), AllowFalseStart,
                                         &false_start_attempted));

  client_->SetDowngradeCheckVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  Connect();
  EXPECT_FALSE(false_start_attempted);
}

TEST_F(TlsConnectTest, TestFallbackFromTls13) {
  client_->SetOption(SSL_ENABLE_HELLO_DOWNGRADE_CHECK, PR_TRUE);
  client_->SetDowngradeCheckVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  ConnectExpectAlert(client_, kTlsAlertIllegalParameter);
  client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_SERVER_HELLO);
  server_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

TEST_P(TlsConnectGeneric, TestFallbackSCSVVersionMatch) {
  client_->SetOption(SSL_ENABLE_FALLBACK_SCSV, PR_TRUE);
  Connect();
}

TEST_P(TlsConnectGenericPre13, TestFallbackSCSVVersionMismatch) {
  client_->SetOption(SSL_ENABLE_FALLBACK_SCSV, PR_TRUE);
  server_->SetVersionRange(version_, version_ + 1);
  ConnectExpectAlert(server_, kTlsAlertInappropriateFallback);
  client_->CheckErrorCode(SSL_ERROR_INAPPROPRIATE_FALLBACK_ALERT);
  server_->CheckErrorCode(SSL_ERROR_INAPPROPRIATE_FALLBACK_ALERT);
}

// The TLS v1.3 spec section C.4 states that 'Implementations MUST NOT send or
// accept any records with a version less than { 3, 0 }'. Thus we will not
// allow version ranges including both SSL v3 and TLS v1.3.
TEST_F(TlsConnectTest, DisallowSSLv3HelloWithTLSv13Enabled) {
  SECStatus rv;
  SSLVersionRange vrange = {SSL_LIBRARY_VERSION_3_0,
                            SSL_LIBRARY_VERSION_TLS_1_3};

  EnsureTlsSetup();
  rv = SSL_VersionRangeSet(client_->ssl_fd(), &vrange);
  EXPECT_EQ(SECFailure, rv);

  rv = SSL_VersionRangeSet(server_->ssl_fd(), &vrange);
  EXPECT_EQ(SECFailure, rv);
}

TEST_P(TlsConnectGeneric, AlertBeforeServerHello) {
  EnsureTlsSetup();
  client_->ExpectReceiveAlert(kTlsAlertUnrecognizedName, kTlsAlertWarning);
  StartConnect();
  client_->Handshake();  // Send ClientHello.
  static const uint8_t kWarningAlert[] = {kTlsAlertWarning,
                                          kTlsAlertUnrecognizedName};
  DataBuffer alert;
  TlsAgentTestBase::MakeRecord(variant_, ssl_ct_alert,
                               SSL_LIBRARY_VERSION_TLS_1_0, kWarningAlert,
                               PR_ARRAY_SIZE(kWarningAlert), &alert);
  client_->adapter()->PacketReceived(alert);
  Handshake();
  CheckConnected();
}

class Tls13NoSupportedVersions : public TlsConnectStreamTls12 {
 protected:
  void Run(uint16_t overwritten_client_version, uint16_t max_server_version) {
    client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                             SSL_LIBRARY_VERSION_TLS_1_2);
    server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2, max_server_version);
    MakeTlsFilter<TlsClientHelloVersionSetter>(client_,
                                               overwritten_client_version);
    auto capture =
        MakeTlsFilter<TlsHandshakeRecorder>(server_, kTlsHandshakeServerHello);
    ConnectExpectAlert(server_, kTlsAlertDecryptError);
    client_->CheckErrorCode(SSL_ERROR_DECRYPT_ERROR_ALERT);
    server_->CheckErrorCode(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE);
    const DataBuffer& server_hello = capture->buffer();
    ASSERT_GT(server_hello.len(), 2U);
    uint32_t ver;
    ASSERT_TRUE(server_hello.Read(0, 2, &ver));
    ASSERT_EQ(static_cast<uint32_t>(SSL_LIBRARY_VERSION_TLS_1_2), ver);
  }
};

// If we offer a 1.3 ClientHello w/o supported_versions, the server should
// negotiate 1.2.
TEST_F(Tls13NoSupportedVersions,
       Tls13ClientHelloWithoutSupportedVersionsServer12) {
  Run(SSL_LIBRARY_VERSION_TLS_1_3, SSL_LIBRARY_VERSION_TLS_1_2);
}

TEST_F(Tls13NoSupportedVersions,
       Tls13ClientHelloWithoutSupportedVersionsServer13) {
  Run(SSL_LIBRARY_VERSION_TLS_1_3, SSL_LIBRARY_VERSION_TLS_1_3);
}

TEST_F(Tls13NoSupportedVersions,
       Tls14ClientHelloWithoutSupportedVersionsServer13) {
  Run(SSL_LIBRARY_VERSION_TLS_1_3 + 1, SSL_LIBRARY_VERSION_TLS_1_3);
}

// Offer 1.3 but with ClientHello.legacy_version == TLS 1.4. This
// causes a bad MAC error when we read EncryptedExtensions.
TEST_F(TlsConnectStreamTls13, Tls14ClientHelloWithSupportedVersions) {
  MakeTlsFilter<TlsClientHelloVersionSetter>(client_,
                                             SSL_LIBRARY_VERSION_TLS_1_3 + 1);
  auto capture = MakeTlsFilter<TlsExtensionCapture>(
      server_, ssl_tls13_supported_versions_xtn);
  client_->ExpectSendAlert(kTlsAlertBadRecordMac);
  server_->ExpectSendAlert(kTlsAlertBadRecordMac);
  ConnectExpectFail();
  client_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
  server_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);

  ASSERT_EQ(2U, capture->extension().len());
  uint32_t version = 0;
  ASSERT_TRUE(capture->extension().Read(0, 2, &version));
  // This way we don't need to change with new draft version.
  ASSERT_LT(static_cast<uint32_t>(SSL_LIBRARY_VERSION_TLS_1_2), version);
}

// Offer 1.3 but with ClientHello.legacy_version == SSL 3.0. This
// causes a protocol version alert.  See RFC 8446 Appendix D.5.
TEST_F(TlsConnectStreamTls13, Ssl30ClientHelloWithSupportedVersions) {
  MakeTlsFilter<TlsClientHelloVersionSetter>(client_, SSL_LIBRARY_VERSION_3_0);
  ConnectExpectAlert(server_, kTlsAlertProtocolVersion);
}

// Verify the client sends only DTLS versions in supported_versions
TEST_F(DtlsConnectTest, DtlsSupportedVersionsEncoding) {
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  auto capture = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_tls13_supported_versions_xtn);
  Connect();

  ASSERT_EQ(7U, capture->extension().len());
  uint32_t version = 0;
  ASSERT_TRUE(capture->extension().Read(1, 2, &version));
  EXPECT_EQ(0x7f00 | DTLS_1_3_DRAFT_VERSION, static_cast<int>(version));
  ASSERT_TRUE(capture->extension().Read(3, 2, &version));
  EXPECT_EQ(SSL_LIBRARY_VERSION_DTLS_1_2_WIRE, static_cast<int>(version));
  ASSERT_TRUE(capture->extension().Read(5, 2, &version));
  EXPECT_EQ(SSL_LIBRARY_VERSION_DTLS_1_0_WIRE, static_cast<int>(version));
}

// Verify the client sends only TLS versions in supported_versions
TEST_F(TlsConnectTest, TlsSupportedVersionsEncoding) {
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_0,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_0,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  auto capture = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_tls13_supported_versions_xtn);
  Connect();

  ASSERT_EQ(9U, capture->extension().len());
  uint32_t version = 0;
  ASSERT_TRUE(capture->extension().Read(1, 2, &version));
  EXPECT_EQ(SSL_LIBRARY_VERSION_TLS_1_3, static_cast<int>(version));
  ASSERT_TRUE(capture->extension().Read(3, 2, &version));
  EXPECT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, static_cast<int>(version));
  ASSERT_TRUE(capture->extension().Read(5, 2, &version));
  EXPECT_EQ(SSL_LIBRARY_VERSION_TLS_1_1, static_cast<int>(version));
  ASSERT_TRUE(capture->extension().Read(7, 2, &version));
  EXPECT_EQ(SSL_LIBRARY_VERSION_TLS_1_0, static_cast<int>(version));
}

INSTANTIATE_TEST_CASE_P(
    TlsDowngradeSentinelTest, TlsDowngradeTest,
    ::testing::Combine(TlsConnectTestBase::kTlsVariantsStream,
                       TlsConnectTestBase::kTlsVAll,
                       TlsConnectTestBase::kTlsV12Plus));

}  // namespace nss_test
