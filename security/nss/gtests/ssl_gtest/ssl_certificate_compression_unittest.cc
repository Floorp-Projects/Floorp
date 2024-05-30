/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "secerr.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"

extern "C" {
// This is not something that should make you happy.
#include "libssl_internals.h"
}

#include "gtest_utils.h"
#include "nss_scoped_ptrs.h"
#include "tls_connect.h"
#include "tls_filter.h"
#include "tls_parser.h"

namespace nss_test {

class TLSCertificateCompressionExtensionCatcher : public TlsExtensionFilter {
 public:
  TLSCertificateCompressionExtensionCatcher(const std::shared_ptr<TlsAgent>& a)
      : TlsExtensionFilter(a),
        received_compressed_certificate_extension_(false){};

  PacketFilter::Action FilterExtension(uint16_t extension_type,
                                       const DataBuffer& input,
                                       DataBuffer* output) {
    if (extension_type != ssl_certificate_compression_xtn) {
      return KEEP;
    }
    received_compressed_certificate_extension_ = true;

    /* struct {
     *   CertificateCompressionAlgorithm algorithms<2..2^8-2>;
     * } CertificateCompressionAlgorithms;
     */
    uint32_t numberOfExtensions = input.data()[0];
    algorithms = DataBuffer(&input.data()[1], numberOfExtensions);
    return KEEP;
  }

  DataBuffer GetBufCompressionAlgs() { return algorithms; }

  bool sawCertificateCompressionExtension() {
    return received_compressed_certificate_extension_;
  }

 private:
  DataBuffer algorithms;
  bool received_compressed_certificate_extension_;
};

class TLSCertificateCompressionExtensionModifier : public TlsExtensionFilter {
 public:
  TLSCertificateCompressionExtensionModifier(const std::shared_ptr<TlsAgent>& a,
                                             uint8_t byte, uint8_t value)
      : TlsExtensionFilter(a), offset_(byte), value_(value){};

  PacketFilter::Action FilterExtension(uint16_t extension_type,
                                       const DataBuffer& input,
                                       DataBuffer* output) {
    if (extension_type != ssl_certificate_compression_xtn) {
      return KEEP;
    }

    *output = input;
    output->data()[offset_] = value_;
    return CHANGE;
  }

 private:
  uint8_t offset_;
  uint8_t value_;
};

/* The function returns a reference to a message of the following Handshake
 * Type. */
uint64_t findPointerToHandshakeType(DataBuffer plaintext, SSLHandshakeType t) {
  uint64_t skip = 0;
  /* struct {
  **     HandshakeType msg_type;
  **     uint24 length;
  **     select (Handshake.msg_type) {
  **        case client_hello:          ClientHello;...
  **     };
  ** } Handshake;
  */
  while (skip < plaintext.len() && plaintext.data()[skip] != t) {
    skip = skip + 1 /* HandshakeType */ + 3 /* length */
           + (plaintext.data()[skip + 1 /* Handshake.msg_type */] << 16) +
           (plaintext.data()[skip + 2] << 8) + (plaintext.data()[skip + 3]);
  }

  return skip;
}

class TLSCertificateCompressionCertificateCatcher : public TlsRecordFilter {
 public:
  TLSCertificateCompressionCertificateCatcher(
      const std::shared_ptr<TlsAgent>& a)
      : TlsRecordFilter(a) {
    received_compressed_certificate_ = false;
    used_compression_algorithm_ = 0x0;
    EnableDecryption();
  }

  bool sawCompressedCertificate() { return received_compressed_certificate_; }
  uint16_t getCertCompressionAlg() { return used_compression_algorithm_; }
  void unsetSawCompressedCertificate() {
    received_compressed_certificate_ = false;
  }

 protected:
  PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                    const DataBuffer& record, size_t* offset,
                                    DataBuffer* output) override {
    uint8_t inner_content_type;
    DataBuffer plaintext;
    uint16_t protection_epoch = 0;
    TlsRecordHeader out_header(header);

    if (!Unprotect(header, record, &protection_epoch, &inner_content_type,
                   &plaintext, &out_header)) {
      return KEEP;
    }

    uint64_t skip =
        findPointerToHandshakeType(plaintext, ssl_hs_compressed_certificate);
    if (skip >= plaintext.len() ||
        plaintext.data()[skip] != ssl_hs_compressed_certificate) {
      return KEEP;
    }

    skip = skip + 1 /* HandshakeType */ + 3 /* length */;
    if (skip + 1 >= plaintext.len()) {
      return KEEP;
    }
    used_compression_algorithm_ =
        (plaintext.data()[skip] << 8) + plaintext.data()[skip + 1];
    received_compressed_certificate_ = true;
    return KEEP;
  }

 private:
  bool received_compressed_certificate_;
  uint16_t used_compression_algorithm_;
};

class TLSCertificateToEncodedCertificateChanger : public TlsRecordFilter {
 public:
  TLSCertificateToEncodedCertificateChanger(const std::shared_ptr<TlsAgent>& a)
      : TlsRecordFilter(a) {
    EnableDecryption();
  }

 protected:
  PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                    const DataBuffer& record, size_t* offset,
                                    DataBuffer* output) override {
    uint8_t inner_content_type;
    DataBuffer plaintext;
    uint16_t protection_epoch = 0;
    TlsRecordHeader out_header(header);

    if (!Unprotect(header, record, &protection_epoch, &inner_content_type,
                   &plaintext, &out_header)) {
      return KEEP;
    }

    uint64_t skip = findPointerToHandshakeType(plaintext, ssl_hs_certificate);
    if (skip >= plaintext.len() ||
        plaintext.data()[skip] != ssl_hs_certificate) {
      return KEEP;
    }

    DataBuffer plaintextEncodedCert(plaintext);
    /* ssl_hs_certificate -> ssl_hs_compressed_certificate */
    plaintextEncodedCert.data()[skip] = ssl_hs_compressed_certificate;
    /* Next 3 bytes are length*/
    uint32_t certificateLen = (plaintext.data()[skip + 1] << 16) +
                              (plaintext.data()[skip + 2] << 8) +
                              plaintext.data()[skip + 3];

    /* Random Encoding*/
    const uint8_t encodingId[2] = {0xff, 0x01};
    /* struct {
            CertificateCompressionAlgorithm algorithm;
            uint24 uncompressed_length;
            opaque compressed_certificate_message<1..2^24-1>;
       } CompressedCertificate; */

    plaintextEncodedCert.Write(skip + 1, certificateLen + 5,
                               3);  // 2 bytes for algorithm + 3 bytes for len
    plaintextEncodedCert.Write(skip + 4, (const uint8_t*)&encodingId, 2);
    /* the uncompressed_length (the same as we did not change the length of the
     * cert) */
    plaintextEncodedCert.Write(skip + 6, certificateLen, 3);
    /* Copy the rest of the certificate. */
    plaintextEncodedCert.Write(skip + 9,
                               (const uint8_t*)&plaintext.data()[skip + 4],
                               plaintext.len() - skip - 4);

    DataBuffer ciphertext;
    bool ok = Protect(spec(protection_epoch), out_header, inner_content_type,
                      plaintextEncodedCert, &ciphertext, &out_header);
    EXPECT_TRUE(ok);
    if (!ok) {
      return KEEP;
    }
    *offset = out_header.Write(output, *offset, ciphertext);
    return CHANGE;
  }
};

/* Test encoding function. */
static SECStatus SimpleXorCertCompEncode(const SECItem* input,
                                         SECItem* output) {
  SECITEM_CopyItem(NULL, output, input);
  PORT_Memcpy(output->data, input->data, output->len);
  for (size_t i = 0; i < output->len; i++) {
    output->data[i] ^= 0x55;
  }
  return SECSuccess;
}

