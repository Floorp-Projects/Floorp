/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNativeBasicTheme_h
#define nsNativeBasicTheme_h

#include "Units.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/Types.h"
#include "nsITheme.h"
#include "nsNativeTheme.h"

namespace mozilla {

enum class StyleSystemColor : uint8_t;

namespace widget {

static constexpr gfx::sRGBColor sDefaultAccent(
    gfx::sRGBColor::UnusualFromARGB(0xff0060df));  // Luminance: 13.69346%
static constexpr gfx::sRGBColor sDefaultAccentForeground(
    gfx::sRGBColor::OpaqueWhite());

static constexpr gfx::sRGBColor sColorGrey10(
    gfx::sRGBColor::UnusualFromARGB(0xffe9e9ed));
static constexpr gfx::sRGBColor sColorGrey10Alpha50(
    gfx::sRGBColor::UnusualFromARGB(0x7fe9e9ed));
static constexpr gfx::sRGBColor sColorGrey20(
    gfx::sRGBColor::UnusualFromARGB(0xffd0d0d7));
static constexpr gfx::sRGBColor sColorGrey30(
    gfx::sRGBColor::UnusualFromARGB(0xffb1b1b9));
static constexpr gfx::sRGBColor sColorGrey40(
    gfx::sRGBColor::UnusualFromARGB(0xff8f8f9d));
static constexpr gfx::sRGBColor sColorGrey40Alpha50(
    gfx::sRGBColor::UnusualFromARGB(0x7f8f8f9d));
static constexpr gfx::sRGBColor sColorGrey50(
    gfx::sRGBColor::UnusualFromARGB(0xff676774));
static constexpr gfx::sRGBColor sColorGrey60(
    gfx::sRGBColor::UnusualFromARGB(0xff484851));

static constexpr gfx::sRGBColor sColorMeterGreen10(
    gfx::sRGBColor::UnusualFromARGB(0xff00ab60));
static constexpr gfx::sRGBColor sColorMeterGreen20(
    gfx::sRGBColor::UnusualFromARGB(0xff056139));
static constexpr gfx::sRGBColor sColorMeterYellow10(
    gfx::sRGBColor::UnusualFromARGB(0xffffbd4f));
static constexpr gfx::sRGBColor sColorMeterYellow20(
    gfx::sRGBColor::UnusualFromARGB(0xffd2811e));
static constexpr gfx::sRGBColor sColorMeterRed10(
    gfx::sRGBColor::UnusualFromARGB(0xffe22850));
static constexpr gfx::sRGBColor sColorMeterRed20(
    gfx::sRGBColor::UnusualFromARGB(0xff810220));

static constexpr gfx::sRGBColor sScrollbarColor(
    gfx::sRGBColor::UnusualFromARGB(0xfff0f0f0));
static constexpr gfx::sRGBColor sScrollbarBorderColor(gfx::sRGBColor(1.0f, 1.0f,
                                                                     1.0f));
static constexpr gfx::sRGBColor sScrollbarThumbColor(
    gfx::sRGBColor::UnusualFromARGB(0xffcdcdcd));

static const CSSCoord kMinimumColorPickerHeight = 32.0f;
static const CSSCoord kMinimumRangeThumbSize = 20.0f;
static const CSSCoord kMinimumDropdownArrowButtonWidth = 18.0f;
static const CSSCoord kMinimumSpinnerButtonWidth = 18.0f;
static const CSSCoord kMinimumSpinnerButtonHeight = 9.0f;
static const CSSCoord kButtonBorderWidth = 1.0f;
static const CSSCoord kMenulistBorderWidth = 1.0f;
static const CSSCoord kTextFieldBorderWidth = 1.0f;
static const CSSCoord kRangeHeight = 6.0f;
static const CSSCoord kProgressbarHeight = 6.0f;
static const CSSCoord kMeterHeight = 12.0f;

// nsCheckboxRadioFrame takes the bottom of the content box as the baseline.
// This border-width makes its baseline 2px under the bottom, which is nice.
static constexpr CSSCoord kCheckboxRadioBorderWidth = 2.0f;

}  // namespace widget
}  // namespace mozilla

class nsNativeBasicTheme : protected nsNativeTheme, public nsITheme {
 protected:
  using sRGBColor = mozilla::gfx::sRGBColor;
  using CSSCoord = mozilla::CSSCoord;
  using CSSPoint = mozilla::CSSPoint;
  using CSSIntCoord = mozilla::CSSIntCoord;
  using ComputedStyle = mozilla::ComputedStyle;
  using EventStates = mozilla::EventStates;
  using DrawTarget = mozilla::gfx::DrawTarget;
  using Path = mozilla::gfx::Path;
  using Rect = mozilla::gfx::Rect;
  using Point = mozilla::gfx::Point;
  using RectCornerRadii = mozilla::gfx::RectCornerRadii;
  using LayoutDeviceCoord = mozilla::LayoutDeviceCoord;
  using LayoutDeviceRect = mozilla::LayoutDeviceRect;
  class AccentColor;
  class Colors;

