/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsMenuButton_h___
#define nsMenuButton_h___

#include "nsIPopUpMenu.h"
#include "nsIMenuButton.h"

#include "nsImageButton.h"

#include "nsIRenderingContext.h"
#include "nsColor.h"
#include "nsFont.h"
#include "nsGUIEvent.h"
#include "nsIImageButtonListener.h"


//--------------------------------------------------------------
class nsMenuButton : public nsIMenuButton,
                     public nsImageButton
{
public:
  nsMenuButton();
  virtual ~nsMenuButton();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetPopUpMenu(nsIPopUpMenu *& aPopUpMenu);
  NS_IMETHOD AddMenuItem(const nsString& aMenuLabel, PRInt32 aCommand);
  NS_IMETHOD InsertMenuItem(const nsString& aMenuLabel, PRInt32 aCommand, PRInt32 aPos);
  NS_IMETHOD RemovesMenuItem(PRInt32 aPos);
  NS_IMETHOD RemoveAllMenuItems(PRInt32 aPos);

  NS_IMETHOD_(nsEventStatus) OnLeftButtonDown(nsGUIEvent *aEvent);
  NS_IMETHOD_(nsEventStatus) PaintBorder(nsIRenderingContext& aRenderingContext,
                                         const nsRect       & aDirtyRect,
                                         const nsRect       & aEntireRect);

  NS_IMETHOD  SetBounds(const nsRect& aBounds);

  nsEventStatus OnPaint(nsIRenderingContext& aRenderingContext,
                                     const nsRect& aDirtyRect);

  NS_IMETHOD_(nsEventStatus) OnMouseEnter(nsGUIEvent *aEvent);
  NS_IMETHOD_(nsEventStatus) OnMouseExit(nsGUIEvent *aEvent);
  NS_IMETHOD_(nsEventStatus) OnMouseMove(nsGUIEvent *aEvent);
  NS_IMETHOD_(nsEventStatus) OnLeftButtonUp(nsGUIEvent *aEvent);

  NS_IMETHOD           GetLabel(nsString&);
  NS_IMETHOD           SetLabel(const nsString& aString);
  NS_IMETHOD           GetRollOverDesc(nsString& aString);
  NS_IMETHOD           SetRollOverDesc(const nsString& aString);
  NS_IMETHOD           GetCommand(PRInt32 & aCommand);
  NS_IMETHOD           SetCommand(PRInt32 aCommand);

  NS_IMETHOD           GetHighlightColor(nscolor &aColor);
  NS_IMETHOD           SetHighlightColor(const nscolor &aColor);

  NS_IMETHOD           GetShadowColor(nscolor &aColor);
  NS_IMETHOD           SetShadowColor(const nscolor &aColor);

  virtual nsEventStatus HandleEvent(nsGUIEvent *aEvent);

  NS_IMETHOD SetImageDimensions(const PRInt32 & aWidth, const PRInt32 & aHeight);
  NS_IMETHOD SetImageURLs(const nsString& aUpURL,
                          const nsString& aPressedURL,
                          const nsString& aDisabledURL,
                          const nsString& aRollOverURL);

  NS_IMETHOD SetShowBorder(PRBool aState);
  NS_IMETHOD SetShowButtonBorder(PRBool aState);
  NS_IMETHOD SetBorderWidth(PRInt32 aWidth);
  NS_IMETHOD SetBorderOffset(PRInt32 aWidth);
  NS_IMETHOD SetShowText(PRBool aState);
  NS_IMETHOD SetShowImage(PRBool aState);
  NS_IMETHOD SetAlwaysShowBorder(PRBool aState);
  NS_IMETHOD SwapHighlightShadowColors();

  // Alignment Methods
  NS_IMETHOD SetImageVerticalAlignment(nsButtonVerticalAligment aAlign);
  NS_IMETHOD SetImageHorizontalAlignment(nsButtonHorizontalAligment aAlign);
  NS_IMETHOD SetTextVerticalAlignment(nsButtonVerticalAligment aAlign);
  NS_IMETHOD SetTextHorizontalAlignment(nsButtonHorizontalAligment aAlign);

  NS_IMETHOD GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight);
  NS_IMETHOD SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight);

  NS_IMETHOD AddListener(nsIImageButtonListener * aListener);
  NS_IMETHOD RemoveListener(nsIImageButtonListener * aListener);


  // nsWindow
  NS_IMETHOD Enable(PRBool aState);

protected:
  void CreatePopUpMenu();

  nsIPopUpMenu * mPopUpMenu;
  PRBool         mMenuIsPoppedUp;

};

#endif /* nsMenuButton_h___ */
