/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNativeBasicTheme_h
#define nsNativeBasicTheme_h

#include "Units.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/HTMLMeterElement.h"
#include "mozilla/dom/HTMLProgressElement.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/Types.h"
#include "nsColorControlFrame.h"
#include "nsDateTimeControlFrame.h"
#include "nsDeviceContext.h"
#include "nsITheme.h"
#include "nsMeterFrame.h"
#include "nsNativeTheme.h"
#include "nsProgressFrame.h"
#include "nsRangeFrame.h"

namespace mozilla {
namespace widget {

static const gfx::sRGBColor sColorWhite(gfx::sRGBColor::OpaqueWhite());
static const gfx::sRGBColor sColorWhiteAlpha50(gfx::sRGBColor::White(0.5f));
static const gfx::sRGBColor sColorWhiteAlpha80(gfx::sRGBColor::White(0.8f));
static const gfx::sRGBColor sColorBlack(gfx::sRGBColor::OpaqueBlack());

static const gfx::sRGBColor sColorGrey10(
    gfx::sRGBColor::UnusualFromARGB(0xffe9e9ed));
static const gfx::sRGBColor sColorGrey10Alpha50(
    gfx::sRGBColor::UnusualFromARGB(0x7fe9e9ed));
static const gfx::sRGBColor sColorGrey20(
    gfx::sRGBColor::UnusualFromARGB(0xffd0d0d7));
static const gfx::sRGBColor sColorGrey30(
    gfx::sRGBColor::UnusualFromARGB(0xffb1b1b9));
static const gfx::sRGBColor sColorGrey40(
    gfx::sRGBColor::UnusualFromARGB(0xff8f8f9d));
static const gfx::sRGBColor sColorGrey40Alpha50(
    gfx::sRGBColor::UnusualFromARGB(0x7f8f8f9d));
static const gfx::sRGBColor sColorGrey50(
    gfx::sRGBColor::UnusualFromARGB(0xff676774));
static const gfx::sRGBColor sColorGrey50Alpha50(
    gfx::sRGBColor::UnusualFromARGB(0x7f676774));
static const gfx::sRGBColor sColorGrey60(
    gfx::sRGBColor::UnusualFromARGB(0xff484851));
static const gfx::sRGBColor sColorGrey60Alpha50(
    gfx::sRGBColor::UnusualFromARGB(0x7f484851));

static const gfx::sRGBColor sColorAccentLight(
    gfx::sRGBColor::UnusualFromARGB(0x4d008deb));
static const gfx::sRGBColor sColorAccent(
    gfx::sRGBColor::UnusualFromARGB(0xff0060df));
static const gfx::sRGBColor sColorAccentDark(
    gfx::sRGBColor::UnusualFromARGB(0xff0250bb));
static const gfx::sRGBColor sColorAccentDarker(
    gfx::sRGBColor::UnusualFromARGB(0xff054096));

static const gfx::sRGBColor sColorMeterGreen10(
    gfx::sRGBColor::UnusualFromARGB(0xff00ab60));
static const gfx::sRGBColor sColorMeterGreen20(
    gfx::sRGBColor::UnusualFromARGB(0xff056139));
static const gfx::sRGBColor sColorMeterYellow10(
    gfx::sRGBColor::UnusualFromARGB(0xffffbd4f));
static const gfx::sRGBColor sColorMeterYellow20(
    gfx::sRGBColor::UnusualFromARGB(0xffd2811e));
static const gfx::sRGBColor sColorMeterRed10(
    gfx::sRGBColor::UnusualFromARGB(0xffe22850));
static const gfx::sRGBColor sColorMeterRed20(
    gfx::sRGBColor::UnusualFromARGB(0xff810220));

static const gfx::sRGBColor sScrollbarColor(gfx::sRGBColor(0.94f, 0.94f,
                                                           0.94f));
static const gfx::sRGBColor sScrollbarBorderColor(gfx::sRGBColor(1.0f, 1.0f,
                                                                 1.0f));
static const gfx::sRGBColor sScrollbarThumbColor(gfx::sRGBColor(0.8f, 0.8f,
                                                                0.8f));
static const gfx::sRGBColor sScrollbarThumbColorActive(gfx::sRGBColor(0.375f,
                                                                      0.375f,
                                                                      0.375f));
static const gfx::sRGBColor sScrollbarThumbColorHover(gfx::sRGBColor(0.65f,
                                                                     0.65f,
                                                                     0.65f));
static const gfx::sRGBColor sScrollbarArrowColor(gfx::sRGBColor(0.375f, 0.375f,
                                                                0.375f));
static const gfx::sRGBColor sScrollbarArrowColorActive(gfx::sRGBColor(1.0f,
                                                                      1.0f,
                                                                      1.0f));
static const gfx::sRGBColor sScrollbarArrowColorHover(gfx::sRGBColor(0.0f, 0.0f,
                                                                     0.0f));
static const gfx::sRGBColor sScrollbarButtonColor(sScrollbarColor);
static const gfx::sRGBColor sScrollbarButtonActiveColor(gfx::sRGBColor(0.375f,
                                                                       0.375f,
                                                                       0.375f));
static const gfx::sRGBColor sScrollbarButtonHoverColor(gfx::sRGBColor(0.86f,
                                                                      0.86f,
                                                                      0.86f));

static const CSSCoord kMinimumWidgetSize = 14.0f;
static const CSSCoord kMinimumScrollbarSize = 14.0f;
static const CSSCoord kMinimumThinScrollbarSize = 6.0f;
static const CSSCoord kMinimumColorPickerHeight = 32.0f;
static const CSSCoord kMinimumRangeThumbSize = 20.0f;
static const CSSCoord kMinimumDropdownArrowButtonWidth = 18.0f;
static const CSSCoord kMinimumSpinnerButtonWidth = 31.0f;
static const CSSCoord kButtonBorderWidth = 1.0f;
static const CSSCoord kMenulistBorderWidth = 1.0f;
static const CSSCoord kTextFieldBorderWidth = 1.0f;
static const CSSCoord kSpinnerBorderWidth = 1.0f;
static const CSSCoord kRangeHeight = 6.0f;
static const CSSCoord kProgressbarHeight = 6.0f;
static const CSSCoord kMeterHeight = 12.0f;

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

