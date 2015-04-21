/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef tls_parser_h_
#define tls_parser_h_

#include <memory>
#include <cstdint>
#include <cstring>
#include <arpa/inet.h>
#include "databuffer.h"

namespace nss_test {

const uint8_t kTlsChangeCipherSpecType = 20;
const uint8_t kTlsAlertType = 21;
const uint8_t kTlsHandshakeType = 22;

const uint8_t kTlsHandshakeClientHello = 1;
const uint8_t kTlsHandshakeServerHello = 2;
const uint8_t kTlsHandshakeCertificate = 11;
const uint8_t kTlsHandshakeServerKeyExchange = 12;

const uint8_t kTlsAlertWarning = 1;
const uint8_t kTlsAlertFatal = 2;

const uint8_t kTlsAlertUnexpectedMessage = 10;
const uint8_t kTlsAlertHandshakeFailure = 40;
const uint8_t kTlsAlertIllegalParameter = 47;
const uint8_t kTlsAlertDecodeError = 50;
const uint8_t kTlsAlertUnsupportedExtension = 110;
const uint8_t kTlsAlertNoApplicationProtocol = 120;

const uint8_t kTlsFakeChangeCipherSpec[] = {
    kTlsChangeCipherSpecType,        // Type
    0xfe,                     0xff,  // Version
    0x00,                     0x00, 0x00, 0x00,
    0x00,                     0x00, 0x00, 0x10,  // Fictitious sequence #
    0x00,                     0x01,              // Length
    0x01                                         // Value
};

inline bool IsDtls(uint16_t version) {
  return (version & 0x8000) == 0x8000;
}

inline uint16_t NormalizeTlsVersion(uint16_t version) {
  if (version == 0xfeff) {
    return 0x0302; // special: DTLS 1.0 == TLS 1.1
  }
  if (IsDtls(version)) {
    return (version ^ 0xffff) + 0x0201;
  }
  return version;
}

inline void WriteVariable(DataBuffer* target, size_t index,
                          const DataBuffer& buf, size_t len_size) {
  target->Write(index, static_cast<uint32_t>(buf.len()), len_size);
  target->Write(index + len_size, buf.data(), buf.len());
}

class TlsParser {
 public:
  TlsParser(const uint8_t* data, size_t len)
      : buffer_(data, len), offset_(0) {}
  explicit TlsParser(const DataBuffer& buf)
      : buffer_(buf), offset_(0) {}

  bool Read(uint8_t* val);
  // Read an integral type of specified width.
  bool Read(uint32_t* val, size_t size);
  // Reads len bytes into dest buffer, overwriting it.
  bool Read(DataBuffer* dest, size_t len);
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

} // namespace nss_test

#endif
