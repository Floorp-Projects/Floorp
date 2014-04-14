/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_psm__SharedCertVerifier_h
#define mozilla_psm__SharedCertVerifier_h

#include "certt.h"
#include "CertVerifier.h"
#include "mozilla/RefPtr.h"

namespace mozilla { namespace psm {

class SharedCertVerifier : public mozilla::psm::CertVerifier,
                           public mozilla::AtomicRefCounted<SharedCertVerifier>
{
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(SharedCertVerifier)
  SharedCertVerifier(implementation_config ic,
#ifndef NSS_NO_LIBPKIX
                     missing_cert_download_config ac, crl_download_config cdc,
#endif
                     ocsp_download_config odc, ocsp_strict_config osc,
                     ocsp_get_config ogc)
    : mozilla::psm::CertVerifier(ic,
#ifndef NSS_NO_LIBPKIX
                                 ac, cdc,
#endif
                                 odc, osc, ogc)
  {
  }
  ~SharedCertVerifier();
};

} } // namespace mozilla::psm

#endif // mozilla_psm__SharedCertVerifier_h
