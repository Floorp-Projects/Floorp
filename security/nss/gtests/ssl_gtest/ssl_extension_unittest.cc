/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ssl.h"
#include "ssl3prot.h"
#include "sslerr.h"
#include "sslproto.h"

#include <memory>

#include "tls_connect.h"
#include "tls_filter.h"
#include "tls_parser.h"

namespace nss_test {

class TlsExtensionTruncator : public TlsExtensionFilter {
 public:
  TlsExtensionTruncator(uint16_t extension, size_t length)
      : extension_(extension), length_(length) {}
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

class TlsExtensionDamager : public TlsExtensionFilter {
 public:
  TlsExtensionDamager(uint16_t extension, size_t index)
      : extension_(extension), index_(index) {}
  virtual PacketFilter::Action FilterExtension(uint16_t extension_type,
                                               const DataBuffer& input,
                                               DataBuffer* output) {
    if (extension_type != extension_) {
      return KEEP;
    }

    *output = input;
    output->data()[index_] += 73;  // Increment selected for maximum damage
    return CHANGE;
  }

 private:
  uint16_t extension_;
  size_t index_;
};

class TlsExtensionInjector : public TlsHandshakeFilter {
 public:
  TlsExtensionInjector(uint16_t ext, DataBuffer& data)
      : extension_(ext), data_(data) {}

  virtual PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                               const DataBuffer& input,
                                               DataBuffer* output) {
    size_t offset;
    if (header.handshake_type() == kTlsHandshakeClientHello) {
      TlsParser parser(input);
      if (!TlsExtensionFilter::FindClientHelloExtensions(&parser, header)) {
        return KEEP;
      }
      offset = parser.consumed();
    } else if (header.handshake_type() == kTlsHandshakeServerHello) {
      TlsParser parser(input);
      if (!TlsExtensionFilter::FindServerHelloExtensions(&parser)) {
        return KEEP;
      }
      offset = parser.consumed();
    } else {
      return KEEP;
    }

    *output = input;

    // Increase the size of the extensions.
    uint16_t ext_len;
    memcpy(&ext_len, output->data() + offset, sizeof(ext_len));
    ext_len = htons(ntohs(ext_len) + data_.len() + 4);
    memcpy(output->data() + offset, &ext_len, sizeof(ext_len));

    // Insert the extension type and length.
    DataBuffer type_length;
    type_length.Allocate(4);
    type_length.Write(0, extension_, 2);
    type_length.Write(2, data_.len(), 2);
    output->Splice(type_length, offset + 2);

    // Insert the payload.
    if (data_.len() > 0) {
      output->Splice(data_, offset + 6);
    }

    return CHANGE;
  }

 private:
  const uint16_t extension_;
  const DataBuffer data_;
};

class TlsExtensionAppender : public TlsHandshakeFilter {
 public:
  TlsExtensionAppender(uint16_t ext, DataBuffer& data)
      : extension_(ext), data_(data) {}

  virtual PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                               const DataBuffer& input,
                                               DataBuffer* output) {
    size_t offset;
    TlsParser parser(input);
    if (header.handshake_type() == kTlsHandshakeClientHello) {
      if (!TlsExtensionFilter::FindClientHelloExtensions(&parser, header)) {
        return KEEP;
      }
    } else if (header.handshake_type() == kTlsHandshakeServerHello) {
      if (!TlsExtensionFilter::FindServerHelloExtensions(&parser)) {
        return KEEP;
      }
    } else {
      return KEEP;
    }
    offset = parser.consumed();
    *output = input;

    uint32_t ext_len;
    if (!parser.Read(&ext_len, 2)) {
      ADD_FAILURE();
      return KEEP;
    }

    ext_len += 4 + data_.len();
    output->Write(offset, ext_len, 2);

    offset = output->len();
    offset = output->Write(offset, extension_, 2);
    WriteVariable(output, offset, data_, 2);

    return CHANGE;
  }

