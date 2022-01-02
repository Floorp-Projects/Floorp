/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNSSCallbacks_h
#define nsNSSCallbacks_h

#include "mozilla/Attributes.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Vector.h"
#include "nspr.h"
#include "nsString.h"
#include "pk11func.h"
#include "mozpkix/pkix.h"
#include "mozpkix/pkixtypes.h"
#include "nsIX509Cert.h"

using mozilla::OriginAttributes;
using mozilla::TimeDuration;
using mozilla::Vector;

class nsILoadGroup;

char* PK11PasswordPrompt(PK11SlotInfo* slot, PRBool retry, void* arg);

void HandshakeCallback(PRFileDesc* fd, void* client_data);
SECStatus CanFalseStartCallback(PRFileDesc* fd, void* client_data,
                                PRBool* canFalseStart);

mozilla::pkix::Result DoOCSPRequest(
    const nsCString& aiaLocation, const OriginAttributes& originAttributes,
    uint8_t (&ocspRequest)[mozilla::pkix::OCSP_REQUEST_MAX_LENGTH],
    size_t ocspRequestLength, TimeDuration timeout,
    /*out*/ Vector<uint8_t>& result);

nsCString getKeaGroupName(uint32_t aKeaGroup);
nsCString getSignatureName(uint32_t aSignatureScheme);

#endif  // nsNSSCallbacks_h
