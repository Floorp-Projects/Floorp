/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_ThemeCocoa_h
#define mozilla_widget_ThemeCocoa_h

#include "Theme.h"

#include "ScrollbarDrawingCocoa.h"

namespace mozilla::widget {

class ThemeCocoa : public Theme {
 public:
  explicit ThemeCocoa(UniquePtr<ScrollbarDrawing>&& aScrollbarDrawing)
      : Theme(std::move(aScrollbarDrawing)) {}

  LayoutDeviceIntSize GetMinimumWidgetSize(
      nsPresContext* aPresContext, nsIFrame* aFrame,
      StyleAppearance aAppearance) override;

  NS_IMETHOD DrawWidgetBackground(gfxContext* aContext, nsIFrame*,
                                  StyleAppearance, const nsRect& aRect,
                                  const nsRect& aDirtyRect,
                                  DrawOverflow) override;

  bool CreateWebRenderCommandsForWidget(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const layers::StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager, nsIFrame*, StyleAppearance,
      const nsRect& aRect) override;

 protected:
  virtual ~ThemeCocoa() = default;
};

}  // namespace mozilla::widget

#endif
