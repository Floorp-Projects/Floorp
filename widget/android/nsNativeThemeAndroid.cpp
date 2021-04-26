/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeThemeAndroid.h"

#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsCSSRendering.h"
#include "nsDateTimeControlFrame.h"
#include "nsDeviceContext.h"
#include "nsLayoutUtils.h"
#include "PathHelpers.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::widget;

namespace mozilla {
namespace widget {

static const sRGBColor sBackgroundColor(sRGBColor(1.0f, 1.0f, 1.0f));
static const sRGBColor sBackgroundActiveColor(sRGBColor(0.88f, 0.88f, 0.9f));
static const sRGBColor sBackgroundActiveColorDisabled(sRGBColor(0.88f, 0.88f,
                                                                0.9f, 0.4f));
static const sRGBColor sBorderColor(sRGBColor(0.62f, 0.62f, 0.68f));
static const sRGBColor sBorderColorDisabled(sRGBColor(0.44f, 0.44f, 0.44f,
                                                      0.4f));
static const sRGBColor sBorderHoverColor(sRGBColor(0.5f, 0.5f, 0.56f));
static const sRGBColor sBorderHoverColorDisabled(sRGBColor(0.5f, 0.5f, 0.56f,
                                                           0.4f));
static const sRGBColor sBorderFocusColor(sRGBColor(0.04f, 0.52f, 1.0f));
static const sRGBColor sCheckBackgroundColor(sRGBColor(0.18f, 0.39f, 0.89f));
static const sRGBColor sCheckBackgroundColorDisabled(sRGBColor(0.18f, 0.39f,
                                                               0.89f, 0.4f));
static const sRGBColor sCheckBackgroundHoverColor(sRGBColor(0.02f, 0.24f,
                                                            0.58f));
static const sRGBColor sCheckBackgroundHoverColorDisabled(
    sRGBColor(0.02f, 0.24f, 0.58f, 0.4f));
static const sRGBColor sCheckBackgroundActiveColor(sRGBColor(0.03f, 0.19f,
                                                             0.45f));
static const sRGBColor sCheckBackgroundActiveColorDisabled(
    sRGBColor(0.03f, 0.19f, 0.45f, 0.4f));
static const sRGBColor sDisabledColor(sRGBColor(0.89f, 0.89f, 0.89f));
static const sRGBColor sActiveColor(sRGBColor(0.47f, 0.47f, 0.48f));
static const sRGBColor sInputHoverColor(sRGBColor(0.05f, 0.05f, 0.05f, 0.5f));
static const sRGBColor sRangeInputBackgroundColor(sRGBColor(0.89f, 0.89f,
                                                            0.89f));
static const sRGBColor sButtonColor(sRGBColor(0.98f, 0.98f, 0.98f));
static const sRGBColor sButtonHoverColor(sRGBColor(0.94f, 0.94f, 0.96f));
static const sRGBColor sButtonActiveColor(sRGBColor(0.88f, 0.88f, 0.90f));
static const sRGBColor sWhiteColor(sRGBColor(1.0f, 1.0f, 1.0f, 0.0f));

static const CSSIntCoord kMinimumAndroidWidgetSize = 17;

}  // namespace widget
}  // namespace mozilla

NS_IMPL_ISUPPORTS_INHERITED(nsNativeThemeAndroid, nsNativeTheme, nsITheme)

static uint32_t GetDPIRatio(nsPresContext* aPresContext) {
  return AppUnitsPerCSSPixel() /
         aPresContext->DeviceContext()->AppUnitsPerDevPixelAtUnitFullZoom();
}

static uint32_t GetDPIRatio(nsIFrame* aFrame) {
  return GetDPIRatio(aFrame->PresContext());
}