 private:
  const uint16_t extension_;
  const DataBuffer data_;
};

class TlsExtensionTestBase : public TlsConnectTestBase {
 protected:
  TlsExtensionTestBase(Mode mode, uint16_t version)
      : TlsConnectTestBase(mode, version) {}
  TlsExtensionTestBase(const std::string& mode, uint16_t version)
      : TlsConnectTestBase(mode, version) {}

  void ClientHelloErrorTest(std::shared_ptr<PacketFilter> filter,
                            uint8_t desc = kTlsAlertDecodeError) {
    SSLAlert alert;

    auto alert_recorder = std::make_shared<TlsAlertRecorder>();
    server_->SetPacketFilter(alert_recorder);
    client_->SetPacketFilter(filter);
    ConnectExpectFail();

    EXPECT_EQ(kTlsAlertFatal, alert_recorder->level());
    EXPECT_EQ(desc, alert_recorder->description());

    // verify no alerts received by the server
    EXPECT_EQ(0U, server_->alert_received_count());

    // verify the alert sent by the server
    EXPECT_EQ(1U, server_->alert_sent_count());
    EXPECT_TRUE(server_->GetLastAlertSent(&alert));
    EXPECT_EQ(kTlsAlertFatal, alert.level);
    EXPECT_EQ(desc, alert.description);

    // verify the alert received by the client
    EXPECT_EQ(1U, client_->alert_received_count());
    EXPECT_TRUE(client_->GetLastAlertReceived(&alert));
    EXPECT_EQ(kTlsAlertFatal, alert.level);
    EXPECT_EQ(desc, alert.description);

    // verify no alerts sent by the client
    EXPECT_EQ(0U, client_->alert_sent_count());
  }

  void ServerHelloErrorTest(std::shared_ptr<PacketFilter> filter,
                            uint8_t desc = kTlsAlertDecodeError) {
    SSLAlert alert;

    auto alert_recorder = std::make_shared<TlsAlertRecorder>();
    client_->SetPacketFilter(alert_recorder);
    server_->SetPacketFilter(filter);
    ConnectExpectFail();

    EXPECT_EQ(kTlsAlertFatal, alert_recorder->level());
    EXPECT_EQ(desc, alert_recorder->description());

    // verify no alerts received by the client
    EXPECT_EQ(0U, client_->alert_received_count());

    // verify the alert sent by the client
    EXPECT_EQ(1U, client_->alert_sent_count());
    EXPECT_TRUE(client_->GetLastAlertSent(&alert));
    EXPECT_EQ(kTlsAlertFatal, alert.level);
    EXPECT_EQ(desc, alert.description);

    // verify the alert received by the server
    EXPECT_EQ(1U, server_->alert_received_count());
    EXPECT_TRUE(server_->GetLastAlertReceived(&alert));
    EXPECT_EQ(kTlsAlertFatal, alert.level);
    EXPECT_EQ(desc, alert.description);

    // verify no alerts sent by the server
    EXPECT_EQ(0U, server_->alert_sent_count());
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
    client_->StartConnect();
    server_->StartConnect();
    client_->Handshake();  // Send ClientHello
    server_->Handshake();  // Send HRR.
    client_->SetPacketFilter(std::make_shared<TlsExtensionDropper>(type));
    Handshake();
    client_->CheckErrorCode(client_error);
    server_->CheckErrorCode(server_error);
  }
};

class TlsExtensionTestDtls : public TlsExtensionTestBase,
                             public ::testing::WithParamInterface<uint16_t> {
 public:
  TlsExtensionTestDtls() : TlsExtensionTestBase(DGRAM, GetParam()) {}
};

class TlsExtensionTest12Plus
    : public TlsExtensionTestBase,
      public ::testing::WithParamInterface<std::tuple<std::string, uint16_t>> {
 public:
  TlsExtensionTest12Plus()
      : TlsExtensionTestBase(std::get<0>(GetParam()), std::get<1>(GetParam())) {
  }
};

