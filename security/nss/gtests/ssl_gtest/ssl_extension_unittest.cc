/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ssl.h"
#include "ssl3prot.h"
#include "sslerr.h"
#include "sslproto.h"

// This is only to get DTLS_1_3_DRAFT_VERSION
#include "ssl3prot.h"

#include <memory>

#include "tls_connect.h"
#include "tls_filter.h"
#include "tls_parser.h"

namespace nss_test {

class Dtls13LegacyCookieInjector : public TlsHandshakeFilter {
 public:
  Dtls13LegacyCookieInjector(const std::shared_ptr<TlsAgent>& a)
      : TlsHandshakeFilter(a, {kTlsHandshakeClientHello}) {}

  virtual PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                               const DataBuffer& input,
                                               DataBuffer* output) {
    const uint8_t cookie_bytes[] = {0x03, 0x0A, 0x0B, 0x0C};
    uint32_t offset = 2 /* version */ + 32 /* random */;

    if (agent()->variant() != ssl_variant_datagram) {
      ADD_FAILURE();
      return KEEP;
    }

    if (header.handshake_type() != ssl_hs_client_hello) {
      return KEEP;
    }

    DataBuffer cookie(cookie_bytes, sizeof(cookie_bytes));
    *output = input;

    // Add the SID length (if any) to locate the cookie.
    uint32_t sid_len = 0;
    if (!output->Read(offset, 1, &sid_len)) {
      ADD_FAILURE();
      return KEEP;
    }
    offset += 1 + sid_len;
    output->Splice(cookie, offset, 1);

    return CHANGE;
  }

 private:
  DataBuffer cookie_;
};

class TlsExtensionTruncator : public TlsExtensionFilter {
 public:
  TlsExtensionTruncator(const std::shared_ptr<TlsAgent>& a, uint16_t extension,
                        size_t length)
      : TlsExtensionFilter(a), extension_(extension), length_(length) {}
  virtual PacketFilter::Action FilterExtension(uint16_t extension_type,
                                               const DataBuffer& input,
                                               DataBuffer* output) {
    if (extension_type != extension_) {
      return KEEP;
    }
    if (input.len() <= length_) {
      return KEEP;
    }

    output->Assign(input.data(), length_);
    return CHANGE;
  }

 private:
  uint16_t extension_;
  size_t length_;
};

class TlsExtensionTestBase : public TlsConnectTestBase {
 protected:
  TlsExtensionTestBase(SSLProtocolVariant variant, uint16_t version)
      : TlsConnectTestBase(variant, version) {}

  void ClientHelloErrorTest(std::shared_ptr<PacketFilter> filter,
                            uint8_t desc = kTlsAlertDecodeError) {
    client_->SetFilter(filter);
    ConnectExpectAlert(server_, desc);
  }

  void ServerHelloErrorTest(std::shared_ptr<PacketFilter> filter,
                            uint8_t desc = kTlsAlertDecodeError) {
    server_->SetFilter(filter);
    ConnectExpectAlert(client_, desc);
  }

  static void InitSimpleSni(DataBuffer* extension) {
    const char* name = "host.name";
    const size_t namelen = PL_strlen(name);
    extension->Allocate(namelen + 5);
    extension->Write(0, namelen + 3, 2);
    extension->Write(2, static_cast<uint32_t>(0), 1);  // 0 == hostname
    extension->Write(3, namelen, 2);
    extension->Write(5, reinterpret_cast<const uint8_t*>(name), namelen);
  }

  void HrrThenRemoveExtensionsTest(SSLExtensionType type, PRInt32 client_error,
                                   PRInt32 server_error) {
    static const std::vector<SSLNamedGroup> client_groups = {
        ssl_grp_ec_secp384r1, ssl_grp_ec_curve25519};
    static const std::vector<SSLNamedGroup> server_groups = {
        ssl_grp_ec_curve25519, ssl_grp_ec_secp384r1};
    client_->ConfigNamedGroups(client_groups);
    server_->ConfigNamedGroups(server_groups);
    EnsureTlsSetup();
    StartConnect();
    client_->Handshake();  // Send ClientHello
    server_->Handshake();  // Send HRR.
    MakeTlsFilter<TlsExtensionDropper>(client_, type);
    Handshake();
    client_->CheckErrorCode(client_error);
    server_->CheckErrorCode(server_error);
  }
};

class TlsExtensionTestDtls : public TlsExtensionTestBase,
                             public ::testing::WithParamInterface<uint16_t> {
 public:
  TlsExtensionTestDtls()
      : TlsExtensionTestBase(ssl_variant_datagram, GetParam()) {}
};

class TlsExtensionTest12Plus : public TlsExtensionTestBase,
                               public ::testing::WithParamInterface<
                                   std::tuple<SSLProtocolVariant, uint16_t>> {
 public:
  TlsExtensionTest12Plus()
      : TlsExtensionTestBase(std::get<0>(GetParam()), std::get<1>(GetParam())) {
  }
};

class TlsExtensionTest12 : public TlsExtensionTestBase,
                           public ::testing::WithParamInterface<
                               std::tuple<SSLProtocolVariant, uint16_t>> {
 public:
  TlsExtensionTest12()
      : TlsExtensionTestBase(std::get<0>(GetParam()), std::get<1>(GetParam())) {
  }
};

class TlsExtensionTest13
    : public TlsExtensionTestBase,
      public ::testing::WithParamInterface<SSLProtocolVariant> {
 public:
  TlsExtensionTest13()
      : TlsExtensionTestBase(GetParam(), SSL_LIBRARY_VERSION_TLS_1_3) {}

  void ConnectWithBogusVersionList(const uint8_t* buf, size_t len) {
    DataBuffer versions_buf(buf, len);
    MakeTlsFilter<TlsExtensionReplacer>(
        client_, ssl_tls13_supported_versions_xtn, versions_buf);
    ConnectExpectAlert(server_, kTlsAlertIllegalParameter);
    client_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
    server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
  }

  void ConnectWithReplacementVersionList(uint16_t version) {
    // Convert the version encoding for DTLS, if needed.
    if (variant_ == ssl_variant_datagram) {
      switch (version) {
        case SSL_LIBRARY_VERSION_TLS_1_3:
#ifdef DTLS_1_3_DRAFT_VERSION
          version = 0x7f00 | DTLS_1_3_DRAFT_VERSION;
#else
          version = SSL_LIBRARY_VERSION_DTLS_1_3_WIRE;
#endif
          break;
        case SSL_LIBRARY_VERSION_TLS_1_2:
          version = SSL_LIBRARY_VERSION_DTLS_1_2_WIRE;
          break;
        case SSL_LIBRARY_VERSION_TLS_1_1:
          /* TLS_1_1 maps to DTLS_1_0, see sslproto.h. */
          version = SSL_LIBRARY_VERSION_DTLS_1_0_WIRE;
          break;
        default:
          PORT_Assert(0);
      }
    }

    DataBuffer versions_buf;
    size_t index = versions_buf.Write(0, 2, 1);
    versions_buf.Write(index, version, 2);
    MakeTlsFilter<TlsExtensionReplacer>(
        client_, ssl_tls13_supported_versions_xtn, versions_buf);
    ConnectExpectFail();
  }
};

