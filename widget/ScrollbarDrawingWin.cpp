/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollbarDrawingWin.h"

#include "mozilla/gfx/Helpers.h"
#include "mozilla/Maybe.h"
#include "mozilla/StaticPrefs_widget.h"
#include "nsLayoutUtils.h"
#include "nsNativeBasicTheme.h"
#include "nsNativeTheme.h"

using mozilla::ComputedStyle;
using mozilla::EventStates;
using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::Some;
using mozilla::StyleAppearance;
using mozilla::StyleScrollbarWidth;

LayoutDeviceIntSize ScrollbarDrawingWin::GetMinimumWidgetSize(
    nsPresContext* aPresContext, StyleAppearance aAppearance,
    nsIFrame* aFrame) {
  MOZ_ASSERT(nsNativeTheme::IsWidgetScrollbarPart(aAppearance));

  switch (aAppearance) {
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonDown:
    case StyleAppearance::ScrollbarbuttonLeft:
    case StyleAppearance::ScrollbarbuttonRight:
      // For scrollbar-width:thin, we don't display the buttons.
      if (IsScrollbarWidthThin(aFrame)) {
        return LayoutDeviceIntSize{};
      }
      [[fallthrough]];
    case StyleAppearance::ScrollbarthumbVertical:
    case StyleAppearance::ScrollbarthumbHorizontal: {
      auto* style = nsLayoutUtils::StyleForScrollbar(aFrame);
      auto width = style->StyleUIReset()->mScrollbarWidth;
      auto sizes = GetScrollbarSizes(aPresContext, width, Overlay::No);
      // TODO: for short scrollbars it could be nice if the thumb could shrink
      // under this size.
      const bool isHorizontal =
          aAppearance == StyleAppearance::ScrollbarthumbHorizontal ||
          aAppearance == StyleAppearance::ScrollbarbuttonLeft ||
          aAppearance == StyleAppearance::ScrollbarbuttonRight;
      const auto size = isHorizontal ? sizes.mHorizontal : sizes.mVertical;
      return LayoutDeviceIntSize{size, size};
    }
    default:
      return LayoutDeviceIntSize{};
  }
}

Maybe<nsITheme::Transparency> ScrollbarDrawingWin::GetScrollbarPartTransparency(
    nsIFrame* aFrame, StyleAppearance aAppearance) {
  if (nsNativeTheme::IsWidgetScrollbarPart(aAppearance)) {
    if (ComputedStyle* style = GetCustomScrollbarStyle(aFrame)) {
      auto* ui = style->StyleUI();
      if (ui->mScrollbarColor.IsAuto() ||
          ui->mScrollbarColor.AsColors().track.MaybeTransparent()) {
        return Some(nsITheme::eTransparent);
      }
      // These widgets may be thinner than the track, so we need to return
      // transparent for them to make the track visible.
      switch (aAppearance) {
        case StyleAppearance::ScrollbarthumbHorizontal:
        case StyleAppearance::ScrollbarthumbVertical:
        case StyleAppearance::ScrollbarbuttonUp:
        case StyleAppearance::ScrollbarbuttonDown:
        case StyleAppearance::ScrollbarbuttonLeft:
        case StyleAppearance::ScrollbarbuttonRight:
          return Some(nsITheme::eTransparent);
        default:
          break;
      }
    }
  }

  switch (aAppearance) {
    case StyleAppearance::ScrollbarHorizontal:
    case StyleAppearance::ScrollbarVertical:
    case StyleAppearance::Scrollcorner:
    case StyleAppearance::Statusbar:
      // Knowing that scrollbars and statusbars are opaque improves
      // performance, because we create layers for them. This better be
      // true across all Windows themes! If it's not true, we should
      // paint an opaque background for them to make it true!
      return Some(nsITheme::eOpaque);
    default:
      break;
  }

  return Nothing();
}

// Returns the style for custom scrollbar if the scrollbar part frame should
// use the custom drawing path, nullptr otherwise.
//
// Optionally the caller can pass a pointer to aDarkScrollbar for whether
// custom scrollbar may be drawn due to dark background.
/*static*/
ComputedStyle* ScrollbarDrawingWin::GetCustomScrollbarStyle(
    nsIFrame* aFrame, bool* aDarkScrollbar) {
  ComputedStyle* style = nsLayoutUtils::StyleForScrollbar(aFrame);
  if (style->StyleUI()->HasCustomScrollbars()) {
    return style;
  }
  bool useDarkScrollbar = !StaticPrefs::widget_disable_dark_scrollbar() &&
                          nsNativeTheme::IsDarkBackground(aFrame);
  if (useDarkScrollbar || IsScrollbarWidthThin(*style)) {
    if (aDarkScrollbar) {
      *aDarkScrollbar = useDarkScrollbar;
    }
    return style;
  }
  return nullptr;
}

template <typename PaintBackendData>
bool ScrollbarDrawingWin::DoPaintScrollbarThumb(
    PaintBackendData& aPaintData, const LayoutDeviceRect& aRect,
    bool aHorizontal, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  sRGBColor thumbColor = ComputeScrollbarThumbColor(
      aFrame, aStyle, aElementState, aDocumentState, aColors);
  ThemeDrawing::FillRect(aPaintData, aRect, thumbColor);
  return true;
}

bool ScrollbarDrawingWin::PaintScrollbarThumb(
    DrawTarget& aDrawTarget, const LayoutDeviceRect& aRect, bool aHorizontal,
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  return DoPaintScrollbarThumb(aDrawTarget, aRect, aHorizontal, aFrame, aStyle,
                               aElementState, aDocumentState, aColors,
                               aDpiRatio);
}

bool ScrollbarDrawingWin::PaintScrollbarThumb(
    WebRenderBackendData& aWrData, const LayoutDeviceRect& aRect,
    bool aHorizontal, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  return DoPaintScrollbarThumb(aWrData, aRect, aHorizontal, aFrame, aStyle,
                               aElementState, aDocumentState, aColors,
                               aDpiRatio);
}

void ScrollbarDrawingWin::RecomputeScrollbarParams() {
  uint32_t defaultSize = 17;
  uint32_t overrideSize =
      StaticPrefs::widget_non_native_theme_scrollbar_size_override();
  if (overrideSize > 0) {
    defaultSize = overrideSize;
  }
  mHorizontalScrollbarHeight = mVerticalScrollbarWidth = defaultSize;

  if (StaticPrefs::widget_non_native_theme_win_scrollbar_use_system_size()) {
    mHorizontalScrollbarHeight = LookAndFeel::GetInt(
        LookAndFeel::IntID::SystemHorizontalScrollbarHeight, defaultSize);
    mVerticalScrollbarWidth = LookAndFeel::GetInt(
        LookAndFeel::IntID::SystemVerticalScrollbarWidth, defaultSize);
  }
}