 public:
  static void Init();
  static void Shutdown();
  static void LookAndFeelChanged();

  using DPIRatio = mozilla::CSSToLayoutDeviceScale;

  NS_DECL_ISUPPORTS_INHERITED

  // The nsITheme interface.
  NS_IMETHOD DrawWidgetBackground(gfxContext* aContext, nsIFrame*,
                                  StyleAppearance, const nsRect& aRect,
                                  const nsRect& aDirtyRect,
                                  DrawOverflow) override;

  struct WebRenderBackendData {
    mozilla::wr::DisplayListBuilder& mBuilder;
    mozilla::wr::IpcResourceUpdateQueue& mResources;
    const mozilla::layers::StackingContextHelper& mSc;
    mozilla::layers::RenderRootStateManager* mManager;
  };

  bool CreateWebRenderCommandsForWidget(
      mozilla::wr::DisplayListBuilder& aBuilder,
      mozilla::wr::IpcResourceUpdateQueue& aResources,
      const mozilla::layers::StackingContextHelper& aSc,
      mozilla::layers::RenderRootStateManager* aManager, nsIFrame*,
      StyleAppearance, const nsRect& aRect) override;

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
  NS_IMETHOD GetMinimumWidgetSize(nsPresContext* aPresContext, nsIFrame*,
                                  StyleAppearance,
                                  mozilla::LayoutDeviceIntSize* aResult,
                                  bool* aIsOverridable) override;
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
  bool ThemeDrawsFocusForWidget(StyleAppearance) override;
  bool ThemeNeedsComboboxDropmarker() override;
  ScrollbarSizes GetScrollbarSizes(nsPresContext*, StyleScrollbarWidth,
                                   Overlay) override;
  static nscolor AdjustUnthemedScrollbarThumbColor(nscolor, EventStates);
  static nscolor GetScrollbarButtonColor(nscolor aTrackColor, EventStates);
  static mozilla::Maybe<nscolor> GetScrollbarArrowColor(nscolor aButtonColor);
  static nscolor ComputeCustomAccentForeground(nscolor aAccent);

  nscoord GetCheckboxRadioPrefSize() override;

 protected:
  nsNativeBasicTheme() = default;
  virtual ~nsNativeBasicTheme() = default;

  static DPIRatio GetDPIRatioForScrollbarPart(nsPresContext*);
  static DPIRatio GetDPIRatio(nsPresContext*, StyleAppearance);
  static DPIRatio GetDPIRatio(nsIFrame*, StyleAppearance);

  // Whether we should use system colors (for high contrast mode).
  static bool ShouldBeHighContrast(const nsPresContext&);

  std::pair<sRGBColor, sRGBColor> ComputeCheckboxColors(const EventStates&,
                                                        StyleAppearance,
                                                        const Colors&);
  sRGBColor ComputeCheckmarkColor(const EventStates&, const Colors&);
  enum class OutlineCoversBorder : bool { No, Yes };
  sRGBColor ComputeBorderColor(const EventStates&, const Colors&,
                               OutlineCoversBorder);

  std::pair<sRGBColor, sRGBColor> ComputeButtonColors(const EventStates&,
                                                      const Colors&,
                                                      nsIFrame* = nullptr);
  std::pair<sRGBColor, sRGBColor> ComputeTextfieldColors(const EventStates&,
                                                         const Colors&,
                                                         OutlineCoversBorder);
  std::pair<sRGBColor, sRGBColor> ComputeRangeProgressColors(const EventStates&,
                                                             const Colors&);
  std::pair<sRGBColor, sRGBColor> ComputeRangeTrackColors(const EventStates&,
                                                          const Colors&);
  std::pair<sRGBColor, sRGBColor> ComputeRangeThumbColors(const EventStates&,
                                                          const Colors&);
  std::pair<sRGBColor, sRGBColor> ComputeProgressColors(const Colors&);
  std::pair<sRGBColor, sRGBColor> ComputeProgressTrackColors(const Colors&);
  std::pair<sRGBColor, sRGBColor> ComputeMeterchunkColors(
      const EventStates& aMeterState, const Colors&);
  std::array<sRGBColor, 3> ComputeFocusRectColors(const Colors&);

