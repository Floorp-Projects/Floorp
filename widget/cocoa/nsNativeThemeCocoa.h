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
#include "nsCOMPtr.h"
#include "nsAtom.h"
#include "nsNativeTheme.h"
#include "ScrollbarDrawingMac.h"

@class CellDrawView;
@class NSProgressBarCell;
class nsDeviceContext;
struct SegmentedControlRenderSettings;

namespace mozilla {
class EventStates;
namespace gfx {
class DrawTarget;
}  // namespace gfx
}  // namespace mozilla

class nsNativeThemeCocoa : private nsNativeTheme, public nsITheme {
 public:
  enum class MenuIcon : uint8_t {
    eCheckmark,
    eMenuArrow,
    eMenuDownScrollArrow,
    eMenuUpScrollArrow
  };

  enum class CheckboxOrRadioState : uint8_t { eOff, eOn, eIndeterminate };

  enum class ButtonType : uint8_t {
    eRegularPushButton,
    eDefaultPushButton,
    eRegularBevelButton,
    eDefaultBevelButton,
    eRoundedBezelPushButton,
    eSquareBezelPushButton,
    eArrowButton,
    eHelpButton,
    eTreeTwistyPointingRight,
    eTreeTwistyPointingDown,
    eDisclosureButtonClosed,
    eDisclosureButtonOpen
  };

  enum class SpinButton : uint8_t { eUp, eDown };

  enum class SegmentType : uint8_t { eToolbarButton, eTab };

  enum class OptimumState : uint8_t { eOptimum, eSubOptimum, eSubSubOptimum };

  struct ControlParams {
    ControlParams()
        : disabled(false), insideActiveWindow(false), pressed(false), focused(false), rtl(false) {}

    bool disabled : 1;
    bool insideActiveWindow : 1;
    bool pressed : 1;
    bool focused : 1;
    bool rtl : 1;
  };

  struct MenuIconParams {
    MenuIcon icon = MenuIcon::eCheckmark;
    bool disabled = false;
    bool insideActiveMenuItem = false;
    bool centerHorizontally = false;
    bool rtl = false;
  };

  struct MenuItemParams {
    bool checked = false;
    bool disabled = false;
    bool selected = false;
    bool rtl = false;
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

  struct UnifiedToolbarParams {
    float unifiedHeight = 0.0f;
    bool isMain = false;
  };

  struct TextBoxParams {
    bool disabled = false;
    bool focused = false;
    bool borderless = false;
  };

  struct SearchFieldParams {
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

  struct TreeHeaderCellParams {
    ControlParams controlParams;
    TreeSortDirection sortDirection = eTreeSortDirection_Natural;
    bool lastTreeHeaderCell = false;
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

  using ScrollbarParams = mozilla::widget::ScrollbarParams;

  enum Widget : uint8_t {
    eColorFill,      // mozilla::gfx::sRGBColor
    eMenuIcon,       // MenuIconParams
    eMenuItem,       // MenuItemParams
    eMenuSeparator,  // MenuItemParams
    eCheckbox,       // CheckboxOrRadioParams
    eRadio,          // CheckboxOrRadioParams
    eButton,         // ButtonParams
    eDropdown,       // DropdownParams
    eFocusOutline,
    eSpinButtons,     // SpinButtonParams
    eSpinButtonUp,    // SpinButtonParams
    eSpinButtonDown,  // SpinButtonParams
    eSegment,         // SegmentParams
    eSeparator,
    eUnifiedToolbar,  // UnifiedToolbarParams
    eToolbar,         // bool
    eNativeTitlebar,  // UnifiedToolbarParams
    eStatusBar,       // bool
    eGroupBox,
    eTextBox,             // TextBoxParams
    eSearchField,         // SearchFieldParams
    eProgressBar,         // ProgressParams
    eMeter,               // MeterParams
    eTreeHeaderCell,      // TreeHeaderCellParams
    eScale,               // ScaleParams
    eScrollbarThumb,      // ScrollbarParams
    eScrollbarTrack,      // ScrollbarParams
    eScrollCorner,        // ScrollbarParams
    eMultilineTextField,  // bool
    eListBox,
    eActiveSourceListSelection,    // bool
    eInactiveSourceListSelection,  // bool
    eTabPanel,
    eResizer
  };