class TlsExtensionTest12
    : public TlsExtensionTestBase,
      public ::testing::WithParamInterface<std::tuple<std::string, uint16_t>> {
 public:
  TlsExtensionTest12()
      : TlsExtensionTestBase(std::get<0>(GetParam()), std::get<1>(GetParam())) {
  }
};

class TlsExtensionTest13 : public TlsExtensionTestBase,
                           public ::testing::WithParamInterface<std::string> {
 public:
  TlsExtensionTest13()
      : TlsExtensionTestBase(GetParam(), SSL_LIBRARY_VERSION_TLS_1_3) {}

  void ConnectWithBogusVersionList(const uint8_t* buf, size_t len) {
    DataBuffer versions_buf(buf, len);
    client_->SetPacketFilter(std::make_shared<TlsExtensionReplacer>(
        ssl_tls13_supported_versions_xtn, versions_buf));
    ConnectExpectFail();
    client_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
    server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
  }

  void ConnectWithReplacementVersionList(uint16_t version) {
    DataBuffer versions_buf;

    size_t index = versions_buf.Write(0, 2, 1);
    versions_buf.Write(index, version, 2);
    client_->SetPacketFilter(std::make_shared<TlsExtensionReplacer>(
        ssl_tls13_supported_versions_xtn, versions_buf));
    ConnectExpectFail();
  }
};

class TlsExtensionTest13Stream : public TlsExtensionTestBase {
 public:
  TlsExtensionTest13Stream()
      : TlsExtensionTestBase(STREAM, SSL_LIBRARY_VERSION_TLS_1_3) {}
};

class TlsExtensionTestGeneric
    : public TlsExtensionTestBase,
      public ::testing::WithParamInterface<std::tuple<std::string, uint16_t>> {
 public:
  TlsExtensionTestGeneric()
      : TlsExtensionTestBase(std::get<0>(GetParam()), std::get<1>(GetParam())) {
  }
};

class TlsExtensionTestPre13
    : public TlsExtensionTestBase,
      public ::testing::WithParamInterface<std::tuple<std::string, uint16_t>> {
 public:
  TlsExtensionTestPre13()
      : TlsExtensionTestBase(std::get<0>(GetParam()), std::get<1>(GetParam())) {
  }
};

TEST_P(TlsExtensionTestGeneric, DamageSniLength) {
  ClientHelloErrorTest(
      std::make_shared<TlsExtensionDamager>(ssl_server_name_xtn, 1));
}

TEST_P(TlsExtensionTestGeneric, DamageSniHostLength) {
  ClientHelloErrorTest(
      std::make_shared<TlsExtensionDamager>(ssl_server_name_xtn, 4));
}

TEST_P(TlsExtensionTestGeneric, TruncateSni) {
  ClientHelloErrorTest(
      std::make_shared<TlsExtensionTruncator>(ssl_server_name_xtn, 7));
}

// A valid extension that appears twice will be reported as unsupported.
TEST_P(TlsExtensionTestGeneric, RepeatSni) {
  DataBuffer extension;
  InitSimpleSni(&extension);
  ClientHelloErrorTest(
      std::make_shared<TlsExtensionInjector>(ssl_server_name_xtn, extension),
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
  ClientHelloErrorTest(
      std::make_shared<TlsExtensionReplacer>(ssl_server_name_xtn, extension));
}

TEST_P(TlsExtensionTestGeneric, EmptySni) {
  DataBuffer extension;
  extension.Allocate(2);
  extension.Write(0, static_cast<uint32_t>(0), 2);
  ClientHelloErrorTest(
      std::make_shared<TlsExtensionReplacer>(ssl_server_name_xtn, extension));
}

TEST_P(TlsExtensionTestGeneric, EmptyAlpnExtension) {
  EnableAlpn();
  DataBuffer extension;
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
                           ssl_app_layer_protocol_xtn, extension),
                       kTlsAlertIllegalParameter);
}