class TlsExtensionTest13Stream : public TlsExtensionTestBase {
 public:
  TlsExtensionTest13Stream()
      : TlsExtensionTestBase(ssl_variant_stream, SSL_LIBRARY_VERSION_TLS_1_3) {}
};

class TlsExtensionTestGeneric : public TlsExtensionTestBase,
                                public ::testing::WithParamInterface<
                                    std::tuple<SSLProtocolVariant, uint16_t>> {
 public:
  TlsExtensionTestGeneric()
      : TlsExtensionTestBase(std::get<0>(GetParam()), std::get<1>(GetParam())) {
  }
};

class TlsExtensionTestPre13 : public TlsExtensionTestBase,
                              public ::testing::WithParamInterface<
                                  std::tuple<SSLProtocolVariant, uint16_t>> {
 public:
  TlsExtensionTestPre13()
      : TlsExtensionTestBase(std::get<0>(GetParam()), std::get<1>(GetParam())) {
  }
};

TEST_P(TlsExtensionTestGeneric, DamageSniLength) {
  ClientHelloErrorTest(
      std::make_shared<TlsExtensionDamager>(client_, ssl_server_name_xtn, 1));
}

TEST_P(TlsExtensionTestGeneric, DamageSniHostLength) {
  ClientHelloErrorTest(
      std::make_shared<TlsExtensionDamager>(client_, ssl_server_name_xtn, 4));
}

TEST_P(TlsExtensionTestGeneric, TruncateSni) {
  ClientHelloErrorTest(
      std::make_shared<TlsExtensionTruncator>(client_, ssl_server_name_xtn, 7));
}

// A valid extension that appears twice will be reported as unsupported.
TEST_P(TlsExtensionTestGeneric, RepeatSni) {
  DataBuffer extension;
  InitSimpleSni(&extension);
  ClientHelloErrorTest(std::make_shared<TlsExtensionInjector>(
                           client_, ssl_server_name_xtn, extension),
                       kTlsAlertIllegalParameter);
}

// An SNI entry with zero length is considered invalid (strangely, not if it is
// the last entry, which is probably a bug).
TEST_P(TlsExtensionTestGeneric, BadSni) {
  DataBuffer simple;
  InitSimpleSni(&simple);
  DataBuffer extension;
  extension.Allocate(simple.len() + 3);
  extension.Write(0, static_cast<uint32_t>(0), 3);
  extension.Write(3, simple);
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      client_, ssl_server_name_xtn, extension));
}

TEST_P(TlsExtensionTestGeneric, EmptySni) {
  DataBuffer extension;
  extension.Allocate(2);
  extension.Write(0, static_cast<uint32_t>(0), 2);
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      client_, ssl_server_name_xtn, extension));
}

TEST_P(TlsExtensionTestGeneric, EmptyAlpnExtension) {
  EnableAlpn();
  DataBuffer extension;
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
                           client_, ssl_app_layer_protocol_xtn, extension),
                       kTlsAlertIllegalParameter);
}

// An empty ALPN isn't considered bad, though it does lead to there being no
// protocol for the server to select.
TEST_P(TlsExtensionTestGeneric, EmptyAlpnList) {
  EnableAlpn();
  const uint8_t val[] = {0x00, 0x00};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
                           client_, ssl_app_layer_protocol_xtn, extension),
                       kTlsAlertNoApplicationProtocol);
}

TEST_P(TlsExtensionTestGeneric, OneByteAlpn) {
  EnableAlpn();
  ClientHelloErrorTest(std::make_shared<TlsExtensionTruncator>(
      client_, ssl_app_layer_protocol_xtn, 1));
}

TEST_P(TlsExtensionTestGeneric, AlpnMissingValue) {
  EnableAlpn();
  // This will leave the length of the second entry, but no value.
  ClientHelloErrorTest(std::make_shared<TlsExtensionTruncator>(
      client_, ssl_app_layer_protocol_xtn, 5));
}

TEST_P(TlsExtensionTestGeneric, AlpnZeroLength) {
  EnableAlpn();
  const uint8_t val[] = {0x00, 0x03, 0x01, 0x61, 0x00};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      client_, ssl_app_layer_protocol_xtn, extension));
}

TEST_P(TlsExtensionTestGeneric, AlpnLengthOverflow) {
  EnableAlpn();
  const uint8_t val[] = {0x00, 0x03, 0x01, 0x61, 0x01};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      client_, ssl_app_layer_protocol_xtn, extension));
}

TEST_P(TlsExtensionTestGeneric, AlpnMismatch) {
  const uint8_t client_alpn[] = {0x01, 0x61};
  client_->EnableAlpn(client_alpn, sizeof(client_alpn));
  const uint8_t server_alpn[] = {0x02, 0x61, 0x62};
  server_->EnableAlpn(server_alpn, sizeof(server_alpn));

  ClientHelloErrorTest(nullptr, kTlsAlertNoApplicationProtocol);
}

TEST_P(TlsExtensionTestGeneric, AlpnDisabledServer) {
  const uint8_t client_alpn[] = {0x01, 0x61};
  client_->EnableAlpn(client_alpn, sizeof(client_alpn));
  server_->EnableAlpn(nullptr, 0);

  ClientHelloErrorTest(nullptr, kTlsAlertUnsupportedExtension);
}

TEST_P(TlsConnectGeneric, AlpnDisabled) {
  server_->EnableAlpn(nullptr, 0);
  Connect();

  SSLNextProtoState state;
  uint8_t buf[255] = {0};
  unsigned int buf_len = 3;
  EXPECT_EQ(SECSuccess, SSL_GetNextProto(client_->ssl_fd(), &state, buf,
                                         &buf_len, sizeof(buf)));
  EXPECT_EQ(SSL_NEXT_PROTO_NO_SUPPORT, state);
  EXPECT_EQ(0U, buf_len);
}

// Many of these tests fail in TLS 1.3 because the extension is encrypted, which
// prevents modification of the value from the ServerHello.
TEST_P(TlsExtensionTestPre13, AlpnReturnedEmptyList) {
  EnableAlpn();
  const uint8_t val[] = {0x00, 0x00};
  DataBuffer extension(val, sizeof(val));
  ServerHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      server_, ssl_app_layer_protocol_xtn, extension));
}

