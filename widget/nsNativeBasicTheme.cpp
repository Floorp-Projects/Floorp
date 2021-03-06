/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeBasicTheme.h"

#include "gfxBlur.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/dom/Document.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/gfx/Filters.h"
#include "mozilla/RelativeLuminanceUtils.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "nsCSSColorUtils.h"
#include "nsCSSRendering.h"
#include "nsLayoutUtils.h"
#include "PathHelpers.h"

#include "nsDeviceContext.h"

#include "nsColorControlFrame.h"
#include "nsDateTimeControlFrame.h"
#include "nsMeterFrame.h"
#include "nsProgressFrame.h"
#include "nsRangeFrame.h"
#include "mozilla/dom/HTMLMeterElement.h"
#include "mozilla/dom/HTMLProgressElement.h"

using namespace mozilla;
using namespace mozilla::widget;
using namespace mozilla::gfx;

NS_IMPL_ISUPPORTS_INHERITED(nsNativeBasicTheme, nsNativeTheme, nsITheme)

namespace {

static constexpr sRGBColor sTransparent = sRGBColor::White(0.0);

// This pushes and pops a clip rect to the draw target.
//
// This is done to reduce fuzz in places where we may have antialiasing,
// because skia is not clip-invariant: given different clips, it does not
// guarantee the same result, even if the painted content doesn't intersect
// the clips.
//
// This is a bit sad, overall, but...
struct MOZ_RAII AutoClipRect {
  AutoClipRect(DrawTarget& aDt, const LayoutDeviceRect& aRect) : mDt(aDt) {
    mDt.PushClipRect(aRect.ToUnknownRect());
  }

  ~AutoClipRect() { mDt.PopClip(); }

 private:
  DrawTarget& mDt;
};

static LayoutDeviceIntCoord SnapBorderWidth(
    CSSCoord aCssWidth, nsNativeBasicTheme::DPIRatio aDpiRatio) {
  if (aCssWidth == 0.0f) {
    return 0;
  }
  return std::max(LayoutDeviceIntCoord(1), (aCssWidth * aDpiRatio).Truncated());
}

[[nodiscard]] static float ScaleLuminanceBy(float aLuminance, float aFactor) {
  return aLuminance >= 0.18f ? aLuminance * aFactor : aLuminance / aFactor;
}

static nscolor ThemedAccentColor(bool aBackground) {
  MOZ_ASSERT(StaticPrefs::widget_non_native_theme_use_theme_accent());
  nscolor color = LookAndFeel::GetColor(
      aBackground ? LookAndFeel::ColorID::MozAccentColor
                  : LookAndFeel::ColorID::MozAccentColorForeground);
  if (NS_GET_A(color) != 0xff) {
    // Blend with white, ensuring the color is opaque to avoid surprises if we
    // overdraw.
    color = NS_ComposeColors(NS_RGB(0xff, 0xff, 0xff), color);
  }
  return color;
}

}  // namespace

sRGBColor nsNativeBasicTheme::sAccentColor = sRGBColor::OpaqueWhite();
sRGBColor nsNativeBasicTheme::sAccentColorForeground = sRGBColor::OpaqueWhite();
sRGBColor nsNativeBasicTheme::sAccentColorLight = sRGBColor::OpaqueWhite();
sRGBColor nsNativeBasicTheme::sAccentColorDark = sRGBColor::OpaqueWhite();
sRGBColor nsNativeBasicTheme::sAccentColorDarker = sRGBColor::OpaqueWhite();

void nsNativeBasicTheme::Init() {
  Preferences::RegisterCallbackAndCall(PrefChangedCallback,
                                       "widget.non-native.use-theme-accent");
}

void nsNativeBasicTheme::Shutdown() {
  Preferences::UnregisterCallback(PrefChangedCallback,
                                  "widget.non-native.use-theme-accent");
}

void nsNativeBasicTheme::LookAndFeelChanged() { RecomputeAccentColors(); }

void nsNativeBasicTheme::RecomputeAccentColors() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!StaticPrefs::widget_non_native_theme_use_theme_accent()) {
    sAccentColorForeground = sColorWhite;
    sAccentColor =
        sRGBColor::UnusualFromARGB(0xff0060df);  // Luminance: 13.69346%
    sAccentColorLight =
        sRGBColor::UnusualFromARGB(0x4d008deb);  // Luminance: 25.04791%
    sAccentColorDark =
        sRGBColor::UnusualFromARGB(0xff0250bb);  // Luminance: 9.33808%
    sAccentColorDarker =
        sRGBColor::UnusualFromARGB(0xff054096);  // Luminance: 5.90106%
    return;
  }

  sAccentColorForeground = sRGBColor::FromABGR(ThemedAccentColor(false));
  const nscolor accent = ThemedAccentColor(true);
  const float luminance = RelativeLuminanceUtils::Compute(accent);

  constexpr float kLightLuminanceScale = 25.048f / 13.693f;
  constexpr float kDarkLuminanceScale = 9.338f / 13.693f;
  constexpr float kDarkerLuminanceScale = 5.901f / 13.693f;

  const float lightLuminanceAdjust =
      ScaleLuminanceBy(luminance, kLightLuminanceScale);
  const float darkLuminanceAdjust =
      ScaleLuminanceBy(luminance, kDarkLuminanceScale);
  const float darkerLuminanceAdjust =
      ScaleLuminanceBy(luminance, kDarkerLuminanceScale);

  sAccentColor = sRGBColor::FromABGR(accent);

  {
    nscolor lightColor =
        RelativeLuminanceUtils::Adjust(accent, lightLuminanceAdjust);
    lightColor = NS_RGBA(NS_GET_R(lightColor), NS_GET_G(lightColor),
                         NS_GET_B(lightColor), 0x4d);
    sAccentColorLight = sRGBColor::FromABGR(lightColor);
  }

  sAccentColorDark = sRGBColor::FromABGR(
      RelativeLuminanceUtils::Adjust(accent, darkLuminanceAdjust));
  sAccentColorDarker = sRGBColor::FromABGR(
      RelativeLuminanceUtils::Adjust(accent, darkerLuminanceAdjust));
}

static bool IsScrollbarWidthThin(nsIFrame* aFrame) {
  ComputedStyle* style = nsLayoutUtils::StyleForScrollbar(aFrame);
  auto scrollbarWidth = style->StyleUIReset()->mScrollbarWidth;
  return scrollbarWidth == StyleScrollbarWidth::Thin;
}

/* static */
auto nsNativeBasicTheme::GetDPIRatioForScrollbarPart(nsPresContext* aPc)
    -> DPIRatio {
  return DPIRatio(float(AppUnitsPerCSSPixel()) /
                  aPc->DeviceContext()->AppUnitsPerDevPixelAtUnitFullZoom());
}

/* static */
auto nsNativeBasicTheme::GetDPIRatio(nsPresContext* aPc,
                                     StyleAppearance aAppearance) -> DPIRatio {
  // Widgets react to zoom, except scrollbars.
  if (IsWidgetScrollbarPart(aAppearance)) {
    return GetDPIRatioForScrollbarPart(aPc);
  }
  return DPIRatio(float(AppUnitsPerCSSPixel()) / aPc->AppUnitsPerDevPixel());
}

/* static */
auto nsNativeBasicTheme::GetDPIRatio(nsIFrame* aFrame,
                                     StyleAppearance aAppearance) -> DPIRatio {
  return GetDPIRatio(aFrame->PresContext(), aAppearance);
}

/* static */
bool nsNativeBasicTheme::IsDateTimeResetButton(nsIFrame* aFrame) {
  if (!aFrame) {
    return false;
  }

  nsIFrame* parent = aFrame->GetParent();
  if (parent && (parent = parent->GetParent()) &&
      (parent = parent->GetParent())) {
    nsDateTimeControlFrame* dateTimeFrame = do_QueryFrame(parent);
    if (dateTimeFrame) {
      return true;
    }
  }
  return false;
}

/* static */
bool nsNativeBasicTheme::IsColorPickerButton(nsIFrame* aFrame) {
  nsColorControlFrame* colorPickerButton = do_QueryFrame(aFrame);
  return colorPickerButton;
}

