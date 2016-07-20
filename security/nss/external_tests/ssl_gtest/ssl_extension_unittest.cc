/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"

#include <memory>

#include "tls_parser.h"
#include "tls_filter.h"
#include "tls_connect.h"

namespace nss_test {

class TlsExtensionDropper : public TlsExtensionFilter {
 public:
  TlsExtensionDropper(uint16_t extension)
      : extension_(extension) {}
  virtual PacketFilter::Action FilterExtension(
      uint16_t extension_type, const DataBuffer&, DataBuffer*) {
    if (extension_type == extension_) {
      return DROP;
    }
    return KEEP;
  }
 private:
    uint16_t extension_;
};

class TlsExtensionTruncator : public TlsExtensionFilter {
 public:
  TlsExtensionTruncator(uint16_t extension, size_t length)
      : extension_(extension), length_(length) {}
  virtual PacketFilter::Action FilterExtension(
      uint16_t extension_type, const DataBuffer& input, DataBuffer* output) {
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
  virtual PacketFilter::Action FilterExtension(
      uint16_t extension_type, const DataBuffer& input, DataBuffer* output) {
    if (extension_type != extension_) {
      return KEEP;
    }

    *output = input;
    output->data()[index_] += 73; // Increment selected for maximum damage
    return CHANGE;
  }
 private:
  uint16_t extension_;
  size_t index_;
};

class TlsExtensionReplacer : public TlsExtensionFilter {
 public:
  TlsExtensionReplacer(uint16_t extension, const DataBuffer& data)
      : extension_(extension), data_(data) {}
  virtual PacketFilter::Action FilterExtension(
      uint16_t extension_type, const DataBuffer& input, DataBuffer* output) {
    if (extension_type != extension_) {
      return KEEP;
    }

    *output = data_;
    return CHANGE;
  }
 private:
  const uint16_t extension_;
  const DataBuffer data_;
};

class TlsExtensionInjector : public TlsHandshakeFilter {
 public:
  TlsExtensionInjector(uint16_t ext, DataBuffer& data)
      : extension_(ext), data_(data) {}

