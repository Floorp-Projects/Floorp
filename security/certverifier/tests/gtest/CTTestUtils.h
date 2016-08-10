/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CTTestUtils_h
#define CTTestUtils_h

#include <iostream>

#include "pkix/Input.h"
#include "pkix/Time.h"
#include "seccomon.h"
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
Buffer GetDEREncodedX509Cert();

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

// Certificate with embedded SCT in an X509v3 extension.
Buffer GetDEREncodedTestEmbeddedCert();

// For the above certificate, the corresponsing TBSCertificate without
// the embedded SCT extension.
Buffer GetDEREncodedTestTbsCert();

// As above, but signed with an intermediate CA certificate containing
// the CT extended key usage OID 1.3.6.1.4.1.11129.2.4.4 for issuing precerts
// (i.e. signed with a "precert CA certificate").
Buffer GetDEREncodedTestEmbeddedWithPreCACert();

// The issuer of the above certificates (self-signed root CA certificate).
Buffer GetDEREncodedCACert();

// An intermediate CA certificate issued by the above CA.
Buffer GetDEREncodedIntermediateCert();

// Certificate with embedded SCT signed by the intermediate certificate above.
Buffer GetDEREncodedTestEmbeddedWithIntermediateCert();

// As above, but signed by the precert CA certificate.
Buffer GetDEREncodedTestEmbeddedWithIntermediatePreCACert();

// Given a DER-encoded certificate, returns its SubjectPublicKeyInfo.
Buffer ExtractCertSPKI(pkix::Input cert);
Buffer ExtractCertSPKI(const Buffer& cert);

// Extracts a SignedCertificateTimestampList from the provided leaf certificate
// (kept in X.509v3 extension with OID 1.3.6.1.4.1.11129.2.4.2).
void ExtractEmbeddedSCTList(pkix::Input cert, Buffer& result);
void ExtractEmbeddedSCTList(const Buffer& cert, Buffer& result);

// Extracts a SignedCertificateTimestampList that has been embedded within
// an OCSP response as an extension with the OID 1.3.6.1.4.1.11129.2.4.5.
// The OCSP response is verified, and the verification must succeed for the
// extension to be extracted.
void ExtractSCTListFromOCSPResponse(pkix::Input cert,
                                    pkix::Input issuerSPKI,
                                    pkix::Input encodedResponse,
                                    pkix::Time time,
                                    Buffer& result);

// We need this in tests code since mozilla::Vector only allows move assignment.
Buffer cloneBuffer(const Buffer& buffer);

// Returns Input for the data stored in the buffer, failing assertion on error.
pkix::Input InputForBuffer(const Buffer& buffer);

// Returns Input for the data stored in the item, failing assertion on error.
pkix::Input InputForSECItem(const SECItem& item);

} } // namespace mozilla::ct


namespace mozilla {

// GTest needs this to be in Buffer's namespace (i.e. in mozilla::Vector's).
std::ostream& operator<<(std::ostream& stream, const ct::Buffer& buf);

} // namespace mozilla

#endif  // CTTestUtils_h