// An empty ALPN isn't considered bad, though it does lead to there being no
// protocol for the server to select.
TEST_P(TlsExtensionTestGeneric, EmptyAlpnList) {
  EnableAlpn();
  const uint8_t val[] = {0x00, 0x00};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
                           ssl_app_layer_protocol_xtn, extension),
                       kTlsAlertNoApplicationProtocol);
}

TEST_P(TlsExtensionTestGeneric, OneByteAlpn) {
  EnableAlpn();
  ClientHelloErrorTest(
      std::make_shared<TlsExtensionTruncator>(ssl_app_layer_protocol_xtn, 1));
}

TEST_P(TlsExtensionTestGeneric, AlpnMissingValue) {
  EnableAlpn();
  // This will leave the length of the second entry, but no value.
  ClientHelloErrorTest(
      std::make_shared<TlsExtensionTruncator>(ssl_app_layer_protocol_xtn, 5));
}

TEST_P(TlsExtensionTestGeneric, AlpnZeroLength) {
  EnableAlpn();
  const uint8_t val[] = {0x01, 0x61, 0x00};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      ssl_app_layer_protocol_xtn, extension));
}

TEST_P(TlsExtensionTestGeneric, AlpnMismatch) {
  const uint8_t client_alpn[] = {0x01, 0x61};
  client_->EnableAlpn(client_alpn, sizeof(client_alpn));
  const uint8_t server_alpn[] = {0x02, 0x61, 0x62};
  server_->EnableAlpn(server_alpn, sizeof(server_alpn));

  ClientHelloErrorTest(nullptr, kTlsAlertNoApplicationProtocol);
}

// Many of these tests fail in TLS 1.3 because the extension is encrypted, which
// prevents modification of the value from the ServerHello.
TEST_P(TlsExtensionTestPre13, AlpnReturnedEmptyList) {
  EnableAlpn();
  const uint8_t val[] = {0x00, 0x00};
  DataBuffer extension(val, sizeof(val));
  ServerHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      ssl_app_layer_protocol_xtn, extension));
}

TEST_P(TlsExtensionTestPre13, AlpnReturnedEmptyName) {
  EnableAlpn();
  const uint8_t val[] = {0x00, 0x01, 0x00};
  DataBuffer extension(val, sizeof(val));
  ServerHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      ssl_app_layer_protocol_xtn, extension));
}

TEST_P(TlsExtensionTestPre13, AlpnReturnedListTrailingData) {
  EnableAlpn();
  const uint8_t val[] = {0x00, 0x02, 0x01, 0x61, 0x00};
  DataBuffer extension(val, sizeof(val));
  ServerHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      ssl_app_layer_protocol_xtn, extension));
}

TEST_P(TlsExtensionTestPre13, AlpnReturnedExtraEntry) {
  EnableAlpn();
  const uint8_t val[] = {0x00, 0x04, 0x01, 0x61, 0x01, 0x62};
  DataBuffer extension(val, sizeof(val));
  ServerHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      ssl_app_layer_protocol_xtn, extension));
}

TEST_P(TlsExtensionTestPre13, AlpnReturnedBadListLength) {
  EnableAlpn();
  const uint8_t val[] = {0x00, 0x99, 0x01, 0x61, 0x00};
  DataBuffer extension(val, sizeof(val));
  ServerHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      ssl_app_layer_protocol_xtn, extension));
}

TEST_P(TlsExtensionTestPre13, AlpnReturnedBadNameLength) {
  EnableAlpn();
  const uint8_t val[] = {0x00, 0x02, 0x99, 0x61};
  DataBuffer extension(val, sizeof(val));
  ServerHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      ssl_app_layer_protocol_xtn, extension));
}

TEST_P(TlsExtensionTestDtls, SrtpShort) {
  EnableSrtp();
  ClientHelloErrorTest(
      std::make_shared<TlsExtensionTruncator>(ssl_use_srtp_xtn, 3));
}

