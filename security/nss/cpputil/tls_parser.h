/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef tls_parser_h_
#define tls_parser_h_

#include <cstdint>
#include <cstring>
#include <memory>
#if defined(WIN32) || defined(WIN64)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif
#include "databuffer.h"
#include "sslt.h"

namespace nss_test {

const uint8_t kTlsHandshakeClientHello = 1;
const uint8_t kTlsHandshakeServerHello = 2;
const uint8_t kTlsHandshakeNewSessionTicket = 4;
const uint8_t kTlsHandshakeHelloRetryRequest = 6;
const uint8_t kTlsHandshakeEncryptedExtensions = 8;
const uint8_t kTlsHandshakeCertificate = 11;
const uint8_t kTlsHandshakeServerKeyExchange = 12;
const uint8_t kTlsHandshakeCertificateRequest = 13;
const uint8_t kTlsHandshakeCertificateVerify = 15;
const uint8_t kTlsHandshakeClientKeyExchange = 16;
const uint8_t kTlsHandshakeFinished = 20;

const uint8_t kTlsAlertWarning = 1;
const uint8_t kTlsAlertFatal = 2;

const uint8_t kTlsAlertCloseNotify = 0;
const uint8_t kTlsAlertUnexpectedMessage = 10;
const uint8_t kTlsAlertBadRecordMac = 20;
const uint8_t kTlsAlertRecordOverflow = 22;
const uint8_t kTlsAlertHandshakeFailure = 40;
const uint8_t kTlsAlertBadCertificate = 42;
const uint8_t kTlsAlertIllegalParameter = 47;
const uint8_t kTlsAlertDecodeError = 50;
const uint8_t kTlsAlertDecryptError = 51;
const uint8_t kTlsAlertProtocolVersion = 70;
const uint8_t kTlsAlertInternalError = 80;
const uint8_t kTlsAlertInappropriateFallback = 86;
const uint8_t kTlsAlertMissingExtension = 109;
const uint8_t kTlsAlertUnsupportedExtension = 110;
const uint8_t kTlsAlertUnrecognizedName = 112;
const uint8_t kTlsAlertNoApplicationProtocol = 120;

const uint8_t kTlsFakeChangeCipherSpec[] = {
    ssl_ct_change_cipher_spec,  // Type
    0xfe,
    0xff,  // Version
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,  // Fictitious sequence #
    0x00,
    0x01,  // Length
    0x01   // Value
};

static const uint8_t kTls13PskKe = 0;
static const uint8_t kTls13PskDhKe = 1;
static const uint8_t kTls13PskAuth = 0;
static const uint8_t kTls13PskSignAuth = 1;

inline std::ostream& operator<<(std::ostream& os, SSLProtocolVariant v) {
  return os << ((v == ssl_variant_stream) ? "TLS" : "DTLS");
}

inline bool IsDtls(uint16_t version) { return (version & 0x8000) == 0x8000; }

inline uint16_t NormalizeTlsVersion(uint16_t version) {
  if (version == 0xfeff) {
    return 0x0302;  // special: DTLS 1.0 == TLS 1.1
  }
  if (IsDtls(version)) {
    return (version ^ 0xffff) + 0x0201;
  }
  return version;
}

inline uint16_t TlsVersionToDtlsVersion(uint16_t version) {
  if (version == 0x0302) {
    return 0xfeff;
  }
  if (version == 0x0304) {
    return version;
  }
  return 0xffff - version + 0x0201;
}

inline size_t WriteVariable(DataBuffer* target, size_t index,
                            const DataBuffer& buf, size_t len_size) {
  index = target->Write(index, static_cast<uint32_t>(buf.len()), len_size);
  return target->Write(index, buf.data(), buf.len());
}

class TlsParser {
 public:
  TlsParser(const uint8_t* data, size_t len) : buffer_(data, len), offset_(0) {}
  explicit TlsParser(const DataBuffer& buf) : buffer_(buf), offset_(0) {}

  bool Read(uint8_t* val);
  // Read an integral type of specified width.
  bool Read(uint32_t* val, size_t size);
  // Reads len bytes into dest buffer, overwriting it.
  bool Read(DataBuffer* dest, size_t len);
  bool ReadFromMark(DataBuffer* val, size_t len, size_t mark);
  // Reads bytes into dest buffer, overwriting it.  The number of bytes is
  // determined by reading from len_size bytes from the stream first.
  bool ReadVariable(DataBuffer* dest, size_t len_size);

  bool Skip(size_t len);
  bool SkipVariable(size_t len_size);

  size_t consumed() const { return offset_; }
  size_t remaining() const { return buffer_.len() - offset_; }

 private:
  void consume(size_t len) { offset_ += len; }
  const uint8_t* ptr() const { return buffer_.data() + offset_; }

  DataBuffer buffer_;
  size_t offset_;
};

}  // namespace nss_test

#endif
