/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CTTestUtils_h
#define CTTestUtils_h

#include <iostream>

#include "pkix/Input.h"
#include "SignedCertificateTimestamp.h"
#include "SignedTreeHead.h"

namespace mozilla { namespace ct {

// Note: unless specified otherwise, all test data is taken from
// Certificate Transparency test data repository at
// https://github.com/google/certificate-transparency/tree/master/test/testdata

// Fills |entry| with test data for an X.509 entry.
void GetX509CertLogEntry(LogEntry& entry);

// Returns a DER-encoded X509 cert. The SCT provided by
// GetX509CertSCT is signed over this certificate.
Buffer GetDerEncodedX509Cert();

// Fills |entry| with test data for a Precertificate entry.
void GetPrecertLogEntry(LogEntry& entry);

// Returns the binary representation of a test DigitallySigned.
Buffer GetTestDigitallySigned();

// Returns the source data of the test DigitallySigned.
Buffer GetTestDigitallySignedData();

// Returns the binary representation of a test serialized SCT.
Buffer GetTestSignedCertificateTimestamp();

// Test log key.
Buffer GetTestPublicKey();

// ID of test log key.
Buffer GetTestPublicKeyId();

// SCT for the X509Certificate provided above.
void GetX509CertSCT(SignedCertificateTimestamp& sct);

// SCT for the Precertificate log entry provided above.
void GetPrecertSCT(SignedCertificateTimestamp& sct);

// Issuer key hash.
Buffer GetDefaultIssuerKeyHash();

// A sample, valid STH.
void GetSampleSignedTreeHead(SignedTreeHead& sth);

// The SHA256 root hash for the sample STH.
Buffer GetSampleSTHSHA256RootHash();

// The tree head signature for the sample STH.
Buffer GetSampleSTHTreeHeadSignature();

// The same signature as GetSampleSTHTreeHeadSignature, decoded.
void GetSampleSTHTreeHeadDecodedSignature(DigitallySigned& signature);

// We need this in tests code since mozilla::Vector only allows move assignment.
Buffer cloneBuffer(const Buffer& buffer);

// Returns Input for the data stored in the buffer, failing assertion on error.
pkix::Input InputForBuffer(const Buffer& buffer);

} } // namespace mozilla::ct


namespace mozilla {

// GTest needs this to be in Buffer's namespace (i.e. in mozilla::Vector's).
std::ostream& operator<<(std::ostream& stream, const ct::Buffer& buf);

} // namespace mozilla

#endif  // CTTestUtils_h
