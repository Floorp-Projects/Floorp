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
  SharedCertVerifier(implementation_config ic, missing_cert_download_config ac,
                     crl_download_config cdc, ocsp_download_config odc,
                     ocsp_strict_config osc, ocsp_get_config ogc)
    : mozilla::psm::CertVerifier(ic, ac, cdc, odc, osc, ogc)
  {
  }
  ~SharedCertVerifier();
};

} } // namespace mozilla::psm

#endif // mozilla_psm__SharedCertVerifier_h
