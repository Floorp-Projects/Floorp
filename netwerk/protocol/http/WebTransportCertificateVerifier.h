/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebTransportCertificateVerifier_h
#define mozilla_net_WebTransportCertificateVerifier_h

#include "nsTArray.h"
#include "nsIWebTransport.h"
#include "nss/mozpkix/pkixtypes.h"

namespace mozilla::net {
// This is a special version for serverCertificateHashes introduced with
// WebTransport
mozilla::pkix::Result AuthCertificateWithServerCertificateHashes(
    nsTArray<uint8_t>& peerCert,
    const nsTArray<RefPtr<nsIWebTransportHash>>& aServerCertHashes);

}  // namespace mozilla::net

#endif  // mozilla_net_WebTransportCertificateVerifier_h