/* Test decoding function.  */
static SECStatus SimpleXorCertCompDecode(const SECItem* input, uint8_t* output,
                                         size_t outputLen,
                                         size_t* receivedOutputLen) {
  if (input->len != outputLen) {
    return SECFailure;
  }

  PORT_Memcpy(output, input->data, input->len);
  for (size_t i = 0; i < outputLen; i++) {
    output[i] ^= 0x55;
  }
  *receivedOutputLen = outputLen;

  return SECSuccess;
}

static SECStatus SimpleXorWithDifferentValueEncode(const SECItem* input,
                                                   SECItem* output) {
  SECITEM_CopyItem(NULL, output, input);
  for (size_t i = 0; i < output->len; i++) {
    output->data[i] ^= 0x77;
  }
  return SECSuccess;
}

/* Test decoding function.  */
static SECStatus SimpleXorWithDifferentValueDecode(const SECItem* input,
                                                   uint8_t* output,
                                                   size_t outputLen,
                                                   size_t* receivedOutputLen) {
  if (input->len != outputLen) {
    return SECFailure;
  }

  PORT_Memcpy(output, input->data, input->len);
  for (size_t i = 0; i < outputLen; i++) {
    output[i] ^= 0x77;
  }
  *receivedOutputLen = outputLen;

  return SECSuccess;
}

/* These tests are checking the behaviour
 * using the different compression algorithms.
 *
 * struct {
 *          CertificateCompressionAlgorithm algorithms<2..2^8-2>;
 *      } CertificateCompressionAlgorithms;
 *
 * The "extension_data" field of this extension
 * SHALL contain a CertificateCompressionAlgorithms value:
 *   enum {
 *     zlib(1),
 *     brotli(2),
 *     zstd(3),
 *     (65535)
 *   } CertificateCompressionAlgorithm;
 */

/* Algorithm number 0 is reserved. If we receive it, we ignore this algorithm:
 * 1) We do not return a failure if we encountered it
 * 2) If it was the only certificate compression algorithm, we consider that we
 *  did not negotiate the extension
 * 3) If there were the other agorithms, the
 *  extension is negotiated if one of the other algorithms is supported by the
 *  both parties.
 */

/* We can not add an algorithm with empty encoding/decoding function. */
TEST_F(TlsConnectStreamTls13,
       CertificateCompression_CannotAddAlgorithmEmptyEncodingAndDecoding) {
  EnsureTlsSetup();
  SSLCertificateCompressionAlgorithm t = {0xff01, "test function", NULL, NULL};

  EXPECT_EQ(SECFailure,
            SSLExp_SetCertificateCompressionAlgorithm(server_->ssl_fd(), t));
}

/* We can not add an algorithm with reserved id. */
TEST_F(TlsConnectStreamTls13,
       CertificateCompression_CannotAddAlgorithmWithReservedID) {
  EnsureTlsSetup();
  SSLCertificateCompressionAlgorithm t = {
      0, "test function", SimpleXorCertCompEncode, SimpleXorCertCompDecode};

  EXPECT_EQ(SECFailure,
            SSLExp_SetCertificateCompressionAlgorithm(server_->ssl_fd(), t));
}

/* We can add an algorithm with the ID already existed.
 * In this case the previous algorithm will be re-written.
 */
TEST_F(TlsConnectStreamTls13, CertificateCompression_AddingAlreadyExistingAlg) {
  EnsureTlsSetup();

  SSLCertificateCompressionAlgorithm alg_ff01 = {0xff01, "test function",
                                                 SimpleXorCertCompEncode,
                                                 SimpleXorCertCompDecode};

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            client_->ssl_fd(), alg_ff01));

  EXPECT_EQ(SECFailure, SSLExp_SetCertificateCompressionAlgorithm(
                            client_->ssl_fd(), alg_ff01));
}

/* The test modifies the length of the compression certificates algorithms
 * supported by a server. Each identifier of CertificateCompressionAlgorithm is
 * 2 bytes, so the odd length is incorrect.
 */
TEST_F(TlsConnectStreamTls13, CertificateCompression_LengthIsOdd) {
  EnsureTlsSetup();
  SSLCertificateCompressionAlgorithm alg_ff01 = {0xff01, "test function",
                                                 SimpleXorCertCompEncode,
                                                 SimpleXorCertCompDecode};

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            server_->ssl_fd(), alg_ff01));

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            client_->ssl_fd(), alg_ff01));

  auto filterExtension =
      MakeTlsFilter<TLSCertificateCompressionExtensionModifier>(client_, 0, 1);
  filterExtension->EnableDecryption();

  ExpectAlert(client_, kTlsAlertDecodeError);
  ConnectExpectAlert(server_, kTlsAlertDecodeError);

  server_->ExpectReceiveAlert(kTlsAlertCloseNotify);
  client_->ExpectSendAlert(kTlsAlertCloseNotify);

  EXPECT_FALSE(SSLInt_ExtensionNegotiated(server_->ssl_fd(),
                                          ssl_certificate_compression_xtn));

  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
  client_->CheckErrorCode(SSL_ERROR_DECODE_ERROR_ALERT);
}

/* The test checks that the extension is not negotiated if in the ClientHello
 * the extension length is bigger than the actual length of the extension.
 */
TEST_F(TlsConnectStreamTls13,
       CertificateCompression_LengthIsBiggerThanExpected) {
  EnsureTlsSetup();

  SSLCertificateCompressionAlgorithm alg_ff01 = {0xff01, "test function",
                                                 SimpleXorCertCompEncode,
                                                 SimpleXorCertCompDecode};

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            server_->ssl_fd(), alg_ff01));

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            client_->ssl_fd(), alg_ff01));

  auto filterExtension =
      /*But we specify 1 algorithm*/
      MakeTlsFilter<TLSCertificateCompressionExtensionModifier>(client_, 0, 4);
  filterExtension->EnableDecryption();

  ExpectAlert(client_, kTlsAlertDecodeError);
  ConnectExpectAlert(server_, kTlsAlertDecodeError);

  server_->ExpectReceiveAlert(kTlsAlertCloseNotify);
  client_->ExpectSendAlert(kTlsAlertCloseNotify);

  EXPECT_FALSE(SSLInt_ExtensionNegotiated(server_->ssl_fd(),
                                          ssl_certificate_compression_xtn));

  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
  client_->CheckErrorCode(SSL_ERROR_DECODE_ERROR_ALERT);
}

/* The test checks that the extension is not negotiated if in the ClientHello
 * the extension length is smaller than the actual length of the extension.
 */
TEST_F(TlsConnectStreamTls13,
       CertificateCompression_LengthIsSmallerThanExpected) {
  EnsureTlsSetup();

  SSLCertificateCompressionAlgorithm alg_ff01 = {0xff01, "test function",
                                                 SimpleXorCertCompEncode,
                                                 SimpleXorCertCompDecode};

  SSLCertificateCompressionAlgorithm alg_ff02 = {0xff02, "test function",
                                                 SimpleXorCertCompEncode,
                                                 SimpleXorCertCompDecode};

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            server_->ssl_fd(), alg_ff01));

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            client_->ssl_fd(), alg_ff01));

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            client_->ssl_fd(), alg_ff02));

  auto filterExtension =
      /*  But we specify two algorithms*/
      MakeTlsFilter<TLSCertificateCompressionExtensionModifier>(client_, 0, 2);
  filterExtension->EnableDecryption();

  ExpectAlert(client_, kTlsAlertDecodeError);
  ConnectExpectAlert(server_, kTlsAlertDecodeError);

  server_->ExpectReceiveAlert(kTlsAlertCloseNotify);
  client_->ExpectSendAlert(kTlsAlertCloseNotify);

  EXPECT_FALSE(SSLInt_ExtensionNegotiated(server_->ssl_fd(),
                                          ssl_certificate_compression_xtn));

  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
  client_->CheckErrorCode(SSL_ERROR_DECODE_ERROR_ALERT);
}

