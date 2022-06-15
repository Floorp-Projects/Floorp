/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ExtendedValidation_h
#define ExtendedValidation_h

#include "ScopedNSSTypes.h"
#include "certt.h"

namespace mozilla {
namespace pkix {
struct CertPolicyId;
}  // namespace pkix
}  // namespace mozilla

namespace mozilla {
namespace psm {

nsresult LoadExtendedValidationInfo();

/**
 * Finds all policy OIDs in the given cert that are known to be EV policy OIDs.
 *
 * @param cert
 *        The bytes of the cert to find the EV policies of.
 * @param policies
 *        The found policies.
 */
void GetKnownEVPolicies(
    const nsTArray<uint8_t>& cert,
    /*out*/ nsTArray<mozilla::pkix::CertPolicyId>& policies);

// CertIsAuthoritativeForEVPolicy does NOT evaluate whether the cert is trusted
// or distrusted.
bool CertIsAuthoritativeForEVPolicy(const nsTArray<uint8_t>& cert,
                                    const mozilla::pkix::CertPolicyId& policy);

}  // namespace psm
}  // namespace mozilla

#endif  // ExtendedValidation_h