  struct WidgetInfo {
    static WidgetInfo ColorFill(const mozilla::gfx::sRGBColor& aParams) {
      return WidgetInfo(Widget::eColorFill, aParams);
    }
    static WidgetInfo MenuIcon(const MenuIconParams& aParams) {
      return WidgetInfo(Widget::eMenuIcon, aParams);
    }
    static WidgetInfo MenuItem(const MenuItemParams& aParams) {
      return WidgetInfo(Widget::eMenuItem, aParams);
    }
    static WidgetInfo MenuSeparator(const MenuItemParams& aParams) {
      return WidgetInfo(Widget::eMenuSeparator, aParams);
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
    static WidgetInfo FocusOutline() { return WidgetInfo(Widget::eFocusOutline, false); }
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
    static WidgetInfo Separator() { return WidgetInfo(Widget::eSeparator, false); }
    static WidgetInfo UnifiedToolbar(const UnifiedToolbarParams& aParams) {
      return WidgetInfo(Widget::eUnifiedToolbar, aParams);
    }
    static WidgetInfo Toolbar(bool aParams) { return WidgetInfo(Widget::eToolbar, aParams); }
    static WidgetInfo NativeTitlebar(const UnifiedToolbarParams& aParams) {
      return WidgetInfo(Widget::eNativeTitlebar, aParams);
    }
    static WidgetInfo StatusBar(bool aParams) { return WidgetInfo(Widget::eStatusBar, aParams); }
    static WidgetInfo GroupBox() { return WidgetInfo(Widget::eGroupBox, false); }
    static WidgetInfo TextBox(const TextBoxParams& aParams) {
      return WidgetInfo(Widget::eTextBox, aParams);
    }
    static WidgetInfo SearchField(const SearchFieldParams& aParams) {
      return WidgetInfo(Widget::eSearchField, aParams);
    }
    static WidgetInfo ProgressBar(const ProgressParams& aParams) {
      return WidgetInfo(Widget::eProgressBar, aParams);
    }
    static WidgetInfo Meter(const MeterParams& aParams) {
      return WidgetInfo(Widget::eMeter, aParams);
    }
    static WidgetInfo TreeHeaderCell(const TreeHeaderCellParams& aParams) {
      return WidgetInfo(Widget::eTreeHeaderCell, aParams);
    }
    static WidgetInfo Scale(const ScaleParams& aParams) {
      return WidgetInfo(Widget::eScale, aParams);
    }
    static WidgetInfo ScrollbarThumb(const ScrollbarParams& aParams) {
      return WidgetInfo(Widget::eScrollbarThumb, aParams);
    }
    static WidgetInfo ScrollbarTrack(const ScrollbarParams& aParams) {
      return WidgetInfo(Widget::eScrollbarTrack, aParams);
    }
    static WidgetInfo ScrollCorner(const ScrollbarParams& aParams) {
      return WidgetInfo(Widget::eScrollCorner, aParams);
    }
    static WidgetInfo MultilineTextField(bool aParams) {
      return WidgetInfo(Widget::eMultilineTextField, aParams);
    }
    static WidgetInfo ListBox() { return WidgetInfo(Widget::eListBox, false); }
    static WidgetInfo ActiveSourceListSelection(bool aParams) {
      return WidgetInfo(Widget::eActiveSourceListSelection, aParams);
    }
    static WidgetInfo InactiveSourceListSelection(bool aParams) {
      return WidgetInfo(Widget::eInactiveSourceListSelection, aParams);
    }
    static WidgetInfo TabPanel(bool aParams) { return WidgetInfo(Widget::eTabPanel, aParams); }
    static WidgetInfo Resizer(bool aParams) { return WidgetInfo(Widget::eResizer, aParams); }

    template <typename T>
    T Params() const {
      MOZ_RELEASE_ASSERT(mVariant.is<T>());
      return mVariant.as<T>();
    }

    enum Widget Widget() const { return mWidget; }

   private:
    template <typename T>
    WidgetInfo(enum Widget aWidget, const T& aParams) : mVariant(aParams), mWidget(aWidget) {}

    mozilla::Variant<mozilla::gfx::sRGBColor, MenuIconParams, MenuItemParams, CheckboxOrRadioParams,
                     ButtonParams, DropdownParams, SpinButtonParams, SegmentParams,
                     UnifiedToolbarParams, TextBoxParams, SearchFieldParams, ProgressParams,
                     MeterParams, TreeHeaderCellParams, ScaleParams, ScrollbarParams, bool>
        mVariant;

    enum Widget mWidget;
  };

  using ScrollbarDrawingMac = mozilla::widget::ScrollbarDrawingMac;

  nsNativeThemeCocoa();

  NS_DECL_ISUPPORTS_INHERITED

