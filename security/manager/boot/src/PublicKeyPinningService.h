/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PublicKeyPinningService_h
#define PublicKeyPinningService_h

#include "cert.h"
#include "pkix/Time.h"

namespace mozilla {
namespace psm {

class PublicKeyPinningService
{
public:
  /**
   * Returns true if the given (host, certList) passes pinning checks,
   * false otherwise. If the host is pinned, return true if one of the keys in
   * the given certificate chain matches the pin set specified by the
   * hostname. If the hostname is null or empty evaluate against all the
   * possible names for the EE cert (Common Name (CN) plus all DNS Name:
   * subject Alt Name entries). The certList's head is the EE cert and the
   * tail is the trust anchor.
   * Note: if an alt name is a wildcard, it won't necessarily find a pinset
   * that would otherwise be valid for it
   */
  static bool ChainHasValidPins(const CERTCertList* certList,
                                const char* hostname,
                                mozilla::pkix::Time time,
                                bool enforceTestMode);
};

}} // namespace mozilla::psm

#endif // PublicKeyPinningServiceService_h