TEST_P(TlsExtensionTestPre13, AlpnReturnedEmptyName) {
  EnableAlpn();
  const uint8_t val[] = {0x00, 0x01, 0x00};
  DataBuffer extension(val, sizeof(val));
  ServerHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      server_, ssl_app_layer_protocol_xtn, extension));
}

TEST_P(TlsExtensionTestPre13, AlpnReturnedListTrailingData) {
  EnableAlpn();
  const uint8_t val[] = {0x00, 0x02, 0x01, 0x61, 0x00};
  DataBuffer extension(val, sizeof(val));
  ServerHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      server_, ssl_app_layer_protocol_xtn, extension));
}

TEST_P(TlsExtensionTestPre13, AlpnReturnedExtraEntry) {
  EnableAlpn();
  const uint8_t val[] = {0x00, 0x04, 0x01, 0x61, 0x01, 0x62};
  DataBuffer extension(val, sizeof(val));
  ServerHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      server_, ssl_app_layer_protocol_xtn, extension));
}

TEST_P(TlsExtensionTestPre13, AlpnReturnedBadListLength) {
  EnableAlpn();
  const uint8_t val[] = {0x00, 0x99, 0x01, 0x61, 0x00};
  DataBuffer extension(val, sizeof(val));
  ServerHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      server_, ssl_app_layer_protocol_xtn, extension));
}

TEST_P(TlsExtensionTestPre13, AlpnReturnedBadNameLength) {
  EnableAlpn();
  const uint8_t val[] = {0x00, 0x02, 0x99, 0x61};
  DataBuffer extension(val, sizeof(val));
  ServerHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      server_, ssl_app_layer_protocol_xtn, extension));
}

TEST_P(TlsExtensionTestPre13, AlpnReturnedUnknownName) {
  EnableAlpn();
  const uint8_t val[] = {0x00, 0x02, 0x01, 0x67};
  DataBuffer extension(val, sizeof(val));
  ServerHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
                           server_, ssl_app_layer_protocol_xtn, extension),
                       kTlsAlertIllegalParameter);
}

TEST_P(TlsExtensionTestDtls, SrtpShort) {
  EnableSrtp();
  ClientHelloErrorTest(
      std::make_shared<TlsExtensionTruncator>(client_, ssl_use_srtp_xtn, 3));
}

TEST_P(TlsExtensionTestDtls, SrtpOdd) {
  EnableSrtp();
  const uint8_t val[] = {0x00, 0x01, 0xff, 0x00};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      client_, ssl_use_srtp_xtn, extension));
}

TEST_P(TlsExtensionTest12Plus, SignatureAlgorithmsBadLength) {
  const uint8_t val[] = {0x00};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      client_, ssl_signature_algorithms_xtn, extension));
}

TEST_P(TlsExtensionTest12Plus, SignatureAlgorithmsTrailingData) {
  const uint8_t val[] = {0x00, 0x02, 0x04, 0x01, 0x00};  // sha-256, rsa
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      client_, ssl_signature_algorithms_xtn, extension));
}

TEST_P(TlsExtensionTest12Plus, SignatureAlgorithmsEmpty) {
  const uint8_t val[] = {0x00, 0x00};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
                           client_, ssl_signature_algorithms_xtn, extension),
                       kTlsAlertHandshakeFailure);
}

TEST_P(TlsExtensionTest12Plus, SignatureAlgorithmsNoOverlap) {
  const uint8_t val[] = {0x00, 0x02, 0xff, 0xff};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
                           client_, ssl_signature_algorithms_xtn, extension),
                       kTlsAlertHandshakeFailure);
}

TEST_P(TlsExtensionTest12Plus, SignatureAlgorithmsOddLength) {
  const uint8_t val[] = {0x00, 0x01, 0x04};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      client_, ssl_signature_algorithms_xtn, extension));
}

TEST_F(TlsExtensionTest13Stream, SignatureAlgorithmsPrecedingGarbage) {
  // 31 unknown signature algorithms followed by sha-256, rsa-pss
  const uint8_t val[] = {
      0x00, 0x40, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x08, 0x04};
  DataBuffer extension(val, sizeof(val));
  MakeTlsFilter<TlsExtensionReplacer>(client_, ssl_signature_algorithms_xtn,
                                      extension);
  client_->ExpectSendAlert(kTlsAlertBadRecordMac);
  server_->ExpectSendAlert(kTlsAlertBadRecordMac);
  ConnectExpectFail();
  client_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
  server_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
}

TEST_P(TlsExtensionTestGeneric, NoSupportedGroups) {
  ClientHelloErrorTest(
      std::make_shared<TlsExtensionDropper>(client_, ssl_supported_groups_xtn),
      version_ < SSL_LIBRARY_VERSION_TLS_1_3 ? kTlsAlertDecryptError
                                             : kTlsAlertMissingExtension);
}

TEST_P(TlsExtensionTestGeneric, SupportedCurvesShort) {
  const uint8_t val[] = {0x00, 0x01, 0x00};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      client_, ssl_elliptic_curves_xtn, extension));
}

TEST_P(TlsExtensionTestGeneric, SupportedCurvesBadLength) {
  const uint8_t val[] = {0x09, 0x99, 0x00, 0x00};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      client_, ssl_elliptic_curves_xtn, extension));
}

TEST_P(TlsExtensionTestGeneric, SupportedCurvesTrailingData) {
  const uint8_t val[] = {0x00, 0x02, 0x00, 0x00, 0x00};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      client_, ssl_elliptic_curves_xtn, extension));
}

