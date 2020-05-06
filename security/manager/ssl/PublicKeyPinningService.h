/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PublicKeyPinningService_h
#define PublicKeyPinningService_h

#include "CertVerifier.h"
#include "ScopedNSSTypes.h"
#include "cert.h"
#include "nsNSSCertificate.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mozpkix/Time.h"

namespace mozilla {
class OriginAttributes;
}

using mozilla::OriginAttributes;

namespace mozilla {
namespace psm {

class PublicKeyPinningService {
 public:
  /**
   * Sets chainHasValidPins to true if the given (host, certList) passes pinning
   * checks, or to false otherwise. If the host is pinned, returns true via
   * chainHasValidPins if one of the keys in the given certificate chain matches
   * the pin set specified by the hostname. The certList's head is the EE cert
   * and the tail is the trust anchor.
   * Note: if an alt name is a wildcard, it won't necessarily find a pinset
   * that would otherwise be valid for it
   */
  static nsresult ChainHasValidPins(
      const nsTArray<RefPtr<nsIX509Cert>>& certList, const char* hostname,
      mozilla::pkix::Time time, bool enforceTestMode,
      const OriginAttributes& originAttributes,
      /*out*/ bool& chainHasValidPins,
      /*optional out*/ PinningTelemetryInfo* pinningTelemetryInfo);

  /**
   * Returns true via the output parameter hostHasPins if there is pinning
   * information for the given host that is valid at the given time, and false
   * otherwise.
   */
  static nsresult HostHasPins(const char* hostname, mozilla::pkix::Time time,
                              bool enforceTestMode,
                              const OriginAttributes& originAttributes,
                              /*out*/ bool& hostHasPins);

  /**
   * Given a hostname of potentially mixed case with potentially multiple
   * trailing '.' (see bug 1118522), canonicalizes it to lowercase with no
   * trailing '.'.
   */
  static nsAutoCString CanonicalizeHostname(const char* hostname);
};

}  // namespace psm
}  // namespace mozilla

#endif  // PublicKeyPinningService_h
