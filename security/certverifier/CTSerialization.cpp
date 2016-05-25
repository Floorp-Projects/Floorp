/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CTSerialization.h"

#include <stdint.h>

#include "mozilla/Assertions.h"
#include "mozilla/TypeTraits.h"

namespace mozilla { namespace ct {

using namespace mozilla::pkix;

// Note: length is always specified in bytes.
// Signed Certificate Timestamp (SCT) Version length
static const size_t kVersionLength = 1;

// Members of a V1 SCT
static const size_t kLogIdLength = 32;
static const size_t kTimestampLength = 8;
static const size_t kExtensionsLengthBytes = 2;
static const size_t kHashAlgorithmLength = 1;
static const size_t kSigAlgorithmLength = 1;
static const size_t kSignatureLengthBytes = 2;

// Members of the digitally-signed struct of a V1 SCT
static const size_t kSignatureTypeLength = 1;
static const size_t kLogEntryTypeLength = 2;
static const size_t kAsn1CertificateLengthBytes = 3;
static const size_t kTbsCertificateLengthBytes = 3;

static const size_t kSCTListLengthBytes = 2;
static const size_t kSerializedSCTLengthBytes = 2;

// Members of digitally-signed struct of a STH
static const size_t kTreeSizeLength = 8;

// Length of sha256RootHash buffer of SignedTreeHead
static const size_t kSthRootHashLength = 32;

enum class SignatureType {
  CertificateTimestamp = 0,
  TreeHash = 1,
};

// Reads a TLS-encoded variable length unsigned integer from |in|.
// The integer is expected to be in big-endian order, which is used by TLS.
// Note: does not check if the output parameter overflows while reading.
// |length| indicates the size (in bytes) of the serialized integer.
static Result
UncheckedReadUint(size_t length, Reader& in, uint64_t& out)
{
  uint64_t result = 0;
  for (size_t i = 0; i < length; ++i) {
    uint8_t value;
    Result rv = in.Read(value);
    if (rv != Success) {
      return rv;
    }
    result = (result << 8) | value;
  }
  out = result;
  return Success;
}

// Performs overflow sanity checks and calls UncheckedReadUint.
template <size_t length, typename T>
static inline Result
ReadUint(Reader& in, T& out)
{
  uint64_t value;
  static_assert(mozilla::IsUnsigned<T>::value, "T must be unsigned");
  static_assert(length <= 8, "At most 8 byte integers can be read");
  static_assert(sizeof(T) >= length, "T must be able to hold <length> bytes");
  Result rv = UncheckedReadUint(length, in, value);
  if (rv != Success) {
    return rv;
  }
  out = static_cast<T>(value);
  return Success;
}

// Reads |length| bytes from |in|.
static Result
ReadFixedBytes(size_t length, Reader& in, Input& out)
{
  return in.Skip(length, out);
}

// Reads a length-prefixed variable amount of bytes from |in|, updating |out|
// on success. |prefixLength| indicates the number of bytes needed to represent
// the length.
template <size_t prefixLength>
static inline Result
ReadVariableBytes(Reader& in, Input& out)
{
  size_t length;
  Result rv = ReadUint<prefixLength>(in, length);
  if (rv != Success) {
    return rv;
  }
  return ReadFixedBytes(length, in, out);
}

// Reads a serialized hash algorithm.
static Result
ReadHashAlgorithm(Reader& in, DigitallySigned::HashAlgorithm& out)
{
  unsigned int value;
  Result rv = ReadUint<kHashAlgorithmLength>(in, value);
  if (rv != Success) {
    return rv;
  }
  DigitallySigned::HashAlgorithm algo =
    static_cast<DigitallySigned::HashAlgorithm>(value);
  switch (algo) {
    case DigitallySigned::HashAlgorithm::None:
    case DigitallySigned::HashAlgorithm::MD5:
    case DigitallySigned::HashAlgorithm::SHA1:
    case DigitallySigned::HashAlgorithm::SHA224:
    case DigitallySigned::HashAlgorithm::SHA256:
    case DigitallySigned::HashAlgorithm::SHA384:
    case DigitallySigned::HashAlgorithm::SHA512:
      out = algo;
      return Success;
  }
  return Result::ERROR_BAD_DER;
}

// Reads a serialized signature algorithm.
static Result
ReadSignatureAlgorithm(Reader& in, DigitallySigned::SignatureAlgorithm& out)
{
  unsigned int value;
  Result rv = ReadUint<kSigAlgorithmLength>(in, value);
  if (rv != Success) {
    return rv;
  }
  DigitallySigned::SignatureAlgorithm algo =
    static_cast<DigitallySigned::SignatureAlgorithm>(value);
  switch (algo) {
    case DigitallySigned::SignatureAlgorithm::Anonymous:
    case DigitallySigned::SignatureAlgorithm::RSA:
    case DigitallySigned::SignatureAlgorithm::DSA:
    case DigitallySigned::SignatureAlgorithm::ECDSA:
      out = algo;
      return Success;
  }
  return Result::ERROR_BAD_DER;
}

// Reads a serialized version enum.
static Result
ReadVersion(Reader& in, SignedCertificateTimestamp::Version& out)
{
  unsigned int value;
  Result rv = ReadUint<kVersionLength>(in, value);
  if (rv != Success) {
    return rv;
  }
  SignedCertificateTimestamp::Version version =
    static_cast<SignedCertificateTimestamp::Version>(value);
  switch (version) {
    case SignedCertificateTimestamp::Version::V1:
      out = version;
      return Success;
  }
  return Result::ERROR_BAD_DER;
}

// Writes a TLS-encoded variable length unsigned integer to |output|.
// Note: range/overflow checks are not performed on the input parameters.
// |length| indicates the size (in bytes) of the integer to be written.
// |value| the value itself to be written.
static Result
UncheckedWriteUint(size_t length, uint64_t value, Buffer& output)
{
  if (!output.reserve(length + output.length())) {
    return Result::FATAL_ERROR_NO_MEMORY;
  }
  for (; length > 0; --length) {
    uint8_t nextByte = (value >> ((length - 1) * 8)) & 0xFF;
    output.infallibleAppend(nextByte);
  }
  return Success;
}

// Performs sanity checks on T and calls UncheckedWriteUint.
template <size_t length, typename T>
static inline Result
WriteUint(T value, Buffer& output)
{
  static_assert(length <= 8, "At most 8 byte integers can be written");
  static_assert(sizeof(T) >= length, "T must be able to hold <length> bytes");
  if (mozilla::IsSigned<T>::value) {
    // We accept signed integer types assuming the actual value is non-negative.
    if (value < 0) {
      return Result::FATAL_ERROR_INVALID_ARGS;
    }
  }
  if (sizeof(T) > length) {
    // We allow the value variable to take more bytes than is written,
    // but the unwritten bytes must be zero.
    // Note: when "sizeof(T) == length" holds, "value >> (length * 8)" is
    // undefined since the shift is too big. On some compilers, this would
    // produce a warning even though the actual code is unreachable.
    if (value >> (length * 8 - 1) > 1) {
      return Result::FATAL_ERROR_INVALID_ARGS;
    }
  }
  return UncheckedWriteUint(length, static_cast<uint64_t>(value), output);
}

// Writes an array to |output| from |input|.
// Should be used in one of two cases:
// * The length of |input| has already been encoded into the |output| stream.
// * The length of |input| is fixed and the reader is expected to specify that
// length when reading.
// If the length of |input| is dynamic and data is expected to follow it,
// WriteVariableBytes must be used.
static Result
WriteEncodedBytes(Input input, Buffer& output)
{
  if (!output.append(input.UnsafeGetData(), input.GetLength())) {
    return Result::FATAL_ERROR_NO_MEMORY;
  }
  return Success;
}

// Same as above, but the source data is in a Buffer.
static Result
WriteEncodedBytes(const Buffer& source, Buffer& output)
{
  if (!output.appendAll(source)) {
    return Result::FATAL_ERROR_NO_MEMORY;
  }
  return Success;
}

// Writes a variable-length array to |output|.
// |prefixLength| indicates the number of bytes needed to represent the length.
// |input| is the array itself.
// Fails if the size of |input| is more than 2^|prefixLength| - 1.
template <size_t prefixLength>
static Result
WriteVariableBytes(Input input, Buffer& output)
{
  size_t inputSize = input.GetLength();
  const size_t maxAllowedInputSize =
    static_cast<size_t>(((1 << (prefixLength * 8)) - 1));
  if (inputSize > maxAllowedInputSize) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }

  Result rv = WriteUint<prefixLength>(inputSize, output);
  if (rv != Success) {
    return rv;
  }
  return WriteEncodedBytes(input, output);
}

// Same as above, but the source data is in a Buffer.
template <size_t prefixLength>
static Result
WriteVariableBytes(const Buffer& source, Buffer& output)
{
  Input input;
  Result rv = BufferToInput(source, input);
  if (rv != Success) {
    return rv;
  }
  return WriteVariableBytes<prefixLength>(input, output);
}

// Writes a LogEntry of type X.509 cert to |output|.
// |input| is the LogEntry containing the certificate.
static Result
EncodeAsn1CertLogEntry(const LogEntry& entry, Buffer& output)
{
  return WriteVariableBytes<kAsn1CertificateLengthBytes>(entry.leafCertificate,
                                                         output);
}

// Writes a LogEntry of type PreCertificate to |output|.
// |input| is the LogEntry containing the TBSCertificate and issuer key hash.
static Result
EncodePrecertLogEntry(const LogEntry& entry, Buffer& output)
{
  if (entry.issuerKeyHash.length() != kLogIdLength) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }
  Result rv = WriteEncodedBytes(entry.issuerKeyHash, output);
  if (rv != Success) {
    return rv;
  }
  return WriteVariableBytes<kTbsCertificateLengthBytes>(entry.tbsCertificate,
                                                        output);
}