TEST_F(TlsConnectStreamTls13,
       CertificateCompression_ClientHelloUsedCompressedCertificate) {
  EnsureTlsSetup();
  auto filterExtension =
      MakeTlsFilter<TLSCertificateCompressionCertificateCatcher>(server_);

  SSLCertificateCompressionAlgorithm alg_ff01 = {
      0xff01, "test function id ff01", SimpleXorCertCompEncode,
      SimpleXorCertCompDecode};

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            server_->ssl_fd(), alg_ff01));

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            client_->ssl_fd(), alg_ff01));
  Connect();

  EXPECT_TRUE(filterExtension->sawCompressedCertificate());
}

TEST_F(TlsConnectStreamTls13,
       CertificateCompression_ClientAuthUsesTheServerPreferredAlg) {
  EnsureTlsSetup();
  auto filterExtension =
      MakeTlsFilter<TLSCertificateCompressionCertificateCatcher>(client_);

  SSLCertificateCompressionAlgorithm serverPreferableAlg = {
      // for decompression
      0xff01, "test function id ff01", SimpleXorCertCompEncode,
      SimpleXorCertCompDecode};

  SSLCertificateCompressionAlgorithm clientPreferableAlg = {
      0xff02, "test function id ff02", SimpleXorWithDifferentValueEncode,
      SimpleXorWithDifferentValueDecode};

  /* The server wants to use serverPreferableAlg for decompression. */
  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            server_->ssl_fd(), serverPreferableAlg));
  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            server_->ssl_fd(), clientPreferableAlg));

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            client_->ssl_fd(), clientPreferableAlg));
  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            client_->ssl_fd(), serverPreferableAlg));

  client_->SetupClientAuth();
  server_->RequestClientAuth(true);

  /* Client is sending the client certificate. */
  Connect();

  EXPECT_TRUE(SSLInt_ExtensionNegotiated(server_->ssl_fd(),
                                         ssl_certificate_compression_xtn));
  EXPECT_TRUE(SSLInt_ExtensionNegotiated(client_->ssl_fd(),
                                         ssl_certificate_compression_xtn));

  uint16_t certCompressionAlg = filterExtension->getCertCompressionAlg();
  EXPECT_EQ(certCompressionAlg, serverPreferableAlg.id);
  EXPECT_TRUE(filterExtension->sawCompressedCertificate());
}

TEST_F(TlsConnectStreamTls13,
       CertificateCompression_ServerReceivedUnexpectedEncodedCertificate) {
  EnsureTlsSetup();
  auto filterExtension =
      MakeTlsFilter<TLSCertificateToEncodedCertificateChanger>(client_);
  client_->SetupClientAuth();
  server_->RequestClientAuth(true);

  ExpectAlert(server_, kTlsAlertDecodeError);
  StartConnect();

  server_->SetServerKeyBits(client_->server_key_bits());
  server_->Handshake();
  client_->Handshake();

  ASSERT_TRUE_WAIT((server_->state() != TlsAgent::STATE_CONNECTING), 5000);
  ASSERT_EQ(TlsAgent::STATE_ERROR, server_->state());

  server_->ExpectSendAlert(kTlsAlertCloseNotify);
  client_->ExpectReceiveAlert(kTlsAlertCloseNotify);

  server_->CheckErrorCode(SEC_ERROR_UNEXPECTED_COMPRESSED_CERTIFICATE);
}

TEST_F(TlsConnectStreamTls13,
       CertificateCompression_UnknownAlgorithmNoExtensionNegotiated) {
  EnsureTlsSetup();

  SSLCertificateCompressionAlgorithm alg_ff01 = {
      0xff01, "test function id ff01", SimpleXorCertCompEncode,
      SimpleXorCertCompDecode};

  /* Server does not support the encoding algorithm, only client. */
  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            client_->ssl_fd(), alg_ff01));

  Connect();
  EXPECT_FALSE(SSLInt_ExtensionNegotiated(server_->ssl_fd(),
                                          ssl_certificate_compression_xtn));
}

class TLSCertificateToEncodedCertificateCrasher : public TlsRecordFilter {
 public:
  TLSCertificateToEncodedCertificateCrasher(const std::shared_ptr<TlsAgent>& a)
      : TlsRecordFilter(a) {
    EnableDecryption();
  }

 protected:
  PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                    const DataBuffer& record, size_t* offset,
                                    DataBuffer* output) override {
    uint8_t inner_content_type;
    DataBuffer plaintext;
    uint16_t protection_epoch = 0;
    TlsRecordHeader out_header(header);

    if (!Unprotect(header, record, &protection_epoch, &inner_content_type,
                   &plaintext, &out_header)) {
      return KEEP;
    }

    uint64_t skip = findPointerToHandshakeType(plaintext, ssl_hs_key_update);
    if (skip >= plaintext.len() ||
        plaintext.data()[skip] != ssl_hs_key_update) {
      return KEEP;
    }

    uint32_t ku_len = (plaintext.data()[skip + 1] << 16) +
                      (plaintext.data()[skip + 2] << 8) +
                      plaintext.data()[skip + 3];

    DataBuffer plaintextEncodedCert(plaintext);
    plaintextEncodedCert.Write(skip, &plaintext.data()[skip], ku_len + 1);

    const uint8_t encodedCert[456] = {
        0x19, 0x00, 0x01, 0xc4, 0xff, 0x01, 0x00, 0x01, 0xbf, 0x00, 0x00, 0x01,
        0xbb, 0x00, 0x01, 0xb6, 0x30, 0x82, 0x01, 0xb2, 0x30, 0x82, 0x01, 0x1b,
        0xa0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x01, 0x01, 0x30, 0x0d, 0x06, 0x09,
        0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b, 0x05, 0x00, 0x30,
        0x11, 0x31, 0x0f, 0x30, 0x0d, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x06,
        0x63, 0x6c, 0x69, 0x65, 0x6e, 0x74, 0x30, 0x1e, 0x17, 0x0d, 0x32, 0x33,
        0x31, 0x32, 0x31, 0x33, 0x31, 0x38, 0x31, 0x34, 0x31, 0x38, 0x5a, 0x17,
        0x0d, 0x33, 0x33, 0x31, 0x32, 0x31, 0x33, 0x31, 0x38, 0x31, 0x34, 0x31,
        0x38, 0x5a, 0x30, 0x11, 0x31, 0x0f, 0x30, 0x0d, 0x06, 0x03, 0x55, 0x04,
        0x03, 0x13, 0x06, 0x63, 0x6c, 0x69, 0x65, 0x6e, 0x74, 0x30, 0x81, 0x9f,
        0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
        0x01, 0x05, 0x00, 0x03, 0x81, 0x8d, 0x00, 0x30, 0x81, 0x89, 0x02, 0x81,
        0x81, 0x00, 0xb2, 0xe9, 0x2c, 0xe7, 0x25, 0xe2, 0x10, 0x83, 0x1c, 0xe5,
        0x22, 0xa2, 0x08, 0x55, 0x1a, 0xdf, 0x17, 0xc9, 0x8b, 0x23, 0x48, 0xef,
        0x45, 0x28, 0xe5, 0x8d, 0x6c, 0x37, 0xc0, 0x5e, 0x74, 0x39, 0x05, 0x01,
        0xde, 0x15, 0x17, 0x43, 0xa0, 0x78, 0x5e, 0x3e, 0x02, 0x05, 0xf4, 0x4c,
        0x61, 0x97, 0xb2, 0x01, 0x29, 0xf7, 0x61, 0xf0, 0x10, 0x47, 0x1d, 0x68,
        0x22, 0xf9, 0xef, 0xf2, 0x8c, 0x3b, 0xe5, 0x78, 0x5f, 0x58, 0xf5, 0x88,
        0xd0, 0xb9, 0xa6, 0xe0, 0x9b, 0x3e, 0x6f, 0x86, 0xf1, 0x9d, 0xe5, 0x34,
        0x78, 0xd6, 0xad, 0x6f, 0x6e, 0x38, 0x90, 0x88, 0x29, 0x45, 0x9d, 0xd8,
        0x6d, 0x12, 0x14, 0x9a, 0x87, 0x45, 0xb7, 0x9e, 0x6b, 0xe3, 0x98, 0xdf,
        0x65, 0xa9, 0xb6, 0x30, 0x53, 0xb6, 0xca, 0xed, 0x82, 0x18, 0x69, 0x30,
        0x4e, 0xda, 0x8e, 0x11, 0xc9, 0x98, 0xc6, 0x2e, 0xd8, 0xcd, 0x02, 0x03,
        0x01, 0x00, 0x01, 0xa3, 0x1a, 0x30, 0x18, 0x30, 0x09, 0x06, 0x03, 0x55,
        0x1d, 0x13, 0x04, 0x02, 0x30, 0x00, 0x30, 0x0b, 0x06, 0x03, 0x55, 0x1d,
        0x0f, 0x04, 0x04, 0x03, 0x02, 0x07, 0x80, 0x30, 0x0d, 0x06, 0x09, 0x2a,
        0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b, 0x05, 0x00, 0x03, 0x81,
        0x81, 0x00, 0x4f, 0x9b, 0x47, 0x31, 0xe7, 0x71, 0x11, 0x45, 0x44, 0xf6,
        0x17, 0x09, 0xb3, 0x32, 0x95, 0xaa, 0xe7, 0xd1, 0xec, 0xb4, 0x8b, 0xb6,
        0xd3, 0xb6, 0xb5, 0x30, 0x61, 0x5b, 0x5a, 0xfe, 0x0e, 0xb8, 0x1d, 0x72,
        0xdb, 0x80, 0xd2, 0xf6, 0xd5, 0xdc, 0xbe, 0xff, 0x99, 0x69, 0xb9, 0x5c,
        0x67, 0x18, 0xd4, 0xcb, 0xa8, 0xcf, 0x57, 0x60, 0x3d, 0xc8, 0x76, 0x5e,
        0xc0, 0xcb, 0x6f, 0x22, 0x1a, 0x2c, 0xe6, 0xd1, 0x0e, 0x59, 0xb7, 0x0d,
        0xef, 0x36, 0x6f, 0x57, 0xd3, 0x77, 0x7b, 0xab, 0x29, 0x00, 0x09, 0x87,
        0x0f, 0xf2, 0xfb, 0x59, 0xf5, 0x72, 0x86, 0x33, 0x2f, 0x2f, 0x16, 0x44,
        0x77, 0xed, 0x03, 0x11, 0x43, 0x29, 0x81, 0x07, 0xd1, 0x1a, 0xc2, 0xd5,
        0x78, 0xd8, 0xaa, 0x20, 0x9e, 0x3e, 0x69, 0xcd, 0x6c, 0x3b, 0xd3, 0x07,
        0x32, 0xa3, 0x12, 0x60, 0x01, 0x2e, 0xf4, 0x29, 0x7f, 0x47, 0x00, 0x00};
    plaintextEncodedCert.Write(skip, encodedCert, 456);
    plaintextEncodedCert.Write(skip + 456, &plaintext.data()[skip], ku_len + 1);

    DataBuffer ciphertext;
    bool ok = Protect(spec(protection_epoch), out_header, inner_content_type,
                      plaintextEncodedCert, &ciphertext, &out_header);
    EXPECT_TRUE(ok);
    if (!ok) {
      return KEEP;
    }
    *offset = out_header.Write(output, *offset, ciphertext);
    return CHANGE;
  }
};

