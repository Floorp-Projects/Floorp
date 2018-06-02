/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeThemeAndroid.h"

#include "nsIFrame.h"
#include "nsThemeConstants.h"
#include "AndroidColors.h"
#include "nsCSSRendering.h"
#include "PathHelpers.h"

NS_IMPL_ISUPPORTS_INHERITED(nsNativeThemeAndroid, nsNativeTheme, nsITheme)

using namespace mozilla;
using namespace mozilla::gfx;

static void
ClampRectAndMoveToCenter(nsRect& aRect)
{
  if (aRect.width < aRect.height) {
    aRect.y += (aRect.height - aRect.width) / 2;
    aRect.height = aRect.width;
    return;
  }

  if (aRect.height < aRect.width) {
    aRect.x += (aRect.width - aRect.height) / 2;
    aRect.width = aRect.height;
  }
}

static void
PaintCheckboxControl(nsIFrame* aFrame,
                     DrawTarget* aDrawTarget,
                     const nsRect& aRect,
                     const EventStates& aState)
{
  // Checkbox controls aren't something that we can render on Android
  // natively. We fake native drawing of appearance: checkbox items
  // out here, and use hardcoded colours from AndroidColors.h to
  // simulate native theming.
  RectCornerRadii innerRadii(2, 2, 2, 2);
  nsRect paddingRect =
    nsCSSRendering::GetBoxShadowInnerPaddingRect(aFrame, aRect);
  const nscoord twipsPerPixel = aFrame->PresContext()->DevPixelsToAppUnits(1);
  Rect shadowGfxRect = NSRectToRect(paddingRect, twipsPerPixel);
  shadowGfxRect.Round();
  RefPtr<Path> roundedRect =
    MakePathForRoundedRect(*aDrawTarget, shadowGfxRect, innerRadii);
  aDrawTarget->Stroke(roundedRect,
    ColorPattern(ToDeviceColor(mozilla::widget::sAndroidBorderColor)));
  aDrawTarget->Fill(roundedRect,
    ColorPattern(ToDeviceColor(mozilla::widget::sAndroidBackgroundColor)));

  if (aState.HasState(NS_EVENT_STATE_DISABLED)) {
    aDrawTarget->Fill(roundedRect,
      ColorPattern(ToDeviceColor(mozilla::widget::sAndroidDisabledColor)));
    return;
  }

  if (aState.HasState(NS_EVENT_STATE_ACTIVE)) {
    aDrawTarget->Fill(roundedRect,
      ColorPattern(ToDeviceColor(mozilla::widget::sAndroidActiveColor)));
  }
}

static void
PaintCheckMark(nsIFrame* aFrame,
               DrawTarget* aDrawTarget,
               const nsRect& aRect)
{
  // Points come from the coordinates on a 7X7 unit box centered at 0,0
  const int32_t checkPolygonX[] = { -3, -1,  3,  3, -1, -3 };
  const int32_t checkPolygonY[] = { -1,  1, -3, -1,  3,  1 };
  const int32_t checkNumPoints = sizeof(checkPolygonX) / sizeof(int32_t);
  const int32_t checkSize      = 9; // 2 units of padding on either side
                                    // of the 7x7 unit checkmark

  // Scale the checkmark based on the smallest dimension
  nscoord paintScale = std::min(aRect.width, aRect.height) / checkSize;
  nsPoint paintCenter(aRect.x + aRect.width  / 2,
                      aRect.y + aRect.height / 2);

  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
  nsPoint p = paintCenter + nsPoint(checkPolygonX[0] * paintScale,
                                    checkPolygonY[0] * paintScale);

  int32_t appUnitsPerDevPixel = aFrame->PresContext()->AppUnitsPerDevPixel();
  builder->MoveTo(NSPointToPoint(p, appUnitsPerDevPixel));
  for (int32_t polyIndex = 1; polyIndex < checkNumPoints; polyIndex++) {
    p = paintCenter + nsPoint(checkPolygonX[polyIndex] * paintScale,
                              checkPolygonY[polyIndex] * paintScale);
    builder->LineTo(NSPointToPoint(p, appUnitsPerDevPixel));
  }
  RefPtr<Path> path = builder->Finish();
  aDrawTarget->Fill(path,
    ColorPattern(ToDeviceColor(mozilla::widget::sAndroidCheckColor)));
}

