/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNativeThemeCocoa_h_
#define nsNativeThemeCocoa_h_

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#include "nsITheme.h"
#include "nsCOMPtr.h"
#include "nsAtom.h"
#include "nsNativeTheme.h"

@class CellDrawView;
@class NSProgressBarCell;
class nsDeviceContext;
struct SegmentedControlRenderSettings;

namespace mozilla {
class EventStates;
} // namespace mozilla

class nsNativeThemeCocoa : private nsNativeTheme,
                           public nsITheme
{
public:
  enum {
    eThemeGeometryTypeTitlebar = eThemeGeometryTypeUnknown + 1,
    eThemeGeometryTypeToolbar,
    eThemeGeometryTypeToolbox,
    eThemeGeometryTypeWindowButtons,
    eThemeGeometryTypeFullscreenButton,
    eThemeGeometryTypeMenu,
    eThemeGeometryTypeHighlightedMenuItem,
    eThemeGeometryTypeVibrancyLight,
    eThemeGeometryTypeVibrancyDark,
    eThemeGeometryTypeVibrantTitlebarLight,
    eThemeGeometryTypeVibrantTitlebarDark,
    eThemeGeometryTypeTooltip,
    eThemeGeometryTypeSheet,
    eThemeGeometryTypeSourceList,
    eThemeGeometryTypeSourceListSelection,
    eThemeGeometryTypeActiveSourceListSelection
  };

  enum class MenuIcon : uint8_t {
    eCheckmark,
    eMenuArrow,
    eMenuDownScrollArrow,
    eMenuUpScrollArrow
  };

  enum class CheckboxOrRadioState : uint8_t {
    eOff,
    eOn,
    eIndeterminate
  };

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

  enum class SpinButton : uint8_t {
    eUp,
    eDown
  };

  enum class SegmentType : uint8_t {
    eToolbarButton,
    eTab
  };

  enum class OptimumState : uint8_t {
    eOptimum,
    eSubOptimum,
    eSubSubOptimum
  };

  struct ControlParams {
    ControlParams()
      : disabled(false)
      , insideActiveWindow(false)
      , pressed(false)
      , focused(false)
      , rtl(false)
    {}

    bool disabled : 1;
    bool insideActiveWindow : 1;
    bool pressed : 1;
    bool focused : 1;
    bool rtl : 1;
  };

  struct MenuBackgroundParams {
    mozilla::Maybe<mozilla::gfx::Color> vibrancyColor;
    bool disabled = false;
    bool submenuRightOfParent = false;
  };

  struct MenuIconParams {
    MenuIcon icon = MenuIcon::eCheckmark;
    bool disabled = false;
    bool insideActiveMenuItem = false;
    bool centerHorizontally = false;
    bool rtl = false;
  };

  struct MenuItemParams {
    mozilla::Maybe<mozilla::gfx::Color> vibrancyColor;
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

  struct ScrollbarParams {
    ScrollbarParams()
      : overlay(false)
      , rolledOver(false)
      , small(false)
      , horizontal(false)
      , rtl(false)
      , onDarkBackground(false)
    {}

    bool overlay : 1;
    bool rolledOver : 1;
    bool small : 1;
    bool horizontal : 1;
    bool rtl : 1;
    bool onDarkBackground : 1;
  };

  nsNativeThemeCocoa();

  NS_DECL_ISUPPORTS_INHERITED

  // The nsITheme interface.
  NS_IMETHOD DrawWidgetBackground(gfxContext* aContext,
                                  nsIFrame* aFrame,
                                  uint8_t aWidgetType,
                                  const nsRect& aRect,
                                  const nsRect& aDirtyRect) override;
  bool CreateWebRenderCommandsForWidget(mozilla::wr::DisplayListBuilder& aBuilder,
                                        mozilla::wr::IpcResourceUpdateQueue& aResources,
                                        const mozilla::layers::StackingContextHelper& aSc,
                                        mozilla::layers::WebRenderLayerManager* aManager,
                                        nsIFrame* aFrame,
                                        uint8_t aWidgetType,
                                        const nsRect& aRect) override;
  NS_IMETHOD GetWidgetBorder(nsDeviceContext* aContext,
                             nsIFrame* aFrame,
                             uint8_t aWidgetType,
                             nsIntMargin* aResult) override;

  virtual bool GetWidgetPadding(nsDeviceContext* aContext,
                                  nsIFrame* aFrame,
                                  uint8_t aWidgetType,
                                  nsIntMargin* aResult) override;

  virtual bool GetWidgetOverflow(nsDeviceContext* aContext, nsIFrame* aFrame,
                                   uint8_t aWidgetType, nsRect* aOverflowRect) override;

  NS_IMETHOD GetMinimumWidgetSize(nsPresContext* aPresContext, nsIFrame* aFrame,
                                  uint8_t aWidgetType,
                                  mozilla::LayoutDeviceIntSize* aResult, bool* aIsOverridable) override;
  NS_IMETHOD WidgetStateChanged(nsIFrame* aFrame, uint8_t aWidgetType, 
                                nsAtom* aAttribute, bool* aShouldRepaint,
                                const nsAttrValue* aOldValue) override;
  NS_IMETHOD ThemeChanged() override;
  bool ThemeSupportsWidget(nsPresContext* aPresContext, nsIFrame* aFrame, uint8_t aWidgetType) override;
  bool WidgetIsContainer(uint8_t aWidgetType) override;
  bool ThemeDrawsFocusForWidget(uint8_t aWidgetType) override;
  bool ThemeNeedsComboboxDropmarker() override;
  virtual bool WidgetAppearanceDependsOnWindowFocus(uint8_t aWidgetType) override;
  virtual bool NeedToClearBackgroundBehindWidget(nsIFrame* aFrame,
                                                 uint8_t aWidgetType) override;
  virtual ThemeGeometryType ThemeGeometryTypeForWidget(nsIFrame* aFrame,
                                                       uint8_t aWidgetType) override;
  virtual Transparency GetWidgetTransparency(nsIFrame* aFrame, uint8_t aWidgetType) override;

  void DrawProgress(CGContextRef context, const HIRect& inBoxRect,
                    const ProgressParams& aParams);

  static void DrawNativeTitlebar(CGContextRef aContext, CGRect aTitlebarRect,
                                 CGFloat aUnifiedHeight, BOOL aIsMain, BOOL aIsFlipped);

protected:
  virtual ~nsNativeThemeCocoa();

  nsIntMargin DirectionAwareMargin(const nsIntMargin& aMargin, nsIFrame* aFrame);
  nsIFrame* SeparatorResponsibility(nsIFrame* aBefore, nsIFrame* aAfter);
  bool IsWindowSheet(nsIFrame* aFrame);
  ControlParams ComputeControlParams(nsIFrame* aFrame,
                                     mozilla::EventStates aEventState);
  MenuBackgroundParams ComputeMenuBackgroundParams(nsIFrame* aFrame,
                                                   mozilla::EventStates aEventState);
  MenuIconParams ComputeMenuIconParams(nsIFrame* aParams,
                                       mozilla::EventStates aEventState,
                                       MenuIcon aIcon);
  MenuItemParams ComputeMenuItemParams(nsIFrame* aFrame,
                                       mozilla::EventStates aEventState,
                                       bool aIsChecked);
  SegmentParams ComputeSegmentParams(nsIFrame* aFrame,
                                     mozilla::EventStates aEventState,
                                     SegmentType aSegmentType);
  SearchFieldParams ComputeSearchFieldParams(nsIFrame* aFrame,
                                             mozilla::EventStates aEventState);
  ProgressParams ComputeProgressParams(nsIFrame* aFrame,
                                       mozilla::EventStates aEventState,
                                       bool aIsHorizontal);
  MeterParams ComputeMeterParams(nsIFrame* aFrame);
  TreeHeaderCellParams ComputeTreeHeaderCellParams(nsIFrame* aFrame,
                                                   mozilla::EventStates aEventState);
  ScaleParams ComputeXULScaleParams(nsIFrame* aFrame,
                                    mozilla::EventStates aEventState,
                                    bool aIsHorizontal);
  mozilla::Maybe<ScaleParams> ComputeHTMLScaleParams(nsIFrame* aFrame,
                                                     mozilla::EventStates aEventState);
  ScrollbarParams ComputeScrollbarParams(nsIFrame* aFrame, bool aIsHorizontal);

  // HITheme drawing routines
  void DrawTextBox(CGContextRef context, const HIRect& inBoxRect,
                   TextBoxParams aParams);
  void DrawMeter(CGContextRef context, const HIRect& inBoxRect,
                 const MeterParams& aParams);
  void DrawSegment(CGContextRef cgContext, const HIRect& inBoxRect,
                   const SegmentParams& aParams);
  void DrawTabPanel(CGContextRef context, const HIRect& inBoxRect,
                    bool aIsInsideActiveWindow);
  void DrawScale(CGContextRef context, const HIRect& inBoxRect,
                 const ScaleParams& aParams);
  void DrawCheckboxOrRadio(CGContextRef cgContext, bool inCheckbox,
                           const HIRect& inBoxRect,
                           const CheckboxOrRadioParams& aParams);
  void DrawSearchField(CGContextRef cgContext, const HIRect& inBoxRect,
                       const SearchFieldParams& aParams);
  void DrawRoundedBezelPushButton(CGContextRef cgContext,
                                  const HIRect& inBoxRect,
                                  ControlParams aControlParams);
  void DrawSquareBezelPushButton(CGContextRef cgContext,
                                 const HIRect& inBoxRect,
                                 ControlParams aControlParams);
  void DrawHelpButton(CGContextRef cgContext, const HIRect& inBoxRect,
                      ControlParams aControlParams);
  void DrawDisclosureButton(CGContextRef cgContext, const HIRect& inBoxRect,
                            ControlParams aControlParams, NSCellStateValue aState);
  void DrawMenuBackground(CGContextRef cgContext, const CGRect& inBoxRect,
                          const MenuBackgroundParams& aParams);
  NSString* GetMenuIconName(const MenuIconParams& aParams);
  NSSize GetMenuIconSize(MenuIcon aIcon);
  void DrawMenuIcon(CGContextRef cgContext, const CGRect& aRect,
                    const MenuIconParams& aParams);
  void DrawMenuItem(CGContextRef cgContext, const CGRect& inBoxRect,
                    const MenuItemParams& aParams);
  void DrawHIThemeButton(CGContextRef cgContext, const HIRect& aRect,
                         ThemeButtonKind aKind, ThemeButtonValue aValue,
                         ThemeDrawState aState, ThemeButtonAdornment aAdornment,
                         const ControlParams& aParams);
  void DrawButton(CGContextRef context, const HIRect& inBoxRect,
                  const ButtonParams& aParams);
  void DrawTreeHeaderCell(CGContextRef context, const HIRect& inBoxRect,
                          const TreeHeaderCellParams& aParams);
  void DrawFocusOutline(CGContextRef cgContext, const HIRect& inBoxRect,
                        mozilla::EventStates inState, uint8_t aWidgetType,
                        nsIFrame* aFrame);
  void DrawDropdown(CGContextRef context, const HIRect& inBoxRect,
                    const DropdownParams& aParams);
  HIThemeButtonDrawInfo SpinButtonDrawInfo(ThemeButtonKind aKind,
                                           const SpinButtonParams& aParams);
  void DrawSpinButtons(CGContextRef context, const HIRect& inBoxRect,
                       const SpinButtonParams& aParams);
  void DrawSpinButton(CGContextRef context,
                      const HIRect& inBoxRect, SpinButton aDrawnButton,
                      const SpinButtonParams& aParams);
  void DrawUnifiedToolbar(CGContextRef cgContext, const HIRect& inBoxRect,
                          const UnifiedToolbarParams& aParams);
  void DrawStatusBar(CGContextRef cgContext, const HIRect& inBoxRect,
                     nsIFrame *aFrame);
  void DrawResizer(CGContextRef cgContext, const HIRect& aRect, nsIFrame *aFrame);
  void DrawScrollbarThumb(CGContextRef cgContext, const CGRect& inBoxRect,
                          ScrollbarParams aParams);
  void DrawScrollbarTrack(CGContextRef cgContext, const CGRect& inBoxRect,
                          ScrollbarParams aParams);

  // Scrollbars
  nsIFrame* GetParentScrollbarFrame(nsIFrame *aFrame);
  bool IsParentScrollbarRolledOver(nsIFrame* aFrame);

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

#endif // nsNativeThemeCocoa_h_
