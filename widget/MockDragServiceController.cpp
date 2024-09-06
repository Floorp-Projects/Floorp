/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MockDragServiceController.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DragEvent.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "nsIFrame.h"
#include "nsPresContext.h"
#include "nsBaseDragService.h"

namespace mozilla::test {

NS_IMPL_ISUPPORTS(MockDragServiceController, nsIMockDragServiceController)

class MockDragService : public nsBaseDragService {
 public:
  MOZ_CAN_RUN_SCRIPT nsresult
  InvokeDragSessionImpl(nsIArray* aTransferableArray,
                        const mozilla::Maybe<mozilla::CSSIntRegion>& aRegion,
                        uint32_t aActionType) override {
    // In Windows' nsDragService, InvokeDragSessionImpl would establish a
    // nested (native) event loop that runs as long as the drag is happening.
    // See DoDragDrop in MSDN.
    // We cannot do anything like that here since it would block mochitest
    // from scripting behavior with MockDragServiceController::SendDragEvent.

    // mDragAction is not yet handled properly in the MockDragService.
    // This should be updated with each drag event.  Instead, we always MOVE.
    mDragAction = DRAGDROP_ACTION_MOVE;
    StartDragSession();
    return NS_OK;
  }

  bool IsMockService() override { return true; }

  uint32_t mLastModifierKeyState = 0;
};

static void SetDragEndPointFromScreenPoint(
    MockDragService* aService, nsPresContext* aPc,
    const LayoutDeviceIntPoint& aScreenPt) {
  // aScreenPt is screen-relative, and we want to be
  // top-level-widget-relative.
  auto* widget = aPc->GetRootWidget();
  auto pt = aScreenPt - widget->WidgetToScreenOffset();
  pt += widget->WidgetToTopLevelWidgetOffset();
  aService->SetDragEndPoint(pt);
}

static bool IsMouseEvent(nsIMockDragServiceController::EventType aEventType) {
  using EventType = nsIMockDragServiceController::EventType;
  switch (aEventType) {
    case EventType::eMouseDown:
    case EventType::eMouseMove:
    case EventType::eMouseUp:
      return true;
    default:
      return false;
  }
}

static EventMessage MockEventTypeToEventMessage(
    nsIMockDragServiceController::EventType aEventType) {
  using EventType = nsIMockDragServiceController::EventType;

  switch (aEventType) {
    case EventType::eDragEnter:
      return EventMessage::eDragEnter;
    case EventType::eDragOver:
      return EventMessage::eDragOver;
    case EventType::eDragLeave:
      return EventMessage::eDragLeave;
    case EventType::eDrop:
      return EventMessage::eDrop;
    case EventType::eMouseDown:
      return EventMessage::eMouseDown;
    case EventType::eMouseMove:
      return EventMessage::eMouseMove;
    case EventType::eMouseUp:
      return EventMessage::eMouseUp;
    default:
      MOZ_ASSERT(false, "Invalid event type");
      return EventMessage::eVoidEvent;
  }
}

MockDragServiceController::MockDragServiceController()
    : mDragService(new MockDragService()) {}

MockDragServiceController::~MockDragServiceController() = default;

NS_IMETHODIMP
MockDragServiceController::GetMockDragService(nsIDragService** aService) {
  RefPtr<nsIDragService> ds = mDragService;
  ds.forget(aService);
  return NS_OK;
}

NS_IMETHODIMP
MockDragServiceController::SendEvent(
    dom::BrowsingContext* aBC,
    nsIMockDragServiceController::EventType aEventType, int32_t aScreenX,
    int32_t aScreenY, uint32_t aKeyModifiers = 0) {
  RefPtr<nsIWidget> widget =
      aBC->Canonical()->GetParentProcessWidgetContaining();
  NS_ENSURE_TRUE(widget, NS_ERROR_UNEXPECTED);
  auto* embedder = aBC->Top()->GetEmbedderElement();
  NS_ENSURE_TRUE(embedder, NS_ERROR_UNEXPECTED);
  auto* frame = embedder->GetPrimaryFrame();
  NS_ENSURE_TRUE(frame, NS_ERROR_UNEXPECTED);
  auto* presCxt = frame->PresContext();
  MOZ_ASSERT(presCxt);

  EventMessage eventType = MockEventTypeToEventMessage(aEventType);
  UniquePtr<WidgetInputEvent> widgetEvent;
  if (IsMouseEvent(aEventType)) {
    widgetEvent = MakeUnique<WidgetMouseEvent>(true, eventType, widget,
                                               WidgetMouseEvent::Reason::eReal);
  } else {
    widgetEvent = MakeUnique<WidgetDragEvent>(true, eventType, widget);
  }

  widgetEvent->mWidget = widget;
  widgetEvent->mFlags.mIsSynthesizedForTests = true;

  auto clientPosInScreenCoords = widget->GetClientBounds().TopLeft();
  widgetEvent->mRefPoint =
      LayoutDeviceIntPoint(aScreenX, aScreenY) - clientPosInScreenCoords;

  RefPtr<MockDragService> ds = mDragService;
  ds->mLastModifierKeyState = aKeyModifiers;

  if (aEventType == EventType::eDragEnter) {
    // We expect StartDragSession to return an "error" when a drag session
    // already exists, which it will since we are testing dragging from
    // inside Gecko, so we don't check the return value.
    ds->StartDragSession();
  }

  nsCOMPtr<nsIDragSession> currentDragSession;
  nsresult rv = ds->GetCurrentSession(getter_AddRefs(currentDragSession));
  NS_ENSURE_SUCCESS(rv, rv);

  switch (aEventType) {
    case EventType::eMouseDown:
    case EventType::eMouseMove:
    case EventType::eMouseUp:
      widget->DispatchInputEvent(widgetEvent.get());
      break;
    case EventType::eDragEnter:
      NS_ENSURE_TRUE(currentDragSession, NS_ERROR_UNEXPECTED);
      currentDragSession->SetDragAction(nsIDragService::DRAGDROP_ACTION_MOVE);
      widget->DispatchInputEvent(widgetEvent.get());
      break;
    case EventType::eDragLeave: {
      NS_ENSURE_TRUE(currentDragSession, NS_ERROR_UNEXPECTED);
      currentDragSession->SetDragAction(nsIDragService::DRAGDROP_ACTION_MOVE);
      widget->DispatchInputEvent(widgetEvent.get());
      SetDragEndPointFromScreenPoint(ds, presCxt,
                                     LayoutDeviceIntPoint(aScreenX, aScreenY));
      nsCOMPtr<nsINode> sourceNode;
      rv = currentDragSession->GetSourceNode(getter_AddRefs(sourceNode));
      NS_ENSURE_SUCCESS(rv, rv);
      if (!sourceNode) {
        rv = ds->EndDragSession(false /* doneDrag */, aKeyModifiers);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    } break;
    case EventType::eDragOver:
      NS_ENSURE_TRUE(currentDragSession, NS_ERROR_UNEXPECTED);
      rv = ds->FireDragEventAtSource(EventMessage::eDrag, aKeyModifiers);
      currentDragSession->SetDragAction(nsIDragService::DRAGDROP_ACTION_MOVE);
      NS_ENSURE_SUCCESS(rv, rv);
      widget->DispatchInputEvent(widgetEvent.get());
      break;
    case EventType::eDrop: {
      NS_ENSURE_TRUE(currentDragSession, NS_ERROR_UNEXPECTED);
      currentDragSession->SetDragAction(nsIDragService::DRAGDROP_ACTION_MOVE);
      widget->DispatchInputEvent(widgetEvent.get());
      SetDragEndPointFromScreenPoint(ds, presCxt,
                                     LayoutDeviceIntPoint(aScreenX, aScreenY));
      rv = ds->EndDragSession(true /* doneDrag */, aKeyModifiers);
      NS_ENSURE_SUCCESS(rv, rv);
    } break;
    default:
      MOZ_ASSERT_UNREACHABLE("Impossible event type?");
      return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
MockDragServiceController::CancelDrag(uint32_t aKeyModifiers = 0) {
  RefPtr<MockDragService> ds = mDragService;
  ds->mLastModifierKeyState = aKeyModifiers;
  return ds->EndDragSession(false /* doneDrag */, aKeyModifiers);
}

}  // namespace mozilla::test
