/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 * 
 * Contributors:
 */

#include "nsITheme.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include <windows.h>

class nsNativeThemeWin: public nsITheme {
public:
  NS_DECL_ISUPPORTS

  // The nsITheme interface.
  NS_IMETHOD DrawWidgetBackground(nsIRenderingContext* aContext,
                                  nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  const nsRect& aRect,
                                  const nsRect& aClipRect);

  NS_IMETHOD GetWidgetPadding(nsIDeviceContext* aContext, 
                              nsIFrame* aFrame,
                              PRUint8 aWidgetType,
                              nsMargin* aResult);

  NS_IMETHOD GetWidgetFont(nsIDeviceContext* aContext, 
                           PRUint8 aWidgetType,
                           nsFont* aFont);

  NS_IMETHOD GetWidgetColor(nsIPresContext* aPresContext,
                            nsIRenderingContext* aContext,
                            nsIFrame* aFrame,
                            PRUint8 aWidgetType,
                            nscolor* aFont);

  NS_IMETHOD GetMinimumWidgetSize(nsIRenderingContext* aContext, nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  nsSize* aResult);

  NS_IMETHOD WidgetStateChanged(nsIFrame* aFrame, PRUint8 aWidgetType, 
                                nsIAtom* aAttribute, PRBool* aShouldRepaint);

  NS_IMETHOD ThemeChanged();

  PRBool ThemeSupportsWidget(nsIPresContext* aPresContext,
                             PRUint8 aWidgetType);

  nsNativeThemeWin();
  virtual ~nsNativeThemeWin();

protected:
  void CloseData();
  HANDLE GetTheme(PRUint8 aWidgetType);
  nsresult GetThemePartAndState(nsIFrame* aFrame, PRUint8 aWidgetType,
                                PRInt32& aPart, PRInt32& aState);
  PRBool IsDisabled(nsIFrame* aFrame);
  PRBool IsChecked(nsIFrame* aFrame);
  PRBool IsSelected(nsIFrame* aFrame);

private:
  HMODULE mThemeDLL;
  HANDLE mButtonTheme;
  HANDLE mToolbarTheme;
  HANDLE mRebarTheme;
  HANDLE mScrollbarTheme;
  HANDLE mStatusbarTheme;
  HANDLE mTabTheme;
  HANDLE mTreeViewTheme;

  nsCOMPtr<nsIAtom> mCheckedAtom;
  nsCOMPtr<nsIAtom> mDisabledAtom;
  nsCOMPtr<nsIAtom> mSBOrientAtom;
  nsCOMPtr<nsIAtom> mSelectedAtom;
  nsCOMPtr<nsIAtom> mTypeAtom;
};

// Creator function
extern NS_METHOD NS_NewNativeThemeWin(nsISupports *aOuter, REFNSIID aIID, void **aResult);