TEST_F(
    TlsConnectStreamTls13,
    CertificateCompression_ServerReceivedUnexpectedEncodedCertificate_PostAuth) {
  SSLCertificateCompressionAlgorithm alg_ff01 = {
      0xff01, "test function id ff01", SimpleXorCertCompEncode,
      SimpleXorCertCompDecode};

  EnsureTlsSetup();
  auto filterExtension =
      MakeTlsFilter<TLSCertificateToEncodedCertificateCrasher>(client_);

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            server_->ssl_fd(), alg_ff01));

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            client_->ssl_fd(), alg_ff01));

  Connect();
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(client_->ssl_fd(), PR_FALSE));

  server_->ExpectSendAlert(kTlsAlertDecodeError);
  client_->ExpectSendAlert(kTlsAlertCloseNotify);
  server_->SendData(50);
  client_->ReadBytes(50);
  client_->SendData(50);
  server_->ExpectReadWriteError();
  server_->ReadBytes(50);
  server_->CheckErrorCode(SEC_ERROR_UNEXPECTED_COMPRESSED_CERTIFICATE);
}

/* Here the server first request a client certificate during the handshake,
   but after the handshake it received another, encoded certificate. */
TEST_F(
    TlsConnectStreamTls13,
    CertificateCompression_ServerReceivedUnexpectedEncodedCertificateAfterRequesting) {
  SSLCertificateCompressionAlgorithm alg_ff01 = {
      0xff01, "test function id ff01", SimpleXorCertCompEncode,
      SimpleXorCertCompDecode};

  EnsureTlsSetup();
  auto filterExtension =
      MakeTlsFilter<TLSCertificateToEncodedCertificateCrasher>(client_);

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            server_->ssl_fd(), alg_ff01));

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            client_->ssl_fd(), alg_ff01));

  client_->SetupClientAuth();
  server_->RequestClientAuth(true);

  Connect();
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(client_->ssl_fd(), PR_FALSE));

  server_->ExpectSendAlert(kTlsAlertDecodeError);
  client_->ExpectSendAlert(kTlsAlertCloseNotify);
  server_->SendData(50);
  client_->ReadBytes(50);
  client_->SendData(50);
  server_->ExpectReadWriteError();
  server_->ReadBytes(50);
  server_->CheckErrorCode(SEC_ERROR_UNEXPECTED_COMPRESSED_CERTIFICATE);
}

TEST_F(TlsConnectStreamTls13, CertificateCompression_OneCommonAlg) {
  EnsureTlsSetup();

  SSLCertificateCompressionAlgorithm alg_ff01 = {
      0xff01, "test function id ff01", SimpleXorCertCompEncode,
      SimpleXorCertCompDecode};

  SSLCertificateCompressionAlgorithm alg_ff02 = {
      0xff02, "test function id ff02", SimpleXorCertCompEncode,
      SimpleXorCertCompDecode};

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            server_->ssl_fd(), alg_ff01));

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            server_->ssl_fd(), alg_ff02));

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            client_->ssl_fd(), alg_ff02));

  Connect();
  EXPECT_TRUE(SSLInt_ExtensionNegotiated(server_->ssl_fd(),
                                         ssl_certificate_compression_xtn));

  SSLCertificateCompressionAlgorithm alg;
  EXPECT_EQ(SECSuccess,
            SSLInt_GetCertificateCompressionAlgorithm(server_->ssl_fd(), &alg));
  EXPECT_EQ(0xff02, alg.id);
}

/*
  Test checking the correct behaviour of the preference choice.
  In NSS, the priority is based on the order of the algorithms set up:

  For the CertificateCompression_Preference case,
  the client algorithm 0xff01 has the higher priority and the
  0xff03 algorithm has the lowest priority.

  Then, for each of the advertised algorithms, the second party checks if there
  is a support of this algorithm. In our case, the server supports algs 0xff01
  and 0xff02.

  But as the algorithms 0xff02 has the highest priority, it will be negotiated.
*/

