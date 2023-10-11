/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_Theme_h
#define mozilla_widget_Theme_h

#include "Units.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/Types.h"
#include "nsITheme.h"
#include "nsNativeTheme.h"
#include "ScrollbarDrawing.h"

namespace mozilla {

enum class StyleSystemColor : uint8_t;

namespace widget {

class Theme : protected nsNativeTheme, public nsITheme {
 protected:
  using sRGBColor = gfx::sRGBColor;
  using DrawTarget = gfx::DrawTarget;
  using Path = gfx::Path;
  using Rect = gfx::Rect;
  using Point = gfx::Point;
  using RectCornerRadii = gfx::RectCornerRadii;
  using Colors = ThemeColors;
  using AccentColor = ThemeAccentColor;
  using ElementState = dom::ElementState;

 public:
  explicit Theme(UniquePtr<ScrollbarDrawing>&& aScrollbarDrawing)
      : mScrollbarDrawing(std::move(aScrollbarDrawing)) {
    mScrollbarDrawing->RecomputeScrollbarParams();
  }

  static void Init();
  static void Shutdown();
  static void LookAndFeelChanged();

  using DPIRatio = CSSToLayoutDeviceScale;

  NS_DECL_ISUPPORTS_INHERITED

  // The nsITheme interface.
  NS_IMETHOD DrawWidgetBackground(gfxContext* aContext, nsIFrame*,
                                  StyleAppearance, const nsRect& aRect,
                                  const nsRect& aDirtyRect,
                                  DrawOverflow) override;

  bool CreateWebRenderCommandsForWidget(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const layers::StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager, nsIFrame*, StyleAppearance,
      const nsRect& aRect) override;

  // PaintBackendData will be either a DrawTarget, or a WebRenderBackendData.
  //
  // The return value represents whether the widget could be painted with the
  // given back-end.
  template <typename PaintBackendData>
  bool DoDrawWidgetBackground(PaintBackendData&, nsIFrame*, StyleAppearance,
                              const nsRect&, DrawOverflow);

  [[nodiscard]] LayoutDeviceIntMargin GetWidgetBorder(nsDeviceContext* aContext,
                                                      nsIFrame*,
                                                      StyleAppearance) override;
  bool GetWidgetPadding(nsDeviceContext* aContext, nsIFrame*, StyleAppearance,
                        LayoutDeviceIntMargin* aResult) override;
  bool GetWidgetOverflow(nsDeviceContext* aContext, nsIFrame*, StyleAppearance,
                         nsRect* aOverflowRect) override;
  LayoutDeviceIntSize GetMinimumWidgetSize(nsPresContext*, nsIFrame*,
                                           StyleAppearance) override;
  Transparency GetWidgetTransparency(nsIFrame*, StyleAppearance) override;
  NS_IMETHOD WidgetStateChanged(nsIFrame*, StyleAppearance, nsAtom* aAttribute,
                                bool* aShouldRepaint,
                                const nsAttrValue* aOldValue) override;
  NS_IMETHOD ThemeChanged() override;
  bool WidgetAppearanceDependsOnWindowFocus(StyleAppearance) override;
  /*bool NeedToClearBackgroundBehindWidget(
      nsIFrame*, StyleAppearance) override;*/
  ThemeGeometryType ThemeGeometryTypeForWidget(nsIFrame*,
                                               StyleAppearance) override;
  bool ThemeSupportsWidget(nsPresContext*, nsIFrame*, StyleAppearance) override;
  bool WidgetIsContainer(StyleAppearance) override;
  bool ThemeDrawsFocusForWidget(nsIFrame*, StyleAppearance) override;
  bool ThemeNeedsComboboxDropmarker() override;

  LayoutDeviceIntCoord GetScrollbarSize(const nsPresContext*,
                                        StyleScrollbarWidth, Overlay) final;

  nscoord GetCheckboxRadioPrefSize() override;

  static UniquePtr<ScrollbarDrawing> ScrollbarStyle();

 protected:
  virtual ~Theme() = default;

  DPIRatio GetDPIRatio(nsPresContext*, StyleAppearance);
  DPIRatio GetDPIRatio(nsIFrame*, StyleAppearance);

  std::tuple<sRGBColor, sRGBColor, sRGBColor> ComputeCheckboxColors(
      const ElementState&, StyleAppearance, const Colors&);
  enum class OutlineCoversBorder : bool { No, Yes };
  sRGBColor ComputeBorderColor(const ElementState&, const Colors&,
                               OutlineCoversBorder);

