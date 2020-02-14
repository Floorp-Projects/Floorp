/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeBasicTheme.h"

#include "mozilla/StaticPrefs_layout.h"
#include "nsComboboxControlFrame.h"
#include "nsCSSRendering.h"
#include "nsDateTimeControlFrame.h"
#include "nsDeviceContext.h"
#include "PathHelpers.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::widget;

namespace mozilla {
namespace widget {

static const Color sBackgroundColor(Color(1.0f, 1.0f, 1.0f));
static const Color sBackgroundActiveColor(Color(0.88f, 0.88f, 0.9f));
static const Color sBackgroundActiveColorDisabled(Color(0.88f, 0.88f, 0.9f,
                                                        0.4f));
static const Color sBorderColor(Color(0.62f, 0.62f, 0.68f));
static const Color sBorderColorDisabled(Color(0.44f, 0.44f, 0.44f, 0.4f));
static const Color sBorderHoverColor(Color(0.5f, 0.5f, 0.56f));
static const Color sBorderHoverColorDisabled(Color(0.5f, 0.5f, 0.56f, 0.4f));
static const Color sBorderFocusColor(Color(0.04f, 0.52f, 1.0f));
static const Color sCheckBackgroundColor(Color(0.18f, 0.39f, 0.89f));
static const Color sCheckBackgroundColorDisabled(Color(0.18f, 0.39f, 0.89f,
                                                       0.4f));
static const Color sCheckBackgroundHoverColor(Color(0.02f, 0.24f, 0.58f));
static const Color sCheckBackgroundHoverColorDisabled(Color(0.02f, 0.24f, 0.58f,
                                                            0.4f));
static const Color sCheckBackgroundActiveColor(Color(0.03f, 0.19f, 0.45f));
static const Color sCheckBackgroundActiveColorDisabled(Color(0.03f, 0.19f,
                                                             0.45f, 0.4f));
static const Color sDisabledColor(Color(0.89f, 0.89f, 0.89f));
static const Color sActiveColor(Color(0.47f, 0.47f, 0.48f));
static const Color sInputHoverColor(Color(0.05f, 0.05f, 0.05f, 0.5f));
static const Color sRangeInputBackgroundColor(Color(0.89f, 0.89f, 0.89f));
static const Color sScrollbarColor(Color(0.94f, 0.94f, 0.94f));
static const Color sScrollbarBorderColor(Color(1.0f, 1.0f, 1.0f));
static const Color sScrollbarThumbColor(Color(0.8f, 0.8f, 0.8f));
static const Color sScrollbarThumbColorActive(Color(0.375f, 0.375f, 0.375f));
static const Color sScrollbarThumbColorHover(Color(0.65f, 0.65f, 0.65f));
static const Color sScrollbarArrowColor(Color(0.375f, 0.375f, 0.375f));
static const Color sScrollbarArrowColorActive(Color(1.0f, 1.0f, 1.0f));
static const Color sScrollbarArrowColorHover(Color(0.0f, 0.0f, 0.0f));
static const Color sScrollbarButtonColor(sScrollbarColor);
static const Color sScrollbarButtonActiveColor(Color(0.375f, 0.375f, 0.375f));
static const Color sScrollbarButtonHoverColor(Color(0.86f, 0.86f, 0.86f));
static const Color sButtonColor(Color(0.98f, 0.98f, 0.98f));
static const Color sButtonHoverColor(Color(0.94f, 0.94f, 0.96f));
static const Color sButtonActiveColor(Color(0.88f, 0.88f, 0.90f));
static const Color sWhiteColor(Color(1.0f, 1.0f, 1.0f, 0.0f));

}  // namespace widget
}  // namespace mozilla

NS_IMPL_ISUPPORTS_INHERITED(nsNativeBasicTheme, nsNativeTheme, nsITheme)

