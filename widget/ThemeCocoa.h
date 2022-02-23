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

  NS_IMETHOD GetMinimumWidgetSize(nsPresContext* aPresContext, nsIFrame* aFrame,
                                  StyleAppearance aAppearance,
                                  mozilla::LayoutDeviceIntSize* aResult,
                                  bool* aIsOverridable) override;

  ThemeGeometryType ThemeGeometryTypeForWidget(nsIFrame*,
                                               StyleAppearance) override;
  bool ThemeSupportsWidget(nsPresContext*, nsIFrame*, StyleAppearance) override;

 protected:
  virtual ~ThemeCocoa() = default;
};

}  // namespace mozilla::widget

#endif