  static bool ShouldUseDarkScrollbar(nsIFrame*, const ComputedStyle&);
  bool IsScrollbarTrackOpaque(nsIFrame*);
  sRGBColor ComputeScrollbarTrackColor(nsIFrame*, const ComputedStyle&,
                                       const EventStates& aDocumentState,
                                       const Colors&);
  sRGBColor ComputeScrollbarThumbColor(nsIFrame*, const ComputedStyle&,
                                       const EventStates& aElementState,
                                       const EventStates& aDocumentState,
                                       const Colors&);
  // Returned colors are button, arrow.
  std::pair<sRGBColor, sRGBColor> ComputeScrollbarButtonColors(
      nsIFrame*, StyleAppearance, const ComputedStyle&,
      const EventStates& aElementState, const EventStates& aDocumentState,
      const Colors&);

  template <typename PaintBackendData>
  void PaintRoundedFocusRect(PaintBackendData&, const LayoutDeviceRect&,
                             const Colors&, DPIRatio, CSSCoord aRadius,
                             CSSCoord aOffset);
  template <typename PaintBackendData>
  void PaintAutoStyleOutline(nsIFrame*, PaintBackendData&,
                             const LayoutDeviceRect&, const Colors&, DPIRatio);

  static void PaintRoundedRectWithRadius(DrawTarget&,
                                         const LayoutDeviceRect& aRect,
                                         const LayoutDeviceRect& aClipRect,
                                         const sRGBColor& aBackgroundColor,
                                         const sRGBColor& aBorderColor,
                                         CSSCoord aBorderWidth,
                                         CSSCoord aRadius, DPIRatio);
  static void PaintRoundedRectWithRadius(WebRenderBackendData&,
                                         const LayoutDeviceRect& aRect,
                                         const LayoutDeviceRect& aClipRect,
                                         const sRGBColor& aBackgroundColor,
                                         const sRGBColor& aBorderColor,
                                         CSSCoord aBorderWidth,
                                         CSSCoord aRadius, DPIRatio);
  template <typename PaintBackendData>
  static void PaintRoundedRectWithRadius(PaintBackendData& aData,
                                         const LayoutDeviceRect& aRect,
                                         const sRGBColor& aBackgroundColor,
                                         const sRGBColor& aBorderColor,
                                         CSSCoord aBorderWidth,
                                         CSSCoord aRadius, DPIRatio aDpiRatio) {
    PaintRoundedRectWithRadius(aData, aRect, aRect, aBackgroundColor,
                               aBorderColor, aBorderWidth, aRadius, aDpiRatio);
  }

  static void FillRect(DrawTarget&, const LayoutDeviceRect&, const sRGBColor&);
  static void FillRect(WebRenderBackendData&, const LayoutDeviceRect&,
                       const sRGBColor&);