Result
EncodeDigitallySigned(const DigitallySigned& data, Buffer& output)
{
  Result rv = WriteUint<kHashAlgorithmLength>(
    static_cast<unsigned int>(data.hashAlgorithm), output);
  if (rv != Success) {
    return rv;
  }
  rv = WriteUint<kSigAlgorithmLength>(
    static_cast<unsigned int>(data.signatureAlgorithm), output);
  if (rv != Success) {
    return rv;
  }
  return WriteVariableBytes<kSignatureLengthBytes>(data.signatureData, output);
}

Result
DecodeDigitallySigned(Reader& reader, DigitallySigned& output)
{
  DigitallySigned result;

  Result rv = ReadHashAlgorithm(reader, result.hashAlgorithm);
  if (rv != Success) {
    return rv;
  }
  rv = ReadSignatureAlgorithm(reader, result.signatureAlgorithm);
  if (rv != Success) {
    return rv;
  }

  Input signatureData;
  rv = ReadVariableBytes<kSignatureLengthBytes>(reader, signatureData);
  if (rv != Success) {
    return rv;
  }
  rv = InputToBuffer(signatureData, result.signatureData);
  if (rv != Success) {
    return rv;
  }

  output = Move(result);
  return Success;
}

Result
EncodeLogEntry(const LogEntry& entry, Buffer& output)
{
  Result rv = WriteUint<kLogEntryTypeLength>(
    static_cast<unsigned int>(entry.type), output);
  if (rv != Success) {
    return rv;
  }
  switch (entry.type) {
    case LogEntry::Type::X509:
      return EncodeAsn1CertLogEntry(entry, output);
    case LogEntry::Type::Precert:
      return EncodePrecertLogEntry(entry, output);
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected LogEntry type");
  }
  return Result::ERROR_BAD_DER;
}

