/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZController.h"
#include "base/message_loop.h"
#include "mozilla/layers/GeckoContentController.h"
#include "nsThreadUtils.h"
#include "MetroUtils.h"
#include "nsPrintfCString.h"
#include "nsIWidgetListener.h"
#include "APZCCallbackHelper.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsIDOMElement.h"
#include "mozilla/dom/Element.h"
#include "nsIDOMWindowUtils.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsLayoutUtils.h"
#include "mozilla/TouchEvents.h"

//#define DEBUG_CONTROLLER 1

#ifdef DEBUG_CONTROLLER
#include "WinUtils.h"
using namespace mozilla::widget;
#endif

namespace mozilla {
namespace widget {
namespace winrt {

nsRefPtr<mozilla::layers::APZCTreeManager> APZController::sAPZC;

/*
 * Metro layout specific - test to see if a sub document is a
 * tab.
 */
static bool
IsTab(nsCOMPtr<nsIDocument>& aSubDocument)
{
  nsRefPtr<nsIDocument> parent = aSubDocument->GetParentDocument();
  if (!parent) {
    NS_WARNING("huh? IsTab should always get a sub document for a parameter");
    return false;
  }
  return parent->IsRootDisplayDocument();
}

/*
 * Returns the sub document associated with the scroll id.
 */
static bool
GetDOMTargets(uint64_t aScrollId,
              nsCOMPtr<nsIDocument>& aSubDocument,
              nsCOMPtr<nsIContent>& aTargetContent)
{
  // For tabs and subframes this will return the HTML sub document
  aTargetContent = nsLayoutUtils::FindContentFor(aScrollId);
  if (!aTargetContent) {
    return false;
  }
  nsCOMPtr<mozilla::dom::Element> domElement = do_QueryInterface(aTargetContent);
  if (!domElement) {
    return false;
  }

  aSubDocument = domElement->OwnerDoc();

  if (!aSubDocument) {
    return false;
  }

  // If the root element equals domElement, FindElementWithViewId found
  // a document, vs. an element within a document.
  if (aSubDocument->GetRootElement() == domElement && IsTab(aSubDocument)) {
    aTargetContent = nullptr;
  }

  return true;
}

class RequestContentRepaintEvent : public nsRunnable
{
  typedef mozilla::layers::FrameMetrics FrameMetrics;
  typedef mozilla::layers::ScrollableLayerGuid ScrollableLayerGuid;

public:
  RequestContentRepaintEvent(const FrameMetrics& aFrameMetrics,
                             nsIWidgetListener* aListener,
                             CSSIntPoint* aLastOffsetOut,
                             ScrollableLayerGuid* aLastScrollId) :
    mFrameMetrics(aFrameMetrics),
    mWidgetListener(aListener),
    mLastOffsetOut(aLastOffsetOut),
    mLastScrollIdOut(aLastScrollId)
  {
  }