TEST_P(TlsExtensionTestDtls, SrtpOdd) {
  EnableSrtp();
  const uint8_t val[] = {0x00, 0x01, 0xff, 0x00};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(
      std::make_shared<TlsExtensionReplacer>(ssl_use_srtp_xtn, extension));
}

TEST_P(TlsExtensionTest12Plus, SignatureAlgorithmsBadLength) {
  const uint8_t val[] = {0x00};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      ssl_signature_algorithms_xtn, extension));
}

TEST_P(TlsExtensionTest12Plus, SignatureAlgorithmsTrailingData) {
  const uint8_t val[] = {0x00, 0x02, 0x04, 0x01, 0x00};  // sha-256, rsa
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      ssl_signature_algorithms_xtn, extension));
}

TEST_P(TlsExtensionTest12Plus, SignatureAlgorithmsEmpty) {
  const uint8_t val[] = {0x00, 0x00};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      ssl_signature_algorithms_xtn, extension));
}

TEST_P(TlsExtensionTest12Plus, SignatureAlgorithmsOddLength) {
  const uint8_t val[] = {0x00, 0x01, 0x04};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      ssl_signature_algorithms_xtn, extension));
}

TEST_P(TlsExtensionTestGeneric, NoSupportedGroups) {
  ClientHelloErrorTest(
      std::make_shared<TlsExtensionDropper>(ssl_supported_groups_xtn),
      version_ < SSL_LIBRARY_VERSION_TLS_1_3 ? kTlsAlertDecryptError
                                             : kTlsAlertMissingExtension);
}

TEST_P(TlsExtensionTestGeneric, SupportedCurvesShort) {
  const uint8_t val[] = {0x00, 0x01, 0x00};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      ssl_elliptic_curves_xtn, extension));
}

TEST_P(TlsExtensionTestGeneric, SupportedCurvesBadLength) {
  const uint8_t val[] = {0x09, 0x99, 0x00, 0x00};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      ssl_elliptic_curves_xtn, extension));
}

TEST_P(TlsExtensionTestGeneric, SupportedCurvesTrailingData) {
  const uint8_t val[] = {0x00, 0x02, 0x00, 0x00, 0x00};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      ssl_elliptic_curves_xtn, extension));
}

TEST_P(TlsExtensionTestPre13, SupportedPointsEmpty) {
  const uint8_t val[] = {0x00};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      ssl_ec_point_formats_xtn, extension));
}

TEST_P(TlsExtensionTestPre13, SupportedPointsBadLength) {
  const uint8_t val[] = {0x99, 0x00, 0x00};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      ssl_ec_point_formats_xtn, extension));
}

TEST_P(TlsExtensionTestPre13, SupportedPointsTrailingData) {
  const uint8_t val[] = {0x01, 0x00, 0x00};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      ssl_ec_point_formats_xtn, extension));
}

TEST_P(TlsExtensionTestPre13, RenegotiationInfoBadLength) {
  const uint8_t val[] = {0x99};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      ssl_renegotiation_info_xtn, extension));
}

TEST_P(TlsExtensionTestPre13, RenegotiationInfoMismatch) {
  const uint8_t val[] = {0x01, 0x00};
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      ssl_renegotiation_info_xtn, extension));
}

// The extension has to contain a length.
TEST_P(TlsExtensionTestPre13, RenegotiationInfoExtensionEmpty) {
  DataBuffer extension;
  ClientHelloErrorTest(std::make_shared<TlsExtensionReplacer>(
      ssl_renegotiation_info_xtn, extension));
}

