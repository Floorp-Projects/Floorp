/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_ScrollbarDrawing_h
#define mozilla_widget_ScrollbarDrawing_h

#include "mozilla/EventStates.h"
#include "mozilla/gfx/2D.h"
#include "nsColor.h"
#include "nsITheme.h"
#include "ThemeColors.h"
#include "ThemeDrawing.h"
#include "Units.h"

namespace mozilla::widget {

static constexpr gfx::sRGBColor sScrollbarColor(
    gfx::sRGBColor::UnusualFromARGB(0xfff0f0f0));
static constexpr gfx::sRGBColor sScrollbarThumbColor(
    gfx::sRGBColor::UnusualFromARGB(0xffcdcdcd));

class ScrollbarDrawing {
 protected:
  using DPIRatio = mozilla::CSSToLayoutDeviceScale;
  using EventStates = mozilla::EventStates;
  using DrawTarget = mozilla::gfx::DrawTarget;
  using sRGBColor = mozilla::gfx::sRGBColor;
  using Colors = ThemeColors;
  using ScrollbarSizes = nsITheme::ScrollbarSizes;
  using Overlay = nsITheme::Overlay;
  using WebRenderBackendData = mozilla::widget::WebRenderBackendData;

 public:
  ScrollbarDrawing() = default;
  virtual ~ScrollbarDrawing() = default;

  struct ScrollbarParams {
    bool isOverlay = false;
    bool isRolledOver = false;
    bool isSmall = false;
    bool isHorizontal = false;
    bool isRtl = false;
    bool isOnDarkBackground = false;
    bool isCustom = false;
    // Two colors only used when custom is true.
    nscolor trackColor = NS_RGBA(0, 0, 0, 0);
    nscolor faceColor = NS_RGBA(0, 0, 0, 0);
  };

  static DPIRatio GetDPIRatioForScrollbarPart(nsPresContext*);

  static nsIFrame* GetParentScrollbarFrame(nsIFrame* aFrame);
  static bool IsParentScrollbarRolledOver(nsIFrame* aFrame);
  static bool IsParentScrollbarHoveredOrActive(nsIFrame* aFrame);

  static bool IsScrollbarWidthThin(const ComputedStyle& aStyle);
  static bool IsScrollbarWidthThin(nsIFrame* aFrame);

  virtual ScrollbarSizes GetScrollbarSizes(nsPresContext*, StyleScrollbarWidth,
                                           Overlay);
  virtual LayoutDeviceIntSize GetMinimumWidgetSize(nsPresContext*,
                                                   StyleAppearance aAppearance,
                                                   nsIFrame* aFrame) = 0;
  virtual Maybe<nsITheme::Transparency> GetScrollbarPartTransparency(
      nsIFrame* aFrame, StyleAppearance aAppearance) {
    return Nothing();
  }

  static bool IsScrollbarTrackOpaque(nsIFrame*);
  static sRGBColor ComputeScrollbarTrackColor(nsIFrame*, const ComputedStyle&,
                                              const EventStates& aDocumentState,
                                              const Colors&);
  static sRGBColor ComputeScrollbarThumbColor(nsIFrame*, const ComputedStyle&,
                                              const EventStates& aElementState,
                                              const EventStates& aDocumentState,
                                              const Colors&);

  static ScrollbarParams ComputeScrollbarParams(nsIFrame* aFrame,
                                                const ComputedStyle& aStyle,
                                                bool aIsHorizontal);
  static bool ShouldUseDarkScrollbar(nsIFrame*, const ComputedStyle&);

  static nscolor GetScrollbarButtonColor(nscolor aTrackColor, EventStates);
  static Maybe<nscolor> GetScrollbarArrowColor(nscolor aButtonColor);

  // Returned colors are button, arrow.
  std::pair<sRGBColor, sRGBColor> ComputeScrollbarButtonColors(
      nsIFrame*, StyleAppearance, const ComputedStyle&,
      const EventStates& aElementState, const EventStates& aDocumentState,
      const Colors&);

