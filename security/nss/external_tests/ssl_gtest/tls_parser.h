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

namespace nss_test {

const uint8_t kTlsChangeCipherSpecType = 20;
const uint8_t kTlsAlertType = 21;
const uint8_t kTlsHandshakeType = 22;
const uint8_t kTlsApplicationDataType = 23;

const uint8_t kTlsHandshakeClientHello = 1;
const uint8_t kTlsHandshakeServerHello = 2;
const uint8_t kTlsHandshakeHelloRetryRequest = 6;
const uint8_t kTlsHandshakeEncryptedExtensions = 8;
const uint8_t kTlsHandshakeCertificate = 11;
const uint8_t kTlsHandshakeServerKeyExchange = 12;
const uint8_t kTlsHandshakeCertificateVerify = 15;
const uint8_t kTlsHandshakeClientKeyExchange = 16;
const uint8_t kTlsHandshakeFinished = 20;

const uint8_t kTlsAlertWarning = 1;
const uint8_t kTlsAlertFatal = 2;

const uint8_t kTlsAlertUnexpectedMessage = 10;
const uint8_t kTlsAlertBadRecordMac = 20;
const uint8_t kTlsAlertHandshakeFailure = 40;
const uint8_t kTlsAlertIllegalParameter = 47;
const uint8_t kTlsAlertDecodeError = 50;
const uint8_t kTlsAlertDecryptError = 51;
const uint8_t kTlsAlertMissingExtension = 109;
const uint8_t kTlsAlertUnsupportedExtension = 110;
const uint8_t kTlsAlertUnrecognizedName = 112;
const uint8_t kTlsAlertNoApplicationProtocol = 120;

const uint16_t kTlsSigSchemeRsaPkcs1Sha1 = 0x0201;
const uint16_t kTlsSigSchemeRsaPssSha256 = 0x0804;

const uint8_t kTlsFakeChangeCipherSpec[] = {
    kTlsChangeCipherSpecType,  // Type
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

// We don't export this yet. Yuck.
enum TlsSignatureScheme {
  kTlsSignatureNone = 0,
  kTlsSignatureRsaPkcs1Sha1 = 0x0201,
  kTlsSignatureRsaPkcs1Sha256 = 0x0401,
  kTlsSignatureRsaPkcs1Sha384 = 0x0501,
  kTlsSignatureRsaPkcs1Sha512 = 0x0601,
  kTlsSignatureEcdsaSecp256r1Sha256 = 0x0403,
  kTlsSignatureEcdsaSecp384r1Sha384 = 0x0503,
  kTlsSignatureEcdsaSecp521r1Sha512 = 0x0603,
  kTlsSignatureRsaPssSha256 = 0x0804,
  kTlsSignatureRsaPssSha384 = 0x0805,
  kTlsSignatureRsaPssSha512 = 0x0806,
  kTlsSignatureEd25519 = 0x0807,
  kTlsSignatureEd448 = 0x0808,
  kTlsSignatureDsaSha1 = 0x0202,
  kTlsSignatureDsaSha256 = 0x0402,
  kTlsSignatureDsaSha384 = 0x0502,
  kTlsSignatureDsaSha512 = 0x0602,
  kTlsSignatureEcdsaSha1 = 0x0203
};

static const uint8_t kTls13PskKe = 0;
static const uint8_t kTls13PskDhKe = 1;
static const uint8_t kTls13PskAuth = 0;
static const uint8_t kTls13PskSignAuth = 1;

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