  virtual PacketFilter::Action FilterHandshake(
      const HandshakeHeader& header,
      const DataBuffer& input, DataBuffer* output) {
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
    uint16_t* len_addr = reinterpret_cast<uint16_t*>(output->data() + offset);
    *len_addr = htons(ntohs(*len_addr) + data_.len() + 4);

    // Insert the extension type and length.
    DataBuffer type_length;
    type_length.Allocate(4);
    type_length.Write(0, extension_, 2);
    type_length.Write(2, data_.len(), 2);
    output->Splice(type_length, offset + 2);

    // Insert the payload.
    output->Splice(data_, offset + 6);

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

  void ClientHelloErrorTest(PacketFilter* filter,
                            uint8_t alert = kTlsAlertDecodeError) {
    auto alert_recorder = new TlsAlertRecorder();
    server_->SetPacketFilter(alert_recorder);
    if (filter) {
      client_->SetPacketFilter(filter);
    }
    ConnectExpectFail();
    EXPECT_EQ(kTlsAlertFatal, alert_recorder->level());
    EXPECT_EQ(alert, alert_recorder->description());
  }

  void ServerHelloErrorTest(PacketFilter* filter,
                            uint8_t alert = kTlsAlertDecodeError) {
    auto alert_recorder = new TlsAlertRecorder();
    client_->SetPacketFilter(alert_recorder);
    if (filter) {
      server_->SetPacketFilter(filter);
    }
    ConnectExpectFail();
    EXPECT_EQ(kTlsAlertFatal, alert_recorder->level());
    EXPECT_EQ(alert, alert_recorder->description());
  }

  static void InitSimpleSni(DataBuffer* extension) {
    const char* name = "host.name";
    const size_t namelen = PL_strlen(name);
    extension->Allocate(namelen + 5);
    extension->Write(0, namelen + 3, 2);
    extension->Write(2, static_cast<uint32_t>(0), 1); // 0 == hostname
    extension->Write(3, namelen, 2);
    extension->Write(5, reinterpret_cast<const uint8_t*>(name), namelen);
  }
};

class TlsExtensionTestDtls
  : public TlsExtensionTestBase,
    public ::testing::WithParamInterface<uint16_t> {
 public:
  TlsExtensionTestDtls() : TlsExtensionTestBase(DGRAM, GetParam()) {}
};

class TlsExtensionTest12Plus
  : public TlsExtensionTestBase,
    public ::testing::WithParamInterface<std::tuple<std::string, uint16_t>> {
 public:
  TlsExtensionTest12Plus()
    : TlsExtensionTestBase(TlsConnectTestBase::ToMode((std::get<0>(GetParam()))),
                           std::get<1>(GetParam())) {}
};

class TlsExtensionTest12
  : public TlsExtensionTestBase,
    public ::testing::WithParamInterface<std::tuple<std::string, uint16_t>> {
 public:
  TlsExtensionTest12()
    : TlsExtensionTestBase(TlsConnectTestBase::ToMode((std::get<0>(GetParam()))),
                           std::get<1>(GetParam())) {}
};

class TlsExtensionTest13
  : public TlsExtensionTestBase,
    public ::testing::WithParamInterface<std::string> {
 public:
  TlsExtensionTest13()
    : TlsExtensionTestBase(TlsConnectTestBase::ToMode(GetParam()),
                           SSL_LIBRARY_VERSION_TLS_1_3) {}
};

class TlsExtensionTest13Stream
    : public TlsExtensionTestBase {
 public:
  TlsExtensionTest13Stream()
    : TlsExtensionTestBase(STREAM,
                           SSL_LIBRARY_VERSION_TLS_1_3) {}
};

class TlsExtensionTestGeneric
  : public TlsExtensionTestBase,
    public ::testing::WithParamInterface<std::tuple<std::string, uint16_t>> {
 public:
  TlsExtensionTestGeneric()
    : TlsExtensionTestBase(TlsConnectTestBase::ToMode((std::get<0>(GetParam()))),
                           std::get<1>(GetParam())) {}
};

class TlsExtensionTestPre13
  : public TlsExtensionTestBase,
    public ::testing::WithParamInterface<std::tuple<std::string, uint16_t>> {
 public:
  TlsExtensionTestPre13()
    : TlsExtensionTestBase(TlsConnectTestBase::ToMode((std::get<0>(GetParam()))),
                           std::get<1>(GetParam())) {}
};

TEST_P(TlsExtensionTestGeneric, DamageSniLength) {
  ClientHelloErrorTest(new TlsExtensionDamager(ssl_server_name_xtn, 1));
}

TEST_P(TlsExtensionTestGeneric, DamageSniHostLength) {
  ClientHelloErrorTest(new TlsExtensionDamager(ssl_server_name_xtn, 4));
}

TEST_P(TlsExtensionTestGeneric, TruncateSni) {
  ClientHelloErrorTest(new TlsExtensionTruncator(ssl_server_name_xtn, 7));
}

// A valid extension that appears twice will be reported as unsupported.
TEST_P(TlsExtensionTestGeneric, RepeatSni) {
  DataBuffer extension;
  InitSimpleSni(&extension);
  ClientHelloErrorTest(new TlsExtensionInjector(ssl_server_name_xtn, extension),
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
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_server_name_xtn, extension));
}

TEST_P(TlsExtensionTestGeneric, EmptySni) {
  DataBuffer extension;
  extension.Allocate(2);
  extension.Write(0, static_cast<uint32_t>(0), 2);
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_server_name_xtn, extension));
}

TEST_P(TlsExtensionTestGeneric, EmptyAlpnExtension) {
  EnableAlpn();
  DataBuffer extension;
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_app_layer_protocol_xtn, extension),
                       kTlsAlertIllegalParameter);
}

// An empty ALPN isn't considered bad, though it does lead to there being no
// protocol for the server to select.
TEST_P(TlsExtensionTestGeneric, EmptyAlpnList) {
  EnableAlpn();
  const uint8_t val[] = { 0x00, 0x00 };
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_app_layer_protocol_xtn, extension),
                       kTlsAlertNoApplicationProtocol);
}

TEST_P(TlsExtensionTestGeneric, OneByteAlpn) {
  EnableAlpn();
  ClientHelloErrorTest(new TlsExtensionTruncator(ssl_app_layer_protocol_xtn, 1));
}

TEST_P(TlsExtensionTestGeneric, AlpnMissingValue) {
  EnableAlpn();
  // This will leave the length of the second entry, but no value.
  ClientHelloErrorTest(new TlsExtensionTruncator(ssl_app_layer_protocol_xtn, 5));
}

