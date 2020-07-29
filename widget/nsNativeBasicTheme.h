/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNativeBasicTheme_h
#define nsNativeBasicTheme_h

#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/HTMLMeterElement.h"
#include "mozilla/dom/HTMLProgressElement.h"
#include "nsColorControlFrame.h"
#include "nsDateTimeControlFrame.h"
#include "nsDeviceContext.h"
#include "nsITheme.h"
#include "nsMeterFrame.h"
#include "nsNativeTheme.h"
#include "nsProgressFrame.h"
#include "nsRangeFrame.h"

using namespace mozilla;
using namespace mozilla::gfx;

namespace mozilla {
namespace widget {

static const sRGBColor sColorWhite(sRGBColor::OpaqueWhite());
static const sRGBColor sColorWhiteAlpha50(sRGBColor::White(0.5f));
static const sRGBColor sColorWhiteAlpha80(sRGBColor::White(0.8f));
static const sRGBColor sColorBlack(sRGBColor::OpaqueBlack());

static const sRGBColor sColorGrey10(sRGBColor::UnusualFromARGB(0xffe9e9ed));
static const sRGBColor sColorGrey10Alpha50(
    sRGBColor::UnusualFromARGB(0x7fe9e9ed));
static const sRGBColor sColorGrey20(sRGBColor::UnusualFromARGB(0xffd0d0d7));
static const sRGBColor sColorGrey30(sRGBColor::UnusualFromARGB(0xffb1b1b9));
static const sRGBColor sColorGrey40(sRGBColor::UnusualFromARGB(0xff8f8f9d));
static const sRGBColor sColorGrey40Alpha50(
    sRGBColor::UnusualFromARGB(0x7f8f8f9d));
static const sRGBColor sColorGrey50(sRGBColor::UnusualFromARGB(0xff676774));
static const sRGBColor sColorGrey50Alpha50(
    sRGBColor::UnusualFromARGB(0x7f676774));
static const sRGBColor sColorGrey60(sRGBColor::UnusualFromARGB(0xff484851));
static const sRGBColor sColorGrey60Alpha50(
    sRGBColor::UnusualFromARGB(0x7f484851));

static const sRGBColor sColorAccentLight(
    sRGBColor::UnusualFromARGB(0x4d008deb));
static const sRGBColor sColorAccent(sRGBColor::UnusualFromARGB(0xff0060df));
static const sRGBColor sColorAccentDark(sRGBColor::UnusualFromARGB(0xff0250bb));
static const sRGBColor sColorAccentDarker(
    sRGBColor::UnusualFromARGB(0xff054096));

static const sRGBColor sColorMeterGreen10(
    sRGBColor::UnusualFromARGB(0xff00ab60));
static const sRGBColor sColorMeterGreen20(
    sRGBColor::UnusualFromARGB(0xff056139));
static const sRGBColor sColorMeterYellow10(
    sRGBColor::UnusualFromARGB(0xffffbd4f));
static const sRGBColor sColorMeterYellow20(
    sRGBColor::UnusualFromARGB(0xffd2811e));
static const sRGBColor sColorMeterRed10(sRGBColor::UnusualFromARGB(0xffe22850));
static const sRGBColor sColorMeterRed20(sRGBColor::UnusualFromARGB(0xff810220));

static const sRGBColor sScrollbarColor(sRGBColor(0.94f, 0.94f, 0.94f));
static const sRGBColor sScrollbarBorderColor(sRGBColor(1.0f, 1.0f, 1.0f));
static const sRGBColor sScrollbarThumbColor(sRGBColor(0.8f, 0.8f, 0.8f));
static const sRGBColor sScrollbarThumbColorActive(sRGBColor(0.375f, 0.375f,
                                                            0.375f));
static const sRGBColor sScrollbarThumbColorHover(sRGBColor(0.65f, 0.65f,
                                                           0.65f));
static const sRGBColor sScrollbarArrowColor(sRGBColor(0.375f, 0.375f, 0.375f));
static const sRGBColor sScrollbarArrowColorActive(sRGBColor(1.0f, 1.0f, 1.0f));
static const sRGBColor sScrollbarArrowColorHover(sRGBColor(0.0f, 0.0f, 0.0f));
static const sRGBColor sScrollbarButtonColor(sScrollbarColor);
static const sRGBColor sScrollbarButtonActiveColor(sRGBColor(0.375f, 0.375f,
                                                             0.375f));
static const sRGBColor sScrollbarButtonHoverColor(sRGBColor(0.86f, 0.86f,
                                                            0.86f));

static const CSSIntCoord kMinimumWidgetSize = 14;
static const CSSIntCoord kMinimumColorPickerHeight = 32;
static const CSSIntCoord kMinimumRangeThumbSize = 20;
static const CSSIntCoord kMinimumDropdownArrowButtonWidth = 18;
static const CSSIntCoord kMinimumSpinnerButtonWidth = 31;
static const CSSCoord kButtonBorderWidth = 1.0f;
static const CSSCoord kMenulistBorderWidth = 1.0f;
static const CSSCoord kTextFieldBorderWidth = 1.0f;
static const CSSCoord kSpinnerBorderWidth = 1.0f;
static const CSSCoord kRangeHeight = 6.0f;
static const CSSCoord kProgressbarHeight = 6.0f;
static const CSSCoord kMeterHeight = 12.0f;

}  // namespace widget
}  // namespace mozilla

