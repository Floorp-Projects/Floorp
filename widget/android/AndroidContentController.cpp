/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidContentController.h"
#include "mozilla/layers/APZCTreeManager.h"
#include "base/message_loop.h"
#include "nsWindow.h"
#include "AndroidBridge.h"

using mozilla::layers::APZCTreeManager;

namespace mozilla {
namespace widget {
namespace android {

NativePanZoomController::GlobalRef AndroidContentController::sNativePanZoomController = nullptr;

NativePanZoomController::LocalRef
AndroidContentController::SetNativePanZoomController(NativePanZoomController::Param obj)
{
    NativePanZoomController::LocalRef old = sNativePanZoomController;
    sNativePanZoomController = obj;
    return old;
}

void
AndroidContentController::NotifyDefaultPrevented(uint64_t aInputBlockId,
                                                 bool aDefaultPrevented)
{
    if (!AndroidBridge::IsJavaUiThread()) {
        // The notification must reach the APZ on the Java UI thread (aka the
        // APZ "controller" thread) but we get it from the Gecko thread, so we
        // have to throw it onto the other thread.
        AndroidBridge::Bridge()->PostTaskToUiThread(NewRunnableFunction(
            &AndroidContentController::NotifyDefaultPrevented,
            aInputBlockId, aDefaultPrevented), 0);
        return;
    }

    MOZ_ASSERT(AndroidBridge::IsJavaUiThread());
    APZCTreeManager* controller = nsWindow::GetAPZCTreeManager();
    if (controller) {
        controller->ContentReceivedInputBlock(aInputBlockId, aDefaultPrevented);
    }
}

void
AndroidContentController::HandleSingleTap(const CSSPoint& aPoint,
                                          Modifiers aModifiers,
                                          const ScrollableLayerGuid& aGuid)
{
    // This function will get invoked first on the Java UI thread, and then
    // again on the main thread (because of the code in ChromeProcessController::
    // HandleSingleTap). We want to post the SingleTap message once; it can be
    // done from either thread but for backwards compatibility with the JPZC
    // architecture it's better to do it as soon as possible.
    if (AndroidBridge::IsJavaUiThread()) {
        CSSIntPoint point = RoundedToInt(aPoint);
        nsCString data = nsPrintfCString("{ \"x\": %d, \"y\": %d }", point.x, point.y);
        nsAppShell::gAppShell->PostEvent(AndroidGeckoEvent::MakeBroadcastEvent(
                NS_LITERAL_CSTRING("Gesture:SingleTap"), data));
    }

    ChromeProcessController::HandleSingleTap(aPoint, aModifiers, aGuid);
}

void
AndroidContentController::PostDelayedTask(Task* aTask, int aDelayMs)
{
    AndroidBridge::Bridge()->PostTaskToUiThread(aTask, aDelayMs);
}

} // namespace android
} // namespace widget
} // namespace mozilla
