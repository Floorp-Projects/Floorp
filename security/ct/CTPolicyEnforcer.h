/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CTPolicyEnforcer_h
#define CTPolicyEnforcer_h

#include "CTLog.h"
#include "CTVerifyResult.h"
#include "mozpkix/Result.h"

namespace mozilla {
namespace ct {

// Information about the compliance of the TLS connection with the
// Certificate Transparency policy.
enum class CTPolicyCompliance {
  // Compliance not checked or not applicable.
  Unknown,
  // The connection complied with the certificate policy
  // by including SCTs that satisfy the policy.
  Compliant,
  // The connection did not have enough valid SCTs to comply.
  NotEnoughScts,
  // The connection had enough valid SCTs, but the diversity requirement
  // was not met (the number of CT log operators independent of the CA
  // and of each other is too low).
  NotDiverseScts,
};

// Checks whether a TLS connection complies with the current CT policy.
// The implemented policy is based on the Mozilla CT Policy draft 0.1.0
// (see https://docs.google.com/document/d/
// 1rnqYYwscAx8WhS-MCdTiNzYQus9e37HuVyafQvEeNro/edit).
//
// NOTE: CT DIVERSITY REQUIREMENT IS TBD, PENDING FINALIZATION
// OF MOZILLA CT POLICY. Specifically:
// 1. CT log operators being CA-dependent is not currently taken into account
// (see CTDiversityPolicy.h).
// 2. The preexisting certificate exception provision of the operator diversity
// requirement is not implemented (see "CT Qualified" section of the policy and
// CheckOperatorDiversityCompliance in CTPolicyEnforcer.cpp).
class CTPolicyEnforcer {
 public:
  // |verifiedSct| - SCTs present on the connection along with their
  // verification status.
  // |certLifetimeInCalendarMonths| - certificate lifetime in full calendar
  // months (i.e. rounded down), based on the notBefore/notAfter fields.
  // |dependentOperators| - which CT log operators are dependent on the CA
  // that issued the certificate. SCTs issued by logs associated with such
  // operators are treated differenly when evaluating the policy.
  // See CTDiversityPolicy class.
  // |compliance| - the result of the compliance check.
  void CheckCompliance(const VerifiedSCTList& verifiedScts,
                       size_t certLifetimeInCalendarMonths,
                       const CTLogOperatorList& dependentOperators,
                       CTPolicyCompliance& compliance);
};

}  // namespace ct
}  // namespace mozilla

#endif  // CTPolicyEnforcer_h
