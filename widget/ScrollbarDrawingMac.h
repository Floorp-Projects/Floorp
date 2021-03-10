/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_ScrollbarDrawing_h
#define mozilla_widget_ScrollbarDrawing_h

#include "nsColor.h"
#include "nsITheme.h"
#include "Units.h"

namespace mozilla {
namespace gfx {
class DrawTarget;
}

namespace widget {

struct ScrollbarParams {
  bool overlay = false;
  bool rolledOver = false;
  bool small = false;
  bool horizontal = false;
  bool rtl = false;
  bool onDarkBackground = false;
  bool custom = false;
  // Two colors only used when custom is true.
  nscolor trackColor = NS_RGBA(0, 0, 0, 0);
  nscolor faceColor = NS_RGBA(0, 0, 0, 0);
};

class ScrollbarDrawingMac final {
 public:
  static CSSIntCoord GetScrollbarSize(StyleScrollbarWidth, bool aOverlay);

  static LayoutDeviceIntCoord GetScrollbarSize(StyleScrollbarWidth,
                                               bool aOverlay, float aDpiRatio);
  static LayoutDeviceIntSize GetMinimumWidgetSize(StyleAppearance aAppearance,
                                                  nsIFrame* aFrame,
                                                  float aDpiRatio);
  static ScrollbarParams ComputeScrollbarParams(nsIFrame* aFrame,
                                                const ComputedStyle& aStyle,
                                                bool aIsHorizontal);
  static void DrawScrollbarThumb(gfx::DrawTarget& aDT, const gfx::Rect& aRect,
                                 const ScrollbarParams& aParams);
  static void DrawScrollbarTrack(gfx::DrawTarget& aDT, const gfx::Rect& aRect,
                                 const ScrollbarParams& aParams);
  static void DrawScrollCorner(gfx::DrawTarget& aDT, const gfx::Rect& aRect,
                               const ScrollbarParams& aParams);
};

}  // namespace widget
}  // namespace mozilla

#endif
