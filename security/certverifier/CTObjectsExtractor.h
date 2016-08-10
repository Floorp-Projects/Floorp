/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CTObjectsExtractor_h
#define CTObjectsExtractor_h

#include "pkix/Input.h"
#include "pkix/Result.h"
#include "SignedCertificateTimestamp.h"

namespace mozilla { namespace ct {

// Obtains a PrecertChain log entry for |leafCertificate|, a DER-encoded
// X.509v3 certificate that contains an X.509v3 extension with the
// OID 1.3.6.1.4.1.11129.2.4.2.
// |issuerSubjectPublicKeyInfo| is a DER-encoded SPKI of |leafCertificate|'s
// issuer.
// On success, fills |output| with the data for a PrecertChain log entry.
// If |leafCertificate| does not contain the required extension,
// an error is returned.
// The returned |output| is intended to be verified by CTLogVerifier::Verify.
// Note that |leafCertificate| is not checked for validity or well-formedness.
// You might want to validate it first using pkix::BuildCertChain or similar.
pkix::Result GetPrecertLogEntry(pkix::Input leafCertificate,
                                pkix::Input issuerSubjectPublicKeyInfo,
                                LogEntry& output);

// Obtains an X509Chain log entry for |leafCertificate|, a DER-encoded
// X.509v3 certificate that is not expected to contain an X.509v3 extension
// with the OID 1.3.6.1.4.1.11129.2.4.2 (meaning a certificate without
// an embedded SCT).
// On success, fills |output| with the data for an X509Chain log entry.
// The returned |output| is intended to be verified by CTLogVerifier::Verify.
// Note that |leafCertificate| is not checked for validity or well-formedness.
// You might want to validate it first using pkix::BuildCertChain or similar.
pkix::Result GetX509LogEntry(pkix::Input leafCertificate, LogEntry& output);

} } // namespace mozilla::ct

#endif  // CTObjectsExtractor_h