TEST_P(TlsExtensionTest12, SupportedCurvesDisableX25519) {
  // Disable session resumption.
  ConfigureSessionCache(RESUME_NONE, RESUME_NONE);

  // Ensure that we can enable its use in the key exchange.
  SECStatus rv =
      NSS_SetAlgorithmPolicy(SEC_OID_CURVE25519, NSS_USE_ALG_IN_SSL_KX, 0);
  ASSERT_EQ(SECSuccess, rv);
  rv = NSS_SetAlgorithmPolicy(SEC_OID_APPLY_SSL_POLICY, NSS_USE_POLICY_IN_SSL,
                              0);
  ASSERT_EQ(SECSuccess, rv);

  auto capture1 =
      MakeTlsFilter<TlsExtensionCapture>(client_, ssl_elliptic_curves_xtn);
  Connect();

  EXPECT_TRUE(capture1->captured());
  const DataBuffer& ext1 = capture1->extension();

  uint32_t count;
  ASSERT_TRUE(ext1.Read(0, 2, &count));

  // Whether or not we've seen x25519 offered in this handshake.
  bool seen1_x25519 = false;
  for (size_t offset = 2; offset <= count; offset++) {
    uint32_t val;
    ASSERT_TRUE(ext1.Read(offset, 2, &val));
    if (val == ssl_grp_ec_curve25519) {
      seen1_x25519 = true;
      break;
    }
  }
  ASSERT_TRUE(seen1_x25519);

  // Ensure that we can disable its use in the key exchange.
  rv = NSS_SetAlgorithmPolicy(SEC_OID_CURVE25519, 0, NSS_USE_ALG_IN_SSL_KX);
  ASSERT_EQ(SECSuccess, rv);
  rv = NSS_SetAlgorithmPolicy(SEC_OID_APPLY_SSL_POLICY, NSS_USE_POLICY_IN_SSL,
                              0);
  ASSERT_EQ(SECSuccess, rv);

  // Clean up after the last run.
  Reset();
  auto capture2 =
      MakeTlsFilter<TlsExtensionCapture>(client_, ssl_elliptic_curves_xtn);
  Connect();

  EXPECT_TRUE(capture2->captured());
  const DataBuffer& ext2 = capture2->extension();

  ASSERT_TRUE(ext2.Read(0, 2, &count));

  // Whether or not we've seen x25519 offered in this handshake.
  bool seen2_x25519 = false;
  for (size_t offset = 2; offset <= count; offset++) {
    uint32_t val;
    ASSERT_TRUE(ext2.Read(offset, 2, &val));

    if (val == ssl_grp_ec_curve25519) {
      seen2_x25519 = true;
      break;
    }
  }

  ASSERT_FALSE(seen2_x25519);
}

TEST_P(TlsExtensionTestPre13, SupportedPointsEmpty) {
  const uint8_t val[] = {0x00};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      client_, ssl_ec_point_formats_xtn, extension));
}

TEST_P(TlsExtensionTestPre13, SupportedPointsBadLength) {
  const uint8_t val[] = {0x99, 0x00, 0x00};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      client_, ssl_ec_point_formats_xtn, extension));
}

TEST_P(TlsExtensionTestPre13, SupportedPointsTrailingData) {
  const uint8_t val[] = {0x01, 0x00, 0x00};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      client_, ssl_ec_point_formats_xtn, extension));
}

TEST_P(TlsExtensionTestPre13, RenegotiationInfoBadLength) {
  const uint8_t val[] = {0x99};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      client_, ssl_renegotiation_info_xtn, extension));
}

TEST_P(TlsExtensionTestPre13, RenegotiationInfoMismatch) {
  const uint8_t val[] = {0x01, 0x00};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      client_, ssl_renegotiation_info_xtn, extension));
}

// The extension has to contain a length.
TEST_P(TlsExtensionTestPre13, RenegotiationInfoExtensionEmpty) {
  DataBuffer extension;
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      client_, ssl_renegotiation_info_xtn, extension));
}

// This only works on TLS 1.2, since it relies on static RSA; otherwise libssl
// picks the wrong cipher suite.
TEST_P(TlsExtensionTest12, SignatureAlgorithmConfiguration) {
  const SSLSignatureScheme schemes[] = {ssl_sig_rsa_pss_rsae_sha512,
                                        ssl_sig_rsa_pss_rsae_sha384};

  auto capture =
      MakeTlsFilter<TlsExtensionCapture>(client_, ssl_signature_algorithms_xtn);
  client_->SetSignatureSchemes(schemes, PR_ARRAY_SIZE(schemes));
  EnableOnlyStaticRsaCiphers();
  Connect();

  const DataBuffer& ext = capture->extension();
  EXPECT_EQ(2 + PR_ARRAY_SIZE(schemes) * 2, ext.len());
  for (size_t i = 0, cursor = 2;
       i < PR_ARRAY_SIZE(schemes) && cursor < ext.len(); ++i) {
    uint32_t v = 0;
    EXPECT_TRUE(ext.Read(cursor, 2, &v));
    cursor += 2;
    EXPECT_EQ(schemes[i], static_cast<SSLSignatureScheme>(v));
  }
}

// This only works on TLS 1.2, since it relies on DSA.
TEST_P(TlsExtensionTest12, SignatureAlgorithmDisableDSA) {
  const std::vector<SSLSignatureScheme> schemes = {
      ssl_sig_dsa_sha1, ssl_sig_dsa_sha256, ssl_sig_dsa_sha384,
      ssl_sig_dsa_sha512, ssl_sig_rsa_pss_rsae_sha256};

  // Connect with DSA enabled by policy.
  SECStatus rv = NSS_SetAlgorithmPolicy(SEC_OID_ANSIX9_DSA_SIGNATURE,
                                        NSS_USE_ALG_IN_SSL_KX, 0);
  ASSERT_EQ(SECSuccess, rv);
  rv = NSS_SetAlgorithmPolicy(SEC_OID_APPLY_SSL_POLICY, NSS_USE_POLICY_IN_SSL,
                              0);
  ASSERT_EQ(SECSuccess, rv);

  Reset(TlsAgent::kServerDsa);
  auto capture1 =
      MakeTlsFilter<TlsExtensionCapture>(client_, ssl_signature_algorithms_xtn);
  client_->SetSignatureSchemes(schemes.data(), schemes.size());
  Connect();

  // Check if all the signature algorithms are advertised.
  EXPECT_TRUE(capture1->captured());
  const DataBuffer& ext1 = capture1->extension();
  EXPECT_EQ(2U + 2U * schemes.size(), ext1.len());

  // Connect with DSA disabled by policy.
  rv = NSS_SetAlgorithmPolicy(SEC_OID_ANSIX9_DSA_SIGNATURE, 0,
                              NSS_USE_ALG_IN_SSL_KX);
  ASSERT_EQ(SECSuccess, rv);
  rv = NSS_SetAlgorithmPolicy(SEC_OID_APPLY_SSL_POLICY, NSS_USE_POLICY_IN_SSL,
                              0);
  ASSERT_EQ(SECSuccess, rv);

  Reset(TlsAgent::kServerDsa);
  auto capture2 =
      MakeTlsFilter<TlsExtensionCapture>(client_, ssl_signature_algorithms_xtn);
  client_->SetSignatureSchemes(schemes.data(), schemes.size());
  ConnectExpectAlert(server_, kTlsAlertHandshakeFailure);
  server_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
  client_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);

  // Check if no DSA algorithms are advertised.
  EXPECT_TRUE(capture2->captured());
  const DataBuffer& ext2 = capture2->extension();
  EXPECT_EQ(2U + 2U, ext2.len());
  uint32_t v = 0;
  EXPECT_TRUE(ext2.Read(2, 2, &v));
  EXPECT_EQ(ssl_sig_rsa_pss_rsae_sha256, v);
}

