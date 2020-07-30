/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeBasicTheme.h"

#include "nsCSSRendering.h"
#include "PathHelpers.h"

using namespace mozilla::widget;

NS_IMPL_ISUPPORTS_INHERITED(nsNativeBasicTheme, nsNativeTheme, nsITheme)

/* static */
uint32_t nsNativeBasicTheme::GetDPIRatio(nsIFrame* aFrame) {
  return AppUnitsPerCSSPixel() / aFrame->PresContext()
                                     ->DeviceContext()
                                     ->AppUnitsPerDevPixelAtUnitFullZoom();
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
std::pair<sRGBColor, sRGBColor> nsNativeBasicTheme::ComputeCheckColors(
    const EventStates& aState) {
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isPressed = !isDisabled && aState.HasAllStates(NS_EVENT_STATE_HOVER |
                                                      NS_EVENT_STATE_ACTIVE);
  bool isHovered = !isDisabled && aState.HasState(NS_EVENT_STATE_HOVER);
  bool isChecked = aState.HasState(NS_EVENT_STATE_CHECKED);

  sRGBColor fillColor = sColorWhite;
  sRGBColor borderColor = sColorGrey40;
  if (isDisabled) {
    if (isChecked) {
      fillColor = borderColor = sColorGrey40Alpha50;
    } else {
      fillColor = sColorWhiteAlpha50;
      borderColor = sColorGrey40Alpha50;
    }
  } else {
    if (isChecked) {
      if (isPressed) {
        fillColor = borderColor = sColorAccentDarker;
      } else if (isHovered) {
        fillColor = borderColor = sColorAccentDark;
      } else {
        fillColor = borderColor = sColorAccent;
      }
    } else if (isPressed) {
      fillColor = sColorGrey20;
      borderColor = sColorGrey60;
    } else if (isHovered) {
      fillColor = sColorWhite;
      borderColor = sColorGrey50;
    } else {
      fillColor = sColorWhite;
      borderColor = sColorGrey40;
    }
  }

  return std::make_pair(fillColor, borderColor);
}

/* static */
Rect nsNativeBasicTheme::FixAspectRatio(const Rect& aRect) {
  // Checkbox and radio need to preserve aspect-ratio for compat.
  Rect rect(aRect);
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
                                            Rect& aFocusRect, CSSCoord aOffset,
                                            const CSSCoord aRadius,
                                            CSSCoord aFocusWidth,
                                            RefPtr<Path>& aOutRect) {
  RectCornerRadii radii(aRadius, aRadius, aRadius, aRadius);
  aFocusRect.y -= aOffset;
  aFocusRect.x -= aOffset;
  aFocusRect.width += 2.0f * aOffset;
  aFocusRect.height += 2.0f * aOffset;
  // Deflate the rect by half the border width, so that the middle of the
  // stroke fills exactly the area we want to fill and not more.
  Rect focusRect(aFocusRect);
  focusRect.Deflate(aFocusWidth * 0.5f);
  aOutRect = MakePathForRoundedRect(*aDrawTarget, focusRect, radii);
}

/* static */
void nsNativeBasicTheme::PaintRoundedFocusRect(DrawTarget* aDrawTarget,
                                               const Rect& aRect,
                                               uint32_t aDpiRatio,
                                               CSSCoord aRadius,
                                               CSSCoord aOffset) {
  Rect focusRect(aRect);
  RefPtr<Path> roundedRect;
  CSSCoord offset = aOffset * aDpiRatio;
  CSSCoord strokeRadius((aRadius * aDpiRatio) + offset);
  CSSCoord strokeWidth(2.0f * aDpiRatio);
  GetFocusStrokeRect(aDrawTarget, focusRect, offset, strokeRadius, strokeWidth,
                     roundedRect);
  aDrawTarget->Stroke(roundedRect, ColorPattern(ToDeviceColor(sColorAccent)),
                      StrokeOptions(strokeWidth));

  offset = 1.0f * aDpiRatio;
  strokeRadius += offset;
  strokeWidth = 1.0f * aDpiRatio;
  GetFocusStrokeRect(aDrawTarget, focusRect, offset, strokeRadius, strokeWidth,
                     roundedRect);
  aDrawTarget->Stroke(roundedRect,
                      ColorPattern(ToDeviceColor(sColorWhiteAlpha80)),
                      StrokeOptions(strokeWidth));

  offset = 2.0f * aDpiRatio;
  strokeRadius += offset;
  strokeWidth = 2.0f * aDpiRatio;
  GetFocusStrokeRect(aDrawTarget, focusRect, offset, strokeRadius, strokeWidth,
                     roundedRect);
  aDrawTarget->Stroke(roundedRect,
                      ColorPattern(ToDeviceColor(sColorAccentLight)),
                      StrokeOptions(strokeWidth));
}

/* static */
void nsNativeBasicTheme::PaintRoundedRect(DrawTarget* aDrawTarget,
                                          const Rect& aRect,
                                          const sRGBColor& aBackgroundColor,
                                          const sRGBColor& aBorderColor,
                                          CSSCoord aBorderWidth,
                                          RectCornerRadii aDpiAdjustedRadii,
                                          uint32_t aDpiRatio) {
  const CSSCoord borderWidth(aBorderWidth * aDpiRatio);

  Rect rect(aRect);
  // Deflate the rect by half the border width, so that the middle of the stroke
  // fills exactly the area we want to fill and not more.
  rect.Deflate(borderWidth * 0.5f);

  RefPtr<Path> roundedRect =
      MakePathForRoundedRect(*aDrawTarget, rect, aDpiAdjustedRadii);

  aDrawTarget->Fill(roundedRect, ColorPattern(ToDeviceColor(aBackgroundColor)));
  aDrawTarget->Stroke(roundedRect, ColorPattern(ToDeviceColor(aBorderColor)),
                      StrokeOptions(borderWidth));
}

/* static */
void nsNativeBasicTheme::PaintRoundedRectWithRadius(
    DrawTarget* aDrawTarget, const Rect& aRect,
    const sRGBColor& aBackgroundColor, const sRGBColor& aBorderColor,
    CSSCoord aBorderWidth, CSSCoord aRadius, uint32_t aDpiRatio) {
  const CSSCoord radius(aRadius * aDpiRatio);
  RectCornerRadii radii(radius, radius, radius, radius);
  PaintRoundedRect(aDrawTarget, aRect, aBackgroundColor, aBorderColor,
                   aBorderWidth, radii, aDpiRatio);
}

/* static */
void nsNativeBasicTheme::PaintCheckboxControl(DrawTarget* aDrawTarget,
                                              const Rect& aRect,
                                              const EventStates& aState,
                                              uint32_t aDpiRatio) {
  const CSSCoord borderWidth = 2.0f;
  const CSSCoord radius = 3.0f - (borderWidth / 2.0f);
  auto [backgroundColor, borderColor] = ComputeCheckColors(aState);
  PaintRoundedRectWithRadius(aDrawTarget, aRect, backgroundColor, borderColor,
                             borderWidth, radius, aDpiRatio);

  if (aState.HasState(NS_EVENT_STATE_FOCUS)) {
    PaintRoundedFocusRect(aDrawTarget, aRect, aDpiRatio, radius, 3.0f);
  }
}

/* static */
void nsNativeBasicTheme::PaintCheckMark(DrawTarget* aDrawTarget,
                                        const Rect& aRect,
                                        const EventStates& aState,
                                        uint32_t aDpiRatio) {
  // Points come from the coordinates on a 14X14 unit box centered at 0,0
  const float checkPolygonX[] = {-4.5f, -1.5f, -0.5f, 5.0f, 4.75f,
                                 3.5f,  -0.5f, -1.5f, -3.5f};
  const float checkPolygonY[] = {0.5f,  4.0f, 4.0f,  -2.5f, -4.0f,
                                 -4.0f, 1.0f, 1.25f, -1.0f};
  const int32_t checkNumPoints = sizeof(checkPolygonX) / sizeof(float);
  const float scale = aDpiRatio * std::min(aRect.width, aRect.height) /
                      (kMinimumWidgetSize * (float)aDpiRatio);

  auto center = aRect.Center();

  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
  Point p = center + Point(checkPolygonX[0] * scale, checkPolygonY[0] * scale);
  builder->MoveTo(p);
  for (int32_t i = 1; i < checkNumPoints; i++) {
    p = center + Point(checkPolygonX[i] * scale, checkPolygonY[i] * scale);
    builder->LineTo(p);
  }
  RefPtr<Path> path = builder->Finish();

  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  sRGBColor fillColor = isDisabled ? sColorWhiteAlpha50 : sColorWhite;
  aDrawTarget->Fill(path, ColorPattern(ToDeviceColor(fillColor)));
}

/* static */
void nsNativeBasicTheme::PaintIndeterminateMark(DrawTarget* aDrawTarget,
                                                const Rect& aRect,
                                                const EventStates& aState,
                                                uint32_t aDpiRatio) {
  const CSSCoord borderWidth = 2.0f;
  const float scale = aDpiRatio * std::min(aRect.width, aRect.height) /
                      (kMinimumWidgetSize * (float)aDpiRatio);

  Rect rect(aRect);
  rect.y += (rect.height / 2) - (borderWidth * scale / 2);
  rect.height = borderWidth * scale;
  rect.x += (borderWidth * scale) + (borderWidth * scale / 8);
  rect.width -= ((borderWidth * scale) + (borderWidth * scale / 8)) * 2;

  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  sRGBColor fillColor = isDisabled ? sColorWhiteAlpha50 : sColorWhite;
  aDrawTarget->FillRect(rect, ColorPattern(ToDeviceColor(fillColor)));
}

/* static */
void nsNativeBasicTheme::PaintStrokedEllipse(DrawTarget* aDrawTarget,
                                             const Rect& aRect,
                                             const sRGBColor& aBackgroundColor,
                                             const sRGBColor& aBorderColor,
                                             const CSSCoord aBorderWidth,
                                             uint32_t aDpiRatio) {
  const CSSCoord borderWidth(aBorderWidth * aDpiRatio);
  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();

  // Deflate for the same reason as PaintRoundedRectWithRadius. Note that the
  // size is the diameter, so we just shrink by the border width once.
  Size size(aRect.Size() - Size(borderWidth, borderWidth));
  AppendEllipseToPath(builder, aRect.Center(), size);
  RefPtr<Path> ellipse = builder->Finish();

  aDrawTarget->Fill(ellipse, ColorPattern(ToDeviceColor(aBackgroundColor)));
  aDrawTarget->Stroke(ellipse, ColorPattern(ToDeviceColor(aBorderColor)),
                      StrokeOptions(borderWidth));
}

/* static */
void nsNativeBasicTheme::PaintRadioControl(DrawTarget* aDrawTarget,
                                           const Rect& aRect,
                                           const EventStates& aState,
                                           uint32_t aDpiRatio) {
  const CSSCoord borderWidth = 2.0f;
  auto [backgroundColor, borderColor] = ComputeCheckColors(aState);

  PaintStrokedEllipse(aDrawTarget, aRect, backgroundColor, borderColor,
                      borderWidth, aDpiRatio);

  if (aState.HasState(NS_EVENT_STATE_FOCUS)) {
    PaintRoundedFocusRect(aDrawTarget, aRect, aDpiRatio, 2.0f, 3.0f);
  }
}

/* static */
void nsNativeBasicTheme::PaintRadioCheckMark(DrawTarget* aDrawTarget,
                                             const Rect& aRect,
                                             const EventStates& aState,
                                             uint32_t aDpiRatio) {
  const CSSCoord borderWidth = 2.0f;
  const float scale = aDpiRatio * std::min(aRect.width, aRect.height) /
                      (kMinimumWidgetSize * (float)aDpiRatio);
  auto [unusedColor, checkColor] = ComputeCheckColors(aState);
  (void)unusedColor;

  Rect rect(aRect);
  rect.y += borderWidth * scale;
  rect.x += borderWidth * scale;
  rect.height -= borderWidth * scale * 2;
  rect.width -= borderWidth * scale * 2;

  PaintStrokedEllipse(aDrawTarget, rect, checkColor, sColorWhite, borderWidth,
                      aDpiRatio);
}

/* static */
sRGBColor nsNativeBasicTheme::ComputeBorderColor(const EventStates& aState) {
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isActive =
      aState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE);
  bool isHovered = !isDisabled && aState.HasState(NS_EVENT_STATE_HOVER);
  bool isFocused = aState.HasState(NS_EVENT_STATE_FOCUS);
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

/* static */
void nsNativeBasicTheme::PaintTextField(DrawTarget* aDrawTarget,
                                        const Rect& aRect,
                                        const EventStates& aState,
                                        uint32_t aDpiRatio) {
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  const sRGBColor& backgroundColor =
      isDisabled ? sColorWhiteAlpha50 : sColorWhite;
  const sRGBColor borderColor = ComputeBorderColor(aState);

  const CSSCoord radius = 2.0f;

  PaintRoundedRectWithRadius(aDrawTarget, aRect, backgroundColor, borderColor,
                             kTextFieldBorderWidth, radius, aDpiRatio);

  if (aState.HasState(NS_EVENT_STATE_FOCUS)) {
    PaintRoundedFocusRect(aDrawTarget, aRect, aDpiRatio, radius, 0.0f);
  }
}

/* static */
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

sRGBColor nsNativeBasicTheme::ComputeRangeProgressBorderColor(
    const EventStates& aState) {
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isActive =
      aState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE);
  bool isHovered = !isDisabled && aState.HasState(NS_EVENT_STATE_HOVER);

  if (isDisabled) {
    return sColorGrey40Alpha50;
  }
  if (isActive || isHovered) {
    return sColorAccentDarker;
  }
  return sColorAccentDark;
}