TEST_F(TlsConnectStreamTls13, CertificateCompression_Preference) {
  EnsureTlsSetup();

  SSLCertificateCompressionAlgorithm alg_ff01 = {
      0xff01, "test function id ff01", SimpleXorCertCompEncode,
      SimpleXorCertCompDecode};

  SSLCertificateCompressionAlgorithm alg_ff02 = {
      0xff02, "test function id ff02", SimpleXorCertCompEncode,
      SimpleXorCertCompDecode};

  SSLCertificateCompressionAlgorithm alg_ff03 = {
      0xff03, "test function id ff02", SimpleXorCertCompEncode,
      SimpleXorCertCompDecode};

  /* By sending a compress_certificate extension, the sender indicates to
   the peer the certificate-compression algorithms it is willing to use
   for decompression. */

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            client_->ssl_fd(), alg_ff03));

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            client_->ssl_fd(), alg_ff02));

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            client_->ssl_fd(), alg_ff01));

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            server_->ssl_fd(), alg_ff01));

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            server_->ssl_fd(), alg_ff02));

  Connect();
  EXPECT_TRUE(SSLInt_ExtensionNegotiated(server_->ssl_fd(),
                                         ssl_certificate_compression_xtn));

  SSLCertificateCompressionAlgorithm alg;
  EXPECT_EQ(SECSuccess,
            SSLInt_GetCertificateCompressionAlgorithm(server_->ssl_fd(), &alg));
  EXPECT_EQ(alg_ff02.id, alg.id);
}

TEST_F(TlsConnectStreamTls13, CertificateCompression_SameIDDifferentAlgs) {
  EnsureTlsSetup();

  SSLCertificateCompressionAlgorithm alg_ff01 = {
      0xff01, "test function id ff01", SimpleXorCertCompEncode,
      SimpleXorCertCompDecode};

  SSLCertificateCompressionAlgorithm alg_ff01_but_diffent_alg = {
      0xff01, "test function pretending to be id ff01",
      SimpleXorWithDifferentValueEncode, SimpleXorWithDifferentValueDecode};

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            server_->ssl_fd(), alg_ff01));

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            client_->ssl_fd(), alg_ff01_but_diffent_alg));

  ExpectAlert(client_, kTlsAlertDecodeError);
  ConnectExpectAlert(server_, kTlsAlertDecodeError);

  server_->ExpectSendAlert(kTlsAlertCloseNotify);
  client_->ExpectReceiveAlert(kTlsAlertCloseNotify);

  server_->CheckErrorCode(SSL_ERROR_DECODE_ERROR_ALERT);
  client_->CheckErrorCode(SSL_ERROR_BAD_SERVER);

  EXPECT_TRUE(SSLInt_ExtensionNegotiated(server_->ssl_fd(),
                                         ssl_certificate_compression_xtn));
}

/* This test ensures that if the supported algorithms between server and client
 * are different, no extension is negotiated.
 */
TEST_F(TlsConnectStreamTls13, CertificateCompression_NoCommonAlgs) {
  EnsureTlsSetup();

  SSLCertificateCompressionAlgorithm alg_ff01 = {0xff01, "test function",
                                                 SimpleXorCertCompEncode,
                                                 SimpleXorCertCompDecode};

  SSLCertificateCompressionAlgorithm alg_ff02 = {0xff02, "test function",
                                                 SimpleXorCertCompEncode,
                                                 SimpleXorCertCompDecode};

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            server_->ssl_fd(), alg_ff01));

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            client_->ssl_fd(), alg_ff02));

  Connect();
  EXPECT_FALSE(SSLInt_ExtensionNegotiated(server_->ssl_fd(),
                                          ssl_certificate_compression_xtn));
}

/* The user is trying to add more certificate compression algorithms than it is
 * allowed. The maximum of algorithms is specified by
 * MAX_SUPPORTED_CERTCOMPR_ALGS.
 */
TEST_F(TlsConnectStreamTls13, CertificateCompression_TooManyAlgorithms) {
  EnsureTlsSetup();

  for (size_t i = 0; i < MAX_SUPPORTED_CERTIFICATE_COMPRESSION_ALGS; i++) {
    SSLCertificateCompressionAlgorithm t = {
        (SSLCertificateCompressionAlgorithmID)(i + 1), "test function",
        SimpleXorCertCompEncode, SimpleXorCertCompDecode};
    EXPECT_EQ(SECSuccess,
              SSLExp_SetCertificateCompressionAlgorithm(server_->ssl_fd(), t));
  }

  SSLCertificateCompressionAlgorithm t_last = {
      (SSLCertificateCompressionAlgorithmID)(
          MAX_SUPPORTED_CERTIFICATE_COMPRESSION_ALGS + 1),
      "test function", SimpleXorCertCompEncode, SimpleXorCertCompDecode};

  EXPECT_EQ(SECFailure, SSLExp_SetCertificateCompressionAlgorithm(
                            server_->ssl_fd(), t_last));
}

/* The test checking that when we install a new compression mechanism, it is
 * advertised.
 */
TEST_F(TlsConnectStreamTls13,
       CertificateCompression_SameEncodingAsInCertificateExt) {
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  StartConnect();
  auto filterExtension =
      MakeTlsFilter<TLSCertificateCompressionExtensionCatcher>(client_);
  filterExtension->EnableDecryption();

  SSLCertificateCompressionAlgorithm t = {0xff01, "test function",
                                          SimpleXorCertCompEncode,
                                          SimpleXorCertCompDecode};

  EXPECT_EQ(SECSuccess,
            SSLExp_SetCertificateCompressionAlgorithm(server_->ssl_fd(), t));
  EXPECT_EQ(SECSuccess,
            SSLExp_SetCertificateCompressionAlgorithm(client_->ssl_fd(), t));

  Connect();

  DataBuffer supportedAlgorithms = filterExtension->GetBufCompressionAlgs();
  bool supportsEstablishedExtension = false;

  for (size_t i = 0; i < supportedAlgorithms.len() / 2; i++) {
    uint16_t alg = (supportedAlgorithms.data()[2 * i] << 8) +
                   supportedAlgorithms.data()[2 * i + 1];
    supportsEstablishedExtension =
        supportsEstablishedExtension || (alg == 0xff01);
  }

  EXPECT_TRUE(supportsEstablishedExtension);
}

/* If there is no certificate compression algorithm is possible,
 * the extension is not sent.
 */
TEST_F(TlsConnectStreamTls13,
       CertificateCompression_ServerChecksEncodingNoneInstalled) {
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  StartConnect();

  auto filterExtension =
      MakeTlsFilter<TLSCertificateCompressionExtensionCatcher>(client_);
  Connect();

  EXPECT_FALSE(filterExtension->sawCertificateCompressionExtension());
}

/* RFC 8879
 * This extension is only supported with TLS 1.3 [RFC8446] and newer;
 * if TLS 1.2 [RFC5246] or earlier is negotiated,
 * the peers MUST ignore this extension.
 */
TEST_P(TlsConnectGeneric, CertificateCompressionTLS12AndBelow) {
  if (version_ == SSL_LIBRARY_VERSION_TLS_1_3) GTEST_SKIP();
  if (version_ < SSL_LIBRARY_VERSION_TLS_1_1) GTEST_SKIP();
  StartConnect();

  /* Adding the certificate compression extension.*/
  const uint8_t empty_buf[] = {0x01, 0x00, 0x01};
  DataBuffer empty(empty_buf, 3);
  auto filter = MakeTlsFilter<TlsExtensionAppender>(
      client_, kTlsHandshakeClientHello, 27, empty);

  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    filter->EnableDecryption();
  }

  ConnectExpectAlert(server_, kTlsAlertDecryptError);

  EXPECT_FALSE(SSLInt_ExtensionNegotiated(server_->ssl_fd(),
                                          ssl_certificate_compression_xtn));

  server_->CheckErrorCode(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE);
  client_->CheckErrorCode(SSL_ERROR_DECRYPT_ERROR_ALERT);
}

/* Test encoding function. Creates an encoded certificate of size 0. */
static SECStatus SimpleXorCertCompEncode_returns_buffer_size_0(
    const SECItem* input, SECItem* output) {
  SECITEM_MakeItem(NULL, output, input->data, 0);
  return SECSuccess;
}

