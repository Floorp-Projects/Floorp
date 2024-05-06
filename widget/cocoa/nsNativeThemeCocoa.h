/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNativeThemeCocoa_h_
#define nsNativeThemeCocoa_h_

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#include "mozilla/Variant.h"

#include "nsITheme.h"
#include "ThemeCocoa.h"
#include "mozilla/dom/RustTypes.h"

@class MOZCellDrawWindow;
@class MOZCellDrawView;
@class MOZSearchFieldCell;
@class NSProgressBarCell;
class nsDeviceContext;
struct SegmentedControlRenderSettings;

namespace mozilla {
namespace gfx {
class DrawTarget;
}  // namespace gfx
}  // namespace mozilla

class nsNativeThemeCocoa : public mozilla::widget::ThemeCocoa {
  using ThemeCocoa = mozilla::widget::ThemeCocoa;

 public:
  enum class CheckboxOrRadioState : uint8_t { eOff, eOn, eIndeterminate };

  enum class ButtonType : uint8_t {
    eRegularPushButton,
    eDefaultPushButton,
    eSquareBezelPushButton,
    eArrowButton,
    eHelpButton,
    eDisclosureButtonClosed,
    eDisclosureButtonOpen
  };

  enum class SpinButton : uint8_t { eUp, eDown };

  enum class SegmentType : uint8_t { eToolbarButton, eTab };

  enum class OptimumState : uint8_t { eOptimum, eSubOptimum, eSubSubOptimum };

  struct ControlParams {
    ControlParams()
        : disabled(false),
          insideActiveWindow(false),
          pressed(false),
          focused(false),
          rtl(false) {}

    bool disabled : 1;
    bool insideActiveWindow : 1;
    bool pressed : 1;
    bool focused : 1;
    bool rtl : 1;
  };

  struct CheckboxOrRadioParams {
    ControlParams controlParams;
    CheckboxOrRadioState state = CheckboxOrRadioState::eOff;
    float verticalAlignFactor = 0.5f;
  };

  struct ButtonParams {
    ControlParams controlParams;
    ButtonType button = ButtonType::eRegularPushButton;
  };

  struct DropdownParams {
    ControlParams controlParams;
    bool pullsDown = false;
    bool editable = false;
  };

  struct SpinButtonParams {
    mozilla::Maybe<SpinButton> pressedButton;
    bool disabled = false;
    bool insideActiveWindow = false;
  };

  struct SegmentParams {
    SegmentType segmentType = SegmentType::eToolbarButton;
    bool insideActiveWindow = false;
    bool pressed = false;
    bool selected = false;
    bool focused = false;
    bool atLeftEnd = false;
    bool atRightEnd = false;
    bool drawsLeftSeparator = false;
    bool drawsRightSeparator = false;
    bool rtl = false;
  };

  struct TextFieldParams {
    float verticalAlignFactor = 0.5f;
    bool insideToolbar = false;
    bool disabled = false;
    bool focused = false;
    bool rtl = false;
  };

  struct ProgressParams {
    double value = 0.0;
    double max = 0.0;
    float verticalAlignFactor = 0.5f;
    bool insideActiveWindow = false;
    bool indeterminate = false;
    bool horizontal = false;
    bool rtl = false;
  };

  struct MeterParams {
    double value = 0;
    double min = 0;
    double max = 0;
    OptimumState optimumState = OptimumState::eOptimum;
    float verticalAlignFactor = 0.5f;
    bool horizontal = true;
    bool rtl = false;
  };

  struct ScaleParams {
    int32_t value = 0;
    int32_t min = 0;
    int32_t max = 0;
    bool insideActiveWindow = false;
    bool disabled = false;
    bool focused = false;
    bool horizontal = true;
    bool reverse = false;
  };

