/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SignedCertificateTimestamp_h
#define SignedCertificateTimestamp_h

#include "Buffer.h"
#include "mozpkix/Input.h"
#include "mozpkix/Result.h"

// Structures related to Certificate Transparency (RFC 6962).
namespace mozilla {
namespace ct {

// LogEntry struct in RFC 6962, Section 3.1.
struct LogEntry {
  // LogEntryType enum in RFC 6962, Section 3.1.
  enum class Type { X509 = 0, Precert = 1 };

  void Reset();

  Type type;

  // Set if type == X509.
  Buffer leafCertificate;

  // Set if type == Precert.
  Buffer issuerKeyHash;
  Buffer tbsCertificate;
};

// Helper structure to represent Digitally Signed data, as described in
// Sections 4.7 and 7.4.1.4.1 of RFC 5246.
struct DigitallySigned {
  enum class HashAlgorithm {
    None = 0,
    MD5 = 1,
    SHA1 = 2,
    SHA224 = 3,
    SHA256 = 4,
    SHA384 = 5,
    SHA512 = 6,
  };

  enum class SignatureAlgorithm { Anonymous = 0, RSA = 1, DSA = 2, ECDSA = 3 };

  // Returns true if |aHashAlgorithm| and |aSignatureAlgorithm|
  // match this DigitallySigned hash and signature algorithms.
  bool SignatureParametersMatch(HashAlgorithm aHashAlgorithm,
                                SignatureAlgorithm aSignatureAlgorithm) const;

  HashAlgorithm hashAlgorithm;
  SignatureAlgorithm signatureAlgorithm;
  // 'signature' field.
  Buffer signatureData;
};

// SignedCertificateTimestamp struct in RFC 6962, Section 3.2.
struct SignedCertificateTimestamp {
  // Version enum in RFC 6962, Section 3.2.
  enum class Version {
    V1 = 0,
  };

  Version version;
  Buffer logId;
  // "timestamp" is the current time in milliseconds, measured since the epoch,
  // ignoring leap seconds. See RFC 6962, Section 3.2.
  uint64_t timestamp;
  Buffer extensions;
  DigitallySigned signature;
};

inline pkix::Result BufferToInput(const Buffer& buffer, pkix::Input& input) {
  if (buffer.empty()) {
    return pkix::Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  return input.Init(buffer.data(), buffer.size());
}

inline void InputToBuffer(pkix::Input input, Buffer& buffer) {
  buffer.assign(input.UnsafeGetData(),
                input.UnsafeGetData() + input.GetLength());
}

}  // namespace ct
}  // namespace mozilla

#endif  // SignedCertificateTimestamp_h