static bool IsDateTimeResetButton(nsIFrame* aFrame) {
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

static bool IsDateTimeTextField(nsIFrame* aFrame) {
  nsDateTimeControlFrame* dateTimeFrame = do_QueryFrame(aFrame);
  return dateTimeFrame;
}

static void ComputeCheckColors(const EventStates& aState,
                               sRGBColor& aBackgroundColor,
                               sRGBColor& aBorderColor) {
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isPressed = !isDisabled && aState.HasAllStates(NS_EVENT_STATE_HOVER |
                                                      NS_EVENT_STATE_ACTIVE);
  bool isHovered = !isDisabled && aState.HasState(NS_EVENT_STATE_HOVER);
  bool isFocused = aState.HasState(NS_EVENT_STATE_FOCUS);
  bool isChecked = aState.HasState(NS_EVENT_STATE_CHECKED);

  sRGBColor fillColor = sBackgroundColor;
  sRGBColor borderColor = sBorderColor;
  if (isDisabled) {
    if (isChecked) {
      fillColor = borderColor = sCheckBackgroundColorDisabled;
    } else {
      fillColor = sBackgroundColor;
      borderColor = sBorderColorDisabled;
    }
  } else {
    if (isChecked) {
      if (isPressed) {
        fillColor = borderColor = sCheckBackgroundActiveColor;
      } else if (isHovered) {
        fillColor = borderColor = sCheckBackgroundHoverColor;
      } else {
        fillColor = borderColor = sCheckBackgroundColor;
      }
    } else if (isPressed) {
      fillColor = sBackgroundActiveColor;
      borderColor = sBorderHoverColor;
    } else if (isFocused) {
      fillColor = sBackgroundActiveColor;
      borderColor = sBorderFocusColor;
    } else if (isHovered) {
      fillColor = sBackgroundColor;
      borderColor = sBorderHoverColor;
    } else {
      fillColor = sBackgroundColor;
      borderColor = sBorderColor;
    }
  }

  aBackgroundColor = fillColor;
  aBorderColor = borderColor;
}

// Checkbox and radio need to preserve aspect-ratio for compat.
static Rect FixAspectRatio(const Rect& aRect) {
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

// This pushes and pops a clip rect to the draw target.
//
// This is done to reduce fuzz in places where we may have antialiasing, because
// skia is not clip-invariant: given different clips, it does not guarantee the
// same result, even if the painted content doesn't intersect the clips.
//
// This is a bit sad, overall, but...
struct MOZ_RAII AutoClipRect {
  AutoClipRect(DrawTarget& aDt, const Rect& aRect) : mDt(aDt) {
    mDt.PushClipRect(aRect);
  }

  ~AutoClipRect() { mDt.PopClip(); }

 private:
  DrawTarget& mDt;
};

static void PaintRoundedRectWithBorder(DrawTarget* aDrawTarget,
                                       const Rect& aRect,
                                       const sRGBColor& aBackgroundColor,
                                       const sRGBColor& aBorderColor,
                                       CSSCoord aBorderWidth, CSSCoord aRadius,
                                       uint32_t aDpi) {
  const LayoutDeviceCoord borderWidth(aBorderWidth * aDpi);
  const LayoutDeviceCoord radius(aRadius * aDpi);

  Rect rect(aRect);
  // Deflate the rect by half the border width, so that the middle of the stroke
  // fills exactly the area we want to fill and not more.
  rect.Deflate(borderWidth * 0.5f);

  RectCornerRadii radii(radius, radius, radius, radius);
  RefPtr<Path> roundedRect = MakePathForRoundedRect(*aDrawTarget, rect, radii);

  aDrawTarget->Fill(roundedRect, ColorPattern(ToDeviceColor(aBackgroundColor)));
  aDrawTarget->Stroke(roundedRect, ColorPattern(ToDeviceColor(aBorderColor)),
                      StrokeOptions(borderWidth));
}

static void PaintCheckboxControl(DrawTarget* aDrawTarget, const Rect& aRect,
                                 const EventStates& aState, uint32_t aDpi) {
  const CSSCoord kBorderWidth = 2.0f;
  const CSSCoord kRadius = 4.0f;

  sRGBColor backgroundColor;
  sRGBColor borderColor;
  ComputeCheckColors(aState, backgroundColor, borderColor);
  PaintRoundedRectWithBorder(aDrawTarget, aRect, backgroundColor, borderColor,
                             kBorderWidth, kRadius, aDpi);
}

static void PaintCheckMark(DrawTarget* aDrawTarget, const Rect& aRect,
                           const EventStates& aState, uint32_t aDpi) {
  // Points come from the coordinates on a 7X7 unit box centered at 0,0
  const float checkPolygonX[] = {-2.5, -0.7, 2.5};
  const float checkPolygonY[] = {-0.3, 1.7, -1.5};
  const int32_t checkNumPoints = sizeof(checkPolygonX) / sizeof(float);
  const int32_t checkSize = 8;

  auto center = aRect.Center();

  // Scale the checkmark based on the smallest dimension
  nscoord paintScale = std::min(aRect.width, aRect.height) / checkSize;
  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
  Point p = center +
            Point(checkPolygonX[0] * paintScale, checkPolygonY[0] * paintScale);
  builder->MoveTo(p);
  for (int32_t polyIndex = 1; polyIndex < checkNumPoints; polyIndex++) {
    p = center + Point(checkPolygonX[polyIndex] * paintScale,
                       checkPolygonY[polyIndex] * paintScale);
    builder->LineTo(p);
  }
  RefPtr<Path> path = builder->Finish();
  aDrawTarget->Stroke(path, ColorPattern(ToDeviceColor(sBackgroundColor)),
                      StrokeOptions(2.0f * aDpi));
}

static void PaintIndeterminateMark(DrawTarget* aDrawTarget, const Rect& aRect,
                                   const EventStates& aState) {
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);

  Rect rect(aRect);
  rect.y += (rect.height - rect.height / 4) / 2;
  rect.height /= 4;

  aDrawTarget->FillRect(
      rect, ColorPattern(
                ToDeviceColor(isDisabled ? sDisabledColor : sBackgroundColor)));
}

static void PaintStrokedEllipse(DrawTarget* aDrawTarget, const Rect& aRect,
                                const sRGBColor& aBackgroundColor,
                                const sRGBColor& aBorderColor,
                                const CSSCoord aBorderWidth, uint32_t aDpi) {
  const LayoutDeviceCoord borderWidth(aBorderWidth * aDpi);
  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();

  // Deflate for the same reason as PaintRoundedRectWithBorder. Note that the
  // size is the diameter, so we just shrink by the border width once.
  Size size(aRect.Size() - Size(borderWidth, borderWidth));
  AppendEllipseToPath(builder, aRect.Center(), size);
  RefPtr<Path> ellipse = builder->Finish();

  aDrawTarget->Fill(ellipse, ColorPattern(ToDeviceColor(aBackgroundColor)));
  aDrawTarget->Stroke(ellipse, ColorPattern(ToDeviceColor(aBorderColor)),
                      StrokeOptions(borderWidth));
}

static void PaintRadioControl(DrawTarget* aDrawTarget, const Rect& aRect,
                              const EventStates& aState, uint32_t aDpi) {
  const CSSCoord kBorderWidth = 2.0f;

  sRGBColor backgroundColor;
  sRGBColor borderColor;
  ComputeCheckColors(aState, backgroundColor, borderColor);

  PaintStrokedEllipse(aDrawTarget, aRect, backgroundColor, borderColor,
                      kBorderWidth, aDpi);
}

static void PaintCheckedRadioButton(DrawTarget* aDrawTarget, const Rect& aRect,
                                    uint32_t aDpi) {
  Rect rect(aRect);
  rect.x += 4.5f * aDpi;
  rect.width -= 9.0f * aDpi;
  rect.y += 4.5f * aDpi;
  rect.height -= 9.0f * aDpi;

  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
  AppendEllipseToPath(builder, rect.Center(), rect.Size());
  RefPtr<Path> ellipse = builder->Finish();
  aDrawTarget->Fill(ellipse, ColorPattern(ToDeviceColor(sBackgroundColor)));
}

static sRGBColor ComputeBorderColor(const EventStates& aState) {
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isHovered = !isDisabled && aState.HasState(NS_EVENT_STATE_HOVER);
  bool isFocused = aState.HasState(NS_EVENT_STATE_FOCUS);
  if (isFocused) {
    return sBorderFocusColor;
  }
  if (isHovered) {
    return sBorderHoverColor;
  }
  return sBorderColor;
}

static void PaintTextField(DrawTarget* aDrawTarget, const Rect& aRect,
                           const EventStates& aState, uint32_t aDpi) {
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  const sRGBColor& backgroundColor =
      isDisabled ? sDisabledColor : sBackgroundColor;
  const sRGBColor borderColor = ComputeBorderColor(aState);

  const CSSCoord kRadius = 4.0f;

  PaintRoundedRectWithBorder(aDrawTarget, aRect, backgroundColor, borderColor,
                             kTextFieldBorderWidth, kRadius, aDpi);
}

std::pair<sRGBColor, sRGBColor> ComputeButtonColors(
    const EventStates& aState, bool aIsDatetimeResetButton = false) {
  bool isActive =
      aState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE);
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isHovered = !isDisabled && aState.HasState(NS_EVENT_STATE_HOVER);

  const sRGBColor& backgroundColor = [&] {
    if (isDisabled) {
      return sDisabledColor;
    }
    if (aIsDatetimeResetButton) {
      return sWhiteColor;
    }
    if (isActive) {
      return sButtonActiveColor;
    }
    if (isHovered) {
      return sButtonHoverColor;
    }
    return sButtonColor;
  }();

  const sRGBColor borderColor = ComputeBorderColor(aState);

  return std::make_pair(backgroundColor, borderColor);
}