// Temporary test to verify that we choke on an empty ClientKeyShare.
// This test will fail when we implement HelloRetryRequest.
TEST_P(TlsExtensionTest13, EmptyClientKeyShare) {
  ClientHelloErrorTest(std::make_shared<TlsExtensionTruncator>(
                           client_, ssl_tls13_key_share_xtn, 2),
                       kTlsAlertHandshakeFailure);
}

// These tests only work in stream mode because the client sends a
// cleartext alert which causes a MAC error on the server. With
// stream this causes handshake failure but with datagram, the
// packet gets dropped.
TEST_F(TlsExtensionTest13Stream, DropServerKeyShare) {
  EnsureTlsSetup();
  MakeTlsFilter<TlsExtensionDropper>(server_, ssl_tls13_key_share_xtn);
  client_->ExpectSendAlert(kTlsAlertMissingExtension);
  server_->ExpectSendAlert(kTlsAlertUnexpectedMessage);
  ConnectExpectFail();
  EXPECT_EQ(SSL_ERROR_MISSING_KEY_SHARE, client_->error_code());
  EXPECT_EQ(SSL_ERROR_RX_UNEXPECTED_RECORD_TYPE, server_->error_code());
}

TEST_F(TlsExtensionTest13Stream, WrongServerKeyShare) {
  const uint16_t wrong_group = ssl_grp_ec_secp384r1;

  static const uint8_t key_share[] = {
      wrong_group >> 8,
      wrong_group & 0xff,  // Group we didn't offer.
      0x00,
      0x02,  // length = 2
      0x01,
      0x02};
  DataBuffer buf(key_share, sizeof(key_share));
  EnsureTlsSetup();
  MakeTlsFilter<TlsExtensionReplacer>(server_, ssl_tls13_key_share_xtn, buf);
  client_->ExpectSendAlert(kTlsAlertIllegalParameter);
  server_->ExpectSendAlert(kTlsAlertUnexpectedMessage);
  ConnectExpectFail();
  EXPECT_EQ(SSL_ERROR_RX_MALFORMED_KEY_SHARE, client_->error_code());
  EXPECT_EQ(SSL_ERROR_RX_UNEXPECTED_RECORD_TYPE, server_->error_code());
}

TEST_F(TlsExtensionTest13Stream, UnknownServerKeyShare) {
  const uint16_t wrong_group = 0xffff;

  static const uint8_t key_share[] = {
      wrong_group >> 8,
      wrong_group & 0xff,  // Group we didn't offer.
      0x00,
      0x02,  // length = 2
      0x01,
      0x02};
  DataBuffer buf(key_share, sizeof(key_share));
  EnsureTlsSetup();
  MakeTlsFilter<TlsExtensionReplacer>(server_, ssl_tls13_key_share_xtn, buf);
  client_->ExpectSendAlert(kTlsAlertIllegalParameter);
  server_->ExpectSendAlert(kTlsAlertUnexpectedMessage);
  ConnectExpectFail();
  EXPECT_EQ(SSL_ERROR_RX_MALFORMED_KEY_SHARE, client_->error_code());
  EXPECT_EQ(SSL_ERROR_RX_UNEXPECTED_RECORD_TYPE, server_->error_code());
}

TEST_F(TlsExtensionTest13Stream, AddServerSignatureAlgorithmsOnResumption) {
  SetupForResume();
  DataBuffer empty;
  MakeTlsFilter<TlsExtensionInjector>(server_, ssl_signature_algorithms_xtn,
                                      empty);
  client_->ExpectSendAlert(kTlsAlertUnsupportedExtension);
  server_->ExpectSendAlert(kTlsAlertUnexpectedMessage);
  ConnectExpectFail();
  EXPECT_EQ(SSL_ERROR_EXTENSION_DISALLOWED_FOR_VERSION, client_->error_code());
  EXPECT_EQ(SSL_ERROR_RX_UNEXPECTED_RECORD_TYPE, server_->error_code());
}

struct PskIdentity {
  DataBuffer identity;
  uint32_t obfuscated_ticket_age;
};

class TlsPreSharedKeyReplacer;

typedef std::function<void(TlsPreSharedKeyReplacer*)>
    TlsPreSharedKeyReplacerFunc;

class TlsPreSharedKeyReplacer : public TlsExtensionFilter {
 public:
  TlsPreSharedKeyReplacer(const std::shared_ptr<TlsAgent>& a,
                          TlsPreSharedKeyReplacerFunc function)
      : TlsExtensionFilter(a), identities_(), binders_(), function_(function) {}

  static size_t CopyAndMaybeReplace(TlsParser* parser, size_t size,
                                    const std::unique_ptr<DataBuffer>& replace,
                                    size_t index, DataBuffer* output) {
    DataBuffer tmp;
    bool ret = parser->ReadVariable(&tmp, size);
    EXPECT_EQ(true, ret);
    if (!ret) return 0;
    if (replace) {
      tmp = *replace;
    }

    return WriteVariable(output, index, tmp, size);
  }

  PacketFilter::Action FilterExtension(uint16_t extension_type,
                                       const DataBuffer& input,
                                       DataBuffer* output) {
    if (extension_type != ssl_tls13_pre_shared_key_xtn) {
      return KEEP;
    }

    if (!Decode(input)) {
      return KEEP;
    }

    // Call the function.
    function_(this);

    Encode(output);

    return CHANGE;
  }

  std::vector<PskIdentity> identities_;
  std::vector<DataBuffer> binders_;

 private:
  bool Decode(const DataBuffer& input) {
    std::unique_ptr<TlsParser> parser(new TlsParser(input));
    DataBuffer identities;

    if (!parser->ReadVariable(&identities, 2)) {
      ADD_FAILURE();
      return false;
    }

    DataBuffer binders;
    if (!parser->ReadVariable(&binders, 2)) {
      ADD_FAILURE();
      return false;
    }
    EXPECT_EQ(0UL, parser->remaining());

    // Now parse the inner sections.
    parser.reset(new TlsParser(identities));
    while (parser->remaining()) {
      PskIdentity identity;

      if (!parser->ReadVariable(&identity.identity, 2)) {
        ADD_FAILURE();
        return false;
      }

      if (!parser->Read(&identity.obfuscated_ticket_age, 4)) {
        ADD_FAILURE();
        return false;
      }

      identities_.push_back(identity);
    }

    parser.reset(new TlsParser(binders));
    while (parser->remaining()) {
      DataBuffer binder;

      if (!parser->ReadVariable(&binder, 1)) {
        ADD_FAILURE();
        return false;
      }

      binders_.push_back(binder);
    }

    return true;
  }

