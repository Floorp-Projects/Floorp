/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_psm_OCSPRequestor_h
#define mozilla_psm_OCSPRequestor_h

#include "CertVerifier.h"
#include "secmodt.h"

namespace mozilla { namespace psm {

// The memory returned is owned by the given arena.
SECItem* DoOCSPRequest(PLArenaPool* arena, const char* url,
                       const SECItem* encodedRequest, PRIntervalTime timeout,
                       bool useGET);

} } // namespace mozilla::psm

#endif // mozilla_psm_OCSPRequestor_h