TEST_P(TlsExtensionTestGeneric, AlpnZeroLength) {
  EnableAlpn();
  const uint8_t val[] = { 0x01, 0x61, 0x00 };
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_app_layer_protocol_xtn, extension));
}

TEST_P(TlsExtensionTestGeneric, AlpnMismatch) {
  const uint8_t client_alpn[] = { 0x01, 0x61 };
  client_->EnableAlpn(client_alpn, sizeof(client_alpn));
  const uint8_t server_alpn[] = { 0x02, 0x61, 0x62 };
  server_->EnableAlpn(server_alpn, sizeof(server_alpn));

  ClientHelloErrorTest(nullptr, kTlsAlertNoApplicationProtocol);
}

// Many of these tests fail in TLS 1.3 because the extension is encrypted, which
// prevents modification of the value from the ServerHello.
TEST_P(TlsExtensionTestPre13, AlpnReturnedEmptyList) {
  EnableAlpn();
  const uint8_t val[] = { 0x00, 0x00 };
  DataBuffer extension(val, sizeof(val));
  ServerHelloErrorTest(new TlsExtensionReplacer(ssl_app_layer_protocol_xtn, extension));
}

TEST_P(TlsExtensionTestPre13, AlpnReturnedEmptyName) {
  EnableAlpn();
  const uint8_t val[] = { 0x00, 0x01, 0x00 };
  DataBuffer extension(val, sizeof(val));
  ServerHelloErrorTest(new TlsExtensionReplacer(ssl_app_layer_protocol_xtn, extension));
}

TEST_P(TlsExtensionTestPre13, AlpnReturnedListTrailingData) {
  EnableAlpn();
  const uint8_t val[] = { 0x00, 0x02, 0x01, 0x61, 0x00 };
  DataBuffer extension(val, sizeof(val));
  ServerHelloErrorTest(new TlsExtensionReplacer(ssl_app_layer_protocol_xtn, extension));
}

TEST_P(TlsExtensionTestPre13, AlpnReturnedExtraEntry) {
  EnableAlpn();
  const uint8_t val[] = { 0x00, 0x04, 0x01, 0x61, 0x01, 0x62 };
  DataBuffer extension(val, sizeof(val));
  ServerHelloErrorTest(new TlsExtensionReplacer(ssl_app_layer_protocol_xtn, extension));
}

TEST_P(TlsExtensionTestPre13, AlpnReturnedBadListLength) {
  EnableAlpn();
  const uint8_t val[] = { 0x00, 0x99, 0x01, 0x61, 0x00 };
  DataBuffer extension(val, sizeof(val));
  ServerHelloErrorTest(new TlsExtensionReplacer(ssl_app_layer_protocol_xtn, extension));
}

TEST_P(TlsExtensionTestPre13, AlpnReturnedBadNameLength) {
  EnableAlpn();
  const uint8_t val[] = { 0x00, 0x02, 0x99, 0x61 };
  DataBuffer extension(val, sizeof(val));
  ServerHelloErrorTest(new TlsExtensionReplacer(ssl_app_layer_protocol_xtn, extension));
}

TEST_P(TlsExtensionTestDtls, SrtpShort) {
  EnableSrtp();
  ClientHelloErrorTest(new TlsExtensionTruncator(ssl_use_srtp_xtn, 3));
}

TEST_P(TlsExtensionTestDtls, SrtpOdd) {
  EnableSrtp();
  const uint8_t val[] = { 0x00, 0x01, 0xff, 0x00 };
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_use_srtp_xtn, extension));
}

TEST_P(TlsExtensionTest12Plus, SignatureAlgorithmsBadLength) {
  const uint8_t val[] = { 0x00 };
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_signature_algorithms_xtn,
                                                extension));
}

TEST_P(TlsExtensionTest12Plus, SignatureAlgorithmsTrailingData) {
  const uint8_t val[] = { 0x00, 0x02, 0x04, 0x01, 0x00 }; // sha-256, rsa
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_signature_algorithms_xtn,
                                                extension));
}

TEST_P(TlsExtensionTest12Plus, SignatureAlgorithmsEmpty) {
  const uint8_t val[] = { 0x00, 0x00 };
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_signature_algorithms_xtn,
                                                extension));
}

TEST_P(TlsExtensionTest12Plus, SignatureAlgorithmsOddLength) {
  const uint8_t val[] = { 0x00, 0x01, 0x04 };
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_signature_algorithms_xtn,
                                                extension));
}

