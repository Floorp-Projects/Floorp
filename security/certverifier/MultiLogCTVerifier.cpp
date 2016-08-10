/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MultiLogCTVerifier.h"

#include "CTObjectsExtractor.h"
#include "CTSerialization.h"
#include "mozilla/Move.h"

namespace mozilla { namespace ct {

using namespace mozilla::pkix;

// The possible verification statuses for a Signed Certificate Timestamp.
enum class SCTVerifyStatus {
  UnknownLog, // The SCT is from an unknown log and can not be verified.
  Invalid, // The SCT is from a known log, but the signature is invalid.
  OK // The SCT is from a known log, and the signature is valid.
};

// Note: this moves |sct| to the target list in |result|, invalidating |sct|.
static Result
StoreVerifiedSct(CTVerifyResult& result,
                 SignedCertificateTimestamp&& sct,
                 SCTVerifyStatus status)
{
  SCTList* target;
  switch (status) {
    case SCTVerifyStatus::UnknownLog:
      target = &result.unknownLogsScts;
      break;
    case SCTVerifyStatus::Invalid:
      target = &result.invalidScts;
      break;
    case SCTVerifyStatus::OK:
      target = &result.verifiedScts;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected SCTVerifyStatus type");
      return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  if (!target->append(Move(sct))) {
    return Result::FATAL_ERROR_NO_MEMORY;
  }
  return Success;
}

void
CTVerifyResult::Reset()
{
  verifiedScts.clear();
  invalidScts.clear();
  unknownLogsScts.clear();
  decodingErrors = 0;
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
                           uint64_t time,
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
                               uint64_t time,
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
                                    uint64_t time,
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
    return StoreVerifiedSct(result, Move(sct), SCTVerifyStatus::UnknownLog);
  }

  if (!matchingLog->SignatureParametersMatch(sct.signature)) {
    // SCT signature parameters do not match the log's.
    return StoreVerifiedSct(result, Move(sct), SCTVerifyStatus::Invalid);
  }

  Result rv = matchingLog->Verify(expectedEntry, sct);
  if (rv != Success) {
    if (rv == Result::ERROR_BAD_SIGNATURE) {
      return StoreVerifiedSct(result, Move(sct), SCTVerifyStatus::Invalid);
    }
    return rv;
  }

  // SCT verified ok, just make sure the timestamp is legitimate.
  if (sct.timestamp > time) {
    return StoreVerifiedSct(result, Move(sct), SCTVerifyStatus::Invalid);
  }

  return StoreVerifiedSct(result, Move(sct), SCTVerifyStatus::OK);
}

} } // namespace mozilla::ct
