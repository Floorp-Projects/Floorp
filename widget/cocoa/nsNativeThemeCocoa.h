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
#include "nsIAtom.h"
#include "nsNativeTheme.h"

@class CellDrawView;
@class NSProgressBarCell;
@class ContextAwareSearchFieldCell;
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
    eThemeGeometryTypeTooltip,
    eThemeGeometryTypeSheet,
    eThemeGeometryTypeSourceList,
  };

  nsNativeThemeCocoa();

  NS_DECL_ISUPPORTS_INHERITED

  // The nsITheme interface.
  NS_IMETHOD DrawWidgetBackground(nsRenderingContext* aContext,
                                  nsIFrame* aFrame,
                                  uint8_t aWidgetType,
                                  const nsRect& aRect,
                                  const nsRect& aDirtyRect) override;
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
                                nsIAtom* aAttribute, bool* aShouldRepaint,
                                const nsAttrValue* aOldValue) override;
  NS_IMETHOD ThemeChanged() override;
  bool ThemeSupportsWidget(nsPresContext* aPresContext, nsIFrame* aFrame, uint8_t aWidgetType) override;
  bool WidgetIsContainer(uint8_t aWidgetType) override;
  bool ThemeDrawsFocusForWidget(uint8_t aWidgetType) override;
  bool ThemeNeedsComboboxDropmarker() override;
  virtual bool WidgetAppearanceDependsOnWindowFocus(uint8_t aWidgetType) override;
  virtual bool NeedToClearBackgroundBehindWidget(nsIFrame* aFrame,
                                                 uint8_t aWidgetType) override;
  virtual bool WidgetProvidesFontSmoothingBackgroundColor(nsIFrame* aFrame, uint8_t aWidgetType,
                                                          nscolor* aColor) override;
  virtual ThemeGeometryType ThemeGeometryTypeForWidget(nsIFrame* aFrame,
                                                       uint8_t aWidgetType) override;
  virtual Transparency GetWidgetTransparency(nsIFrame* aFrame, uint8_t aWidgetType) override;

  void DrawProgress(CGContextRef context, const HIRect& inBoxRect,
                    bool inIsIndeterminate, bool inIsHorizontal,
                    double inValue, double inMaxValue, nsIFrame* aFrame);

  static void DrawNativeTitlebar(CGContextRef aContext, CGRect aTitlebarRect,
                                 CGFloat aUnifiedHeight, BOOL aIsMain, BOOL aIsFlipped);

protected:
  virtual ~nsNativeThemeCocoa();

  nsIntMargin DirectionAwareMargin(const nsIntMargin& aMargin, nsIFrame* aFrame);
  nsIFrame* SeparatorResponsibility(nsIFrame* aBefore, nsIFrame* aAfter);
  CGRect SeparatorAdjustedRect(CGRect aRect, nsIFrame* aLeft,
                               nsIFrame* aCurrent, nsIFrame* aRight);
  bool IsWindowSheet(nsIFrame* aFrame);

  // HITheme drawing routines
  void DrawFrame(CGContextRef context, HIThemeFrameKind inKind,
                 const HIRect& inBoxRect, bool inReadOnly,
                 mozilla::EventStates inState);
  void DrawMeter(CGContextRef context, const HIRect& inBoxRect,
                 nsIFrame* aFrame);
  void DrawSegment(CGContextRef cgContext, const HIRect& inBoxRect,
                   mozilla::EventStates inState, nsIFrame* aFrame,
                   const SegmentedControlRenderSettings& aSettings);
  void DrawTabPanel(CGContextRef context, const HIRect& inBoxRect, nsIFrame* aFrame);
  void DrawScale(CGContextRef context, const HIRect& inBoxRect,
                 mozilla::EventStates inState, bool inDirection,
                 bool inIsReverse, int32_t inCurrentValue, int32_t inMinValue,
                 int32_t inMaxValue, nsIFrame* aFrame);
  void DrawCheckboxOrRadio(CGContextRef cgContext, bool inCheckbox,
                           const HIRect& inBoxRect, bool inSelected,
                           mozilla::EventStates inState, nsIFrame* aFrame);
  void DrawSearchField(CGContextRef cgContext, const HIRect& inBoxRect,
                       nsIFrame* aFrame, mozilla::EventStates inState);
  void DrawPushButton(CGContextRef cgContext, const HIRect& inBoxRect,
                      mozilla::EventStates inState, uint8_t aWidgetType,
                      nsIFrame* aFrame, float aOriginalHeight);
  void DrawMenuIcon(CGContextRef cgContext, const CGRect& aRect,
                    mozilla::EventStates inState, nsIFrame* aFrame,
                    const NSSize& aIconSize, NSString* aImageName,
                    bool aCenterHorizontally);
  void DrawButton(CGContextRef context, ThemeButtonKind inKind,
                  const HIRect& inBoxRect, bool inIsDefault, 
                  ThemeButtonValue inValue, ThemeButtonAdornment inAdornment,
                  mozilla::EventStates inState, nsIFrame* aFrame);
  void DrawFocusOutline(CGContextRef cgContext, const HIRect& inBoxRect,
                        mozilla::EventStates inState, uint8_t aWidgetType,
                        nsIFrame* aFrame);
  void DrawDropdown(CGContextRef context, const HIRect& inBoxRect,
                    mozilla::EventStates inState, uint8_t aWidgetType,
                    nsIFrame* aFrame);
  void DrawSpinButtons(CGContextRef context, ThemeButtonKind inKind,
                       const HIRect& inBoxRect, ThemeDrawState inDrawState,
                       ThemeButtonAdornment inAdornment,
                       mozilla::EventStates inState, nsIFrame* aFrame);
  void DrawSpinButton(CGContextRef context, ThemeButtonKind inKind,
                      const HIRect& inBoxRect, ThemeDrawState inDrawState,
                      ThemeButtonAdornment inAdornment,
                      mozilla::EventStates inState,
                      nsIFrame* aFrame, uint8_t aWidgetType);
  void DrawUnifiedToolbar(CGContextRef cgContext, const HIRect& inBoxRect,
                          NSWindow* aWindow);
  void DrawStatusBar(CGContextRef cgContext, const HIRect& inBoxRect,
                     nsIFrame *aFrame);
  void DrawResizer(CGContextRef cgContext, const HIRect& aRect, nsIFrame *aFrame);

  // Scrollbars
  void GetScrollbarPressStates(nsIFrame *aFrame,
                               mozilla::EventStates aButtonStates[]);
  nsIFrame* GetParentScrollbarFrame(nsIFrame *aFrame);
  bool IsParentScrollbarRolledOver(nsIFrame* aFrame);

private:
  NSButtonCell* mDisclosureButtonCell;
  NSButtonCell* mHelpButtonCell;
  NSButtonCell* mPushButtonCell;
  NSButtonCell* mRadioButtonCell;
  NSButtonCell* mCheckboxCell;
  ContextAwareSearchFieldCell* mSearchFieldCell;
  NSPopUpButtonCell* mDropdownCell;
  NSComboBoxCell* mComboBoxCell;
  NSProgressBarCell* mProgressBarCell;
  NSLevelIndicatorCell* mMeterBarCell;
  CellDrawView* mCellDrawView;
};

#endif // nsNativeThemeCocoa_h_
