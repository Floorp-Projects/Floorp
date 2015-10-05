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

class TlsExtensionFilter : public TlsHandshakeFilter {
 protected:
  virtual bool FilterHandshake(uint16_t version, uint8_t handshake_type,
                               const DataBuffer& input, DataBuffer* output) {
    if (handshake_type == kTlsHandshakeClientHello) {
      TlsParser parser(input);
      if (!FindClientHelloExtensions(parser, version)) {
        return false;
      }
      return FilterExtensions(parser, input, output);
    }
    if (handshake_type == kTlsHandshakeServerHello) {
      TlsParser parser(input);
      if (!FindServerHelloExtensions(parser, version)) {
        return false;
      }
      return FilterExtensions(parser, input, output);
    }
    return false;
  }

  virtual bool FilterExtension(uint16_t extension_type,
                               const DataBuffer& input, DataBuffer* output) = 0;

 public:
  static bool FindClientHelloExtensions(TlsParser& parser, uint16_t version) {
    if (!parser.Skip(2 + 32)) { // version + random
      return false;
    }
    if (!parser.SkipVariable(1)) { // session ID
      return false;
    }
    if (IsDtls(version) && !parser.SkipVariable(1)) { // DTLS cookie
      return false;
    }
    if (!parser.SkipVariable(2)) { // cipher suites
      return false;
    }
    if (!parser.SkipVariable(1)) { // compression methods
      return false;
    }
    return true;
  }

  static bool FindServerHelloExtensions(TlsParser& parser, uint16_t version) {
    if (!parser.Skip(2 + 32)) { // version + random
      return false;
    }
    if (!parser.SkipVariable(1)) { // session ID
      return false;
    }
    if (!parser.Skip(2)) { // cipher suite
      return false;
    }
    if (NormalizeTlsVersion(version) <= SSL_LIBRARY_VERSION_TLS_1_2) {
      if (!parser.Skip(1)) { // compression method
        return false;
      }
    }
    return true;
  }

 private:
  bool FilterExtensions(TlsParser& parser,
                        const DataBuffer& input, DataBuffer* output) {
    size_t length_offset = parser.consumed();
    uint32_t all_extensions;
    if (!parser.Read(&all_extensions, 2)) {
      return false; // no extensions, odd but OK
    }
    if (all_extensions != parser.remaining()) {
      return false; // malformed
    }

    bool changed = false;

    // Write out the start of the message.
    output->Allocate(input.len());
    output->Write(0, input.data(), parser.consumed());
    size_t output_offset = parser.consumed();

    while (parser.remaining()) {
      uint32_t extension_type;
      if (!parser.Read(&extension_type, 2)) {
        return false; // malformed
      }

      // Copy extension type.
      output->Write(output_offset, extension_type, 2);

      DataBuffer extension;
      if (!parser.ReadVariable(&extension, 2)) {
        return false; // malformed
      }
      output_offset = ApplyFilter(static_cast<uint16_t>(extension_type), extension,
                                  output, output_offset + 2, &changed);
    }
    output->Truncate(output_offset);

    if (changed) {
      size_t newlen = output->len() - length_offset - 2;
      if (newlen >= 0x10000) {
        return false; // bad: size increased too much
      }
      output->Write(length_offset, newlen, 2);
    }
    return changed;
  }

  size_t ApplyFilter(uint16_t extension_type, const DataBuffer& extension,
                     DataBuffer* output, size_t offset, bool* changed) {
    const DataBuffer* source = &extension;
    DataBuffer filtered;
    if (FilterExtension(extension_type, extension, &filtered) &&
        filtered.len() < 0x10000) {
      *changed = true;
      std::cerr << "extension old: " << extension << std::endl;
      std::cerr << "extension new: " << filtered << std::endl;
      source = &filtered;
    }

    output->Write(offset, source->len(), 2);
    output->Write(offset + 2, *source);
    return offset + 2 + source->len();
  }
};

class TlsExtensionTruncator : public TlsExtensionFilter {
 public:
  TlsExtensionTruncator(uint16_t extension, size_t length)
      : extension_(extension), length_(length) {}
  virtual bool FilterExtension(uint16_t extension_type,
                               const DataBuffer& input, DataBuffer* output) {
    if (extension_type != extension_) {
      return false;
    }
    if (input.len() <= length_) {
      return false;
    }

    output->Assign(input.data(), length_);
    return true;
  }
 private:
    uint16_t extension_;
    size_t length_;
};