static uint32_t GetDPIRatio(nsIFrame* aFrame) {
  return AppUnitsPerCSSPixel() / aFrame->PresContext()
                                     ->DeviceContext()
                                     ->AppUnitsPerDevPixelAtUnitFullZoom();
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
                               Color& aBackgroundColor, Color& aBorderColor) {
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isActive =
      aState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE);
  bool isHovered = !isDisabled && aState.HasState(NS_EVENT_STATE_HOVER);
  bool isChecked = aState.HasState(NS_EVENT_STATE_CHECKED);

  Color fillColor = sBackgroundColor;
  Color borderColor = sBorderColor;
  if (isDisabled) {
    if (isChecked) {
      if (isActive) {
        fillColor = borderColor = sCheckBackgroundActiveColorDisabled;
      } else {
        fillColor = borderColor = sCheckBackgroundColorDisabled;
      }
    } else if (isActive) {
      fillColor = sBackgroundActiveColorDisabled;
      borderColor = sBorderHoverColorDisabled;
    } else {
      fillColor = sBackgroundColor;
      borderColor = sBorderColorDisabled;
    }
  } else {
    if (isChecked) {
      if (isActive) {
        fillColor = borderColor = sCheckBackgroundActiveColor;
      } else if (isHovered) {
        fillColor = borderColor = sCheckBackgroundHoverColor;
      } else {
        fillColor = borderColor = sCheckBackgroundColor;
      }
    } else if (isActive) {
      fillColor = sBackgroundActiveColor;
      borderColor = sBorderHoverColor;
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

static void PaintCheckboxControl(DrawTarget* aDrawTarget, const Rect& aRect,
                                 const EventStates& aState, uint32_t aDpi) {
  uint32_t radius = 3 * aDpi;
  RectCornerRadii innerRadii(radius, radius, radius, radius);
  Rect rect(aRect);
  rect.Round();
  RefPtr<Path> roundedRect =
      MakePathForRoundedRect(*aDrawTarget, rect, innerRadii);

  Color backgroundColor;
  Color borderColor;
  ComputeCheckColors(aState, backgroundColor, borderColor);

  aDrawTarget->Fill(roundedRect, ColorPattern(ToDeviceColor(backgroundColor)));
  aDrawTarget->Stroke(roundedRect, ColorPattern(ToDeviceColor(borderColor)),
                      StrokeOptions(2.0f * aDpi));
}

static void PaintCheckMark(DrawTarget* aDrawTarget, const Rect& aRect,
                           const EventStates& aState, uint32_t aDpi) {
  // Points come from the coordinates on a 7X7 unit box centered at 0,0
  const float checkPolygonX[] = {-2.5, -0.7, 2.5};
  const float checkPolygonY[] = {-0.3, 1.7, -1.5};
  const int32_t checkNumPoints = sizeof(checkPolygonX) / sizeof(float);
  const int32_t checkSize = 8;

  // Scale the checkmark based on the smallest dimension
  nscoord paintScale = std::min(aRect.width, aRect.height) / checkSize;
  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
  Point p = aRect.Center() +
            Point(checkPolygonX[0] * paintScale, checkPolygonY[0] * paintScale);
  builder->MoveTo(p);
  for (int32_t polyIndex = 1; polyIndex < checkNumPoints; polyIndex++) {
    p = aRect.Center() + Point(checkPolygonX[polyIndex] * paintScale,
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

static void PaintRadioControl(DrawTarget* aDrawTarget, const Rect& aRect,
                              const EventStates& aState, uint32_t aDpi) {
  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
  AppendEllipseToPath(builder, aRect.Center(), aRect.Size());
  RefPtr<Path> ellipse = builder->Finish();

  Color backgroundColor;
  Color borderColor;
  ComputeCheckColors(aState, backgroundColor, borderColor);

  aDrawTarget->Fill(ellipse, ColorPattern(ToDeviceColor(backgroundColor)));
  aDrawTarget->Stroke(ellipse, ColorPattern(ToDeviceColor(borderColor)),
                      StrokeOptions(2.0f * aDpi));
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

static void PaintTextField(DrawTarget* aDrawTarget, const Rect& aRect,
                           const EventStates& aState, uint32_t aDpi) {
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isHovered = !isDisabled && aState.HasState(NS_EVENT_STATE_HOVER);

  uint32_t radius = 4 * aDpi;
  RectCornerRadii innerRadii(radius, radius, radius, radius);
  RefPtr<Path> roundedRect =
      MakePathForRoundedRect(*aDrawTarget, aRect, innerRadii);

  aDrawTarget->Fill(roundedRect,
                    ColorPattern(ToDeviceColor(isDisabled ? sDisabledColor
                                                          : sBackgroundColor)));
  aDrawTarget->Stroke(
      roundedRect,
      ColorPattern(ToDeviceColor(isHovered ? sBorderHoverColor : sBorderColor)),
      StrokeOptions(1.0f * aDpi));
}

static void PaintMenulist(DrawTarget* aDrawTarget, const Rect& aRect,
                          const EventStates& aState, uint32_t aDpi) {
  bool isActive =
      aState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE);
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isHovered = !isDisabled && aState.HasState(NS_EVENT_STATE_HOVER);

  uint32_t radius = 4 * aDpi;
  RectCornerRadii innerRadii(radius, radius, radius, radius);
  RefPtr<Path> roundedRect =
      MakePathForRoundedRect(*aDrawTarget, aRect, innerRadii);

  if (isDisabled) {
    aDrawTarget->Fill(roundedRect, ColorPattern(ToDeviceColor(sDisabledColor)));
  } else {
    aDrawTarget->Fill(
        roundedRect,
        ColorPattern(ToDeviceColor(isActive ? sButtonActiveColor
                                            : isHovered ? sButtonHoverColor
                                                        : sButtonColor)));
  }

  aDrawTarget->Stroke(
      roundedRect,
      ColorPattern(ToDeviceColor(isHovered ? sBorderHoverColor : sBorderColor)),
      StrokeOptions(1.0f * aDpi));
}

static void PaintArrow(DrawTarget* aDrawTarget, const Rect& aRect,
                       const int32_t aArrowPolygonX[],
                       const int32_t aArrowPolygonY[],
                       const int32_t aArrowNumPoints, const int32_t aArrowSize,
                       const Color aFillColor, uint32_t aDpi) {
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

static void PaintMenulistButton(nsIFrame* aFrame, DrawTarget* aDrawTarget,
                                const Rect& aRect, const EventStates& aState,
                                uint32_t aDpi) {
  bool isActive =
      aState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE);
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isHovered = !isDisabled && aState.HasState(NS_EVENT_STATE_HOVER);
  bool isHTML = nsNativeTheme::IsHTMLContent(aFrame);

  if (!isHTML && nsNativeTheme::CheckBooleanAttr(aFrame, nsGkAtoms::open)) {
    isHovered = false;
  }

  const int32_t arrowSize = 8;
  int32_t arrowPolygonX[] = {-4, -2, 0};
  int32_t arrowPolygonY[] = {-1, 1, -1};
  const int32_t arrowNumPoints = sizeof(arrowPolygonX) / sizeof(int32_t);

  PaintArrow(
      aDrawTarget, aRect, arrowPolygonX, arrowPolygonY, arrowNumPoints,
      arrowSize,
      isActive ? sActiveColor : isHovered ? sBorderHoverColor : sBorderColor,
      aDpi);
}

static void PaintSpinnerButton(DrawTarget* aDrawTarget, const Rect& aRect,
                               const EventStates& aState,
                               StyleAppearance aAppearance, uint32_t aDpi) {
  bool isActive =
      aState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE);
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isHovered = !isDisabled && aState.HasState(NS_EVENT_STATE_HOVER);

  const int32_t arrowSize = 8;
  int32_t arrowPolygonX[] = {0, 2, 4};
  int32_t arrowPolygonY[] = {-3, -1, -3};
  const int32_t arrowNumPoints = sizeof(arrowPolygonX) / sizeof(int32_t);

  if (aAppearance == StyleAppearance::SpinnerUpbutton) {
    for (int32_t i = 0; i < arrowNumPoints; i++) {
      arrowPolygonY[i] *= -1;
    }
  }

  PaintArrow(
      aDrawTarget, aRect, arrowPolygonX, arrowPolygonY, arrowNumPoints,
      arrowSize,
      isActive ? sActiveColor : isHovered ? sBorderHoverColor : sBorderColor,
      aDpi);
}

static void PaintRangeInputBackground(DrawTarget* aDrawTarget,
                                      const Rect& aRect,
                                      const EventStates& aState,
                                      uint32_t aDpi) {
  Rect rect(aRect);
  rect.y += (rect.height - rect.height / 4) / 2;
  rect.height /= 4;

  aDrawTarget->FillRect(
      rect, ColorPattern(ToDeviceColor(sRangeInputBackgroundColor)));
  aDrawTarget->StrokeRect(rect,
                          ColorPattern(ToDeviceColor(sButtonActiveColor)));
}

static void PaintScrollbarthumbHorizontal(DrawTarget* aDrawTarget,
                                          const Rect& aRect,
                                          const EventStates& aState) {
  Color thumbColor = sScrollbarThumbColor;
  if (aState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE)) {
    thumbColor = sScrollbarThumbColorActive;
  } else if (aState.HasState(NS_EVENT_STATE_HOVER)) {
    thumbColor = sScrollbarThumbColorHover;
  }
  aDrawTarget->FillRect(aRect, ColorPattern(ToDeviceColor(thumbColor)));
}

static void PaintScrollbarthumbVertical(DrawTarget* aDrawTarget,
                                        const Rect& aRect,
                                        const EventStates& aState) {
  Color thumbColor = sScrollbarThumbColor;
  if (aState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE)) {
    thumbColor = sScrollbarThumbColorActive;
  } else if (aState.HasState(NS_EVENT_STATE_HOVER)) {
    thumbColor = sScrollbarThumbColorHover;
  }
  aDrawTarget->FillRect(aRect, ColorPattern(ToDeviceColor(thumbColor)));
}

static void PaintScrollbarHorizontal(DrawTarget* aDrawTarget,
                                     const Rect& aRect) {
  aDrawTarget->FillRect(aRect, ColorPattern(ToDeviceColor(sScrollbarColor)));
  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
  builder->MoveTo(Point(aRect.x, aRect.y));
  builder->LineTo(Point(aRect.x + aRect.width, aRect.y));
  RefPtr<Path> path = builder->Finish();
  aDrawTarget->Stroke(path, ColorPattern(ToDeviceColor(sScrollbarBorderColor)));
}

static void PaintScrollbarVerticalAndCorner(DrawTarget* aDrawTarget,
                                            const Rect& aRect, uint32_t aDpi) {
  aDrawTarget->FillRect(aRect, ColorPattern(ToDeviceColor(sScrollbarColor)));
  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
  builder->MoveTo(Point(aRect.x, aRect.y));
  builder->LineTo(Point(aRect.x, aRect.y + aRect.height));
  RefPtr<Path> path = builder->Finish();
  aDrawTarget->Stroke(path, ColorPattern(ToDeviceColor(sScrollbarBorderColor)),
                      StrokeOptions(1.0f * aDpi));
}

static void PaintScrollbarbutton(DrawTarget* aDrawTarget,
                                 StyleAppearance aAppearance, const Rect& aRect,
                                 const EventStates& aState, uint32_t aDpi) {
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
             aDpi);

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
                      StrokeOptions(1.0f * aDpi));
}

