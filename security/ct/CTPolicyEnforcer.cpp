/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CTPolicyEnforcer.h"

#include <algorithm>
#include <stdint.h>
#include <vector>

namespace mozilla {
namespace ct {

using namespace mozilla::pkix;

// Returns the number of embedded SCTs required to be present on a
// certificate for Qualification Case #2 (embedded SCTs).
static size_t GetRequiredEmbeddedSctsCount(
    size_t certLifetimeInFullCalendarMonths) {
  // "there are Embedded SCTs from AT LEAST N+1 once or currently qualified
  // logs, where N is the lifetime of the certificate in years (normally
  // rounding up, but rounding down when up to 3 months over), and must be
  // at least 1"
  return 1 + (certLifetimeInFullCalendarMonths + 9) / 12;
}

// Whether a valid embedded SCT is present in the list.
static bool HasValidEmbeddedSct(const VerifiedSCTList& verifiedScts) {
  for (const VerifiedSCT& verifiedSct : verifiedScts) {
    if (verifiedSct.status == VerifiedSCT::Status::Valid &&
        verifiedSct.origin == VerifiedSCT::Origin::Embedded) {
      return true;
    }
  }
  return false;
}

// Whether a valid non-embedded SCT is present in the list.
static bool HasValidNonEmbeddedSct(const VerifiedSCTList& verifiedScts) {
  for (const VerifiedSCT& verifiedSct : verifiedScts) {
    if (verifiedSct.status == VerifiedSCT::Status::Valid &&
        (verifiedSct.origin == VerifiedSCT::Origin::TLSExtension ||
         verifiedSct.origin == VerifiedSCT::Origin::OCSPResponse)) {
      return true;
    }
  }
  return false;
}

// Given a list of verified SCTs, counts the number of distinct CA-independent
// log operators running the CT logs that issued the SCTs which satisfy
// the provided boolean predicate.
template <typename SelectFunc>
void CountIndependentLogOperatorsForSelectedScts(
    const VerifiedSCTList& verifiedScts,
    const CTLogOperatorList& dependentOperators, size_t& count,
    SelectFunc selected) {
  CTLogOperatorList operatorIds;
  for (const VerifiedSCT& verifiedSct : verifiedScts) {
    CTLogOperatorId sctLogOperatorId = verifiedSct.logOperatorId;
    // Check if |sctLogOperatorId| is CA-dependent.
    bool isDependentOperator = false;
    for (CTLogOperatorId dependentOperator : dependentOperators) {
      if (sctLogOperatorId == dependentOperator) {
        isDependentOperator = true;
        break;
      }
    }
    if (isDependentOperator || !selected(verifiedSct)) {
      continue;
    }
    // Check if |sctLogOperatorId| is in |operatorIds|...
    bool alreadyAdded = false;
    for (CTLogOperatorId id : operatorIds) {
      if (id == sctLogOperatorId) {
        alreadyAdded = true;
        break;
      }
    }
    // ...and if not, add it.
    if (!alreadyAdded) {
      operatorIds.push_back(sctLogOperatorId);
    }
  }
  count = operatorIds.size();
}

// Given a list of verified SCTs, counts the number of distinct CT logs
// that issued the SCTs that satisfy the |selected| predicate.
template <typename SelectFunc>
static void CountLogsForSelectedScts(const VerifiedSCTList& verifiedScts,
                                     size_t& count, SelectFunc selected) {
  // Keep pointers to log ids (of type Buffer) from |verifiedScts| to save on
  // memory allocations.
  std::vector<const Buffer*> logIds;
  for (const VerifiedSCT& verifiedSct : verifiedScts) {
    if (!selected(verifiedSct)) {
      continue;
    }

    const Buffer* sctLogId = &verifiedSct.sct.logId;
    // Check if |sctLogId| points to data already in |logIds|...
    bool alreadyAdded = false;
    for (const Buffer* logId : logIds) {
      if (*logId == *sctLogId) {
        alreadyAdded = true;
        break;
      }
    }
    // ...and if not, add it.
    if (!alreadyAdded) {
      logIds.push_back(sctLogId);
    }
  }
  count = logIds.size();
}

// Calculates the effective issuance time of connection's certificate using
// the SCTs present on the connection (we can't rely on notBefore validity
// field of the certificate since it can be backdated).
// Used to determine whether to accept SCTs issued by past qualified logs.
// The effective issuance time is defined as the earliest of all SCTs,
// rather than the latest of embedded SCTs, in order to give CAs the benefit
// of the doubt in the event a log is revoked in the midst of processing
// a precertificate and issuing the certificate.
// It is acceptable to ignore the origin of the SCTs because SCTs
// delivered via OCSP/TLS extension will cover the full certificate,
// which necessarily will exist only after the precertificate
// has been logged and the actual certificate issued.
static uint64_t GetEffectiveCertIssuanceTime(
    const VerifiedSCTList& verifiedScts) {
  uint64_t result = UINT64_MAX;
  for (const VerifiedSCT& verifiedSct : verifiedScts) {
    if (verifiedSct.status == VerifiedSCT::Status::Valid) {
      result = std::min(result, verifiedSct.sct.timestamp);
    }
  }
  return result;
}

// Checks if the log that issued the given SCT is "once or currently qualified"
// (i.e. was qualified at the time of the certificate issuance). In addition,
// makes sure the SCT is before the disqualification.
static bool LogWasQualifiedForSct(const VerifiedSCT& verifiedSct,
                                  uint64_t certIssuanceTime) {
  if (verifiedSct.status == VerifiedSCT::Status::Valid) {
    return true;
  }
  if (verifiedSct.status == VerifiedSCT::Status::ValidFromDisqualifiedLog) {
    uint64_t logDisqualificationTime = verifiedSct.logDisqualificationTime;
    return certIssuanceTime < logDisqualificationTime &&
           verifiedSct.sct.timestamp < logDisqualificationTime;
  }
  return false;
}

// "A certificate is CT Qualified if it is presented with at least two SCTs
// from once or currently qualified logs run by a minimum of two entities
// independent of the CA and of each other."
// By the preexisting certificate exception provision (not currently
// implemented), certificates "are CT Qualified if they are presented with SCTs
// from once or currently qualified logs run by a minimum of one entity
// independent of the CA."
static void CheckOperatorDiversityCompliance(
    const VerifiedSCTList& verifiedScts, uint64_t certIssuanceTime,
    const CTLogOperatorList& dependentOperators, bool& compliant) {
  size_t independentOperatorsCount;
  CountIndependentLogOperatorsForSelectedScts(
      verifiedScts, dependentOperators, independentOperatorsCount,
      [certIssuanceTime](const VerifiedSCT& verifiedSct) -> bool {
        return LogWasQualifiedForSct(verifiedSct, certIssuanceTime);
      });
  // Having at least 2 operators implies we have at least 2 SCTs.
  // For the preexisting certificate exception provision (1 operator) we will
  // need to include an additional SCTs count check using
  // CountLogsForSelectedScts(verifiedScts, sctsCount,
  //   [certIssuanceTime](const VerifiedSCT& verifiedSct)->bool {
  //     return LogWasQualifiedForSct(verifiedSct, certIssuanceTime);
  // });
  compliant = independentOperatorsCount >= 2;
}

// Qualification Case #1 (non-embedded SCTs) - the following must hold:
// a. An SCT from a log qualified at the time of check is presented via the
// TLS extension OR is embedded within a stapled OCSP response;
// AND
// b. There are at least two SCTs from logs qualified at the time of check,
// presented via any method.
static void CheckNonEmbeddedCompliance(const VerifiedSCTList& verifiedScts,
                                       bool& compliant) {
  if (!HasValidNonEmbeddedSct(verifiedScts)) {
    compliant = false;
    return;
  }

  size_t validSctsCount;
  CountLogsForSelectedScts(
      verifiedScts, validSctsCount, [](const VerifiedSCT& verifiedSct) -> bool {
        return verifiedSct.status == VerifiedSCT::Status::Valid;
      });

  compliant = validSctsCount >= 2;
}

// Qualification Case #2 (embedded SCTs) - the following must hold:
// a. An Embedded SCT from a log qualified at the time of check is presented;
// AND
// b. There are Embedded SCTs from AT LEAST N + 1 once or currently qualified
// logs, where N is the lifetime of the certificate in years (normally
// rounding up, but rounding down when up to 3 months over), and must be
// at least 1.
static void CheckEmbeddedCompliance(const VerifiedSCTList& verifiedScts,
                                    size_t certLifetimeInCalendarMonths,
                                    uint64_t certIssuanceTime,
                                    bool& compliant) {
  if (!HasValidEmbeddedSct(verifiedScts)) {
    compliant = false;
    return;
  }

  // Count the compliant embedded SCTs. Only a single SCT from each log
  // is accepted. Note that a given log might return several different SCTs
  // for the same precertificate (it is permitted, but advised against).
  size_t embeddedSctsCount;
  CountLogsForSelectedScts(
      verifiedScts, embeddedSctsCount,
      [certIssuanceTime](const VerifiedSCT& verifiedSct) -> bool {
        return verifiedSct.origin == VerifiedSCT::Origin::Embedded &&
               LogWasQualifiedForSct(verifiedSct, certIssuanceTime);
      });

  size_t requiredSctsCount =
      GetRequiredEmbeddedSctsCount(certLifetimeInCalendarMonths);

  compliant = embeddedSctsCount >= requiredSctsCount;
}

void CTPolicyEnforcer::CheckCompliance(
    const VerifiedSCTList& verifiedScts, size_t certLifetimeInCalendarMonths,
    const CTLogOperatorList& dependentOperators,
    CTPolicyCompliance& compliance) {
  uint64_t certIssuanceTime = GetEffectiveCertIssuanceTime(verifiedScts);

  bool diversityOK;
  CheckOperatorDiversityCompliance(verifiedScts, certIssuanceTime,
                                   dependentOperators, diversityOK);

  bool nonEmbeddedCaseOK;
  CheckNonEmbeddedCompliance(verifiedScts, nonEmbeddedCaseOK);

  bool embeddedCaseOK;
  CheckEmbeddedCompliance(verifiedScts, certLifetimeInCalendarMonths,
                          certIssuanceTime, embeddedCaseOK);

  if (nonEmbeddedCaseOK || embeddedCaseOK) {
    compliance = diversityOK ? CTPolicyCompliance::Compliant
                             : CTPolicyCompliance::NotDiverseScts;
  } else {
    // Not enough SCTs are present to satisfy either case of the policy.
    compliance = CTPolicyCompliance::NotEnoughScts;
  }

  switch (compliance) {
    case CTPolicyCompliance::Compliant:
    case CTPolicyCompliance::NotEnoughScts:
    case CTPolicyCompliance::NotDiverseScts:
      break;
    case CTPolicyCompliance::Unknown:
    default:
      assert(false);
  }
}

}  // namespace ct
}  // namespace mozilla