  std::pair<sRGBColor, sRGBColor> ComputeButtonColors(const ElementState&,
                                                      const Colors&,
                                                      nsIFrame* = nullptr);
  std::pair<sRGBColor, sRGBColor> ComputeTextfieldColors(const ElementState&,
                                                         const Colors&,
                                                         OutlineCoversBorder);
  std::pair<sRGBColor, sRGBColor> ComputeRangeProgressColors(
      const ElementState&, const Colors&);
  std::pair<sRGBColor, sRGBColor> ComputeRangeTrackColors(const ElementState&,
                                                          const Colors&);
  std::pair<sRGBColor, sRGBColor> ComputeRangeThumbColors(const ElementState&,
                                                          const Colors&);
  std::pair<sRGBColor, sRGBColor> ComputeProgressColors(const Colors&);
  std::pair<sRGBColor, sRGBColor> ComputeProgressTrackColors(const Colors&);
  std::pair<sRGBColor, sRGBColor> ComputeMeterchunkColors(
      const ElementState& aMeterState, const Colors&);
  std::array<sRGBColor, 3> ComputeFocusRectColors(const Colors&);

  template <typename PaintBackendData>
  void PaintRoundedFocusRect(PaintBackendData&, const LayoutDeviceRect&,
                             const Colors&, DPIRatio, CSSCoord aRadius,
                             CSSCoord aOffset);
  template <typename PaintBackendData>
  void PaintAutoStyleOutline(nsIFrame*, PaintBackendData&,
                             const LayoutDeviceRect&, const Colors&, DPIRatio);

  void PaintCheckboxControl(DrawTarget& aDrawTarget, const LayoutDeviceRect&,
                            const ElementState&, const Colors&, DPIRatio);
  void PaintCheckMark(DrawTarget&, const LayoutDeviceRect&, const sRGBColor&);
  void PaintIndeterminateMark(DrawTarget&, const LayoutDeviceRect&,
                              const sRGBColor&);

  template <typename PaintBackendData>
  void PaintStrokedCircle(PaintBackendData&, const LayoutDeviceRect&,
                          const sRGBColor& aBackgroundColor,
                          const sRGBColor& aBorderColor,
                          const CSSCoord aBorderWidth, DPIRatio);
  void PaintCircleShadow(DrawTarget&, const LayoutDeviceRect& aBoxRect,
                         const LayoutDeviceRect& aClipRect, float aShadowAlpha,
                         const CSSPoint& aShadowOffset,
                         CSSCoord aShadowBlurStdDev, DPIRatio);
  void PaintCircleShadow(WebRenderBackendData&,
                         const LayoutDeviceRect& aBoxRect,
                         const LayoutDeviceRect& aClipRect, float aShadowAlpha,
                         const CSSPoint& aShadowOffset,
                         CSSCoord aShadowBlurStdDev, DPIRatio);
  template <typename PaintBackendData>
  void PaintRadioControl(PaintBackendData&, const LayoutDeviceRect&,
                         const ElementState&, const Colors&, DPIRatio);
  template <typename PaintBackendData>
  void PaintRadioCheckmark(PaintBackendData&, const LayoutDeviceRect&,
                           const ElementState&, DPIRatio);
  template <typename PaintBackendData>
  void PaintTextField(PaintBackendData&, const LayoutDeviceRect&,
                      const ElementState&, const Colors&, DPIRatio);
  template <typename PaintBackendData>
  void PaintListbox(PaintBackendData&, const LayoutDeviceRect&,
                    const ElementState&, const Colors&, DPIRatio);
  template <typename PaintBackendData>
  void PaintMenulist(PaintBackendData&, const LayoutDeviceRect&,
                     const ElementState&, const Colors&, DPIRatio);
  void PaintMenulistArrow(nsIFrame*, DrawTarget&, const LayoutDeviceRect&);
  void PaintSpinnerButton(nsIFrame*, DrawTarget&, const LayoutDeviceRect&,
                          const ElementState&, StyleAppearance, const Colors&,
                          DPIRatio);
  template <typename PaintBackendData>
  void PaintRange(nsIFrame*, PaintBackendData&, const LayoutDeviceRect&,
                  const ElementState&, const Colors&, DPIRatio,
                  bool aHorizontal);
  template <typename PaintBackendData>
  void PaintProgress(nsIFrame*, PaintBackendData&, const LayoutDeviceRect&,
                     const ElementState&, const Colors&, DPIRatio,
                     bool aIsMeter);
  template <typename PaintBackendData>
  void PaintButton(nsIFrame*, PaintBackendData&, const LayoutDeviceRect&,
                   const ElementState&, const Colors&, DPIRatio);

  static void PrefChangedCallback(const char*, void*) {
    LookAndFeel::NotifyChangedAllWindows(ThemeChangeKind::Layout);
  }

  void SetScrollbarDrawing(UniquePtr<ScrollbarDrawing>&& aScrollbarDrawing) {
    mScrollbarDrawing = std::move(aScrollbarDrawing);
    mScrollbarDrawing->RecomputeScrollbarParams();
  }
  ScrollbarDrawing& GetScrollbarDrawing() const { return *mScrollbarDrawing; }
  UniquePtr<ScrollbarDrawing> mScrollbarDrawing;

  bool ThemeSupportsScrollbarButtons() override;
};

}  // namespace widget
}  // namespace mozilla

#endif