/* The CompressedCertificate message is formed as follows:
 * struct {
 *  CertificateCompressionAlgorithm algorithm;
 *  uint24 uncompressed_length;
 *  opaque compressed_certificate_message<1..2^24-1>;
 * } CompressedCertificate;
 */

TEST_F(TlsConnectStreamTls13,
       CertificateCompression_CompressionFunctionCreatesABufferOfSize0) {
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  StartConnect();

  SSLCertificateCompressionAlgorithm t = {
      0xff01, "test function", SimpleXorCertCompEncode_returns_buffer_size_0,
      SimpleXorCertCompDecode};

  EXPECT_EQ(SECSuccess,
            SSLExp_SetCertificateCompressionAlgorithm(server_->ssl_fd(), t));
  EXPECT_EQ(SECSuccess,
            SSLExp_SetCertificateCompressionAlgorithm(client_->ssl_fd(), t));

  ConnectExpectAlert(server_, kTlsAlertHandshakeFailure);
  server_->CheckErrorCode(SEC_ERROR_LIBRARY_FAILURE);
}

class TLSCertificateCompressionCertificateModifier : public TlsRecordFilter {
 public:
  TLSCertificateCompressionCertificateModifier(
      const std::shared_ptr<TlsAgent>& a, uint64_t _byte, uint64_t _value)
      : TlsRecordFilter(a),
        offset_start_(_byte),
        offset_finish_(0xffffffff),
        value_(_value) {
    EnableDecryption();
  }
  TLSCertificateCompressionCertificateModifier(
      const std::shared_ptr<TlsAgent>& a, uint64_t _byteStart,
      uint64_t _byteFinish, uint64_t _value)
      : TlsRecordFilter(a),
        offset_start_(_byteStart),
        offset_finish_(_byteFinish),
        value_(_value) {
    EnableDecryption();
  }

 protected:
  PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                    const DataBuffer& record, size_t* offset,
                                    DataBuffer* output) override {
    uint8_t inner_content_type;
    DataBuffer plaintext;
    uint16_t protection_epoch = 0;
    TlsRecordHeader out_header(header);

    if (!Unprotect(header, record, &protection_epoch, &inner_content_type,
                   &plaintext, &out_header)) {
      return KEEP;
    }

    uint64_t skip =
        findPointerToHandshakeType(plaintext, ssl_hs_compressed_certificate);
    if (skip >= plaintext.len() ||
        plaintext.data()[skip] != ssl_hs_compressed_certificate) {
      return KEEP;
    }

    if (offset_finish_ == 0xffffffff) {
      plaintext.data()[skip + offset_start_] = value_;
    } else {
      for (size_t i = offset_start_; i < offset_finish_ + 1; i++) {
        plaintext.data()[skip + i] = value_;
      }
    }

    DataBuffer ciphertext;
    bool ok = Protect(spec(protection_epoch), out_header, inner_content_type,
                      plaintext, &ciphertext, &out_header);
    EXPECT_TRUE(ok);
    if (!ok) {
      return KEEP;
    }
    *offset = out_header.Write(output, *offset, ciphertext);
    return CHANGE;
  }

 private:
  uint64_t offset_start_;
  uint64_t offset_finish_;
  uint8_t value_;
};

class TLSCertificateCompressionCertificateElongator : public TlsRecordFilter {
 public:
  TLSCertificateCompressionCertificateElongator(
      const std::shared_ptr<TlsAgent>& a, uint64_t len)
      : TlsRecordFilter(a), len_(len) {
    EnableDecryption();
  }

 protected:
  PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                    const DataBuffer& record, size_t* offset,
                                    DataBuffer* output) override {
    uint8_t inner_content_type;
    DataBuffer plaintext;
    uint16_t protection_epoch = 0;
    TlsRecordHeader out_header(header);

    if (!Unprotect(header, record, &protection_epoch, &inner_content_type,
                   &plaintext, &out_header)) {
      return KEEP;
    }

    uint64_t skip =
        findPointerToHandshakeType(plaintext, ssl_hs_compressed_certificate);
    if (skip >= plaintext.len() ||
        plaintext.data()[skip] != ssl_hs_compressed_certificate) {
      return KEEP;
    }

    plaintext.Write(plaintext.len(), (uint32_t)0, len_);

    DataBuffer ciphertext;
    bool ok = Protect(spec(protection_epoch), out_header, inner_content_type,
                      plaintext, &ciphertext, &out_header);
    EXPECT_TRUE(ok);
    if (!ok) {
      return KEEP;
    }
    *offset = out_header.Write(output, *offset, ciphertext);
    return CHANGE;
  }

 private:
  uint64_t len_;
};

/* The CompressedCertificate message is formed as follows:
 * struct {
 *  CertificateCompressionAlgorithm algorithm;
 *  uint24 uncompressed_length;
 *  opaque compressed_certificate_message<1..2^24-1>;
 * } CompressedCertificate;
 *
 * algorithm:
 *  The algorithm used to compress the certificate.
 *  The algorithm MUST be one of the algorithms listed in the peer's
 *    compress_certificate extension.
 *
 * In the next test we modify the encoding used to encode the certificate to the
 * one that the server did not advertise.
 */
TEST_F(TlsConnectStreamTls13, CertificateCompression_ReceivedWrongAlgorithm) {
  EnsureTlsSetup();
  auto filterExtension =
      MakeTlsFilter<TLSCertificateCompressionCertificateModifier>(server_, 0x5,
                                                                  0x2);

  SSLCertificateCompressionAlgorithm t = {0xff01, "test function",
                                          SimpleXorCertCompEncode,
                                          SimpleXorCertCompDecode};

  EXPECT_EQ(SECSuccess,
            SSLExp_SetCertificateCompressionAlgorithm(server_->ssl_fd(), t));
  EXPECT_EQ(SECSuccess,
            SSLExp_SetCertificateCompressionAlgorithm(client_->ssl_fd(), t));

  ExpectAlert(client_, kTlsAlertIllegalParameter);
  StartConnect();

  client_->SetServerKeyBits(server_->server_key_bits());
  client_->Handshake();
  server_->Handshake();

  ASSERT_TRUE_WAIT((client_->state() != TlsAgent::STATE_CONNECTING), 5000);
  ASSERT_EQ(TlsAgent::STATE_ERROR, client_->state());

  client_->ExpectSendAlert(kTlsAlertCloseNotify);
  server_->ExpectReceiveAlert(kTlsAlertCloseNotify);

  client_->CheckErrorCode(
      SEC_ERROR_CERTIFICATE_COMPRESSION_ALGORITHM_NOT_SUPPORTED);
}

/* The next test modifies the length of the encoded certificate
 *  (compressed_certificate_message len);
 * the new length is compressed_certificate_message is equal to 0.
 */
TEST_F(TlsConnectStreamTls13,
       CertificateCompression_ReceivedZeroCompressedMessage) {
  EnsureTlsSetup();
  auto filterExtension =
      MakeTlsFilter<TLSCertificateCompressionCertificateModifier>(server_, 0xa,
                                                                  0xb, 0x0);

  SSLCertificateCompressionAlgorithm t = {0xff01, "test function",
                                          SimpleXorCertCompEncode,
                                          SimpleXorCertCompDecode};

  EXPECT_EQ(SECSuccess,
            SSLExp_SetCertificateCompressionAlgorithm(server_->ssl_fd(), t));
  EXPECT_EQ(SECSuccess,
            SSLExp_SetCertificateCompressionAlgorithm(client_->ssl_fd(), t));

  ExpectAlert(client_, kTlsAlertBadCertificate);
  StartConnect();

  client_->SetServerKeyBits(server_->server_key_bits());
  client_->Handshake();
  server_->Handshake();

  ASSERT_TRUE_WAIT((client_->state() != TlsAgent::STATE_CONNECTING), 5000);
  ASSERT_EQ(TlsAgent::STATE_ERROR, client_->state());

  client_->ExpectSendAlert(kTlsAlertCloseNotify);
  server_->ExpectReceiveAlert(kTlsAlertCloseNotify);

  client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CERTIFICATE);
}