static void
PaintIndeterminateMark(nsIFrame* aFrame,
                       DrawTarget* aDrawTarget,
                       const nsRect& aRect)
{
  int32_t appUnitsPerDevPixel = aFrame->PresContext()->AppUnitsPerDevPixel();

  nsRect rect(aRect);
  rect.y += (rect.height - rect.height/4) / 2;
  rect.height /= 4;

  Rect devPxRect = NSRectToSnappedRect(rect, appUnitsPerDevPixel, *aDrawTarget);

  aDrawTarget->FillRect(devPxRect,
    ColorPattern(ToDeviceColor(mozilla::widget::sAndroidCheckColor)));
}

static void
PaintRadioControl(nsIFrame* aFrame,
                  DrawTarget* aDrawTarget,
                  const nsRect& aRect,
                  const EventStates& aState)
{
  // Radio controls aren't something that we can render on Android
  // natively. We fake native drawing of appearance: radio items
  // out here, and use hardcoded colours to simulate native
  // theming.
  const nscoord twipsPerPixel = aFrame->PresContext()->DevPixelsToAppUnits(1);
  Rect devPxRect = NSRectToRect(aRect, twipsPerPixel);
  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
  AppendEllipseToPath(builder, devPxRect.Center(), devPxRect.Size());
  RefPtr<Path> ellipse = builder->Finish();
  aDrawTarget->Stroke(ellipse,
    ColorPattern(ToDeviceColor(mozilla::widget::sAndroidBorderColor)));
  aDrawTarget->Fill(ellipse,
    ColorPattern(ToDeviceColor(mozilla::widget::sAndroidBackgroundColor)));

  if (aState.HasState(NS_EVENT_STATE_DISABLED)) {
    aDrawTarget->Fill(ellipse,
      ColorPattern(ToDeviceColor(mozilla::widget::sAndroidDisabledColor)));
    return;
  }

  if (aState.HasState(NS_EVENT_STATE_ACTIVE)) {
    aDrawTarget->Fill(ellipse,
      ColorPattern(ToDeviceColor(mozilla::widget::sAndroidActiveColor)));
  }
}

static void
PaintCheckedRadioButton(nsIFrame* aFrame,
                        DrawTarget* aDrawTarget,
                        const nsRect& aRect)
{
  // The dot is an ellipse 2px on all sides smaller than the content-box,
  // drawn in the foreground color.
  nsRect rect(aRect);
  rect.Deflate(nsPresContext::CSSPixelsToAppUnits(2),
               nsPresContext::CSSPixelsToAppUnits(2));

  Rect devPxRect =
    ToRect(nsLayoutUtils::RectToGfxRect(rect,
                                        aFrame->PresContext()->AppUnitsPerDevPixel()));

  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
  AppendEllipseToPath(builder, devPxRect.Center(), devPxRect.Size());
  RefPtr<Path> ellipse = builder->Finish();
  aDrawTarget->Fill(ellipse,
    ColorPattern(ToDeviceColor(mozilla::widget::sAndroidCheckColor)));
}

NS_IMETHODIMP
nsNativeThemeAndroid::DrawWidgetBackground(gfxContext* aContext,
                                           nsIFrame* aFrame,
                                           uint8_t aWidgetType,
                                           const nsRect& aRect,
                                           const nsRect& aDirtyRect)
{
  EventStates eventState = GetContentState(aFrame, aWidgetType);
  nsRect rect(aRect);
  ClampRectAndMoveToCenter(rect);

  switch (aWidgetType) {
    case NS_THEME_RADIO:
      PaintRadioControl(aFrame, aContext->GetDrawTarget(), rect, eventState);
      if (IsSelected(aFrame)) {
        PaintCheckedRadioButton(aFrame, aContext->GetDrawTarget(), rect);
      }
      break;
    case NS_THEME_CHECKBOX:
      PaintCheckboxControl(aFrame, aContext->GetDrawTarget(), rect, eventState);
      if (IsChecked(aFrame)) {
        PaintCheckMark(aFrame, aContext->GetDrawTarget(), rect);
      }
      if (GetIndeterminate(aFrame)) {
        PaintIndeterminateMark(aFrame, aContext->GetDrawTarget(), rect);
      }
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Should not get here with a widget type we don't support.");
      return NS_ERROR_NOT_IMPLEMENTED;
  }

  return NS_OK;
}

