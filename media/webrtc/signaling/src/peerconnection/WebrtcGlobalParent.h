/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _WEBRTC_GLOBAL_PARENT_H_
#define _WEBRTC_GLOBAL_PARENT_H_

#include "mozilla/dom/PWebrtcGlobalParent.h"
#include "mozilla/dom/RTCStatsReportBinding.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace dom {

class WebrtcParents;

class WebrtcGlobalParent
  : public PWebrtcGlobalParent
  , public RefCounted<WebrtcGlobalParent>
{
  friend class ContentParent;
  friend class WebrtcGlobalInformation;
  friend class WebrtcContentParents;

  bool mShutdown;

  MOZ_IMPLICIT WebrtcGlobalParent();

  static WebrtcGlobalParent* Alloc();
  static bool Dealloc(WebrtcGlobalParent* aActor);

  virtual bool RecvGetStatsResult(const int& aRequestId,
                                  nsTArray<RTCStatsReportInternal>&& aStats) override;
  virtual bool RecvGetLogResult(const int& aRequestId,
                                const WebrtcGlobalLog& aLog) override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  virtual bool Recv__delete__() override;
public:
  virtual ~WebrtcGlobalParent();
  MOZ_DECLARE_REFCOUNTED_TYPENAME(WebrtcGlobalParent)
  bool IsActive()
  {
    return !mShutdown;
  }
};

} // namespace dom
} // namespace mozilla

#endif  // _WEBRTC_GLOBAL_PARENT_H_
