/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SignedCertificateTimestamp_h
#define SignedCertificateTimestamp_h

#include "mozilla/Vector.h"
#include "pkix/Input.h"
#include "pkix/Result.h"

// Structures related to Certificate Transparency (RFC 6962).
namespace mozilla { namespace ct {

typedef Vector<uint8_t> Buffer;

// LogEntry struct in RFC 6962, Section 3.1.
struct LogEntry
{

  // LogEntryType enum in RFC 6962, Section 3.1.
  enum class Type {
    X509 = 0,
    Precert = 1
  };

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
struct DigitallySigned
{
  enum class HashAlgorithm {
    None = 0,
    MD5 = 1,
    SHA1 = 2,
    SHA224 = 3,
    SHA256 = 4,
    SHA384 = 5,
    SHA512 = 6,
  };

  enum class SignatureAlgorithm {
    Anonymous = 0,
    RSA = 1,
    DSA = 2,
    ECDSA = 3
  };

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
struct SignedCertificateTimestamp
{
  // Version enum in RFC 6962, Section 3.2.
  enum class Version {
    V1 = 0,
  };

  // Source of the SCT - supplementary, not defined in CT RFC.
  // Note: The numeric values are used within histograms and should not change
  // or be re-assigned.
  enum class Origin {
    Embedded = 0,
    TLSExtension = 1,
    OCSPResponse = 2,
  };

  Version version;
  Buffer logId;
  // "timestamp" is the current time in milliseconds, measured since the epoch,
  // ignoring leap seconds. See RFC 6962, Section 3.2.
  uint64_t timestamp;
  Buffer extensions;
  DigitallySigned signature;
  Origin origin;
};


inline pkix::Result BufferToInput(const Buffer& buffer, pkix::Input& input)
{
  return input.Init(buffer.begin(), buffer.length());
}

inline pkix::Result InputToBuffer(pkix::Input input, Buffer& buffer)
{
  buffer.clear();
  if (!buffer.append(input.UnsafeGetData(), input.GetLength())) {
    return pkix::Result::FATAL_ERROR_NO_MEMORY;
  }
  return pkix::Success;
}

} } // namespace mozilla::ct

namespace mozilla {

// Comparison operators are placed under mozilla namespace since
// mozilla::ct::Buffer is actually mozilla::Vector.
bool operator==(const ct::Buffer& a, const ct::Buffer& b);
bool operator!=(const ct::Buffer& a, const ct::Buffer& b);

} // namespace mozilla

#endif // SignedCertificateTimestamp_h