 public:
  using DPIRatio = mozilla::CSSToLayoutDeviceScale;

  NS_DECL_ISUPPORTS_INHERITED

  // The nsITheme interface.
  NS_IMETHOD DrawWidgetBackground(gfxContext* aContext, nsIFrame* aFrame,
                                  StyleAppearance aAppearance,
                                  const nsRect& aRect,
                                  const nsRect& aDirtyRect) override;
  /*bool CreateWebRenderCommandsForWidget(mozilla::wr::DisplayListBuilder&
     aBuilder, mozilla::wr::IpcResourceUpdateQueue& aResources, const
     mozilla::layers::StackingContextHelper& aSc,
                                        mozilla::layers::RenderRootStateManager*
     aManager, nsIFrame* aFrame, StyleAppearance aAppearance, const nsRect&
     aRect) override;*/
  [[nodiscard]] LayoutDeviceIntMargin GetWidgetBorder(
      nsDeviceContext* aContext, nsIFrame* aFrame,
      StyleAppearance aAppearance) override;
  bool GetWidgetPadding(nsDeviceContext* aContext, nsIFrame* aFrame,
                        StyleAppearance aAppearance,
                        LayoutDeviceIntMargin* aResult) override;
  bool GetWidgetOverflow(nsDeviceContext* aContext, nsIFrame* aFrame,
                         StyleAppearance aAppearance,
                         nsRect* aOverflowRect) override;
  NS_IMETHOD GetMinimumWidgetSize(nsPresContext* aPresContext, nsIFrame* aFrame,
                                  StyleAppearance aAppearance,
                                  mozilla::LayoutDeviceIntSize* aResult,
                                  bool* aIsOverridable) override;
  Transparency GetWidgetTransparency(nsIFrame* aFrame,
                                     StyleAppearance aAppearance) override;
  NS_IMETHOD WidgetStateChanged(nsIFrame* aFrame, StyleAppearance aAppearance,
                                nsAtom* aAttribute, bool* aShouldRepaint,
                                const nsAttrValue* aOldValue) override;
  NS_IMETHOD ThemeChanged() override;
  bool WidgetAppearanceDependsOnWindowFocus(
      StyleAppearance aAppearance) override;
  /*bool NeedToClearBackgroundBehindWidget(
      nsIFrame* aFrame, StyleAppearance aAppearance) override;*/
  ThemeGeometryType ThemeGeometryTypeForWidget(
      nsIFrame* aFrame, StyleAppearance aAppearance) override;
  bool ThemeSupportsWidget(nsPresContext* aPresContext, nsIFrame* aFrame,
                           StyleAppearance aAppearance) override;
  bool WidgetIsContainer(StyleAppearance aAppearance) override;
  bool ThemeDrawsFocusForWidget(StyleAppearance aAppearance) override;
  bool ThemeNeedsComboboxDropmarker() override;

 protected:
  nsNativeBasicTheme() = default;
  virtual ~nsNativeBasicTheme() = default;