static void PaintButton(nsIFrame* aFrame, DrawTarget* aDrawTarget,
                        const Rect& aRect, const EventStates& aState,
                        uint32_t aDpi) {
  bool isActive =
      aState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE);
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isHovered = !isDisabled && aState.HasState(NS_EVENT_STATE_HOVER);

  uint32_t radius = 4 * aDpi;
  RectCornerRadii innerRadii(radius, radius, radius, radius);
  Rect rect(aRect);
  rect.Round();
  RefPtr<Path> roundedRect =
      MakePathForRoundedRect(*aDrawTarget, rect, innerRadii);

  if (isDisabled) {
    aDrawTarget->Fill(roundedRect, ColorPattern(ToDeviceColor(sDisabledColor)));
  } else if (IsDateTimeResetButton(aFrame)) {
    aDrawTarget->Fill(roundedRect, ColorPattern(ToDeviceColor(sWhiteColor)));
  } else {
    aDrawTarget->Fill(
        roundedRect,
        ColorPattern(ToDeviceColor(isActive ? sButtonActiveColor
                                            : isHovered ? sButtonHoverColor
                                                        : sButtonColor)));
  }

  aDrawTarget->Stroke(
      roundedRect,
      ColorPattern(ToDeviceColor(isHovered ? sBorderHoverColor : sBorderColor)),
      StrokeOptions(1.0f * aDpi));
}