/* static */
LayoutDeviceRect nsNativeBasicTheme::FixAspectRatio(
    const LayoutDeviceRect& aRect) {
  // Checkbox and radio need to preserve aspect-ratio for compat.
  LayoutDeviceRect rect(aRect);
  if (rect.width == rect.height) {
    return rect;
  }

  if (rect.width > rect.height) {
    auto diff = rect.width - rect.height;
    rect.width = rect.height;
    rect.x += diff / 2;
  } else {
    auto diff = rect.height - rect.width;
    rect.height = rect.width;
    rect.y += diff / 2;
  }

  return rect;
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicTheme::ComputeCheckboxColors(
    const EventStates& aState, StyleAppearance aAppearance) {
  MOZ_ASSERT(aAppearance == StyleAppearance::Checkbox ||
             aAppearance == StyleAppearance::Radio);

  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isPressed = !isDisabled && aState.HasAllStates(NS_EVENT_STATE_HOVER |
                                                      NS_EVENT_STATE_ACTIVE);
  bool isHovered = !isDisabled && aState.HasState(NS_EVENT_STATE_HOVER);
  bool isChecked = aState.HasState(NS_EVENT_STATE_CHECKED);
  bool isIndeterminate = aAppearance == StyleAppearance::Checkbox &&
                         aState.HasState(NS_EVENT_STATE_INDETERMINATE);

  sRGBColor backgroundColor = sColorWhite;
  sRGBColor borderColor = sColorGrey40;
  if (isDisabled) {
    if (isChecked || isIndeterminate) {
      backgroundColor = borderColor = sColorGrey40Alpha50;
    } else {
      backgroundColor = sColorWhiteAlpha50;
      borderColor = sColorGrey40Alpha50;
    }
  } else {
    if (isChecked || isIndeterminate) {
      const auto& color = isPressed   ? sAccentColorDarker
                          : isHovered ? sAccentColorDark
                                      : sAccentColor;
      backgroundColor = borderColor = color;
    } else if (isPressed) {
      backgroundColor = sColorGrey20;
      borderColor = sColorGrey60;
    } else if (isHovered) {
      backgroundColor = sColorWhite;
      borderColor = sColorGrey50;
    } else {
      backgroundColor = sColorWhite;
      borderColor = sColorGrey40;
    }
  }

  return std::make_pair(backgroundColor, borderColor);
}

sRGBColor nsNativeBasicTheme::ComputeCheckmarkColor(const EventStates& aState) {
  if (aState.HasState(NS_EVENT_STATE_DISABLED)) {
    return sColorWhiteAlpha50;
  }
  return sAccentColorForeground;
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicTheme::ComputeRadioCheckmarkColors(
    const EventStates& aState) {
  auto [unusedColor, checkColor] =
      ComputeCheckboxColors(aState, StyleAppearance::Radio);
  Unused << unusedColor;
  return std::make_pair(ComputeCheckmarkColor(aState), checkColor);
}

sRGBColor nsNativeBasicTheme::ComputeBorderColor(const EventStates& aState) {
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isActive =
      aState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE);
  bool isHovered = !isDisabled && aState.HasState(NS_EVENT_STATE_HOVER);
  bool isFocused = aState.HasState(NS_EVENT_STATE_FOCUSRING);
  if (isDisabled) {
    return sColorGrey40Alpha50;
  }
  if (isFocused) {
    // We draw the outline over the border for all controls that call into this,
    // so to prevent issues where the border shows underneath if it snaps in the
    // wrong direction, we use a transparent border. An alternative to this is
    // ensuring that we snap the offset in PaintRoundedFocusRect the same was a
    // we snap border widths, so that negative offsets are guaranteed to cover
    // the border. But this looks harder to mess up.
    return sTransparent;
  }
  if (isActive) {
    return sColorGrey60;
  }
  if (isHovered) {
    return sColorGrey50;
  }
  return sColorGrey40;
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicTheme::ComputeButtonColors(
    const EventStates& aState, nsIFrame* aFrame) {
  bool isActive =
      aState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE);
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isHovered = !isDisabled && aState.HasState(NS_EVENT_STATE_HOVER);

  const sRGBColor& backgroundColor = [&] {
    if (isDisabled) {
      return sColorGrey10Alpha50;
    }
    if (IsDateTimeResetButton(aFrame)) {
      return sColorWhite;
    }
    if (isActive) {
      return sColorGrey30;
    }
    if (isHovered) {
      return sColorGrey20;
    }
    return sColorGrey10;
  }();

  const sRGBColor borderColor = ComputeBorderColor(aState);
  return std::make_pair(backgroundColor, borderColor);
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicTheme::ComputeTextfieldColors(
    const EventStates& aState) {
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  const sRGBColor& backgroundColor =
      isDisabled ? sColorWhiteAlpha50 : sColorWhite;
  const sRGBColor borderColor = ComputeBorderColor(aState);

  return std::make_pair(backgroundColor, borderColor);
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicTheme::ComputeRangeProgressColors(
    const EventStates& aState) {
  bool isActive =
      aState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE);
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isHovered = !isDisabled && aState.HasState(NS_EVENT_STATE_HOVER);

  if (isDisabled) {
    return std::make_pair(sColorGrey40Alpha50, sColorGrey40Alpha50);
  }
  if (isActive || isHovered) {
    return std::make_pair(sAccentColorDark, sAccentColorDarker);
  }
  return std::make_pair(sAccentColor, sAccentColorDark);
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicTheme::ComputeRangeTrackColors(
    const EventStates& aState) {
  bool isActive =
      aState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE);
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isHovered = !isDisabled && aState.HasState(NS_EVENT_STATE_HOVER);

  if (isDisabled) {
    return std::make_pair(sColorGrey10Alpha50, sColorGrey40Alpha50);
  }
  if (isActive || isHovered) {
    return std::make_pair(sColorGrey20, sColorGrey50);
  }
  return std::make_pair(sColorGrey10, sColorGrey40);
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicTheme::ComputeRangeThumbColors(
    const EventStates& aState) {
  bool isActive =
      aState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE);
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isHovered = !isDisabled && aState.HasState(NS_EVENT_STATE_HOVER);

  const sRGBColor& backgroundColor = [&] {
    if (isDisabled) {
      return sColorGrey40;
    }
    if (isActive) {
      return sAccentColor;
    }
    if (isHovered) {
      return sColorGrey60;
    }
    return sColorGrey50;
  }();

  const sRGBColor borderColor = sColorWhite;

  return std::make_pair(backgroundColor, borderColor);
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicTheme::ComputeProgressColors() {
  return std::make_pair(sAccentColor, sAccentColorDark);
}

std::pair<sRGBColor, sRGBColor>
nsNativeBasicTheme::ComputeProgressTrackColors() {
  return std::make_pair(sColorGrey10, sColorGrey40);
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicTheme::ComputeMeterchunkColors(
    const EventStates& aMeterState) {
  sRGBColor borderColor = sColorMeterGreen20;
  sRGBColor chunkColor = sColorMeterGreen10;

  if (aMeterState.HasState(NS_EVENT_STATE_SUB_OPTIMUM)) {
    borderColor = sColorMeterYellow20;
    chunkColor = sColorMeterYellow10;
  } else if (aMeterState.HasState(NS_EVENT_STATE_SUB_SUB_OPTIMUM)) {
    borderColor = sColorMeterRed20;
    chunkColor = sColorMeterRed10;
  }

  return std::make_pair(chunkColor, borderColor);
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicTheme::ComputeMeterTrackColors() {
  return std::make_pair(sColorGrey10, sColorGrey40);
}

sRGBColor nsNativeBasicTheme::ComputeMenulistArrowButtonColor(
    const EventStates& aState) {
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  return isDisabled ? sColorGrey60Alpha50 : sColorGrey60;
}

std::array<sRGBColor, 3> nsNativeBasicTheme::ComputeFocusRectColors() {
  return {sAccentColor, sColorWhiteAlpha80, sAccentColorLight};
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicTheme::ComputeScrollbarColors(
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aDocumentState) {
  const nsStyleUI* ui = aStyle.StyleUI();
  nscolor color;
  if (ui->mScrollbarColor.IsColors()) {
    color = ui->mScrollbarColor.AsColors().track.CalcColor(aStyle);
  } else if (aDocumentState.HasAllStates(NS_DOCUMENT_STATE_WINDOW_INACTIVE)) {
    color = LookAndFeel::GetColor(LookAndFeel::ColorID::ThemedScrollbarInactive,
                                  sScrollbarColor.ToABGR());
  } else {
    color = LookAndFeel::GetColor(LookAndFeel::ColorID::ThemedScrollbar,
                                  sScrollbarColor.ToABGR());
  }
  return std::make_pair(gfx::sRGBColor::FromABGR(color), sScrollbarBorderColor);
}

nscolor nsNativeBasicTheme::AdjustUnthemedScrollbarThumbColor(
    nscolor aFaceColor, EventStates aStates) {
  // In Windows 10, scrollbar thumb has the following colors:
  //
  // State  | Color    | Luminance
  // -------+----------+----------
  // Normal | Gray 205 |     61.0%
  // Hover  | Gray 166 |     38.1%
  // Active | Gray 96  |     11.7%
  //
  // This function is written based on the ratios between the values.
  bool isActive = aStates.HasState(NS_EVENT_STATE_ACTIVE);
  bool isHover = aStates.HasState(NS_EVENT_STATE_HOVER);
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

sRGBColor nsNativeBasicTheme::ComputeScrollbarThumbColor(
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState) {
  const nsStyleUI* ui = aStyle.StyleUI();
  nscolor color;
  if (ui->mScrollbarColor.IsColors()) {
    color = AdjustUnthemedScrollbarThumbColor(
        ui->mScrollbarColor.AsColors().thumb.CalcColor(aStyle), aElementState);
  } else if (aDocumentState.HasAllStates(NS_DOCUMENT_STATE_WINDOW_INACTIVE)) {
    color = LookAndFeel::GetColor(
        LookAndFeel::ColorID::ThemedScrollbarThumbInactive,
        sScrollbarThumbColor.ToABGR());
  } else if (aElementState.HasAllStates(NS_EVENT_STATE_ACTIVE)) {
    color =
        LookAndFeel::GetColor(LookAndFeel::ColorID::ThemedScrollbarThumbActive,
                              sScrollbarThumbColorActive.ToABGR());
  } else if (aElementState.HasAllStates(NS_EVENT_STATE_HOVER)) {
    color =
        LookAndFeel::GetColor(LookAndFeel::ColorID::ThemedScrollbarThumbHover,
                              sScrollbarThumbColorHover.ToABGR());
  } else {
    color = LookAndFeel::GetColor(LookAndFeel::ColorID::ThemedScrollbarThumb,
                                  sScrollbarThumbColor.ToABGR());
  }
  return gfx::sRGBColor::FromABGR(color);
}

std::array<sRGBColor, 3> nsNativeBasicTheme::ComputeScrollbarButtonColors(
    nsIFrame* aFrame, StyleAppearance aAppearance, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState) {
  bool isActive = aElementState.HasState(NS_EVENT_STATE_ACTIVE);
  bool isHovered = aElementState.HasState(NS_EVENT_STATE_HOVER);

  bool hasCustomColor = aStyle.StyleUI()->mScrollbarColor.IsColors();
  sRGBColor buttonColor;
  if (hasCustomColor) {
    // When scrollbar-color is in use, use the thumb color for the button.
    buttonColor = ComputeScrollbarThumbColor(aFrame, aStyle, aElementState,
                                             aDocumentState);
  } else if (isActive) {
    buttonColor = sScrollbarButtonActiveColor;
  } else if (!hasCustomColor && isHovered) {
    buttonColor = sScrollbarButtonHoverColor;
  } else {
    buttonColor = sScrollbarColor;
  }

  sRGBColor arrowColor;
  if (hasCustomColor) {
    // When scrollbar-color is in use, derive the arrow color from the button
    // color.
    nscolor bg = buttonColor.ToABGR();
    bool darken = NS_GetLuminosity(bg) >= NS_MAX_LUMINOSITY / 2;
    if (isActive) {
      float c = darken ? 0.0f : 1.0f;
      arrowColor = sRGBColor(c, c, c);
    } else {
      uint8_t c = darken ? 0 : 255;
      arrowColor =
          sRGBColor::FromABGR(NS_ComposeColors(bg, NS_RGBA(c, c, c, 160)));
    }
  } else if (isActive) {
    arrowColor = sScrollbarArrowColorActive;
  } else if (isHovered) {
    arrowColor = sScrollbarArrowColorHover;
  } else {
    arrowColor = sScrollbarArrowColor;
  }

  return {buttonColor, arrowColor, sScrollbarBorderColor};
}

static const CSSCoord kInnerFocusOutlineWidth = 2.0f;

template <typename PaintBackendData>
void nsNativeBasicTheme::PaintRoundedFocusRect(PaintBackendData& aBackendData,
                                               const LayoutDeviceRect& aRect,
                                               DPIRatio aDpiRatio,
                                               CSSCoord aRadius,
                                               CSSCoord aOffset) {
  // NOTE(emilio): If the widths or offsets here change, make sure to tweak
  // the GetWidgetOverflow path for FocusOutline.
  auto [innerColor, middleColor, outerColor] = ComputeFocusRectColors();

  LayoutDeviceRect focusRect(aRect);

  // The focus rect is painted outside of the border area (aRect), see:
  //
  //   data:text/html,<div style="border: 1px solid; outline: 2px solid
  //   red">Foobar</div>
  //
  // But some controls might provide a negative offset to cover the border, if
  // necessary.
  CSSCoord strokeWidth = kInnerFocusOutlineWidth;
  auto strokeWidthDevPx =
      LayoutDeviceCoord(SnapBorderWidth(strokeWidth, aDpiRatio));
  CSSCoord strokeRadius = aRadius;
  focusRect.Inflate(aOffset * aDpiRatio + strokeWidthDevPx);

  PaintRoundedRectWithRadius(aBackendData, focusRect, sTransparent, innerColor,
                             strokeWidth, strokeRadius, aDpiRatio);

  strokeWidth = CSSCoord(1.0f);
  strokeWidthDevPx = LayoutDeviceCoord(SnapBorderWidth(strokeWidth, aDpiRatio));
  strokeRadius += strokeWidth;
  focusRect.Inflate(strokeWidthDevPx);

  PaintRoundedRectWithRadius(aBackendData, focusRect, sTransparent, middleColor,
                             strokeWidth, strokeRadius, aDpiRatio);

  strokeWidth = CSSCoord(2.0f);
  strokeWidthDevPx = LayoutDeviceCoord(SnapBorderWidth(strokeWidth, aDpiRatio));
  strokeRadius += strokeWidth;
  focusRect.Inflate(strokeWidthDevPx);

  PaintRoundedRectWithRadius(aBackendData, focusRect, sTransparent, outerColor,
                             strokeWidth, strokeRadius, aDpiRatio);
}

void nsNativeBasicTheme::PaintRoundedRectWithRadius(
    WebRenderBackendData& aWrData, const LayoutDeviceRect& aRect,
    const LayoutDeviceRect& aClipRect, const sRGBColor& aBackgroundColor,
    const sRGBColor& aBorderColor, CSSCoord aBorderWidth, CSSCoord aRadius,
    DPIRatio aDpiRatio) {
  const bool kBackfaceIsVisible = true;
  const LayoutDeviceCoord borderWidth(SnapBorderWidth(aBorderWidth, aDpiRatio));
  const LayoutDeviceCoord radius(aRadius * aDpiRatio);
  const wr::LayoutRect dest = wr::ToLayoutRect(aRect);
  const wr::LayoutRect clip = wr::ToLayoutRect(aClipRect);

  // Push the background.
  if (aBackgroundColor.a) {
    auto backgroundColor = wr::ToColorF(ToDeviceColor(aBackgroundColor));
    wr::LayoutRect backgroundRect = [&] {
      LayoutDeviceRect bg = aRect;
      bg.Deflate(borderWidth);
      return wr::ToLayoutRect(bg);
    }();
    if (!radius) {
      aWrData.mBuilder.PushRect(backgroundRect, clip, kBackfaceIsVisible,
                                backgroundColor);
    } else {
      // NOTE(emilio): This follows DisplayListBuilder::PushRoundedRect and
      // draws the rounded fill as an extra thick rounded border instead of a
      // rectangle that's clipped to a rounded clip. Refer to that method for a
      // justification. See bug 1694269.
      LayoutDeviceCoord backgroundRadius =
          std::max(0.0f, float(radius) - float(borderWidth));
      wr::BorderSide side = {backgroundColor, wr::BorderStyle::Solid};
      const wr::BorderSide sides[4] = {side, side, side, side};
      float h = backgroundRect.size.width * 0.6f;
      float v = backgroundRect.size.height * 0.6f;
      wr::LayoutSideOffsets widths = {v, h, v, h};
      wr::BorderRadius radii = {{backgroundRadius, backgroundRadius},
                                {backgroundRadius, backgroundRadius},
                                {backgroundRadius, backgroundRadius},
                                {backgroundRadius, backgroundRadius}};
      aWrData.mBuilder.PushBorder(backgroundRect, clip, kBackfaceIsVisible,
                                  widths, {sides, 4}, radii);
    }
  }

  if (borderWidth && aBorderColor.a) {
    // Push the border.
    const auto borderColor = ToDeviceColor(aBorderColor);
    const auto side = wr::ToBorderSide(borderColor, StyleBorderStyle::Solid);
    const wr::BorderSide sides[4] = {side, side, side, side};
    const LayoutDeviceSize sideRadius(radius, radius);
    const auto widths =
        wr::ToBorderWidths(borderWidth, borderWidth, borderWidth, borderWidth);
    const auto wrRadius =
        wr::ToBorderRadius(sideRadius, sideRadius, sideRadius, sideRadius);
    aWrData.mBuilder.PushBorder(dest, clip, kBackfaceIsVisible, widths,
                                {sides, 4}, wrRadius);
  }
}

void nsNativeBasicTheme::FillRect(DrawTarget& aDt,
                                  const LayoutDeviceRect& aRect,
                                  const sRGBColor& aColor) {
  aDt.FillRect(aRect.ToUnknownRect(), ColorPattern(ToDeviceColor(aColor)));
}

void nsNativeBasicTheme::FillRect(WebRenderBackendData& aWrData,
                                  const LayoutDeviceRect& aRect,
                                  const sRGBColor& aColor) {
  const bool kBackfaceIsVisible = true;
  auto dest = wr::ToLayoutRect(aRect);
  aWrData.mBuilder.PushRect(dest, dest, kBackfaceIsVisible,
                            wr::ToColorF(ToDeviceColor(aColor)));
}

void nsNativeBasicTheme::PaintRoundedRectWithRadius(
    DrawTarget& aDrawTarget, const LayoutDeviceRect& aRect,
    const LayoutDeviceRect& aClipRect, const sRGBColor& aBackgroundColor,
    const sRGBColor& aBorderColor, CSSCoord aBorderWidth, CSSCoord aRadius,
    DPIRatio aDpiRatio) {
  const LayoutDeviceCoord borderWidth(SnapBorderWidth(aBorderWidth, aDpiRatio));
  const bool needsClip = !(aRect == aClipRect);
  if (needsClip) {
    aDrawTarget.PushClipRect(aClipRect.ToUnknownRect());
  }

  LayoutDeviceRect rect(aRect);
  // Deflate the rect by half the border width, so that the middle of the
  // stroke fills exactly the area we want to fill and not more.
  rect.Deflate(borderWidth * 0.5f);

  LayoutDeviceCoord radius(aRadius * aDpiRatio);
  // Fix up the radius if it's too large with the rect we're going to paint.
  {
    LayoutDeviceCoord min = std::min(rect.width, rect.height);
    if (radius * 2.0f > min) {
      radius = min * 0.5f;
    }
  }

  Maybe<ColorPattern> backgroundPattern;
  if (aBackgroundColor.a) {
    backgroundPattern.emplace(ToDeviceColor(aBackgroundColor));
  }
  Maybe<ColorPattern> borderPattern;
  if (borderWidth && aBorderColor.a) {
    borderPattern.emplace(ToDeviceColor(aBorderColor));
  }

  if (borderPattern || backgroundPattern) {
    if (radius) {
      RectCornerRadii radii(radius, radius, radius, radius);
      RefPtr<Path> roundedRect =
          MakePathForRoundedRect(aDrawTarget, rect.ToUnknownRect(), radii);

      if (backgroundPattern) {
        aDrawTarget.Fill(roundedRect, *backgroundPattern);
      }
      if (borderPattern) {
        aDrawTarget.Stroke(roundedRect, *borderPattern,
                           StrokeOptions(borderWidth));
      }
    } else {
      if (backgroundPattern) {
        aDrawTarget.FillRect(rect.ToUnknownRect(), *backgroundPattern);
      }
      if (borderPattern) {
        aDrawTarget.StrokeRect(rect.ToUnknownRect(), *borderPattern,
                               StrokeOptions(borderWidth));
      }
    }
  }

  if (needsClip) {
    aDrawTarget.PopClip();
  }
}

void nsNativeBasicTheme::PaintCheckboxControl(DrawTarget& aDrawTarget,
                                              const LayoutDeviceRect& aRect,
                                              const EventStates& aState,
                                              DPIRatio aDpiRatio) {
  const CSSCoord radius = 2.0f;
  auto [backgroundColor, borderColor] =
      ComputeCheckboxColors(aState, StyleAppearance::Checkbox);
  PaintRoundedRectWithRadius(aDrawTarget, aRect, backgroundColor, borderColor,
                             kCheckboxRadioBorderWidth, radius, aDpiRatio);

  if (aState.HasState(NS_EVENT_STATE_FOCUSRING)) {
    PaintRoundedFocusRect(aDrawTarget, aRect, aDpiRatio, 5.0f, 1.0f);
  }
}

constexpr CSSCoord kCheckboxRadioContentBoxSize = 10.0f;
constexpr CSSCoord kCheckboxRadioBorderBoxSize =
    kCheckboxRadioContentBoxSize + kCheckboxRadioBorderWidth * 2.0f;

// Returns the right scale for points in a aSize x aSize sized box, centered at
// 0x0 to fill aRect in the smaller dimension.
static float ScaleToFillRect(const LayoutDeviceRect& aRect, const float aSize) {
  return std::min(aRect.width, aRect.height) / aSize;
}

void nsNativeBasicTheme::PaintCheckMark(DrawTarget& aDrawTarget,
                                        const LayoutDeviceRect& aRect,
                                        const EventStates& aState) {
  // Points come from the coordinates on a 14X14 (kCheckboxRadioBorderBoxSize)
  // unit box centered at 0,0
  const float checkPolygonX[] = {-4.5f, -1.5f, -0.5f, 5.0f, 4.75f,
                                 3.5f,  -0.5f, -1.5f, -3.5f};
  const float checkPolygonY[] = {0.5f,  4.0f, 4.0f,  -2.5f, -4.0f,
                                 -4.0f, 1.0f, 1.25f, -1.0f};
  const int32_t checkNumPoints = sizeof(checkPolygonX) / sizeof(float);
  const float scale = ScaleToFillRect(aRect, kCheckboxRadioBorderBoxSize);
  auto center = aRect.Center().ToUnknownPoint();

  RefPtr<PathBuilder> builder = aDrawTarget.CreatePathBuilder();
  Point p = center + Point(checkPolygonX[0] * scale, checkPolygonY[0] * scale);
  builder->MoveTo(p);
  for (int32_t i = 1; i < checkNumPoints; i++) {
    p = center + Point(checkPolygonX[i] * scale, checkPolygonY[i] * scale);
    builder->LineTo(p);
  }
  RefPtr<Path> path = builder->Finish();

  sRGBColor fillColor = ComputeCheckmarkColor(aState);
  aDrawTarget.Fill(path, ColorPattern(ToDeviceColor(fillColor)));
}

void nsNativeBasicTheme::PaintIndeterminateMark(DrawTarget& aDrawTarget,
                                                const LayoutDeviceRect& aRect,
                                                const EventStates& aState) {
  const CSSCoord borderWidth = 2.0f;
  const float scale = ScaleToFillRect(aRect, kCheckboxRadioBorderBoxSize);

  Rect rect = aRect.ToUnknownRect();
  rect.y += (rect.height / 2) - (borderWidth * scale / 2);
  rect.height = borderWidth * scale;
  rect.x += (borderWidth * scale) + (borderWidth * scale / 8);
  rect.width -= ((borderWidth * scale) + (borderWidth * scale / 8)) * 2;

  sRGBColor fillColor = ComputeCheckmarkColor(aState);
  aDrawTarget.FillRect(rect, ColorPattern(ToDeviceColor(fillColor)));
}

template <typename PaintBackendData>
void nsNativeBasicTheme::PaintStrokedCircle(PaintBackendData& aPaintData,
                                            const LayoutDeviceRect& aRect,
                                            const sRGBColor& aBackgroundColor,
                                            const sRGBColor& aBorderColor,
                                            const CSSCoord aBorderWidth,
                                            DPIRatio aDpiRatio) {
  auto radius = LayoutDeviceCoord(aRect.Size().width) / aDpiRatio;
  PaintRoundedRectWithRadius(aPaintData, aRect, aBackgroundColor, aBorderColor,
                             aBorderWidth, radius, aDpiRatio);
}

void nsNativeBasicTheme::PaintCircleShadow(WebRenderBackendData& aWrData,
                                           const LayoutDeviceRect& aBoxRect,
                                           const LayoutDeviceRect& aClipRect,
                                           float aShadowAlpha,
                                           const CSSPoint& aShadowOffset,
                                           CSSCoord aShadowBlurStdDev,
                                           DPIRatio aDpiRatio) {
  const bool kBackfaceIsVisible = true;
  const LayoutDeviceCoord stdDev = aShadowBlurStdDev * aDpiRatio;
  const LayoutDevicePoint shadowOffset = aShadowOffset * aDpiRatio;
  const IntSize inflation =
      gfxAlphaBoxBlur::CalculateBlurRadius(gfxPoint(stdDev, stdDev));
  LayoutDeviceRect shadowRect = aBoxRect;
  shadowRect.MoveBy(shadowOffset);
  shadowRect.Inflate(inflation.width, inflation.height);
  const auto boxRect = wr::ToLayoutRect(aBoxRect);
  aWrData.mBuilder.PushBoxShadow(
      wr::ToLayoutRect(shadowRect), wr::ToLayoutRect(aClipRect),
      kBackfaceIsVisible, boxRect,
      wr::ToLayoutVector2D(aShadowOffset * aDpiRatio),
      wr::ToColorF(DeviceColor(0.0f, 0.0f, 0.0f, aShadowAlpha)), stdDev,
      /* aSpread = */ 0.0f,
      wr::ToBorderRadius(gfx::RectCornerRadii(aBoxRect.Size().width)),
      wr::BoxShadowClipMode::Outset);
}

void nsNativeBasicTheme::PaintCircleShadow(DrawTarget& aDrawTarget,
                                           const LayoutDeviceRect& aBoxRect,
                                           const LayoutDeviceRect& aClipRect,
                                           float aShadowAlpha,
                                           const CSSPoint& aShadowOffset,
                                           CSSCoord aShadowBlurStdDev,
                                           DPIRatio aDpiRatio) {
  Float stdDev = aShadowBlurStdDev * aDpiRatio;
  Point offset = (aShadowOffset * aDpiRatio).ToUnknownPoint();

  RefPtr<FilterNode> blurFilter =
      aDrawTarget.CreateFilter(FilterType::GAUSSIAN_BLUR);
  if (!blurFilter) {
    return;
  }

  blurFilter->SetAttribute(ATT_GAUSSIAN_BLUR_STD_DEVIATION, stdDev);

  IntSize inflation =
      gfxAlphaBoxBlur::CalculateBlurRadius(gfxPoint(stdDev, stdDev));
  Rect inflatedRect = aBoxRect.ToUnknownRect();
  inflatedRect.Inflate(inflation.width, inflation.height);
  Rect sourceRectInFilterSpace =
      inflatedRect - aBoxRect.TopLeft().ToUnknownPoint();
  Point destinationPointOfSourceRect = inflatedRect.TopLeft() + offset;

  IntSize dtSize = RoundedToInt(aBoxRect.Size().ToUnknownSize());
  RefPtr<DrawTarget> ellipseDT = aDrawTarget.CreateSimilarDrawTargetForFilter(
      dtSize, SurfaceFormat::A8, blurFilter, blurFilter,
      sourceRectInFilterSpace, destinationPointOfSourceRect);
  if (!ellipseDT) {
    return;
  }

  AutoClipRect clipRect(aDrawTarget, aClipRect);

  RefPtr<Path> ellipse = MakePathForEllipse(
      *ellipseDT, (aBoxRect - aBoxRect.TopLeft()).Center().ToUnknownPoint(),
      aBoxRect.Size().ToUnknownSize());
  ellipseDT->Fill(ellipse,
                  ColorPattern(DeviceColor(0.0f, 0.0f, 0.0f, aShadowAlpha)));
  RefPtr<SourceSurface> ellipseSurface = ellipseDT->Snapshot();

  blurFilter->SetInput(IN_GAUSSIAN_BLUR_IN, ellipseSurface);
  aDrawTarget.DrawFilter(blurFilter, sourceRectInFilterSpace,
                         destinationPointOfSourceRect);
}

template <typename PaintBackendData>
void nsNativeBasicTheme::PaintRadioControl(PaintBackendData& aPaintData,
                                           const LayoutDeviceRect& aRect,
                                           const EventStates& aState,
                                           DPIRatio aDpiRatio) {
  const CSSCoord borderWidth = 2.0f;
  auto [backgroundColor, borderColor] =
      ComputeCheckboxColors(aState, StyleAppearance::Radio);

  PaintStrokedCircle(aPaintData, aRect, backgroundColor, borderColor,
                     borderWidth, aDpiRatio);

  if (aState.HasState(NS_EVENT_STATE_FOCUSRING)) {
    PaintRoundedFocusRect(aPaintData, aRect, aDpiRatio, 5.0f, 1.0f);
  }
}

template <typename PaintBackendData>
void nsNativeBasicTheme::PaintRadioCheckmark(PaintBackendData& aPaintData,
                                             const LayoutDeviceRect& aRect,
                                             const EventStates& aState,
                                             DPIRatio aDpiRatio) {
  const CSSCoord borderWidth = 2.0f;
  const float scale = ScaleToFillRect(aRect, kCheckboxRadioBorderBoxSize);
  auto [backgroundColor, checkColor] = ComputeRadioCheckmarkColors(aState);

  LayoutDeviceRect rect(aRect);
  rect.Deflate(borderWidth * scale);

  PaintStrokedCircle(aPaintData, rect, checkColor, backgroundColor, borderWidth,
                     aDpiRatio);
}

template <typename PaintBackendData>
void nsNativeBasicTheme::PaintTextField(PaintBackendData& aPaintData,
                                        const LayoutDeviceRect& aRect,
                                        const EventStates& aState,
                                        DPIRatio aDpiRatio) {
  auto [backgroundColor, borderColor] = ComputeTextfieldColors(aState);

  const CSSCoord radius = 2.0f;

  PaintRoundedRectWithRadius(aPaintData, aRect, backgroundColor, borderColor,
                             kTextFieldBorderWidth, radius, aDpiRatio);

  if (aState.HasState(NS_EVENT_STATE_FOCUSRING)) {
    PaintRoundedFocusRect(aPaintData, aRect, aDpiRatio,
                          radius + kTextFieldBorderWidth,
                          -kTextFieldBorderWidth);
  }
}

template <typename PaintBackendData>
void nsNativeBasicTheme::PaintListbox(PaintBackendData& aPaintData,
                                      const LayoutDeviceRect& aRect,
                                      const EventStates& aState,
                                      DPIRatio aDpiRatio) {
  const CSSCoord radius = 2.0f;
  auto [backgroundColor, borderColor] = ComputeTextfieldColors(aState);

  PaintRoundedRectWithRadius(aPaintData, aRect, backgroundColor, borderColor,
                             kMenulistBorderWidth, radius, aDpiRatio);

  if (aState.HasState(NS_EVENT_STATE_FOCUSRING)) {
    PaintRoundedFocusRect(aPaintData, aRect, aDpiRatio,
                          radius + kMenulistBorderWidth, -kMenulistBorderWidth);
  }
}

template <typename PaintBackendData>
void nsNativeBasicTheme::PaintMenulist(PaintBackendData& aDrawTarget,
                                       const LayoutDeviceRect& aRect,
                                       const EventStates& aState,
                                       DPIRatio aDpiRatio) {
  const CSSCoord radius = 4.0f;
  auto [backgroundColor, borderColor] = ComputeButtonColors(aState);

  PaintRoundedRectWithRadius(aDrawTarget, aRect, backgroundColor, borderColor,
                             kMenulistBorderWidth, radius, aDpiRatio);

  if (aState.HasState(NS_EVENT_STATE_FOCUSRING)) {
    PaintRoundedFocusRect(aDrawTarget, aRect, aDpiRatio,
                          radius + kMenulistBorderWidth, -kMenulistBorderWidth);
  }
}

void nsNativeBasicTheme::PaintArrow(DrawTarget& aDrawTarget,
                                    const LayoutDeviceRect& aRect,
                                    const float aArrowPolygonX[],
                                    const float aArrowPolygonY[],
                                    const float aArrowPolygonSize,
                                    const int32_t aArrowNumPoints,
                                    const sRGBColor aFillColor) {
  const float scale = ScaleToFillRect(aRect, aArrowPolygonSize);

  auto center = aRect.Center().ToUnknownPoint();

  RefPtr<PathBuilder> builder = aDrawTarget.CreatePathBuilder();
  Point p =
      center + Point(aArrowPolygonX[0] * scale, aArrowPolygonY[0] * scale);
  builder->MoveTo(p);
  for (int32_t i = 1; i < aArrowNumPoints; i++) {
    p = center + Point(aArrowPolygonX[i] * scale, aArrowPolygonY[i] * scale);
    builder->LineTo(p);
  }
  RefPtr<Path> path = builder->Finish();

  aDrawTarget.Fill(path, ColorPattern(ToDeviceColor(aFillColor)));
}

void nsNativeBasicTheme::PaintMenulistArrowButton(nsIFrame* aFrame,
                                                  DrawTarget& aDrawTarget,
                                                  const LayoutDeviceRect& aRect,
                                                  const EventStates& aState) {
  const float kPolygonX[] = {-4.0f, -0.5f, 0.5f, 4.0f,  4.0f,
                             3.0f,  0.0f,  0.0f, -3.0f, -4.0f};
  const float kPolygonY[] = {-1,    3.0f, 3.0f, -1.0f, -2.0f,
                             -2.0f, 1.5f, 1.5f, -2.0f, -2.0f};

  const float kPolygonSize = kMinimumDropdownArrowButtonWidth;

  sRGBColor arrowColor = ComputeMenulistArrowButtonColor(aState);
  PaintArrow(aDrawTarget, aRect, kPolygonX, kPolygonY, kPolygonSize,
             ArrayLength(kPolygonX), arrowColor);
}

void nsNativeBasicTheme::PaintSpinnerButton(nsIFrame* aFrame,
                                            DrawTarget& aDrawTarget,
                                            const LayoutDeviceRect& aRect,
                                            const EventStates& aState,
                                            StyleAppearance aAppearance,
                                            DPIRatio aDpiRatio) {
  auto [backgroundColor, borderColor] = ComputeButtonColors(aState);

  aDrawTarget.FillRect(aRect.ToUnknownRect(),
                       ColorPattern(ToDeviceColor(backgroundColor)));

  const float kPolygonX[] = {-3.5f, -0.5f, 0.5f, 3.5f,  3.5f,
                             2.5f,  0.0f,  0.0f, -2.5f, -3.5f};
  float polygonY[] = {-1.5f, 1.5f, 1.5f, -1.5f, -2.5f,
                      -2.5f, 0.0f, 0.0f, -2.5f, -2.5f};

  const float kPolygonSize = kMinimumSpinnerButtonHeight;
  if (aAppearance == StyleAppearance::SpinnerUpbutton) {
    for (auto& coord : polygonY) {
      coord = -coord;
    }
  }

  PaintArrow(aDrawTarget, aRect, kPolygonX, polygonY, kPolygonSize,
             ArrayLength(kPolygonX), borderColor);
}

template <typename PaintBackendData>
void nsNativeBasicTheme::PaintRange(nsIFrame* aFrame,
                                    PaintBackendData& aPaintData,
                                    const LayoutDeviceRect& aRect,
                                    const EventStates& aState,
                                    DPIRatio aDpiRatio, bool aHorizontal) {
  nsRangeFrame* rangeFrame = do_QueryFrame(aFrame);
  if (!rangeFrame) {
    return;
  }

  double progress = rangeFrame->GetValueAsFractionOfRange();
  auto rect = aRect;
  LayoutDeviceRect thumbRect(0, 0, kMinimumRangeThumbSize * aDpiRatio,
                             kMinimumRangeThumbSize * aDpiRatio);
  LayoutDeviceRect progressClipRect(aRect);
  LayoutDeviceRect trackClipRect(aRect);
  const LayoutDeviceCoord verticalSize = kRangeHeight * aDpiRatio;
  if (aHorizontal) {
    rect.height = verticalSize;
    rect.y = aRect.y + (aRect.height - rect.height) / 2;
    thumbRect.y = aRect.y + (aRect.height - thumbRect.height) / 2;

    if (IsFrameRTL(aFrame)) {
      thumbRect.x =
          aRect.x + (aRect.width - thumbRect.width) * (1.0 - progress);
      float midPoint = thumbRect.Center().X();
      trackClipRect.SetBoxX(aRect.X(), midPoint);
      progressClipRect.SetBoxX(midPoint, aRect.XMost());
    } else {
      thumbRect.x = aRect.x + (aRect.width - thumbRect.width) * progress;
      float midPoint = thumbRect.Center().X();
      progressClipRect.SetBoxX(aRect.X(), midPoint);
      trackClipRect.SetBoxX(midPoint, aRect.XMost());
    }
  } else {
    rect.width = verticalSize;
    rect.x = aRect.x + (aRect.width - rect.width) / 2;
    thumbRect.x = aRect.x + (aRect.width - thumbRect.width) / 2;

    thumbRect.y = aRect.y + (aRect.height - thumbRect.height) * progress;
    float midPoint = thumbRect.Center().Y();
    trackClipRect.SetBoxY(aRect.Y(), midPoint);
    progressClipRect.SetBoxY(midPoint, aRect.YMost());
  }

  const CSSCoord borderWidth = 1.0f;
  const CSSCoord radius = 2.0f;

  auto [progressColor, progressBorderColor] =
      ComputeRangeProgressColors(aState);
  auto [trackColor, trackBorderColor] = ComputeRangeTrackColors(aState);

  PaintRoundedRectWithRadius(aPaintData, rect, progressClipRect, progressColor,
                             progressBorderColor, borderWidth, radius,
                             aDpiRatio);

  PaintRoundedRectWithRadius(aPaintData, rect, trackClipRect, trackColor,
                             trackBorderColor, borderWidth, radius, aDpiRatio);

  if (!aState.HasState(NS_EVENT_STATE_DISABLED)) {
    // Ensure the shadow doesn't expand outside of our overflow rect declared in
    // GetWidgetOverflow().
    auto overflowRect = aRect;
    overflowRect.Inflate(CSSCoord(6.0f) * aDpiRatio);
    // Thumb shadow
    PaintCircleShadow(aPaintData, thumbRect, overflowRect, 0.3f,
                      CSSPoint(0.0f, 2.0f), 2.0f, aDpiRatio);
  }

  // Draw the thumb on top.
  const CSSCoord thumbBorderWidth = 2.0f;
  auto [thumbColor, thumbBorderColor] = ComputeRangeThumbColors(aState);

  PaintStrokedCircle(aPaintData, thumbRect, thumbColor, thumbBorderColor,
                     thumbBorderWidth, aDpiRatio);

  if (aState.HasState(NS_EVENT_STATE_FOCUSRING)) {
    PaintRoundedFocusRect(aPaintData, aRect, aDpiRatio, radius, 1.0f);
  }
}

// TODO: Indeterminate state.
template <typename PaintBackendData>
void nsNativeBasicTheme::PaintProgress(nsIFrame* aFrame,
                                       PaintBackendData& aPaintData,
                                       const LayoutDeviceRect& aRect,
                                       const EventStates& aState,
                                       DPIRatio aDpiRatio, bool aIsMeter,
                                       bool aBar) {
  auto [backgroundColor, borderColor] = [&] {
    if (aIsMeter) {
      return aBar ? ComputeMeterTrackColors() : ComputeMeterchunkColors(aState);
    }
    return aBar ? ComputeProgressTrackColors() : ComputeProgressColors();
  }();

  const CSSCoord borderWidth = 1.0f;
  const CSSCoord radius = aIsMeter ? 5.0f : 2.0f;

  LayoutDeviceRect rect(aRect);
  const LayoutDeviceCoord thickness =
      (aIsMeter ? kMeterHeight : kProgressbarHeight) * aDpiRatio;

  const bool isHorizontal = !nsNativeTheme::IsVerticalProgress(aFrame);
  if (isHorizontal) {
    // Center it vertically.
    rect.y += (rect.height - thickness) / 2;
    rect.height = thickness;
  } else {
    // Center it horizontally.
    rect.x += (rect.width - thickness) / 2;
    rect.width = thickness;
  }

  // This is the progress chunk, clip it to the right amount.
  LayoutDeviceRect clipRect = rect;
  if (!aBar) {
    double position = [&] {
      if (aIsMeter) {
        auto* meter = dom::HTMLMeterElement::FromNode(aFrame->GetContent());
        if (!meter) {
          return 0.0;
        }
        return meter->Value() / meter->Max();
      }
      auto* progress = dom::HTMLProgressElement::FromNode(aFrame->GetContent());
      if (!progress) {
        return 0.0;
      }
      return progress->Value() / progress->Max();
    }();
    if (isHorizontal) {
      double clipWidth = rect.width * position;
      clipRect.width = clipWidth;
      if (IsFrameRTL(aFrame)) {
        clipRect.x += rect.width - clipWidth;
      }
    } else {
      double clipHeight = rect.height * position;
      clipRect.height = clipHeight;
      clipRect.y += rect.height - clipHeight;
    }
  }

  PaintRoundedRectWithRadius(aPaintData, rect, clipRect, backgroundColor,
                             borderColor, borderWidth, radius, aDpiRatio);
}

template <typename PaintBackendData>
void nsNativeBasicTheme::PaintButton(nsIFrame* aFrame,
                                     PaintBackendData& aPaintData,
                                     const LayoutDeviceRect& aRect,
                                     const EventStates& aState,
                                     DPIRatio aDpiRatio) {
  const CSSCoord radius = 4.0f;
  auto [backgroundColor, borderColor] = ComputeButtonColors(aState, aFrame);

  PaintRoundedRectWithRadius(aPaintData, aRect, backgroundColor, borderColor,
                             kButtonBorderWidth, radius, aDpiRatio);

  if (aState.HasState(NS_EVENT_STATE_FOCUSRING)) {
    PaintRoundedFocusRect(aPaintData, aRect, aDpiRatio,
                          radius + kButtonBorderWidth, -kButtonBorderWidth);
  }
}

template <typename PaintBackendData>
bool nsNativeBasicTheme::DoPaintDefaultScrollbarThumb(
    PaintBackendData& aPaintData, const LayoutDeviceRect& aRect,
    bool aHorizontal, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    DPIRatio aDpiRatio) {
  sRGBColor thumbColor =
      ComputeScrollbarThumbColor(aFrame, aStyle, aElementState, aDocumentState);
  FillRect(aPaintData, aRect, thumbColor);
  return true;
}

bool nsNativeBasicTheme::PaintScrollbarThumb(DrawTarget& aDrawTarget,
                                             const LayoutDeviceRect& aRect,
                                             bool aHorizontal, nsIFrame* aFrame,
                                             const ComputedStyle& aStyle,
                                             const EventStates& aElementState,
                                             const EventStates& aDocumentState,
                                             DPIRatio aDpiRatio) {
  return DoPaintDefaultScrollbarThumb(aDrawTarget, aRect, aHorizontal, aFrame,
                                      aStyle, aElementState, aDocumentState,
                                      aDpiRatio);
}

bool nsNativeBasicTheme::PaintScrollbarThumb(WebRenderBackendData& aWrData,
                                             const LayoutDeviceRect& aRect,
                                             bool aHorizontal, nsIFrame* aFrame,
                                             const ComputedStyle& aStyle,
                                             const EventStates& aElementState,
                                             const EventStates& aDocumentState,
                                             DPIRatio aDpiRatio) {
  return DoPaintDefaultScrollbarThumb(aWrData, aRect, aHorizontal, aFrame,
                                      aStyle, aElementState, aDocumentState,
                                      aDpiRatio);
}

template <typename PaintBackendData>
bool nsNativeBasicTheme::DoPaintDefaultScrollbar(
    PaintBackendData& aPaintData, const LayoutDeviceRect& aRect,
    bool aHorizontal, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aDocumentState, DPIRatio aDpiRatio) {
  auto [scrollbarColor, borderColor] =
      ComputeScrollbarColors(aFrame, aStyle, aDocumentState);
  FillRect(aPaintData, aRect, scrollbarColor);
  // FIXME(heycam): We should probably derive the border color when custom
  // scrollbar colors are in use too.  But for now, just skip painting it,
  // to avoid ugliness.
  if (aStyle.StyleUI()->mScrollbarColor.IsAuto()) {
    // Draw a 1px-wide line in the top / left of the scrollbar.
    LayoutDeviceRect borderRect(aRect);
    LayoutDeviceCoord onePx = CSSCoord(1.0f) * aDpiRatio;
    if (aHorizontal) {
      borderRect.height = onePx;
    } else {
      borderRect.width = onePx;
    }
    FillRect(aPaintData, borderRect, borderColor);
  }
  return true;
}

bool nsNativeBasicTheme::PaintScrollbar(DrawTarget& aDrawTarget,
                                        const LayoutDeviceRect& aRect,
                                        bool aHorizontal, nsIFrame* aFrame,
                                        const ComputedStyle& aStyle,
                                        const EventStates& aDocumentState,
                                        DPIRatio aDpiRatio) {
  return DoPaintDefaultScrollbar(aDrawTarget, aRect, aHorizontal, aFrame,
                                 aStyle, aDocumentState, aDpiRatio);
}

bool nsNativeBasicTheme::PaintScrollbar(WebRenderBackendData& aWrData,
                                        const LayoutDeviceRect& aRect,
                                        bool aHorizontal, nsIFrame* aFrame,
                                        const ComputedStyle& aStyle,
                                        const EventStates& aDocumentState,
                                        DPIRatio aDpiRatio) {
  return DoPaintDefaultScrollbar(aWrData, aRect, aHorizontal, aFrame, aStyle,
                                 aDocumentState, aDpiRatio);
}

template <typename PaintBackendData>
bool nsNativeBasicTheme::DoPaintDefaultScrollCorner(
    PaintBackendData& aPaintData, const LayoutDeviceRect& aRect,
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aDocumentState, DPIRatio aDpiRatio) {
  auto [scrollbarColor, borderColor] =
      ComputeScrollbarColors(aFrame, aStyle, aDocumentState);
  Unused << borderColor;
  FillRect(aPaintData, aRect, scrollbarColor);
  return true;
}

bool nsNativeBasicTheme::PaintScrollCorner(DrawTarget& aDrawTarget,
                                           const LayoutDeviceRect& aRect,
                                           nsIFrame* aFrame,
                                           const ComputedStyle& aStyle,
                                           const EventStates& aDocumentState,
                                           DPIRatio aDpiRatio) {
  return DoPaintDefaultScrollCorner(aDrawTarget, aRect, aFrame, aStyle,
                                    aDocumentState, aDpiRatio);
}

bool nsNativeBasicTheme::PaintScrollCorner(WebRenderBackendData& aWrData,
                                           const LayoutDeviceRect& aRect,
                                           nsIFrame* aFrame,
                                           const ComputedStyle& aStyle,
                                           const EventStates& aDocumentState,
                                           DPIRatio aDpiRatio) {
  return DoPaintDefaultScrollCorner(aWrData, aRect, aFrame, aStyle,
                                    aDocumentState, aDpiRatio);
}

void nsNativeBasicTheme::PaintScrollbarButton(
    DrawTarget& aDrawTarget, StyleAppearance aAppearance,
    const LayoutDeviceRect& aRect, nsIFrame* aFrame,
    const ComputedStyle& aStyle, const EventStates& aElementState,
    const EventStates& aDocumentState, DPIRatio aDpiRatio) {
  bool hasCustomColor = aStyle.StyleUI()->mScrollbarColor.IsColors();
  auto [buttonColor, arrowColor, borderColor] = ComputeScrollbarButtonColors(
      aFrame, aAppearance, aStyle, aElementState, aDocumentState);
  aDrawTarget.FillRect(aRect.ToUnknownRect(),
                       ColorPattern(ToDeviceColor(buttonColor)));

  // Start with Up arrow.
  float arrowPolygonX[] = {-4.0f, 0.0f, 4.0f, 4.0f, 0.0f, -4.0f};
  float arrowPolygonY[] = {0.0f, -4.0f, 0.0f, 3.0f, -1.0f, 3.0f};

  const float kPolygonSize = kMinimumScrollbarSize;

  const int32_t arrowNumPoints = ArrayLength(arrowPolygonX);
  switch (aAppearance) {
    case StyleAppearance::ScrollbarbuttonUp:
      break;
    case StyleAppearance::ScrollbarbuttonDown:
      for (int32_t i = 0; i < arrowNumPoints; i++) {
        arrowPolygonY[i] *= -1;
      }
      break;
    case StyleAppearance::ScrollbarbuttonLeft:
      for (int32_t i = 0; i < arrowNumPoints; i++) {
        int32_t temp = arrowPolygonX[i];
        arrowPolygonX[i] = arrowPolygonY[i];
        arrowPolygonY[i] = temp;
      }
      break;
    case StyleAppearance::ScrollbarbuttonRight:
      for (int32_t i = 0; i < arrowNumPoints; i++) {
        int32_t temp = arrowPolygonX[i];
        arrowPolygonX[i] = arrowPolygonY[i] * -1;
        arrowPolygonY[i] = temp;
      }
      break;
    default:
      return;
  }
  PaintArrow(aDrawTarget, aRect, arrowPolygonX, arrowPolygonY, kPolygonSize,
             arrowNumPoints, arrowColor);

  // FIXME(heycam): We should probably derive the border color when custom
  // scrollbar colors are in use too.  But for now, just skip painting it,
  // to avoid ugliness.
  if (!hasCustomColor) {
    RefPtr<PathBuilder> builder = aDrawTarget.CreatePathBuilder();
    builder->MoveTo(Point(aRect.x, aRect.y));
    if (aAppearance == StyleAppearance::ScrollbarbuttonUp ||
        aAppearance == StyleAppearance::ScrollbarbuttonDown) {
      builder->LineTo(Point(aRect.x, aRect.y + aRect.height));
    } else {
      builder->LineTo(Point(aRect.x + aRect.width, aRect.y));
    }

    RefPtr<Path> path = builder->Finish();
    aDrawTarget.Stroke(path, ColorPattern(ToDeviceColor(borderColor)),
                       StrokeOptions(CSSCoord(1.0f) * aDpiRatio));
  }
}

NS_IMETHODIMP
nsNativeBasicTheme::DrawWidgetBackground(gfxContext* aContext, nsIFrame* aFrame,
                                         StyleAppearance aAppearance,
                                         const nsRect& aRect,
                                         const nsRect& /* aDirtyRect */) {
  if (!DoDrawWidgetBackground(*aContext->GetDrawTarget(), aFrame, aAppearance,
                              aRect)) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  return NS_OK;
}

bool nsNativeBasicTheme::CreateWebRenderCommandsForWidget(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const mozilla::layers::StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager, nsIFrame* aFrame,
    StyleAppearance aAppearance, const nsRect& aRect) {
  if (!StaticPrefs::widget_non_native_theme_webrender()) {
    return false;
  }
  WebRenderBackendData data{aBuilder, aResources, aSc, aManager};
  return DoDrawWidgetBackground(data, aFrame, aAppearance, aRect);
}

static LayoutDeviceRect ToSnappedRect(const nsRect& aRect,
                                      nscoord aTwipsPerPixel, DrawTarget& aDt) {
  return LayoutDeviceRect::FromUnknownRect(
      NSRectToSnappedRect(aRect, aTwipsPerPixel, aDt));
}

static LayoutDeviceRect ToSnappedRect(
    const nsRect& aRect, nscoord aTwipsPerPixel,
    nsNativeBasicTheme::WebRenderBackendData& aDt) {
  // TODO: Do we need to do any more snapping here?
  return LayoutDeviceRect::FromAppUnits(aRect, aTwipsPerPixel);
}

template <typename PaintBackendData>
bool nsNativeBasicTheme::DoDrawWidgetBackground(PaintBackendData& aPaintData,
                                                nsIFrame* aFrame,
                                                StyleAppearance aAppearance,
                                                const nsRect& aRect) {
  static_assert(std::is_same_v<PaintBackendData, DrawTarget> ||
                std::is_same_v<PaintBackendData, WebRenderBackendData>);

  const nsPresContext* pc = aFrame->PresContext();
  const nscoord twipsPerPixel = pc->AppUnitsPerDevPixel();
  const auto devPxRect = ToSnappedRect(aRect, twipsPerPixel, aPaintData);

  const EventStates docState = pc->Document()->GetDocumentState();
  EventStates eventState = GetContentState(aFrame, aAppearance);
  if (aAppearance == StyleAppearance::MozMenulistArrowButton) {
    bool isHTML = IsHTMLContent(aFrame);
    nsIFrame* parentFrame = aFrame->GetParent();
    bool isMenulist = !isHTML && parentFrame->IsMenuFrame();
    // HTML select and XUL menulist dropdown buttons get state from the
    // parent.
    if (isHTML || isMenulist) {
      aFrame = parentFrame;
      eventState = GetContentState(parentFrame, aAppearance);
    }
  }

  // Hack to avoid skia fuzziness: Add a dummy clip if the widget doesn't
  // overflow devPxRect.
  Maybe<AutoClipRect> maybeClipRect;
  if constexpr (std::is_same_v<PaintBackendData, DrawTarget>) {
    if (aAppearance != StyleAppearance::FocusOutline &&
        aAppearance != StyleAppearance::Range &&
        !eventState.HasState(NS_EVENT_STATE_FOCUSRING)) {
      maybeClipRect.emplace(aPaintData, devPxRect);
    }
  }

  DPIRatio dpiRatio = GetDPIRatio(aFrame, aAppearance);

  switch (aAppearance) {
    case StyleAppearance::Radio: {
      auto rect = FixAspectRatio(devPxRect);
      PaintRadioControl(aPaintData, rect, eventState, dpiRatio);
      if (IsSelected(aFrame)) {
        PaintRadioCheckmark(aPaintData, rect, eventState, dpiRatio);
      }
      break;
    }
    case StyleAppearance::Checkbox: {
      if constexpr (std::is_same_v<PaintBackendData, WebRenderBackendData>) {
        // TODO: Need to figure out how to best draw this using WR.
        return false;
      } else {
        auto rect = FixAspectRatio(devPxRect);
        PaintCheckboxControl(aPaintData, rect, eventState, dpiRatio);
        if (GetIndeterminate(aFrame)) {
          PaintIndeterminateMark(aPaintData, rect, eventState);
        } else if (IsChecked(aFrame)) {
          PaintCheckMark(aPaintData, rect, eventState);
        }
      }
      break;
    }
    case StyleAppearance::Textarea:
    case StyleAppearance::Textfield:
    case StyleAppearance::NumberInput:
      PaintTextField(aPaintData, devPxRect, eventState, dpiRatio);
      break;
    case StyleAppearance::Listbox:
      PaintListbox(aPaintData, devPxRect, eventState, dpiRatio);
      break;
    case StyleAppearance::MenulistButton:
    case StyleAppearance::Menulist:
      PaintMenulist(aPaintData, devPxRect, eventState, dpiRatio);
      break;
    case StyleAppearance::MozMenulistArrowButton:
      if constexpr (std::is_same_v<PaintBackendData, WebRenderBackendData>) {
        // TODO: Need to figure out how to best draw this using WR.
        return false;
      } else {
        PaintMenulistArrowButton(aFrame, aPaintData, devPxRect, eventState);
      }
      break;
    case StyleAppearance::SpinnerUpbutton:
    case StyleAppearance::SpinnerDownbutton:
      if constexpr (std::is_same_v<PaintBackendData, WebRenderBackendData>) {
        // TODO: Need to figure out how to best draw this using WR.
        return false;
      } else {
        PaintSpinnerButton(aFrame, aPaintData, devPxRect, eventState,
                           aAppearance, dpiRatio);
      }
      break;
    case StyleAppearance::Range:
      PaintRange(aFrame, aPaintData, devPxRect, eventState, dpiRatio,
                 IsRangeHorizontal(aFrame));
      break;
    case StyleAppearance::RangeThumb:
      // Painted as part of StyleAppearance::Range.
      break;
    case StyleAppearance::ProgressBar:
      PaintProgress(aFrame, aPaintData, devPxRect, eventState, dpiRatio,
                    /* aIsMeter = */ false, /* aBar = */ true);
      break;
    case StyleAppearance::Progresschunk:
      if (nsProgressFrame* f = do_QueryFrame(aFrame->GetParent())) {
        PaintProgress(f, aPaintData, devPxRect,
                      f->GetContent()->AsElement()->State(), dpiRatio,
                      /* aIsMeter = */ false, /* aBar = */ false);
      }
      break;
    case StyleAppearance::Meter:
      PaintProgress(aFrame, aPaintData, devPxRect, eventState, dpiRatio,
                    /* aIsMeter = */ true, /* aBar = */ true);
      break;
    case StyleAppearance::Meterchunk:
      if (nsMeterFrame* f = do_QueryFrame(aFrame->GetParent())) {
        PaintProgress(f, aPaintData, devPxRect,
                      f->GetContent()->AsElement()->State(), dpiRatio,
                      /* aIsMeter = */ true, /* aBar = */ false);
      }
      break;
    case StyleAppearance::ScrollbarthumbHorizontal:
    case StyleAppearance::ScrollbarthumbVertical: {
      bool isHorizontal =
          aAppearance == StyleAppearance::ScrollbarthumbHorizontal;
      return PaintScrollbarThumb(aPaintData, devPxRect, isHorizontal, aFrame,
                                 *nsLayoutUtils::StyleForScrollbar(aFrame),
                                 eventState, docState, dpiRatio);
    }
    case StyleAppearance::ScrollbartrackHorizontal:
    case StyleAppearance::ScrollbartrackVertical: {
      bool isHorizontal =
          aAppearance == StyleAppearance::ScrollbartrackHorizontal;
      return PaintScrollbarTrack(aPaintData, devPxRect, isHorizontal, aFrame,
                                 *nsLayoutUtils::StyleForScrollbar(aFrame),
                                 docState, dpiRatio);
    }
    case StyleAppearance::ScrollbarHorizontal:
    case StyleAppearance::ScrollbarVertical: {
      bool isHorizontal = aAppearance == StyleAppearance::ScrollbarHorizontal;
      return PaintScrollbar(aPaintData, devPxRect, isHorizontal, aFrame,
                            *nsLayoutUtils::StyleForScrollbar(aFrame), docState,
                            dpiRatio);
    }
    case StyleAppearance::Scrollcorner:
      return PaintScrollCorner(aPaintData, devPxRect, aFrame,
                               *nsLayoutUtils::StyleForScrollbar(aFrame),
                               docState, dpiRatio);
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonDown:
    case StyleAppearance::ScrollbarbuttonLeft:
    case StyleAppearance::ScrollbarbuttonRight:
      // For scrollbar-width:thin, we don't display the buttons.
      if (!IsScrollbarWidthThin(aFrame)) {
        if constexpr (std::is_same_v<PaintBackendData, WebRenderBackendData>) {
          // TODO: Need to figure out how to best draw this using WR.
          return false;
        } else {
          PaintScrollbarButton(aPaintData, aAppearance, devPxRect, aFrame,
                               *nsLayoutUtils::StyleForScrollbar(aFrame),
                               eventState, docState, dpiRatio);
        }
      }
      break;
    case StyleAppearance::Button:
      PaintButton(aFrame, aPaintData, devPxRect, eventState, dpiRatio);
      break;
    case StyleAppearance::FocusOutline:
      PaintAutoStyleOutline(aFrame, aPaintData, devPxRect, dpiRatio);
      break;
    default:
      // Various appearance values are used for XUL elements.  Normally these
      // will not be available in content documents (and thus in the content
      // processes where the native basic theme can be used), but tests are
      // run with the remote XUL pref enabled and so we can get in here.  So
      // we just return an error rather than assert.
      return false;
  }

  return true;
}

template <typename PaintBackendData>
void nsNativeBasicTheme::PaintAutoStyleOutline(nsIFrame* aFrame,
                                               PaintBackendData& aPaintData,
                                               const LayoutDeviceRect& aRect,
                                               DPIRatio aDpiRatio) {
  auto [innerColor, middleColor, outerColor] = ComputeFocusRectColors();
  Unused << middleColor;
  Unused << outerColor;

  LayoutDeviceRect rect(aRect);
  auto width =
      LayoutDeviceCoord(SnapBorderWidth(kInnerFocusOutlineWidth, aDpiRatio));
  rect.Inflate(width);

  nscoord cssRadii[8];
  if (!aFrame->GetBorderRadii(cssRadii)) {
    return PaintRoundedRectWithRadius(aPaintData, rect, sRGBColor::White(0.0f),
                                      innerColor, kInnerFocusOutlineWidth,
                                      /* aRadius = */ 0.0f, aDpiRatio);
  }

  nsPresContext* pc = aFrame->PresContext();
  const nscoord offset = aFrame->StyleOutline()->mOutlineOffset.ToAppUnits();
  const Float devPixelOffset = pc->AppUnitsToFloatDevPixels(offset);

  RectCornerRadii innerRadii;
  nsCSSRendering::ComputePixelRadii(cssRadii, pc->AppUnitsPerDevPixel(),
                                    &innerRadii);

  const auto borderColor = ToDeviceColor(innerColor);
  // NOTE(emilio): This doesn't use PaintRoundedRectWithRadius because we need
  // to support arbitrary radii.
  RectCornerRadii outerRadii;
  if constexpr (std::is_same_v<PaintBackendData, WebRenderBackendData>) {
    const Float widths[4] = {devPixelOffset, devPixelOffset, devPixelOffset,
                             devPixelOffset};
    nsCSSBorderRenderer::ComputeOuterRadii(innerRadii, widths, &outerRadii);

    const auto dest = wr::ToLayoutRect(rect);
    const auto side = wr::ToBorderSide(borderColor, StyleBorderStyle::Solid);
    const wr::BorderSide sides[4] = {side, side, side, side};
    const bool kBackfaceIsVisible = true;
    const auto wrWidths = wr::ToBorderWidths(width, width, width, width);
    const auto wrRadius = wr::ToBorderRadius(outerRadii);
    aPaintData.mBuilder.PushBorder(dest, dest, kBackfaceIsVisible, wrWidths,
                                   {sides, 4}, wrRadius);
  } else {
    const LayoutDeviceCoord halfWidth = width * 0.5f;
    rect.Deflate(halfWidth);
    const Float widths[4] = {
        halfWidth + devPixelOffset, halfWidth + devPixelOffset,
        halfWidth + devPixelOffset, halfWidth + devPixelOffset};
    nsCSSBorderRenderer::ComputeOuterRadii(innerRadii, widths, &outerRadii);
    RefPtr<Path> path =
        MakePathForRoundedRect(aPaintData, rect.ToUnknownRect(), outerRadii);
    aPaintData.Stroke(path, ColorPattern(borderColor), StrokeOptions(width));
  }
}

LayoutDeviceIntMargin nsNativeBasicTheme::GetWidgetBorder(
    nsDeviceContext* aContext, nsIFrame* aFrame, StyleAppearance aAppearance) {
  switch (aAppearance) {
    case StyleAppearance::Textfield:
    case StyleAppearance::Textarea:
    case StyleAppearance::NumberInput:
    case StyleAppearance::Listbox:
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton:
    case StyleAppearance::Button:
      // Return the border size from the UA sheet, even though what we paint
      // doesn't actually match that. We know this is the UA sheet border
      // because we disable native theming when different border widths are
      // specified by authors, see nsNativeBasicTheme::IsWidgetStyled.
      //
      // The Rounded() bit is technically redundant, but needed to appease the
      // type system, we should always end up with full device pixels due to
      // round_border_to_device_pixels at style time.
      return LayoutDeviceIntMargin::FromAppUnits(
                 aFrame->StyleBorder()->GetComputedBorder(),
                 aFrame->PresContext()->AppUnitsPerDevPixel())
          .Rounded();
    case StyleAppearance::Checkbox:
    case StyleAppearance::Radio: {
      DPIRatio dpiRatio = GetDPIRatio(aFrame, aAppearance);
      LayoutDeviceIntCoord w =
          SnapBorderWidth(kCheckboxRadioBorderWidth, dpiRatio);
      return LayoutDeviceIntMargin(w, w, w, w);
    }
    default:
      return LayoutDeviceIntMargin();
  }
}

bool nsNativeBasicTheme::GetWidgetPadding(nsDeviceContext* aContext,
                                          nsIFrame* aFrame,
                                          StyleAppearance aAppearance,
                                          LayoutDeviceIntMargin* aResult) {
  switch (aAppearance) {
    // Radios and checkboxes return a fixed size in GetMinimumWidgetSize
    // and have a meaningful baseline, so they can't have
    // author-specified padding.
    case StyleAppearance::Radio:
    case StyleAppearance::Checkbox:
      aResult->SizeTo(0, 0, 0, 0);
      return true;
    default:
      break;
  }
  return false;
}

bool nsNativeBasicTheme::GetWidgetOverflow(nsDeviceContext* aContext,
                                           nsIFrame* aFrame,
                                           StyleAppearance aAppearance,
                                           nsRect* aOverflowRect) {
  nsIntMargin overflow;
  switch (aAppearance) {
    case StyleAppearance::FocusOutline:
      // 2px * one segment
      overflow.SizeTo(2, 2, 2, 2);
      break;
    case StyleAppearance::Radio:
    case StyleAppearance::Checkbox:
    case StyleAppearance::Range:
      // 2px for each outline segment, plus 1px separation, plus we paint with a
      // 1px extra offset, so 6px.
      overflow.SizeTo(6, 6, 6, 6);
      break;
    case StyleAppearance::Textarea:
    case StyleAppearance::Textfield:
    case StyleAppearance::NumberInput:
    case StyleAppearance::Listbox:
    case StyleAppearance::MenulistButton:
    case StyleAppearance::Menulist:
    case StyleAppearance::Button:
      // 2px for each segment, plus 1px separation, but we paint 1px inside
      // the border area so 4px overflow.
      overflow.SizeTo(4, 4, 4, 4);
      break;
    default:
      return false;
  }

  // TODO: This should convert from device pixels to app units, not from CSS
  // pixels. And it should take the dpi ratio into account.
  // Using CSS pixels can cause the overflow to be too small if the page is
  // zoomed out.
  aOverflowRect->Inflate(nsMargin(CSSPixel::ToAppUnits(overflow.top),
                                  CSSPixel::ToAppUnits(overflow.right),
                                  CSSPixel::ToAppUnits(overflow.bottom),
                                  CSSPixel::ToAppUnits(overflow.left)));

  return true;
}

auto nsNativeBasicTheme::GetScrollbarSizes(nsPresContext* aPresContext,
                                           StyleScrollbarWidth aWidth, Overlay)
    -> ScrollbarSizes {
  CSSCoord size = aWidth == StyleScrollbarWidth::Thin
                      ? kMinimumThinScrollbarSize
                      : kMinimumScrollbarSize;
  LayoutDeviceIntCoord s =
      (size * GetDPIRatioForScrollbarPart(aPresContext)).Rounded();
  return {s, s};
}

nscoord nsNativeBasicTheme::GetCheckboxRadioPrefSize() {
  return CSSPixel::ToAppUnits(kCheckboxRadioContentBoxSize);
}

NS_IMETHODIMP
nsNativeBasicTheme::GetMinimumWidgetSize(nsPresContext* aPresContext,
                                         nsIFrame* aFrame,
                                         StyleAppearance aAppearance,
                                         LayoutDeviceIntSize* aResult,
                                         bool* aIsOverridable) {
  DPIRatio dpiRatio = GetDPIRatio(aFrame, aAppearance);

  aResult->width = aResult->height = 0;
  *aIsOverridable = true;

  switch (aAppearance) {
    case StyleAppearance::Button:
      if (IsColorPickerButton(aFrame)) {
        aResult->height = (kMinimumColorPickerHeight * dpiRatio).Rounded();
      }
      break;
    case StyleAppearance::RangeThumb:
      aResult->SizeTo((kMinimumRangeThumbSize * dpiRatio).Rounded(),
                      (kMinimumRangeThumbSize * dpiRatio).Rounded());
      break;
    case StyleAppearance::MozMenulistArrowButton:
      aResult->width = (kMinimumDropdownArrowButtonWidth * dpiRatio).Rounded();
      break;
    case StyleAppearance::SpinnerUpbutton:
    case StyleAppearance::SpinnerDownbutton:
      aResult->width = (kMinimumSpinnerButtonWidth * dpiRatio).Rounded();
      aResult->height = (kMinimumSpinnerButtonHeight * dpiRatio).Rounded();
      break;
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonDown:
    case StyleAppearance::ScrollbarbuttonLeft:
    case StyleAppearance::ScrollbarbuttonRight:
      // For scrollbar-width:thin, we don't display the buttons.
      if (IsScrollbarWidthThin(aFrame)) {
        aResult->SizeTo(0, 0);
        break;
      }
      [[fallthrough]];
    case StyleAppearance::ScrollbarthumbVertical:
    case StyleAppearance::ScrollbarthumbHorizontal: {
      auto* style = nsLayoutUtils::StyleForScrollbar(aFrame);
      auto width = style->StyleUIReset()->mScrollbarWidth;
      auto sizes = GetScrollbarSizes(aPresContext, width, Overlay::No);
      MOZ_ASSERT(sizes.mHorizontal == sizes.mVertical);
      // TODO: for short scrollbars it could be nice if the thumb could shrink
      // under this size.
      aResult->SizeTo(sizes.mHorizontal, sizes.mHorizontal);
      break;
    }
    default:
      break;
  }

  return NS_OK;
}

nsITheme::Transparency nsNativeBasicTheme::GetWidgetTransparency(
    nsIFrame* aFrame, StyleAppearance aAppearance) {
  return eUnknownTransparency;
}

NS_IMETHODIMP
nsNativeBasicTheme::WidgetStateChanged(nsIFrame* aFrame,
                                       StyleAppearance aAppearance,
                                       nsAtom* aAttribute, bool* aShouldRepaint,
                                       const nsAttrValue* aOldValue) {
  if (!aAttribute) {
    // Hover/focus/active changed.  Always repaint.
    *aShouldRepaint = true;
  } else {
    // Check the attribute to see if it's relevant.
    // disabled, checked, dlgtype, default, etc.
    *aShouldRepaint = false;
    if ((aAttribute == nsGkAtoms::disabled) ||
        (aAttribute == nsGkAtoms::checked) ||
        (aAttribute == nsGkAtoms::selected) ||
        (aAttribute == nsGkAtoms::visuallyselected) ||
        (aAttribute == nsGkAtoms::menuactive) ||
        (aAttribute == nsGkAtoms::sortDirection) ||
        (aAttribute == nsGkAtoms::focused) ||
        (aAttribute == nsGkAtoms::_default) ||
        (aAttribute == nsGkAtoms::open) || (aAttribute == nsGkAtoms::hover)) {
      *aShouldRepaint = true;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNativeBasicTheme::ThemeChanged() { return NS_OK; }

bool nsNativeBasicTheme::WidgetAppearanceDependsOnWindowFocus(
    StyleAppearance aAppearance) {
  return IsWidgetScrollbarPart(aAppearance);
}

nsITheme::ThemeGeometryType nsNativeBasicTheme::ThemeGeometryTypeForWidget(
    nsIFrame* aFrame, StyleAppearance aAppearance) {
  return eThemeGeometryTypeUnknown;
}

bool nsNativeBasicTheme::ThemeSupportsWidget(nsPresContext* aPresContext,
                                             nsIFrame* aFrame,
                                             StyleAppearance aAppearance) {
  switch (aAppearance) {
    case StyleAppearance::Radio:
    case StyleAppearance::Checkbox:
    case StyleAppearance::FocusOutline:
    case StyleAppearance::Textarea:
    case StyleAppearance::Textfield:
    case StyleAppearance::Range:
    case StyleAppearance::RangeThumb:
    case StyleAppearance::ProgressBar:
    case StyleAppearance::Progresschunk:
    case StyleAppearance::Meter:
    case StyleAppearance::Meterchunk:
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonDown:
    case StyleAppearance::ScrollbarbuttonLeft:
    case StyleAppearance::ScrollbarbuttonRight:
    case StyleAppearance::ScrollbarthumbHorizontal:
    case StyleAppearance::ScrollbarthumbVertical:
    case StyleAppearance::ScrollbartrackHorizontal:
    case StyleAppearance::ScrollbartrackVertical:
    case StyleAppearance::ScrollbarHorizontal:
    case StyleAppearance::ScrollbarVertical:
    case StyleAppearance::Scrollcorner:
    case StyleAppearance::Button:
    case StyleAppearance::Listbox:
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton:
    case StyleAppearance::NumberInput:
    case StyleAppearance::MozMenulistArrowButton:
    case StyleAppearance::SpinnerUpbutton:
    case StyleAppearance::SpinnerDownbutton:
      return !IsWidgetStyled(aPresContext, aFrame, aAppearance);
    default:
      return false;
  }
}

bool nsNativeBasicTheme::WidgetIsContainer(StyleAppearance aAppearance) {
  switch (aAppearance) {
    case StyleAppearance::MozMenulistArrowButton:
    case StyleAppearance::Radio:
    case StyleAppearance::Checkbox:
      return false;
    default:
      return true;
  }
}

bool nsNativeBasicTheme::ThemeDrawsFocusForWidget(StyleAppearance aAppearance) {
  return true;
}

bool nsNativeBasicTheme::ThemeNeedsComboboxDropmarker() { return true; }
