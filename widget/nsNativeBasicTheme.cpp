/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeBasicTheme.h"

#include "gfxBlur.h"
#include "mozilla/dom/Document.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/gfx/Filters.h"
#include "nsCSSColorUtils.h"
#include "nsCSSRendering.h"
#include "nsLayoutUtils.h"
#include "PathHelpers.h"

using namespace mozilla;
using namespace mozilla::widget;
using namespace mozilla::gfx;

NS_IMPL_ISUPPORTS_INHERITED(nsNativeBasicTheme, nsNativeTheme, nsITheme)

/* static */
auto nsNativeBasicTheme::GetDPIRatio(nsIFrame* aFrame) -> DPIRatio {
  return DPIRatio(float(AppUnitsPerCSSPixel()) /
                  aFrame->PresContext()
                      ->DeviceContext()
                      ->AppUnitsPerDevPixelAtUnitFullZoom());
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
bool nsNativeBasicTheme::IsDateTimeTextField(nsIFrame* aFrame) {
  nsDateTimeControlFrame* dateTimeFrame = do_QueryFrame(aFrame);
  return dateTimeFrame;
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

/* static */
void nsNativeBasicTheme::GetFocusStrokeRect(DrawTarget* aDrawTarget,
                                            LayoutDeviceRect& aFocusRect,
                                            LayoutDeviceCoord aOffset,
                                            const LayoutDeviceCoord aRadius,
                                            LayoutDeviceCoord aFocusWidth,
                                            RefPtr<Path>& aOutRect) {
  RectCornerRadii radii(aRadius, aRadius, aRadius, aRadius);
  aFocusRect.y -= aOffset;
  aFocusRect.x -= aOffset;
  aFocusRect.width += 2.0f * aOffset;
  aFocusRect.height += 2.0f * aOffset;
  // Deflate the rect by half the border width, so that the middle of the
  // stroke fills exactly the area we want to fill and not more.
  LayoutDeviceRect focusRect(aFocusRect);
  focusRect.Deflate(aFocusWidth * 0.5f);
  aOutRect =
      MakePathForRoundedRect(*aDrawTarget, focusRect.ToUnknownRect(), radii);
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicTheme::ComputeCheckboxColors(
    const EventStates& aState) {
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isPressed = !isDisabled && aState.HasAllStates(NS_EVENT_STATE_HOVER |
                                                      NS_EVENT_STATE_ACTIVE);
  bool isHovered = !isDisabled && aState.HasState(NS_EVENT_STATE_HOVER);
  bool isChecked = aState.HasState(NS_EVENT_STATE_CHECKED);
  bool isIndeterminate = aState.HasState(NS_EVENT_STATE_INDETERMINATE);

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
      if (isPressed) {
        backgroundColor = borderColor = sColorAccentDarker;
      } else if (isHovered) {
        backgroundColor = borderColor = sColorAccentDark;
      } else {
        backgroundColor = borderColor = sColorAccent;
      }
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
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  return isDisabled ? sColorWhiteAlpha50 : sColorWhite;
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicTheme::ComputeRadioCheckmarkColors(
    const EventStates& aState) {
  auto [unusedColor, checkColor] = ComputeCheckboxColors(aState);
  (void)unusedColor;

  return std::make_pair(sColorWhite, checkColor);
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
    return sColorAccent;
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
    return std::make_pair(sColorAccentDark, sColorAccentDarker);
  }
  return std::make_pair(sColorAccent, sColorAccentDark);
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
      return sColorAccent;
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
  return std::make_pair(sColorAccent, sColorAccentDark);
}

std::pair<sRGBColor, sRGBColor>
nsNativeBasicTheme::ComputeProgressTrackColors() {
  return std::make_pair(sColorGrey10, sColorGrey40);
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicTheme::ComputeMeterchunkColors(
    const double aValue, const double aOptimum, const double aLow) {
  sRGBColor borderColor = sColorMeterGreen20;
  sRGBColor chunkColor = sColorMeterGreen10;

  if (aValue < aOptimum) {
    if (aValue < aLow) {
      borderColor = sColorMeterRed20;
      chunkColor = sColorMeterRed10;
    } else {
      borderColor = sColorMeterYellow20;
      chunkColor = sColorMeterYellow10;
    }
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
  std::array<sRGBColor, 3> colors = {sColorAccent, sColorWhiteAlpha80,
                                     sColorAccentLight};
  return colors;
}

sRGBColor nsNativeBasicTheme::ComputeScrollbarColor(
    const ComputedStyle& aStyle, const EventStates& aDocumentState,
    bool aIsRoot) {
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
  if (aIsRoot) {
    // Root scrollbars must be opaque.
    nscolor bg = LookAndFeel::GetColor(LookAndFeel::ColorID::WindowBackground,
                                       NS_RGB(0xff, 0xff, 0xff));
    color = NS_ComposeColors(bg, color);
  }
  return gfx::sRGBColor::FromABGR(color);
}

sRGBColor nsNativeBasicTheme::ComputeScrollbarthumbColor(
    const ComputedStyle& aStyle, const EventStates& aElementState,
    const EventStates& aDocumentState) {
  const nsStyleUI* ui = aStyle.StyleUI();
  nscolor color;
  if (ui->mScrollbarColor.IsColors()) {
    color = ui->mScrollbarColor.AsColors().thumb.CalcColor(aStyle);
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

void nsNativeBasicTheme::PaintRoundedFocusRect(DrawTarget* aDrawTarget,
                                               const LayoutDeviceRect& aRect,
                                               DPIRatio aDpiRatio,
                                               CSSCoord aRadius,
                                               CSSCoord aOffset) {
  // NOTE(emilio): If the widths or offsets here change, make sure to tweak the
  // GetWidgetOverflow path for FocusOutline.
  auto [innerColor, middleColor, outerColor] = ComputeFocusRectColors();
  LayoutDeviceRect focusRect(aRect);
  RefPtr<Path> roundedRect;
  LayoutDeviceCoord offset = aOffset * aDpiRatio;
  LayoutDeviceCoord strokeRadius((aRadius * aDpiRatio) + offset);
  LayoutDeviceCoord strokeWidth(CSSCoord(2.0f) * aDpiRatio);
  GetFocusStrokeRect(aDrawTarget, focusRect, offset, strokeRadius, strokeWidth,
                     roundedRect);
  aDrawTarget->Stroke(roundedRect, ColorPattern(ToDeviceColor(innerColor)),
                      StrokeOptions(strokeWidth));

  offset = CSSCoord(1.0f) * aDpiRatio;
  strokeRadius += offset;
  strokeWidth = CSSCoord(1.0f) * aDpiRatio;
  GetFocusStrokeRect(aDrawTarget, focusRect, offset, strokeRadius, strokeWidth,
                     roundedRect);
  aDrawTarget->Stroke(roundedRect, ColorPattern(ToDeviceColor(middleColor)),
                      StrokeOptions(strokeWidth));

  offset = CSSCoord(2.0f) * aDpiRatio;
  strokeRadius += offset;
  strokeWidth = CSSCoord(2.0f) * aDpiRatio;
  GetFocusStrokeRect(aDrawTarget, focusRect, offset, strokeRadius, strokeWidth,
                     roundedRect);
  aDrawTarget->Stroke(roundedRect, ColorPattern(ToDeviceColor(outerColor)),
                      StrokeOptions(strokeWidth));
}

void nsNativeBasicTheme::PaintRoundedRect(DrawTarget* aDrawTarget,
                                          const LayoutDeviceRect& aRect,
                                          const sRGBColor& aBackgroundColor,
                                          const sRGBColor& aBorderColor,
                                          CSSCoord aBorderWidth,
                                          RectCornerRadii aDpiAdjustedRadii,
                                          DPIRatio aDpiRatio) {
  const LayoutDeviceCoord borderWidth(aBorderWidth * aDpiRatio);

  LayoutDeviceRect rect(aRect);
  // Deflate the rect by half the border width, so that the middle of the stroke
  // fills exactly the area we want to fill and not more.
  rect.Deflate(borderWidth * 0.5f);

  RefPtr<Path> roundedRect = MakePathForRoundedRect(
      *aDrawTarget, rect.ToUnknownRect(), aDpiAdjustedRadii);

  aDrawTarget->Fill(roundedRect, ColorPattern(ToDeviceColor(aBackgroundColor)));
  aDrawTarget->Stroke(roundedRect, ColorPattern(ToDeviceColor(aBorderColor)),
                      StrokeOptions(borderWidth));
}

void nsNativeBasicTheme::PaintRoundedRectWithRadius(
    DrawTarget* aDrawTarget, const LayoutDeviceRect& aRect,
    const sRGBColor& aBackgroundColor, const sRGBColor& aBorderColor,
    CSSCoord aBorderWidth, CSSCoord aRadius, DPIRatio aDpiRatio) {
  const LayoutDeviceCoord radius(aRadius * aDpiRatio);
  RectCornerRadii radii(radius, radius, radius, radius);
  PaintRoundedRect(aDrawTarget, aRect, aBackgroundColor, aBorderColor,
                   aBorderWidth, radii, aDpiRatio);
}

void nsNativeBasicTheme::PaintCheckboxControl(DrawTarget* aDrawTarget,
                                              const LayoutDeviceRect& aRect,
                                              const EventStates& aState,
                                              DPIRatio aDpiRatio) {
  const CSSCoord borderWidth = 2.0f;
  const CSSCoord radius = 2.0f;
  auto [backgroundColor, borderColor] = ComputeCheckboxColors(aState);
  PaintRoundedRectWithRadius(aDrawTarget, aRect, backgroundColor, borderColor,
                             borderWidth, radius, aDpiRatio);

  if (aState.HasState(NS_EVENT_STATE_FOCUSRING)) {
    PaintRoundedFocusRect(aDrawTarget, aRect, aDpiRatio, radius, 3.0f);
  }
}

// Returns the right scale to cover aRect in the smaller dimension, accounting
// for aDpiRatio.
static float ScaleToWidgetRect(const LayoutDeviceRect& aRect,
                               nsNativeBasicTheme::DPIRatio aDpiRatio) {
  return std::min(aRect.width, aRect.height) / kMinimumWidgetSize;
}

void nsNativeBasicTheme::PaintCheckMark(DrawTarget* aDrawTarget,
                                        const LayoutDeviceRect& aRect,
                                        const EventStates& aState,
                                        DPIRatio aDpiRatio) {
  // Points come from the coordinates on a 14X14 unit box centered at 0,0
  const float checkPolygonX[] = {-4.5f, -1.5f, -0.5f, 5.0f, 4.75f,
                                 3.5f,  -0.5f, -1.5f, -3.5f};
  const float checkPolygonY[] = {0.5f,  4.0f, 4.0f,  -2.5f, -4.0f,
                                 -4.0f, 1.0f, 1.25f, -1.0f};
  const int32_t checkNumPoints = sizeof(checkPolygonX) / sizeof(float);
  const float scale = ScaleToWidgetRect(aRect, aDpiRatio);
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
                                                const EventStates& aState,
                                                DPIRatio aDpiRatio) {
  const CSSCoord borderWidth = 2.0f;
  const float scale = ScaleToWidgetRect(aRect, aDpiRatio);

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
  auto [backgroundColor, borderColor] = ComputeCheckboxColors(aState);

  PaintStrokedEllipse(aDrawTarget, aRect, backgroundColor, borderColor,
                      borderWidth, aDpiRatio);

  if (aState.HasState(NS_EVENT_STATE_FOCUSRING)) {
    PaintRoundedFocusRect(aDrawTarget, aRect, aDpiRatio, 2.0f, 3.0f);
  }
}

void nsNativeBasicTheme::PaintRadioCheckmark(DrawTarget* aDrawTarget,
                                             const LayoutDeviceRect& aRect,
                                             const EventStates& aState,
                                             DPIRatio aDpiRatio) {
  const CSSCoord borderWidth = 2.0f;
  const float scale = ScaleToWidgetRect(aRect, aDpiRatio);
  auto [backgroundColor, checkColor] = ComputeRadioCheckmarkColors(aState);

  LayoutDeviceRect rect(aRect);
  rect.y += borderWidth * scale;
  rect.x += borderWidth * scale;
  rect.height -= borderWidth * scale * 2;
  rect.width -= borderWidth * scale * 2;

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
    PaintRoundedFocusRect(aDrawTarget, aRect, aDpiRatio, radius, 0.0f);
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
    PaintRoundedFocusRect(aDrawTarget, aRect, aDpiRatio, radius, 0.0f);
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
    PaintRoundedFocusRect(aDrawTarget, aRect, aDpiRatio, radius, 0.0f);
  }
}

void nsNativeBasicTheme::PaintArrow(
    DrawTarget* aDrawTarget, const LayoutDeviceRect& aRect,
    const int32_t aArrowPolygonX[], const int32_t aArrowPolygonY[],
    const int32_t aArrowNumPoints, const int32_t aArrowSize,
    const sRGBColor aFillColor, DPIRatio aDpiRatio) {
  nscoord paintScale = std::min(aRect.width, aRect.height) / aArrowSize;
  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
  auto center = aRect.Center().ToUnknownPoint();
  Point p = center + Point(aArrowPolygonX[0] * paintScale,
                           aArrowPolygonY[0] * paintScale);

  builder->MoveTo(p);
  for (int32_t polyIndex = 1; polyIndex < aArrowNumPoints; polyIndex++) {
    p = center + Point(aArrowPolygonX[polyIndex] * paintScale,
                       aArrowPolygonY[polyIndex] * paintScale);
    builder->LineTo(p);
  }
  RefPtr<Path> path = builder->Finish();

  aDrawTarget->Stroke(path, ColorPattern(ToDeviceColor(aFillColor)),
                      StrokeOptions(CSSCoord(2.0f) * aDpiRatio));
}

void nsNativeBasicTheme::PaintMenulistArrowButton(nsIFrame* aFrame,
                                                  DrawTarget* aDrawTarget,
                                                  const LayoutDeviceRect& aRect,
                                                  const EventStates& aState,
                                                  DPIRatio aDpiRatio) {
  const float arrowPolygonX[] = {-3.5f, -0.5f, 0.5f,  3.5f,  3.5f,
                                 3.0f,  0.5f,  -0.5f, -3.0f, -3.5};
  const float arrowPolygonY[] = {-0.5f, 2.5f, 2.5f, -0.5f, -2.0f,
                                 -2.0f, 1.0f, 1.0f, -2.0f, -2.0f};

  const int32_t arrowNumPoints = sizeof(arrowPolygonX) / sizeof(float);
  const float scale = ScaleToWidgetRect(aRect, aDpiRatio);

  auto center = aRect.Center().ToUnknownPoint();

  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
  Point p = center + Point(arrowPolygonX[0] * scale, arrowPolygonY[0] * scale);
  builder->MoveTo(p);
  for (int32_t i = 1; i < arrowNumPoints; i++) {
    p = center + Point(arrowPolygonX[i] * scale, arrowPolygonY[i] * scale);
    builder->LineTo(p);
  }
  RefPtr<Path> path = builder->Finish();

  sRGBColor arrowColor = ComputeMenulistArrowButtonColor(aState);
  aDrawTarget->Fill(path, ColorPattern(ToDeviceColor(arrowColor)));
}

void nsNativeBasicTheme::PaintSpinnerButton(nsIFrame* aFrame,
                                            DrawTarget* aDrawTarget,
                                            const LayoutDeviceRect& aRect,
                                            const EventStates& aState,
                                            StyleAppearance aAppearance,
                                            DPIRatio aDpiRatio) {
  auto [backgroundColor, borderColor] = ComputeButtonColors(aState);

  RefPtr<Path> pathRect = MakePathForRect(*aDrawTarget, aRect.ToUnknownRect());

  aDrawTarget->Fill(pathRect, ColorPattern(ToDeviceColor(backgroundColor)));

  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
  Point p;
  if (IsFrameRTL(aFrame)) {
    p = Point(aRect.x + aRect.width - 0.5f, aRect.y);
  } else {
    p = Point(aRect.x - 0.5f, aRect.y);
  }
  builder->MoveTo(p);
  p = Point(p.x, p.y + aRect.height);
  builder->LineTo(p);
  RefPtr<Path> path = builder->Finish();

  aDrawTarget->Stroke(path, ColorPattern(ToDeviceColor(borderColor)),
                      StrokeOptions(kSpinnerBorderWidth * aDpiRatio));

  const float arrowPolygonX[] = {-3.5f, -0.5f, 0.5f,  3.5f,  3.5f,
                                 3.0f,  0.5f,  -0.5f, -3.0f, -3.5};
  float arrowPolygonY[] = {-3.5f, -0.5f, -0.5f, -3.5f, -5.0f,
                           -5.0f, -2.0f, -2.0f, -5.0f, -5.0f};

  const int32_t arrowNumPoints = sizeof(arrowPolygonX) / sizeof(float);
  const float scale = ScaleToWidgetRect(aRect, aDpiRatio);

  if (aAppearance == StyleAppearance::SpinnerUpbutton) {
    for (int32_t i = 0; i < arrowNumPoints; i++) {
      arrowPolygonY[i] *= -1;
    }
  }

  auto center = aRect.Center().ToUnknownPoint();

  builder = aDrawTarget->CreatePathBuilder();
  p = center + Point(arrowPolygonX[0] * scale, arrowPolygonY[0] * scale);
  builder->MoveTo(p);
  for (int32_t i = 1; i < arrowNumPoints; i++) {
    p = center + Point(arrowPolygonX[i] * scale, arrowPolygonY[i] * scale);
    builder->LineTo(p);
  }
  path = builder->Finish();
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
    PaintRoundedFocusRect(aDrawTarget, aRect, aDpiRatio, radius, 3.0f);
  }
}

void nsNativeBasicTheme::PaintProgressBar(DrawTarget* aDrawTarget,
                                          const LayoutDeviceRect& aRect,
                                          const EventStates& aState,
                                          DPIRatio aDpiRatio) {
  const CSSCoord borderWidth = 1.0f;
  const CSSCoord radius = 2.0f;

  LayoutDeviceRect rect(aRect);
  const LayoutDeviceCoord height = kProgressbarHeight * aDpiRatio;
  rect.y += (rect.height - height) / 2;
  rect.height = height;

  auto [trackColor, trackBorderColor] = ComputeProgressTrackColors();

  PaintRoundedRectWithRadius(aDrawTarget, rect, trackColor, trackBorderColor,
                             borderWidth, radius, aDpiRatio);
}

void nsNativeBasicTheme::PaintProgresschunk(nsIFrame* aFrame,
                                            DrawTarget* aDrawTarget,
                                            const LayoutDeviceRect& aRect,
                                            const EventStates& aState,
                                            DPIRatio aDpiRatio) {
  // TODO: vertical?
  // TODO: Address artifacts when position is between 0 and radius + border.
  // TODO: Handle indeterminate case.
  nsProgressFrame* progressFrame = do_QueryFrame(aFrame->GetParent());
  if (!progressFrame) {
    return;
  }

  const CSSCoord borderWidth = 1.0f;
  const LayoutDeviceCoord radius = CSSCoord(2.0f) * aDpiRatio;
  LayoutDeviceCoord progressEndRadius = 0.0f;

  LayoutDeviceRect rect(aRect);
  const LayoutDeviceCoord height = kProgressbarHeight * aDpiRatio;
  rect.y += (rect.height - height) / 2;
  rect.height = height;

  double position = GetProgressValue(aFrame) / GetProgressMaxValue(aFrame);
  if (rect.width - (rect.width * position) <
      (borderWidth * aDpiRatio + radius)) {
    // Round corners when the progress chunk approaches the maximum value to
    // avoid artifacts.
    progressEndRadius = radius;
  }
  RectCornerRadii radii;
  if (IsFrameRTL(aFrame)) {
    radii =
        RectCornerRadii(progressEndRadius, radius, radius, progressEndRadius);
  } else {
    radii =
        RectCornerRadii(radius, progressEndRadius, progressEndRadius, radius);
  }

  auto [progressColor, progressBorderColor] = ComputeProgressColors();

  PaintRoundedRect(aDrawTarget, rect, progressColor, progressBorderColor,
                   borderWidth, radii, aDpiRatio);
}

void nsNativeBasicTheme::PaintMeter(DrawTarget* aDrawTarget,
                                    const LayoutDeviceRect& aRect,
                                    const EventStates& aState,
                                    DPIRatio aDpiRatio) {
  const CSSCoord borderWidth = 1.0f;
  const CSSCoord radius = 5.0f;

  LayoutDeviceRect rect(aRect);
  const LayoutDeviceCoord height = kMeterHeight * aDpiRatio;
  rect.y += (rect.height - height) / 2;
  rect.height = height;

  auto [backgroundColor, borderColor] = ComputeMeterTrackColors();

  PaintRoundedRectWithRadius(aDrawTarget, rect, backgroundColor, borderColor,
                             borderWidth, radius, aDpiRatio);
}

void nsNativeBasicTheme::PaintMeterchunk(nsIFrame* aFrame,
                                         DrawTarget* aDrawTarget,
                                         const LayoutDeviceRect& aRect,
                                         const EventStates& aState,
                                         DPIRatio aDpiRatio) {
  // TODO: Address artifacts when position is between 0 and (radius + border).
  nsMeterFrame* meterFrame = do_QueryFrame(aFrame->GetParent());
  if (!meterFrame) {
    return;
  }

  const CSSCoord borderWidth = 1.0f;
  const LayoutDeviceCoord radius = CSSCoord(5.0f) * aDpiRatio;
  LayoutDeviceCoord progressEndRadius = 0.0f;

  LayoutDeviceRect rect(aRect);
  const LayoutDeviceCoord height = kMeterHeight * aDpiRatio;
  rect.y += (rect.height - height) / 2;
  rect.height = height;

  auto* meter =
      static_cast<mozilla::dom::HTMLMeterElement*>(meterFrame->GetContent());
  double value = meter->Value();
  double max = meter->Max();
  double position = value / max;
  if (rect.width - (rect.width * position) <
      (borderWidth * aDpiRatio + radius)) {
    // Round corners when the progress chunk approaches the maximum value to
    // avoid artifacts.
    progressEndRadius = radius;
  }
  RectCornerRadii radii;
  if (IsFrameRTL(aFrame)) {
    radii =
        RectCornerRadii(progressEndRadius, radius, radius, progressEndRadius);
  } else {
    radii =
        RectCornerRadii(radius, progressEndRadius, progressEndRadius, radius);
  }

  auto [chunkColor, borderColor] =
      ComputeMeterchunkColors(value, meter->Optimum(), meter->Low());

  PaintRoundedRect(aDrawTarget, rect, chunkColor, borderColor, borderWidth,
                   radii, aDpiRatio);
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
    PaintRoundedFocusRect(aDrawTarget, aRect, aDpiRatio, radius, 0.0f);
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
      ComputeScrollbarthumbColor(aStyle, aElementState, aDocumentState);
  aDrawTarget->FillRect(aRect.ToUnknownRect(),
                        ColorPattern(ToDeviceColor(thumbColor)));
}

void nsNativeBasicTheme::PaintScrollbarTrack(DrawTarget* aDrawTarget,
                                             const LayoutDeviceRect& aRect,
                                             bool aHorizontal, nsIFrame* aFrame,
                                             const ComputedStyle& aStyle,
                                             const EventStates& aDocumentState,
                                             DPIRatio aDpiRatio, bool aIsRoot) {
  // Draw nothing by default. Subclasses can override this.
}

void nsNativeBasicTheme::PaintScrollbar(DrawTarget* aDrawTarget,
                                        const LayoutDeviceRect& aRect,
                                        bool aHorizontal, nsIFrame* aFrame,
                                        const ComputedStyle& aStyle,
                                        const EventStates& aDocumentState,
                                        DPIRatio aDpiRatio, bool aIsRoot) {
  sRGBColor scrollbarColor =
      ComputeScrollbarColor(aStyle, aDocumentState, aIsRoot);
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
    aDrawTarget->Stroke(path,
                        ColorPattern(ToDeviceColor(sScrollbarBorderColor)),
                        StrokeOptions(CSSCoord(1.0f) * aDpiRatio));
  }
}

void nsNativeBasicTheme::PaintScrollCorner(DrawTarget* aDrawTarget,
                                           const LayoutDeviceRect& aRect,
                                           nsIFrame* aFrame,
                                           const ComputedStyle& aStyle,
                                           const EventStates& aDocumentState,
                                           DPIRatio aDpiRatio, bool aIsRoot) {
  sRGBColor scrollbarColor =
      ComputeScrollbarColor(aStyle, aDocumentState, aIsRoot);
  aDrawTarget->FillRect(aRect.ToUnknownRect(),
                        ColorPattern(ToDeviceColor(scrollbarColor)));
}

void nsNativeBasicTheme::PaintScrollbarbutton(DrawTarget* aDrawTarget,
                                              StyleAppearance aAppearance,
                                              const LayoutDeviceRect& aRect,
                                              const ComputedStyle& aStyle,
                                              const EventStates& aElementState,
                                              const EventStates& aDocumentState,
                                              DPIRatio aDpiRatio) {
  bool isActive = aElementState.HasState(NS_EVENT_STATE_ACTIVE);
  bool isHovered = aElementState.HasState(NS_EVENT_STATE_HOVER);

  bool hasCustomColor = aStyle.StyleUI()->mScrollbarColor.IsColors();
  sRGBColor buttonColor;
  if (hasCustomColor) {
    // When scrollbar-color is in use, use the thumb color for the button.
    buttonColor =
        ComputeScrollbarthumbColor(aStyle, aElementState, aDocumentState);
  } else if (isActive) {
    buttonColor = sScrollbarButtonActiveColor;
  } else if (!hasCustomColor && isHovered) {
    buttonColor = sScrollbarButtonHoverColor;
  } else {
    buttonColor = sScrollbarColor;
  }
  aDrawTarget->FillRect(aRect.ToUnknownRect(),
                        ColorPattern(ToDeviceColor(buttonColor)));

  // Start with Up arrow.
  int32_t arrowPolygonX[] = {3, 0, -3};
  int32_t arrowPolygonY[] = {2, -1, 2};
  const int32_t arrowNumPoints = sizeof(arrowPolygonX) / sizeof(int32_t);
  const int32_t arrowSize = 14;

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
  PaintArrow(aDrawTarget, aRect, arrowPolygonX, arrowPolygonY, arrowNumPoints,
             arrowSize, arrowColor, aDpiRatio);

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
    aDrawTarget->Stroke(path,
                        ColorPattern(ToDeviceColor(sScrollbarBorderColor)),
                        StrokeOptions(CSSCoord(1.0f) * aDpiRatio));
  }
}

// Checks whether the frame is for a root <scrollbar> or <scrollcorner>, which
// influences some platforms' scrollbar rendering.
bool nsNativeBasicTheme::IsRootScrollbar(nsIFrame* aFrame) {
  return CheckBooleanAttr(aFrame, nsGkAtoms::root_) &&
         aFrame->PresContext()->IsRootContentDocument() &&
         aFrame->GetContent() &&
         aFrame->GetContent()->IsInNamespace(kNameSpaceID_XUL);
}

NS_IMETHODIMP
nsNativeBasicTheme::DrawWidgetBackground(gfxContext* aContext, nsIFrame* aFrame,
                                         StyleAppearance aAppearance,
                                         const nsRect& aRect,
                                         const nsRect& /* aDirtyRect */) {
  DrawTarget* dt = aContext->GetDrawTarget();
  // FIXME(emilio): Why does this use AppUnitsPerDevPixel() but GetDPIRatio()
  // uses AppUnitsPerDevPixelAtUnitFullZoom()?
  const nscoord twipsPerPixel = aFrame->PresContext()->AppUnitsPerDevPixel();
  EventStates eventState = GetContentState(aFrame, aAppearance);
  EventStates docState = aFrame->GetContent()->OwnerDoc()->GetDocumentState();
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

  DPIRatio dpiRatio = GetDPIRatio(aFrame);

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
        PaintIndeterminateMark(dt, rect, eventState, dpiRatio);
      } else if (IsChecked(aFrame)) {
        PaintCheckMark(dt, rect, eventState, dpiRatio);
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
      PaintMenulistArrowButton(aFrame, dt, devPxRect, eventState, dpiRatio);
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
      PaintProgressBar(dt, devPxRect, eventState, dpiRatio);
      break;
    case StyleAppearance::Progresschunk:
      PaintProgresschunk(aFrame, dt, devPxRect, eventState, dpiRatio);
      break;
    case StyleAppearance::Meter:
      PaintMeter(dt, devPxRect, eventState, dpiRatio);
      break;
    case StyleAppearance::Meterchunk:
      PaintMeterchunk(aFrame, dt, devPxRect, eventState, dpiRatio);
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
                          dpiRatio, IsRootScrollbar(aFrame));
      break;
    }
    case StyleAppearance::ScrollbarHorizontal:
    case StyleAppearance::ScrollbarVertical: {
      bool isHorizontal = aAppearance == StyleAppearance::ScrollbarHorizontal;
      PaintScrollbar(dt, devPxRect, isHorizontal, aFrame,
                     *nsLayoutUtils::StyleForScrollbar(aFrame), docState,
                     dpiRatio, IsRootScrollbar(aFrame));
      break;
    }
    case StyleAppearance::Scrollcorner:
      PaintScrollCorner(dt, devPxRect, aFrame,
                        *nsLayoutUtils::StyleForScrollbar(aFrame), docState,
                        dpiRatio, IsRootScrollbar(aFrame));
      break;
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonDown:
    case StyleAppearance::ScrollbarbuttonLeft:
    case StyleAppearance::ScrollbarbuttonRight:
      PaintScrollbarbutton(dt, aAppearance, devPxRect,
                           *nsLayoutUtils::StyleForScrollbar(aFrame),
                           eventState, docState, dpiRatio);
      break;
    case StyleAppearance::Button:
      PaintButton(aFrame, dt, devPxRect, eventState, dpiRatio);
      break;
    case StyleAppearance::FocusOutline:
      PaintRoundedFocusRect(dt, devPxRect, dpiRatio, 0.0f, 0.0f);
      break;
    default:
      MOZ_ASSERT_UNREACHABLE(
          "Should not get here with a widget type we don't support.");
      return NS_ERROR_NOT_IMPLEMENTED;
  }

  return NS_OK;
}