  enum Widget : uint8_t {
    eColorFill,       // mozilla::gfx::sRGBColor
    eCheckbox,        // CheckboxOrRadioParams
    eRadio,           // CheckboxOrRadioParams
    eButton,          // ButtonParams
    eDropdown,        // DropdownParams
    eSpinButtons,     // SpinButtonParams
    eSpinButtonUp,    // SpinButtonParams
    eSpinButtonDown,  // SpinButtonParams
    eSegment,         // SegmentParams
    eSeparator,
    eStatusBar,  // bool
    eGroupBox,
    eTextField,           // TextFieldParams
    eSearchField,         // TextFieldParams
    eProgressBar,         // ProgressParams
    eMeter,               // MeterParams
    eScale,               // ScaleParams
    eMultilineTextField,  // bool
    eListBox,
    eTabPanel,
  };

  struct WidgetInfo {
    static WidgetInfo ColorFill(const mozilla::gfx::sRGBColor& aParams) {
      return WidgetInfo(Widget::eColorFill, aParams);
    }
    static WidgetInfo Checkbox(const CheckboxOrRadioParams& aParams) {
      return WidgetInfo(Widget::eCheckbox, aParams);
    }
    static WidgetInfo Radio(const CheckboxOrRadioParams& aParams) {
      return WidgetInfo(Widget::eRadio, aParams);
    }
    static WidgetInfo Button(const ButtonParams& aParams) {
      return WidgetInfo(Widget::eButton, aParams);
    }
    static WidgetInfo Dropdown(const DropdownParams& aParams) {
      return WidgetInfo(Widget::eDropdown, aParams);
    }
    static WidgetInfo SpinButtons(const SpinButtonParams& aParams) {
      return WidgetInfo(Widget::eSpinButtons, aParams);
    }
    static WidgetInfo SpinButtonUp(const SpinButtonParams& aParams) {
      return WidgetInfo(Widget::eSpinButtonUp, aParams);
    }
    static WidgetInfo SpinButtonDown(const SpinButtonParams& aParams) {
      return WidgetInfo(Widget::eSpinButtonDown, aParams);
    }
    static WidgetInfo Segment(const SegmentParams& aParams) {
      return WidgetInfo(Widget::eSegment, aParams);
    }
    static WidgetInfo Separator() {
      return WidgetInfo(Widget::eSeparator, false);
    }
    static WidgetInfo StatusBar(bool aParams) {
      return WidgetInfo(Widget::eStatusBar, aParams);
    }
    static WidgetInfo GroupBox() {
      return WidgetInfo(Widget::eGroupBox, false);
    }
    static WidgetInfo TextField(const TextFieldParams& aParams) {
      return WidgetInfo(Widget::eTextField, aParams);
    }
    static WidgetInfo SearchField(const TextFieldParams& aParams) {
      return WidgetInfo(Widget::eSearchField, aParams);
    }
    static WidgetInfo ProgressBar(const ProgressParams& aParams) {
      return WidgetInfo(Widget::eProgressBar, aParams);
    }
    static WidgetInfo Meter(const MeterParams& aParams) {
      return WidgetInfo(Widget::eMeter, aParams);
    }
    static WidgetInfo Scale(const ScaleParams& aParams) {
      return WidgetInfo(Widget::eScale, aParams);
    }
    static WidgetInfo MultilineTextField(bool aParams) {
      return WidgetInfo(Widget::eMultilineTextField, aParams);
    }
    static WidgetInfo ListBox() { return WidgetInfo(Widget::eListBox, false); }
    static WidgetInfo TabPanel(bool aParams) {
      return WidgetInfo(Widget::eTabPanel, aParams);
    }

    template <typename T>
    T Params() const {
      MOZ_RELEASE_ASSERT(mVariant.is<T>());
      return mVariant.as<T>();
    }

    enum Widget Widget() const { return mWidget; }

   private:
    template <typename T>
    WidgetInfo(enum Widget aWidget, const T& aParams)
        : mVariant(aParams), mWidget(aWidget) {}