  bool PaintScrollbarButton(DrawTarget&, StyleAppearance,
                            const LayoutDeviceRect&, nsIFrame*,
                            const ComputedStyle&,
                            const EventStates& aElementState,
                            const EventStates& aDocumentState, const Colors&,
                            const DPIRatio&);

  virtual bool PaintScrollbarThumb(DrawTarget&, const LayoutDeviceRect&,
                                   bool aHorizontal, nsIFrame*,
                                   const ComputedStyle&,
                                   const EventStates& aElementState,
                                   const EventStates& aDocumentState,
                                   const Colors&, const DPIRatio&) = 0;
  virtual bool PaintScrollbarThumb(WebRenderBackendData&,
                                   const LayoutDeviceRect&, bool aHorizontal,
                                   nsIFrame*, const ComputedStyle&,
                                   const EventStates& aElementState,
                                   const EventStates& aDocumentState,
                                   const Colors&, const DPIRatio&) = 0;

  template <typename PaintBackendData>
  bool DoPaintDefaultScrollbar(PaintBackendData&, const LayoutDeviceRect&,
                               bool aHorizontal, nsIFrame*,
                               const ComputedStyle&,
                               const EventStates& aElementState,
                               const EventStates& aDocumentState, const Colors&,
                               const DPIRatio&);
  virtual bool PaintScrollbar(DrawTarget&, const LayoutDeviceRect&,
                              bool aHorizontal, nsIFrame*, const ComputedStyle&,
                              const EventStates& aElementState,
                              const EventStates& aDocumentState, const Colors&,
                              const DPIRatio&);
  virtual bool PaintScrollbar(WebRenderBackendData&, const LayoutDeviceRect&,
                              bool aHorizontal, nsIFrame*, const ComputedStyle&,
                              const EventStates& aElementState,
                              const EventStates& aDocumentState, const Colors&,
                              const DPIRatio&);

  virtual bool PaintScrollbarTrack(DrawTarget&, const LayoutDeviceRect&,
                                   bool aHorizontal, nsIFrame*,
                                   const ComputedStyle&,
                                   const EventStates& aDocumentState,
                                   const Colors&, const DPIRatio&) {
    // Draw nothing by default. Subclasses can override this.
    return true;
  }
  virtual bool PaintScrollbarTrack(WebRenderBackendData&,
                                   const LayoutDeviceRect&, bool aHorizontal,
                                   nsIFrame*, const ComputedStyle&,
                                   const EventStates& aDocumentState,
                                   const Colors&, const DPIRatio&) {
    // Draw nothing by default. Subclasses can override this.
    return true;
  }

  template <typename PaintBackendData>
  bool DoPaintDefaultScrollCorner(PaintBackendData&, const LayoutDeviceRect&,
                                  nsIFrame*, const ComputedStyle&,
                                  const EventStates& aDocumentState,
                                  const Colors&, const DPIRatio&);
  virtual bool PaintScrollCorner(DrawTarget&, const LayoutDeviceRect&,
                                 nsIFrame*, const ComputedStyle&,
                                 const EventStates& aDocumentState,
                                 const Colors&, const DPIRatio&);
  virtual bool PaintScrollCorner(WebRenderBackendData&, const LayoutDeviceRect&,
                                 nsIFrame*, const ComputedStyle&,
                                 const EventStates& aDocumentState,
                                 const Colors&, const DPIRatio&);

  virtual void RecomputeScrollbarParams() = 0;

  virtual bool ShouldDrawScrollbarButtons() { return true; }

  uint32_t GetHorizontalScrollbarHeight() const {
    return mHorizontalScrollbarHeight;
  }
  uint32_t GetVerticalScrollbarWidth() const { return mVerticalScrollbarWidth; }

 protected:
  uint32_t mHorizontalScrollbarHeight = 0;
  uint32_t mVerticalScrollbarWidth = 0;
};

}  // namespace mozilla::widget

#endif