/* The next test modifies the length of the encoded certificate
 * (compressed_certificate_message len);
 * the new length is compressed_certificate_message is longer than the
 * certificate.
 */
TEST_F(TlsConnectStreamTls13,
       CertificateCompression_ReceivedLongerCompressedMessage) {
  EnsureTlsSetup();
  auto filterExtension =
      MakeTlsFilter<TLSCertificateCompressionCertificateModifier>(server_, 0x9,
                                                                  0xb, 0xff);

  SSLCertificateCompressionAlgorithm t = {0xff01, "test function",
                                          SimpleXorCertCompEncode,
                                          SimpleXorCertCompDecode};

  EXPECT_EQ(SECSuccess,
            SSLExp_SetCertificateCompressionAlgorithm(server_->ssl_fd(), t));
  EXPECT_EQ(SECSuccess,
            SSLExp_SetCertificateCompressionAlgorithm(client_->ssl_fd(), t));

  ExpectAlert(client_, kTlsAlertBadCertificate);
  StartConnect();

  client_->SetServerKeyBits(server_->server_key_bits());
  client_->Handshake();
  server_->Handshake();

  ASSERT_TRUE_WAIT((client_->state() != TlsAgent::STATE_CONNECTING), 5000);
  ASSERT_EQ(TlsAgent::STATE_ERROR, client_->state());

  client_->ExpectSendAlert(kTlsAlertCloseNotify);
  server_->ExpectReceiveAlert(kTlsAlertCloseNotify);

  client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CERTIFICATE);
}

TEST_F(TlsConnectStreamTls13,
       CertificateCompression_ReceivedCertificateTooLong) {
  EnsureTlsSetup();
  auto filterExtension =
      MakeTlsFilter<TLSCertificateCompressionCertificateElongator>(server_,
                                                                   0x4);

  SSLCertificateCompressionAlgorithm t = {0xff01, "test function",
                                          SimpleXorCertCompEncode,
                                          SimpleXorCertCompDecode};
  EXPECT_EQ(SECSuccess,
            SSLExp_SetCertificateCompressionAlgorithm(server_->ssl_fd(), t));
  EXPECT_EQ(SECSuccess,
            SSLExp_SetCertificateCompressionAlgorithm(client_->ssl_fd(), t));

  ExpectAlert(client_, kTlsAlertUnexpectedMessage);
  StartConnect();
  Handshake();

  server_->ExpectReceiveAlert(kTlsAlertCloseNotify);
  client_->ExpectSendAlert(kTlsAlertCloseNotify);

  client_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_HANDSHAKE);
}

/* Test encoding function. Returns error unconditionally. */
static SECStatus SimpleXorCertCompEncode_always_error(const SECItem* input,
                                                      SECItem* output) {
  return SECFailure;
}

/* Test decoding function.  Returns error unconditionally. */
static SECStatus SimpleXorCertCompDecode_always_error(
    const SECItem* input, uint8_t* output, size_t outputLen,
    size_t* receivedOutputLen) {
  return SECFailure;
}

TEST_F(TlsConnectStreamTls13, CertificateCompression_CertificateCannotEncode) {
  EnsureTlsSetup();
  SSLCertificateCompressionAlgorithm t = {0xff01, "test function",
                                          SimpleXorCertCompEncode_always_error,
                                          SimpleXorCertCompDecode};

  EXPECT_EQ(SECSuccess,
            SSLExp_SetCertificateCompressionAlgorithm(server_->ssl_fd(), t));
  EXPECT_EQ(SECSuccess,
            SSLExp_SetCertificateCompressionAlgorithm(client_->ssl_fd(), t));

  ExpectAlert(server_, kTlsAlertHandshakeFailure);
  StartConnect();

  client_->SetServerKeyBits(server_->server_key_bits());
  client_->Handshake();
  server_->Handshake();

  ASSERT_TRUE_WAIT(client_->state() != TlsAgent::STATE_CONNECTING, 5000);

  server_->ExpectReceiveAlert(kTlsAlertCloseNotify);
  client_->ExpectSendAlert(kTlsAlertCloseNotify);

  server_->CheckErrorCode(SEC_ERROR_NO_MEMORY);
}

TEST_F(TlsConnectStreamTls13, CertificateCompression_CertificateCannotDecode) {
  EnsureTlsSetup();

  SSLCertificateCompressionAlgorithm t = {0xff01, "test function",
                                          SimpleXorCertCompEncode,
                                          SimpleXorCertCompDecode_always_error};

  EXPECT_EQ(SECSuccess,
            SSLExp_SetCertificateCompressionAlgorithm(server_->ssl_fd(), t));
  EXPECT_EQ(SECSuccess,
            SSLExp_SetCertificateCompressionAlgorithm(client_->ssl_fd(), t));

  ExpectAlert(client_, kTlsAlertBadCertificate);
  StartConnect();

  client_->SetServerKeyBits(server_->server_key_bits());
  client_->Handshake();
  server_->Handshake();

  ASSERT_TRUE_WAIT(client_->state() != TlsAgent::STATE_CONNECTING, 5000);

  server_->ExpectReceiveAlert(kTlsAlertCloseNotify);
  client_->ExpectSendAlert(kTlsAlertCloseNotify);

  client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CERTIFICATE);
}

/* Decoding function returning unexpected decoded certificate length.  */
static SECStatus WrongUsedLenCertCompDecode(const SECItem* input,
                                            uint8_t* output, size_t outputLen,
                                            size_t* receivedOutputLen) {
  if (input->len != outputLen) {
    return SECFailure;
  }

  PORT_Memcpy(output, input->data, input->len);
  *receivedOutputLen = outputLen - 1;

  return SECSuccess;
}

TEST_F(TlsConnectStreamTls13,
       CertificateCompression_WrongDecodedCertificateLength) {
  EnsureTlsSetup();

  SSLCertificateCompressionAlgorithm t = {0xff01, "test function",
                                          SimpleXorCertCompEncode,
                                          WrongUsedLenCertCompDecode};

  EXPECT_EQ(SECSuccess,
            SSLExp_SetCertificateCompressionAlgorithm(server_->ssl_fd(), t));
  EXPECT_EQ(SECSuccess,
            SSLExp_SetCertificateCompressionAlgorithm(client_->ssl_fd(), t));

  ExpectAlert(client_, kTlsAlertBadCertificate);
  StartConnect();

  client_->SetServerKeyBits(server_->server_key_bits());
  client_->Handshake();
  server_->Handshake();

  ASSERT_TRUE_WAIT(client_->state() != TlsAgent::STATE_CONNECTING, 5000);

  server_->ExpectReceiveAlert(kTlsAlertCloseNotify);
  client_->ExpectSendAlert(kTlsAlertCloseNotify);

  client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CERTIFICATE);
}

/* The test checking the client authentification is successful using certificate
 * compression. */