  void PaintCheckboxControl(DrawTarget& aDrawTarget, const LayoutDeviceRect&,
                            const EventStates&, const Colors&, DPIRatio);
  void PaintCheckMark(DrawTarget&, const LayoutDeviceRect&, const EventStates&,
                      const Colors&);
  void PaintIndeterminateMark(DrawTarget&, const LayoutDeviceRect&,
                              const EventStates&, const Colors&);

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
                         const EventStates&, const Colors&, DPIRatio);
  template <typename PaintBackendData>
  void PaintRadioCheckmark(PaintBackendData&, const LayoutDeviceRect&,
                           const EventStates&, DPIRatio);
  template <typename PaintBackendData>
  void PaintTextField(PaintBackendData&, const LayoutDeviceRect&,
                      const EventStates&, const Colors&, DPIRatio);
  template <typename PaintBackendData>
  void PaintListbox(PaintBackendData&, const LayoutDeviceRect&,
                    const EventStates&, const Colors&, DPIRatio);
  template <typename PaintBackendData>
  void PaintMenulist(PaintBackendData&, const LayoutDeviceRect&,
                     const EventStates&, const Colors&, DPIRatio);
  void PaintArrow(DrawTarget&, const LayoutDeviceRect&,
                  const float aArrowPolygonX[], const float aArrowPolygonY[],
                  const float aArrowPolygonSize, const int32_t aArrowNumPoints,
                  const sRGBColor aFillColor);
  void PaintMenulistArrowButton(nsIFrame*, DrawTarget&, const LayoutDeviceRect&,
                                const EventStates&);
  void PaintSpinnerButton(nsIFrame*, DrawTarget&, const LayoutDeviceRect&,
                          const EventStates&, StyleAppearance, const Colors&,
                          DPIRatio);
  template <typename PaintBackendData>
  void PaintRange(nsIFrame*, PaintBackendData&, const LayoutDeviceRect&,
                  const EventStates&, const Colors&, DPIRatio,
                  bool aHorizontal);
  template <typename PaintBackendData>
  void PaintProgress(nsIFrame*, PaintBackendData&, const LayoutDeviceRect&,
                     const EventStates&, const Colors&, DPIRatio,
                     bool aIsMeter);
  template <typename PaintBackendData>
  void PaintButton(nsIFrame*, PaintBackendData&, const LayoutDeviceRect&,
                   const EventStates&, const Colors&, DPIRatio);

  void PaintScrollbarButton(DrawTarget&, StyleAppearance,
                            const LayoutDeviceRect&, nsIFrame*,
                            const ComputedStyle&,
                            const EventStates& aElementState,
                            const EventStates& aDocumentState, const Colors&,
                            DPIRatio);

  virtual bool PaintScrollbarThumb(DrawTarget&, const LayoutDeviceRect&,
                                   bool aHorizontal, nsIFrame*,
                                   const ComputedStyle&,
                                   const EventStates& aElementState,
                                   const EventStates& aDocumentState,
                                   const Colors&, DPIRatio);
  virtual bool PaintScrollbarThumb(WebRenderBackendData&,
                                   const LayoutDeviceRect&, bool aHorizontal,
                                   nsIFrame*, const ComputedStyle&,
                                   const EventStates& aElementState,
                                   const EventStates& aDocumentState,
                                   const Colors&, DPIRatio);
  template <typename PaintBackendData>
  bool DoPaintDefaultScrollbarThumb(PaintBackendData&, const LayoutDeviceRect&,
                                    bool aHorizontal, nsIFrame*,
                                    const ComputedStyle&,
                                    const EventStates& aElementState,
                                    const EventStates& aDocumentState,
                                    const Colors&, DPIRatio);

  virtual bool PaintScrollbar(DrawTarget&, const LayoutDeviceRect&,
                              bool aHorizontal, nsIFrame*, const ComputedStyle&,
                              const EventStates& aElementState,
                              const EventStates& aDocumentState, const Colors&,
                              DPIRatio);
  virtual bool PaintScrollbar(WebRenderBackendData&, const LayoutDeviceRect&,
                              bool aHorizontal, nsIFrame*, const ComputedStyle&,
                              const EventStates& aElementState,
                              const EventStates& aDocumentState, const Colors&,
                              DPIRatio);
  template <typename PaintBackendData>
  bool DoPaintDefaultScrollbar(PaintBackendData&, const LayoutDeviceRect&,
                               bool aHorizontal, nsIFrame*,
                               const ComputedStyle&,
                               const EventStates& aElementState,
                               const EventStates& aDocumentState, const Colors&,
                               DPIRatio);

  virtual bool PaintScrollbarTrack(DrawTarget&, const LayoutDeviceRect&,
                                   bool aHorizontal, nsIFrame*,
                                   const ComputedStyle&,
                                   const EventStates& aDocumentState,
                                   const Colors&, DPIRatio) {
    // Draw nothing by default. Subclasses can override this.
    return true;
  }
  virtual bool PaintScrollbarTrack(WebRenderBackendData&,
                                   const LayoutDeviceRect&, bool aHorizontal,
                                   nsIFrame*, const ComputedStyle&,
                                   const EventStates& aDocumentState,
                                   const Colors&, DPIRatio) {
    // Draw nothing by default. Subclasses can override this.
    return true;
  }

  virtual bool PaintScrollCorner(DrawTarget&, const LayoutDeviceRect&,
                                 nsIFrame*, const ComputedStyle&,
                                 const EventStates& aDocumentState,
                                 const Colors&, DPIRatio);
  virtual bool PaintScrollCorner(WebRenderBackendData&, const LayoutDeviceRect&,
                                 nsIFrame*, const ComputedStyle&,
                                 const EventStates& aDocumentState,
                                 const Colors&, DPIRatio);
  template <typename PaintBackendData>
  bool DoPaintDefaultScrollCorner(PaintBackendData&, const LayoutDeviceRect&,
                                  nsIFrame*, const ComputedStyle&,
                                  const EventStates& aDocumentState,
                                  const Colors&, DPIRatio);

  static CSSIntCoord sHorizontalScrollbarHeight;
  static CSSIntCoord sVerticalScrollbarWidth;

  static void PrefChangedCallback(const char*, void*) { LookAndFeelChanged(); }
  static void RecomputeAccentColors();
  static void RecomputeScrollbarParams();
};

#endif
