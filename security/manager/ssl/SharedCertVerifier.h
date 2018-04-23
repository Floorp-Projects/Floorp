/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SharedCertVerifier_h
#define SharedCertVerifier_h

#include "CertVerifier.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"

namespace mozilla { namespace psm {

class SharedCertVerifier : public mozilla::psm::CertVerifier
{
protected:
  ~SharedCertVerifier();

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SharedCertVerifier)

  SharedCertVerifier(OcspDownloadConfig odc, OcspStrictConfig osc,
                     mozilla::TimeDuration ocspSoftTimeout,
                     mozilla::TimeDuration ocspHardTimeout,
                     uint32_t certShortLifetimeInDays,
                     PinningMode pinningMode, SHA1Mode sha1Mode,
                     BRNameMatchingPolicy::Mode nameMatchingMode,
                     NetscapeStepUpPolicy netscapeStepUpPolicy,
                     CertificateTransparencyMode ctMode,
                     DistrustedCAPolicy distrustedCAPolicy)
    : mozilla::psm::CertVerifier(odc, osc, ocspSoftTimeout,
                                 ocspHardTimeout, certShortLifetimeInDays,
                                 pinningMode, sha1Mode, nameMatchingMode,
                                 netscapeStepUpPolicy, ctMode,
                                 distrustedCAPolicy)
  {
  }
};

} } // namespace mozilla::psm

#endif // SharedCertVerifier_h