class TlsExtensionDamager : public TlsExtensionFilter {
 public:
  TlsExtensionDamager(uint16_t extension, size_t index)
      : extension_(extension), index_(index) {}
  virtual bool FilterExtension(uint16_t extension_type,
                               const DataBuffer& input, DataBuffer* output) {
    if (extension_type != extension_) {
      return false;
    }

    *output = input;
    output->data()[index_] += 73; // Increment selected for maximum damage
    return true;
  }
 private:
  uint16_t extension_;
  size_t index_;
};

class TlsExtensionReplacer : public TlsExtensionFilter {
 public:
  TlsExtensionReplacer(uint16_t extension, const DataBuffer& data)
      : extension_(extension), data_(data) {}
  virtual bool FilterExtension(uint16_t extension_type,
                               const DataBuffer& input, DataBuffer* output) {
    if (extension_type != extension_) {
      return false;
    }

    *output = data_;
    return true;
  }
 private:
  uint16_t extension_;
  DataBuffer data_;
};

class TlsExtensionInjector : public TlsHandshakeFilter {
 public:
  TlsExtensionInjector(uint16_t ext, DataBuffer& data)
      : extension_(ext), data_(data) {}

  virtual bool FilterHandshake(uint16_t version, uint8_t handshake_type,
                               const DataBuffer& input, DataBuffer* output) {
    size_t offset;
    if (handshake_type == kTlsHandshakeClientHello) {
      TlsParser parser(input);
      if (!TlsExtensionFilter::FindClientHelloExtensions(parser, version)) {
        return false;
      }
      offset = parser.consumed();
    } else if (handshake_type == kTlsHandshakeServerHello) {
      TlsParser parser(input);
      if (!TlsExtensionFilter::FindServerHelloExtensions(parser, version)) {
        return false;
      }
      offset = parser.consumed();
    } else {
      return false;
    }

    *output = input;

    std::cerr << "Pre:" << input << std::endl;
    std::cerr << "Lof:" << offset << std::endl;

    // Increase the size of the extensions.
    uint16_t* len_addr = reinterpret_cast<uint16_t*>(output->data() + offset);
    std::cerr << "L-p:" << ntohs(*len_addr) << std::endl;
    *len_addr = htons(ntohs(*len_addr) + data_.len() + 4);
    std::cerr << "L-i:" << ntohs(*len_addr) << std::endl;


    // Insert the extension type and length.
    DataBuffer type_length;
    type_length.Allocate(4);
    type_length.Write(0, extension_, 2);
    type_length.Write(2, data_.len(), 2);
    output->Splice(type_length, offset + 2);

    // Insert the payload.
    output->Splice(data_, offset + 6);

    std::cerr << "Aft:" << *output << std::endl;
    return true;
  }

 private:
  uint16_t extension_;
  DataBuffer data_;
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
    public ::testing::WithParamInterface<std::string> {
 public:
  TlsExtensionTest12Plus()
    : TlsExtensionTestBase(TlsConnectTestBase::ToMode(GetParam()),
                           SSL_LIBRARY_VERSION_TLS_1_2) {}
};

class TlsExtensionTestGeneric
  : public TlsExtensionTestBase,
    public ::testing::WithParamInterface<std::tuple<std::string, uint16_t>> {
 public:
  TlsExtensionTestGeneric()
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

TEST_P(TlsExtensionTestGeneric, AlpnReturnedEmptyList) {
  EnableAlpn();
  const uint8_t val[] = { 0x00, 0x00 };
  DataBuffer extension(val, sizeof(val));
  ServerHelloErrorTest(new TlsExtensionReplacer(ssl_app_layer_protocol_xtn, extension));
}

TEST_P(TlsExtensionTestGeneric, AlpnReturnedEmptyName) {
  EnableAlpn();
  const uint8_t val[] = { 0x00, 0x01, 0x00 };
  DataBuffer extension(val, sizeof(val));
  ServerHelloErrorTest(new TlsExtensionReplacer(ssl_app_layer_protocol_xtn, extension));
}

TEST_P(TlsExtensionTestGeneric, AlpnReturnedListTrailingData) {
  EnableAlpn();
  const uint8_t val[] = { 0x00, 0x02, 0x01, 0x61, 0x00 };
  DataBuffer extension(val, sizeof(val));
  ServerHelloErrorTest(new TlsExtensionReplacer(ssl_app_layer_protocol_xtn, extension));
}

TEST_P(TlsExtensionTestGeneric, AlpnReturnedExtraEntry) {
  EnableAlpn();
  const uint8_t val[] = { 0x00, 0x04, 0x01, 0x61, 0x01, 0x62 };
  DataBuffer extension(val, sizeof(val));
  ServerHelloErrorTest(new TlsExtensionReplacer(ssl_app_layer_protocol_xtn, extension));
}

TEST_P(TlsExtensionTestGeneric, AlpnReturnedBadListLength) {
  EnableAlpn();
  const uint8_t val[] = { 0x00, 0x99, 0x01, 0x61, 0x00 };
  DataBuffer extension(val, sizeof(val));
  ServerHelloErrorTest(new TlsExtensionReplacer(ssl_app_layer_protocol_xtn, extension));
}

TEST_P(TlsExtensionTestGeneric, AlpnReturnedBadNameLength) {
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
  EnableSomeEcdheCiphers();
  const uint8_t val[] = { 0x00, 0x01, 0x00 };
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_elliptic_curves_xtn,
                                                extension));
}

