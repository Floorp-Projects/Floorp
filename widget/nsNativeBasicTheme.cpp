/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeBasicTheme.h"
#include "nsNativeBasicThemeCocoa.h"

#include "gfxBlur.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/HTMLMeterElement.h"
#include "mozilla/dom/HTMLProgressElement.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/gfx/Filters.h"
#include "mozilla/RelativeLuminanceUtils.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "nsCSSColorUtils.h"
#include "nsCSSRendering.h"
#include "nsDeviceContext.h"
#include "nsLayoutUtils.h"
#include "nsRangeFrame.h"
#include "PathHelpers.h"
#include "ScrollbarDrawingAndroid.h"
#include "ScrollbarDrawingCocoa.h"
#include "ScrollbarDrawingGTK.h"
#include "ScrollbarDrawingWin.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::widget;

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

}  // namespace

static StaticRefPtr<nsITheme> gInstance;
static StaticRefPtr<nsITheme> gRDMInstance;

already_AddRefed<nsITheme> do_GetBasicNativeThemeDoNotUseDirectly() {
  if (MOZ_UNLIKELY(!gInstance)) {
    UniquePtr<ScrollbarDrawing> scrollbarDrawing =
        nsNativeBasicTheme::DetermineScrollbarStyle();
#ifdef MOZ_WIDGET_COCOA
    gInstance = new nsNativeBasicThemeCocoa(std::move(scrollbarDrawing));
#else
    gInstance = new nsNativeBasicTheme(std::move(scrollbarDrawing));
#endif
    ClearOnShutdown(&gInstance);
  }
  return do_AddRef(gInstance);
}

#ifdef ANDROID
already_AddRefed<nsITheme> do_GetNativeThemeDoNotUseDirectly() {
  // Android doesn't have a native theme.
  return do_GetBasicNativeThemeDoNotUseDirectly();
}
#endif

already_AddRefed<nsITheme> do_GetRDMThemeDoNotUseDirectly() {
  if (MOZ_UNLIKELY(!gRDMInstance)) {
    UniquePtr<ScrollbarDrawing> scrollbarDrawing =
        nsNativeBasicTheme::DetermineScrollbarStyleSetByPrefs();
    if (!scrollbarDrawing) {
      scrollbarDrawing = MakeUnique<ScrollbarDrawingAndroid>();
    }
    gRDMInstance = new nsNativeBasicTheme(std::move(scrollbarDrawing));
    ClearOnShutdown(&gRDMInstance);
  }
  return do_AddRef(gRDMInstance);
}

static constexpr nsLiteralCString kPrefs[] = {
    "widget.non-native-theme.use-theme-accent"_ns,
    "widget.non-native-theme.win.scrollbar.use-system-size"_ns,
    "widget.non-native-theme.scrollbar.size.override"_ns,
    "widget.non-native-theme.scrollbar.style"_ns,
};

void nsNativeBasicTheme::Init() {
  for (const auto& pref : kPrefs) {
    Preferences::RegisterCallback(PrefChangedCallback, pref);
  }
  LookAndFeelChanged();
}

void nsNativeBasicTheme::Shutdown() {
  for (const auto& pref : kPrefs) {
    Preferences::UnregisterCallback(PrefChangedCallback, pref);
  }
}

/* static */
void nsNativeBasicTheme::LookAndFeelChanged() {
  ThemeColors::RecomputeAccentColors();
  auto* basicTheme = static_cast<nsNativeBasicTheme*>(gInstance.get());
  if (basicTheme) {
    basicTheme->SetScrollbarDrawing(
        nsNativeBasicTheme::DetermineScrollbarStyle());
    basicTheme->GetScrollbarDrawing().RecomputeScrollbarParams();
  }
}

/* static */
auto nsNativeBasicTheme::GetDPIRatio(nsPresContext* aPc,
                                     StyleAppearance aAppearance) -> DPIRatio {
  // Widgets react to zoom, except scrollbars.
  if (IsWidgetScrollbarPart(aAppearance)) {
    return ScrollbarDrawing::GetDPIRatioForScrollbarPart(aPc);
  }
  return DPIRatio(float(AppUnitsPerCSSPixel()) / aPc->AppUnitsPerDevPixel());
}

/* static */
auto nsNativeBasicTheme::GetDPIRatio(nsIFrame* aFrame,
                                     StyleAppearance aAppearance) -> DPIRatio {
  return GetDPIRatio(aFrame->PresContext(), aAppearance);
}

// Checkbox and radio need to preserve aspect-ratio for compat. We also snap the
// size to exact device pixels to avoid snapping disorting the circles.
static LayoutDeviceRect CheckBoxRadioRect(const LayoutDeviceRect& aRect) {
  // Place a square rect in the center of aRect.
  auto size = std::trunc(std::min(aRect.width, aRect.height));
  auto position = aRect.Center() - LayoutDevicePoint(size * 0.5, size * 0.5);
  return LayoutDeviceRect(position, LayoutDeviceSize(size, size));
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicTheme::ComputeCheckboxColors(
    const EventStates& aState, StyleAppearance aAppearance,
    const Colors& aColors) {
  MOZ_ASSERT(aAppearance == StyleAppearance::Checkbox ||
             aAppearance == StyleAppearance::Radio);

  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isChecked = aState.HasState(NS_EVENT_STATE_CHECKED);
  bool isIndeterminate = aAppearance == StyleAppearance::Checkbox &&
                         aState.HasState(NS_EVENT_STATE_INDETERMINATE);

  if (isChecked || isIndeterminate) {
    if (isDisabled) {
      auto color = ComputeBorderColor(aState, aColors, OutlineCoversBorder::No);
      return std::make_pair(color, color);
    }

    bool isActive =
        aState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE);
    bool isHovered = aState.HasState(NS_EVENT_STATE_HOVER);
    const auto& color = isActive    ? aColors.Accent().GetDarker()
                        : isHovered ? aColors.Accent().GetDark()
                                    : aColors.Accent().Get();
    return std::make_pair(color, color);
  }

  return ComputeTextfieldColors(aState, aColors, OutlineCoversBorder::No);
}