// The extension handling ignores unsupported hashes, so breaking that has no
// effect on success rates.  However, ssl3_SendServerKeyExchange catches an
// unsupported signature algorithm.

// This actually fails with a decryption error (fatal alert 51).  That's a bad
// to fail, since any tampering with the handshake will trigger that alert when
// verifying the Finished message.  Thus, this test is disabled until this error
// is turned into an alert.
TEST_P(TlsExtensionTest12Plus, DISABLED_SignatureAlgorithmsSigUnsupported) {
  const uint8_t val[] = { 0x00, 0x02, 0x04, 0x99 };
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_signature_algorithms_xtn,
                                                extension));
}

TEST_P(TlsExtensionTestGeneric, SupportedCurvesShort) {
  const uint8_t val[] = { 0x00, 0x01, 0x00 };
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_elliptic_curves_xtn,
                                                extension));
}

TEST_P(TlsExtensionTestGeneric, SupportedCurvesBadLength) {
  const uint8_t val[] = { 0x09, 0x99, 0x00, 0x00 };
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_elliptic_curves_xtn,
                                                extension));
}

TEST_P(TlsExtensionTestGeneric, SupportedCurvesTrailingData) {
  const uint8_t val[] = { 0x00, 0x02, 0x00, 0x00, 0x00 };
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_elliptic_curves_xtn,
                                                extension));
}

TEST_P(TlsExtensionTestPre13, SupportedPointsEmpty) {
  const uint8_t val[] = { 0x00 };
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_ec_point_formats_xtn,
                                                extension));
}

TEST_P(TlsExtensionTestPre13, SupportedPointsBadLength) {
  const uint8_t val[] = { 0x99, 0x00, 0x00 };
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_ec_point_formats_xtn,
                                                extension));
}

TEST_P(TlsExtensionTestPre13, SupportedPointsTrailingData) {
  const uint8_t val[] = { 0x01, 0x00, 0x00 };
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_ec_point_formats_xtn,
                                                extension));
}

TEST_P(TlsExtensionTestPre13, RenegotiationInfoBadLength) {
  const uint8_t val[] = { 0x99 };
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_renegotiation_info_xtn,
                                                extension));
}

TEST_P(TlsExtensionTestPre13, RenegotiationInfoMismatch) {
  const uint8_t val[] = { 0x01, 0x00 };
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_renegotiation_info_xtn,
                                                extension));
}

// The extension has to contain a length.
TEST_P(TlsExtensionTestPre13, RenegotiationInfoExtensionEmpty) {
  DataBuffer extension;
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_renegotiation_info_xtn,
                                                extension));
}

// This only works on TLS 1.2, since it relies on static RSA; otherwise libssl
// picks the wrong cipher suite.
TEST_P(TlsExtensionTest12, SignatureAlgorithmConfiguration) {
  const SSLSignatureAndHashAlg algorithms[] = {
    {ssl_hash_sha512, ssl_sign_rsa},
    {ssl_hash_sha384, ssl_sign_ecdsa}
  };

  TlsExtensionCapture *capture =
    new TlsExtensionCapture(ssl_signature_algorithms_xtn);
  client_->SetSignatureAlgorithms(algorithms, PR_ARRAY_SIZE(algorithms));
  client_->SetPacketFilter(capture);
  EnableOnlyStaticRsaCiphers();
  Connect();

  const DataBuffer& ext = capture->extension();
  EXPECT_EQ(2 + PR_ARRAY_SIZE(algorithms) * 2, ext.len());
  for (size_t i = 0, cursor = 2;
       i < PR_ARRAY_SIZE(algorithms) && cursor < ext.len();
       ++i) {
    uint32_t v;
    EXPECT_TRUE(ext.Read(cursor++, 1, &v));
    EXPECT_EQ(algorithms[i].hashAlg, static_cast<SSLHashType>(v));
    EXPECT_TRUE(ext.Read(cursor++, 1, &v));
    EXPECT_EQ(algorithms[i].sigAlg, static_cast<SSLSignType>(v));
  }
}

/*
 * Tests for Certificate Transparency (RFC 6962)
 * These don't work with TLS 1.3: see bug 1252745.
 */

