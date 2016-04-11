/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CTSerialization_h
#define CTSerialization_h

#include "pkix/Input.h"
#include "pkix/Result.h"
#include "SignedCertificateTimestamp.h"
#include "SignedTreeHead.h"

// Utility functions for encoding/decoding structures used by Certificate
// Transparency to/from the TLS wire format encoding.
namespace mozilla { namespace ct {

// Encodes the DigitallySigned |data| to |output|.
pkix::Result EncodeDigitallySigned(const DigitallySigned& data,
                                   Buffer& output);

// Reads and decodes a DigitallySigned object from |reader|.
// On failure, the cursor position of |reader| is undefined.
pkix::Result DecodeDigitallySigned(pkix::Reader& reader,
                                   DigitallySigned& output);

// Encodes the |input| LogEntry to |output|. The size of the entry
// must not exceed the allowed size in RFC6962.
pkix::Result EncodeLogEntry(const LogEntry& entry, Buffer& output);

// Encodes the data signed by a Signed Certificate Timestamp (SCT) into
// |output|. The signature included in the SCT can then be verified over these
// bytes.
// |timestamp| timestamp from the SCT.
// |serializedLogEntry| the log entry signed by the SCT.
// |extensions| CT extensions.
pkix::Result EncodeV1SCTSignedData(uint64_t timestamp,
                                   pkix::Input serializedLogEntry,
                                   pkix::Input extensions,
                                   Buffer& output);

// Encodes the data signed by a Signed Tree Head (STH) |signedTreeHead| into
// |output|. The signature included in the |signedTreeHead| can then be
// verified over these bytes.
pkix::Result EncodeTreeHeadSignature(const SignedTreeHead& signedTreeHead,
                                     Buffer& output);

// Decodes a list of Signed Certificate Timestamps
// (SignedCertificateTimestampList as defined in RFC6962). This list
// is typically obtained from the CT extension in a certificate.
// To extract the individual items of the list, call ReadSCTListItem on
// the returned reader until the reader reaches its end.
// Note that the validity of each extracted SCT should be checked separately.
pkix::Result DecodeSCTList(pkix::Input input, pkix::Reader& listReader);

// Reads a single SCT from the reader returned by DecodeSCTList.
pkix::Result ReadSCTListItem(pkix::Reader& listReader, pkix::Input& result);

// Decodes a single SCT from |input| to |output|.
pkix::Result DecodeSignedCertificateTimestamp(pkix::Reader& input,
  SignedCertificateTimestamp& output);

} } // namespace mozilla::ct

#endif // CTSerialization_h
