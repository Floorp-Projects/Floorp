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
  MOZ_ASSERT(StaticPrefs::widget_non_native_use_theme_accent());
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

  if (!StaticPrefs::widget_non_native_use_theme_accent()) {
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
    return sAccentColor;
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
      return sColorGrey50Alpha50;
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

static already_AddRefed<Path> GetFocusStrokePath(
    DrawTarget* aDrawTarget, LayoutDeviceRect& aFocusRect,
    LayoutDeviceCoord aOffset, const LayoutDeviceCoord aRadius,
    LayoutDeviceCoord aFocusWidth) {
  RectCornerRadii radii(aRadius, aRadius, aRadius, aRadius);
  aFocusRect.Inflate(aOffset);

  LayoutDeviceRect focusRect(aFocusRect);
  // Deflate the rect by half the border width, so that the middle of the
  // stroke fills exactly the area we want to fill and not more.
  focusRect.Deflate(aFocusWidth * 0.5f);

  return MakePathForRoundedRect(*aDrawTarget, focusRect.ToUnknownRect(), radii);
}

static const CSSCoord kInnerFocusOutlineWidth = 2.0f;

void nsNativeBasicTheme::PaintRoundedFocusRect(DrawTarget* aDrawTarget,
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
  LayoutDeviceCoord offset = aOffset * aDpiRatio;
  LayoutDeviceCoord strokeWidth = kInnerFocusOutlineWidth * aDpiRatio;
  focusRect.Inflate(strokeWidth);

  LayoutDeviceCoord strokeRadius = aRadius * aDpiRatio;
  RefPtr<Path> roundedRect = GetFocusStrokePath(aDrawTarget, focusRect, offset,
                                                strokeRadius, strokeWidth);
  aDrawTarget->Stroke(roundedRect, ColorPattern(ToDeviceColor(innerColor)),
                      StrokeOptions(strokeWidth));

  offset = CSSCoord(1.0f) * aDpiRatio;
  strokeRadius += offset;
  strokeWidth = CSSCoord(1.0f) * aDpiRatio;
  roundedRect = GetFocusStrokePath(aDrawTarget, focusRect, offset, strokeRadius,
                                   strokeWidth);
  aDrawTarget->Stroke(roundedRect, ColorPattern(ToDeviceColor(middleColor)),
                      StrokeOptions(strokeWidth));

  offset = CSSCoord(2.0f) * aDpiRatio;
  strokeRadius += offset;
  strokeWidth = CSSCoord(2.0f) * aDpiRatio;
  roundedRect = GetFocusStrokePath(aDrawTarget, focusRect, offset, strokeRadius,
                                   strokeWidth);
  aDrawTarget->Stroke(roundedRect, ColorPattern(ToDeviceColor(outerColor)),
                      StrokeOptions(strokeWidth));
}

void nsNativeBasicTheme::PaintRoundedRectWithRadius(
    DrawTarget* aDrawTarget, const LayoutDeviceRect& aRect,
    const sRGBColor& aBackgroundColor, const sRGBColor& aBorderColor,
    CSSCoord aBorderWidth, CSSCoord aRadius, DPIRatio aDpiRatio) {
  const LayoutDeviceCoord borderWidth(SnapBorderWidth(aBorderWidth, aDpiRatio));

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

  RectCornerRadii radii(radius, radius, radius, radius);
  RefPtr<Path> roundedRect =
      MakePathForRoundedRect(*aDrawTarget, rect.ToUnknownRect(), radii);

  aDrawTarget->Fill(roundedRect, ColorPattern(ToDeviceColor(aBackgroundColor)));
  aDrawTarget->Stroke(roundedRect, ColorPattern(ToDeviceColor(aBorderColor)),
                      StrokeOptions(borderWidth));
}

void nsNativeBasicTheme::PaintCheckboxControl(DrawTarget* aDrawTarget,
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


// Returns the right scale for points in a 14x14 unit box centered at 0x0 to
// fill aRect in the smaller dimension.
static float ScaleToFillRect(const LayoutDeviceRect& aRect) {
  static constexpr float kPathPointsScale = 14.0f;
  return std::min(aRect.width, aRect.height) / kPathPointsScale;
}

void nsNativeBasicTheme::PaintCheckMark(DrawTarget* aDrawTarget,
                                        const LayoutDeviceRect& aRect,
                                        const EventStates& aState) {
  // Points come from the coordinates on a 14X14 unit box centered at 0,0
  const float checkPolygonX[] = {-4.5f, -1.5f, -0.5f, 5.0f, 4.75f,
                                 3.5f,  -0.5f, -1.5f, -3.5f};
  const float checkPolygonY[] = {0.5f,  4.0f, 4.0f,  -2.5f, -4.0f,
                                 -4.0f, 1.0f, 1.25f, -1.0f};
  const int32_t checkNumPoints = sizeof(checkPolygonX) / sizeof(float);
  const float scale = ScaleToFillRect(aRect);
  auto center = aRect.Center().ToUnknownPoint();

  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
  Point p = center + Point(checkPolygonX[0] * scale, checkPolygonY[0] * scale);
  builder->MoveTo(p);
  for (int32_t i = 1; i < checkNumPoints; i++) {
    p = center + Point(checkPolygonX[i] * scale, checkPolygonY[i] * scale);
    builder->LineTo(p);
  }
  RefPtr<Path> path = builder->Finish();

  sRGBColor fillColor = ComputeCheckmarkColor(aState);
  aDrawTarget->Fill(path, ColorPattern(ToDeviceColor(fillColor)));
}

void nsNativeBasicTheme::PaintIndeterminateMark(DrawTarget* aDrawTarget,
                                                const LayoutDeviceRect& aRect,
                                                const EventStates& aState) {
  const CSSCoord borderWidth = 2.0f;
  const float scale = ScaleToFillRect(aRect);

  Rect rect = aRect.ToUnknownRect();
  rect.y += (rect.height / 2) - (borderWidth * scale / 2);
  rect.height = borderWidth * scale;
  rect.x += (borderWidth * scale) + (borderWidth * scale / 8);
  rect.width -= ((borderWidth * scale) + (borderWidth * scale / 8)) * 2;

  sRGBColor fillColor = ComputeCheckmarkColor(aState);
  aDrawTarget->FillRect(rect, ColorPattern(ToDeviceColor(fillColor)));
}

void nsNativeBasicTheme::PaintStrokedEllipse(DrawTarget* aDrawTarget,
                                             const LayoutDeviceRect& aRect,
                                             const sRGBColor& aBackgroundColor,
                                             const sRGBColor& aBorderColor,
                                             const CSSCoord aBorderWidth,
                                             DPIRatio aDpiRatio) {
  const LayoutDeviceCoord borderWidth(aBorderWidth * aDpiRatio);
  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();

  // Deflate for the same reason as PaintRoundedRectWithRadius. Note that the
  // size is the diameter, so we just shrink by the border width once.
  auto size = aRect.Size() - LayoutDeviceSize(borderWidth, borderWidth);
  AppendEllipseToPath(builder, aRect.Center().ToUnknownPoint(),
                      size.ToUnknownSize());
  RefPtr<Path> ellipse = builder->Finish();

  aDrawTarget->Fill(ellipse, ColorPattern(ToDeviceColor(aBackgroundColor)));
  aDrawTarget->Stroke(ellipse, ColorPattern(ToDeviceColor(aBorderColor)),
                      StrokeOptions(borderWidth));
}

void nsNativeBasicTheme::PaintEllipseShadow(DrawTarget* aDrawTarget,
                                            const LayoutDeviceRect& aRect,
                                            float aShadowAlpha,
                                            const CSSPoint& aShadowOffset,
                                            CSSCoord aShadowBlurStdDev,
                                            DPIRatio aDpiRatio) {
  Float stdDev = aShadowBlurStdDev * aDpiRatio;
  Point offset = (aShadowOffset * aDpiRatio).ToUnknownPoint();

  RefPtr<FilterNode> blurFilter =
      aDrawTarget->CreateFilter(FilterType::GAUSSIAN_BLUR);
  if (!blurFilter) {
    return;
  }

  blurFilter->SetAttribute(ATT_GAUSSIAN_BLUR_STD_DEVIATION, stdDev);

  IntSize inflation =
      gfxAlphaBoxBlur::CalculateBlurRadius(gfxPoint(stdDev, stdDev));
  Rect inflatedRect = aRect.ToUnknownRect();
  inflatedRect.Inflate(inflation.width, inflation.height);
  Rect sourceRectInFilterSpace =
      inflatedRect - aRect.TopLeft().ToUnknownPoint();
  Point destinationPointOfSourceRect = inflatedRect.TopLeft() + offset;

  IntSize dtSize = RoundedToInt(aRect.Size().ToUnknownSize());
  RefPtr<DrawTarget> ellipseDT = aDrawTarget->CreateSimilarDrawTargetForFilter(
      dtSize, SurfaceFormat::A8, blurFilter, blurFilter,
      sourceRectInFilterSpace, destinationPointOfSourceRect);
  if (!ellipseDT) {
    return;
  }

  RefPtr<Path> ellipse = MakePathForEllipse(
      *ellipseDT, (aRect - aRect.TopLeft()).Center().ToUnknownPoint(),
      aRect.Size().ToUnknownSize());
  ellipseDT->Fill(ellipse,
                  ColorPattern(DeviceColor(0.0f, 0.0f, 0.0f, aShadowAlpha)));
  RefPtr<SourceSurface> ellipseSurface = ellipseDT->Snapshot();

  blurFilter->SetInput(IN_GAUSSIAN_BLUR_IN, ellipseSurface);
  aDrawTarget->DrawFilter(blurFilter, sourceRectInFilterSpace,
                          destinationPointOfSourceRect);
}

void nsNativeBasicTheme::PaintRadioControl(DrawTarget* aDrawTarget,
                                           const LayoutDeviceRect& aRect,
                                           const EventStates& aState,
                                           DPIRatio aDpiRatio) {
  const CSSCoord borderWidth = 2.0f;
  auto [backgroundColor, borderColor] =
      ComputeCheckboxColors(aState, StyleAppearance::Radio);

  PaintStrokedEllipse(aDrawTarget, aRect, backgroundColor, borderColor,
                      borderWidth, aDpiRatio);

  if (aState.HasState(NS_EVENT_STATE_FOCUSRING)) {
    PaintRoundedFocusRect(aDrawTarget, aRect, aDpiRatio, 5.0f, 1.0f);
  }
}

void nsNativeBasicTheme::PaintRadioCheckmark(DrawTarget* aDrawTarget,
                                             const LayoutDeviceRect& aRect,
                                             const EventStates& aState,
                                             DPIRatio aDpiRatio) {
  const CSSCoord borderWidth = 2.0f;
  const float scale = ScaleToFillRect(aRect);
  auto [backgroundColor, checkColor] = ComputeRadioCheckmarkColors(aState);

  LayoutDeviceRect rect(aRect);
  rect.Deflate(borderWidth * scale);

  PaintStrokedEllipse(aDrawTarget, rect, checkColor, backgroundColor,
                      borderWidth, aDpiRatio);
}

void nsNativeBasicTheme::PaintTextField(DrawTarget* aDrawTarget,
                                        const LayoutDeviceRect& aRect,
                                        const EventStates& aState,
                                        DPIRatio aDpiRatio) {
  auto [backgroundColor, borderColor] = ComputeTextfieldColors(aState);

  const CSSCoord radius = 2.0f;

  PaintRoundedRectWithRadius(aDrawTarget, aRect, backgroundColor, borderColor,
                             kTextFieldBorderWidth, radius, aDpiRatio);

  if (aState.HasState(NS_EVENT_STATE_FOCUSRING)) {
    PaintRoundedFocusRect(aDrawTarget, aRect, aDpiRatio, radius,
                          -kTextFieldBorderWidth);
  }
}

void nsNativeBasicTheme::PaintListbox(DrawTarget* aDrawTarget,
                                      const LayoutDeviceRect& aRect,
                                      const EventStates& aState,
                                      DPIRatio aDpiRatio) {
  const CSSCoord radius = 2.0f;
  auto [backgroundColor, borderColor] = ComputeTextfieldColors(aState);

  PaintRoundedRectWithRadius(aDrawTarget, aRect, backgroundColor, borderColor,
                             kMenulistBorderWidth, radius, aDpiRatio);

  if (aState.HasState(NS_EVENT_STATE_FOCUSRING)) {
    PaintRoundedFocusRect(aDrawTarget, aRect, aDpiRatio, radius,
                          -kMenulistBorderWidth);
  }
}

void nsNativeBasicTheme::PaintMenulist(DrawTarget* aDrawTarget,
                                       const LayoutDeviceRect& aRect,
                                       const EventStates& aState,
                                       DPIRatio aDpiRatio) {
  const CSSCoord radius = 4.0f;
  auto [backgroundColor, borderColor] = ComputeButtonColors(aState);

  PaintRoundedRectWithRadius(aDrawTarget, aRect, backgroundColor, borderColor,
                             kMenulistBorderWidth, radius, aDpiRatio);

  if (aState.HasState(NS_EVENT_STATE_FOCUSRING)) {
    PaintRoundedFocusRect(aDrawTarget, aRect, aDpiRatio, radius,
                          -kMenulistBorderWidth);
  }
}

void nsNativeBasicTheme::PaintArrow(DrawTarget* aDrawTarget,
                                    const LayoutDeviceRect& aRect,
                                    const float aArrowPolygonX[],
                                    const float aArrowPolygonY[],
                                    const int32_t aArrowNumPoints,
                                    const sRGBColor aFillColor) {
  const float scale = ScaleToFillRect(aRect);

  auto center = aRect.Center().ToUnknownPoint();

  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
  Point p =
      center + Point(aArrowPolygonX[0] * scale, aArrowPolygonY[0] * scale);
  builder->MoveTo(p);
  for (int32_t i = 1; i < aArrowNumPoints; i++) {
    p = center + Point(aArrowPolygonX[i] * scale, aArrowPolygonY[i] * scale);
    builder->LineTo(p);
  }
  RefPtr<Path> path = builder->Finish();

  aDrawTarget->Fill(path, ColorPattern(ToDeviceColor(aFillColor)));
}

void nsNativeBasicTheme::PaintMenulistArrowButton(nsIFrame* aFrame,
                                                  DrawTarget* aDrawTarget,
                                                  const LayoutDeviceRect& aRect,
                                                  const EventStates& aState) {
  const float arrowPolygonX[] = {-3.5f, -0.5f, 0.5f,  3.5f,  3.5f,
                                 3.0f,  0.5f,  -0.5f, -3.0f, -3.5f};
  const float arrowPolygonY[] = {-0.5f, 2.5f, 2.5f, -0.5f, -2.0f,
                                 -2.0f, 1.0f, 1.0f, -2.0f, -2.0f};
  const int32_t arrowNumPoints = ArrayLength(arrowPolygonX);
  sRGBColor arrowColor = ComputeMenulistArrowButtonColor(aState);
  PaintArrow(aDrawTarget, aRect, arrowPolygonX, arrowPolygonY, arrowNumPoints,
             arrowColor);
}

void nsNativeBasicTheme::PaintSpinnerButton(nsIFrame* aFrame,
                                            DrawTarget* aDrawTarget,
                                            const LayoutDeviceRect& aRect,
                                            const EventStates& aState,
                                            StyleAppearance aAppearance,
                                            DPIRatio aDpiRatio) {
  auto [backgroundColor, borderColor] = ComputeButtonColors(aState);

  aDrawTarget->FillRect(aRect.ToUnknownRect(),
                        ColorPattern(ToDeviceColor(backgroundColor)));

  const float arrowPolygonX[] = {-5.25f, -0.75f, 0.75f,  5.25f, 5.25f,
                                 4.5f,   0.75f,  -0.75f, -4.5f, -5.25f};
  const float arrowPolygonY[] = {-1.875f, 2.625f, 2.625f, -1.875f, -4.125f,
                                 -4.125f, 0.375f, 0.375f, -4.125f, -4.125f};
  const int32_t arrowNumPoints = ArrayLength(arrowPolygonX);
  const float scaleX = ScaleToFillRect(aRect);
  const float scaleY =
      aAppearance == StyleAppearance::SpinnerDownbutton ? scaleX : -scaleX;

  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
  auto center = aRect.Center().ToUnknownPoint();
  Point p =
      center + Point(arrowPolygonX[0] * scaleX, arrowPolygonY[0] * scaleY);
  builder->MoveTo(p);
  for (int32_t i = 1; i < arrowNumPoints; i++) {
    p = center + Point(arrowPolygonX[i] * scaleX, arrowPolygonY[i] * scaleY);
    builder->LineTo(p);
  }
  RefPtr<Path> path = builder->Finish();
  aDrawTarget->Fill(path, ColorPattern(ToDeviceColor(borderColor)));
}

void nsNativeBasicTheme::PaintRange(nsIFrame* aFrame, DrawTarget* aDrawTarget,
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
  Rect overflowRect = aRect.ToUnknownRect();
  overflowRect.Inflate(CSSCoord(6.0f) * aDpiRatio);  // See GetWidgetOverflow
  Rect progressClipRect(overflowRect);
  Rect trackClipRect(overflowRect);
  const LayoutDeviceCoord verticalSize = kRangeHeight * aDpiRatio;
  if (aHorizontal) {
    rect.height = verticalSize;
    rect.y = aRect.y + (aRect.height - rect.height) / 2;
    thumbRect.y = aRect.y + (aRect.height - thumbRect.height) / 2;

    if (IsFrameRTL(aFrame)) {
      thumbRect.x =
          aRect.x + (aRect.width - thumbRect.width) * (1.0 - progress);
      float midPoint = thumbRect.Center().X();
      trackClipRect.SetBoxX(overflowRect.X(), midPoint);
      progressClipRect.SetBoxX(midPoint, overflowRect.XMost());
    } else {
      thumbRect.x = aRect.x + (aRect.width - thumbRect.width) * progress;
      float midPoint = thumbRect.Center().X();
      progressClipRect.SetBoxX(overflowRect.X(), midPoint);
      trackClipRect.SetBoxX(midPoint, overflowRect.XMost());
    }
  } else {
    rect.width = verticalSize;
    rect.x = aRect.x + (aRect.width - rect.width) / 2;
    thumbRect.x = aRect.x + (aRect.width - thumbRect.width) / 2;

    thumbRect.y = aRect.y + (aRect.height - thumbRect.height) * progress;
    float midPoint = thumbRect.Center().Y();
    trackClipRect.SetBoxY(overflowRect.Y(), midPoint);
    progressClipRect.SetBoxY(midPoint, overflowRect.YMost());
  }

  const CSSCoord borderWidth = 1.0f;
  const CSSCoord radius = 2.0f;

  auto [progressColor, progressBorderColor] =
      ComputeRangeProgressColors(aState);
  auto [trackColor, trackBorderColor] = ComputeRangeTrackColors(aState);

  // Make a path that clips out the range thumb.
  RefPtr<PathBuilder> builder =
      aDrawTarget->CreatePathBuilder(FillRule::FILL_EVEN_ODD);
  AppendRectToPath(builder, overflowRect);
  AppendEllipseToPath(builder, thumbRect.Center().ToUnknownPoint(),
                      thumbRect.Size().ToUnknownSize());
  RefPtr<Path> path = builder->Finish();

  // Draw the progress and track pieces with the thumb clipped out, so that
  // they're not visible behind the thumb even if the thumb is partially
  // transparent (which is the case in the disabled state).
  aDrawTarget->PushClip(path);
  {
    aDrawTarget->PushClipRect(progressClipRect);
    PaintRoundedRectWithRadius(aDrawTarget, rect, progressColor,
                               progressBorderColor, borderWidth, radius,
                               aDpiRatio);
    aDrawTarget->PopClip();

    aDrawTarget->PushClipRect(trackClipRect);
    PaintRoundedRectWithRadius(aDrawTarget, rect, trackColor, trackBorderColor,
                               borderWidth, radius, aDpiRatio);
    aDrawTarget->PopClip();

    if (!aState.HasState(NS_EVENT_STATE_DISABLED)) {
      // Thumb shadow
      PaintEllipseShadow(aDrawTarget, thumbRect, 0.3f, CSSPoint(0.0f, 2.0f),
                         2.0f, aDpiRatio);
    }
  }
  aDrawTarget->PopClip();

  // Draw the thumb on top.
  const CSSCoord thumbBorderWidth = 2.0f;
  auto [thumbColor, thumbBorderColor] = ComputeRangeThumbColors(aState);

  PaintStrokedEllipse(aDrawTarget, thumbRect, thumbColor, thumbBorderColor,
                      thumbBorderWidth, aDpiRatio);

  if (aState.HasState(NS_EVENT_STATE_FOCUSRING)) {
    PaintRoundedFocusRect(aDrawTarget, aRect, aDpiRatio, radius, 1.0f);
  }
}

// TODO: Indeterminate state.
void nsNativeBasicTheme::PaintProgress(
    nsIFrame* aFrame, DrawTarget* aDrawTarget, const LayoutDeviceRect& aRect,
    const EventStates& aState, DPIRatio aDpiRatio, bool aIsMeter, bool aBar) {
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
    LayoutDeviceRect clipRect = rect;
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
    aDrawTarget->PushClipRect(clipRect.ToUnknownRect());
  }

  PaintRoundedRectWithRadius(aDrawTarget, rect, backgroundColor, borderColor,
                             borderWidth, radius, aDpiRatio);

  if (!aBar) {
    aDrawTarget->PopClip();
  }
}

void nsNativeBasicTheme::PaintButton(nsIFrame* aFrame, DrawTarget* aDrawTarget,
                                     const LayoutDeviceRect& aRect,
                                     const EventStates& aState,
                                     DPIRatio aDpiRatio) {
  const CSSCoord radius = 4.0f;
  auto [backgroundColor, borderColor] = ComputeButtonColors(aState, aFrame);

  PaintRoundedRectWithRadius(aDrawTarget, aRect, backgroundColor, borderColor,
                             kButtonBorderWidth, radius, aDpiRatio);

  if (aState.HasState(NS_EVENT_STATE_FOCUSRING)) {
    PaintRoundedFocusRect(aDrawTarget, aRect, aDpiRatio, radius,
                          -kButtonBorderWidth);
  }
}

void nsNativeBasicTheme::PaintScrollbarThumb(DrawTarget* aDrawTarget,
                                             const LayoutDeviceRect& aRect,
                                             bool aHorizontal, nsIFrame* aFrame,
                                             const ComputedStyle& aStyle,
                                             const EventStates& aElementState,
                                             const EventStates& aDocumentState,
                                             DPIRatio aDpiRatio) {
  sRGBColor thumbColor =
      ComputeScrollbarThumbColor(aFrame, aStyle, aElementState, aDocumentState);

  aDrawTarget->FillRect(aRect.ToUnknownRect(),
                        ColorPattern(ToDeviceColor(thumbColor)));
}

void nsNativeBasicTheme::PaintScrollbarTrack(DrawTarget* aDrawTarget,
                                             const LayoutDeviceRect& aRect,
                                             bool aHorizontal, nsIFrame* aFrame,
                                             const ComputedStyle& aStyle,
                                             const EventStates& aDocumentState,
                                             DPIRatio aDpiRatio) {
  // Draw nothing by default. Subclasses can override this.
}

void nsNativeBasicTheme::PaintScrollbar(DrawTarget* aDrawTarget,
                                        const LayoutDeviceRect& aRect,
                                        bool aHorizontal, nsIFrame* aFrame,
                                        const ComputedStyle& aStyle,
                                        const EventStates& aDocumentState,
                                        DPIRatio aDpiRatio) {
  auto [scrollbarColor, borderColor] =
      ComputeScrollbarColors(aFrame, aStyle, aDocumentState);
  aDrawTarget->FillRect(aRect.ToUnknownRect(),
                        ColorPattern(ToDeviceColor(scrollbarColor)));
  // FIXME(heycam): We should probably derive the border color when custom
  // scrollbar colors are in use too.  But for now, just skip painting it,
  // to avoid ugliness.
  if (aStyle.StyleUI()->mScrollbarColor.IsAuto()) {
    RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
    LayoutDeviceRect strokeRect(aRect);
    strokeRect.Deflate(CSSCoord(0.5f) * aDpiRatio);
    builder->MoveTo(strokeRect.TopLeft().ToUnknownPoint());
    builder->LineTo(
        (aHorizontal ? strokeRect.TopRight() : strokeRect.BottomLeft())
            .ToUnknownPoint());
    RefPtr<Path> path = builder->Finish();
    aDrawTarget->Stroke(path, ColorPattern(ToDeviceColor(borderColor)),
                        StrokeOptions(CSSCoord(1.0f) * aDpiRatio));
  }
}

void nsNativeBasicTheme::PaintScrollCorner(DrawTarget* aDrawTarget,
                                           const LayoutDeviceRect& aRect,
                                           nsIFrame* aFrame,
                                           const ComputedStyle& aStyle,
                                           const EventStates& aDocumentState,
                                           DPIRatio aDpiRatio) {
  auto [scrollbarColor, borderColor] =
      ComputeScrollbarColors(aFrame, aStyle, aDocumentState);
  Unused << borderColor;
  aDrawTarget->FillRect(aRect.ToUnknownRect(),
                        ColorPattern(ToDeviceColor(scrollbarColor)));
}

void nsNativeBasicTheme::PaintScrollbarButton(
    DrawTarget* aDrawTarget, StyleAppearance aAppearance,
    const LayoutDeviceRect& aRect, nsIFrame* aFrame,
    const ComputedStyle& aStyle, const EventStates& aElementState,
    const EventStates& aDocumentState, DPIRatio aDpiRatio) {
  bool hasCustomColor = aStyle.StyleUI()->mScrollbarColor.IsColors();
  auto [buttonColor, arrowColor, borderColor] = ComputeScrollbarButtonColors(
      aFrame, aAppearance, aStyle, aElementState, aDocumentState);
  aDrawTarget->FillRect(aRect.ToUnknownRect(),
                        ColorPattern(ToDeviceColor(buttonColor)));

  // Start with Up arrow.
  float arrowPolygonX[] = {-3.0f, 0.0f, 3.0f, 3.0f, 0.0f, -3.0f};
  float arrowPolygonY[] = {0.4f, -3.1f, 0.4f, 2.7f, -0.8f, 2.7f};

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
  PaintArrow(aDrawTarget, aRect, arrowPolygonX, arrowPolygonY, arrowNumPoints,
             arrowColor);

  // FIXME(heycam): We should probably derive the border color when custom
  // scrollbar colors are in use too.  But for now, just skip painting it,
  // to avoid ugliness.
  if (!hasCustomColor) {
    RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
    builder->MoveTo(Point(aRect.x, aRect.y));
    if (aAppearance == StyleAppearance::ScrollbarbuttonUp ||
        aAppearance == StyleAppearance::ScrollbarbuttonDown) {
      builder->LineTo(Point(aRect.x, aRect.y + aRect.height));
    } else {
      builder->LineTo(Point(aRect.x + aRect.width, aRect.y));
    }

    RefPtr<Path> path = builder->Finish();
    aDrawTarget->Stroke(path, ColorPattern(ToDeviceColor(borderColor)),
                        StrokeOptions(CSSCoord(1.0f) * aDpiRatio));
  }
}

NS_IMETHODIMP
nsNativeBasicTheme::DrawWidgetBackground(gfxContext* aContext, nsIFrame* aFrame,
                                         StyleAppearance aAppearance,
                                         const nsRect& aRect,
                                         const nsRect& /* aDirtyRect */) {
  DrawTarget* dt = aContext->GetDrawTarget();
  const nscoord twipsPerPixel = aFrame->PresContext()->AppUnitsPerDevPixel();
  EventStates eventState = GetContentState(aFrame, aAppearance);
  EventStates docState = aFrame->PresContext()->Document()->GetDocumentState();
  auto devPxRect = LayoutDeviceRect::FromUnknownRect(
      NSRectToSnappedRect(aRect, twipsPerPixel, *dt));

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
  if (aAppearance != StyleAppearance::FocusOutline &&
      aAppearance != StyleAppearance::Range &&
      !eventState.HasState(NS_EVENT_STATE_FOCUSRING)) {
    maybeClipRect.emplace(*dt, devPxRect);
  }

  DPIRatio dpiRatio = GetDPIRatio(aFrame, aAppearance);

  switch (aAppearance) {
    case StyleAppearance::Radio: {
      auto rect = FixAspectRatio(devPxRect);
      PaintRadioControl(dt, rect, eventState, dpiRatio);
      if (IsSelected(aFrame)) {
        PaintRadioCheckmark(dt, rect, eventState, dpiRatio);
      }
      break;
    }
    case StyleAppearance::Checkbox: {
      auto rect = FixAspectRatio(devPxRect);
      PaintCheckboxControl(dt, rect, eventState, dpiRatio);
      if (GetIndeterminate(aFrame)) {
        PaintIndeterminateMark(dt, rect, eventState);
      } else if (IsChecked(aFrame)) {
        PaintCheckMark(dt, rect, eventState);
      }
      break;
    }
    case StyleAppearance::Textarea:
    case StyleAppearance::Textfield:
    case StyleAppearance::NumberInput:
      PaintTextField(dt, devPxRect, eventState, dpiRatio);
      break;
    case StyleAppearance::Listbox:
      PaintListbox(dt, devPxRect, eventState, dpiRatio);
      break;
    case StyleAppearance::MenulistButton:
    case StyleAppearance::Menulist:
      PaintMenulist(dt, devPxRect, eventState, dpiRatio);
      break;
    case StyleAppearance::MozMenulistArrowButton:
      PaintMenulistArrowButton(aFrame, dt, devPxRect, eventState);
      break;
    case StyleAppearance::SpinnerUpbutton:
    case StyleAppearance::SpinnerDownbutton:
      PaintSpinnerButton(aFrame, dt, devPxRect, eventState, aAppearance,
                         dpiRatio);
      break;
    case StyleAppearance::Range:
      PaintRange(aFrame, dt, devPxRect, eventState, dpiRatio,
                 IsRangeHorizontal(aFrame));
      break;
    case StyleAppearance::RangeThumb:
      // Painted as part of StyleAppearance::Range.
      break;
    case StyleAppearance::ProgressBar:
      PaintProgress(aFrame, dt, devPxRect, eventState, dpiRatio,
                    /* aMeter = */ false, /* aBar = */ true);
      break;
    case StyleAppearance::Progresschunk:
      if (nsProgressFrame* f = do_QueryFrame(aFrame->GetParent())) {
        PaintProgress(f, dt, devPxRect, f->GetContent()->AsElement()->State(),
                      dpiRatio, /* aMeter = */ false, /* aBar = */ false);
      }
      break;
    case StyleAppearance::Meter:
      PaintProgress(aFrame, dt, devPxRect, eventState, dpiRatio,
                    /* aMeter = */ true, /* aBar = */ true);
      break;
    case StyleAppearance::Meterchunk:
      if (nsMeterFrame* f = do_QueryFrame(aFrame->GetParent())) {
        PaintProgress(f, dt, devPxRect, f->GetContent()->AsElement()->State(),
                      dpiRatio, /* aMeter = */ true, /* aBar = */ false);
      }
      break;
    case StyleAppearance::ScrollbarthumbHorizontal:
    case StyleAppearance::ScrollbarthumbVertical: {
      bool isHorizontal =
          aAppearance == StyleAppearance::ScrollbarthumbHorizontal;
      PaintScrollbarThumb(dt, devPxRect, isHorizontal, aFrame,
                          *nsLayoutUtils::StyleForScrollbar(aFrame), eventState,
                          docState, dpiRatio);
      break;
    }
    case StyleAppearance::ScrollbartrackHorizontal:
    case StyleAppearance::ScrollbartrackVertical: {
      bool isHorizontal =
          aAppearance == StyleAppearance::ScrollbartrackHorizontal;
      PaintScrollbarTrack(dt, devPxRect, isHorizontal, aFrame,
                          *nsLayoutUtils::StyleForScrollbar(aFrame), docState,
                          dpiRatio);
      break;
    }
    case StyleAppearance::ScrollbarHorizontal:
    case StyleAppearance::ScrollbarVertical: {
      bool isHorizontal = aAppearance == StyleAppearance::ScrollbarHorizontal;
      PaintScrollbar(dt, devPxRect, isHorizontal, aFrame,
                     *nsLayoutUtils::StyleForScrollbar(aFrame), docState,
                     dpiRatio);
      break;
    }
    case StyleAppearance::Scrollcorner:
      PaintScrollCorner(dt, devPxRect, aFrame,
                        *nsLayoutUtils::StyleForScrollbar(aFrame), docState,
                        dpiRatio);
      break;
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonDown:
    case StyleAppearance::ScrollbarbuttonLeft:
    case StyleAppearance::ScrollbarbuttonRight:
      // For scrollbar-width:thin, we don't display the buttons.
      if (!IsScrollbarWidthThin(aFrame)) {
        PaintScrollbarButton(dt, aAppearance, devPxRect, aFrame,
                             *nsLayoutUtils::StyleForScrollbar(aFrame),
                             eventState, docState, dpiRatio);
      }
      break;
    case StyleAppearance::Button:
      PaintButton(aFrame, dt, devPxRect, eventState, dpiRatio);
      break;
    case StyleAppearance::FocusOutline:
      PaintAutoStyleOutline(aFrame, dt, devPxRect, dpiRatio);
      break;
    default:
      // Various appearance values are used for XUL elements.  Normally these
      // will not be available in content documents (and thus in the content
      // processes where the native basic theme can be used), but tests are
      // run with the remote XUL pref enabled and so we can get in here.  So
      // we just return an error rather than assert.
      return NS_ERROR_NOT_IMPLEMENTED;
  }

  return NS_OK;
}

void nsNativeBasicTheme::PaintAutoStyleOutline(nsIFrame* aFrame,
                                               DrawTarget* aDt,
                                               const LayoutDeviceRect& aRect,
                                               DPIRatio aDpiRatio) {
  auto [innerColor, middleColor, outerColor] = ComputeFocusRectColors();
  Unused << middleColor;
  Unused << outerColor;

  const LayoutDeviceCoord width = kInnerFocusOutlineWidth * aDpiRatio;
  const LayoutDeviceCoord halfWidth = width * 0.5f;

  LayoutDeviceRect rect(aRect);
  // This is equivalent to Inflate(width), to paint the outline outside of
  // aRect, then Deflate(width * 0.5), to stroke at the right place.
  rect.Inflate(halfWidth);

  nscoord cssRadii[8];
  if (!aFrame->GetBorderRadii(cssRadii)) {
    return aDt->StrokeRect(rect.ToUnknownRect(),
                           ColorPattern(ToDeviceColor(innerColor)),
                           StrokeOptions(width));
  }

  nsPresContext* pc = aFrame->PresContext();
  const nscoord offset = aFrame->StyleOutline()->mOutlineOffset.ToAppUnits();
  const Float devPixelOffset = pc->AppUnitsToFloatDevPixels(offset);

  RectCornerRadii innerRadii;
  nsCSSRendering::ComputePixelRadii(cssRadii, pc->AppUnitsPerDevPixel(),
                                    &innerRadii);

  RectCornerRadii outerRadii;
  const Float widths[4] = {
      halfWidth + devPixelOffset, halfWidth + devPixelOffset,
      halfWidth + devPixelOffset, halfWidth + devPixelOffset};
  nsCSSBorderRenderer::ComputeOuterRadii(innerRadii, widths, &outerRadii);
  RefPtr<Path> path =
      MakePathForRoundedRect(*aDt, rect.ToUnknownRect(), outerRadii);
  aDt->Stroke(path, ColorPattern(ToDeviceColor(innerColor)),
              StrokeOptions(width));
}

/*bool
nsNativeBasicTheme::CreateWebRenderCommandsForWidget(mozilla::wr::DisplayListBuilder&
aBuilder, mozilla::wr::IpcResourceUpdateQueue& aResources, const
mozilla::layers::StackingContextHelper& aSc,
                                      mozilla::layers::RenderRootStateManager*
aManager, nsIFrame* aFrame, StyleAppearance aAppearance, const nsRect& aRect)
{
}*/

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

static int GetScrollbarButtonCount() {
  int32_t buttons = LookAndFeel::GetInt(LookAndFeel::IntID::ScrollArrowStyle);
  return CountPopulation32(static_cast<uint32_t>(buttons));
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
  return CSSPixel::ToAppUnits(10);
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
    case StyleAppearance::ScrollbarthumbHorizontal:
    case StyleAppearance::ScrollbarVertical:
    case StyleAppearance::ScrollbarHorizontal:
    case StyleAppearance::ScrollbartrackHorizontal:
    case StyleAppearance::ScrollbartrackVertical:
    case StyleAppearance::Scrollcorner: {
      auto* style = nsLayoutUtils::StyleForScrollbar(aFrame);
      auto width = style->StyleUIReset()->mScrollbarWidth;
      auto sizes = GetScrollbarSizes(aPresContext, width, Overlay::No);
      MOZ_ASSERT(sizes.mHorizontal == sizes.mVertical);
      aResult->SizeTo(sizes.mHorizontal, sizes.mHorizontal);
      if (width != StyleScrollbarWidth::Thin) {
        // If the scrollbar has any buttons, then we increase the minimum
        // size so that they fit too.
        //
        // FIXME(heycam): We should probably ensure that the thumb disappears
        // if a scrollbar is big enough to fit the buttons but not the thumb,
        // which is what the Windows native theme does.  If we do that, then
        // the minimum size here needs to be reduced accordingly.
        switch (aAppearance) {
          case StyleAppearance::ScrollbarHorizontal:
            aResult->width *= GetScrollbarButtonCount() + 1;
            break;
          case StyleAppearance::ScrollbarVertical:
            aResult->height *= GetScrollbarButtonCount() + 1;
            break;
          default:
            break;
        }
      }
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