static Result
WriteTimeSinceEpoch(uint64_t timestamp, Buffer& output)
{
  return WriteUint<kTimestampLength>(timestamp, output);
}

Result
EncodeV1SCTSignedData(uint64_t timestamp, Input serializedLogEntry,
                      Input extensions, Buffer& output)
{
  Result rv = WriteUint<kVersionLength>(static_cast<unsigned int>(
    SignedCertificateTimestamp::Version::V1), output);
  if (rv != Success) {
    return rv;
  }
  rv = WriteUint<kSignatureTypeLength>(static_cast<unsigned int>(
    SignatureType::CertificateTimestamp), output);
  if (rv != Success) {
    return rv;
  }
  rv = WriteTimeSinceEpoch(timestamp, output);
  if (rv != Success) {
    return rv;
  }
  // NOTE: serializedLogEntry must already be serialized and contain the
  // length as the prefix.
  rv = WriteEncodedBytes(serializedLogEntry, output);
  if (rv != Success) {
    return rv;
  }
  return WriteVariableBytes<kExtensionsLengthBytes>(extensions, output);
}

Result
EncodeTreeHeadSignature(const SignedTreeHead& signedTreeHead,
                        Buffer& output)
{
  Result rv = WriteUint<kVersionLength>(
    static_cast<unsigned int>(signedTreeHead.version), output);
  if (rv != Success) {
    return rv;
  }
  rv = WriteUint<kSignatureTypeLength>(
    static_cast<unsigned int>(SignatureType::TreeHash), output);
  if (rv != Success) {
    return rv;
  }
  rv = WriteTimeSinceEpoch(signedTreeHead.timestamp, output);
  if (rv != Success) {
    return rv;
  }
  rv = WriteUint<kTreeSizeLength>(signedTreeHead.treeSize, output);
  if (rv != Success) {
    return rv;
  }
  if (signedTreeHead.sha256RootHash.length() != kSthRootHashLength) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }
  return WriteEncodedBytes(signedTreeHead.sha256RootHash, output);
}

Result
DecodeSCTList(Input input, Reader& listReader)
{
  Reader inputReader(input);
  Input listData;
  Result rv = ReadVariableBytes<kSCTListLengthBytes>(inputReader, listData);
  if (rv != Success) {
    return rv;
  }
  return listReader.Init(listData);
}

Result
ReadSCTListItem(Reader& listReader, Input& output)
{
  if (listReader.AtEnd()) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }

  Result rv = ReadVariableBytes<kSerializedSCTLengthBytes>(listReader, output);
  if (rv != Success) {
    return rv;
  }
  if (output.GetLength() == 0) {
    return Result::ERROR_BAD_DER;
  }
  return Success;
}

Result
DecodeSignedCertificateTimestamp(Reader& reader,
                                 SignedCertificateTimestamp& output)
{
  SignedCertificateTimestamp result;

  Result rv = ReadVersion(reader, result.version);
  if (rv != Success) {
    return rv;
  }

  uint64_t timestamp;
  Input logId;
  Input extensions;

  rv = ReadFixedBytes(kLogIdLength, reader, logId);
  if (rv != Success) {
    return rv;
  }
  rv = ReadUint<kTimestampLength>(reader, timestamp);
  if (rv != Success) {
    return rv;
  }
  rv = ReadVariableBytes<kExtensionsLengthBytes>(reader, extensions);
  if (rv != Success) {
    return rv;
  }
  rv = DecodeDigitallySigned(reader, result.signature);
  if (rv != Success) {
    return rv;
  }

  rv = InputToBuffer(logId, result.logId);
  if (rv != Success) {
    return rv;
  }
  rv = InputToBuffer(extensions, result.extensions);
  if (rv != Success) {
    return rv;
  }
  result.timestamp = timestamp;

  output = Move(result);
  return Success;
}

} } // namespace mozilla::ct