sRGBColor nsNativeBasicTheme::ComputeCheckmarkColor(const EventStates& aState,
                                                    const Colors& aColors) {
  if (aColors.HighContrast()) {
    return aColors.System(StyleSystemColor::Selecteditemtext);
  }
  if (aState.HasState(NS_EVENT_STATE_DISABLED)) {
    return sRGBColor::White(.8f);
  }
  return aColors.Accent().GetForeground();
}

sRGBColor nsNativeBasicTheme::ComputeBorderColor(
    const EventStates& aState, const Colors& aColors,
    OutlineCoversBorder aOutlineCoversBorder) {
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  if (aColors.HighContrast()) {
    return aColors.System(isDisabled ? StyleSystemColor::Graytext
                                     : StyleSystemColor::Buttontext);
  }
  bool isActive =
      aState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE);
  bool isHovered = aState.HasState(NS_EVENT_STATE_HOVER);
  bool isFocused = aState.HasState(NS_EVENT_STATE_FOCUSRING);
  if (isDisabled) {
    return sColorGrey40Alpha50;
  }
  if (isFocused && aOutlineCoversBorder == OutlineCoversBorder::Yes) {
    // If we draw the outline over the border, prevent issues where the border
    // shows underneath if it snaps in the wrong direction by using a
    // transparent border. An alternative to this is ensuring that we snap the
    // offset in PaintRoundedFocusRect the same was a we snap border widths, so
    // that negative offsets are guaranteed to cover the border.
    // But this looks harder to mess up.
    return sTransparent;
  }
  bool dark = aColors.IsDark();
  if (isActive) {
    return dark ? sColorGrey20 : sColorGrey60;
  }
  if (isHovered) {
    return dark ? sColorGrey30 : sColorGrey50;
  }
  return sColorGrey40;
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicTheme::ComputeButtonColors(
    const EventStates& aState, const Colors& aColors, nsIFrame* aFrame) {
  bool isActive =
      aState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE);
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isHovered = aState.HasState(NS_EVENT_STATE_HOVER);

  const sRGBColor backgroundColor = [&] {
    if (isDisabled) {
      return aColors.System(StyleSystemColor::MozButtondisabledface);
    }
    if (isActive) {
      return aColors.System(StyleSystemColor::MozButtonactiveface);
    }
    if (isHovered) {
      return aColors.System(StyleSystemColor::MozButtonhoverface);
    }
    return aColors.System(StyleSystemColor::Buttonface);
  }();

  const sRGBColor borderColor =
      ComputeBorderColor(aState, aColors, OutlineCoversBorder::Yes);
  return std::make_pair(backgroundColor, borderColor);
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicTheme::ComputeTextfieldColors(
    const EventStates& aState, const Colors& aColors,
    OutlineCoversBorder aOutlineCoversBorder) {
  nscolor backgroundColor = [&] {
    if (aState.HasState(NS_EVENT_STATE_DISABLED)) {
      return aColors.SystemNs(StyleSystemColor::MozDisabledfield);
    }
    return aColors.SystemNs(StyleSystemColor::Field);
  }();

  if (aState.HasState(NS_EVENT_STATE_AUTOFILL)) {
    backgroundColor = NS_ComposeColors(
        backgroundColor,
        aColors.SystemNs(StyleSystemColor::MozAutofillBackground));
  }

  const sRGBColor borderColor =
      ComputeBorderColor(aState, aColors, aOutlineCoversBorder);
  return std::make_pair(sRGBColor::FromABGR(backgroundColor), borderColor);
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicTheme::ComputeRangeProgressColors(
    const EventStates& aState, const Colors& aColors) {
  if (aColors.HighContrast()) {
    return aColors.SystemPair(StyleSystemColor::Selecteditem,
                              StyleSystemColor::Buttontext);
  }

  bool isActive =
      aState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE);
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isHovered = aState.HasState(NS_EVENT_STATE_HOVER);

  if (isDisabled) {
    return std::make_pair(sColorGrey40Alpha50, sColorGrey40Alpha50);
  }
  if (isActive || isHovered) {
    return std::make_pair(aColors.Accent().GetDark(),
                          aColors.Accent().GetDarker());
  }
  return std::make_pair(aColors.Accent().Get(), aColors.Accent().GetDark());
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicTheme::ComputeRangeTrackColors(
    const EventStates& aState, const Colors& aColors) {
  if (aColors.HighContrast()) {
    return aColors.SystemPair(StyleSystemColor::Window,
                              StyleSystemColor::Buttontext);
  }
  bool isActive =
      aState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE);
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isHovered = aState.HasState(NS_EVENT_STATE_HOVER);

  if (isDisabled) {
    return std::make_pair(sColorGrey10Alpha50, sColorGrey40Alpha50);
  }
  if (isActive || isHovered) {
    return std::make_pair(sColorGrey20, sColorGrey50);
  }
  return std::make_pair(sColorGrey10, sColorGrey40);
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicTheme::ComputeRangeThumbColors(
    const EventStates& aState, const Colors& aColors) {
  if (aColors.HighContrast()) {
    return aColors.SystemPair(StyleSystemColor::Selecteditemtext,
                              StyleSystemColor::Selecteditem);
  }

  bool isActive =
      aState.HasAllStates(NS_EVENT_STATE_HOVER | NS_EVENT_STATE_ACTIVE);
  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isHovered = aState.HasState(NS_EVENT_STATE_HOVER);

  const sRGBColor& backgroundColor = [&] {
    if (isDisabled) {
      return sColorGrey40;
    }
    if (isActive) {
      return aColors.Accent().Get();
    }
    if (isHovered) {
      return sColorGrey60;
    }
    return sColorGrey50;
  }();

  const sRGBColor borderColor = sRGBColor::OpaqueWhite();
  return std::make_pair(backgroundColor, borderColor);
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicTheme::ComputeProgressColors(
    const Colors& aColors) {
  if (aColors.HighContrast()) {
    return aColors.SystemPair(StyleSystemColor::Selecteditem,
                              StyleSystemColor::Buttontext);
  }
  return std::make_pair(aColors.Accent().Get(), aColors.Accent().GetDark());
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicTheme::ComputeProgressTrackColors(
    const Colors& aColors) {
  if (aColors.HighContrast()) {
    return aColors.SystemPair(StyleSystemColor::Buttonface,
                              StyleSystemColor::Buttontext);
  }
  return std::make_pair(sColorGrey10, sColorGrey40);
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicTheme::ComputeMeterchunkColors(
    const EventStates& aMeterState, const Colors& aColors) {
  if (aColors.HighContrast()) {
    return ComputeProgressColors(aColors);
  }
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

std::array<sRGBColor, 3> nsNativeBasicTheme::ComputeFocusRectColors(
    const Colors& aColors) {
  if (aColors.HighContrast()) {
    return {aColors.System(StyleSystemColor::Selecteditem),
            aColors.System(StyleSystemColor::Buttontext),
            aColors.System(StyleSystemColor::Window)};
  }
  const auto& accent = aColors.Accent();
  const sRGBColor middle =
      aColors.IsDark() ? sRGBColor::Black(.3f) : sRGBColor::White(.3f);
  return {accent.Get(), middle, accent.GetLight()};
}

static const CSSCoord kInnerFocusOutlineWidth = 2.0f;

template <typename PaintBackendData>
void nsNativeBasicTheme::PaintRoundedFocusRect(PaintBackendData& aBackendData,
                                               const LayoutDeviceRect& aRect,
                                               const Colors& aColors,
                                               DPIRatio aDpiRatio,
                                               CSSCoord aRadius,
                                               CSSCoord aOffset) {
  // NOTE(emilio): If the widths or offsets here change, make sure to tweak
  // the GetWidgetOverflow path for FocusOutline.
  auto [innerColor, middleColor, outerColor] = ComputeFocusRectColors(aColors);

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
      LayoutDeviceCoord(ThemeDrawing::SnapBorderWidth(strokeWidth, aDpiRatio));
  CSSCoord strokeRadius = aRadius;
  focusRect.Inflate(aOffset * aDpiRatio + strokeWidthDevPx);

  ThemeDrawing::PaintRoundedRectWithRadius(
      aBackendData, focusRect, sTransparent, innerColor, strokeWidth,
      strokeRadius, aDpiRatio);

  strokeWidth = CSSCoord(1.0f);
  strokeWidthDevPx =
      LayoutDeviceCoord(ThemeDrawing::SnapBorderWidth(strokeWidth, aDpiRatio));
  strokeRadius += strokeWidth;
  focusRect.Inflate(strokeWidthDevPx);

  ThemeDrawing::PaintRoundedRectWithRadius(
      aBackendData, focusRect, sTransparent, middleColor, strokeWidth,
      strokeRadius, aDpiRatio);

  strokeWidth = CSSCoord(2.0f);
  strokeWidthDevPx =
      LayoutDeviceCoord(ThemeDrawing::SnapBorderWidth(strokeWidth, aDpiRatio));
  strokeRadius += strokeWidth;
  focusRect.Inflate(strokeWidthDevPx);

  ThemeDrawing::PaintRoundedRectWithRadius(
      aBackendData, focusRect, sTransparent, outerColor, strokeWidth,
      strokeRadius, aDpiRatio);
}

void nsNativeBasicTheme::PaintCheckboxControl(DrawTarget& aDrawTarget,
                                              const LayoutDeviceRect& aRect,
                                              const EventStates& aState,
                                              const Colors& aColors,
                                              DPIRatio aDpiRatio) {
  auto [backgroundColor, borderColor] =
      ComputeCheckboxColors(aState, StyleAppearance::Checkbox, aColors);
  {
    const CSSCoord radius = 2.0f;
    CSSCoord borderWidth = kCheckboxRadioBorderWidth;
    if (backgroundColor == borderColor) {
      borderWidth = 0.0f;
    }
    ThemeDrawing::PaintRoundedRectWithRadius(aDrawTarget, aRect,
                                             backgroundColor, borderColor,
                                             borderWidth, radius, aDpiRatio);
  }

  if (aState.HasState(NS_EVENT_STATE_INDETERMINATE)) {
    PaintIndeterminateMark(aDrawTarget, aRect, aState, aColors);
  } else if (aState.HasState(NS_EVENT_STATE_CHECKED)) {
    PaintCheckMark(aDrawTarget, aRect, aState, aColors);
  }

  if (aState.HasState(NS_EVENT_STATE_FOCUSRING)) {
    PaintRoundedFocusRect(aDrawTarget, aRect, aColors, aDpiRatio, 5.0f, 1.0f);
  }
}

constexpr CSSCoord kCheckboxRadioContentBoxSize = 10.0f;
constexpr CSSCoord kCheckboxRadioBorderBoxSize =
    kCheckboxRadioContentBoxSize + kCheckboxRadioBorderWidth * 2.0f;

void nsNativeBasicTheme::PaintCheckMark(DrawTarget& aDrawTarget,
                                        const LayoutDeviceRect& aRect,
                                        const EventStates& aState,
                                        const Colors& aColors) {
  // Points come from the coordinates on a 14X14 (kCheckboxRadioBorderBoxSize)
  // unit box centered at 0,0
  const float checkPolygonX[] = {-4.5f, -1.5f, -0.5f, 5.0f, 4.75f,
                                 3.5f,  -0.5f, -1.5f, -3.5f};
  const float checkPolygonY[] = {0.5f,  4.0f, 4.0f,  -2.5f, -4.0f,
                                 -4.0f, 1.0f, 1.25f, -1.0f};
  const int32_t checkNumPoints = sizeof(checkPolygonX) / sizeof(float);
  const float scale =
      ThemeDrawing::ScaleToFillRect(aRect, kCheckboxRadioBorderBoxSize);
  auto center = aRect.Center().ToUnknownPoint();

  RefPtr<PathBuilder> builder = aDrawTarget.CreatePathBuilder();
  Point p = center + Point(checkPolygonX[0] * scale, checkPolygonY[0] * scale);
  builder->MoveTo(p);
  for (int32_t i = 1; i < checkNumPoints; i++) {
    p = center + Point(checkPolygonX[i] * scale, checkPolygonY[i] * scale);
    builder->LineTo(p);
  }
  RefPtr<Path> path = builder->Finish();

  sRGBColor fillColor = ComputeCheckmarkColor(aState, aColors);
  aDrawTarget.Fill(path, ColorPattern(ToDeviceColor(fillColor)));
}

void nsNativeBasicTheme::PaintIndeterminateMark(DrawTarget& aDrawTarget,
                                                const LayoutDeviceRect& aRect,
                                                const EventStates& aState,
                                                const Colors& aColors) {
  const CSSCoord borderWidth = 2.0f;
  const float scale =
      ThemeDrawing::ScaleToFillRect(aRect, kCheckboxRadioBorderBoxSize);

  Rect rect = aRect.ToUnknownRect();
  rect.y += (rect.height / 2) - (borderWidth * scale / 2);
  rect.height = borderWidth * scale;
  rect.x += (borderWidth * scale) + (borderWidth * scale / 8);
  rect.width -= ((borderWidth * scale) + (borderWidth * scale / 8)) * 2;

  sRGBColor fillColor = ComputeCheckmarkColor(aState, aColors);
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
  ThemeDrawing::PaintRoundedRectWithRadius(aPaintData, aRect, aBackgroundColor,
                                           aBorderColor, aBorderWidth, radius,
                                           aDpiRatio);
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
                                           const Colors& aColors,
                                           DPIRatio aDpiRatio) {
  auto [backgroundColor, borderColor] =
      ComputeCheckboxColors(aState, StyleAppearance::Radio, aColors);
  {
    CSSCoord borderWidth = kCheckboxRadioBorderWidth;
    if (backgroundColor == borderColor) {
      borderWidth = 0.0f;
    }
    PaintStrokedCircle(aPaintData, aRect, backgroundColor, borderColor,
                       borderWidth, aDpiRatio);
  }

  if (aState.HasState(NS_EVENT_STATE_CHECKED)) {
    LayoutDeviceRect rect(aRect);
    auto width = LayoutDeviceCoord(
        ThemeDrawing::SnapBorderWidth(kCheckboxRadioBorderWidth, aDpiRatio));
    rect.Deflate(width);

    auto checkColor = ComputeCheckmarkColor(aState, aColors);
    PaintStrokedCircle(aPaintData, rect, backgroundColor, checkColor,
                       kCheckboxRadioBorderWidth, aDpiRatio);
  }

  if (aState.HasState(NS_EVENT_STATE_FOCUSRING)) {
    PaintRoundedFocusRect(aPaintData, aRect, aColors, aDpiRatio, 5.0f, 1.0f);
  }
}

template <typename PaintBackendData>
void nsNativeBasicTheme::PaintTextField(PaintBackendData& aPaintData,
                                        const LayoutDeviceRect& aRect,
                                        const EventStates& aState,
                                        const Colors& aColors,
                                        DPIRatio aDpiRatio) {
  auto [backgroundColor, borderColor] =
      ComputeTextfieldColors(aState, aColors, OutlineCoversBorder::Yes);

  const CSSCoord radius = 2.0f;

  ThemeDrawing::PaintRoundedRectWithRadius(aPaintData, aRect, backgroundColor,
                                           borderColor, kTextFieldBorderWidth,
                                           radius, aDpiRatio);

  if (aState.HasState(NS_EVENT_STATE_FOCUSRING)) {
    PaintRoundedFocusRect(aPaintData, aRect, aColors, aDpiRatio,
                          radius + kTextFieldBorderWidth,
                          -kTextFieldBorderWidth);
  }
}

template <typename PaintBackendData>
void nsNativeBasicTheme::PaintListbox(PaintBackendData& aPaintData,
                                      const LayoutDeviceRect& aRect,
                                      const EventStates& aState,
                                      const Colors& aColors,
                                      DPIRatio aDpiRatio) {
  const CSSCoord radius = 2.0f;
  auto [backgroundColor, borderColor] =
      ComputeTextfieldColors(aState, aColors, OutlineCoversBorder::Yes);

  ThemeDrawing::PaintRoundedRectWithRadius(aPaintData, aRect, backgroundColor,
                                           borderColor, kMenulistBorderWidth,
                                           radius, aDpiRatio);

  if (aState.HasState(NS_EVENT_STATE_FOCUSRING)) {
    PaintRoundedFocusRect(aPaintData, aRect, aColors, aDpiRatio,
                          radius + kMenulistBorderWidth, -kMenulistBorderWidth);
  }
}

template <typename PaintBackendData>
void nsNativeBasicTheme::PaintMenulist(PaintBackendData& aDrawTarget,
                                       const LayoutDeviceRect& aRect,
                                       const EventStates& aState,
                                       const Colors& aColors,
                                       DPIRatio aDpiRatio) {
  const CSSCoord radius = 4.0f;
  auto [backgroundColor, borderColor] = ComputeButtonColors(aState, aColors);

  ThemeDrawing::PaintRoundedRectWithRadius(aDrawTarget, aRect, backgroundColor,
                                           borderColor, kMenulistBorderWidth,
                                           radius, aDpiRatio);

  if (aState.HasState(NS_EVENT_STATE_FOCUSRING)) {
    PaintRoundedFocusRect(aDrawTarget, aRect, aColors, aDpiRatio,
                          radius + kMenulistBorderWidth, -kMenulistBorderWidth);
  }
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

  const auto arrowColor = sRGBColor::FromABGR(
      nsLayoutUtils::GetColor(aFrame, &nsStyleText::mWebkitTextFillColor));
  ThemeDrawing::PaintArrow(aDrawTarget, aRect, kPolygonX, kPolygonY,
                           kPolygonSize, ArrayLength(kPolygonX), arrowColor);
}

void nsNativeBasicTheme::PaintSpinnerButton(
    nsIFrame* aFrame, DrawTarget& aDrawTarget, const LayoutDeviceRect& aRect,
    const EventStates& aState, StyleAppearance aAppearance,
    const Colors& aColors, DPIRatio aDpiRatio) {
  auto [backgroundColor, borderColor] = ComputeButtonColors(aState, aColors);

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

  ThemeDrawing::PaintArrow(aDrawTarget, aRect, kPolygonX, polygonY,
                           kPolygonSize, ArrayLength(kPolygonX), borderColor);
}

template <typename PaintBackendData>
void nsNativeBasicTheme::PaintRange(nsIFrame* aFrame,
                                    PaintBackendData& aPaintData,
                                    const LayoutDeviceRect& aRect,
                                    const EventStates& aState,
                                    const Colors& aColors, DPIRatio aDpiRatio,
                                    bool aHorizontal) {
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

    thumbRect.y =
        aRect.y + (aRect.height - thumbRect.height) * (1.0 - progress);
    float midPoint = thumbRect.Center().Y();
    trackClipRect.SetBoxY(aRect.Y(), midPoint);
    progressClipRect.SetBoxY(midPoint, aRect.YMost());
  }

  const CSSCoord borderWidth = 1.0f;
  const CSSCoord radius = 3.0f;

  auto [progressColor, progressBorderColor] =
      ComputeRangeProgressColors(aState, aColors);
  auto [trackColor, trackBorderColor] =
      ComputeRangeTrackColors(aState, aColors);

  ThemeDrawing::PaintRoundedRectWithRadius(aPaintData, rect, progressClipRect,
                                           progressColor, progressBorderColor,
                                           borderWidth, radius, aDpiRatio);

  ThemeDrawing::PaintRoundedRectWithRadius(aPaintData, rect, trackClipRect,
                                           trackColor, trackBorderColor,
                                           borderWidth, radius, aDpiRatio);

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
  auto [thumbColor, thumbBorderColor] =
      ComputeRangeThumbColors(aState, aColors);

  PaintStrokedCircle(aPaintData, thumbRect, thumbColor, thumbBorderColor,
                     thumbBorderWidth, aDpiRatio);

  if (aState.HasState(NS_EVENT_STATE_FOCUSRING)) {
    PaintRoundedFocusRect(aPaintData, aRect, aColors, aDpiRatio, radius, 1.0f);
  }
}

template <typename PaintBackendData>
void nsNativeBasicTheme::PaintProgress(nsIFrame* aFrame,
                                       PaintBackendData& aPaintData,
                                       const LayoutDeviceRect& aRect,
                                       const EventStates& aState,
                                       const Colors& aColors,
                                       DPIRatio aDpiRatio, bool aIsMeter) {
  const CSSCoord borderWidth = 1.0f;
  const CSSCoord radius = aIsMeter ? 6.0f : 3.0f;

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

  {
    // Paint the track, unclipped.
    auto [backgroundColor, borderColor] = ComputeProgressTrackColors(aColors);
    ThemeDrawing::PaintRoundedRectWithRadius(aPaintData, rect, rect,
                                             backgroundColor, borderColor,
                                             borderWidth, radius, aDpiRatio);
  }

  // Now paint the chunk, clipped as needed.
  LayoutDeviceRect clipRect = rect;
  if (aState.HasState(NS_EVENT_STATE_INDETERMINATE)) {
    // For indeterminate progress, we paint an animated chunk of 1/3 of the
    // progress size.
    //
    // Animation speed and math borrowed from GTK.
    const LayoutDeviceCoord size = isHorizontal ? rect.width : rect.height;
    const LayoutDeviceCoord barSize = size * 0.3333f;
    const LayoutDeviceCoord travel = 2.0f * (size - barSize);

    // Period equals to travel / pixelsPerMillisecond where pixelsPerMillisecond
    // equals progressSize / 1000.0.  This is equivalent to 1600.
    const unsigned kPeriod = 1600;

    const int t = PR_IntervalToMilliseconds(PR_IntervalNow()) % kPeriod;
    const LayoutDeviceCoord dx = travel * float(t) / float(kPeriod);
    if (isHorizontal) {
      rect.width = barSize;
      rect.x += (dx < travel * .5f) ? dx : travel - dx;
    } else {
      rect.height = barSize;
      rect.y += (dx < travel * .5f) ? dx : travel - dx;
    }
    clipRect = rect;
    // Queue the next frame if needed.
    if (!QueueAnimatedContentForRefresh(aFrame->GetContent(), 60)) {
      NS_WARNING("Couldn't refresh indeterminate <progress>");
    }
  } else {
    // This is the progress chunk, clip it to the right amount.
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

  auto [backgroundColor, borderColor] =
      aIsMeter ? ComputeMeterchunkColors(aState, aColors)
               : ComputeProgressColors(aColors);
  ThemeDrawing::PaintRoundedRectWithRadius(aPaintData, rect, clipRect,
                                           backgroundColor, borderColor,
                                           borderWidth, radius, aDpiRatio);
}

template <typename PaintBackendData>
void nsNativeBasicTheme::PaintButton(nsIFrame* aFrame,
                                     PaintBackendData& aPaintData,
                                     const LayoutDeviceRect& aRect,
                                     const EventStates& aState,
                                     const Colors& aColors,
                                     DPIRatio aDpiRatio) {
  const CSSCoord radius = 4.0f;
  auto [backgroundColor, borderColor] =
      ComputeButtonColors(aState, aColors, aFrame);

  ThemeDrawing::PaintRoundedRectWithRadius(aPaintData, aRect, backgroundColor,
                                           borderColor, kButtonBorderWidth,
                                           radius, aDpiRatio);

  if (aState.HasState(NS_EVENT_STATE_FOCUSRING)) {
    PaintRoundedFocusRect(aPaintData, aRect, aColors, aDpiRatio,
                          radius + kButtonBorderWidth, -kButtonBorderWidth);
  }
}

NS_IMETHODIMP
nsNativeBasicTheme::DrawWidgetBackground(gfxContext* aContext, nsIFrame* aFrame,
                                         StyleAppearance aAppearance,
                                         const nsRect& aRect,
                                         const nsRect& /* aDirtyRect */,
                                         DrawOverflow aDrawOverflow) {
  if (!DoDrawWidgetBackground(*aContext->GetDrawTarget(), aFrame, aAppearance,
                              aRect, aDrawOverflow)) {
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
  return DoDrawWidgetBackground(data, aFrame, aAppearance, aRect,
                                DrawOverflow::Yes);
}

static LayoutDeviceRect ToSnappedRect(const nsRect& aRect,
                                      nscoord aTwipsPerPixel, DrawTarget& aDt) {
  return LayoutDeviceRect::FromUnknownRect(
      NSRectToSnappedRect(aRect, aTwipsPerPixel, aDt));
}

static LayoutDeviceRect ToSnappedRect(const nsRect& aRect,
                                      nscoord aTwipsPerPixel,
                                      WebRenderBackendData& aDt) {
  // TODO: Do we need to do any more snapping here?
  return LayoutDeviceRect::FromAppUnits(aRect, aTwipsPerPixel);
}

template <typename PaintBackendData>
bool nsNativeBasicTheme::DoDrawWidgetBackground(PaintBackendData& aPaintData,
                                                nsIFrame* aFrame,
                                                StyleAppearance aAppearance,
                                                const nsRect& aRect,
                                                DrawOverflow aDrawOverflow) {
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

  // Don't paint the outline if we're asked not to draw overflow, or if the
  // author has specified another kind of outline on focus.
  if (aDrawOverflow == DrawOverflow::No ||
      !aFrame->StyleOutline()->mOutlineStyle.IsAuto()) {
    eventState &= ~NS_EVENT_STATE_FOCUSRING;
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

  const Colors colors(aFrame);
  DPIRatio dpiRatio = GetDPIRatio(aFrame, aAppearance);

  switch (aAppearance) {
    case StyleAppearance::Radio: {
      auto rect = CheckBoxRadioRect(devPxRect);
      PaintRadioControl(aPaintData, rect, eventState, colors, dpiRatio);
      break;
    }
    case StyleAppearance::Checkbox: {
      if constexpr (std::is_same_v<PaintBackendData, WebRenderBackendData>) {
        // TODO: Need to figure out how to best draw this using WR.
        return false;
      } else {
        auto rect = CheckBoxRadioRect(devPxRect);
        PaintCheckboxControl(aPaintData, rect, eventState, colors, dpiRatio);
      }
      break;
    }
    case StyleAppearance::Textarea:
    case StyleAppearance::Textfield:
    case StyleAppearance::NumberInput:
      PaintTextField(aPaintData, devPxRect, eventState, colors, dpiRatio);
      break;
    case StyleAppearance::Listbox:
      PaintListbox(aPaintData, devPxRect, eventState, colors, dpiRatio);
      break;
    case StyleAppearance::MenulistButton:
    case StyleAppearance::Menulist:
      PaintMenulist(aPaintData, devPxRect, eventState, colors, dpiRatio);
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
                           aAppearance, colors, dpiRatio);
      }
      break;
    case StyleAppearance::Range:
      PaintRange(aFrame, aPaintData, devPxRect, eventState, colors, dpiRatio,
                 IsRangeHorizontal(aFrame));
      break;
    case StyleAppearance::RangeThumb:
      // Painted as part of StyleAppearance::Range.
      break;
    case StyleAppearance::ProgressBar:
      PaintProgress(aFrame, aPaintData, devPxRect, eventState, colors, dpiRatio,
                    /* aIsMeter = */ false);
      break;
    case StyleAppearance::Progresschunk:
      /* Painted as part of the progress bar */
      break;
    case StyleAppearance::Meter:
      PaintProgress(aFrame, aPaintData, devPxRect, eventState, colors, dpiRatio,
                    /* aIsMeter = */ true);
      break;
    case StyleAppearance::Meterchunk:
      /* Painted as part of the meter bar */
      break;
    case StyleAppearance::ScrollbarthumbHorizontal:
    case StyleAppearance::ScrollbarthumbVertical: {
      bool isHorizontal =
          aAppearance == StyleAppearance::ScrollbarthumbHorizontal;
      return GetScrollbarDrawing().PaintScrollbarThumb(
          aPaintData, devPxRect, isHorizontal, aFrame,
          *nsLayoutUtils::StyleForScrollbar(aFrame), eventState, docState,
          colors, dpiRatio);
    }
    case StyleAppearance::ScrollbartrackHorizontal:
    case StyleAppearance::ScrollbartrackVertical: {
      bool isHorizontal =
          aAppearance == StyleAppearance::ScrollbartrackHorizontal;
      return GetScrollbarDrawing().PaintScrollbarTrack(
          aPaintData, devPxRect, isHorizontal, aFrame,
          *nsLayoutUtils::StyleForScrollbar(aFrame), docState, colors,
          dpiRatio);
    }
    case StyleAppearance::ScrollbarHorizontal:
    case StyleAppearance::ScrollbarVertical: {
      bool isHorizontal = aAppearance == StyleAppearance::ScrollbarHorizontal;
      return GetScrollbarDrawing().PaintScrollbar(
          aPaintData, devPxRect, isHorizontal, aFrame,
          *nsLayoutUtils::StyleForScrollbar(aFrame), eventState, docState,
          colors, dpiRatio);
    }
    case StyleAppearance::Scrollcorner:
      return GetScrollbarDrawing().PaintScrollCorner(
          aPaintData, devPxRect, aFrame,
          *nsLayoutUtils::StyleForScrollbar(aFrame), docState, colors,
          dpiRatio);
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonDown:
    case StyleAppearance::ScrollbarbuttonLeft:
    case StyleAppearance::ScrollbarbuttonRight:
      // For scrollbar-width:thin, we don't display the buttons.
      if (!ScrollbarDrawing::IsScrollbarWidthThin(aFrame)) {
        if constexpr (std::is_same_v<PaintBackendData, WebRenderBackendData>) {
          // TODO: Need to figure out how to best draw this using WR.
          return false;
        } else {
          GetScrollbarDrawing().PaintScrollbarButton(
              aPaintData, aAppearance, devPxRect, aFrame,
              *nsLayoutUtils::StyleForScrollbar(aFrame), eventState, docState,
              colors, dpiRatio);
        }
      }
      break;
    case StyleAppearance::Button:
      PaintButton(aFrame, aPaintData, devPxRect, eventState, colors, dpiRatio);
      break;
    case StyleAppearance::FocusOutline:
      PaintAutoStyleOutline(aFrame, aPaintData, devPxRect, colors, dpiRatio);
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
                                               const Colors& aColors,
                                               DPIRatio aDpiRatio) {
  auto [innerColor, middleColor, outerColor] = ComputeFocusRectColors(aColors);
  Unused << middleColor;
  Unused << outerColor;

  LayoutDeviceRect rect(aRect);
  auto width = LayoutDeviceCoord(
      ThemeDrawing::SnapBorderWidth(kInnerFocusOutlineWidth, aDpiRatio));
  rect.Inflate(width);

  const nscoord offset = aFrame->StyleOutline()->mOutlineOffset.ToAppUnits();
  nscoord cssRadii[8];
  if (!aFrame->GetBorderRadii(cssRadii)) {
    const CSSCoord cssOffset = CSSCoord::FromAppUnits(offset);
    const CSSCoord radius =
        cssOffset >= 0.0f
            ? kInnerFocusOutlineWidth
            : std::max(kInnerFocusOutlineWidth + cssOffset, CSSCoord(0.0f));
    return ThemeDrawing::PaintRoundedRectWithRadius(
        aPaintData, rect, sRGBColor::White(0.0f), innerColor,
        kInnerFocusOutlineWidth, radius, aDpiRatio);
  }

  nsPresContext* pc = aFrame->PresContext();
  const Float devPixelOffset = pc->AppUnitsToFloatDevPixels(offset);

  RectCornerRadii innerRadii;
  nsCSSRendering::ComputePixelRadii(cssRadii, pc->AppUnitsPerDevPixel(),
                                    &innerRadii);

  const auto borderColor = ToDeviceColor(innerColor);
  // NOTE(emilio): This doesn't use PaintRoundedRectWithRadius because we need
  // to support arbitrary radii.
  RectCornerRadii outerRadii;
  if constexpr (std::is_same_v<PaintBackendData, WebRenderBackendData>) {
    const Float widths[4] = {width + devPixelOffset, width + devPixelOffset,
                             width + devPixelOffset, width + devPixelOffset};
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
          ThemeDrawing::SnapBorderWidth(kCheckboxRadioBorderWidth, dpiRatio);
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
                                           StyleScrollbarWidth aWidth,
                                           Overlay aOverlay) -> ScrollbarSizes {
  return GetScrollbarDrawing().GetScrollbarSizes(aPresContext, aWidth,
                                                 aOverlay);
}

nscoord nsNativeBasicTheme::GetCheckboxRadioPrefSize() {
  return CSSPixel::ToAppUnits(kCheckboxRadioContentBoxSize);
}

/* static */
UniquePtr<ScrollbarDrawing>
nsNativeBasicTheme::DetermineScrollbarStyleSetByPrefs() {
  switch (StaticPrefs::widget_non_native_theme_scrollbar_style()) {
    case 1:
      return MakeUnique<ScrollbarDrawingCocoa>();
    case 2:
      return MakeUnique<ScrollbarDrawingGTK>();
    case 3:
      return MakeUnique<ScrollbarDrawingAndroid>();
    case 4:
      return MakeUnique<ScrollbarDrawingWin>();
    default:
      return nullptr;
  }
}

/* static */
UniquePtr<ScrollbarDrawing> nsNativeBasicTheme::DetermineScrollbarStyle() {
  // Check if a preferred scrollbar style is set via prefs.
  if (UniquePtr<ScrollbarDrawing> scrollbarDrawing =
          DetermineScrollbarStyleSetByPrefs()) {
    return scrollbarDrawing;
  }
  // Default to native scrollbar style for each platform.
#ifdef XP_WIN
  return MakeUnique<ScrollbarDrawingWin>();
#elif MOZ_WIDGET_COCOA
  return MakeUnique<ScrollbarDrawingCocoa>();
#elif MOZ_WIDGET_GTK
  return MakeUnique<ScrollbarDrawingGTK>();
#elif ANDROID
  return MakeUnique<ScrollbarDrawingAndroid>();
#else
#  error "Unknown platform, need scrollbar implementation."
#endif
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

  if (IsWidgetScrollbarPart(aAppearance)) {
    *aResult = GetScrollbarDrawing().GetMinimumWidgetSize(aPresContext,
                                                          aAppearance, aFrame);
    return NS_OK;
  }

  switch (aAppearance) {
    case StyleAppearance::Button:
      if (aFrame->IsColorControlFrame()) {
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
    default:
      break;
  }

  return NS_OK;
}

nsITheme::Transparency nsNativeBasicTheme::GetWidgetTransparency(
    nsIFrame* aFrame, StyleAppearance aAppearance) {
  return GetScrollbarDrawing()
      .GetScrollbarPartTransparency(aFrame, aAppearance)
      .valueOr(eUnknownTransparency);
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
    if (aAttribute == nsGkAtoms::disabled || aAttribute == nsGkAtoms::checked ||
        aAttribute == nsGkAtoms::selected ||
        aAttribute == nsGkAtoms::visuallyselected ||
        aAttribute == nsGkAtoms::menuactive ||
        aAttribute == nsGkAtoms::sortDirection ||
        aAttribute == nsGkAtoms::focused || aAttribute == nsGkAtoms::_default ||
        aAttribute == nsGkAtoms::open || aAttribute == nsGkAtoms::hover) {
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

bool nsNativeBasicTheme::ThemeDrawsFocusForWidget(nsIFrame*, StyleAppearance) {
  return true;
}

bool nsNativeBasicTheme::ThemeNeedsComboboxDropmarker() { return true; }

bool nsNativeBasicTheme::ThemeSupportsScrollbarButtons() {
  return GetScrollbarDrawing().ShouldDrawScrollbarButtons();
}
