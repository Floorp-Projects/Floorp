/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CTDiversityPolicy.h"

namespace mozilla {
namespace ct {

typedef mozilla::pkix::Result Result;

void GetCTLogOperatorsFromVerifiedSCTList(const VerifiedSCTList& list,
                                          CTLogOperatorList& operators) {
  operators.clear();
  for (const VerifiedSCT& verifiedSct : list) {
    CTLogOperatorId sctLogOperatorId = verifiedSct.logOperatorId;
    bool alreadyAdded = false;
    for (CTLogOperatorId id : operators) {
      if (id == sctLogOperatorId) {
        alreadyAdded = true;
        break;
      }
    }
    if (!alreadyAdded) {
      operators.push_back(sctLogOperatorId);
    }
  }
}

Result CTDiversityPolicy::GetDependentOperators(
    const nsTArray<nsTArray<uint8_t>>& builtChain,
    const CTLogOperatorList& operators, CTLogOperatorList& dependentOperators) {
  return Success;
}

}  // namespace ct
}  // namespace mozilla
