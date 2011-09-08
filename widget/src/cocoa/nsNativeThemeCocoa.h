/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mike Pinkerton (pinkerton@netscape.com).
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsNativeThemeCocoa_h_
#define nsNativeThemeCocoa_h_

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#include "nsITheme.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsILookAndFeel.h"
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

  virtual PRBool GetWidgetPadding(nsDeviceContext* aContext,
                                  nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  nsIntMargin* aResult);

  virtual PRBool GetWidgetOverflow(nsDeviceContext* aContext, nsIFrame* aFrame,
                                   PRUint8 aWidgetType, nsRect* aOverflowRect);

  NS_IMETHOD GetMinimumWidgetSize(nsRenderingContext* aContext, nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  nsIntSize* aResult, PRBool* aIsOverridable);
  NS_IMETHOD WidgetStateChanged(nsIFrame* aFrame, PRUint8 aWidgetType, 
                                nsIAtom* aAttribute, PRBool* aShouldRepaint);
  NS_IMETHOD ThemeChanged();
  PRBool ThemeSupportsWidget(nsPresContext* aPresContext, nsIFrame* aFrame, PRUint8 aWidgetType);
  PRBool WidgetIsContainer(PRUint8 aWidgetType);
  PRBool ThemeDrawsFocusForWidget(nsPresContext* aPresContext, nsIFrame* aFrame, PRUint8 aWidgetType);
  PRBool ThemeNeedsComboboxDropmarker();
  virtual Transparency GetWidgetTransparency(nsIFrame* aFrame, PRUint8 aWidgetType);

protected:  

  nsresult GetSystemColor(PRUint8 aWidgetType, nsILookAndFeel::nsColorID& aColorID);
  nsIntMargin RTLAwareMargin(const nsIntMargin& aMargin, nsIFrame* aFrame);
  nsIFrame* SeparatorResponsibility(nsIFrame* aBefore, nsIFrame* aAfter);
  CGRect SeparatorAdjustedRect(CGRect aRect, nsIFrame* aLeft,
                               nsIFrame* aCurrent, nsIFrame* aRight);

  // Helpers for progressbar.
  double GetProgressValue(nsIFrame* aFrame);
  double GetProgressMaxValue(nsIFrame* aFrame);

  // HITheme drawing routines
  void DrawFrame(CGContextRef context, HIThemeFrameKind inKind,
                 const HIRect& inBoxRect, PRBool inReadOnly,
                 nsEventStates inState);
  void DrawProgress(CGContextRef context, const HIRect& inBoxRect,
                    PRBool inIsIndeterminate, PRBool inIsHorizontal,
                    double inValue, double inMaxValue, nsIFrame* aFrame);
  void DrawSegment(CGContextRef cgContext, const HIRect& inBoxRect,
                   nsEventStates inState, nsIFrame* aFrame,
                   const SegmentedControlRenderSettings& aSettings);
  void DrawTabPanel(CGContextRef context, const HIRect& inBoxRect, nsIFrame* aFrame);
  void DrawScale(CGContextRef context, const HIRect& inBoxRect,
                 nsEventStates inState, PRBool inDirection,
                 PRBool inIsReverse, PRInt32 inCurrentValue, PRInt32 inMinValue,
                 PRInt32 inMaxValue, nsIFrame* aFrame);
  void DrawCheckboxOrRadio(CGContextRef cgContext, PRBool inCheckbox,
                           const HIRect& inBoxRect, PRBool inSelected,
                           nsEventStates inState, nsIFrame* aFrame);
  void DrawSearchField(CGContextRef cgContext, const HIRect& inBoxRect,
                       nsIFrame* aFrame, nsEventStates inState);
  void DrawPushButton(CGContextRef cgContext, const HIRect& inBoxRect,
                      nsEventStates inState, nsIFrame* aFrame);
  void DrawButton(CGContextRef context, ThemeButtonKind inKind,
                  const HIRect& inBoxRect, PRBool inIsDefault, 
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
                             const CGSize& aSize, PRBool aShouldGetButtonStates);
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