// This only works on TLS 1.2, since it relies on static RSA; otherwise libssl
// picks the wrong cipher suite.
TEST_P(TlsExtensionTest12, SignatureAlgorithmConfiguration) {
  const SSLSignatureScheme schemes[] = {ssl_sig_rsa_pss_sha512,
                                        ssl_sig_rsa_pss_sha384};

  auto capture =
      std::make_shared<TlsExtensionCapture>(ssl_signature_algorithms_xtn);
  client_->SetSignatureSchemes(schemes, PR_ARRAY_SIZE(schemes));
  client_->SetPacketFilter(capture);
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

// Temporary test to verify that we choke on an empty ClientKeyShare.
// This test will fail when we implement HelloRetryRequest.
TEST_P(TlsExtensionTest13, EmptyClientKeyShare) {
  ClientHelloErrorTest(
      std::make_shared<TlsExtensionTruncator>(ssl_tls13_key_share_xtn, 2),
      kTlsAlertHandshakeFailure);
}

// These tests only work in stream mode because the client sends a
// cleartext alert which causes a MAC error on the server. With
// stream this causes handshake failure but with datagram, the
// packet gets dropped.
TEST_F(TlsExtensionTest13Stream, DropServerKeyShare) {
  EnsureTlsSetup();
  server_->SetPacketFilter(
      std::make_shared<TlsExtensionDropper>(ssl_tls13_key_share_xtn));
  ConnectExpectFail();
  EXPECT_EQ(SSL_ERROR_MISSING_KEY_SHARE, client_->error_code());
  EXPECT_EQ(SSL_ERROR_BAD_MAC_READ, server_->error_code());
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
  server_->SetPacketFilter(
      std::make_shared<TlsExtensionReplacer>(ssl_tls13_key_share_xtn, buf));
  ConnectExpectFail();
  EXPECT_EQ(SSL_ERROR_RX_MALFORMED_KEY_SHARE, client_->error_code());
  EXPECT_EQ(SSL_ERROR_BAD_MAC_READ, server_->error_code());
}

// TODO(ekr@rtfm.com): This is the wrong error code. See bug 1307269.
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
  server_->SetPacketFilter(
      std::make_shared<TlsExtensionReplacer>(ssl_tls13_key_share_xtn, buf));
  ConnectExpectFail();
  EXPECT_EQ(SSL_ERROR_MISSING_KEY_SHARE, client_->error_code());
  EXPECT_EQ(SSL_ERROR_BAD_MAC_READ, server_->error_code());
}

TEST_F(TlsExtensionTest13Stream, AddServerSignatureAlgorithmsOnResumption) {
  SetupForResume();
  DataBuffer empty;
  server_->SetPacketFilter(std::make_shared<TlsExtensionInjector>(
      ssl_signature_algorithms_xtn, empty));
  ConnectExpectFail();
  EXPECT_EQ(SSL_ERROR_EXTENSION_DISALLOWED_FOR_VERSION, client_->error_code());
  EXPECT_EQ(SSL_ERROR_BAD_MAC_READ, server_->error_code());
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
  TlsPreSharedKeyReplacer(TlsPreSharedKeyReplacerFunc function)
      : identities_(), binders_(), function_(function) {}

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

  client_->SetPacketFilter(std::make_shared<TlsPreSharedKeyReplacer>([](
      TlsPreSharedKeyReplacer* r) { r->identities_[0].identity.Truncate(0); }));
  ConnectExpectFail();
  client_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
}

// Flip the first byte of the binder.
TEST_F(TlsExtensionTest13Stream, ResumeIncorrectBinderValue) {
  SetupForResume();

  client_->SetPacketFilter(
      std::make_shared<TlsPreSharedKeyReplacer>([](TlsPreSharedKeyReplacer* r) {
        r->binders_[0].Write(0, r->binders_[0].data()[0] ^ 0xff, 1);
      }));
  ConnectExpectFail();
  client_->CheckErrorCode(SSL_ERROR_DECRYPT_ERROR_ALERT);
  server_->CheckErrorCode(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE);
}

// Extend the binder by one.
TEST_F(TlsExtensionTest13Stream, ResumeIncorrectBinderLength) {
  SetupForResume();

  client_->SetPacketFilter(
      std::make_shared<TlsPreSharedKeyReplacer>([](TlsPreSharedKeyReplacer* r) {
        r->binders_[0].Write(r->binders_[0].len(), 0xff, 1);
      }));
  ConnectExpectFail();
  client_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
}