TEST_P(TlsExtensionTestGeneric, SupportedCurvesBadLength) {
  EnableSomeEcdheCiphers();
  const uint8_t val[] = { 0x09, 0x99, 0x00, 0x00 };
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_elliptic_curves_xtn,
                                                extension));
}

TEST_P(TlsExtensionTestGeneric, SupportedCurvesTrailingData) {
  EnableSomeEcdheCiphers();
  const uint8_t val[] = { 0x00, 0x02, 0x00, 0x00, 0x00 };
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_elliptic_curves_xtn,
                                                extension));
}

TEST_P(TlsExtensionTestGeneric, SupportedPointsEmpty) {
  EnableSomeEcdheCiphers();
  const uint8_t val[] = { 0x00 };
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_ec_point_formats_xtn,
                                                extension));
}

TEST_P(TlsExtensionTestGeneric, SupportedPointsBadLength) {
  EnableSomeEcdheCiphers();
  const uint8_t val[] = { 0x99, 0x00, 0x00 };
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_ec_point_formats_xtn,
                                                extension));
}

TEST_P(TlsExtensionTestGeneric, SupportedPointsTrailingData) {
  EnableSomeEcdheCiphers();
  const uint8_t val[] = { 0x01, 0x00, 0x00 };
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_ec_point_formats_xtn,
                                                extension));
}

TEST_P(TlsExtensionTestGeneric, RenegotiationInfoBadLength) {
  const uint8_t val[] = { 0x99 };
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_renegotiation_info_xtn,
                                                extension));
}

TEST_P(TlsExtensionTestGeneric, RenegotiationInfoMismatch) {
  const uint8_t val[] = { 0x01, 0x00 };
  DataBuffer extension(val, sizeof(val));
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_renegotiation_info_xtn,
                                                extension));
}

// The extension has to contain a length.
TEST_P(TlsExtensionTestGeneric, RenegotiationInfoExtensionEmpty) {
  DataBuffer extension;
  ClientHelloErrorTest(new TlsExtensionReplacer(ssl_renegotiation_info_xtn,
                                                extension));
}

INSTANTIATE_TEST_CASE_P(ExtensionTls10, TlsExtensionTestGeneric,
                        ::testing::Combine(
                          TlsConnectTestBase::kTlsModesStream,
                          TlsConnectTestBase::kTlsV10));
INSTANTIATE_TEST_CASE_P(ExtensionVariants, TlsExtensionTestGeneric,
                        ::testing::Combine(
                          TlsConnectTestBase::kTlsModesAll,
                          TlsConnectTestBase::kTlsV11V12));
INSTANTIATE_TEST_CASE_P(ExtensionTls12Plus, TlsExtensionTest12Plus,
                        TlsConnectTestBase::kTlsModesAll);
INSTANTIATE_TEST_CASE_P(ExtensionDgram, TlsExtensionTestDtls,
                        TlsConnectTestBase::kTlsV11V12);

}  // namespace nspr_test