LayoutDeviceIntMargin
nsNativeThemeAndroid::GetWidgetBorder(nsDeviceContext* aContext, nsIFrame* aFrame,
                                      uint8_t aWidgetType)
{
  return LayoutDeviceIntMargin();
}

bool
nsNativeThemeAndroid::GetWidgetPadding(nsDeviceContext* aContext,
                                       nsIFrame* aFrame, uint8_t aWidgetType,
                                       LayoutDeviceIntMargin* aResult)
{
  switch (aWidgetType) {
    // Radios and checkboxes return a fixed size in GetMinimumWidgetSize
    // and have a meaningful baseline, so they can't have
    // author-specified padding.
    case NS_THEME_CHECKBOX:
    case NS_THEME_RADIO:
      aResult->SizeTo(0, 0, 0, 0);
      return true;
  }
  return false;
}

bool
nsNativeThemeAndroid::GetWidgetOverflow(nsDeviceContext* aContext,
                                        nsIFrame* aFrame, uint8_t aWidgetType,
                                        nsRect* aOverflowRect)
{
  return false;
}

NS_IMETHODIMP
nsNativeThemeAndroid::GetMinimumWidgetSize(nsPresContext* aPresContext,
                                       nsIFrame* aFrame, uint8_t aWidgetType,
                                       LayoutDeviceIntSize* aResult,
                                       bool* aIsOverridable)
{
  if (aWidgetType == NS_THEME_RADIO || aWidgetType == NS_THEME_CHECKBOX) {
    // 9px + (1px padding + 1px border) * 2
    aResult->width = aPresContext->CSSPixelsToDevPixels(13);
    aResult->height = aPresContext->CSSPixelsToDevPixels(13);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeAndroid::WidgetStateChanged(nsIFrame* aFrame, uint8_t aWidgetType,
                                     nsAtom* aAttribute, bool* aShouldRepaint,
                                     const nsAttrValue* aOldValue)
{
  if (aWidgetType == NS_THEME_RADIO || aWidgetType == NS_THEME_CHECKBOX) {
    if (aAttribute == nsGkAtoms::active ||
        aAttribute == nsGkAtoms::disabled ||
        aAttribute == nsGkAtoms::hover) {
      *aShouldRepaint = true;
      return NS_OK;
    }
  }

  *aShouldRepaint = false;
  return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeAndroid::ThemeChanged()
{
  return NS_OK;
}

NS_IMETHODIMP_(bool)
nsNativeThemeAndroid::ThemeSupportsWidget(nsPresContext* aPresContext,
                                          nsIFrame* aFrame,
                                          uint8_t aWidgetType)
{
  switch (aWidgetType) {
    case NS_THEME_RADIO:
    case NS_THEME_CHECKBOX:
      return true;
  }

  return false;
}

NS_IMETHODIMP_(bool)
nsNativeThemeAndroid::WidgetIsContainer(uint8_t aWidgetType)
{
  return false;
}

bool
nsNativeThemeAndroid::ThemeDrawsFocusForWidget(uint8_t aWidgetType)
{
  return false;
}

bool
nsNativeThemeAndroid::ThemeNeedsComboboxDropmarker()
{
  return false;
}

nsITheme::Transparency
nsNativeThemeAndroid::GetWidgetTransparency(nsIFrame* aFrame, uint8_t aWidgetType)
{
  return eUnknownTransparency;
}
