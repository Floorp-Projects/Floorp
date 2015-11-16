/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidContentController.h"

#include "AndroidBridge.h"
#include "base/message_loop.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "mozilla/layers/APZCTreeManager.h"
#include "nsLayoutUtils.h"
#include "nsWindow.h"

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
    // done from either thread but we need access to the callback transform
    // so we do it from the main thread.
    if (NS_IsMainThread()) {
        CSSPoint point = mozilla::layers::APZCCallbackHelper::ApplyCallbackTransform(aPoint, aGuid);

        nsIContent* content = nsLayoutUtils::FindContentFor(aGuid.mScrollId);
        nsIPresShell* shell = content
            ? mozilla::layers::APZCCallbackHelper::GetRootContentDocumentPresShellForContent(content)
            : nullptr;

        if (shell && shell->ScaleToResolution()) {
            // We need to convert from the root document to the root content document,
            // by unapplying the resolution that's on the content document.
            const float resolution = shell->GetResolution();
            point.x /= resolution;
            point.y /= resolution;
        }

        CSSIntPoint rounded = RoundedToInt(point);
        nsCString data = nsPrintfCString("{ \"x\": %d, \"y\": %d }", rounded.x, rounded.y);
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
