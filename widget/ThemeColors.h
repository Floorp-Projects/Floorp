/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_ThemeColors_h
#define mozilla_widget_ThemeColors_h

#include "mozilla/dom/Document.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/LookAndFeel.h"
#include "nsIFrame.h"

namespace mozilla::widget {

static constexpr gfx::sRGBColor sDefaultAccent(
    gfx::sRGBColor::UnusualFromARGB(0xff0060df));  // Luminance: 13.69346%
static constexpr gfx::sRGBColor sDefaultAccentForeground(
    gfx::sRGBColor::OpaqueWhite());

class ThemeAccentColor {
 protected:
  using sRGBColor = mozilla::gfx::sRGBColor;
  using ComputedStyle = mozilla::ComputedStyle;

  Maybe<nscolor> mAccentColor;

 public:
  explicit ThemeAccentColor(const ComputedStyle& aStyle);
  virtual ~ThemeAccentColor() = default;

  sRGBColor Get() const;
  sRGBColor GetForeground() const;
  sRGBColor GetLight() const;
  sRGBColor GetDark() const;
  sRGBColor GetDarker() const;
};

// Widget color information associated to a particular frame.
class ThemeColors {
 protected:
  using Document = mozilla::dom::Document;
  using sRGBColor = mozilla::gfx::sRGBColor;
  using LookAndFeel = mozilla::LookAndFeel;
  using StyleSystemColor = mozilla::StyleSystemColor;
  using AccentColor = ThemeAccentColor;

  const AccentColor mAccentColor;
  const Document& mDoc;
  const bool mHighContrast;
  const LookAndFeel::ColorScheme mColorScheme;

 public:
  explicit ThemeColors(const nsIFrame* aFrame)
      : mAccentColor(*aFrame->Style()),
        mDoc(*aFrame->PresContext()->Document()),
        mHighContrast(ShouldBeHighContrast(*aFrame->PresContext())),
        mColorScheme(LookAndFeel::ColorSchemeForFrame(aFrame)) {}
  virtual ~ThemeColors() = default;

  [[nodiscard]] static float ScaleLuminanceBy(float aLuminance, float aFactor) {
    return aLuminance >= 0.18f ? aLuminance * aFactor : aLuminance / aFactor;
  }

  const AccentColor& Accent() const { return mAccentColor; }
  bool HighContrast() const { return mHighContrast; }
  bool IsDark() const { return mColorScheme == LookAndFeel::ColorScheme::Dark; }

  nscolor SystemNs(StyleSystemColor aColor) const {
    return LookAndFeel::Color(aColor, mColorScheme,
                              LookAndFeel::ShouldUseStandins(mDoc, aColor));
  }

  sRGBColor System(StyleSystemColor aColor) const {
    return sRGBColor::FromABGR(SystemNs(aColor));
  }

  template <typename Compute>
  sRGBColor SystemOrElse(StyleSystemColor aColor, Compute aCompute) const {
    if (auto color = LookAndFeel::GetColor(
            aColor, mColorScheme,
            LookAndFeel::ShouldUseStandins(mDoc, aColor))) {
      return sRGBColor::FromABGR(*color);
    }
    return aCompute();
  }

  std::pair<sRGBColor, sRGBColor> SystemPair(StyleSystemColor aFirst,
                                             StyleSystemColor aSecond) const {
    return std::make_pair(System(aFirst), System(aSecond));
  }

  // Whether we should use system colors (for high contrast mode).
  static bool ShouldBeHighContrast(const nsPresContext&);

  static void RecomputeAccentColors();

  static nscolor ComputeCustomAccentForeground(nscolor aColor);

  static nscolor AdjustUnthemedScrollbarThumbColor(nscolor aFaceColor,
                                                   EventStates aStates);
};

}  // namespace mozilla::widget

#endif
