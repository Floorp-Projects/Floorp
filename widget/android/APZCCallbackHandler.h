/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef APZCCallbackHandler_h__
#define APZCCallbackHandler_h__

#include "mozilla/layers/ChromeProcessController.h"
#include "mozilla/layers/APZEventState.h"
#include "mozilla/EventForwards.h"  // for Modifiers
#include "mozilla/StaticPtr.h"
#include "mozilla/TimeStamp.h"
#include "GeneratedJNIWrappers.h"
#include "nsIDOMWindowUtils.h"
#include "nsTArray.h"

namespace mozilla {
namespace widget {
namespace android {

class APZCCallbackHandler final : public mozilla::layers::ChromeProcessController
{
private:
    static StaticRefPtr<APZCCallbackHandler> sInstance;

    static NativePanZoomController::GlobalRef sNativePanZoomController;

private:
    APZCCallbackHandler(nsIWidget* aWidget, mozilla::layers::APZEventState* aAPZEventState)
      : mozilla::layers::ChromeProcessController(aWidget, aAPZEventState)
    {}

public:
    static void Initialize(nsIWidget* aWidget, mozilla::layers::APZEventState* aAPZEventState) {

        MOZ_ASSERT(!sInstance.get(), "APZCCallbackHandler.Initialize() got called twice");
        sInstance = new APZCCallbackHandler(aWidget, aAPZEventState);
    }

    static APZCCallbackHandler* GetInstance() {
        MOZ_ASSERT(sInstance.get(), "Calling APZCCallbackHandler.GetInstance() before it's initialization");
        return sInstance.get();
    }

    static NativePanZoomController::LocalRef SetNativePanZoomController(NativePanZoomController::Param obj);
    void NotifyDefaultPrevented(uint64_t aInputBlockId, bool aDefaultPrevented);

public: // ChromeProcessController methods

    void PostDelayedTask(Task* aTask, int aDelayMs) override;
};

} // namespace android
} // namespace widget
} // namespace mozilla

#endif