    mozilla::Variant<mozilla::gfx::sRGBColor, CheckboxOrRadioParams,
                     ButtonParams, DropdownParams, SpinButtonParams,
                     SegmentParams, TextFieldParams, ProgressParams,
                     MeterParams, ScaleParams, bool>
        mVariant;

    enum Widget mWidget;
  };

  explicit nsNativeThemeCocoa();

  NS_DECL_ISUPPORTS_INHERITED

  // The nsITheme interface.
  NS_IMETHOD DrawWidgetBackground(gfxContext* aContext, nsIFrame*,
                                  StyleAppearance, const nsRect& aRect,
                                  const nsRect& aDirtyRect,
                                  DrawOverflow) override;
  bool CreateWebRenderCommandsForWidget(
      mozilla::wr::DisplayListBuilder& aBuilder,
      mozilla::wr::IpcResourceUpdateQueue& aResources,
      const mozilla::layers::StackingContextHelper& aSc,
      mozilla::layers::RenderRootStateManager* aManager, nsIFrame*,
      StyleAppearance, const nsRect& aRect) override;
  [[nodiscard]] LayoutDeviceIntMargin GetWidgetBorder(nsDeviceContext* aContext,
                                                      nsIFrame*,
                                                      StyleAppearance) override;

  bool GetWidgetPadding(nsDeviceContext* aContext, nsIFrame*, StyleAppearance,
                        LayoutDeviceIntMargin* aResult) override;

  bool GetWidgetOverflow(nsDeviceContext* aContext, nsIFrame*, StyleAppearance,
                         nsRect* aOverflowRect) override;

  LayoutDeviceIntSize GetMinimumWidgetSize(nsPresContext*, nsIFrame*,
                                           StyleAppearance) override;
  NS_IMETHOD WidgetStateChanged(nsIFrame*, StyleAppearance, nsAtom* aAttribute,
                                bool* aShouldRepaint,
                                const nsAttrValue* aOldValue) override;
  NS_IMETHOD ThemeChanged() override;
  bool ThemeSupportsWidget(nsPresContext* aPresContext, nsIFrame*,
                           StyleAppearance) override;
  bool WidgetIsContainer(StyleAppearance) override;
  bool ThemeDrawsFocusForWidget(nsIFrame*, StyleAppearance) override;
  bool ThemeNeedsComboboxDropmarker() override;
  bool WidgetAppearanceDependsOnWindowFocus(StyleAppearance) override;
  ThemeGeometryType ThemeGeometryTypeForWidget(nsIFrame*,
                                               StyleAppearance) override;
  Transparency GetWidgetTransparency(nsIFrame*, StyleAppearance) override;
  mozilla::Maybe<WidgetInfo> ComputeWidgetInfo(nsIFrame*, StyleAppearance,
                                               const nsRect& aRect);
  void DrawProgress(CGContextRef context, const HIRect& inBoxRect,
                    const ProgressParams& aParams);

 protected:
  virtual ~nsNativeThemeCocoa();

  LayoutDeviceIntMargin DirectionAwareMargin(const LayoutDeviceIntMargin&,
                                             nsIFrame*);
  nsIFrame* SeparatorResponsibility(nsIFrame* aBefore, nsIFrame* aAfter);
  ControlParams ComputeControlParams(nsIFrame*, mozilla::dom::ElementState);
  SegmentParams ComputeSegmentParams(nsIFrame*, mozilla::dom::ElementState,
                                     SegmentType);
  TextFieldParams ComputeTextFieldParams(nsIFrame*, mozilla::dom::ElementState);
  ProgressParams ComputeProgressParams(nsIFrame*, mozilla::dom::ElementState,
                                       bool aIsHorizontal);
  MeterParams ComputeMeterParams(nsIFrame*);
  mozilla::Maybe<ScaleParams> ComputeHTMLScaleParams(
      nsIFrame*, mozilla::dom::ElementState);