  // The nsITheme interface.
  NS_IMETHOD DrawWidgetBackground(gfxContext* aContext, nsIFrame* aFrame,
                                  StyleAppearance aAppearance, const nsRect& aRect,
                                  const nsRect& aDirtyRect) override;
  bool CreateWebRenderCommandsForWidget(mozilla::wr::DisplayListBuilder& aBuilder,
                                        mozilla::wr::IpcResourceUpdateQueue& aResources,
                                        const mozilla::layers::StackingContextHelper& aSc,
                                        mozilla::layers::RenderRootStateManager* aManager,
                                        nsIFrame* aFrame, StyleAppearance aAppearance,
                                        const nsRect& aRect) override;
  [[nodiscard]] LayoutDeviceIntMargin GetWidgetBorder(nsDeviceContext* aContext, nsIFrame* aFrame,
                                                      StyleAppearance aAppearance) override;

  bool GetWidgetPadding(nsDeviceContext* aContext, nsIFrame* aFrame, StyleAppearance aAppearance,
                        LayoutDeviceIntMargin* aResult) override;

  virtual bool GetWidgetOverflow(nsDeviceContext* aContext, nsIFrame* aFrame,
                                 StyleAppearance aAppearance, nsRect* aOverflowRect) override;

  ScrollbarSizes GetScrollbarSizes(nsPresContext*, StyleScrollbarWidth, Overlay) override;
  NS_IMETHOD GetMinimumWidgetSize(nsPresContext* aPresContext, nsIFrame* aFrame,
                                  StyleAppearance aAppearance,
                                  mozilla::LayoutDeviceIntSize* aResult,
                                  bool* aIsOverridable) override;
  NS_IMETHOD WidgetStateChanged(nsIFrame* aFrame, StyleAppearance aAppearance, nsAtom* aAttribute,
                                bool* aShouldRepaint, const nsAttrValue* aOldValue) override;
  NS_IMETHOD ThemeChanged() override;
  bool ThemeSupportsWidget(nsPresContext* aPresContext, nsIFrame* aFrame,
                           StyleAppearance aAppearance) override;
  bool WidgetIsContainer(StyleAppearance aAppearance) override;
  bool ThemeDrawsFocusForWidget(StyleAppearance aAppearance) override;
  bool ThemeNeedsComboboxDropmarker() override;
  virtual bool WidgetAppearanceDependsOnWindowFocus(StyleAppearance aAppearance) override;
  virtual ThemeGeometryType ThemeGeometryTypeForWidget(nsIFrame* aFrame,
                                                       StyleAppearance aAppearance) override;
  virtual Transparency GetWidgetTransparency(nsIFrame* aFrame,
                                             StyleAppearance aAppearance) override;
  mozilla::Maybe<WidgetInfo> ComputeWidgetInfo(nsIFrame* aFrame, StyleAppearance aAppearance,
                                               const nsRect& aRect);
  void DrawProgress(CGContextRef context, const HIRect& inBoxRect, const ProgressParams& aParams);

  static void DrawNativeTitlebar(CGContextRef aContext, CGRect aTitlebarRect,
                                 CGFloat aUnifiedHeight, BOOL aIsMain, BOOL aIsFlipped);

 protected:
  virtual ~nsNativeThemeCocoa();

  LayoutDeviceIntMargin DirectionAwareMargin(const LayoutDeviceIntMargin& aMargin,
                                             nsIFrame* aFrame);
  nsIFrame* SeparatorResponsibility(nsIFrame* aBefore, nsIFrame* aAfter);
  ControlParams ComputeControlParams(nsIFrame* aFrame, mozilla::EventStates aEventState);
  MenuIconParams ComputeMenuIconParams(nsIFrame* aParams, mozilla::EventStates aEventState,
                                       MenuIcon aIcon);
  MenuItemParams ComputeMenuItemParams(nsIFrame* aFrame, mozilla::EventStates aEventState,
                                       bool aIsChecked);
  SegmentParams ComputeSegmentParams(nsIFrame* aFrame, mozilla::EventStates aEventState,
                                     SegmentType aSegmentType);
  SearchFieldParams ComputeSearchFieldParams(nsIFrame* aFrame, mozilla::EventStates aEventState);
  ProgressParams ComputeProgressParams(nsIFrame* aFrame, mozilla::EventStates aEventState,
                                       bool aIsHorizontal);
  MeterParams ComputeMeterParams(nsIFrame* aFrame);
  TreeHeaderCellParams ComputeTreeHeaderCellParams(nsIFrame* aFrame,
                                                   mozilla::EventStates aEventState);
  mozilla::Maybe<ScaleParams> ComputeHTMLScaleParams(nsIFrame* aFrame,
                                                     mozilla::EventStates aEventState);

