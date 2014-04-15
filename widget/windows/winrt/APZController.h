/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "mozwrlbase.h"

#include "mozilla/layers/GeckoContentController.h"
#include "mozilla/layers/APZCTreeManager.h"
#include "mozilla/EventForwards.h"
#include "FrameMetrics.h"
#include "Units.h"

class nsIWidgetListener;

namespace mozilla {
namespace widget {
namespace winrt {

class APZController :
  public mozilla::layers::GeckoContentController
{
  typedef mozilla::layers::FrameMetrics FrameMetrics;
  typedef mozilla::layers::ScrollableLayerGuid ScrollableLayerGuid;
  typedef mozilla::layers::ZoomConstraints ZoomConstraints;

public:
  APZController() :
    mWidgetListener(nullptr)
  {
  }

  // GeckoContentController interface
  virtual void RequestContentRepaint(const FrameMetrics& aFrameMetrics);
  virtual void AcknowledgeScrollUpdate(const FrameMetrics::ViewID& aScrollId, const uint32_t& aScrollGeneration);
  virtual void HandleDoubleTap(const mozilla::CSSPoint& aPoint,
                               int32_t aModifiers,
                               const mozilla::layers::ScrollableLayerGuid& aGuid);
  virtual void HandleSingleTap(const mozilla::CSSPoint& aPoint,
                               int32_t aModifiers,
                               const mozilla::layers::ScrollableLayerGuid& aGuid);
  virtual void HandleLongTap(const mozilla::CSSPoint& aPoint,
                             int32_t aModifiers,
                             const mozilla::layers::ScrollableLayerGuid& aGuid);
  virtual void HandleLongTapUp(const mozilla::CSSPoint& aPoint,
                               int32_t aModifiers,
                               const mozilla::layers::ScrollableLayerGuid& aGuid);
  virtual void SendAsyncScrollDOMEvent(bool aIsRoot, const mozilla::CSSRect &aContentRect, const mozilla::CSSSize &aScrollableSize);
  virtual void PostDelayedTask(Task* aTask, int aDelayMs);
  virtual bool GetRootZoomConstraints(ZoomConstraints* aOutConstraints);
  virtual void NotifyTransformBegin(const ScrollableLayerGuid& aGuid);
  virtual void NotifyTransformEnd(const ScrollableLayerGuid& aGuid);
  
  void SetWidgetListener(nsIWidgetListener* aWidgetListener);

  bool HitTestAPZC(mozilla::ScreenIntPoint& aPoint);
  void TransformCoordinateToGecko(const mozilla::ScreenIntPoint& aPoint,
                                  LayoutDeviceIntPoint* aRefPointOut);
  void ContentReceivedTouch(const ScrollableLayerGuid& aGuid, bool aPreventDefault);
  nsEventStatus ReceiveInputEvent(mozilla::WidgetInputEvent* aEvent,
                                  ScrollableLayerGuid* aOutTargetGuid);

public:
  // todo: make this a member variable as prep for multiple views
  static nsRefPtr<mozilla::layers::APZCTreeManager> sAPZC;

private:
  nsIWidgetListener* mWidgetListener;
};

} } }
