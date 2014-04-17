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
#include "mozilla/layers/APZCCallbackHelper.h"
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

public:
  RequestContentRepaintEvent(const FrameMetrics& aFrameMetrics,
                             nsIWidgetListener* aListener) :
    mFrameMetrics(aFrameMetrics),
    mWidgetListener(aListener)
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
    if (!GetDOMTargets(mFrameMetrics.GetScrollId(),
                       subDocument, targetContent)) {
      return NS_OK;
    }

    // If we're dealing with a sub frame or content editable element,
    // call UpdateSubFrame.
    if (targetContent) {
#ifdef DEBUG_CONTROLLER
      WinUtils::Log("APZController: detected subframe or content editable");
#endif
      mozilla::layers::APZCCallbackHelper::UpdateSubFrame(targetContent, mFrameMetrics);
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
        mozilla::layers::APZCCallbackHelper::UpdateRootFrame(utils, mFrameMetrics);

#ifdef DEBUG_CONTROLLER
        WinUtils::Log("APZController: %I64d mDisplayPortMargins: %0.2f %0.2f %0.2f %0.2f",
          mFrameMetrics.GetScrollId(),
          mFrameMetrics.GetDisplayPortMargins().left,
          mFrameMetrics.GetDisplayPortMargins().top,
          mFrameMetrics.GetDisplayPortMargins().right,
          mFrameMetrics.GetDisplayPortMargins().bottom);
#endif
      }
    }
    return NS_OK;
  }
protected:
  FrameMetrics mFrameMetrics;
  nsIWidgetListener* mWidgetListener;
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
    aFrameMetrics.GetScrollId());
#endif
  nsCOMPtr<nsIRunnable> r1 = new RequestContentRepaintEvent(aFrameMetrics,
                                                            mWidgetListener);
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(r1);
  } else {
    r1->Run();
  }
}

void
APZController::AcknowledgeScrollUpdate(const FrameMetrics::ViewID& aScrollId,
                                       const uint32_t& aScrollGeneration)
{
#ifdef DEBUG_CONTROLLER
  WinUtils::Log("APZController::AcknowledgeScrollUpdate scrollid=%I64d gen=%lu",
    aScrollId, aScrollGeneration);
#endif
  mozilla::layers::APZCCallbackHelper::AcknowledgeScrollUpdate(aScrollId, aScrollGeneration);
}

void
APZController::HandleDoubleTap(const CSSPoint& aPoint,
                               int32_t aModifiers,
                               const ScrollableLayerGuid& aGuid)
{
}

void
APZController::HandleSingleTap(const CSSPoint& aPoint,
                               int32_t aModifiers,
                               const ScrollableLayerGuid& aGuid)
{
}

void
APZController::HandleLongTap(const CSSPoint& aPoint,
                             int32_t aModifiers,
                             const ScrollableLayerGuid& aGuid)
{
}

void
APZController::HandleLongTapUp(const CSSPoint& aPoint,
                               int32_t aModifiers,
                               const ScrollableLayerGuid& aGuid)
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

bool
APZController::GetRootZoomConstraints(ZoomConstraints* aOutConstraints)
{
  if (aOutConstraints) {
    // Until we support the meta-viewport tag properly allow zooming
    // from 1/4 to 4x by default.
    aOutConstraints->mAllowZoom = true;
    aOutConstraints->mAllowDoubleTapZoom = false;
    aOutConstraints->mMinZoom = CSSToScreenScale(0.25f);
    aOutConstraints->mMaxZoom = CSSToScreenScale(4.0f);
    return true;
  }
  return false;
}

// apzc notifications

class TransformedStartEvent : public nsRunnable
{
  NS_IMETHOD Run() {
    MetroUtils::FireObserver("apzc-transform-start", L"");
    return NS_OK;
  }
};

class TransformedEndEvent : public nsRunnable
{
  NS_IMETHOD Run() {
    MetroUtils::FireObserver("apzc-transform-end", L"");
    return NS_OK;
  }
};

void
APZController::NotifyAPZStateChange(const ScrollableLayerGuid& aGuid,
                                    APZStateChange aChange,
                                    int aArg)
{
  switch (aChange) {
    case APZStateChange::TransformBegin:
    {
      if (NS_IsMainThread()) {
        MetroUtils::FireObserver("apzc-transform-begin", L"");
        return;
      }
      nsCOMPtr<nsIRunnable> runnable = new TransformedStartEvent();
      NS_DispatchToMainThread(runnable);
      break;
    }
    case APZStateChange::TransformEnd:
    {
      if (NS_IsMainThread()) {
        MetroUtils::FireObserver("apzc-transform-end", L"");
        return;
      }
      nsCOMPtr<nsIRunnable> runnable = new TransformedEndEvent();
      NS_DispatchToMainThread(runnable);
      break;
    }
    default:
    {
      // We don't currently care about other state changes.
      break;
    }
  }
}

} } }