/* static */
sRGBColor nsNativeBasicTheme::ComputeRangeTrackBorderColor(
    const EventStates& aState) {
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isActive =
      aState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE);
  bool isHovered = !isDisabled && aState.HasState(NS_EVENT_STATE_HOVER);

  if (isDisabled) {
    return sColorGrey40Alpha50;
  }
  if (isActive || isHovered) {
    return sColorGrey50;
  }
  return sColorGrey40;
}

/* static */
std::pair<sRGBColor, sRGBColor> nsNativeBasicTheme::ComputeRangeProgressColors(
    const EventStates& aState) {
  bool isActive =
      aState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE);
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isHovered = !isDisabled && aState.HasState(NS_EVENT_STATE_HOVER);

  const sRGBColor& backgroundColor = [&] {
    if (isDisabled) {
      return sColorGrey40Alpha50;
    }
    if (isActive || isHovered) {
      return sColorAccentDark;
    }
    return sColorAccent;
  }();

  const sRGBColor borderColor = ComputeRangeProgressBorderColor(aState);

  return std::make_pair(backgroundColor, borderColor);
}

/* static */
std::pair<sRGBColor, sRGBColor> nsNativeBasicTheme::ComputeRangeTrackColors(
    const EventStates& aState) {
  bool isActive =
      aState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE);
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isHovered = !isDisabled && aState.HasState(NS_EVENT_STATE_HOVER);

  const sRGBColor& backgroundColor = [&] {
    if (isDisabled) {
      return sColorGrey10Alpha50;
    }
    if (isActive || isHovered) {
      return sColorGrey20;
    }
    return sColorGrey10;
  }();

  const sRGBColor borderColor = ComputeRangeTrackBorderColor(aState);

  return std::make_pair(backgroundColor, borderColor);
}