  static DPIRatio GetDPIRatio(nsIFrame* aFrame);
  static bool IsDateTimeResetButton(nsIFrame* aFrame);
  static bool IsDateTimeTextField(nsIFrame* aFrame);
  static bool IsColorPickerButton(nsIFrame* aFrame);
  static bool IsRootScrollbar(nsIFrame* aFrame);
  static LayoutDeviceRect FixAspectRatio(const LayoutDeviceRect& aRect);

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

  static void GetFocusStrokeRect(DrawTarget* aDrawTarget,
                                 LayoutDeviceRect& aFocusRect,
                                 LayoutDeviceCoord aOffset,
                                 const LayoutDeviceCoord aRadius,
                                 LayoutDeviceCoord aFocusWidth,
                                 RefPtr<Path>& aOutRect);

  virtual std::pair<sRGBColor, sRGBColor> ComputeCheckboxColors(
      const EventStates& aState, StyleAppearance aAppearance);
  virtual sRGBColor ComputeCheckmarkColor(const EventStates& aState);
  virtual std::pair<sRGBColor, sRGBColor> ComputeRadioCheckmarkColors(
      const EventStates& aState);
  virtual sRGBColor ComputeBorderColor(const EventStates& aState);

  virtual std::pair<sRGBColor, sRGBColor> ComputeButtonColors(
      const EventStates& aState, nsIFrame* aFrame = nullptr);
  virtual std::pair<sRGBColor, sRGBColor> ComputeTextfieldColors(
      const EventStates& aState);
  virtual std::pair<sRGBColor, sRGBColor> ComputeRangeProgressColors(
      const EventStates& aState);
  virtual std::pair<sRGBColor, sRGBColor> ComputeRangeTrackColors(
      const EventStates& aState);
  virtual std::pair<sRGBColor, sRGBColor> ComputeRangeThumbColors(
      const EventStates& aState);
  virtual std::pair<sRGBColor, sRGBColor> ComputeProgressColors();
  virtual std::pair<sRGBColor, sRGBColor> ComputeProgressTrackColors();
  virtual std::pair<sRGBColor, sRGBColor> ComputeMeterchunkColors(
      const EventStates& aMeterState);
  virtual std::pair<sRGBColor, sRGBColor> ComputeMeterTrackColors();
  virtual sRGBColor ComputeMenulistArrowButtonColor(const EventStates& aState);
  virtual std::array<sRGBColor, 3> ComputeFocusRectColors();
  virtual sRGBColor ComputeScrollbarthumbColor(
      const ComputedStyle& aStyle, const EventStates& aElementState,
      const EventStates& aDocumentState);
  virtual sRGBColor ComputeScrollbarColor(const ComputedStyle& aStyle,
                                          const EventStates& aDocumentState,
                                          bool aIsRoot);

