/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentHelper.h"
#include "nsQueryFrame.h"
#include "nsIContent.h"
#include "nsIScrollableFrame.h"
#include "nsLayoutUtils.h"
#include "nsStyleConsts.h"
#include "nsView.h"

namespace mozilla {
namespace widget {

uint32_t
ContentHelper::GetTouchActionFromFrame(nsIFrame* aFrame)
{
  if (!aFrame || !aFrame->GetContent() || !aFrame->GetContent()->GetPrimaryFrame()) {
    // If frame is invalid or null then return default value.
    return NS_STYLE_TOUCH_ACTION_AUTO;
  }

  if (!aFrame->IsFrameOfType(nsIFrame::eSVG) && !aFrame->IsFrameOfType(nsIFrame::eBlockFrame)) {
    // Since touch-action property can be applied to only svg and block-level
    // elements we ignore frames of other types.
    return NS_STYLE_TOUCH_ACTION_AUTO;
  }

  return (aFrame->GetContent()->GetPrimaryFrame()->StyleDisplay()->mTouchAction);
}

void
ContentHelper::UpdateAllowedBehavior(uint32_t aTouchActionValue, bool aConsiderPanning, TouchBehaviorFlags& aOutBehavior)
{
  if (aTouchActionValue != NS_STYLE_TOUCH_ACTION_AUTO) {
    // Dropping zoom flag since zooming requires touch-action values of all touches
    // to be AUTO.
    aOutBehavior &= ~AllowedTouchBehavior::ZOOM;
  }

  if (aConsiderPanning) {
    if (aTouchActionValue == NS_STYLE_TOUCH_ACTION_NONE) {
      aOutBehavior &= ~AllowedTouchBehavior::VERTICAL_PAN;
      aOutBehavior &= ~AllowedTouchBehavior::HORIZONTAL_PAN;
    }

    // Values pan-x and pan-y set at the same time to the same element do not affect panning constraints.
    // Therefore we need to check whether pan-x is set without pan-y and the same for pan-y.
    if ((aTouchActionValue & NS_STYLE_TOUCH_ACTION_PAN_X) && !(aTouchActionValue & NS_STYLE_TOUCH_ACTION_PAN_Y)) {
      aOutBehavior &= ~AllowedTouchBehavior::VERTICAL_PAN;
    } else if ((aTouchActionValue & NS_STYLE_TOUCH_ACTION_PAN_Y) && !(aTouchActionValue & NS_STYLE_TOUCH_ACTION_PAN_X)) {
      aOutBehavior &= ~AllowedTouchBehavior::HORIZONTAL_PAN;
    }
  }
}

ContentHelper::TouchBehaviorFlags
ContentHelper::GetAllowedTouchBehavior(nsIWidget* aWidget, const nsIntPoint& aPoint)
{
  nsView *view = nsView::GetViewFor(aWidget);
  nsIFrame *viewFrame = view->GetFrame();

  nsPoint relativePoint = nsLayoutUtils::GetEventCoordinatesRelativeTo(aWidget, aPoint, viewFrame);

  nsIFrame *target = nsLayoutUtils::GetFrameForPoint(viewFrame, relativePoint, nsLayoutUtils::IGNORE_ROOT_SCROLL_FRAME);
  nsIScrollableFrame *nearestScrollableParent = nsLayoutUtils::GetNearestScrollableFrame(target, 0);
  nsIFrame* nearestScrollableFrame = do_QueryFrame(nearestScrollableParent);

  // We're walking up the DOM tree until we meet the element with touch behavior and accumulating
  // touch-action restrictions of all elements in this chain.
  // The exact quote from the spec, that clarifies more:
  // To determine the effect of a touch, find the nearest ancestor (starting from the element itself)
  // that has a default touch behavior. Then examine the touch-action property of each element between
  // the hit tested element and the element with the default touch behavior (including both the hit
  // tested element and the element with the default touch behavior). If the touch-action property of
  // any of those elements disallows the default touch behavior, do nothing. Otherwise allow the element
  // to start considering the touch for the purposes of executing a default touch behavior.

  // Currently we support only two touch behaviors: panning and zooming.
  // For panning we walk up until we meet the first scrollable element (the element that supports panning)
  // or root element.
  // For zooming we walk up until the root element since Firefox currently supports only zooming of the
  // root frame but not the subframes.

  bool considerPanning = true;
  TouchBehaviorFlags behavior = AllowedTouchBehavior::VERTICAL_PAN | AllowedTouchBehavior::HORIZONTAL_PAN |
                                AllowedTouchBehavior::ZOOM;

  for (nsIFrame *frame = target; frame && frame->GetContent() && behavior; frame = frame->GetParent()) {
    UpdateAllowedBehavior(GetTouchActionFromFrame(frame), considerPanning, behavior);

    if (frame == nearestScrollableFrame) {
      // We met the scrollable element, after it we shouldn't consider touch-action
      // values for the purpose of panning but only for zooming.
      considerPanning = false;
    }
  }

  return behavior;
}

}
}