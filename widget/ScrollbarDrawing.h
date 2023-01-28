/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_ScrollbarDrawing_h
#define mozilla_widget_ScrollbarDrawing_h

#include "mozilla/dom/RustTypes.h"
#include "mozilla/gfx/2D.h"
#include "nsColor.h"
#include "nsITheme.h"
#include "ThemeColors.h"
#include "ThemeDrawing.h"
#include "Units.h"

namespace mozilla::widget {

class ScrollbarDrawing {
 protected:
  using DPIRatio = mozilla::CSSToLayoutDeviceScale;
  using ElementState = dom::ElementState;
  using DocumentState = dom::DocumentState;
  using DrawTarget = mozilla::gfx::DrawTarget;
  using sRGBColor = mozilla::gfx::sRGBColor;
  using Colors = ThemeColors;
  using Overlay = nsITheme::Overlay;
  using WebRenderBackendData = mozilla::widget::WebRenderBackendData;

  enum class Kind : uint8_t {
    Android,
    Cocoa,
    Gtk,
    Win10,
    Win11,
  };

  explicit ScrollbarDrawing(Kind aKind) : mKind(aKind) {}

 public:
  virtual ~ScrollbarDrawing() = default;

  enum class ScrollbarKind : uint8_t {
    Horizontal,
    VerticalLeft,
    VerticalRight,
  };

  DPIRatio GetDPIRatioForScrollbarPart(const nsPresContext*);

  static nsIFrame* GetParentScrollbarFrame(nsIFrame* aFrame);
  static bool IsParentScrollbarRolledOver(nsIFrame* aFrame);
  static bool IsParentScrollbarHoveredOrActive(nsIFrame* aFrame);

  static bool IsScrollbarWidthThin(const ComputedStyle& aStyle);
  static bool IsScrollbarWidthThin(nsIFrame* aFrame);

  CSSIntCoord GetCSSScrollbarSize(StyleScrollbarWidth, Overlay) const;
  LayoutDeviceIntCoord GetScrollbarSize(const nsPresContext*,
                                        StyleScrollbarWidth, Overlay);
  LayoutDeviceIntCoord GetScrollbarSize(const nsPresContext*, nsIFrame*);

  virtual LayoutDeviceIntSize GetMinimumWidgetSize(nsPresContext*,
                                                   StyleAppearance aAppearance,
                                                   nsIFrame* aFrame) = 0;
  virtual Maybe<nsITheme::Transparency> GetScrollbarPartTransparency(
      nsIFrame* aFrame, StyleAppearance aAppearance) {
    return Nothing();
  }

  bool IsScrollbarTrackOpaque(nsIFrame*);
  virtual sRGBColor ComputeScrollbarTrackColor(nsIFrame*, const ComputedStyle&,
                                               const DocumentState&,
                                               const Colors&);
  virtual sRGBColor ComputeScrollbarThumbColor(nsIFrame*, const ComputedStyle&,
                                               const ElementState&,
                                               const DocumentState&,
                                               const Colors&);

  nscolor GetScrollbarButtonColor(nscolor aTrackColor, ElementState);
  Maybe<nscolor> GetScrollbarArrowColor(nscolor aButtonColor);

  // Returned colors are button, arrow.
  virtual std::pair<sRGBColor, sRGBColor> ComputeScrollbarButtonColors(
      nsIFrame*, StyleAppearance, const ComputedStyle&, const ElementState&,
      const DocumentState&, const Colors&);

  virtual bool PaintScrollbarButton(DrawTarget&, StyleAppearance,
                                    const LayoutDeviceRect&, ScrollbarKind,
                                    nsIFrame*, const ComputedStyle&,
                                    const ElementState&, const DocumentState&,
                                    const Colors&, const DPIRatio&);

  virtual bool PaintScrollbarThumb(DrawTarget&, const LayoutDeviceRect&,
                                   ScrollbarKind, nsIFrame*,
                                   const ComputedStyle&, const ElementState&,
                                   const DocumentState&, const Colors&,
                                   const DPIRatio&) = 0;
  virtual bool PaintScrollbarThumb(WebRenderBackendData&,
                                   const LayoutDeviceRect&, ScrollbarKind,
                                   nsIFrame*, const ComputedStyle&,
                                   const ElementState&, const DocumentState&,
                                   const Colors&, const DPIRatio&) = 0;

  template <typename PaintBackendData>
  bool DoPaintDefaultScrollbar(PaintBackendData&, const LayoutDeviceRect&,
                               ScrollbarKind, nsIFrame*, const ComputedStyle&,
                               const ElementState&, const DocumentState&,
                               const Colors&, const DPIRatio&);
  bool PaintScrollbar(DrawTarget&, const LayoutDeviceRect&, ScrollbarKind,
                      nsIFrame*, const ComputedStyle&, const ElementState&,
                      const DocumentState&, const Colors&, const DPIRatio&);
  bool PaintScrollbar(WebRenderBackendData&, const LayoutDeviceRect&,
                      ScrollbarKind, nsIFrame*, const ComputedStyle&,
                      const ElementState&, const DocumentState&, const Colors&,
                      const DPIRatio&);

  virtual bool PaintScrollbarTrack(DrawTarget&, const LayoutDeviceRect&,
                                   ScrollbarKind, nsIFrame*,
                                   const ComputedStyle&, const DocumentState&,
                                   const Colors&, const DPIRatio&) {
    // Draw nothing by default. Subclasses can override this.
    return true;
  }
  virtual bool PaintScrollbarTrack(WebRenderBackendData&,
                                   const LayoutDeviceRect&, ScrollbarKind,
                                   nsIFrame*, const ComputedStyle&,
                                   const DocumentState&, const Colors&,
                                   const DPIRatio&) {
    // Draw nothing by default. Subclasses can override this.
    return true;
  }

  template <typename PaintBackendData>
  bool DoPaintDefaultScrollCorner(PaintBackendData&, const LayoutDeviceRect&,
                                  ScrollbarKind, nsIFrame*,
                                  const ComputedStyle&, const DocumentState&,
                                  const Colors&, const DPIRatio&);
  virtual bool PaintScrollCorner(DrawTarget&, const LayoutDeviceRect&,
                                 ScrollbarKind, nsIFrame*, const ComputedStyle&,
                                 const DocumentState&, const Colors&,
                                 const DPIRatio&);
  virtual bool PaintScrollCorner(WebRenderBackendData&, const LayoutDeviceRect&,
                                 ScrollbarKind, nsIFrame*, const ComputedStyle&,
                                 const DocumentState&, const Colors&,
                                 const DPIRatio&);

  virtual void RecomputeScrollbarParams() = 0;

  virtual bool ShouldDrawScrollbarButtons() { return true; }

 private:
  // The scrollbar sizes for all our scrollbars. Indices are overlay or not,
  // then thin or not. Should be configured via ConfigureScrollbarSize.
  CSSIntCoord mScrollbarSize[2][2]{};

 protected:
  // For some kind of style differences a full virtual method is overkill, so we
  // store the kind here so we can branch on it if necessary.
  Kind mKind;

  // Configures the scrollbar sizes based on a single size.
  void ConfigureScrollbarSize(CSSIntCoord);

  // Configures a particular scrollbar size.
  void ConfigureScrollbarSize(StyleScrollbarWidth, Overlay, CSSIntCoord);
};

}  // namespace mozilla::widget

#endif
