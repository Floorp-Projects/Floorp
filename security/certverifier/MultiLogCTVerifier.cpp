/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MultiLogCTVerifier.h"

#include "CTObjectsExtractor.h"
#include "CTSerialization.h"
#include "mozilla/Assertions.h"
#include "mozilla/Move.h"

namespace mozilla { namespace ct {

using namespace mozilla::pkix;

// Note: this moves |sct| to the target list in |result|, invalidating |sct|.
static Result
StoreVerifiedSct(CTVerifyResult& result,
                 SignedCertificateTimestamp&& sct,
                 SignedCertificateTimestamp::VerificationStatus status)
{
  sct.verificationStatus = status;
  if (!result.scts.append(Move(sct))) {
    return Result::FATAL_ERROR_NO_MEMORY;
  }
  return Success;
}

Result
MultiLogCTVerifier::AddLog(Input publicKey)
{
  CTLogVerifier log;
  Result rv = log.Init(publicKey);
  if (rv != Success) {
    return rv;
  }
  if (!mLogs.append(Move(log))) {
    return Result::FATAL_ERROR_NO_MEMORY;
  }
  return Success;
}

Result
MultiLogCTVerifier::Verify(Input cert,
                           Input issuerSubjectPublicKeyInfo,
                           Input sctListFromCert,
                           Input sctListFromOCSPResponse,
                           Input sctListFromTLSExtension,
                           Time time,
                           CTVerifyResult& result)
{
  MOZ_ASSERT(cert.GetLength() > 0);
  result.Reset();

  Result rv;

  // Verify embedded SCTs
  if (issuerSubjectPublicKeyInfo.GetLength() > 0 &&
      sctListFromCert.GetLength() > 0) {
    LogEntry precertEntry;
    rv = GetPrecertLogEntry(cert, issuerSubjectPublicKeyInfo, precertEntry);
    if (rv != Success) {
      return rv;
    }
    rv = VerifySCTs(sctListFromCert, precertEntry,
                    SignedCertificateTimestamp::Origin::Embedded, time,
                    result);
    if (rv != Success) {
      return rv;
    }
  }

  LogEntry x509Entry;
  rv = GetX509LogEntry(cert, x509Entry);
  if (rv != Success) {
    return rv;
  }

  // Verify SCTs from a stapled OCSP response
  if (sctListFromOCSPResponse.GetLength() > 0) {
    rv = VerifySCTs(sctListFromOCSPResponse, x509Entry,
                    SignedCertificateTimestamp::Origin::OCSPResponse, time,
                    result);
    if (rv != Success) {
      return rv;
    }
  }

  // Verify SCTs from a TLS extension
  if (sctListFromTLSExtension.GetLength() > 0) {
    rv = VerifySCTs(sctListFromTLSExtension, x509Entry,
                    SignedCertificateTimestamp::Origin::TLSExtension, time,
                    result);
    if (rv != Success) {
      return rv;
    }
  }
  return Success;
}

Result
MultiLogCTVerifier::VerifySCTs(Input encodedSctList,
                               const LogEntry& expectedEntry,
                               SignedCertificateTimestamp::Origin origin,
                               Time time,
                               CTVerifyResult& result)
{
  Reader listReader;
  Result rv = DecodeSCTList(encodedSctList, listReader);
  if (rv != Success) {
    result.decodingErrors++;
    return Success;
  }

  while (!listReader.AtEnd()) {
    Input encodedSct;
    rv = ReadSCTListItem(listReader, encodedSct);
    if (rv != Success) {
      result.decodingErrors++;
      return Success;
    }

    Reader encodedSctReader(encodedSct);
    SignedCertificateTimestamp sct;
    rv = DecodeSignedCertificateTimestamp(encodedSctReader, sct);
    if (rv != Success) {
      result.decodingErrors++;
      continue;
    }
    sct.origin = origin;

    rv = VerifySingleSCT(Move(sct), expectedEntry, time, result);
    if (rv != Success) {
      return rv;
    }
  }
  return Success;
}

Result
MultiLogCTVerifier::VerifySingleSCT(SignedCertificateTimestamp&& sct,
                                    const LogEntry& expectedEntry,
                                    Time time,
                                    CTVerifyResult& result)
{
  CTLogVerifier* matchingLog = nullptr;
  for (auto& log : mLogs) {
    if (log.keyId() == sct.logId) {
      matchingLog = &log;
      break;
    }
  }

  if (!matchingLog) {
    // SCT does not match any known log.
    return StoreVerifiedSct(result, Move(sct),
      SignedCertificateTimestamp::VerificationStatus::UnknownLog);
  }

  if (!matchingLog->SignatureParametersMatch(sct.signature)) {
    // SCT signature parameters do not match the log's.
    return StoreVerifiedSct(result, Move(sct),
      SignedCertificateTimestamp::VerificationStatus::InvalidSignature);
  }

  Result rv = matchingLog->Verify(expectedEntry, sct);
  if (rv != Success) {
    if (rv == Result::ERROR_BAD_SIGNATURE) {
      return StoreVerifiedSct(result, Move(sct),
        SignedCertificateTimestamp::VerificationStatus::InvalidSignature);
    }
    return rv;
  }

  // |sct.timestamp| is measured in milliseconds since the epoch,
  // ignoring leap seconds. When converting it to a second-level precision
  // pkix::Time, we need to round it either up or down. In our case, rounding up
  // is more "secure", although practically it does not matter.
  Time sctTime = TimeFromEpochInSeconds((sct.timestamp + 999u) / 1000u);

  // SCT verified ok, just make sure the timestamp is legitimate.
  if (sctTime > time) {
    return StoreVerifiedSct(result, Move(sct),
      SignedCertificateTimestamp::VerificationStatus::InvalidTimestamp);
  }

  return StoreVerifiedSct(result, Move(sct),
    SignedCertificateTimestamp::VerificationStatus::OK);
}

} } // namespace mozilla::ct