/* static */
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

/* static */
void nsNativeBasicTheme::PaintListbox(DrawTarget* aDrawTarget,
                                      const Rect& aRect,
                                      const EventStates& aState,
                                      uint32_t aDpiRatio) {
  const CSSCoord radius = 2.0f;
  auto [unusedColor, borderColor] = ComputeButtonColors(aState);
  (void)unusedColor;

  PaintRoundedRectWithRadius(aDrawTarget, aRect, sColorWhite, borderColor,
                             kMenulistBorderWidth, radius, aDpiRatio);

  if (aState.HasState(NS_EVENT_STATE_FOCUS)) {
    PaintRoundedFocusRect(aDrawTarget, aRect, aDpiRatio, radius, 0.0f);
  }
}

/* static */
void nsNativeBasicTheme::PaintMenulist(DrawTarget* aDrawTarget,
                                       const Rect& aRect,
                                       const EventStates& aState,
                                       uint32_t aDpiRatio) {
  const CSSCoord radius = 4.0f;
  auto [backgroundColor, borderColor] = ComputeButtonColors(aState);

  PaintRoundedRectWithRadius(aDrawTarget, aRect, backgroundColor, borderColor,
                             kMenulistBorderWidth, radius, aDpiRatio);

  if (aState.HasState(NS_EVENT_STATE_FOCUS)) {
    PaintRoundedFocusRect(aDrawTarget, aRect, aDpiRatio, radius, 0.0f);
  }
}

