/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidContentController_h__
#define AndroidContentController_h__

#include "mozilla/layers/ChromeProcessController.h"
#include "mozilla/EventForwards.h"  // for Modifiers
#include "mozilla/StaticPtr.h"
#include "mozilla/TimeStamp.h"
#include "nsTArray.h"
#include "nsWindow.h"

namespace mozilla {
namespace layers {
class APZEventState;
class IAPZCTreeManager;
}  // namespace layers
namespace widget {

class AndroidContentController final
    : public mozilla::layers::ChromeProcessController {
 public:
  AndroidContentController(nsWindow* aWindow,
                           mozilla::layers::APZEventState* aAPZEventState,
                           mozilla::layers::IAPZCTreeManager* aAPZCTreeManager)
      : mozilla::layers::ChromeProcessController(aWindow, aAPZEventState,
                                                 aAPZCTreeManager),
        mAndroidWindow(aWindow) {}

  // ChromeProcessController methods
  virtual void Destroy() override;
  void UpdateOverscrollVelocity(const ScrollableLayerGuid& aGuid,
                                const float aX, const float aY,
                                const bool aIsRootContent) override;
  void UpdateOverscrollOffset(const ScrollableLayerGuid& aGuid, const float aX,
                              const float aY,
                              const bool aIsRootContent) override;
  void NotifyAPZStateChange(const ScrollableLayerGuid& aGuid,
                            APZStateChange aChange, int aArg) override;

 private:
  nsWindow* mAndroidWindow;
};

}  // namespace widget
}  // namespace mozilla

#endif