/*bool
nsNativeBasicTheme::CreateWebRenderCommandsForWidget(mozilla::wr::DisplayListBuilder&
aBuilder, mozilla::wr::IpcResourceUpdateQueue& aResources, const
mozilla::layers::StackingContextHelper& aSc,
                                      mozilla::layers::RenderRootStateManager*
aManager, nsIFrame* aFrame, StyleAppearance aAppearance, const nsRect& aRect) {
}*/

LayoutDeviceIntMargin nsNativeBasicTheme::GetWidgetBorder(
    nsDeviceContext* aContext, nsIFrame* aFrame, StyleAppearance aAppearance) {
  DPIRatio dpiRatio = GetDPIRatio(aFrame);
  switch (aAppearance) {
    case StyleAppearance::Textfield:
    case StyleAppearance::Textarea:
    case StyleAppearance::NumberInput: {
      // FIXME: Do we want this margin not to be int-based? The native windows
      // theme rounds (see ScaleForDPI)...
      LayoutDeviceIntCoord w = (kTextFieldBorderWidth * dpiRatio).Rounded();
      return LayoutDeviceIntMargin(w, w, w, w);
    }
    case StyleAppearance::Listbox:
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton: {
      LayoutDeviceIntCoord w = (kMenulistBorderWidth * dpiRatio).Rounded();
      return LayoutDeviceIntMargin(w, w, w, w);
    }
    case StyleAppearance::Button: {
      LayoutDeviceIntCoord w = (kButtonBorderWidth * dpiRatio).Rounded();
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

  // Respect author padding.
  //
  // TODO(emilio): Consider just unconditionally returning false, so that the
  // default size of all elements matches other platforms and the UA stylesheet.
  if (aFrame->PresContext()->HasAuthorSpecifiedRules(
          aFrame, NS_AUTHOR_SPECIFIED_PADDING)) {
    return false;
  }

  DPIRatio dpiRatio = GetDPIRatio(aFrame);
  switch (aAppearance) {
    case StyleAppearance::Listbox:
    case StyleAppearance::Menulist:
      aResult->SizeTo(0, 0, 0, 0);
      return true;
    case StyleAppearance::NumberInput:
      *aResult = (CSSMargin(1.0f, 0, 1.0f, 4.0f) * dpiRatio).Rounded();
      if (IsFrameRTL(aFrame)) {
        std::swap(aResult->left, aResult->right);
      }
      return true;
    case StyleAppearance::Textarea:
    case StyleAppearance::MozMenulistArrowButton:
    case StyleAppearance::Menuitemtext:
    case StyleAppearance::Menuitem:
    case StyleAppearance::MenulistText:
    case StyleAppearance::MenulistButton:
      *aResult = (CSSMargin(1.0f, 4.0f, 1.0f, 4.0f) * dpiRatio).Rounded();
      return true;
    case StyleAppearance::Button:
      if (IsColorPickerButton(aFrame)) {
        *aResult = (CSSMargin(4.0f, 4.0f, 4.0f, 4.0f) * dpiRatio).Rounded();
        return true;
      }
      *aResult = (CSSMargin(1.0f, 4.0f, 1.0f, 4.0f) * dpiRatio).Rounded();
      return true;
    case StyleAppearance::Textfield:
      if (IsDateTimeTextField(aFrame)) {
        *aResult = (CSSMargin(2.0f, 3.0f, 0.0f, 3.0f) * dpiRatio).Rounded();
        return true;
      }
      *aResult = (CSSMargin(1.0f, 4.0f, 1.0f, 4.0f) * dpiRatio).Rounded();
      return true;
    default:
      return false;
  }
}

bool nsNativeBasicTheme::GetWidgetOverflow(nsDeviceContext* aContext,
                                           nsIFrame* aFrame,
                                           StyleAppearance aAppearance,
                                           nsRect* aOverflowRect) {
  nsIntMargin overflow;
  switch (aAppearance) {
    case StyleAppearance::FocusOutline:
      // 2px * each of the segments + 1 px for the separation between them.
      overflow.SizeTo(5, 5, 5, 5);
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

NS_IMETHODIMP
nsNativeBasicTheme::GetMinimumWidgetSize(nsPresContext* aPresContext,
                                         nsIFrame* aFrame,
                                         StyleAppearance aAppearance,
                                         LayoutDeviceIntSize* aResult,
                                         bool* aIsOverridable) {
  DPIRatio dpiRatio = GetDPIRatio(aFrame);

  aResult->width = aResult->height = (kMinimumWidgetSize * dpiRatio).Rounded();

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
      break;
    case StyleAppearance::ScrollbarVertical:
    case StyleAppearance::ScrollbarHorizontal:
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonDown:
    case StyleAppearance::ScrollbarbuttonLeft:
    case StyleAppearance::ScrollbarbuttonRight:
    case StyleAppearance::ScrollbarthumbVertical:
    case StyleAppearance::ScrollbarthumbHorizontal:
    case StyleAppearance::ScrollbartrackHorizontal:
    case StyleAppearance::ScrollbartrackVertical:
    case StyleAppearance::Scrollcorner: {
      ComputedStyle* style = nsLayoutUtils::StyleForScrollbar(aFrame);
      if (style->StyleUIReset()->mScrollbarWidth == StyleScrollbarWidth::Thin) {
        aResult->SizeTo((kMinimumThinScrollbarSize * dpiRatio).Rounded(),
                        (kMinimumThinScrollbarSize * dpiRatio).Rounded());
      } else {
        aResult->SizeTo((kMinimumScrollbarSize * dpiRatio).Rounded(),
                        (kMinimumScrollbarSize * dpiRatio).Rounded());
      }
      break;
    }
    default:
      break;
  }

  *aIsOverridable = true;
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
    case StyleAppearance::Menuitem:
    case StyleAppearance::Menuitemtext:
    case StyleAppearance::MenulistText:
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
