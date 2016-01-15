/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidContentController_h__
#define AndroidContentController_h__

#include "mozilla/layers/ChromeProcessController.h"
#include "mozilla/EventForwards.h"  // for Modifiers
#include "mozilla/StaticPtr.h"
#include "mozilla/TimeStamp.h"
#include "nsIDOMWindowUtils.h"
#include "nsTArray.h"

namespace mozilla {
namespace layers {
class APZEventState;
class APZCTreeManager;
}
namespace widget {

class AndroidContentController final
    : public mozilla::layers::ChromeProcessController
{
public:
    AndroidContentController(nsIWidget* aWidget,
                             mozilla::layers::APZEventState* aAPZEventState,
                             mozilla::layers::APZCTreeManager* aAPZCTreeManager)
      : mozilla::layers::ChromeProcessController(aWidget, aAPZEventState, aAPZCTreeManager)
    {}

    // ChromeProcessController methods
    void HandleSingleTap(const CSSPoint& aPoint,
                         Modifiers aModifiers,
                         const ScrollableLayerGuid& aGuid) override;
    void PostDelayedTask(Task* aTask, int aDelayMs) override;

    static void NotifyDefaultPrevented(mozilla::layers::APZCTreeManager* aManager,
                                       uint64_t aInputBlockId, bool aDefaultPrevented);
};

} // namespace widget
} // namespace mozilla

#endif
