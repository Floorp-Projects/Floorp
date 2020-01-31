/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _SSLSERVERCERTVERIFICATION_H
#define _SSLSERVERCERTVERIFICATION_H

#include "mozilla/Maybe.h"
#include "nsTArray.h"
#include "seccomon.h"
#include "prio.h"

namespace mozilla {
namespace psm {

class TransportSecurityInfo;

SECStatus AuthCertificateHook(void* arg, PRFileDesc* fd, PRBool checkSig,
                              PRBool isServer);

// This function triggers the certificate verification. The verification is
// asynchronous and the info object will be notified when the verification has
// completed via SetCertVerificationResult.
SECStatus AuthCertificateHookWithInfo(
    TransportSecurityInfo* infoObject, const void* aPtrForLogging,
    nsTArray<nsTArray<uint8_t>>&& peerCertChain,
    Maybe<nsTArray<nsTArray<uint8_t>>>& stapledOCSPResponses,
    Maybe<nsTArray<uint8_t>>& sctsFromTLSExtension, uint32_t providerFlags);
}  // namespace psm
}  // namespace mozilla

#endif
