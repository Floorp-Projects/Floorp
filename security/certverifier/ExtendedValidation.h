/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ExtendedValidation_h
#define ExtendedValidation_h

#include "ScopedNSSTypes.h"
#include "certt.h"
#include "prtypes.h"

namespace mozilla { namespace pkix { struct CertPolicyId; } }

namespace mozilla { namespace psm {

#ifndef MOZ_NO_EV_CERTS
void EnsureIdentityInfoLoaded();
void CleanupIdentityInfo();
SECStatus GetFirstEVPolicy(CERTCertificate* cert,
                           /*out*/ mozilla::pkix::CertPolicyId& policy,
                           /*out*/ SECOidTag& policyOidTag);

// CertIsAuthoritativeForEVPolicy does NOT evaluate whether the cert is trusted
// or distrusted.
bool CertIsAuthoritativeForEVPolicy(const UniqueCERTCertificate& cert,
                                    const mozilla::pkix::CertPolicyId& policy);
#endif

} } // namespace mozilla::psm

#endif // ExtendedValidation_h
