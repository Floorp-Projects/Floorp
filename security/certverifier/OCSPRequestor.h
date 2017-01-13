/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef OCSPRequestor_h
#define OCSPRequestor_h

#include "CertVerifier.h"
#include "secmodt.h"

namespace mozilla {
class OriginAttributes;
}

namespace mozilla { namespace psm {

// The memory returned via |encodedResponse| is owned by the given arena.
Result DoOCSPRequest(const UniquePLArenaPool& arena, const char* url,
                     const OriginAttributes& originAttributes,
                     const SECItem* encodedRequest, PRIntervalTime timeout,
                     bool useGET,
             /*out*/ SECItem*& encodedResponse);

} } // namespace mozilla::psm

#endif // OCSPRequestor_h