static void PaintMenulist(DrawTarget* aDrawTarget, const Rect& aRect,
                          const EventStates& aState, uint32_t aDpi) {
  const CSSCoord kRadius = 4.0f;

  sRGBColor backgroundColor, borderColor;
  std::tie(backgroundColor, borderColor) = ComputeButtonColors(aState);

  PaintRoundedRectWithBorder(aDrawTarget, aRect, backgroundColor, borderColor,
                             kMenulistBorderWidth, kRadius, aDpi);
}

static void PaintArrow(DrawTarget* aDrawTarget, const Rect& aRect,
                       const int32_t aArrowPolygonX[],
                       const int32_t aArrowPolygonY[],
                       const int32_t aArrowNumPoints, const int32_t aArrowSize,
                       const sRGBColor aFillColor, uint32_t aDpi) {
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
                      StrokeOptions(2.0f * aDpi));
}

static void PaintMenulistArrowButton(nsIFrame* aFrame, DrawTarget* aDrawTarget,
                                     const Rect& aRect,
                                     const EventStates& aState, uint32_t aDpi) {
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isPressed = !isDisabled && aState.HasAllStates(NS_EVENT_STATE_HOVER |
                                                      NS_EVENT_STATE_ACTIVE);
  bool isHovered = !isDisabled && aState.HasState(NS_EVENT_STATE_HOVER);
  bool isHTML = nsNativeTheme::IsHTMLContent(aFrame);

  if (!isHTML && nsNativeTheme::CheckBooleanAttr(aFrame, nsGkAtoms::open)) {
    isHovered = false;
  }

  const int32_t arrowSize = 8;
  int32_t arrowPolygonX[] = {-4, -2, 0};
  int32_t arrowPolygonY[] = {-1, 1, -1};
  const int32_t arrowNumPoints = ArrayLength(arrowPolygonX);

  PaintArrow(aDrawTarget, aRect, arrowPolygonX, arrowPolygonY, arrowNumPoints,
             arrowSize,
             isPressed   ? sActiveColor
             : isHovered ? sBorderHoverColor
                         : sBorderColor,
             aDpi);
}

