/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _REMOTE_TRACK_SOURCE_H_
#define _REMOTE_TRACK_SOURCE_H_

#include "MediaStreamTrack.h"
#include "MediaStreamError.h"

namespace mozilla {

class RemoteTrackSource : public dom::MediaStreamTrackSource {
 public:
  explicit RemoteTrackSource(SourceMediaStream* aStream,
                             nsIPrincipal* aPrincipal, const nsString& aLabel)
      : dom::MediaStreamTrackSource(aPrincipal, aLabel), mStream(aStream) {}

  dom::MediaSourceEnum GetMediaSource() const override {
    return dom::MediaSourceEnum::Other;
  }

  RefPtr<ApplyConstraintsPromise> ApplyConstraints(
      const dom::MediaTrackConstraints& aConstraints,
      dom::CallerType aCallerType) override {
    return ApplyConstraintsPromise::CreateAndReject(
        MakeRefPtr<MediaMgrError>(
            dom::MediaStreamError::Name::OverconstrainedError,
            NS_LITERAL_STRING("")),
        __func__);
  }

  void Stop() override {
    // XXX (Bug 1314270): Implement rejection logic if necessary when we have
    //                    clarity in the spec.
  }

  void Disable() override {}

  void Enable() override {}

  void SetPrincipal(nsIPrincipal* aPrincipal) {
    mPrincipal = aPrincipal;
    PrincipalChanged();
  }
  void SetMuted(bool aMuted) { MutedChanged(aMuted); }
  void ForceEnded() { OverrideEnded(); }

  const RefPtr<SourceMediaStream> mStream;

 protected:
  virtual ~RemoteTrackSource() {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mStream->IsDestroyed());
  }
};

}  // namespace mozilla

#endif  // _REMOTE_TRACK_SOURCE_H_
