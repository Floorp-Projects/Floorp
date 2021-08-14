/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CTDiversityPolicy_h
#define CTDiversityPolicy_h

#include "CTLog.h"
#include "CTVerifyResult.h"
#include "certt.h"
#include "nsTArray.h"
#include "mozpkix/Result.h"

namespace mozilla {
namespace ct {

// Retuns the list of unique CT log operator IDs appearing in the provided
// list of verified SCTs.
void GetCTLogOperatorsFromVerifiedSCTList(const VerifiedSCTList& list,
                                          CTLogOperatorList& operators);

// Helper class used by CTPolicyEnforcer to check the CT log operators
// diversity requirements of the CT Policy.
// See CTPolicyEnforcer.h for more details.
class CTDiversityPolicy {
 public:
  // Given a certificate chain and a set of CT log operators,
  // returns the subset of log operators that are dependent on the CA
  // issuing the certificate (as defined by the CT Policy).
  //
  // NOTE: TBD, PENDING FINALIZATION OF MOZILLA CT POLICY.
  pkix::Result GetDependentOperators(
      const nsTArray<nsTArray<uint8_t>>& builtChain,
      const CTLogOperatorList& operators,
      CTLogOperatorList& dependentOperators);
};

}  // namespace ct
}  // namespace mozilla

#endif  // CTDiversityPolicy_h