/* static */
void nsNativeBasicTheme::PaintArrow(
    DrawTarget* aDrawTarget, const Rect& aRect, const int32_t aArrowPolygonX[],
    const int32_t aArrowPolygonY[], const int32_t aArrowNumPoints,
    const int32_t aArrowSize, const sRGBColor aFillColor, uint32_t aDpiRatio) {
  nscoord paintScale = std::min(aRect.width, aRect.height) / aArrowSize;
  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
  Point p = aRect.Center() + Point(aArrowPolygonX[0] * paintScale,
                                   aArrowPolygonY[0] * paintScale);

  builder->MoveTo(p);
  for (int32_t polyIndex = 1; polyIndex < aArrowNumPoints; polyIndex++) {
    p = aRect.Center() + Point(aArrowPolygonX[polyIndex] * paintScale,
                               aArrowPolygonY[polyIndex] * paintScale);
    builder->LineTo(p);
  }
  RefPtr<Path> path = builder->Finish();

  aDrawTarget->Stroke(path, ColorPattern(ToDeviceColor(aFillColor)),
                      StrokeOptions(2.0f * aDpiRatio));
}

/* static */
void nsNativeBasicTheme::PaintMenulistArrowButton(nsIFrame* aFrame,
                                                  DrawTarget* aDrawTarget,
                                                  const Rect& aRect,
                                                  const EventStates& aState,
                                                  uint32_t aDpiRatio) {
  const float arrowPolygonX[] = {-3.5f, -0.5f, 0.5f,  3.5f,  3.5f,
                                 3.0f,  0.5f,  -0.5f, -3.0f, -3.5};
  const float arrowPolygonY[] = {-0.5f, 2.5f, 2.5f, -0.5f, -2.0f,
                                 -2.0f, 1.0f, 1.0f, -2.0f, -2.0f};

  const int32_t arrowNumPoints = sizeof(arrowPolygonX) / sizeof(float);
  const float scale = aDpiRatio * std::min(aRect.width, aRect.height) /
                      (kMinimumWidgetSize * (float)aDpiRatio);

  auto center = aRect.Center();

  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
  Point p = center + Point(arrowPolygonX[0] * scale, arrowPolygonY[0] * scale);
  builder->MoveTo(p);
  for (int32_t i = 1; i < arrowNumPoints; i++) {
    p = center + Point(arrowPolygonX[i] * scale, arrowPolygonY[i] * scale);
    builder->LineTo(p);
  }
  RefPtr<Path> path = builder->Finish();

  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  sRGBColor fillColor = isDisabled ? sColorGrey60Alpha50 : sColorGrey60;
  aDrawTarget->Fill(path, ColorPattern(ToDeviceColor(fillColor)));
}

