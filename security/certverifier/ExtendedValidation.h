/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_psm_ExtendedValidation_h
#define mozilla_psm_ExtendedValidation_h

#include "certt.h"
#include "prtypes.h"

namespace mozilla { namespace psm {

#ifndef NSS_NO_LIBPKIX
void EnsureIdentityInfoLoaded();
SECStatus GetFirstEVPolicy(CERTCertificate *cert, SECOidTag &outOidTag);
CERTCertList* GetRootsForOid(SECOidTag oid_tag);
void CleanupIdentityInfo();
#endif

} } // namespace mozilla::psm

#endif // mozilla_psm_ExtendedValidation_h
