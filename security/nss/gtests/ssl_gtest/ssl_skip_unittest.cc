/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sslerr.h"

#include "tls_connect.h"
#include "tls_filter.h"
#include "tls_parser.h"

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
  TlsHandshakeSkipFilter(const std::shared_ptr<TlsAgent>& a,
                         uint8_t handshake_type)
      : TlsRecordFilter(a), handshake_type_(handshake_type), skipped_(false) {}

 protected:
  // Takes a record; if it is a handshake record, it removes the first handshake
  // message that is of handshake_type_ type.
  virtual PacketFilter::Action FilterRecord(
      const TlsRecordHeader& record_header, const DataBuffer& input,
      DataBuffer* output) {
    if (record_header.content_type() != ssl_ct_handshake) {
      return KEEP;
    }

    size_t output_offset = 0U;
    output->Allocate(input.len());

    TlsParser parser(input);
    while (parser.remaining()) {
      size_t start = parser.consumed();
      TlsHandshakeFilter::HandshakeHeader header;
      DataBuffer ignored;
      bool complete = false;
      if (!header.Parse(&parser, record_header, DataBuffer(), &ignored,
                        &complete)) {
        ADD_FAILURE() << "Error parsing handshake header";
        return KEEP;
      }
      if (!complete) {
        ADD_FAILURE() << "Don't want to deal with fragmented input";
        return KEEP;
      }

      if (skipped_ || header.handshake_type() != handshake_type_) {
        size_t entire_length = parser.consumed() - start;
        output->Write(output_offset, input.data() + start, entire_length);
        // DTLS sequence numbers need to be rewritten
        if (skipped_ && header.is_dtls()) {
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
    return skipped_ ? CHANGE : KEEP;
  }

 private:
  // The type of handshake message to drop.
  uint8_t handshake_type_;
  // Whether this filter has ever skipped a handshake message.  Track this so
  // that sequence numbers on DTLS handshake messages can be rewritten in
  // subsequent calls.
  bool skipped_;
};

class TlsSkipTest : public TlsConnectTestBase,
                    public ::testing::WithParamInterface<
                        std::tuple<SSLProtocolVariant, uint16_t>> {
 protected:
  TlsSkipTest()
      : TlsConnectTestBase(std::get<0>(GetParam()), std::get<1>(GetParam())) {}

  void SetUp() override {
    TlsConnectTestBase::SetUp();
    EnsureTlsSetup();
  }

  void ServerSkipTest(std::shared_ptr<PacketFilter> filter,
                      uint8_t alert = kTlsAlertUnexpectedMessage) {
    server_->SetFilter(filter);
    ConnectExpectAlert(client_, alert);
  }
};

class Tls13SkipTest : public TlsConnectTestBase,
                      public ::testing::WithParamInterface<SSLProtocolVariant> {
 protected:
  Tls13SkipTest()
      : TlsConnectTestBase(GetParam(), SSL_LIBRARY_VERSION_TLS_1_3) {}

  void SetUp() override {
    TlsConnectTestBase::SetUp();
    EnsureTlsSetup();
  }

  void ServerSkipTest(std::shared_ptr<TlsRecordFilter> filter, int32_t error) {
    filter->EnableDecryption();
    server_->SetFilter(filter);
    ExpectAlert(client_, kTlsAlertUnexpectedMessage);
    ConnectExpectFail();
    client_->CheckErrorCode(error);
    server_->CheckErrorCode(SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT);
  }

  void ClientSkipTest(std::shared_ptr<TlsRecordFilter> filter, int32_t error) {
    filter->EnableDecryption();
    client_->SetFilter(filter);
    server_->ExpectSendAlert(kTlsAlertUnexpectedMessage);
    ConnectExpectFailOneSide(TlsAgent::SERVER);

    server_->CheckErrorCode(error);
    ASSERT_EQ(TlsAgent::STATE_CONNECTED, client_->state());

    client_->Handshake();  // Make sure to consume the alert the server sends.
  }
};

TEST_P(TlsSkipTest, SkipCertificateRsa) {
  EnableOnlyStaticRsaCiphers();
  ServerSkipTest(std::make_shared<TlsHandshakeSkipFilter>(
      server_, kTlsHandshakeCertificate));
  client_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_HELLO_DONE);
}

TEST_P(TlsSkipTest, SkipCertificateDhe) {
  ServerSkipTest(std::make_shared<TlsHandshakeSkipFilter>(
      server_, kTlsHandshakeCertificate));
  client_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_SERVER_KEY_EXCH);
}