  void PaintRoundedFocusRect(DrawTarget* aDrawTarget,
                             const LayoutDeviceRect& aRect, DPIRatio aDpiRatio,
                             CSSCoord aRadius, CSSCoord aOffset);
  void PaintRoundedRect(DrawTarget* aDrawTarget, const LayoutDeviceRect& aRect,
                        const sRGBColor& aBackgroundColor,
                        const sRGBColor& aBorderColor, CSSCoord aBorderWidth,
                        RectCornerRadii aDpiAdjustedRadii, DPIRatio aDpiRatio);
  void PaintRoundedRectWithRadius(DrawTarget* aDrawTarget,
                                  const LayoutDeviceRect& aRect,
                                  const sRGBColor& aBackgroundColor,
                                  const sRGBColor& aBorderColor,
                                  CSSCoord aBorderWidth, CSSCoord aRadius,
                                  DPIRatio aDpiRatio);
  void PaintCheckboxControl(DrawTarget* aDrawTarget,
                            const LayoutDeviceRect& aRect,
                            const EventStates& aState, DPIRatio aDpiRatio);
  void PaintCheckMark(DrawTarget* aDrawTarget, const LayoutDeviceRect& aRect,
                      const EventStates& aState, DPIRatio aDpiRatio);
  void PaintIndeterminateMark(DrawTarget* aDrawTarget,
                              const LayoutDeviceRect& aRect,
                              const EventStates& aState, DPIRatio aDpiRatio);
  void PaintStrokedEllipse(DrawTarget* aDrawTarget,
                           const LayoutDeviceRect& aRect,
                           const sRGBColor& aBackgroundColor,
                           const sRGBColor& aBorderColor,
                           const CSSCoord aBorderWidth, DPIRatio aDpiRatio);
  void PaintEllipseShadow(DrawTarget* aDrawTarget,
                          const LayoutDeviceRect& aRect, float aShadowAlpha,
                          const CSSPoint& aShadowOffset,
                          CSSCoord aShadowBlurStdDev, DPIRatio aDpiRatio);
  void PaintRadioControl(DrawTarget* aDrawTarget, const LayoutDeviceRect& aRect,
                         const EventStates& aState, DPIRatio aDpiRatio);
  void PaintRadioCheckmark(DrawTarget* aDrawTarget,
                           const LayoutDeviceRect& aRect,
                           const EventStates& aState, DPIRatio aDpiRatio);
  void PaintTextField(DrawTarget* aDrawTarget, const LayoutDeviceRect& aRect,
                      const EventStates& aState, DPIRatio aDpiRatio);
  void PaintListbox(DrawTarget* aDrawTarget, const LayoutDeviceRect& aRect,
                    const EventStates& aState, DPIRatio aDpiRatio);
  void PaintMenulist(DrawTarget* aDrawTarget, const LayoutDeviceRect& aRect,
                     const EventStates& aState, DPIRatio aDpiRatio);
  void PaintArrow(DrawTarget* aDrawTarget, const LayoutDeviceRect& aRect,
                  const int32_t aArrowPolygonX[],
                  const int32_t aArrowPolygonY[], const int32_t aArrowNumPoints,
                  const int32_t aArrowSize, const sRGBColor aFillColor,
                  DPIRatio aDpiRatio);
  void PaintMenulistArrowButton(nsIFrame* aFrame, DrawTarget* aDrawTarget,
                                const LayoutDeviceRect& aRect,
                                const EventStates& aState, DPIRatio aDpiRatio);
  void PaintSpinnerButton(nsIFrame* aFrame, DrawTarget* aDrawTarget,
                          const LayoutDeviceRect& aRect,
                          const EventStates& aState,
                          StyleAppearance aAppearance, DPIRatio aDpiRatio);
  void PaintRange(nsIFrame* aFrame, DrawTarget* aDrawTarget,
                  const LayoutDeviceRect& aRect, const EventStates& aState,
                  DPIRatio aDpiRatio, bool aHorizontal);
  void PaintProgressBar(DrawTarget* aDrawTarget, const LayoutDeviceRect& aRect,
                        const EventStates& aState, DPIRatio aDpiRatio);
  void PaintProgresschunk(nsIFrame* aFrame, DrawTarget* aDrawTarget,
                          const LayoutDeviceRect& aRect,
                          const EventStates& aState, DPIRatio aDpiRatio);
  void PaintMeter(DrawTarget* aDrawTarget, const LayoutDeviceRect& aRect,
                  const EventStates& aState, DPIRatio aDpiRatio);
  void PaintMeterchunk(nsIFrame* aFrame, DrawTarget* aDrawTarget,
                       const LayoutDeviceRect& aRect, DPIRatio aDpiRatio);
  void PaintButton(nsIFrame* aFrame, DrawTarget* aDrawTarget,
                   const LayoutDeviceRect& aRect, const EventStates& aState,
                   DPIRatio aDpiRatio);

  virtual void PaintScrollbarThumb(DrawTarget* aDrawTarget,
                                   const LayoutDeviceRect& aRect,
                                   bool aHorizontal, nsIFrame* aFrame,
                                   const ComputedStyle& aStyle,
                                   const EventStates& aElementState,
                                   const EventStates& aDocumentState,
                                   DPIRatio aDpiRatio);
  virtual void PaintScrollbar(DrawTarget* aDrawTarget,
                              const LayoutDeviceRect& aRect, bool aHorizontal,
                              nsIFrame* aFrame, const ComputedStyle& aStyle,
                              const EventStates& aDocumentState,
                              DPIRatio aDpiRatio, bool aIsRoot);
  virtual void PaintScrollbarTrack(DrawTarget* aDrawTarget,
                                   const LayoutDeviceRect& aRect,
                                   bool aHorizontal, nsIFrame* aFrame,
                                   const ComputedStyle& aStyle,
                                   const EventStates& aDocumentState,
                                   DPIRatio aDpiRatio, bool aIsRoot);
  virtual void PaintScrollCorner(DrawTarget* aDrawTarget,
                                 const LayoutDeviceRect& aRect,
                                 nsIFrame* aFrame, const ComputedStyle& aStyle,
                                 const EventStates& aDocumentState,
                                 DPIRatio aDpiRatio, bool aIsRoot);
  virtual void PaintScrollbarbutton(DrawTarget* aDrawTarget,
                                    StyleAppearance aAppearance,
                                    const LayoutDeviceRect& aRect,
                                    const ComputedStyle& aStyle,
                                    const EventStates& aElementState,
                                    const EventStates& aDocumentState,
                                    DPIRatio aDpiRatio);
};

#endif
