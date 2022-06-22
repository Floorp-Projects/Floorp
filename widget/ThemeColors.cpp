/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThemeColors.h"

#include "mozilla/RelativeLuminanceUtils.h"
#include "mozilla/StaticPrefs_widget.h"
#include "ThemeDrawing.h"
#include "nsNativeTheme.h"

using namespace mozilla::gfx;

namespace mozilla::widget {

struct ColorPalette {
  ColorPalette(nscolor aAccent, nscolor aForeground);

  constexpr ColorPalette(sRGBColor aAccent, sRGBColor aForeground,
                         sRGBColor aLight, sRGBColor aDark, sRGBColor aDarker)
      : mAccent(aAccent),
        mForeground(aForeground),
        mAccentLight(aLight),
        mAccentDark(aDark),
        mAccentDarker(aDarker) {}

  constexpr static ColorPalette Default() {
    return ColorPalette(
        sDefaultAccent, sDefaultAccentForeground,
        sRGBColor::UnusualFromARGB(0x4d008deb),  // Luminance: 25.04791%
        sRGBColor::UnusualFromARGB(0xff0250bb),  // Luminance: 9.33808%
        sRGBColor::UnusualFromARGB(0xff054096)   // Luminance: 5.90106%
    );
  }

  // Ensure accent color is opaque by blending with white. This serves two
  // purposes: On one hand, it avoids surprises if we overdraw. On the other, it
  // makes our math below make more sense, as we want to match the browser
  // style, which has an opaque accent color.
  static nscolor EnsureOpaque(nscolor aAccent) {
    if (NS_GET_A(aAccent) != 0xff) {
      return NS_ComposeColors(NS_RGB(0xff, 0xff, 0xff), aAccent);
    }
    return aAccent;
  }

  static nscolor GetLight(nscolor aAccent) {
    // The luminance from the light color divided by the one of the accent color
    // in the default palette.
    constexpr float kLightLuminanceScale = 25.048f / 13.693f;
    const float lightLuminanceAdjust = ThemeColors::ScaleLuminanceBy(
        RelativeLuminanceUtils::Compute(aAccent), kLightLuminanceScale);
    nscolor lightColor =
        RelativeLuminanceUtils::Adjust(aAccent, lightLuminanceAdjust);
    return NS_RGBA(NS_GET_R(lightColor), NS_GET_G(lightColor),
                   NS_GET_B(lightColor), 0x4d);
  }

  static nscolor GetDark(nscolor aAccent) {
    // Same deal as above (but without the alpha).
    constexpr float kDarkLuminanceScale = 9.338f / 13.693f;
    const float darkLuminanceAdjust = ThemeColors::ScaleLuminanceBy(
        RelativeLuminanceUtils::Compute(aAccent), kDarkLuminanceScale);
    return RelativeLuminanceUtils::Adjust(aAccent, darkLuminanceAdjust);
  }

  static nscolor GetDarker(nscolor aAccent) {
    // Same deal as above.
    constexpr float kDarkerLuminanceScale = 5.901f / 13.693f;
    const float darkerLuminanceAdjust = ThemeColors::ScaleLuminanceBy(
        RelativeLuminanceUtils::Compute(aAccent), kDarkerLuminanceScale);
    return RelativeLuminanceUtils::Adjust(aAccent, darkerLuminanceAdjust);
  }

  sRGBColor mAccent;
  sRGBColor mForeground;

