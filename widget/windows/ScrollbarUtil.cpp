/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollbarUtil.h"

#include "mozilla/Maybe.h"
#include "mozilla/RelativeLuminanceUtils.h"
#include "mozilla/StaticPrefs_widget.h"
#include "nsLayoutUtils.h"
#include "nsNativeTheme.h"
#include "nsNativeBasicTheme.h"

using mozilla::ComputedStyle;
using mozilla::EventStates;
using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::RelativeLuminanceUtils;
using mozilla::Some;
using mozilla::StyleAppearance;
using mozilla::StyleScrollbarWidth;

namespace StaticPrefs = mozilla::StaticPrefs;

/*static*/
bool ScrollbarUtil::IsScrollbarWidthThin(ComputedStyle* aStyle) {
  auto scrollbarWidth = aStyle->StyleUIReset()->mScrollbarWidth;
  return scrollbarWidth == StyleScrollbarWidth::Thin;
}

/*static*/
bool ScrollbarUtil::IsScrollbarWidthThin(nsIFrame* aFrame) {
  ComputedStyle* style = nsLayoutUtils::StyleForScrollbar(aFrame);
  return IsScrollbarWidthThin(style);
}

/*static*/
ComputedStyle* ScrollbarUtil::GetCustomScrollbarStyle(nsIFrame* aFrame,
                                                      bool* aDarkScrollbar) {
  ComputedStyle* style = nsLayoutUtils::StyleForScrollbar(aFrame);
  if (style->StyleUI()->HasCustomScrollbars()) {
    return style;
  }
  bool useDarkScrollbar = !StaticPrefs::widget_disable_dark_scrollbar() &&
                          nsNativeTheme::IsDarkBackground(aFrame);
  if (useDarkScrollbar || IsScrollbarWidthThin(style)) {
    if (aDarkScrollbar) {
      *aDarkScrollbar = useDarkScrollbar;
    }
    return style;
  }
  return nullptr;
}

/*static*/
nscolor ScrollbarUtil::GetScrollbarButtonColor(nscolor aTrackColor,
                                               EventStates aStates) {
  // See numbers in GetScrollbarArrowColor.
  // This function is written based on ratios between values listed there.

  bool isActive = aStates.HasState(NS_EVENT_STATE_ACTIVE);
  bool isHover = aStates.HasState(NS_EVENT_STATE_HOVER);
  if (!isActive && !isHover) {
    return aTrackColor;
  }
  float luminance = RelativeLuminanceUtils::Compute(aTrackColor);
  if (isActive) {
    if (luminance >= 0.18f) {
      luminance *= 0.134f;
    } else {
      luminance /= 0.134f;
      luminance = std::min(luminance, 1.0f);
    }
  } else {
    if (luminance >= 0.18f) {
      luminance *= 0.805f;
    } else {
      luminance /= 0.805f;
    }
  }
  return RelativeLuminanceUtils::Adjust(aTrackColor, luminance);
}

/*static*/
nscolor ScrollbarUtil::GetScrollbarArrowColor(nscolor aButtonColor) {
  // In Windows 10 scrollbar, there are several gray colors used:
  //
  // State  | Background (lum) | Arrow   | Contrast
  // -------+------------------+---------+---------
  // Normal | Gray 240 (87.1%) | Gray 96 |     5.5
  // Hover  | Gray 218 (70.1%) | Black   |    15.0
  // Active | Gray 96  (11.7%) | White   |     6.3
  //
  // Contrast value is computed based on the definition in
  // https://www.w3.org/TR/WCAG20/#contrast-ratiodef
  //
  // This function is written based on these values.

  float luminance = RelativeLuminanceUtils::Compute(aButtonColor);
  // Color with luminance larger than 0.72 has contrast ratio over 4.6
  // to color with luminance of gray 96, so this value is chosen for
  // this range. It is the luminance of gray 221.
  if (luminance >= 0.72) {
    // ComputeRelativeLuminanceFromComponents(96). That function cannot
    // be constexpr because of std::pow.
    const float GRAY96_LUMINANCE = 0.117f;
    return RelativeLuminanceUtils::Adjust(aButtonColor, GRAY96_LUMINANCE);
  }
  // The contrast ratio of a color to black equals that to white when its
  // luminance is around 0.18, with a contrast ratio ~4.6 to both sides,
  // thus the value below. It's the lumanince of gray 118.
  if (luminance >= 0.18) {
    return NS_RGBA(0, 0, 0, NS_GET_A(aButtonColor));
  }
  return NS_RGBA(255, 255, 255, NS_GET_A(aButtonColor));
}

/*static*/
nscolor ScrollbarUtil::GetScrollbarTrackColor(nsIFrame* aFrame) {
  bool darkScrollbar = false;
  ComputedStyle* style = GetCustomScrollbarStyle(aFrame, &darkScrollbar);
  if (style) {
    const nsStyleUI* ui = style->StyleUI();
    auto* customColors = ui->mScrollbarColor.IsAuto()
                             ? nullptr
                             : &ui->mScrollbarColor.AsColors();
    if (customColors) {
      return customColors->track.CalcColor(*style);
    }
  }
  return darkScrollbar ? NS_RGBA(20, 20, 25, 77) : NS_RGB(240, 240, 240);
}

/*static*/
nscolor ScrollbarUtil::GetScrollbarThumbColor(nsIFrame* aFrame,
                                              EventStates aEventStates) {
  bool darkScrollbar = false;
  ComputedStyle* style = GetCustomScrollbarStyle(aFrame, &darkScrollbar);
  nscolor color =
      darkScrollbar ? NS_RGBA(249, 249, 250, 102) : NS_RGB(205, 205, 205);
  if (style) {
    const nsStyleUI* ui = style->StyleUI();
    auto* customColors = ui->mScrollbarColor.IsAuto()
                             ? nullptr
                             : &ui->mScrollbarColor.AsColors();
    if (customColors) {
      color = customColors->thumb.CalcColor(*style);
    }
  }
  return nsNativeBasicTheme::AdjustUnthemedScrollbarThumbColor(color,
                                                               aEventStates);
}

/*static*/
Maybe<nsITheme::Transparency> ScrollbarUtil::GetScrollbarPartTransparency(
    nsIFrame* aFrame, StyleAppearance aAppearance) {
  if (nsNativeTheme::IsWidgetScrollbarPart(aAppearance)) {
    if (ComputedStyle* style = ScrollbarUtil::GetCustomScrollbarStyle(aFrame)) {
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