static void PaintSpinnerButton(DrawTarget* aDrawTarget, const Rect& aRect,
                               const EventStates& aState,
                               StyleAppearance aAppearance, uint32_t aDpi) {
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isPressed = !isDisabled && aState.HasAllStates(NS_EVENT_STATE_HOVER |
                                                      NS_EVENT_STATE_ACTIVE);
  bool isHovered = !isDisabled && aState.HasState(NS_EVENT_STATE_HOVER);

  const int32_t arrowSize = 8;
  int32_t arrowPolygonX[] = {0, 2, 4};
  int32_t arrowPolygonY[] = {-3, -1, -3};
  const int32_t arrowNumPoints = ArrayLength(arrowPolygonX);

  if (aAppearance == StyleAppearance::SpinnerUpbutton) {
    for (int32_t i = 0; i < arrowNumPoints; i++) {
      arrowPolygonY[i] *= -1;
    }
  }

  PaintArrow(aDrawTarget, aRect, arrowPolygonX, arrowPolygonY, arrowNumPoints,
             arrowSize,
             isPressed   ? sActiveColor
             : isHovered ? sBorderHoverColor
                         : sBorderColor,
             aDpi);
}

static void PaintRangeInputBackground(DrawTarget* aDrawTarget,
                                      const Rect& aRect,
                                      const EventStates& aState, uint32_t aDpi,
                                      bool aHorizontal) {
  Rect rect(aRect);
  const LayoutDeviceCoord kVerticalSize =
      kMinimumAndroidWidgetSize * 0.25f * aDpi;

  if (aHorizontal) {
    rect.y += (rect.height - kVerticalSize) / 2;
    rect.height = kVerticalSize;
  } else {
    rect.x += (rect.width - kVerticalSize) / 2;
    rect.width = kVerticalSize;
  }

  aDrawTarget->FillRect(
      rect, ColorPattern(ToDeviceColor(sRangeInputBackgroundColor)));
  aDrawTarget->StrokeRect(rect,
                          ColorPattern(ToDeviceColor(sButtonActiveColor)));
}