  // Note that depending on the exact accent color, lighter/darker might really
  // be inverted.
  sRGBColor mAccentLight;
  sRGBColor mAccentDark;
  sRGBColor mAccentDarker;
};

static nscolor ThemedAccentColor(bool aBackground, ColorScheme aScheme) {
  MOZ_ASSERT(StaticPrefs::widget_non_native_theme_use_theme_accent());
  return ColorPalette::EnsureOpaque(
      LookAndFeel::Color(aBackground ? LookAndFeel::ColorID::Accentcolor
                                     : LookAndFeel::ColorID::Accentcolortext,
                         aScheme, LookAndFeel::UseStandins::No));
}

static ColorPalette sDefaultLightPalette = ColorPalette::Default();
static ColorPalette sDefaultDarkPalette = ColorPalette::Default();

ColorPalette::ColorPalette(nscolor aAccent, nscolor aForeground) {
  mAccent = sRGBColor::FromABGR(aAccent);
  mForeground = sRGBColor::FromABGR(aForeground);
  mAccentLight = sRGBColor::FromABGR(GetLight(aAccent));
  mAccentDark = sRGBColor::FromABGR(GetDark(aAccent));
  mAccentDarker = sRGBColor::FromABGR(GetDarker(aAccent));
}

ThemeAccentColor::ThemeAccentColor(const ComputedStyle& aStyle,
                                   ColorScheme aScheme) {
  const auto& color = aStyle.StyleUI()->mAccentColor;
  if (color.IsColor()) {
    mAccentColor.emplace(
        ColorPalette::EnsureOpaque(color.AsColor().CalcColor(aStyle)));
  } else {
    MOZ_ASSERT(color.IsAuto());
    mDefaultPalette = aScheme == ColorScheme::Light ? &sDefaultLightPalette
                                                    : &sDefaultDarkPalette;
  }
}

sRGBColor ThemeAccentColor::Get() const {
  if (!mAccentColor) {
    return mDefaultPalette->mAccent;
  }
  return sRGBColor::FromABGR(*mAccentColor);
}

sRGBColor ThemeAccentColor::GetForeground() const {
  if (!mAccentColor) {
    return mDefaultPalette->mForeground;
  }
  return sRGBColor::FromABGR(
      ThemeColors::ComputeCustomAccentForeground(*mAccentColor));
}

sRGBColor ThemeAccentColor::GetLight() const {
  if (!mAccentColor) {
    return mDefaultPalette->mAccentLight;
  }
  return sRGBColor::FromABGR(ColorPalette::GetLight(*mAccentColor));
}

sRGBColor ThemeAccentColor::GetDark() const {
  if (!mAccentColor) {
    return mDefaultPalette->mAccentDark;
  }
  return sRGBColor::FromABGR(ColorPalette::GetDark(*mAccentColor));
}

sRGBColor ThemeAccentColor::GetDarker() const {
  if (!mAccentColor) {
    return mDefaultPalette->mAccentDarker;
  }
  return sRGBColor::FromABGR(ColorPalette::GetDarker(*mAccentColor));
}

bool ThemeColors::ShouldBeHighContrast(const nsPresContext& aPc) {
  // We make sure that we're drawing backgrounds, since otherwise layout will
  // darken our used text colors etc anyways, and that can cause contrast issues
  // with dark high-contrast themes.
  return aPc.GetBackgroundColorDraw() &&
         PreferenceSheet::PrefsFor(*aPc.Document())
             .NonNativeThemeShouldBeHighContrast();
}

ColorScheme ThemeColors::ColorSchemeForWidget(const nsIFrame* aFrame,
                                              StyleAppearance aAppearance,
                                              bool aHighContrast) {
  if (!nsNativeTheme::IsWidgetScrollbarPart(aAppearance)) {
    return LookAndFeel::ColorSchemeForFrame(aFrame);
  }
  // Scrollbars are a bit tricky. Their used color-scheme depends on whether the
  // background they are on is light or dark.
  //
  // TODO(emilio): This heuristic effectively predates the color-scheme CSS
  // property. Perhaps we should check whether the style or the document set
  // `color-scheme` to something that isn't `normal`, and if so go through the
  // code-path above.
  if (aHighContrast) {
    return ColorScheme::Light;
  }
  if (StaticPrefs::widget_disable_dark_scrollbar()) {
    return ColorScheme::Light;
  }
  return nsNativeTheme::IsDarkBackground(const_cast<nsIFrame*>(aFrame))
             ? ColorScheme::Dark
             : ColorScheme::Light;
}

/*static*/
void ThemeColors::RecomputeAccentColors() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!StaticPrefs::widget_non_native_theme_use_theme_accent()) {
    sDefaultLightPalette = sDefaultDarkPalette = ColorPalette::Default();
    return;
  }

  sDefaultLightPalette =
      ColorPalette(ThemedAccentColor(true, ColorScheme::Light),
                   ThemedAccentColor(false, ColorScheme::Light));

  sDefaultDarkPalette =
      ColorPalette(ThemedAccentColor(true, ColorScheme::Dark),
                   ThemedAccentColor(false, ColorScheme::Dark));
}

/*static*/
nscolor ThemeColors::ComputeCustomAccentForeground(nscolor aColor) {
  // Contrast ratio is defined in
  // https://www.w3.org/TR/WCAG20/#contrast-ratiodef as:
  //
  //   (L1 + 0.05) / (L2 + 0.05)
  //
  // Where L1 is the lighter color, and L2 is the darker one. So we determine
  // whether we're dark or light and resolve the equation for the target ratio.
  //
  // So when lightening:
  //
  //   L1 = k * (L2 + 0.05) - 0.05
  //
  // And when darkening:
  //
  //   L2 = (L1 + 0.05) / k - 0.05
  //
  const float luminance = RelativeLuminanceUtils::Compute(aColor);

  // We generally prefer white unless we can't because the color is really light
  // and we can't provide reasonable contrast.
  const float ratioWithWhite = 1.05f / (luminance + 0.05f);
  const bool canBeWhite =
      ratioWithWhite >=
      StaticPrefs::layout_css_accent_color_min_contrast_ratio();
  if (canBeWhite) {
    return NS_RGB(0xff, 0xff, 0xff);
  }
  const float targetRatio =
      StaticPrefs::layout_css_accent_color_darkening_target_contrast_ratio();
  const float targetLuminance = (luminance + 0.05f) / targetRatio - 0.05f;
  return RelativeLuminanceUtils::Adjust(aColor, targetLuminance);
}

nscolor ThemeColors::AdjustUnthemedScrollbarThumbColor(nscolor aFaceColor,
                                                       ElementState aStates) {
  // In Windows 10, scrollbar thumb has the following colors:
  //
  // State  | Color    | Luminance
  // -------+----------+----------
  // Normal | Gray 205 |     61.0%
  // Hover  | Gray 166 |     38.1%
  // Active | Gray 96  |     11.7%
  //
  // This function is written based on the ratios between the values.
  bool isActive = aStates.HasState(ElementState::ACTIVE);
  bool isHover = aStates.HasState(ElementState::HOVER);
  if (!isActive && !isHover) {
    return aFaceColor;
  }
  float luminance = RelativeLuminanceUtils::Compute(aFaceColor);
  if (isActive) {
    // 11.7 / 61.0
    luminance = ScaleLuminanceBy(luminance, 0.192f);
  } else {
    // 38.1 / 61.0
    luminance = ScaleLuminanceBy(luminance, 0.625f);
  }
  return RelativeLuminanceUtils::Adjust(aFaceColor, luminance);
}

}  // namespace mozilla::widget