/* static */
void nsNativeBasicTheme::PaintSpinnerButton(nsIFrame* aFrame,
                                            DrawTarget* aDrawTarget,
                                            const Rect& aRect,
                                            const EventStates& aState,
                                            StyleAppearance aAppearance,
                                            uint32_t aDpiRatio) {
  auto [backgroundColor, borderColor] = ComputeButtonColors(aState);

  RefPtr<Path> pathRect = MakePathForRect(*aDrawTarget, aRect);

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
  const float scale = aDpiRatio * std::min(aRect.width, aRect.height) /
                      (kMinimumWidgetSize * (float)aDpiRatio);

  if (aAppearance == StyleAppearance::SpinnerUpbutton) {
    for (int32_t i = 0; i < arrowNumPoints; i++) {
      arrowPolygonY[i] *= -1;
    }
  }

  auto center = aRect.Center();

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

/* static */
void nsNativeBasicTheme::PaintRangeTrackBackground(
    nsIFrame* aFrame, DrawTarget* aDrawTarget, const Rect& aRect,
    const EventStates& aState, uint32_t aDpiRatio, bool aHorizontal) {
  nsRangeFrame* rangeFrame = do_QueryFrame(aFrame);
  if (!rangeFrame) {
    return;
  }

  Rect rect(aRect);
  const CSSCoord verticalSize = kRangeHeight * aDpiRatio;
  if (aHorizontal) {
    rect.y += (rect.height - verticalSize) / 2;
    rect.height = verticalSize;
  } else {
    rect.x += (rect.width - verticalSize) / 2;
    rect.width = verticalSize;
  }

  double progress = rangeFrame->GetValueAsFractionOfRange();
  Rect progressRect(rect);
  Rect trackRect(rect);
  if (aHorizontal) {
    progressRect.width = rect.width * progress;
    if (IsFrameRTL(aFrame)) {
      progressRect.x += rect.width - progressRect.width;
    } else {
      trackRect.x += progressRect.width;
    }
    trackRect.width -= progressRect.width;
  } else {
    progressRect.height = rect.height * progress;
    progressRect.y += rect.height - progressRect.height;
    trackRect.height -= progressRect.height;
  }

  const CSSCoord borderWidth = 1.0f;
  const CSSCoord radius = 2.0f;

  // Avoid artifacts between thumb and track when progress is approaching
  // 0.0f or 1.0f.
  if ((aHorizontal && ((progressRect.width > (radius * 2.0f)))) ||
      (!aHorizontal && ((progressRect.height > radius * 2.0f)))) {
    sRGBColor progressColor, progressBorderColor;
    std::tie(progressColor, progressBorderColor) =
        ComputeRangeProgressColors(aState);
    PaintRoundedRectWithRadius(aDrawTarget, progressRect, progressColor,
                               progressBorderColor, borderWidth, radius,
                               aDpiRatio);
  }
  if ((aHorizontal && ((trackRect.width > (radius * 2.0f)))) ||
      (!aHorizontal && (trackRect.height > (radius * 2.0f)))) {
    sRGBColor trackColor, trackBorderColor;
    std::tie(trackColor, trackBorderColor) = ComputeRangeTrackColors(aState);
    PaintRoundedRectWithRadius(aDrawTarget, trackRect, trackColor,
                               trackBorderColor, borderWidth, radius,
                               aDpiRatio);
  }

  if (aState.HasState(NS_EVENT_STATE_FOCUS)) {
    PaintRoundedFocusRect(aDrawTarget, aRect, aDpiRatio, radius, 3.0f);
  }
}

/* static */
void nsNativeBasicTheme::PaintRangeThumb(DrawTarget* aDrawTarget,
                                         const Rect& aRect,
                                         const EventStates& aState,
                                         uint32_t aDpiRatio) {
  const CSSCoord borderWidth = 2.0f;
  auto [backgroundColor, borderColor] = ComputeRangeThumbColors(aState);

  PaintStrokedEllipse(aDrawTarget, aRect, backgroundColor, borderColor,
                      borderWidth, aDpiRatio);

  // TODO: Paint thumb shadow.
}

/* static */
void nsNativeBasicTheme::PaintProgressBar(DrawTarget* aDrawTarget,
                                          const Rect& aRect,
                                          const EventStates& aState,
                                          uint32_t aDpiRatio) {
  const CSSCoord borderWidth = 1.0f;
  const CSSCoord radius = 2.0f;

  Rect rect(aRect);
  const CSSCoord height = kProgressbarHeight * aDpiRatio;
  rect.y += (rect.height - height) / 2;
  rect.height = height;

  PaintRoundedRectWithRadius(aDrawTarget, rect, sColorGrey10, sColorGrey40,
                             borderWidth, radius, aDpiRatio);
}

/* static */
void nsNativeBasicTheme::PaintProgresschunk(nsIFrame* aFrame,
                                            DrawTarget* aDrawTarget,
                                            const Rect& aRect,
                                            const EventStates& aState,
                                            uint32_t aDpiRatio) {
  // TODO: vertical?
  // TODO: Address artifacts when position is between 0 and radius + border.
  // TODO: Handle indeterminate case.
  nsProgressFrame* progressFrame = do_QueryFrame(aFrame->GetParent());
  if (!progressFrame) {
    return;
  }

  const CSSCoord borderWidth = 1.0f;
  const CSSCoord radius = 2.0f * aDpiRatio;
  CSSCoord progressEndRadius = 0.0f;

  Rect rect(aRect);
  const CSSCoord height = kProgressbarHeight * aDpiRatio;
  rect.y += (rect.height - height) / 2;
  rect.height = height;

  double position = GetProgressValue(aFrame) / GetProgressMaxValue(aFrame);
  if (rect.width - (rect.width * position) < (borderWidth + radius)) {
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

  PaintRoundedRect(aDrawTarget, rect, sColorAccent, sColorAccentDark,
                   borderWidth, radii, aDpiRatio);
}

/* static */
void nsNativeBasicTheme::PaintMeter(DrawTarget* aDrawTarget, const Rect& aRect,
                                    const EventStates& aState,
                                    uint32_t aDpiRatio) {
  const CSSCoord borderWidth = 1.0f;
  const CSSCoord radius = 5.0f;

  Rect rect(aRect);
  const CSSCoord height = kMeterHeight * aDpiRatio;
  rect.y += (rect.height - height) / 2;
  rect.height = height;

  PaintRoundedRectWithRadius(aDrawTarget, rect, sColorGrey10, sColorGrey40,
                             borderWidth, radius, aDpiRatio);
}

/* static */
void nsNativeBasicTheme::PaintMeterchunk(nsIFrame* aFrame,
                                         DrawTarget* aDrawTarget,
                                         const Rect& aRect,
                                         const EventStates& aState,
                                         uint32_t aDpiRatio) {
  // TODO: Address artifacts when position is between 0 and (radius + border).
  nsMeterFrame* meterFrame = do_QueryFrame(aFrame->GetParent());
  if (!meterFrame) {
    return;
  }

  const CSSCoord borderWidth = 1.0f;
  const CSSCoord radius = 5.0f * aDpiRatio;
  CSSCoord progressEndRadius = 0.0f;

  Rect rect(aRect);
  const CSSCoord height = kMeterHeight * aDpiRatio;
  rect.y += (rect.height - height) / 2;
  rect.height = height;

  auto* meter =
      static_cast<mozilla::dom::HTMLMeterElement*>(meterFrame->GetContent());
  double value = meter->Value();
  double max = meter->Max();
  double position = value / max;
  if (rect.width - (rect.width * position) < (borderWidth + radius)) {
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

  sRGBColor borderColor = sColorMeterGreen20;
  sRGBColor chunkColor = sColorMeterGreen10;

  double optimum = meter->Optimum();
  if (value < optimum) {
    double low = meter->Low();
    if (value < low) {
      borderColor = sColorMeterRed20;
      chunkColor = sColorMeterRed10;
    } else {
      borderColor = sColorMeterYellow20;
      chunkColor = sColorMeterYellow10;
    }
  }

  PaintRoundedRect(aDrawTarget, rect, chunkColor, borderColor, borderWidth,
                   radii, aDpiRatio);
}

void nsNativeBasicTheme::PaintButton(nsIFrame* aFrame, DrawTarget* aDrawTarget,
                                     const Rect& aRect,
                                     const EventStates& aState,
                                     uint32_t aDpiRatio) {
  const CSSCoord radius = 4.0f;
  auto [backgroundColor, borderColor] = ComputeButtonColors(aState, aFrame);

  PaintRoundedRectWithRadius(aDrawTarget, aRect, backgroundColor, borderColor,
                             kButtonBorderWidth, radius, aDpiRatio);

  if (aState.HasState(NS_EVENT_STATE_FOCUS)) {
    PaintRoundedFocusRect(aDrawTarget, aRect, aDpiRatio, radius, 0.0f);
  }
}

void nsNativeBasicTheme::PaintScrollbarthumbHorizontal(
    DrawTarget* aDrawTarget, const Rect& aRect, const EventStates& aState) {
  sRGBColor thumbColor = sScrollbarThumbColor;
  if (aState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE)) {
    thumbColor = sScrollbarThumbColorActive;
  } else if (aState.HasState(NS_EVENT_STATE_HOVER)) {
    thumbColor = sScrollbarThumbColorHover;
  }
  aDrawTarget->FillRect(aRect, ColorPattern(ToDeviceColor(thumbColor)));
}

void nsNativeBasicTheme::PaintScrollbarthumbVertical(
    DrawTarget* aDrawTarget, const Rect& aRect, const EventStates& aState) {
  sRGBColor thumbColor = sScrollbarThumbColor;
  if (aState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE)) {
    thumbColor = sScrollbarThumbColorActive;
  } else if (aState.HasState(NS_EVENT_STATE_HOVER)) {
    thumbColor = sScrollbarThumbColorHover;
  }
  aDrawTarget->FillRect(aRect, ColorPattern(ToDeviceColor(thumbColor)));
}

void nsNativeBasicTheme::PaintScrollbarHorizontal(DrawTarget* aDrawTarget,
                                                  const Rect& aRect) {
  aDrawTarget->FillRect(aRect, ColorPattern(ToDeviceColor(sScrollbarColor)));
  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
  builder->MoveTo(Point(aRect.x, aRect.y));
  builder->LineTo(Point(aRect.x + aRect.width, aRect.y));
  RefPtr<Path> path = builder->Finish();
  aDrawTarget->Stroke(path, ColorPattern(ToDeviceColor(sScrollbarBorderColor)));
}

void nsNativeBasicTheme::PaintScrollbarVerticalAndCorner(
    DrawTarget* aDrawTarget, const Rect& aRect, uint32_t aDpiRatio) {
  aDrawTarget->FillRect(aRect, ColorPattern(ToDeviceColor(sScrollbarColor)));
  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
  builder->MoveTo(Point(aRect.x, aRect.y));
  builder->LineTo(Point(aRect.x, aRect.y + aRect.height));
  RefPtr<Path> path = builder->Finish();
  aDrawTarget->Stroke(path, ColorPattern(ToDeviceColor(sScrollbarBorderColor)),
                      StrokeOptions(1.0f * aDpiRatio));
}

void nsNativeBasicTheme::PaintScrollbarbutton(DrawTarget* aDrawTarget,
                                              StyleAppearance aAppearance,
                                              const Rect& aRect,
                                              const EventStates& aState,
                                              uint32_t aDpiRatio) {
  bool isActive =
      aState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE);
  bool isHovered = aState.HasState(NS_EVENT_STATE_HOVER);

  aDrawTarget->FillRect(
      aRect, ColorPattern(
                 ToDeviceColor(isActive ? sScrollbarButtonActiveColor
                                        : isHovered ? sScrollbarButtonHoverColor
                                                    : sScrollbarColor)));

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

  PaintArrow(aDrawTarget, aRect, arrowPolygonX, arrowPolygonY, arrowNumPoints,
             arrowSize,
             isActive
                 ? sScrollbarArrowColorActive
                 : isHovered ? sScrollbarArrowColorHover : sScrollbarArrowColor,
             aDpiRatio);

  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
  builder->MoveTo(Point(aRect.x, aRect.y));
  if (aAppearance == StyleAppearance::ScrollbarbuttonUp ||
      aAppearance == StyleAppearance::ScrollbarbuttonDown) {
    builder->LineTo(Point(aRect.x, aRect.y + aRect.height));
  } else {
    builder->LineTo(Point(aRect.x + aRect.width, aRect.y));
  }

  RefPtr<Path> path = builder->Finish();
  aDrawTarget->Stroke(path, ColorPattern(ToDeviceColor(sScrollbarBorderColor)),
                      StrokeOptions(1.0f * aDpiRatio));
}