static void PaintButton(nsIFrame* aFrame, DrawTarget* aDrawTarget,
                        const Rect& aRect, const EventStates& aState,
                        uint32_t aDpi) {
  const CSSCoord kRadius = 4.0f;

  // FIXME: The DateTimeResetButton bit feels like a bit of a hack.
  sRGBColor backgroundColor, borderColor;
  std::tie(backgroundColor, borderColor) =
      ComputeButtonColors(aState, IsDateTimeResetButton(aFrame));

  PaintRoundedRectWithBorder(aDrawTarget, aRect, backgroundColor, borderColor,
                             kButtonBorderWidth, kRadius, aDpi);
}

static void PaintRangeThumb(DrawTarget* aDrawTarget, const Rect& aRect,
                            const EventStates& aState, uint32_t aDpi) {
  const CSSCoord kBorderWidth = 2.0f;

  sRGBColor backgroundColor, borderColor;
  std::tie(backgroundColor, borderColor) = ComputeButtonColors(aState);

  PaintStrokedEllipse(aDrawTarget, aRect, backgroundColor, borderColor,
                      kBorderWidth, aDpi);
}

NS_IMETHODIMP
nsNativeThemeAndroid::DrawWidgetBackground(
    gfxContext* aContext, nsIFrame* aFrame, StyleAppearance aAppearance,
    const nsRect& aRect, const nsRect& /* aDirtyRect */, DrawOverflow) {
  DrawTarget* dt = aContext->GetDrawTarget();
  const nscoord twipsPerPixel = aFrame->PresContext()->AppUnitsPerDevPixel();
  EventStates eventState = GetContentState(aFrame, aAppearance);

  Rect devPxRect = NSRectToSnappedRect(aRect, twipsPerPixel, *dt);
  AutoClipRect clip(*dt, devPxRect);

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

  uint32_t dpi = GetDPIRatio(aFrame);

  switch (aAppearance) {
    case StyleAppearance::Radio: {
      auto rect = FixAspectRatio(devPxRect);
      PaintRadioControl(dt, rect, eventState, dpi);
      if (IsSelected(aFrame)) {
        PaintCheckedRadioButton(dt, rect, dpi);
      }
      break;
    }
    case StyleAppearance::Checkbox: {
      auto rect = FixAspectRatio(devPxRect);
      PaintCheckboxControl(dt, rect, eventState, dpi);
      if (IsChecked(aFrame)) {
        PaintCheckMark(dt, rect, eventState, dpi);
      }
      if (GetIndeterminate(aFrame)) {
        PaintIndeterminateMark(dt, rect, eventState);
      }
      break;
    }
    case StyleAppearance::Textarea:
    case StyleAppearance::Textfield:
    case StyleAppearance::NumberInput:
      PaintTextField(dt, devPxRect, eventState, dpi);
      break;
    case StyleAppearance::Listbox:
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton:
      PaintMenulist(dt, devPxRect, eventState, dpi);
      break;
    case StyleAppearance::MozMenulistArrowButton:
      PaintMenulistArrowButton(aFrame, dt, devPxRect, eventState, dpi);
      break;
    case StyleAppearance::SpinnerUpbutton:
    case StyleAppearance::SpinnerDownbutton:
      PaintSpinnerButton(dt, devPxRect, eventState, aAppearance, dpi);
      break;
    case StyleAppearance::Range:
      PaintRangeInputBackground(dt, devPxRect, eventState, dpi,
                                IsRangeHorizontal(aFrame));
      break;
    case StyleAppearance::RangeThumb:
      // TODO(emilio): Do we want to enforce it being a circle using
      // FixAspectRatio here? For now let authors tweak, it's a custom pseudo so
      // it doesn't probably have much compat impact if at all.
      PaintRangeThumb(dt, devPxRect, eventState, dpi);
      break;
    case StyleAppearance::Button:
      PaintButton(aFrame, dt, devPxRect, eventState, dpi);
      break;
    default:
      MOZ_ASSERT_UNREACHABLE(
          "Should not get here with a widget type we don't support.");
      return NS_ERROR_NOT_IMPLEMENTED;
  }

  return NS_OK;
}