  void Encode(DataBuffer* output) {
    DataBuffer identities;
    size_t index = 0;
    for (auto id : identities_) {
      index = WriteVariable(&identities, index, id.identity, 2);
      index = identities.Write(index, id.obfuscated_ticket_age, 4);
    }

    DataBuffer binders;
    index = 0;
    for (auto binder : binders_) {
      index = WriteVariable(&binders, index, binder, 1);
    }

    output->Truncate(0);
    index = 0;
    index = WriteVariable(output, index, identities, 2);
    index = WriteVariable(output, index, binders, 2);
  }

  TlsPreSharedKeyReplacerFunc function_;
};

TEST_F(TlsExtensionTest13Stream, ResumeEmptyPskLabel) {
  SetupForResume();

  MakeTlsFilter<TlsPreSharedKeyReplacer>(
      client_, [](TlsPreSharedKeyReplacer* r) {
        r->identities_[0].identity.Truncate(0);
      });
  ConnectExpectAlert(server_, kTlsAlertIllegalParameter);
  client_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
}

// Flip the first byte of the binder.
TEST_F(TlsExtensionTest13Stream, ResumeIncorrectBinderValue) {
  SetupForResume();

  MakeTlsFilter<TlsPreSharedKeyReplacer>(
      client_, [](TlsPreSharedKeyReplacer* r) {
        r->binders_[0].Write(0, r->binders_[0].data()[0] ^ 0xff, 1);
      });
  ConnectExpectAlert(server_, kTlsAlertDecryptError);
  client_->CheckErrorCode(SSL_ERROR_DECRYPT_ERROR_ALERT);
  server_->CheckErrorCode(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE);
}

// Do the same with an External PSK.
TEST_P(TlsConnectTls13, TestTls13PskInvalidBinderValue) {
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_TRUE(!!slot);
  ScopedPK11SymKey key(
      PK11_KeyGen(slot.get(), CKM_HKDF_KEY_GEN, nullptr, 16, nullptr));
  ASSERT_TRUE(!!key);
  AddPsk(key, std::string("foo"), ssl_hash_sha256);
  StartConnect();
  ASSERT_TRUE(client_->MaybeSetResumptionToken());

  MakeTlsFilter<TlsPreSharedKeyReplacer>(
      client_, [](TlsPreSharedKeyReplacer* r) {
        r->binders_[0].Write(0, r->binders_[0].data()[0] ^ 0xff, 1);
      });
  ConnectExpectAlert(server_, kTlsAlertDecryptError);
  client_->CheckErrorCode(SSL_ERROR_DECRYPT_ERROR_ALERT);
  server_->CheckErrorCode(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE);
}

// Extend the binder by one.
TEST_F(TlsExtensionTest13Stream, ResumeIncorrectBinderLength) {
  SetupForResume();

  MakeTlsFilter<TlsPreSharedKeyReplacer>(
      client_, [](TlsPreSharedKeyReplacer* r) {
        r->binders_[0].Write(r->binders_[0].len(), 0xff, 1);
      });
  ConnectExpectAlert(server_, kTlsAlertIllegalParameter);
  client_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
}

// Binders must be at least 32 bytes.
TEST_F(TlsExtensionTest13Stream, ResumeBinderTooShort) {
  SetupForResume();

  MakeTlsFilter<TlsPreSharedKeyReplacer>(
      client_, [](TlsPreSharedKeyReplacer* r) { r->binders_[0].Truncate(31); });
  ConnectExpectAlert(server_, kTlsAlertIllegalParameter);
  client_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
}

// Duplicate the identity and binder. This will fail with an error
// processing the binder (because we extended the identity list.)
TEST_F(TlsExtensionTest13Stream, ResumeTwoPsks) {
  SetupForResume();

  MakeTlsFilter<TlsPreSharedKeyReplacer>(
      client_, [](TlsPreSharedKeyReplacer* r) {
        r->identities_.push_back(r->identities_[0]);
        r->binders_.push_back(r->binders_[0]);
      });
  ConnectExpectAlert(server_, kTlsAlertDecryptError);
  client_->CheckErrorCode(SSL_ERROR_DECRYPT_ERROR_ALERT);
  server_->CheckErrorCode(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE);
}

// The next two tests have mismatches in the number of identities
// and binders. This generates an illegal parameter alert.
TEST_F(TlsExtensionTest13Stream, ResumeTwoIdentitiesOneBinder) {
  SetupForResume();

  MakeTlsFilter<TlsPreSharedKeyReplacer>(
      client_, [](TlsPreSharedKeyReplacer* r) {
        r->identities_.push_back(r->identities_[0]);
      });
  ConnectExpectAlert(server_, kTlsAlertIllegalParameter);
  client_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
}

TEST_F(TlsExtensionTest13Stream, ResumeOneIdentityTwoBinders) {
  SetupForResume();

  MakeTlsFilter<TlsPreSharedKeyReplacer>(
      client_, [](TlsPreSharedKeyReplacer* r) {
        r->binders_.push_back(r->binders_[0]);
      });
  ConnectExpectAlert(server_, kTlsAlertIllegalParameter);
  client_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
}

TEST_F(TlsExtensionTest13Stream, ResumePskExtensionNotLast) {
  SetupForResume();

  const uint8_t empty_buf[] = {0};
  DataBuffer empty(empty_buf, 0);
  // Inject an unused extension after the PSK extension.
  MakeTlsFilter<TlsExtensionAppender>(client_, kTlsHandshakeClientHello, 0xffff,
                                      empty);
  ConnectExpectAlert(server_, kTlsAlertIllegalParameter);
  client_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
}

TEST_F(TlsExtensionTest13Stream, ResumeNoKeModes) {
  SetupForResume();

  DataBuffer empty;
  MakeTlsFilter<TlsExtensionDropper>(client_,
                                     ssl_tls13_psk_key_exchange_modes_xtn);
  ConnectExpectAlert(server_, kTlsAlertMissingExtension);
  client_->CheckErrorCode(SSL_ERROR_MISSING_EXTENSION_ALERT);
  server_->CheckErrorCode(SSL_ERROR_MISSING_PSK_KEY_EXCHANGE_MODES);
}

// The following test contains valid but unacceptable PreSharedKey
// modes and therefore produces non-resumption followed by MAC
// errors.
TEST_F(TlsExtensionTest13Stream, ResumeBogusKeModes) {
  SetupForResume();
  const static uint8_t ke_modes[] = {1,  // Length
                                     kTls13PskKe};

  DataBuffer modes(ke_modes, sizeof(ke_modes));
  MakeTlsFilter<TlsExtensionReplacer>(
      client_, ssl_tls13_psk_key_exchange_modes_xtn, modes);
  client_->ExpectSendAlert(kTlsAlertBadRecordMac);
  server_->ExpectSendAlert(kTlsAlertBadRecordMac);
  ConnectExpectFail();
  client_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
  server_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
}