NS_IMETHODIMP
nsNativeBasicTheme::DrawWidgetBackground(gfxContext* aContext, nsIFrame* aFrame,
                                         StyleAppearance aAppearance,
                                         const nsRect& aRect,
                                         const nsRect& /* aDirtyRect */) {
  DrawTarget* dt = aContext->GetDrawTarget();
  const nscoord twipsPerPixel = aFrame->PresContext()->AppUnitsPerDevPixel();
  EventStates eventState = GetContentState(aFrame, aAppearance);
  Rect devPxRect = NSRectToSnappedRect(aRect, twipsPerPixel, *dt);

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

  uint32_t dpiRatio = GetDPIRatio(aFrame);

  switch (aAppearance) {
    case StyleAppearance::Radio: {
      auto rect = FixAspectRatio(devPxRect);
      PaintRadioControl(dt, rect, eventState, dpiRatio);
      if (IsSelected(aFrame)) {
        PaintRadioCheckMark(dt, rect, eventState, dpiRatio);
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
      PaintRangeTrackBackground(aFrame, dt, devPxRect, eventState, dpiRatio,
                                IsRangeHorizontal(aFrame));
      break;
    case StyleAppearance::RangeThumb:
      PaintRangeThumb(dt, devPxRect, eventState, dpiRatio);
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
      PaintScrollbarthumbHorizontal(dt, devPxRect, eventState);
      break;
    case StyleAppearance::ScrollbarthumbVertical:
      PaintScrollbarthumbVertical(dt, devPxRect, eventState);
      break;
    case StyleAppearance::ScrollbarHorizontal:
      PaintScrollbarHorizontal(dt, devPxRect);
      break;
    case StyleAppearance::ScrollbarVertical:
    case StyleAppearance::Scrollcorner:
      PaintScrollbarVerticalAndCorner(dt, devPxRect, dpiRatio);
      break;
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonDown:
    case StyleAppearance::ScrollbarbuttonLeft:
    case StyleAppearance::ScrollbarbuttonRight:
      PaintScrollbarbutton(dt, aAppearance, devPxRect, eventState, dpiRatio);
      break;
    case StyleAppearance::Button:
      PaintButton(aFrame, dt, devPxRect, eventState, dpiRatio);
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
  uint32_t dpiRatio = GetDPIRatio(aFrame);
  switch (aAppearance) {
    case StyleAppearance::Textfield:
    case StyleAppearance::Textarea:
    case StyleAppearance::NumberInput: {
      LayoutDeviceIntCoord w = kTextFieldBorderWidth * dpiRatio;
      return LayoutDeviceIntMargin(w, w, w, w);
    }
    case StyleAppearance::Listbox:
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton: {
      const LayoutDeviceIntCoord w = kMenulistBorderWidth * dpiRatio;
      return LayoutDeviceIntMargin(w, w, w, w);
    }
    case StyleAppearance::Button: {
      LayoutDeviceIntCoord w = kButtonBorderWidth * dpiRatio;
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

  uint32_t dpiRatio = GetDPIRatio(aFrame);
  switch (aAppearance) {
    case StyleAppearance::Listbox:
    case StyleAppearance::Menulist:
      aResult->SizeTo(0, 0, 0, 0);
      return true;
    case StyleAppearance::NumberInput:
      if (IsFrameRTL(aFrame)) {
        aResult->SizeTo(1 * dpiRatio, 4 * dpiRatio, 1 * dpiRatio, 0);
      } else {
        aResult->SizeTo(1 * dpiRatio, 0, 1 * dpiRatio, 4 * dpiRatio);
      }
      return true;
    case StyleAppearance::Textarea:
    case StyleAppearance::MozMenulistArrowButton:
    case StyleAppearance::Menuitemtext:
    case StyleAppearance::Menuitem:
    case StyleAppearance::MenulistText:
    case StyleAppearance::MenulistButton:
      aResult->SizeTo(1 * dpiRatio, 4 * dpiRatio, 1 * dpiRatio, 4 * dpiRatio);
      return true;
    case StyleAppearance::Button:
      if (IsColorPickerButton(aFrame)) {
        aResult->SizeTo(4 * dpiRatio, 4 * dpiRatio, 4 * dpiRatio, 4 * dpiRatio);
        return true;
      }
      aResult->SizeTo(1 * dpiRatio, 4 * dpiRatio, 1 * dpiRatio, 4 * dpiRatio);
      return true;
    case StyleAppearance::Textfield:
      if (IsDateTimeTextField(aFrame)) {
        aResult->SizeTo(2 * dpiRatio, 3 * dpiRatio, 0, 3 * dpiRatio);
        return true;
      }
      aResult->SizeTo(1 * dpiRatio, 4 * dpiRatio, 1 * dpiRatio, 4 * dpiRatio);
      return true;
    default:
      return false;
  }
}

bool nsNativeBasicTheme::GetWidgetOverflow(nsDeviceContext* aContext,
                                           nsIFrame* aFrame,
                                           StyleAppearance aAppearance,
                                           nsRect* aOverflowRect) {
  // TODO(bug 1620360): This should return non-zero for
  // StyleAppearance::FocusOutline, if we implement outline-style: auto.
  nsIntMargin overflow;
  switch (aAppearance) {
    case StyleAppearance::Radio:
    case StyleAppearance::Checkbox:
    case StyleAppearance::Range:
      overflow.SizeTo(6, 6, 6, 6);
      break;
    default:
      overflow.SizeTo(4, 4, 4, 4);
      break;
  }

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
  uint32_t dpiRatio = GetDPIRatio(aFrame);

  aResult->width = aResult->height =
      static_cast<uint32_t>(kMinimumWidgetSize) * dpiRatio;

  switch (aAppearance) {
    case StyleAppearance::Button:
      if (IsColorPickerButton(aFrame)) {
        aResult->height =
            static_cast<uint32_t>(kMinimumColorPickerHeight) * dpiRatio;
      }
      break;
    case StyleAppearance::RangeThumb:
      aResult->SizeTo(static_cast<uint32_t>(kMinimumRangeThumbSize) * dpiRatio,
                      static_cast<uint32_t>(kMinimumRangeThumbSize) * dpiRatio);
      break;
    case StyleAppearance::MozMenulistArrowButton:
      aResult->width =
          static_cast<uint32_t>(kMinimumDropdownArrowButtonWidth) * dpiRatio;
      break;
    case StyleAppearance::SpinnerUpbutton:
    case StyleAppearance::SpinnerDownbutton:
      aResult->width =
          static_cast<uint32_t>(kMinimumSpinnerButtonWidth) * dpiRatio;
      break;
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

bool nsNativeBasicTheme::WidgetAppearanceDependsOnWindowFocus(StyleAppearance) {
  return false;
}

nsITheme::ThemeGeometryType nsNativeBasicTheme::ThemeGeometryTypeForWidget(
    nsIFrame* aFrame, StyleAppearance aAppearance) {
  return eThemeGeometryTypeUnknown;
}

bool nsNativeBasicTheme::ThemeSupportsWidget(nsPresContext* aPresContext,
                                             nsIFrame* aFrame,
                                             StyleAppearance aAppearance) {
  if (IsWidgetScrollbarPart(aAppearance)) {
    const auto* style = nsLayoutUtils::StyleForScrollbar(aFrame);
    // We don't currently handle custom scrollbars on nsNativeBasicTheme. We
    // could, potentially.
    if (style->StyleUI()->HasCustomScrollbars() ||
        style->StyleUIReset()->mScrollbarWidth == StyleScrollbarWidth::Thin) {
      return false;
    }
  }

  switch (aAppearance) {
    case StyleAppearance::Radio:
    case StyleAppearance::Checkbox:
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