/*bool
nsNativeThemeAndroid::CreateWebRenderCommandsForWidget(mozilla::wr::DisplayListBuilder&
aBuilder, mozilla::wr::IpcResourceUpdateQueue& aResources, const
mozilla::layers::StackingContextHelper& aSc,
                                      mozilla::layers::RenderRootStateManager*
aManager, nsIFrame* aFrame, StyleAppearance aAppearance, const nsRect& aRect) {
}*/

LayoutDeviceIntMargin nsNativeThemeAndroid::GetWidgetBorder(
    nsDeviceContext* aContext, nsIFrame* aFrame, StyleAppearance aAppearance) {
  uint32_t dpi = GetDPIRatio(aFrame);
  switch (aAppearance) {
    case StyleAppearance::Textfield:
    case StyleAppearance::Textarea:
    case StyleAppearance::NumberInput: {
      const LayoutDeviceIntCoord w = kTextFieldBorderWidth * dpi;
      return LayoutDeviceIntMargin(w, w, w, w);
    }
    case StyleAppearance::Listbox:
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton: {
      const LayoutDeviceIntCoord w = kMenulistBorderWidth * dpi;
      return LayoutDeviceIntMargin(w, w, w, w);
    }
    case StyleAppearance::Button: {
      const LayoutDeviceIntCoord w = kButtonBorderWidth * dpi;
      return LayoutDeviceIntMargin(w, w, w, w);
    }
    default:
      return LayoutDeviceIntMargin();
  }
}

