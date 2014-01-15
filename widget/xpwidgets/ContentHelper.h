/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozilla_widget_ContentHelper_h__
#define __mozilla_widget_ContentHelper_h__

#include "nsIFrame.h"
#include "nsIWidget.h"
#include "mozilla/layers/APZCTreeManager.h"

namespace mozilla {
namespace widget {

/*
 * Allow different platform widgets to access Content/DOM stuff.
 */
class ContentHelper
{
  typedef mozilla::layers::AllowedTouchBehavior AllowedTouchBehavior;
  typedef uint32_t TouchBehaviorFlags;

private:
  static uint32_t GetTouchActionFromFrame(nsIFrame* aFrame);
  static void UpdateAllowedBehavior(uint32_t aTouchActionValue, bool aConsiderPanning, TouchBehaviorFlags& aOutBehavior);

public:
  /*
   * Performs hit testing on content, finds frame that corresponds to the aPoint and retrieves
   * touch-action css property value from it according the rules specified in the spec:
   * http://www.w3.org/TR/pointerevents/#the-touch-action-css-property.
   */
  static TouchBehaviorFlags GetAllowedTouchBehavior(nsIWidget* aWidget, const nsIntPoint& aPoint);
};

}
}

#endif /*__mozilla_widget_ContentHelper_h__ */