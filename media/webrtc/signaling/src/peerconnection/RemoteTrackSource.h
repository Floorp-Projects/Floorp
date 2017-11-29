/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _REMOTE_TRACK_SOURCE_H_
#define _REMOTE_TRACK_SOURCE_H_

#include "MediaStreamTrack.h"
#include "MediaStreamError.h"

namespace mozilla {

class RemoteTrackSource : public dom::MediaStreamTrackSource
{
public:
  explicit RemoteTrackSource(nsIPrincipal* aPrincipal, const nsString& aLabel)
    : dom::MediaStreamTrackSource(aPrincipal, aLabel) {}

  dom::MediaSourceEnum GetMediaSource() const override
  {
    return dom::MediaSourceEnum::Other;
  }

  already_AddRefed<PledgeVoid>
  ApplyConstraints(nsPIDOMWindowInner* aWindow,
                   const dom::MediaTrackConstraints& aConstraints,
                   dom::CallerType aCallerType) override
  {
    RefPtr<PledgeVoid> p = new PledgeVoid();
    p->Reject(
        new dom::MediaStreamError(aWindow,
                                  NS_LITERAL_STRING("OverconstrainedError"),
                                  NS_LITERAL_STRING("")));
    return p.forget();
  }

  void Stop() override
  {
    // XXX (Bug 1314270): Implement rejection logic if necessary when we have
    //                    clarity in the spec.
  }

  void SetPrincipal(nsIPrincipal* aPrincipal)
  {
    mPrincipal = aPrincipal;
    PrincipalChanged();
  }

protected:
  virtual ~RemoteTrackSource() {}
};

} // namespace mozilla

#endif // _REMOTE_TRACK_SOURCE_H_

