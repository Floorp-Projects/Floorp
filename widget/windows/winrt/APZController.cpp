/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZController.h"
#include "base/message_loop.h"
#include "mozilla/layers/GeckoContentController.h"
#include "nsThreadUtils.h"
#include "MetroUtils.h"
#include "nsPrintfCString.h"

namespace mozilla {
namespace widget {
namespace winrt {

class RequestContentRepaintEvent : public nsRunnable
{
    typedef mozilla::layers::FrameMetrics FrameMetrics;

public:
    RequestContentRepaintEvent(const FrameMetrics& aFrameMetrics) : mFrameMetrics(aFrameMetrics)
    {
    }

    NS_IMETHOD Run() {
        // This event shuts down the worker thread and so must be main thread.
        MOZ_ASSERT(NS_IsMainThread());

        CSSToScreenScale resolution = mFrameMetrics.mZoom;
        CSSRect compositedRect = mFrameMetrics.CalculateCompositedRectInCssPixels();

        NS_ConvertASCIItoUTF16 data(nsPrintfCString("{ " \
                                                    "  \"resolution\": %.2f, " \
                                                    "  \"scrollId\": %d, " \
                                                    "  \"compositedRect\": { \"width\": %d, \"height\": %d }, " \
                                                    "  \"displayPort\":    { \"x\": %d, \"y\": %d, \"width\": %d, \"height\": %d }, " \
                                                    "  \"scrollTo\":       { \"x\": %d, \"y\": %d }" \
                                                    "}",
                                                    (float)(resolution.scale / mFrameMetrics.mDevPixelsPerCSSPixel.scale),
                                                    (int)mFrameMetrics.mScrollId,
                                                    (int)compositedRect.width,
                                                    (int)compositedRect.height,
                                                    (int)mFrameMetrics.mDisplayPort.x,
                                                    (int)mFrameMetrics.mDisplayPort.y,
                                                    (int)mFrameMetrics.mDisplayPort.width,
                                                    (int)mFrameMetrics.mDisplayPort.height,
                                                    (int)mFrameMetrics.mScrollOffset.x,
                                                    (int)mFrameMetrics.mScrollOffset.y));

        MetroUtils::FireObserver("apzc-request-content-repaint", data.get());
        return NS_OK;
    }
protected:
    const FrameMetrics mFrameMetrics;
};

void
APZController::RequestContentRepaint(const FrameMetrics& aFrameMetrics)
{
  // Send the result back to the main thread so that it can shutdown
  nsCOMPtr<nsIRunnable> r1 = new RequestContentRepaintEvent(aFrameMetrics);
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(r1);
  } else {
    r1->Run();
  }
}

void
APZController::HandleDoubleTap(const CSSIntPoint& aPoint)
{
}

void
APZController::HandleSingleTap(const CSSIntPoint& aPoint)
{
}

void
APZController::HandleLongTap(const CSSIntPoint& aPoint)
{
}

void
APZController::SendAsyncScrollDOMEvent(FrameMetrics::ViewID aScrollId, const CSSRect &aContentRect, const CSSSize &aScrollableSize)
{
}

void
APZController::PostDelayedTask(Task* aTask, int aDelayMs)
{
  MessageLoop::current()->PostDelayedTask(FROM_HERE, aTask, aDelayMs);
}

void
APZController::HandlePanBegin()
{
  MetroUtils::FireObserver("apzc-handle-pan-begin", L"");
}

void
APZController::HandlePanEnd()
{
  MetroUtils::FireObserver("apzc-handle-pan-end", L"");
}

} } }