TEST_P(TlsSkipTest, SkipCertificateEcdhe) {
  ServerSkipTest(std::make_shared<TlsHandshakeSkipFilter>(
      server_, kTlsHandshakeCertificate));
  client_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_SERVER_KEY_EXCH);
}

TEST_P(TlsSkipTest, SkipCertificateEcdsa) {
  Reset(TlsAgent::kServerEcdsa256);
  ServerSkipTest(std::make_shared<TlsHandshakeSkipFilter>(
      server_, kTlsHandshakeCertificate));
  client_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_SERVER_KEY_EXCH);
}

TEST_P(TlsSkipTest, SkipServerKeyExchange) {
  ServerSkipTest(std::make_shared<TlsHandshakeSkipFilter>(
      server_, kTlsHandshakeServerKeyExchange));
  client_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_HELLO_DONE);
}

TEST_P(TlsSkipTest, SkipServerKeyExchangeEcdsa) {
  Reset(TlsAgent::kServerEcdsa256);
  ServerSkipTest(std::make_shared<TlsHandshakeSkipFilter>(
      server_, kTlsHandshakeServerKeyExchange));
  client_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_HELLO_DONE);
}

TEST_P(TlsSkipTest, SkipCertAndKeyExch) {
  auto chain = std::make_shared<ChainedPacketFilter>(
      ChainedPacketFilterInit{std::make_shared<TlsHandshakeSkipFilter>(
                                  server_, kTlsHandshakeCertificate),
                              std::make_shared<TlsHandshakeSkipFilter>(
                                  server_, kTlsHandshakeServerKeyExchange)});
  ServerSkipTest(chain);
  client_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_HELLO_DONE);
}

TEST_P(TlsSkipTest, SkipCertAndKeyExchEcdsa) {
  Reset(TlsAgent::kServerEcdsa256);
  auto chain = std::make_shared<ChainedPacketFilter>();
  chain->Add(std::make_shared<TlsHandshakeSkipFilter>(
      server_, kTlsHandshakeCertificate));
  chain->Add(std::make_shared<TlsHandshakeSkipFilter>(
      server_, kTlsHandshakeServerKeyExchange));
  ServerSkipTest(chain);
  client_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_HELLO_DONE);
}

TEST_P(Tls13SkipTest, SkipEncryptedExtensions) {
  ServerSkipTest(std::make_shared<TlsHandshakeSkipFilter>(
                     server_, kTlsHandshakeEncryptedExtensions),
                 SSL_ERROR_RX_UNEXPECTED_CERTIFICATE);
}

TEST_P(Tls13SkipTest, SkipServerCertificate) {
  ServerSkipTest(std::make_shared<TlsHandshakeSkipFilter>(
                     server_, kTlsHandshakeCertificate),
                 SSL_ERROR_RX_UNEXPECTED_CERT_VERIFY);
}

TEST_P(Tls13SkipTest, SkipServerCertificateVerify) {
  ServerSkipTest(std::make_shared<TlsHandshakeSkipFilter>(
                     server_, kTlsHandshakeCertificateVerify),
                 SSL_ERROR_RX_UNEXPECTED_FINISHED);
}

TEST_P(Tls13SkipTest, SkipClientCertificate) {
  client_->SetupClientAuth();
  server_->RequestClientAuth(true);
  client_->ExpectReceiveAlert(kTlsAlertUnexpectedMessage);
  ClientSkipTest(std::make_shared<TlsHandshakeSkipFilter>(
                     client_, kTlsHandshakeCertificate),
                 SSL_ERROR_RX_UNEXPECTED_CERT_VERIFY);
}

TEST_P(Tls13SkipTest, SkipClientCertificateVerify) {
  client_->SetupClientAuth();
  server_->RequestClientAuth(true);
  client_->ExpectReceiveAlert(kTlsAlertUnexpectedMessage);
  ClientSkipTest(std::make_shared<TlsHandshakeSkipFilter>(
                     client_, kTlsHandshakeCertificateVerify),
                 SSL_ERROR_RX_UNEXPECTED_FINISHED);
}

INSTANTIATE_TEST_SUITE_P(
    SkipTls10, TlsSkipTest,
    ::testing::Combine(TlsConnectTestBase::kTlsVariantsStream,
                       TlsConnectTestBase::kTlsV10));
INSTANTIATE_TEST_SUITE_P(SkipVariants, TlsSkipTest,
                         ::testing::Combine(TlsConnectTestBase::kTlsVariantsAll,
                                            TlsConnectTestBase::kTlsV11V12));
INSTANTIATE_TEST_SUITE_P(Skip13Variants, Tls13SkipTest,
                         TlsConnectTestBase::kTlsVariantsAll);
}  // namespace nss_test