TEST_P(TlsExtensionTest13, NoKeModesIfResumptionOff) {
  ConfigureSessionCache(RESUME_NONE, RESUME_NONE);
  auto capture = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_tls13_psk_key_exchange_modes_xtn);
  Connect();
  EXPECT_FALSE(capture->captured());
}

// In these tests, we downgrade to TLS 1.2, causing the
// server to negotiate TLS 1.2.
// 1. Both sides only support TLS 1.3, so we get a cipher version
//    error.
TEST_P(TlsExtensionTest13, RemoveTls13FromVersionList) {
  ExpectAlert(server_, kTlsAlertProtocolVersion);
  ConnectWithReplacementVersionList(SSL_LIBRARY_VERSION_TLS_1_2);
  client_->CheckErrorCode(SSL_ERROR_PROTOCOL_VERSION_ALERT);
  server_->CheckErrorCode(SSL_ERROR_UNSUPPORTED_VERSION);
}

// 2. Server supports 1.2 and 1.3, client supports 1.2, so we
//    can't negotiate any ciphers.
TEST_P(TlsExtensionTest13, RemoveTls13FromVersionListServerV12) {
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  ExpectAlert(server_, kTlsAlertHandshakeFailure);
  ConnectWithReplacementVersionList(SSL_LIBRARY_VERSION_TLS_1_2);
  client_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
  server_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
}

// 3. Server supports 1.2 and 1.3, client supports 1.2 and 1.3
// but advertises 1.2 (because we changed things).
TEST_P(TlsExtensionTest13, RemoveTls13FromVersionListBothV12) {
  client_->SetOption(SSL_ENABLE_HELLO_DOWNGRADE_CHECK, PR_TRUE);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
// The downgrade check is disabled in DTLS 1.3, so all that happens when we
// tamper with the supported versions is that the Finished check fails.
#ifdef DTLS_1_3_DRAFT_VERSION
  if (variant_ == ssl_variant_datagram) {
    ExpectAlert(server_, kTlsAlertDecryptError);
  } else
#endif
  {
    ExpectAlert(client_, kTlsAlertIllegalParameter);
  }
  ConnectWithReplacementVersionList(SSL_LIBRARY_VERSION_TLS_1_2);
#ifdef DTLS_1_3_DRAFT_VERSION
  if (variant_ == ssl_variant_datagram) {
    client_->CheckErrorCode(SSL_ERROR_DECRYPT_ERROR_ALERT);
    server_->CheckErrorCode(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE);
  } else
#endif
  {
    client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_SERVER_HELLO);
    server_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
  }
}

TEST_P(TlsExtensionTest13, HrrThenRemoveSignatureAlgorithms) {
  ExpectAlert(server_, kTlsAlertMissingExtension);
  HrrThenRemoveExtensionsTest(ssl_signature_algorithms_xtn,
                              SSL_ERROR_MISSING_EXTENSION_ALERT,
                              SSL_ERROR_MISSING_SIGNATURE_ALGORITHMS_EXTENSION);
}

TEST_P(TlsExtensionTest13, HrrThenRemoveKeyShare) {
  ExpectAlert(server_, kTlsAlertIllegalParameter);
  HrrThenRemoveExtensionsTest(ssl_tls13_key_share_xtn,
                              SSL_ERROR_ILLEGAL_PARAMETER_ALERT,
                              SSL_ERROR_BAD_2ND_CLIENT_HELLO);
}

TEST_P(TlsExtensionTest13, HrrThenRemoveSupportedGroups) {
  ExpectAlert(server_, kTlsAlertMissingExtension);
  HrrThenRemoveExtensionsTest(ssl_supported_groups_xtn,
                              SSL_ERROR_MISSING_EXTENSION_ALERT,
                              SSL_ERROR_MISSING_SUPPORTED_GROUPS_EXTENSION);
}

TEST_P(TlsExtensionTest13, EmptyVersionList) {
  static const uint8_t kExt[] = {0x00, 0x00};
  ConnectWithBogusVersionList(kExt, sizeof(kExt));
}

TEST_P(TlsExtensionTest13, OddVersionList) {
  static const uint8_t kExt[] = {0x00, 0x01, 0x00};
  ConnectWithBogusVersionList(kExt, sizeof(kExt));
}

// Use the stream version number for TLS 1.3 (0x0304) in DTLS.
TEST_F(TlsConnectDatagram13, TlsVersionInDtls) {
  static const uint8_t kExt[] = {0x02, 0x03, 0x04};

  DataBuffer versions_buf(kExt, sizeof(kExt));
  MakeTlsFilter<TlsExtensionReplacer>(client_, ssl_tls13_supported_versions_xtn,
                                      versions_buf);
  ConnectExpectAlert(server_, kTlsAlertProtocolVersion);
  client_->CheckErrorCode(SSL_ERROR_PROTOCOL_VERSION_ALERT);
  server_->CheckErrorCode(SSL_ERROR_UNSUPPORTED_VERSION);
}

// TODO: this only tests extensions in server messages.  The client can extend
// Certificate messages, which is not checked here.
class TlsBogusExtensionTest : public TlsConnectTestBase,
                              public ::testing::WithParamInterface<
                                  std::tuple<SSLProtocolVariant, uint16_t>> {
 public:
  TlsBogusExtensionTest()
      : TlsConnectTestBase(std::get<0>(GetParam()), std::get<1>(GetParam())) {}

 protected:
  virtual void ConnectAndFail(uint8_t message) = 0;

  void AddFilter(uint8_t message, uint16_t extension) {
    static uint8_t empty_buf[1] = {0};
    DataBuffer empty(empty_buf, 0);
    auto filter =
        MakeTlsFilter<TlsExtensionAppender>(server_, message, extension, empty);
    if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
      filter->EnableDecryption();
    }
  }

  void Run(uint8_t message, uint16_t extension = 0xff) {
    EnsureTlsSetup();
    AddFilter(message, extension);
    ConnectAndFail(message);
  }
};

class TlsBogusExtensionTestPre13 : public TlsBogusExtensionTest {
 protected:
  void ConnectAndFail(uint8_t) override {
    ConnectExpectAlert(client_, kTlsAlertUnsupportedExtension);
  }
};

class TlsBogusExtensionTest13 : public TlsBogusExtensionTest {
 protected:
  void ConnectAndFail(uint8_t message) override {
    if (message != kTlsHandshakeServerHello) {
      ConnectExpectAlert(client_, kTlsAlertUnsupportedExtension);
      return;
    }

    FailWithAlert(kTlsAlertUnsupportedExtension);
  }

