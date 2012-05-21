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
#include "gfxASurface.h"

@class CellDrawView;
@class NSProgressBarCell;
@class ContextAwareSearchFieldCell;
class nsDeviceContext;
struct SegmentedControlRenderSettings;

class nsNativeThemeCocoa : private nsNativeTheme,
                           public nsITheme
{
public:
  nsNativeThemeCocoa();
  virtual ~nsNativeThemeCocoa();

  NS_DECL_ISUPPORTS_INHERITED

  // The nsITheme interface.
  NS_IMETHOD DrawWidgetBackground(nsRenderingContext* aContext,
                                  nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  const nsRect& aRect,
                                  const nsRect& aDirtyRect);
  NS_IMETHOD GetWidgetBorder(nsDeviceContext* aContext, 
                             nsIFrame* aFrame,
                             PRUint8 aWidgetType,
                             nsIntMargin* aResult);

  virtual bool GetWidgetPadding(nsDeviceContext* aContext,
                                  nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  nsIntMargin* aResult);

  virtual bool GetWidgetOverflow(nsDeviceContext* aContext, nsIFrame* aFrame,
                                   PRUint8 aWidgetType, nsRect* aOverflowRect);

  NS_IMETHOD GetMinimumWidgetSize(nsRenderingContext* aContext, nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  nsIntSize* aResult, bool* aIsOverridable);
  NS_IMETHOD WidgetStateChanged(nsIFrame* aFrame, PRUint8 aWidgetType, 
                                nsIAtom* aAttribute, bool* aShouldRepaint);
  NS_IMETHOD ThemeChanged();
  bool ThemeSupportsWidget(nsPresContext* aPresContext, nsIFrame* aFrame, PRUint8 aWidgetType);
  bool WidgetIsContainer(PRUint8 aWidgetType);
  bool ThemeDrawsFocusForWidget(nsPresContext* aPresContext, nsIFrame* aFrame, PRUint8 aWidgetType);
  bool ThemeNeedsComboboxDropmarker();
  virtual Transparency GetWidgetTransparency(nsIFrame* aFrame, PRUint8 aWidgetType);

protected:  

  nsIntMargin RTLAwareMargin(const nsIntMargin& aMargin, nsIFrame* aFrame);
  nsIFrame* SeparatorResponsibility(nsIFrame* aBefore, nsIFrame* aAfter);
  CGRect SeparatorAdjustedRect(CGRect aRect, nsIFrame* aLeft,
                               nsIFrame* aCurrent, nsIFrame* aRight);

  // Helpers for progressbar.
  double GetProgressValue(nsIFrame* aFrame);
  double GetProgressMaxValue(nsIFrame* aFrame);

  // HITheme drawing routines
  void DrawFrame(CGContextRef context, HIThemeFrameKind inKind,
                 const HIRect& inBoxRect, bool inReadOnly,
                 nsEventStates inState);
  void DrawProgress(CGContextRef context, const HIRect& inBoxRect,
                    bool inIsIndeterminate, bool inIsHorizontal,
                    double inValue, double inMaxValue, nsIFrame* aFrame);
  void DrawSegment(CGContextRef cgContext, const HIRect& inBoxRect,
                   nsEventStates inState, nsIFrame* aFrame,
                   const SegmentedControlRenderSettings& aSettings);
  void DrawTabPanel(CGContextRef context, const HIRect& inBoxRect, nsIFrame* aFrame);
  void DrawScale(CGContextRef context, const HIRect& inBoxRect,
                 nsEventStates inState, bool inDirection,
                 bool inIsReverse, PRInt32 inCurrentValue, PRInt32 inMinValue,
                 PRInt32 inMaxValue, nsIFrame* aFrame);
  void DrawCheckboxOrRadio(CGContextRef cgContext, bool inCheckbox,
                           const HIRect& inBoxRect, bool inSelected,
                           nsEventStates inState, nsIFrame* aFrame);
  void DrawSearchField(CGContextRef cgContext, const HIRect& inBoxRect,
                       nsIFrame* aFrame, nsEventStates inState);
  void DrawPushButton(CGContextRef cgContext, const HIRect& inBoxRect,
                      nsEventStates inState, nsIFrame* aFrame);
  void DrawButton(CGContextRef context, ThemeButtonKind inKind,
                  const HIRect& inBoxRect, bool inIsDefault, 
                  ThemeButtonValue inValue, ThemeButtonAdornment inAdornment,
                  nsEventStates inState, nsIFrame* aFrame);
  void DrawDropdown(CGContextRef context, const HIRect& inBoxRect,
                    nsEventStates inState, PRUint8 aWidgetType,
                    nsIFrame* aFrame);
  void DrawSpinButtons(CGContextRef context, ThemeButtonKind inKind,
                       const HIRect& inBoxRect, ThemeDrawState inDrawState,
                       ThemeButtonAdornment inAdornment, nsEventStates inState,
                       nsIFrame* aFrame);
  void DrawUnifiedToolbar(CGContextRef cgContext, const HIRect& inBoxRect,
                          NSWindow* aWindow);
  void DrawStatusBar(CGContextRef cgContext, const HIRect& inBoxRect,
                     nsIFrame *aFrame);
  void DrawResizer(CGContextRef cgContext, const HIRect& aRect, nsIFrame *aFrame);

  // Scrollbars
  void DrawScrollbar(CGContextRef aCGContext, const HIRect& aBoxRect, nsIFrame *aFrame);
  void GetScrollbarPressStates (nsIFrame *aFrame, nsEventStates aButtonStates[]);
  void GetScrollbarDrawInfo (HIThemeTrackDrawInfo& aTdi, nsIFrame *aFrame, 
                             const CGSize& aSize, bool aShouldGetButtonStates);
  nsIFrame* GetParentScrollbarFrame(nsIFrame *aFrame);

private:
  NSButtonCell* mPushButtonCell;
  NSButtonCell* mRadioButtonCell;
  NSButtonCell* mCheckboxCell;
  ContextAwareSearchFieldCell* mSearchFieldCell;
  NSPopUpButtonCell* mDropdownCell;
  NSComboBoxCell* mComboBoxCell;
  NSProgressBarCell* mProgressBarCell;
  CellDrawView* mCellDrawView;
};

#endif // nsNativeThemeCocoa_h_