// Helper class - stores signed certificate timestamps as provided
// by the relevant callbacks on the client.
class SignedCertificateTimestampsExtractor {
 public:
  SignedCertificateTimestampsExtractor(TlsAgent& client) {
    client.SetAuthCertificateCallback(
      [&](TlsAgent& agent, PRBool checksig, PRBool isServer) -> SECStatus {
        const SECItem *scts = SSL_PeerSignedCertTimestamps(agent.ssl_fd());
        EXPECT_TRUE(scts);
        if (!scts) {
          return SECFailure;
        }
        auth_timestamps_.reset(new DataBuffer(scts->data, scts->len));
        return SECSuccess;
      }
    );
    client.SetHandshakeCallback(
      [&](TlsAgent& agent) {
        const SECItem *scts = SSL_PeerSignedCertTimestamps(agent.ssl_fd());
        ASSERT_TRUE(scts);
        handshake_timestamps_.reset(new DataBuffer(scts->data, scts->len));
      }
    );
  }

  void assertTimestamps(const DataBuffer& timestamps) {
    EXPECT_TRUE(auth_timestamps_);
    EXPECT_EQ(timestamps, *auth_timestamps_);

    EXPECT_TRUE(handshake_timestamps_);
    EXPECT_EQ(timestamps, *handshake_timestamps_);
  }

 private:
  std::unique_ptr<DataBuffer> auth_timestamps_;
  std::unique_ptr<DataBuffer> handshake_timestamps_;
};

// Test timestamps extraction during a successful handshake.
TEST_P(TlsExtensionTestPre13, SignedCertificateTimestampsHandshake) {
  uint8_t val[] = { 0x01, 0x23, 0x45, 0x67, 0x89 };
  const SECItem si_timestamps = { siBuffer, val, sizeof(val) };
  const DataBuffer timestamps(val, sizeof(val));

  server_->StartConnect();
  ASSERT_EQ(SECSuccess,
    SSL_SetSignedCertTimestamps(server_->ssl_fd(),
      &si_timestamps, ssl_kea_rsa));

  client_->StartConnect();
  ASSERT_EQ(SECSuccess,
    SSL_OptionSet(client_->ssl_fd(),
      SSL_ENABLE_SIGNED_CERT_TIMESTAMPS, PR_TRUE));

  SignedCertificateTimestampsExtractor timestamps_extractor(*client_);
  Handshake();
  CheckConnected();
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    timestamps_extractor.assertTimestamps(timestamps);
  }
  const SECItem* c_timestamps = SSL_PeerSignedCertTimestamps(client_->ssl_fd());
  ASSERT_EQ(SECEqual, SECITEM_CompareItem(&si_timestamps, c_timestamps));
}

// Test SSL_PeerSignedCertTimestamps returning zero-length SECItem
// when the client / the server / both have not enabled the feature.
TEST_P(TlsExtensionTestPre13, SignedCertificateTimestampsInactiveClient) {
  uint8_t val[] = { 0x01, 0x23, 0x45, 0x67, 0x89 };
  const SECItem si_timestamps = { siBuffer, val, sizeof(val) };

  server_->StartConnect();
  ASSERT_EQ(SECSuccess,
    SSL_SetSignedCertTimestamps(server_->ssl_fd(),
      &si_timestamps, ssl_kea_rsa));

  client_->StartConnect();

  SignedCertificateTimestampsExtractor timestamps_extractor(*client_);
  Handshake();
  CheckConnected();
  timestamps_extractor.assertTimestamps(DataBuffer());
}

TEST_P(TlsExtensionTestPre13, SignedCertificateTimestampsInactiveServer) {
  server_->StartConnect();

  client_->StartConnect();
  ASSERT_EQ(SECSuccess,
    SSL_OptionSet(client_->ssl_fd(),
      SSL_ENABLE_SIGNED_CERT_TIMESTAMPS, PR_TRUE));

  SignedCertificateTimestampsExtractor timestamps_extractor(*client_);
  Handshake();
  CheckConnected();
  timestamps_extractor.assertTimestamps(DataBuffer());
}

TEST_P(TlsExtensionTestPre13, SignedCertificateTimestampsInactiveBoth) {
  server_->StartConnect();
  client_->StartConnect();

  SignedCertificateTimestampsExtractor timestamps_extractor(*client_);
  Handshake();
  CheckConnected();
  timestamps_extractor.assertTimestamps(DataBuffer());
}


