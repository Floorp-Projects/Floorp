/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MultiLogCTVerifier.h"

#include "CTObjectsExtractor.h"
#include "CTSerialization.h"

namespace mozilla {
namespace ct {

using namespace mozilla::pkix;

// Note: this moves |verifiedSct| to the target list in |result|.
static void StoreVerifiedSct(CTVerifyResult& result, VerifiedSCT&& verifiedSct,
                             VerifiedSCT::Status status) {
  verifiedSct.status = status;
  result.verifiedScts.push_back(std::move(verifiedSct));
}

void MultiLogCTVerifier::AddLog(CTLogVerifier&& log) {
  mLogs.push_back(std::move(log));
}

Result MultiLogCTVerifier::Verify(Input cert, Input issuerSubjectPublicKeyInfo,
                                  Input sctListFromCert,
                                  Input sctListFromOCSPResponse,
                                  Input sctListFromTLSExtension, Time time,
                                  CTVerifyResult& result) {
  assert(cert.GetLength() > 0);
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
                    VerifiedSCT::Origin::Embedded, time, result);
    if (rv != Success) {
      return rv;
    }
  }

  LogEntry x509Entry;
  GetX509LogEntry(cert, x509Entry);

  // Verify SCTs from a stapled OCSP response
  if (sctListFromOCSPResponse.GetLength() > 0) {
    rv = VerifySCTs(sctListFromOCSPResponse, x509Entry,
                    VerifiedSCT::Origin::OCSPResponse, time, result);
    if (rv != Success) {
      return rv;
    }
  }

  // Verify SCTs from a TLS extension
  if (sctListFromTLSExtension.GetLength() > 0) {
    rv = VerifySCTs(sctListFromTLSExtension, x509Entry,
                    VerifiedSCT::Origin::TLSExtension, time, result);
    if (rv != Success) {
      return rv;
    }
  }
  return Success;
}

void DecodeSCTs(Input encodedSctList,
                std::vector<SignedCertificateTimestamp>& decodedSCTs,
                size_t& decodingErrors) {
  decodedSCTs.clear();

  Reader listReader;
  Result rv = DecodeSCTList(encodedSctList, listReader);
  if (rv != Success) {
    decodingErrors++;
    return;
  }

  while (!listReader.AtEnd()) {
    Input encodedSct;
    rv = ReadSCTListItem(listReader, encodedSct);
    if (rv != Success) {
      decodingErrors++;
      return;
    }

    Reader encodedSctReader(encodedSct);
    SignedCertificateTimestamp sct;
    rv = DecodeSignedCertificateTimestamp(encodedSctReader, sct);
    if (rv != Success) {
      decodingErrors++;
      continue;
    }
    decodedSCTs.push_back(std::move(sct));
  }
}

Result MultiLogCTVerifier::VerifySCTs(Input encodedSctList,
                                      const LogEntry& expectedEntry,
                                      VerifiedSCT::Origin origin, Time time,
                                      CTVerifyResult& result) {
  std::vector<SignedCertificateTimestamp> decodedSCTs;
  DecodeSCTs(encodedSctList, decodedSCTs, result.decodingErrors);
  for (auto sct : decodedSCTs) {
    Result rv =
        VerifySingleSCT(std::move(sct), expectedEntry, origin, time, result);
    if (rv != Success) {
      return rv;
    }
  }
  return Success;
}

Result MultiLogCTVerifier::VerifySingleSCT(SignedCertificateTimestamp&& sct,
                                           const LogEntry& expectedEntry,
                                           VerifiedSCT::Origin origin,
                                           Time time, CTVerifyResult& result) {
  VerifiedSCT verifiedSct;
  verifiedSct.origin = origin;
  verifiedSct.sct = std::move(sct);

  CTLogVerifier* matchingLog = nullptr;
  for (auto& log : mLogs) {
    if (log.keyId() == verifiedSct.sct.logId) {
      matchingLog = &log;
      break;
    }
  }

  if (!matchingLog) {
    // SCT does not match any known log.
    StoreVerifiedSct(result, std::move(verifiedSct),
                     VerifiedSCT::Status::UnknownLog);
    return Success;
  }

  verifiedSct.logOperatorId = matchingLog->operatorId();

  if (!matchingLog->SignatureParametersMatch(verifiedSct.sct.signature)) {
    // SCT signature parameters do not match the log's.
    StoreVerifiedSct(result, std::move(verifiedSct),
                     VerifiedSCT::Status::InvalidSignature);
    return Success;
  }

  Result rv = matchingLog->Verify(expectedEntry, verifiedSct.sct);
  if (rv != Success) {
    if (rv == Result::ERROR_BAD_SIGNATURE) {
      StoreVerifiedSct(result, std::move(verifiedSct),
                       VerifiedSCT::Status::InvalidSignature);
      return Success;
    }
    return rv;
  }

  // Make sure the timestamp is legitimate (not in the future).
  // SCT's |timestamp| is measured in milliseconds since the epoch,
  // ignoring leap seconds. When converting it to a second-level precision
  // pkix::Time, we need to round it either up or down. In our case, rounding up
  // (towards the future) is more "secure", although practically
  // it does not matter.
  Time sctTime =
      TimeFromEpochInSeconds((verifiedSct.sct.timestamp + 999u) / 1000u);
  if (sctTime > time) {
    StoreVerifiedSct(result, std::move(verifiedSct),
                     VerifiedSCT::Status::InvalidTimestamp);
    return Success;
  }

  // SCT verified ok, see if the log is qualified. Since SCTs from
  // disqualified logs are treated as valid under certain circumstances (see
  // the CT Policy), the log qualification check must be the last one we do.
  if (matchingLog->isDisqualified()) {
    verifiedSct.logDisqualificationTime = matchingLog->disqualificationTime();
    StoreVerifiedSct(result, std::move(verifiedSct),
                     VerifiedSCT::Status::ValidFromDisqualifiedLog);
    return Success;
  }

  StoreVerifiedSct(result, std::move(verifiedSct), VerifiedSCT::Status::Valid);
  return Success;
}

}  // namespace ct
}  // namespace mozilla