class nsNativeBasicTheme : private nsNativeTheme, public nsITheme {
 public:
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

  static uint32_t GetDPIRatio(nsIFrame* aFrame);
  static bool IsDateTimeResetButton(nsIFrame* aFrame);
  static bool IsDateTimeTextField(nsIFrame* aFrame);
  static bool IsColorPickerButton(nsIFrame* aFrame);
  static std::pair<sRGBColor, sRGBColor> ComputeCheckColors(
      const EventStates& aState);
  static gfx::Rect FixAspectRatio(const gfx::Rect& aRect);

  // This pushes and pops a clip rect to the draw target.
  //
  // This is done to reduce fuzz in places where we may have antialiasing,
  // because skia is not clip-invariant: given different clips, it does not
  // guarantee the same result, even if the painted content doesn't intersect
  // the clips.
  //
  // This is a bit sad, overall, but...
  struct MOZ_RAII AutoClipRect {
    AutoClipRect(DrawTarget& aDt, const gfx::Rect& aRect) : mDt(aDt) {
      mDt.PushClipRect(aRect);
    }

    ~AutoClipRect() { mDt.PopClip(); }

   private:
    DrawTarget& mDt;
  };

  static void GetFocusStrokeRect(DrawTarget* aDrawTarget, gfx::Rect& aFocusRect,
                                 CSSCoord aOffset, const CSSCoord aRadius,
                                 CSSCoord aFocusWidth, RefPtr<Path>& aOutRect);
  static void PaintRoundedFocusRect(DrawTarget* aDrawTarget,
                                    const gfx::Rect& aRect, uint32_t aDpiRatio,
                                    CSSCoord aRadius, CSSCoord aOffset);
  static void PaintRoundedRect(DrawTarget* aDrawTarget, const gfx::Rect& aRect,
                               const sRGBColor& aBackgroundColor,
                               const sRGBColor& aBorderColor,
                               CSSCoord aBorderWidth,
                               RectCornerRadii aDpiAdjustedRadii,
                               uint32_t aDpiRatio);
  static void PaintRoundedRectWithRadius(DrawTarget* aDrawTarget,
                                         const gfx::Rect& aRect,
                                         const sRGBColor& aBackgroundColor,
                                         const sRGBColor& aBorderColor,
                                         CSSCoord aBorderWidth,
                                         CSSCoord aRadius, uint32_t aDpiRatio);
  static void PaintCheckboxControl(DrawTarget* aDrawTarget,
                                   const gfx::Rect& aRect,
                                   const EventStates& aState,
                                   uint32_t aDpiRatio);
  static void PaintCheckMark(DrawTarget* aDrawTarget, const gfx::Rect& aRect,
                             const EventStates& aState, uint32_t aDpiRatio);
  static void PaintIndeterminateMark(DrawTarget* aDrawTarget,
                                     const gfx::Rect& aRect,
                                     const EventStates& aState,
                                     uint32_t aDpiRatio);
  static void PaintStrokedEllipse(DrawTarget* aDrawTarget,
                                  const gfx::Rect& aRect,
                                  const sRGBColor& aBackgroundColor,
                                  const sRGBColor& aBorderColor,
                                  const CSSCoord aBorderWidth,
                                  uint32_t aDpiRatio);
  static void PaintRadioControl(DrawTarget* aDrawTarget, const gfx::Rect& aRect,
                                const EventStates& aState, uint32_t aDpiRatio);
  static void PaintRadioCheckMark(DrawTarget* aDrawTarget,
                                  const gfx::Rect& aRect,
                                  const EventStates& aState,
                                  uint32_t aDpiRatio);
  static sRGBColor ComputeBorderColor(const EventStates& aState);
  static void PaintTextField(DrawTarget* aDrawTarget, const gfx::Rect& aRect,
                             const EventStates& aState, uint32_t aDpiRatio);
  static std::pair<sRGBColor, sRGBColor> ComputeButtonColors(
      const EventStates& aState, nsIFrame* aFrame = nullptr);
  static sRGBColor ComputeRangeProgressBorderColor(const EventStates& aState);
  static sRGBColor ComputeRangeTrackBorderColor(const EventStates& aState);
  static std::pair<sRGBColor, sRGBColor> ComputeRangeProgressColors(
      const EventStates& aState);
  static std::pair<sRGBColor, sRGBColor> ComputeRangeTrackColors(
      const EventStates& aState);
  static std::pair<sRGBColor, sRGBColor> ComputeRangeThumbColors(
      const EventStates& aState);
  static void PaintListbox(DrawTarget* aDrawTarget, const gfx::Rect& aRect,
                           const EventStates& aState, uint32_t aDpiRatio);
  static void PaintMenulist(DrawTarget* aDrawTarget, const gfx::Rect& aRect,
                            const EventStates& aState, uint32_t aDpiRatio);
  static void PaintArrow(DrawTarget* aDrawTarget, const gfx::Rect& aRect,
                         const int32_t aArrowPolygonX[],
                         const int32_t aArrowPolygonY[],
                         const int32_t aArrowNumPoints,
                         const int32_t aArrowSize, const sRGBColor aFillColor,
                         uint32_t aDpiRatio);
  static void PaintMenulistArrowButton(nsIFrame* aFrame,
                                       DrawTarget* aDrawTarget,
                                       const gfx::Rect& aRect,
                                       const EventStates& aState,
                                       uint32_t aDpiRatio);
  static void PaintSpinnerButton(nsIFrame* aFrame, DrawTarget* aDrawTarget,
                                 const gfx::Rect& aRect,
                                 const EventStates& aState,
                                 StyleAppearance aAppearance,
                                 uint32_t aDpiRatio);
  static void PaintRangeTrackBackground(nsIFrame* aFrame,
                                        DrawTarget* aDrawTarget,
                                        const gfx::Rect& aRect,
                                        const EventStates& aState,
                                        uint32_t aDpiRatio, bool aHorizontal);
  static void PaintRangeThumb(DrawTarget* aDrawTarget, const gfx::Rect& aRect,
                              const EventStates& aState, uint32_t aDpiRatio);
  static void PaintProgressBar(DrawTarget* aDrawTarget, const gfx::Rect& aRect,
                               const EventStates& aState, uint32_t aDpiRatio);
  static void PaintProgresschunk(nsIFrame* aFrame, DrawTarget* aDrawTarget,
                                 const gfx::Rect& aRect,
                                 const EventStates& aState, uint32_t aDpiRatio);
  static void PaintMeter(DrawTarget* aDrawTarget, const gfx::Rect& aRect,
                         const EventStates& aState, uint32_t aDpiRatio);
  static void PaintMeterchunk(nsIFrame* aFrame, DrawTarget* aDrawTarget,
                              const gfx::Rect& aRect, const EventStates& aState,
                              uint32_t aDpiRatio);
  static void PaintButton(nsIFrame* aFrame, DrawTarget* aDrawTarget,
                          const gfx::Rect& aRect, const EventStates& aState,
                          uint32_t aDpiRatio);

  virtual void PaintScrollbarthumbHorizontal(DrawTarget* aDrawTarget,
                                             const gfx::Rect& aRect,
                                             const EventStates& aState);
  virtual void PaintScrollbarthumbVertical(DrawTarget* aDrawTarget,
                                           const gfx::Rect& aRect,
                                           const EventStates& aState);
  virtual void PaintScrollbarHorizontal(DrawTarget* aDrawTarget,
                                        const gfx::Rect& aRect);
  virtual void PaintScrollbarVerticalAndCorner(DrawTarget* aDrawTarget,
                                               const gfx::Rect& aRect,
                                               uint32_t aDpiRatio);
  virtual void PaintScrollbarbutton(DrawTarget* aDrawTarget,
                                    StyleAppearance aAppearance,
                                    const gfx::Rect& aRect,
                                    const EventStates& aState,
                                    uint32_t aDpiRatio);
};

#endif