// Temporary test to verify that we choke on an empty ClientKeyShare.
// This test will fail when we implement HelloRetryRequest.
TEST_P(TlsExtensionTest13, EmptyClientKeyShare) {
  ClientHelloErrorTest(new TlsExtensionTruncator(ssl_tls13_key_share_xtn, 2),
                       kTlsAlertHandshakeFailure);
}

TEST_P(TlsExtensionTest13, DropDraftVersion) {
  EnsureTlsSetup();
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  client_->SetPacketFilter(
      new TlsExtensionDropper(ssl_tls13_draft_version_xtn));
  ConnectExpectFail();
  // This will still fail (we can't just modify ClientHello without consequence)
  // but the error is discovered later.
  EXPECT_EQ(SSL_ERROR_DECRYPT_ERROR_ALERT, client_->error_code());
  EXPECT_EQ(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE, server_->error_code());
}

TEST_P(TlsExtensionTest13, DropDraftVersionAndFail) {
  EnsureTlsSetup();
  // Since this is setup as TLS 1.3 only, expect the handshake to fail rather
  // than just falling back to TLS 1.2.
  client_->SetPacketFilter(
      new TlsExtensionDropper(ssl_tls13_draft_version_xtn));
  ConnectExpectFail();
  EXPECT_EQ(SSL_ERROR_PROTOCOL_VERSION_ALERT, client_->error_code());
  EXPECT_EQ(SSL_ERROR_UNSUPPORTED_VERSION, server_->error_code());
}

TEST_P(TlsExtensionTest13, ModifyDraftVersionAndFail) {
  EnsureTlsSetup();
  // As above, dropping back to 1.2 fails.
  client_->SetPacketFilter(
      new TlsExtensionDamager(ssl_tls13_draft_version_xtn, 1));
  ConnectExpectFail();
  EXPECT_EQ(SSL_ERROR_PROTOCOL_VERSION_ALERT, client_->error_code());
  EXPECT_EQ(SSL_ERROR_UNSUPPORTED_VERSION, server_->error_code());
}

#ifdef NSS_ENABLE_TLS_1_3
// This test only works with TLS because the MAC error causes a
// timeout on the server.
TEST_F(TlsExtensionTest13Stream, DropServerKeyShare) {
  EnsureTlsSetup();
  server_->SetPacketFilter(
      new TlsExtensionDropper(ssl_tls13_key_share_xtn));
  ConnectExpectFail();
  EXPECT_EQ(SSL_ERROR_MISSING_KEY_SHARE, client_->error_code());
  // We are trying to decrypt but we can't. Kind of a screwy error
  // from the TLS 1.3 stack.
  EXPECT_EQ(SSL_ERROR_BAD_MAC_READ, server_->error_code());
}
#endif

INSTANTIATE_TEST_CASE_P(ExtensionStream, TlsExtensionTestGeneric,
                        ::testing::Combine(
                          TlsConnectTestBase::kTlsModesStream,
                          TlsConnectTestBase::kTlsVAll));
INSTANTIATE_TEST_CASE_P(ExtensionDatagram, TlsExtensionTestGeneric,
                        ::testing::Combine(
                          TlsConnectTestBase::kTlsModesAll,
                          TlsConnectTestBase::kTlsV11Plus));
INSTANTIATE_TEST_CASE_P(ExtensionDatagramOnly, TlsExtensionTestDtls,
                        TlsConnectTestBase::kTlsV11Plus);

INSTANTIATE_TEST_CASE_P(ExtensionTls12Plus, TlsExtensionTest12Plus,
                        ::testing::Combine(
                          TlsConnectTestBase::kTlsModesAll,
                          TlsConnectTestBase::kTlsV12Plus));

INSTANTIATE_TEST_CASE_P(ExtensionPre13Stream, TlsExtensionTestPre13,
                        ::testing::Combine(
                          TlsConnectTestBase::kTlsModesStream,
                          TlsConnectTestBase::kTlsV10ToV12));
INSTANTIATE_TEST_CASE_P(ExtensionPre13Datagram, TlsExtensionTestPre13,
                        ::testing::Combine(
                          TlsConnectTestBase::kTlsModesAll,
                          TlsConnectTestBase::kTlsV11V12));

#ifdef NSS_ENABLE_TLS_1_3
INSTANTIATE_TEST_CASE_P(ExtensionTls13, TlsExtensionTest13,
                        TlsConnectTestBase::kTlsModesAll);
#endif

}  // namespace nspr_test