  void FailWithAlert(uint8_t alert) {
    StartConnect();
    client_->Handshake();  // ClientHello
    server_->Handshake();  // ServerHello

    client_->ExpectSendAlert(alert);
    client_->Handshake();
    if (variant_ == ssl_variant_stream) {
      server_->ExpectSendAlert(kTlsAlertUnexpectedMessage);
    }
    server_->Handshake();
  }
};

TEST_P(TlsBogusExtensionTestPre13, AddBogusExtensionServerHello) {
  Run(kTlsHandshakeServerHello);
}

TEST_P(TlsBogusExtensionTest13, AddBogusExtensionServerHello) {
  Run(kTlsHandshakeServerHello);
}

TEST_P(TlsBogusExtensionTest13, AddBogusExtensionEncryptedExtensions) {
  Run(kTlsHandshakeEncryptedExtensions);
}

TEST_P(TlsBogusExtensionTest13, AddBogusExtensionCertificate) {
  Run(kTlsHandshakeCertificate);
}

// It's perfectly valid to set unknown extensions in CertificateRequest.
TEST_P(TlsBogusExtensionTest13, AddBogusExtensionCertificateRequest) {
  server_->RequestClientAuth(false);
  AddFilter(kTlsHandshakeCertificateRequest, 0xff);
  ConnectExpectAlert(client_, kTlsAlertDecryptError);
  client_->CheckErrorCode(SEC_ERROR_BAD_SIGNATURE);
}

TEST_P(TlsBogusExtensionTest13, AddBogusExtensionHelloRetryRequest) {
  static const std::vector<SSLNamedGroup> groups = {ssl_grp_ec_secp384r1};
  server_->ConfigNamedGroups(groups);

  Run(kTlsHandshakeHelloRetryRequest);
}

TEST_P(TlsBogusExtensionTest13, AddVersionExtensionEncryptedExtensions) {
  Run(kTlsHandshakeEncryptedExtensions, ssl_tls13_supported_versions_xtn);
}

TEST_P(TlsBogusExtensionTest13, AddVersionExtensionCertificate) {
  Run(kTlsHandshakeCertificate, ssl_tls13_supported_versions_xtn);
}

TEST_P(TlsBogusExtensionTest13, AddVersionExtensionCertificateRequest) {
  server_->RequestClientAuth(false);
  Run(kTlsHandshakeCertificateRequest, ssl_tls13_supported_versions_xtn);
}

// NewSessionTicket allows unknown extensions AND it isn't protected by the
// Finished.  So adding an unknown extension doesn't cause an error.
TEST_P(TlsBogusExtensionTest13, AddBogusExtensionNewSessionTicket) {
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);

  AddFilter(kTlsHandshakeNewSessionTicket, 0xff);
  Connect();
  SendReceive();
  CheckKeys();

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  ExpectResumption(RESUME_TICKET);
  Connect();
  SendReceive();
}

TEST_P(TlsConnectStream, IncludePadding) {
  EnsureTlsSetup();
  SSL_EnableTls13GreaseEch(client_->ssl_fd(), PR_FALSE);  // Don't GREASE

  // This needs to be long enough to push a TLS 1.0 ClientHello over 255, but
  // short enough not to push a TLS 1.3 ClientHello over 511.
  static const char* long_name =
      "chickenchickenchickenchickenchickenchickenchickenchicken."
      "chickenchickenchickenchickenchickenchickenchickenchicken."
      "chickenchickenchickenchickenchicken.";
  SECStatus rv = SSL_SetURL(client_->ssl_fd(), long_name);
  EXPECT_EQ(SECSuccess, rv);

  auto capture = MakeTlsFilter<TlsExtensionCapture>(client_, ssl_padding_xtn);
  client_->StartConnect();
  client_->Handshake();
  EXPECT_TRUE(capture->captured());
}

TEST_F(TlsConnectDatagram13, Dtls13RejectLegacyCookie) {
  EnsureTlsSetup();
  MakeTlsFilter<Dtls13LegacyCookieInjector>(client_);
  ConnectExpectAlert(server_, kTlsAlertIllegalParameter);
  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
  client_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

INSTANTIATE_TEST_SUITE_P(
    ExtensionStream, TlsExtensionTestGeneric,
    ::testing::Combine(TlsConnectTestBase::kTlsVariantsStream,
                       TlsConnectTestBase::kTlsVAll));
INSTANTIATE_TEST_SUITE_P(
    ExtensionDatagram, TlsExtensionTestGeneric,
    ::testing::Combine(TlsConnectTestBase::kTlsVariantsDatagram,
                       TlsConnectTestBase::kTlsV11Plus));
INSTANTIATE_TEST_SUITE_P(ExtensionDatagramOnly, TlsExtensionTestDtls,
                         TlsConnectTestBase::kTlsV11Plus);

INSTANTIATE_TEST_SUITE_P(ExtensionTls12, TlsExtensionTest12,
                         ::testing::Combine(TlsConnectTestBase::kTlsVariantsAll,
                                            TlsConnectTestBase::kTlsV12));

INSTANTIATE_TEST_SUITE_P(ExtensionTls12Plus, TlsExtensionTest12Plus,
                         ::testing::Combine(TlsConnectTestBase::kTlsVariantsAll,
                                            TlsConnectTestBase::kTlsV12Plus));

INSTANTIATE_TEST_SUITE_P(
    ExtensionPre13Stream, TlsExtensionTestPre13,
    ::testing::Combine(TlsConnectTestBase::kTlsVariantsStream,
                       TlsConnectTestBase::kTlsV10ToV12));
INSTANTIATE_TEST_SUITE_P(ExtensionPre13Datagram, TlsExtensionTestPre13,
                         ::testing::Combine(TlsConnectTestBase::kTlsVariantsAll,
                                            TlsConnectTestBase::kTlsV11V12));

INSTANTIATE_TEST_SUITE_P(ExtensionTls13, TlsExtensionTest13,
                         TlsConnectTestBase::kTlsVariantsAll);

INSTANTIATE_TEST_SUITE_P(
    BogusExtensionStream, TlsBogusExtensionTestPre13,
    ::testing::Combine(TlsConnectTestBase::kTlsVariantsStream,
                       TlsConnectTestBase::kTlsV10ToV12));
INSTANTIATE_TEST_SUITE_P(
    BogusExtensionDatagram, TlsBogusExtensionTestPre13,
    ::testing::Combine(TlsConnectTestBase::kTlsVariantsDatagram,
                       TlsConnectTestBase::kTlsV11V12));

INSTANTIATE_TEST_SUITE_P(BogusExtension13, TlsBogusExtensionTest13,
                         ::testing::Combine(TlsConnectTestBase::kTlsVariantsAll,
                                            TlsConnectTestBase::kTlsV13));

}  // namespace nss_test