  // HITheme drawing routines
  void DrawMeter(CGContextRef context, const HIRect& inBoxRect,
                 const MeterParams& aParams);
  void DrawSegment(CGContextRef cgContext, const HIRect& inBoxRect,
                   const SegmentParams& aParams);
  void DrawSegmentBackground(CGContextRef cgContext, const HIRect& inBoxRect,
                             const SegmentParams& aParams);
  void DrawTabPanel(CGContextRef context, const HIRect& inBoxRect,
                    bool aIsInsideActiveWindow);
  void DrawScale(CGContextRef context, const HIRect& inBoxRect,
                 const ScaleParams& aParams);
  void DrawCheckboxOrRadio(CGContextRef cgContext, bool inCheckbox,
                           const HIRect& inBoxRect,
                           const CheckboxOrRadioParams& aParams);
  void DrawSearchField(CGContextRef cgContext, const HIRect& inBoxRect,
                       const TextFieldParams& aParams);
  void DrawTextField(CGContextRef cgContext, const HIRect& inBoxRect,
                     const TextFieldParams& aParams);
  void DrawPushButton(CGContextRef cgContext, const HIRect& inBoxRect,
                      ButtonType aButtonType, ControlParams aControlParams);
  void DrawSquareBezelPushButton(CGContextRef cgContext,
                                 const HIRect& inBoxRect,
                                 ControlParams aControlParams);
  void DrawHelpButton(CGContextRef cgContext, const HIRect& inBoxRect,
                      ControlParams aControlParams);
  void DrawDisclosureButton(CGContextRef cgContext, const HIRect& inBoxRect,
                            ControlParams aControlParams,
                            NSControlStateValue aState);
  void DrawHIThemeButton(CGContextRef cgContext, const HIRect& aRect,
                         ThemeButtonKind aKind, ThemeButtonValue aValue,
                         ThemeDrawState aState, ThemeButtonAdornment aAdornment,
                         const ControlParams& aParams);
  void DrawButton(CGContextRef context, const HIRect& inBoxRect,
                  const ButtonParams& aParams);
  void DrawDropdown(CGContextRef context, const HIRect& inBoxRect,
                    const DropdownParams& aParams);
  HIThemeButtonDrawInfo SpinButtonDrawInfo(ThemeButtonKind aKind,
                                           const SpinButtonParams& aParams);
  void DrawSpinButtons(CGContextRef context, const HIRect& inBoxRect,
                       const SpinButtonParams& aParams);
  void DrawSpinButton(CGContextRef context, const HIRect& inBoxRect,
                      SpinButton aDrawnButton, const SpinButtonParams& aParams);
  void DrawToolbar(CGContextRef cgContext, const CGRect& inBoxRect,
                   bool aIsMain);
  void DrawStatusBar(CGContextRef cgContext, const HIRect& inBoxRect,
                     bool aIsMain);
  void DrawMultilineTextField(CGContextRef cgContext, const CGRect& inBoxRect,
                              bool aIsFocused);
  void RenderWidget(const WidgetInfo& aWidgetInfo, mozilla::ColorScheme,
                    mozilla::gfx::DrawTarget& aDrawTarget,
                    const mozilla::gfx::Rect& aWidgetRect,
                    const mozilla::gfx::Rect& aDirtyRect, float aScale);

 private:
  NSButtonCell* mDisclosureButtonCell;
  NSButtonCell* mHelpButtonCell;
  NSButtonCell* mPushButtonCell;
  NSButtonCell* mRadioButtonCell;
  NSButtonCell* mCheckboxCell;
  NSTextFieldCell* mTextFieldCell;
  MOZSearchFieldCell* mSearchFieldCell;
  NSPopUpButtonCell* mDropdownCell;
  NSComboBoxCell* mComboBoxCell;
  NSProgressBarCell* mProgressBarCell;
  NSLevelIndicatorCell* mMeterBarCell;
  MOZCellDrawWindow* mCellDrawWindow = nil;
  MOZCellDrawView* mCellDrawView;
};

#endif  // nsNativeThemeCocoa_h_