NS_IMETHODIMP
nsNativeBasicTheme::DrawWidgetBackground(gfxContext* aContext, nsIFrame* aFrame,
                                         StyleAppearance aAppearance,
                                         const nsRect& aRect,
                                         const nsRect& aDirtyRect) {
  DrawTarget* dt = aContext->GetDrawTarget();
  const nscoord twipsPerPixel = aFrame->PresContext()->AppUnitsPerDevPixel();
  Rect devPxRect = NSRectToRect(aRect, twipsPerPixel);
  EventStates eventState = GetContentState(aFrame, aAppearance);

  if (aAppearance == StyleAppearance::MenulistButton ||
      aAppearance == StyleAppearance::MozMenulistButton) {
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
    case StyleAppearance::Radio:
      PaintRadioControl(dt, devPxRect, eventState, dpi);
      if (IsSelected(aFrame)) {
        PaintCheckedRadioButton(dt, devPxRect, dpi);
      }
      break;
    case StyleAppearance::Checkbox:
      PaintCheckboxControl(dt, devPxRect, eventState, dpi);
      if (IsChecked(aFrame)) {
        PaintCheckMark(dt, devPxRect, eventState, dpi);
      }
      if (GetIndeterminate(aFrame)) {
        PaintIndeterminateMark(dt, devPxRect, eventState);
      }
      break;
    case StyleAppearance::Textarea:
    case StyleAppearance::Textfield:
    case StyleAppearance::NumberInput:
      PaintTextField(dt, devPxRect, eventState, dpi);
      break;
    case StyleAppearance::Listbox:
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistTextfield:
      PaintMenulist(dt, devPxRect, eventState, dpi);
      break;
    case StyleAppearance::MenulistButton:
    case StyleAppearance::MozMenulistButton:
      PaintMenulistButton(aFrame, dt, devPxRect, eventState, dpi);
      break;
    case StyleAppearance::SpinnerUpbutton:
    case StyleAppearance::SpinnerDownbutton:
      PaintSpinnerButton(dt, devPxRect, eventState, aAppearance, dpi);
      break;
    case StyleAppearance::Range:
      PaintRangeInputBackground(dt, devPxRect, eventState, dpi);
      break;
    case StyleAppearance::RangeThumb:
      // TODO: Paint Range Thumb
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
      PaintScrollbarVerticalAndCorner(dt, devPxRect, dpi);
      break;
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonDown:
    case StyleAppearance::ScrollbarbuttonLeft:
    case StyleAppearance::ScrollbarbuttonRight:
      PaintScrollbarbutton(dt, aAppearance, devPxRect, eventState, dpi);
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
nsNativeBasicTheme::CreateWebRenderCommandsForWidget(mozilla::wr::DisplayListBuilder&
aBuilder, mozilla::wr::IpcResourceUpdateQueue& aResources, const
mozilla::layers::StackingContextHelper& aSc,
                                      mozilla::layers::RenderRootStateManager*
aManager, nsIFrame* aFrame, StyleAppearance aAppearance, const nsRect& aRect) {
}*/

LayoutDeviceIntMargin nsNativeBasicTheme::GetWidgetBorder(
    nsDeviceContext* aContext, nsIFrame* aFrame, StyleAppearance aAppearance) {
  return LayoutDeviceIntMargin();
}

bool nsNativeBasicTheme::GetWidgetPadding(nsDeviceContext* aContext,
                                          nsIFrame* aFrame,
                                          StyleAppearance aAppearance,
                                          LayoutDeviceIntMargin* aResult) {
  if (aAppearance == StyleAppearance::Menulist ||
      aAppearance == StyleAppearance::MenulistTextfield ||
      aAppearance == StyleAppearance::NumberInput ||
      aAppearance == StyleAppearance::Textarea ||
      aAppearance == StyleAppearance::Textfield) {
    // If we have author-specified padding for these elements, don't do the
    // fixups below.
    if (aFrame->PresContext()->HasAuthorSpecifiedRules(
            aFrame, NS_AUTHOR_SPECIFIED_PADDING)) {
      return false;
    }
  }

  uint32_t dpi = GetDPIRatio(aFrame);
  switch (aAppearance) {
    // Radios and checkboxes return a fixed size in GetMinimumWidgetSize
    // and have a meaningful baseline, so they can't have
    // author-specified padding.
    case StyleAppearance::Radio:
    case StyleAppearance::Checkbox:
    case StyleAppearance::MenulistButton:
    case StyleAppearance::MozMenulistButton:
      aResult->SizeTo(0, 0, 0, 0);
      return true;
    case StyleAppearance::Textarea:
    case StyleAppearance::Listbox:
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistTextfield:
    case StyleAppearance::NumberInput:
      aResult->SizeTo(7 * dpi, 8 * dpi, 7 * dpi, 8 * dpi);
      return true;
    case StyleAppearance::Button:
      aResult->SizeTo(7 * dpi, 22 * dpi, 7 * dpi, 22 * dpi);
      return true;
    case StyleAppearance::Textfield:
      if (IsDateTimeTextField(aFrame)) {
        aResult->SizeTo(8 * dpi, 8 * dpi, 6 * dpi, 8 * dpi);
        return true;
      }
      aResult->SizeTo(7 * dpi, 8 * dpi, 7 * dpi, 8 * dpi);
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
    case StyleAppearance::Button:
    case StyleAppearance::MozMacDisclosureButtonOpen:
    case StyleAppearance::MozMacDisclosureButtonClosed:
    case StyleAppearance::MozMacHelpButton:
    case StyleAppearance::Toolbarbutton:
    case StyleAppearance::NumberInput:
    case StyleAppearance::Textfield:
    case StyleAppearance::Textarea:
    case StyleAppearance::Searchfield:
    case StyleAppearance::Listbox:
    // NOTE: if you change Menulist and MenulistButton to behave differently,
    // be sure to handle StaticPrefs::layout_css_webkit_appearance_enabled.
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton:
    case StyleAppearance::MozMenulistButton:
    case StyleAppearance::MenulistTextfield:
    case StyleAppearance::Checkbox:
    case StyleAppearance::Radio:
    case StyleAppearance::Tab:
    case StyleAppearance::FocusOutline: {
      overflow.SizeTo(2, 2, 2, 2);
      break;
    }
    case StyleAppearance::ProgressBar: {
      // Progress bars draw a 2 pixel white shadow under their progress
      // indicators.
      overflow.bottom = 2;
      break;
    }
    case StyleAppearance::Meter: {
      // Meter bars overflow their boxes by about 2 pixels.
      overflow.SizeTo(2, 2, 2, 2);
      break;
    }
    default:
      break;
  }

  if (overflow != nsIntMargin()) {
    int32_t p2a = aFrame->PresContext()->AppUnitsPerDevPixel();
    aOverflowRect->Inflate(nsMargin(NSIntPixelsToAppUnits(overflow.top, p2a),
                                    NSIntPixelsToAppUnits(overflow.right, p2a),
                                    NSIntPixelsToAppUnits(overflow.bottom, p2a),
                                    NSIntPixelsToAppUnits(overflow.left, p2a)));
    return true;
  }

  return false;
}

NS_IMETHODIMP
nsNativeBasicTheme::GetMinimumWidgetSize(nsPresContext* aPresContext,
                                         nsIFrame* aFrame,
                                         StyleAppearance aAppearance,
                                         mozilla::LayoutDeviceIntSize* aResult,
                                         bool* aIsOverridable) {
  aResult->width = aResult->height = 17 * GetDPIRatio(aFrame);
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
  // Some widget types just never change state.
  switch (aAppearance) {
    case StyleAppearance::MozWindowTitlebar:
    case StyleAppearance::Toolbox:
    case StyleAppearance::Toolbar:
    case StyleAppearance::Statusbar:
    case StyleAppearance::Statusbarpanel:
    case StyleAppearance::Resizerpanel:
    case StyleAppearance::Tooltip:
    case StyleAppearance::Tabpanels:
    case StyleAppearance::Tabpanel:
    case StyleAppearance::Dialog:
    case StyleAppearance::Menupopup:
    case StyleAppearance::Groupbox:
    case StyleAppearance::Progresschunk:
    case StyleAppearance::ProgressBar:
    case StyleAppearance::ProgressbarVertical:
    case StyleAppearance::Meter:
    case StyleAppearance::Meterchunk:
    case StyleAppearance::MozMacVibrancyLight:
    case StyleAppearance::MozMacVibrancyDark:
    case StyleAppearance::MozMacVibrantTitlebarLight:
    case StyleAppearance::MozMacVibrantTitlebarDark:
      *aShouldRepaint = false;
      return NS_OK;
    default:
      break;
  }

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
  switch (aAppearance) {
    case StyleAppearance::MozWindowTitlebar:
    case StyleAppearance::MozWindowTitlebarMaximized:
    case StyleAppearance::MozWindowFrameLeft:
    case StyleAppearance::MozWindowFrameRight:
    case StyleAppearance::MozWindowFrameBottom:
    case StyleAppearance::MozWindowButtonClose:
    case StyleAppearance::MozWindowButtonMinimize:
    case StyleAppearance::MozWindowButtonMaximize:
    case StyleAppearance::MozWindowButtonRestore:
      return true;
    default:
      return false;
  }
}

nsITheme::ThemeGeometryType nsNativeBasicTheme::ThemeGeometryTypeForWidget(
    nsIFrame* aFrame, StyleAppearance aAppearance) {
  return eThemeGeometryTypeUnknown;
}

bool nsNativeBasicTheme::ThemeSupportsWidget(nsPresContext* aPresContext,
                                             nsIFrame* aFrame,
                                             StyleAppearance aAppearance) {
  if (aAppearance == StyleAppearance::MenulistButton &&
      StaticPrefs::layout_css_webkit_appearance_enabled()) {
    aAppearance = StyleAppearance::Menulist;
  }

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
    /*case StyleAppearance::RangeThumb:*/
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
    case StyleAppearance::MenulistTextfield:
    case StyleAppearance::NumberInput:
    case StyleAppearance::MenulistButton:
    case StyleAppearance::MozMenulistButton:
    case StyleAppearance::SpinnerUpbutton:
    case StyleAppearance::SpinnerDownbutton:
      return !IsWidgetStyled(aPresContext, aFrame, aAppearance);
    default:
      return false;
  }
}

bool nsNativeBasicTheme::WidgetIsContainer(StyleAppearance aAppearance) {
  if (aAppearance == StyleAppearance::MenulistButton &&
      StaticPrefs::layout_css_webkit_appearance_enabled()) {
    aAppearance = StyleAppearance::Menulist;
  }

  switch (aAppearance) {
    case StyleAppearance::MenulistButton:
    case StyleAppearance::MozMenulistButton:
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