// Binders must be at least 32 bytes.
TEST_F(TlsExtensionTest13Stream, ResumeBinderTooShort) {
  SetupForResume();

  client_->SetPacketFilter(std::make_shared<TlsPreSharedKeyReplacer>(
      [](TlsPreSharedKeyReplacer* r) { r->binders_[0].Truncate(31); }));
  ConnectExpectFail();
  client_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
}

// Duplicate the identity and binder. This will fail with an error
// processing the binder (because we extended the identity list.)
TEST_F(TlsExtensionTest13Stream, ResumeTwoPsks) {
  SetupForResume();

  client_->SetPacketFilter(
      std::make_shared<TlsPreSharedKeyReplacer>([](TlsPreSharedKeyReplacer* r) {
        r->identities_.push_back(r->identities_[0]);
        r->binders_.push_back(r->binders_[0]);
      }));
  ConnectExpectFail();
  client_->CheckErrorCode(SSL_ERROR_DECRYPT_ERROR_ALERT);
  server_->CheckErrorCode(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE);
}

// The next two tests have mismatches in the number of identities
// and binders. This generates an illegal parameter alert.
TEST_F(TlsExtensionTest13Stream, ResumeTwoIdentitiesOneBinder) {
  SetupForResume();

  client_->SetPacketFilter(
      std::make_shared<TlsPreSharedKeyReplacer>([](TlsPreSharedKeyReplacer* r) {
        r->identities_.push_back(r->identities_[0]);
      }));
  ConnectExpectFail();
  client_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
}

TEST_F(TlsExtensionTest13Stream, ResumeOneIdentityTwoBinders) {
  SetupForResume();

  client_->SetPacketFilter(std::make_shared<TlsPreSharedKeyReplacer>([](
      TlsPreSharedKeyReplacer* r) { r->binders_.push_back(r->binders_[0]); }));
  ConnectExpectFail();
  client_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
}

TEST_F(TlsExtensionTest13Stream, ResumePskExtensionNotLast) {
  SetupForResume();

  const uint8_t empty_buf[] = {0};
  DataBuffer empty(empty_buf, 0);
  client_->SetPacketFilter(
      // Inject an unused extension.
      std::make_shared<TlsExtensionAppender>(0xffff, empty));
  ConnectExpectFail();
  client_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
}

TEST_F(TlsExtensionTest13Stream, ResumeNoKeModes) {
  SetupForResume();

  DataBuffer empty;
  client_->SetPacketFilter(std::make_shared<TlsExtensionDropper>(
      ssl_tls13_psk_key_exchange_modes_xtn));
  ConnectExpectFail();
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
  client_->SetPacketFilter(std::make_shared<TlsExtensionReplacer>(
      ssl_tls13_psk_key_exchange_modes_xtn, modes));
  ConnectExpectFail();
  client_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
  server_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
}

TEST_P(TlsExtensionTest13, NoKeModesIfResumptionOff) {
  ConfigureSessionCache(RESUME_NONE, RESUME_NONE);
  auto capture = std::make_shared<TlsExtensionCapture>(
      ssl_tls13_psk_key_exchange_modes_xtn);
  client_->SetPacketFilter(capture);
  Connect();
  EXPECT_FALSE(capture->captured());
}

// In these tests, we downgrade to TLS 1.2, causing the
// server to negotiate TLS 1.2.
// 1. Both sides only support TLS 1.3, so we get a cipher version
//    error.
TEST_P(TlsExtensionTest13, RemoveTls13FromVersionList) {
  ConnectWithReplacementVersionList(SSL_LIBRARY_VERSION_TLS_1_2);
  client_->CheckErrorCode(SSL_ERROR_PROTOCOL_VERSION_ALERT);
  server_->CheckErrorCode(SSL_ERROR_UNSUPPORTED_VERSION);
}