bool nsNativeThemeAndroid::GetWidgetPadding(nsDeviceContext* aContext,
                                            nsIFrame* aFrame,
                                            StyleAppearance aAppearance,
                                            LayoutDeviceIntMargin* aResult) {
  uint32_t dpiRatio = GetDPIRatio(aFrame);
  switch (aAppearance) {
    // Radios and checkboxes return a fixed size in GetMinimumWidgetSize
    // and have a meaningful baseline, so they can't have
    // author-specified padding.
    case StyleAppearance::Radio:
    case StyleAppearance::Checkbox:
      aResult->SizeTo(0, 0, 0, 0);
      return true;
    case StyleAppearance::MozMenulistArrowButton:
      aResult->SizeTo(1 * dpiRatio, 4 * dpiRatio, 1 * dpiRatio, 4 * dpiRatio);
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

  uint32_t dpi = GetDPIRatio(aFrame);
  switch (aAppearance) {
    case StyleAppearance::Textarea:
    case StyleAppearance::Listbox:
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton:
    case StyleAppearance::NumberInput:
      aResult->SizeTo(6 * dpi, 7 * dpi, 6 * dpi, 7 * dpi);
      return true;
    case StyleAppearance::Button:
      aResult->SizeTo(6 * dpi, 21 * dpi, 6 * dpi, 21 * dpi);
      return true;
    case StyleAppearance::Textfield:
      if (IsDateTimeTextField(aFrame)) {
        aResult->SizeTo(7 * dpi, 7 * dpi, 5 * dpi, 7 * dpi);
        return true;
      }
      aResult->SizeTo(6 * dpi, 7 * dpi, 6 * dpi, 7 * dpi);
      return true;
    default:
      return false;
  }
}

bool nsNativeThemeAndroid::GetWidgetOverflow(nsDeviceContext* aContext,
                                             nsIFrame* aFrame,
                                             StyleAppearance aAppearance,
                                             nsRect* aOverflowRect) {
  // TODO(bug 1620360): This should return non-zero for
  // StyleAppearance::FocusOutline, if we implement outline-style: auto.
  return false;
}

NS_IMETHODIMP
nsNativeThemeAndroid::GetMinimumWidgetSize(nsPresContext* aPresContext,
                                           nsIFrame* aFrame,
                                           StyleAppearance aAppearance,
                                           LayoutDeviceIntSize* aResult,
                                           bool* aIsOverridable) {
  uint32_t dpiRatio = GetDPIRatio(aFrame);
  aResult->width = aResult->height =
      static_cast<uint32_t>(kMinimumAndroidWidgetSize) * dpiRatio;
  *aIsOverridable = true;
  if (aAppearance == StyleAppearance::MozMenulistArrowButton) {
    aResult->width =
        static_cast<uint32_t>(kMinimumDropdownArrowButtonWidth) * dpiRatio;
  }
  return NS_OK;
}

auto nsNativeThemeAndroid::GetScrollbarSizes(nsPresContext* aPresContext,
                                             StyleScrollbarWidth aWidth,
                                             Overlay aOverlay)
    -> ScrollbarSizes {
  int32_t size = kMinimumAndroidWidgetSize * int32_t(GetDPIRatio(aPresContext));
  return {size, size};
}

nsITheme::Transparency nsNativeThemeAndroid::GetWidgetTransparency(
    nsIFrame* aFrame, StyleAppearance aAppearance) {
  return eUnknownTransparency;
}

NS_IMETHODIMP
nsNativeThemeAndroid::WidgetStateChanged(nsIFrame* aFrame,
                                         StyleAppearance aAppearance,
                                         nsAtom* aAttribute,
                                         bool* aShouldRepaint,
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
nsNativeThemeAndroid::ThemeChanged() { return NS_OK; }

bool nsNativeThemeAndroid::WidgetAppearanceDependsOnWindowFocus(
    StyleAppearance) {
  return false;
}

nsITheme::ThemeGeometryType nsNativeThemeAndroid::ThemeGeometryTypeForWidget(
    nsIFrame* aFrame, StyleAppearance aAppearance) {
  return eThemeGeometryTypeUnknown;
}

bool nsNativeThemeAndroid::ThemeSupportsWidget(nsPresContext* aPresContext,
                                               nsIFrame* aFrame,
                                               StyleAppearance aAppearance) {
  if (IsWidgetScrollbarPart(aAppearance)) {
    const auto* style = nsLayoutUtils::StyleForScrollbar(aFrame);
    // We don't currently handle custom scrollbars on
    // nsNativeThemeAndroid. We could, potentially.
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
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonDown:
    case StyleAppearance::ScrollbarbuttonLeft:
    case StyleAppearance::ScrollbarbuttonRight:
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

bool nsNativeThemeAndroid::WidgetIsContainer(StyleAppearance aAppearance) {
  switch (aAppearance) {
    case StyleAppearance::MozMenulistArrowButton:
    case StyleAppearance::Radio:
    case StyleAppearance::Checkbox:
      return false;
    default:
      return true;
  }
}

bool nsNativeThemeAndroid::ThemeDrawsFocusForWidget(
    StyleAppearance aAppearance) {
  switch (aAppearance) {
    case StyleAppearance::Range:
    // TODO(emilio): Checkbox / Radio don't have focus indicators when checked.
    // If they did, we could just return true here unconditionally.
    case StyleAppearance::Checkbox:
    case StyleAppearance::Radio:
      return false;
    default:
      return true;
  }
}

bool nsNativeThemeAndroid::ThemeNeedsComboboxDropmarker() { return true; }

already_AddRefed<nsITheme> do_GetNativeThemeDoNotUseDirectly() {
  static StaticRefPtr<nsITheme> gInstance;
  if (MOZ_UNLIKELY(!gInstance)) {
    gInstance = new nsNativeThemeAndroid();
    ClearOnShutdown(&gInstance);
  }
  return do_AddRef(gInstance);
}
