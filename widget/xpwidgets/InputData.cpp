/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputData.h"

#include "nsGUIEvent.h"
#include "mozilla/dom/Touch.h"
#include "nsDebug.h"
#include "nsThreadUtils.h"

namespace mozilla {

using namespace dom;

MultiTouchInput::MultiTouchInput(const nsTouchEvent& aTouchEvent)
  : InputData(MULTITOUCH_INPUT, aTouchEvent.time)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(),
                    "Can only copy from nsTouchEvent on main thread");

  switch (aTouchEvent.message) {
    case NS_TOUCH_START:
      mType = MULTITOUCH_START;
      break;
    case NS_TOUCH_MOVE:
      mType = MULTITOUCH_MOVE;
      break;
    case NS_TOUCH_END:
      mType = MULTITOUCH_END;
      break;
    case NS_TOUCH_ENTER:
      mType = MULTITOUCH_ENTER;
      break;
    case NS_TOUCH_LEAVE:
      mType = MULTITOUCH_LEAVE;
      break;
    case NS_TOUCH_CANCEL:
      mType = MULTITOUCH_CANCEL;
      break;
    default:
      NS_WARNING("Did not assign a type to a MultiTouchInput");
      break;
  }

  for (size_t i = 0; i < aTouchEvent.touches.Length(); i++) {
    Touch* domTouch = static_cast<Touch*>(aTouchEvent.touches[i].get());

    // Extract data from weird interfaces.
    int32_t identifier, radiusX, radiusY;
    float rotationAngle, force;
    domTouch->GetIdentifier(&identifier);
    domTouch->GetRadiusX(&radiusX);
    domTouch->GetRadiusY(&radiusY);
    domTouch->GetRotationAngle(&rotationAngle);
    domTouch->GetForce(&force);

    SingleTouchData data(identifier,
                         ScreenIntPoint::FromUnknownPoint(
                           gfx::IntPoint(domTouch->mRefPoint.x,
                                         domTouch->mRefPoint.y)),
                         ScreenSize(radiusX, radiusY),
                         rotationAngle,
                         force);

    mTouches.AppendElement(data);
  }
}

// This conversion from nsMouseEvent to MultiTouchInput is needed because on
// the B2G emulator we can only receive mouse events, but we need to be able
// to pan correctly. To do this, we convert the events into a format that the
// panning code can handle. This code is very limited and only supports
// SingleTouchData. It also sends garbage for the identifier, radius, force
// and rotation angle.
MultiTouchInput::MultiTouchInput(const nsMouseEvent& aMouseEvent)
  : InputData(MULTITOUCH_INPUT, aMouseEvent.time)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(),
                    "Can only copy from nsMouseEvent on main thread");
  switch (aMouseEvent.message) {
  case NS_MOUSE_BUTTON_DOWN:
    mType = MULTITOUCH_START;
    break;
  case NS_MOUSE_MOVE:
    mType = MULTITOUCH_MOVE;
    break;
  case NS_MOUSE_BUTTON_UP:
    mType = MULTITOUCH_END;
    break;
  // The mouse pointer has been interrupted in an implementation-specific
  // manner, such as a synchronous event or action cancelling the touch, or a
  // touch point leaving the document window and going into a non-document
  // area capable of handling user interactions.
  case NS_MOUSE_EXIT:
    mType = MULTITOUCH_CANCEL;
    break;
  default:
    NS_WARNING("Did not assign a type to a MultiTouchInput");
    break;
  }

  mTouches.AppendElement(SingleTouchData(0,
                                         ScreenIntPoint::FromUnknownPoint(
                                           gfx::IntPoint(aMouseEvent.refPoint.x,
                                                         aMouseEvent.refPoint.y)),
                                         ScreenSize(1, 1),
                                         180.0f,
                                         1.0f));
}
}
