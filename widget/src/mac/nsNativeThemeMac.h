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

#include <Appearance.h>

#include "nsITheme.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsILookAndFeel.h"
#include "nsIDeviceContext.h"
#include "nsNativeTheme.h"

class nsNativeThemeMac : private nsNativeTheme,
                         public nsITheme
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

  virtual PRBool GetWidgetPadding(nsIDeviceContext* aContext,
                                  nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  nsMargin* aResult);

  NS_IMETHOD GetMinimumWidgetSize(nsIRenderingContext* aContext, nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  nsSize* aResult, PRBool* aIsOverridable);
  NS_IMETHOD WidgetStateChanged(nsIFrame* aFrame, PRUint8 aWidgetType, 
                                nsIAtom* aAttribute, PRBool* aShouldRepaint);
  NS_IMETHOD ThemeChanged();
  PRBool ThemeSupportsWidget(nsPresContext* aPresContext, nsIFrame* aFrame, PRUint8 aWidgetType);
  PRBool WidgetIsContainer(PRUint8 aWidgetType);

protected:

    // Some widths and margins. You'd think there would be metrics for these, but no.
  enum {
    kAquaPushButtonEndcaps = 14,
    kAquaPushButtonTopBottom = 2,
    kAquaSmallPushButtonEndcaps = 10,

    kAquaDropdownLeftEndcap = 9,
    kAquaDropwdonRightEndcap = 20     // wider on right to encompass the button
  };
  
  nsresult GetSystemColor(PRUint8 aWidgetType, nsILookAndFeel::nsColorID& aColorID);
  nsresult GetSystemFont(PRUint8 aWidgetType, nsSystemFontID& aFont);


  // Appearance Manager drawing routines
  void DrawCheckbox ( const Rect& inBoxRect, PRBool inChecked, PRBool inDisabled, PRInt32 inState ) ;
  void DrawSmallCheckbox ( const Rect& inBoxRect, PRBool inChecked, PRBool inDisabled, PRInt32 inState );
  void DrawRadio ( const Rect& inBoxRect, PRBool inChecked, PRBool inDisabled, PRInt32 inState ) ;
  void DrawSmallRadio ( const Rect& inBoxRect, PRBool inChecked, PRBool inDisabled, PRInt32 inState );
  void DrawToolbar ( const Rect& inBoxRect ) ;
  void DrawEditText ( const Rect& inBoxRect, PRBool inIsDisabled ) ;
  void DrawListBox ( const Rect& inBoxRect, PRBool inIsDisabled ) ;
  void DrawProgress ( const Rect& inBoxRect, PRBool inIsDisabled, PRBool inIsIndeterminate, 
                        PRBool inIsHorizontal, PRInt32 inValue ) ;
  void DrawTab ( const Rect& inBoxRect, PRBool inIsDisabled, PRBool inIsFrontmost, 
                  PRBool inIsHorizontal, PRBool inTabBottom, PRInt32 inState ) ;
  void DrawTabPanel ( const Rect& inBoxRect, PRBool inIsDisabled ) ;
  void DrawSeparator ( const Rect& inBoxRect, PRBool inIsDisabled ) ;
//  void DrawScrollArrows ( const Rect& inScrollbarRect, PRBool inIsDisabled, PRInt32 inWidget, PRInt32 inState ) ;
  
  void DrawButton ( ThemeButtonKind inKind, const Rect& inBoxRect, PRBool inIsDefault, 
                      PRBool inDisabled, ThemeButtonValue inValue, ThemeButtonAdornment inAdornment, PRInt32 inState ) ;
  void DrawCheckboxRadio ( ThemeButtonKind inKind, const Rect& inBoxRect, PRBool inChecked, 
                              PRBool inDisabled, PRInt32 inState ) ;

private:

  ThemeEraseUPP mEraseProc;
};
