/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sslerr.h"

#include "tls_parser.h"
#include "tls_filter.h"
#include "tls_connect.h"

/*
 * The tests in this file test that the TLS state machine is robust against
 * attacks that alter the order of handshake messages.
 *
 * See <https://www.smacktls.com/smack.pdf> for a description of the problems
 * that this sort of attack can enable.
 */
namespace nss_test {

class TlsHandshakeSkipFilter : public TlsRecordFilter {
 public:
  // A TLS record filter that skips handshake messages of the identified type.
  TlsHandshakeSkipFilter(uint8_t handshake_type)
      : handshake_type_(handshake_type),
        skipped_(false) {}

 protected:
  // Takes a record; if it is a handshake record, it removes the first handshake
  // message that is of handshake_type_ type.
  virtual bool FilterRecord(uint8_t content_type, uint16_t version,
                            const DataBuffer& input, DataBuffer* output) {
    if (content_type != kTlsHandshakeType) {
      return false;
    }

    size_t output_offset = 0U;
    output->Allocate(input.len());

    TlsParser parser(input);
    while (parser.remaining()) {
      size_t start = parser.consumed();
      uint8_t handshake_type;
      if (!parser.Read(&handshake_type)) {
        return false;
      }
      uint32_t length;
      if (!TlsHandshakeFilter::ReadLength(&parser, version, &length)) {
        return false;
      }

      if (!parser.Skip(length)) {
        return false;
      }

      if (skipped_ || handshake_type != handshake_type_) {
        size_t entire_length = parser.consumed() - start;
        output->Write(output_offset, input.data() + start,
                      entire_length);
        // DTLS sequence numbers need to be rewritten
        if (skipped_ && IsDtls(version)) {
          output->data()[start + 5] -= 1;
        }
        output_offset += entire_length;
      } else {
        std::cerr << "Dropping handshake: "
                  << static_cast<unsigned>(handshake_type_) << std::endl;
        // We only need to report that the output contains changed data if we
        // drop a handshake message.  But once we've skipped one message, we
        // have to modify all subsequent handshake messages so that they include
        // the correct DTLS sequence numbers.
        skipped_ = true;
      }
    }
    output->Truncate(output_offset);
    return skipped_;
  }

 private:
  // The type of handshake message to drop.
  uint8_t handshake_type_;
  // Whether this filter has ever skipped a handshake message.  Track this so
  // that sequence numbers on DTLS handshake messages can be rewritten in
  // subsequent calls.
  bool skipped_;
};

class TlsSkipTest
  : public TlsConnectTestBase,
    public ::testing::WithParamInterface<std::tuple<std::string, uint16_t>> {

 protected:
  TlsSkipTest()
    : TlsConnectTestBase(TlsConnectTestBase::ToMode(std::get<0>(GetParam())),
                         std::get<1>(GetParam())) {}

  void ServerSkipTest(PacketFilter* filter,
                      uint8_t alert = kTlsAlertUnexpectedMessage) {
    auto alert_recorder = new TlsAlertRecorder();
    client_->SetPacketFilter(alert_recorder);
    if (filter) {
      server_->SetPacketFilter(filter);
    }
    ConnectExpectFail();
    EXPECT_EQ(kTlsAlertFatal, alert_recorder->level());
    EXPECT_EQ(alert, alert_recorder->description());
  }
};

TEST_P(TlsSkipTest, SkipCertificate) {
  ServerSkipTest(new TlsHandshakeSkipFilter(kTlsHandshakeCertificate));
  client_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_HELLO_DONE);
}

TEST_P(TlsSkipTest, SkipCertificateEcdhe) {
  EnableSomeEcdheCiphers();
  ServerSkipTest(new TlsHandshakeSkipFilter(kTlsHandshakeCertificate));
  client_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_SERVER_KEY_EXCH);
}

TEST_P(TlsSkipTest, SkipCertificateEcdsa) {
  ResetEcdsa();
  ServerSkipTest(new TlsHandshakeSkipFilter(kTlsHandshakeCertificate));
  client_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_SERVER_KEY_EXCH);
}

TEST_P(TlsSkipTest, SkipServerKeyExchange) {
  // Have to enable some ephemeral suites, or ServerKeyExchange doesn't appear.
  EnableSomeEcdheCiphers();
  ServerSkipTest(new TlsHandshakeSkipFilter(kTlsHandshakeServerKeyExchange));
  client_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_HELLO_DONE);
}

TEST_P(TlsSkipTest, SkipServerKeyExchangeEcdsa) {
  ResetEcdsa();
  ServerSkipTest(new TlsHandshakeSkipFilter(kTlsHandshakeServerKeyExchange));
  client_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_HELLO_DONE);
}

TEST_P(TlsSkipTest, SkipCertAndKeyExch) {
  auto chain = new ChainedPacketFilter();
  chain->Add(new TlsHandshakeSkipFilter(kTlsHandshakeCertificate));
  chain->Add(new TlsHandshakeSkipFilter(kTlsHandshakeServerKeyExchange));
  ServerSkipTest(chain);
  client_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_HELLO_DONE);
}

TEST_P(TlsSkipTest, SkipCertAndKeyExchEcdsa) {
  ResetEcdsa();
  auto chain = new ChainedPacketFilter();
  chain->Add(new TlsHandshakeSkipFilter(kTlsHandshakeCertificate));
  chain->Add(new TlsHandshakeSkipFilter(kTlsHandshakeServerKeyExchange));
  ServerSkipTest(chain);
  client_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_HELLO_DONE);
}

INSTANTIATE_TEST_CASE_P(SkipTls10, TlsSkipTest,
                        ::testing::Combine(
                          TlsConnectTestBase::kTlsModesStream,
                          TlsConnectTestBase::kTlsV10));
INSTANTIATE_TEST_CASE_P(SkipVariants, TlsSkipTest,
                        ::testing::Combine(
                          TlsConnectTestBase::kTlsModesAll,
                          TlsConnectTestBase::kTlsV11V12));

}  // namespace nss_test