  NS_IMETHOD Run() {
    // This must be on the gecko thread since we access the dom
    MOZ_ASSERT(NS_IsMainThread());

#ifdef DEBUG_CONTROLLER
    WinUtils::Log("APZController: mScrollOffset: %f %f", mFrameMetrics.mScrollOffset.x,
      mFrameMetrics.mScrollOffset.y);
#endif

    nsCOMPtr<nsIDocument> subDocument;
    nsCOMPtr<nsIContent> targetContent;
    if (!GetDOMTargets(mFrameMetrics.mScrollId,
                       subDocument, targetContent)) {
      return NS_OK;
    }

    // If we're dealing with a sub frame or content editable element,
    // call UpdateSubFrame.
    if (targetContent) {
#ifdef DEBUG_CONTROLLER
      WinUtils::Log("APZController: detected subframe or content editable");
#endif
      APZCCallbackHelper::UpdateSubFrame(targetContent, mFrameMetrics);
      return NS_OK;
    }

#ifdef DEBUG_CONTROLLER
    WinUtils::Log("APZController: detected tab");
#endif

    // We're dealing with a tab, call UpdateRootFrame.
    nsCOMPtr<nsIDOMWindowUtils> utils;
    nsCOMPtr<nsIDOMWindow> window = subDocument->GetDefaultView();
    if (window) {
      utils = do_GetInterface(window);
      if (utils) {
        APZCCallbackHelper::UpdateRootFrame(utils, mFrameMetrics);

        // Return the actual scroll value so we can use it to filter
        // out scroll messages triggered by setting the display port.
        CSSIntPoint actualScrollOffset;
        utils->GetScrollXY(false, &actualScrollOffset.x, &actualScrollOffset.y);
        if (mLastOffsetOut) {
          *mLastOffsetOut = actualScrollOffset;
        }
        if (mLastScrollIdOut) {
          mLastScrollIdOut->mScrollId = mFrameMetrics.mScrollId;
          mLastScrollIdOut->mPresShellId = mFrameMetrics.mPresShellId;
        }

#ifdef DEBUG_CONTROLLER
        WinUtils::Log("APZController: %I64d mDisplayPort: %0.2f %0.2f %0.2f %0.2f",
          mFrameMetrics.mScrollId,
          mFrameMetrics.mDisplayPort.x,
          mFrameMetrics.mDisplayPort.y,
          mFrameMetrics.mDisplayPort.width,
          mFrameMetrics.mDisplayPort.height);
#endif
      }
    }
    return NS_OK;
  }
protected:
  FrameMetrics mFrameMetrics;
  nsIWidgetListener* mWidgetListener;
  CSSIntPoint* mLastOffsetOut;
  ScrollableLayerGuid* mLastScrollIdOut;
};

void
APZController::SetWidgetListener(nsIWidgetListener* aWidgetListener)
{
  mWidgetListener = aWidgetListener;
}

void
APZController::ContentReceivedTouch(const ScrollableLayerGuid& aGuid, bool aPreventDefault)
{
  if (!sAPZC) {
    return;
  }
  sAPZC->ContentReceivedTouch(aGuid, aPreventDefault);
}

bool
APZController::HitTestAPZC(ScreenIntPoint& aPoint)
{
  if (!sAPZC) {
    return false;
  }
  return sAPZC->HitTestAPZC(aPoint);
}

void
APZController::TransformCoordinateToGecko(const ScreenIntPoint& aPoint,
                                          LayoutDeviceIntPoint* aRefPointOut)
{
  if (!sAPZC || !aRefPointOut) {
    return;
  }
  sAPZC->TransformCoordinateToGecko(aPoint, aRefPointOut);
}

nsEventStatus
APZController::ReceiveInputEvent(WidgetInputEvent* aEvent,
                                 ScrollableLayerGuid* aOutTargetGuid)
{
  MOZ_ASSERT(aEvent);

  if (!sAPZC) {
    return nsEventStatus_eIgnore;
  }
  return sAPZC->ReceiveInputEvent(*aEvent->AsInputEvent(), aOutTargetGuid);
}

nsEventStatus
APZController::ReceiveInputEvent(WidgetInputEvent* aInEvent,
                                 ScrollableLayerGuid* aOutTargetGuid,
                                 WidgetInputEvent* aOutEvent)
{
  MOZ_ASSERT(aInEvent);
  MOZ_ASSERT(aOutEvent);

  if (!sAPZC) {
    return nsEventStatus_eIgnore;
  }
  return sAPZC->ReceiveInputEvent(*aInEvent->AsInputEvent(),
                                  aOutTargetGuid,
                                  aOutEvent);
}

// APZC sends us this request when we need to update the display port on
// the scrollable frame the apzc is managing.
void
APZController::RequestContentRepaint(const FrameMetrics& aFrameMetrics)
{
  if (!mWidgetListener) {
    NS_WARNING("Can't update display port, !mWidgetListener");
    return;
  }

#ifdef DEBUG_CONTROLLER
  WinUtils::Log("APZController::RequestContentRepaint scrollid=%I64d",
    aFrameMetrics.mScrollId);
#endif
  nsCOMPtr<nsIRunnable> r1 = new RequestContentRepaintEvent(aFrameMetrics,
                                                            mWidgetListener,
                                                            &mLastScrollOffset,
                                                            &mLastScrollLayerGuid);
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(r1);
  } else {
    r1->Run();
  }
}

// Content send us this when it detect content has scrolled via
// a dom scroll event. Note we get these in response to dom scroll
// events and as a result of apzc scrolling which we filter out.
void
APZController::UpdateScrollOffset(const mozilla::layers::ScrollableLayerGuid& aScrollLayerId,
                                  CSSIntPoint& aScrollOffset)
{
#ifdef DEBUG_CONTROLLER
  WinUtils::Log("APZController::UpdateScrollOffset: scrollid:%I64d == %I64d offsets: %d,%d == %d,%d",
    aScrollLayerId.mScrollId, aScrollLayerId.mScrollId,
    aScrollOffset.x, aScrollOffset.y,
    mLastScrollOffset.x, mLastScrollOffset.y);
#endif

  // Bail if this the same scroll guid the apzc just scrolled and the offsets
  // equal the offset the apzc set.
  if (!sAPZC || (mLastScrollLayerGuid.mScrollId == aScrollLayerId.mScrollId &&
                 mLastScrollLayerGuid.mPresShellId == aScrollLayerId.mPresShellId &&
                 mLastScrollOffset == aScrollOffset)) {
#ifdef DEBUG_CONTROLLER
    WinUtils::Log("Skipping UpdateScrollOffset");
#endif
    return;
  }
  sAPZC->UpdateScrollOffset(aScrollLayerId, aScrollOffset);
}

// Gesture event handlers from the APZC. Currently not in use.

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

// requests that we send a mozbrowserasyncscroll domevent. not in use.
void
APZController::SendAsyncScrollDOMEvent(bool aIsRoot,
                                       const CSSRect &aContentRect,
                                       const CSSSize &aScrollableSize)
{
}

void
APZController::PostDelayedTask(Task* aTask, int aDelayMs)
{
  MessageLoop::current()->PostDelayedTask(FROM_HERE, aTask, aDelayMs);
}

// async scroll notifications

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
