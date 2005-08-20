/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 *   Mike Pinkerton (pinkerton@netscape.com)
 *
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <Appearance.h>

#include "nsITheme.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsILookAndFeel.h"
#include "nsIDeviceContext.h"


class nsNativeThemeMac : public nsITheme
{
public:
  nsNativeThemeMac();
  virtual ~nsNativeThemeMac();

  NS_DECL_ISUPPORTS

  // The nsITheme interface.
  NS_IMETHOD DrawWidgetBackground(nsIRenderingContext* aContext,
                                  nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  const nsRect& aRect,
                                  const nsRect& aClipRect);
  NS_IMETHOD GetWidgetBorder(nsIDeviceContext* aContext, 
                             nsIFrame* aFrame,
                             PRUint8 aWidgetType,
                             nsMargin* aResult);
  NS_IMETHOD GetMinimumWidgetSize(nsIRenderingContext* aContext, nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  nsSize* aResult, PRBool* aIsOverridable);
  NS_IMETHOD WidgetStateChanged(nsIFrame* aFrame, PRUint8 aWidgetType, 
                                nsIAtom* aAttribute, PRBool* aShouldRepaint);
  NS_IMETHOD ThemeChanged();
  PRBool ThemeSupportsWidget(nsIPresContext* aPresContext, PRUint8 aWidgetType);
  PRBool WidgetIsContainer(PRUint8 aWidgetType);

protected:

  //nsresult GetThemePartAndState(nsIFrame* aFrame, PRUint8 aWidgetType, 
  //                               PRInt32& aPart, PRInt32& aState);
  nsresult GetSystemColor(PRUint8 aWidgetType, nsILookAndFeel::nsColorID& aColorID);
  nsresult GetSystemFont(PRUint8 aWidgetType, nsSystemFontID& aFont);

  PRBool IsDisabled(nsIFrame* aFrame);
  PRBool IsChecked(nsIFrame* aFrame);
  PRBool IsSelected(nsIFrame* aFrame);
  PRBool IsDefaultButton(nsIFrame* aFrame);
  PRBool IsIndeterminate(nsIFrame* aFrame);

    // Appearance Manager drawing routines
  void DrawCheckbox ( const Rect& inBoxRect, PRBool inChecked, PRBool inDisabled, PRInt32 inState ) ;
  void DrawRadio ( const Rect& inBoxRect, PRBool inChecked, PRBool inDisabled, PRInt32 inState ) ;
  void DrawToolbar ( const Rect& inBoxRect ) ;
  void DrawEditText ( const Rect& inBoxRect, PRBool inIsDisabled ) ;
  void DrawProgress ( const Rect& inBoxRect, PRBool inIsDisabled, PRBool inIsIndeterminate, 
                        PRBool inIsHorizontal, PRInt32 inValue ) ;
  void DrawFullScrollbar  ( const Rect& inScrollbarRect, PRInt32 inWidgetHit, PRInt32 inLineHeight, PRBool inIsDisabled,
                             PRInt32 inMax, PRInt32 inValue, PRInt32 inState ) ;
//  void DrawScrollArrows ( const Rect& inScrollbarRect, PRBool inIsDisabled, PRInt32 inWidget, PRInt32 inState ) ;
  
  void DrawButton ( ThemeButtonKind inKind, const Rect& inBoxRect, PRBool inIsDefault, 
                      PRBool inDisabled, PRInt32 inState ) ;
  void DrawCheckboxRadio ( ThemeButtonKind inKind, const Rect& inBoxRect, PRBool inChecked, 
                              PRBool inDisabled, PRInt32 inState ) ;

    // some utility routines
  nsIFrame* GetScrollbarParent(nsIFrame* inButton, nsPoint* offset);
  nsIFrame* GetScrollbarParentLocalRect(nsIFrame* inButton, nsTransform2D* inMatrix, Rect* outAdjustedRect);

private:

  nsCOMPtr<nsIAtom> mCheckedAtom;
  nsCOMPtr<nsIAtom> mDisabledAtom;
  nsCOMPtr<nsIAtom> mSelectedAtom;
  nsCOMPtr<nsIAtom> mDefaultAtom;
  nsCOMPtr<nsIAtom> mValueAtom;
  nsCOMPtr<nsIAtom> mModeAtom;
  nsCOMPtr<nsIAtom> mOrientAtom;
  nsCOMPtr<nsIAtom> mCurPosAtom;
  nsCOMPtr<nsIAtom> mMaxPosAtom;
  nsCOMPtr<nsIAtom> mScrollbarAtom;
};