TEST_F(TlsConnectStreamTls13, CertificateCompression_PostAuth) {
  EnsureTlsSetup();

  auto filterExtension =
      MakeTlsFilter<TLSCertificateCompressionCertificateCatcher>(client_);

  SSLCertificateCompressionAlgorithm t = {0xff01, "test function",
                                          SimpleXorCertCompEncode,
                                          SimpleXorCertCompDecode};

  EXPECT_EQ(SECSuccess,
            SSLExp_SetCertificateCompressionAlgorithm(server_->ssl_fd(), t));
  EXPECT_EQ(SECSuccess,
            SSLExp_SetCertificateCompressionAlgorithm(client_->ssl_fd(), t));

  SSLSignatureScheme scheme = ssl_sig_rsa_pss_rsae_sha256;
  SECStatus rv = SSL_SignatureSchemePrefSet(server_->ssl_fd(), &scheme, 1);
  EXPECT_EQ(SECSuccess, rv);
  rv = SSL_SignatureSchemePrefSet(client_->ssl_fd(), &scheme, 1);
  EXPECT_EQ(SECSuccess, rv);

  client_->SetupClientAuth();
  client_->SetOption(SSL_ENABLE_POST_HANDSHAKE_AUTH, PR_TRUE);
  size_t called = 0;
  server_->SetAuthCertificateCallback(
      [&called](TlsAgent*, PRBool, PRBool) -> SECStatus {
        called++;
        return SECSuccess;
      });
  Connect();
  // Send CertificateRequest.
  EXPECT_EQ(SECSuccess, SSL_SendCertificateRequest(server_->ssl_fd()))
      << "Unexpected error: " << PORT_ErrorToName(PORT_GetError());

  // Need to do a round-trip so that the post-handshake message is
  // handled on both client and server.
  server_->SendData(50);
  client_->ReadBytes(50);
  client_->SendData(50);
  server_->ReadBytes(50);

  EXPECT_EQ(1U, called);
  EXPECT_TRUE(SSLInt_ExtensionNegotiated(client_->ssl_fd(),
                                         ssl_certificate_compression_xtn));

  SendReceive(60);
  client_->CheckClientAuthCompleted();

  /* Ensuring that we used CompressedCertificate*/
  EXPECT_TRUE(filterExtension->sawCompressedCertificate());
}

/* Partial decoding/encoding algorithms. */
TEST_F(TlsConnectStreamTls13, CertificateCompression_ClientOnlyDecodes) {
  EnsureTlsSetup();
  auto filterExtension =
      MakeTlsFilter<TLSCertificateCompressionCertificateCatcher>(server_);

  SSLCertificateCompressionAlgorithm alg_only_encode = {
      0xff01, "test function id ff01", SimpleXorCertCompEncode, NULL};

  SSLCertificateCompressionAlgorithm alg_only_decode = {
      0xff01, "test function id ff01", NULL, SimpleXorCertCompDecode};

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            server_->ssl_fd(), alg_only_encode));

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            client_->ssl_fd(), alg_only_decode));

  Connect();

  EXPECT_TRUE(filterExtension->sawCompressedCertificate());
}

TEST_F(TlsConnectStreamTls13,
       CertificateCompression_ClientOnlyDecodes_NoEncoding) {
  EnsureTlsSetup();
  auto filterExtension =
      MakeTlsFilter<TLSCertificateCompressionCertificateCatcher>(client_);

  SSLCertificateCompressionAlgorithm alg_only_encode = {
      0xff01, "test function id ff01", SimpleXorCertCompEncode, NULL};

  SSLCertificateCompressionAlgorithm alg_only_decode = {
      0xff01, "test function id ff01", NULL, SimpleXorCertCompDecode};

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            server_->ssl_fd(), alg_only_encode));

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            client_->ssl_fd(), alg_only_decode));

  SSLSignatureScheme scheme = ssl_sig_rsa_pss_rsae_sha256;
  SECStatus rv = SSL_SignatureSchemePrefSet(server_->ssl_fd(), &scheme, 1);
  EXPECT_EQ(SECSuccess, rv);
  rv = SSL_SignatureSchemePrefSet(client_->ssl_fd(), &scheme, 1);
  EXPECT_EQ(SECSuccess, rv);

  client_->SetupClientAuth();
  client_->SetOption(SSL_ENABLE_POST_HANDSHAKE_AUTH, PR_TRUE);
  size_t called = 0;
  server_->SetAuthCertificateCallback(
      [&called](TlsAgent*, PRBool, PRBool) -> SECStatus {
        called++;
        return SECSuccess;
      });
  Connect();
  // Send CertificateRequest.
  EXPECT_EQ(SECSuccess, SSL_SendCertificateRequest(server_->ssl_fd()))
      << "Unexpected error: " << PORT_ErrorToName(PORT_GetError());

  // Need to do a round-trip so that the post-handshake message is
  // handled on both client and server.
  server_->SendData(50);
  client_->ReadBytes(50);
  client_->SendData(50);
  server_->ReadBytes(50);

  EXPECT_EQ(1U, called);
  EXPECT_TRUE(SSLInt_ExtensionNegotiated(client_->ssl_fd(),
                                         ssl_certificate_compression_xtn));

  SendReceive(60);
  client_->CheckClientAuthCompleted();

  /* Ensuring that we have not used CompressedCertificate. */
  EXPECT_FALSE(filterExtension->sawCompressedCertificate());
}

/* SSL_SendCertificateRequest function called by a server advertises
   the Certificate Compression Extension. */
TEST_F(TlsConnectStreamTls13,
       CertificateCompression_TwoEncodedCertificateRequests) {
  EnsureTlsSetup();
  auto filterExtension =
      MakeTlsFilter<TLSCertificateCompressionCertificateCatcher>(client_);

  SSLCertificateCompressionAlgorithm alg_ff01 = {
      0xff01, "test function id ff01", SimpleXorCertCompEncode,
      SimpleXorCertCompDecode};

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            server_->ssl_fd(), alg_ff01));

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            client_->ssl_fd(), alg_ff01));

  client_->SetupClientAuth();
  client_->SetOption(SSL_ENABLE_POST_HANDSHAKE_AUTH, PR_TRUE);
  size_t called = 0;
  server_->SetAuthCertificateCallback(
      [&called](TlsAgent*, PRBool, PRBool) -> SECStatus {
        called++;
        return SECSuccess;
      });
  Connect();
  // Send CertificateRequest.
  EXPECT_EQ(SECSuccess, SSL_SendCertificateRequest(server_->ssl_fd()))
      << "Unexpected error: " << PORT_ErrorToName(PORT_GetError());

  // Need to do a round-trip so that the post-handshake message is
  // handled on both client and server.
  server_->SendData(50);
  client_->ReadBytes(50);
  client_->SendData(50);
  server_->ReadBytes(50);

  EXPECT_EQ(1U, called);

  SendReceive(60);
  client_->CheckClientAuthCompleted();

  EXPECT_TRUE(filterExtension->sawCompressedCertificate());
  filterExtension->unsetSawCompressedCertificate();
  EXPECT_FALSE(filterExtension->sawCompressedCertificate());

  /* Advertising again the certificate encoding alg. */
  EXPECT_EQ(SECSuccess, SSL_SendCertificateRequest(server_->ssl_fd()))
      << "Unexpected error: " << PORT_ErrorToName(PORT_GetError());

  server_->SendData(50);
  client_->ReadBytes(50);
  client_->SendData(50);
  server_->ReadBytes(50);

  EXPECT_EQ(2U, called);
  /* We saw the compressed certificate again. */
  EXPECT_TRUE(filterExtension->sawCompressedCertificate());

  SendReceive(200);
  client_->CheckClientAuthCallbacksCompleted(2);
}

TEST_F(TlsConnectStreamTls13, CertificateCompression_ServerDecodingIsNULL) {
  EnsureTlsSetup();
  auto filterExtension =
      MakeTlsFilter<TLSCertificateCompressionCertificateCatcher>(server_);

  SSLCertificateCompressionAlgorithm alg_only_encode = {
      0xff01, "test function id ff01", SimpleXorCertCompEncode, NULL};

  SSLCertificateCompressionAlgorithm alg_only_decode = {
      0xff01, "test function id ff01", SimpleXorCertCompEncode, NULL};

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            server_->ssl_fd(), alg_only_encode));

  EXPECT_EQ(SECSuccess, SSLExp_SetCertificateCompressionAlgorithm(
                            client_->ssl_fd(), alg_only_decode));

  ExpectAlert(client_, kTlsAlertIllegalParameter);
  ConnectExpectAlert(server_, kTlsAlertIllegalParameter);

  server_->ExpectSendAlert(kTlsAlertCloseNotify);
  client_->ExpectReceiveAlert(kTlsAlertCloseNotify);

  server_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
  client_->CheckErrorCode(SEC_ERROR_LIBRARY_FAILURE);
}

}  // namespace nss_test