  // HITheme drawing routines
  void DrawTextBox(CGContextRef context, const HIRect& inBoxRect, TextBoxParams aParams);
  void DrawMeter(CGContextRef context, const HIRect& inBoxRect, const MeterParams& aParams);
  void DrawSegment(CGContextRef cgContext, const HIRect& inBoxRect, const SegmentParams& aParams);
  void DrawSegmentBackground(CGContextRef cgContext, const HIRect& inBoxRect,
                             const SegmentParams& aParams);
  void DrawTabPanel(CGContextRef context, const HIRect& inBoxRect, bool aIsInsideActiveWindow);
  void DrawScale(CGContextRef context, const HIRect& inBoxRect, const ScaleParams& aParams);
  void DrawCheckboxOrRadio(CGContextRef cgContext, bool inCheckbox, const HIRect& inBoxRect,
                           const CheckboxOrRadioParams& aParams);
  void DrawSearchField(CGContextRef cgContext, const HIRect& inBoxRect,
                       const SearchFieldParams& aParams);
  void DrawRoundedBezelPushButton(CGContextRef cgContext, const HIRect& inBoxRect,
                                  ControlParams aControlParams);
  void DrawSquareBezelPushButton(CGContextRef cgContext, const HIRect& inBoxRect,
                                 ControlParams aControlParams);
  void DrawHelpButton(CGContextRef cgContext, const HIRect& inBoxRect,
                      ControlParams aControlParams);
  void DrawDisclosureButton(CGContextRef cgContext, const HIRect& inBoxRect,
                            ControlParams aControlParams, NSCellStateValue aState);
  NSString* GetMenuIconName(const MenuIconParams& aParams);
  NSSize GetMenuIconSize(MenuIcon aIcon);
  void DrawMenuIcon(CGContextRef cgContext, const CGRect& aRect, const MenuIconParams& aParams);
  void DrawMenuItem(CGContextRef cgContext, const CGRect& inBoxRect, const MenuItemParams& aParams);
  void DrawMenuSeparator(CGContextRef cgContext, const CGRect& inBoxRect,
                         const MenuItemParams& aParams);
  void DrawHIThemeButton(CGContextRef cgContext, const HIRect& aRect, ThemeButtonKind aKind,
                         ThemeButtonValue aValue, ThemeDrawState aState,
                         ThemeButtonAdornment aAdornment, const ControlParams& aParams);
  void DrawButton(CGContextRef context, const HIRect& inBoxRect, const ButtonParams& aParams);
  void DrawTreeHeaderCell(CGContextRef context, const HIRect& inBoxRect,
                          const TreeHeaderCellParams& aParams);
  void DrawFocusOutline(CGContextRef cgContext, const HIRect& inBoxRect);
  void DrawDropdown(CGContextRef context, const HIRect& inBoxRect, const DropdownParams& aParams);
  HIThemeButtonDrawInfo SpinButtonDrawInfo(ThemeButtonKind aKind, const SpinButtonParams& aParams);
  void DrawSpinButtons(CGContextRef context, const HIRect& inBoxRect,
                       const SpinButtonParams& aParams);
  void DrawSpinButton(CGContextRef context, const HIRect& inBoxRect, SpinButton aDrawnButton,
                      const SpinButtonParams& aParams);
  void DrawToolbar(CGContextRef cgContext, const CGRect& inBoxRect, bool aIsMain);
  void DrawUnifiedToolbar(CGContextRef cgContext, const HIRect& inBoxRect,
                          const UnifiedToolbarParams& aParams);
  void DrawNativeTitlebar(CGContextRef aContext, CGRect aTitlebarRect,
                          const UnifiedToolbarParams& aParams);
  void DrawStatusBar(CGContextRef cgContext, const HIRect& inBoxRect, bool aIsMain);
  void DrawResizer(CGContextRef cgContext, const HIRect& aRect, bool aIsRTL);
  void DrawMultilineTextField(CGContextRef cgContext, const CGRect& inBoxRect, bool aIsFocused);
  void DrawSourceListSelection(CGContextRef aContext, const CGRect& aRect, bool aWindowIsActive,
                               bool aSelectionIsActive);

  void RenderWidget(const WidgetInfo& aWidgetInfo, mozilla::gfx::DrawTarget& aDrawTarget,
                    const mozilla::gfx::Rect& aWidgetRect, const mozilla::gfx::Rect& aDirtyRect,
                    float aScale);

 private:
  NSButtonCell* mDisclosureButtonCell;
  NSButtonCell* mHelpButtonCell;
  NSButtonCell* mPushButtonCell;
  NSButtonCell* mRadioButtonCell;
  NSButtonCell* mCheckboxCell;
  NSSearchFieldCell* mSearchFieldCell;
  NSSearchFieldCell* mToolbarSearchFieldCell;
  NSPopUpButtonCell* mDropdownCell;
  NSComboBoxCell* mComboBoxCell;
  NSProgressBarCell* mProgressBarCell;
  NSLevelIndicatorCell* mMeterBarCell;
  CellDrawView* mCellDrawView;
};

#endif  // nsNativeThemeCocoa_h_
