/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SharedCertVerifier_h
#define SharedCertVerifier_h

#include "CertVerifier.h"
#include "mozilla/RefPtr.h"

namespace mozilla { namespace psm {

class SharedCertVerifier : public mozilla::psm::CertVerifier
{
protected:
  ~SharedCertVerifier();

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SharedCertVerifier)

  SharedCertVerifier(OcspDownloadConfig odc, OcspStrictConfig osc,
                     OcspGetConfig ogc, uint32_t certShortLifetimeInDays,
                     PinningMode pinningMode, SHA1Mode sha1Mode,
                     BRNameMatchingPolicy::Mode nameMatchingMode,
                     NetscapeStepUpPolicy netscapeStepUpPolicy)
    : mozilla::psm::CertVerifier(odc, osc, ogc, certShortLifetimeInDays,
                                 pinningMode, sha1Mode, nameMatchingMode,
                                 netscapeStepUpPolicy)
  {
  }
};

} } // namespace mozilla::psm

#endif // SharedCertVerifier_h