// 2. Server supports 1.2 and 1.3, client supports 1.2, so we
//    can't negotiate any ciphers.
TEST_P(TlsExtensionTest13, RemoveTls13FromVersionListServerV12) {
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  ConnectWithReplacementVersionList(SSL_LIBRARY_VERSION_TLS_1_2);
  client_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
  server_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
}

// 3. Server supports 1.2 and 1.3, client supports 1.2 and 1.3
// but advertises 1.2 (because we changed things).
TEST_P(TlsExtensionTest13, RemoveTls13FromVersionListBothV12) {
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  ConnectWithReplacementVersionList(SSL_LIBRARY_VERSION_TLS_1_2);
#ifndef TLS_1_3_DRAFT_VERSION
  client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_SERVER_HELLO);
  server_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
#else
  client_->CheckErrorCode(SSL_ERROR_DECRYPT_ERROR_ALERT);
  server_->CheckErrorCode(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE);
#endif
}

TEST_P(TlsExtensionTest13, HrrThenRemoveSignatureAlgorithms) {
  HrrThenRemoveExtensionsTest(ssl_signature_algorithms_xtn,
                              SSL_ERROR_MISSING_EXTENSION_ALERT,
                              SSL_ERROR_MISSING_SIGNATURE_ALGORITHMS_EXTENSION);
}

TEST_P(TlsExtensionTest13, HrrThenRemoveKeyShare) {
  HrrThenRemoveExtensionsTest(ssl_tls13_key_share_xtn,
                              SSL_ERROR_ILLEGAL_PARAMETER_ALERT,
                              SSL_ERROR_BAD_2ND_CLIENT_HELLO);
}

TEST_P(TlsExtensionTest13, HrrThenRemoveSupportedGroups) {
  HrrThenRemoveExtensionsTest(ssl_supported_groups_xtn,
                              SSL_ERROR_MISSING_EXTENSION_ALERT,
                              SSL_ERROR_MISSING_SUPPORTED_GROUPS_EXTENSION);
}

TEST_P(TlsExtensionTest13, EmptyVersionList) {
  static const uint8_t ext[] = {0x00, 0x00};
  ConnectWithBogusVersionList(ext, sizeof(ext));
}

TEST_P(TlsExtensionTest13, OddVersionList) {
  static const uint8_t ext[] = {0x00, 0x01, 0x00};
  ConnectWithBogusVersionList(ext, sizeof(ext));
}

INSTANTIATE_TEST_CASE_P(ExtensionStream, TlsExtensionTestGeneric,
                        ::testing::Combine(TlsConnectTestBase::kTlsModesStream,
                                           TlsConnectTestBase::kTlsVAll));
INSTANTIATE_TEST_CASE_P(ExtensionDatagram, TlsExtensionTestGeneric,
                        ::testing::Combine(TlsConnectTestBase::kTlsModesAll,
                                           TlsConnectTestBase::kTlsV11Plus));
INSTANTIATE_TEST_CASE_P(ExtensionDatagramOnly, TlsExtensionTestDtls,
                        TlsConnectTestBase::kTlsV11Plus);

INSTANTIATE_TEST_CASE_P(ExtensionTls12Plus, TlsExtensionTest12Plus,
                        ::testing::Combine(TlsConnectTestBase::kTlsModesAll,
                                           TlsConnectTestBase::kTlsV12Plus));

INSTANTIATE_TEST_CASE_P(ExtensionPre13Stream, TlsExtensionTestPre13,
                        ::testing::Combine(TlsConnectTestBase::kTlsModesStream,
                                           TlsConnectTestBase::kTlsV10ToV12));
INSTANTIATE_TEST_CASE_P(ExtensionPre13Datagram, TlsExtensionTestPre13,
                        ::testing::Combine(TlsConnectTestBase::kTlsModesAll,
                                           TlsConnectTestBase::kTlsV11V12));

INSTANTIATE_TEST_CASE_P(ExtensionTls13, TlsExtensionTest13,
                        TlsConnectTestBase::kTlsModesAll);

}  // namespace nspr_test
