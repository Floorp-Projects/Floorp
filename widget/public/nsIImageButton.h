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

#ifndef nsIImageButton_h___
#define nsIImageButton_h___

#include "nsISupports.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsColor.h"

class nsIImageButtonListener;

//f58c2550-4a7c-11d2-bee2-00805f8a8dbd
#define NS_IIMAGEBUTTON_IID      \
 { 0xf58c2550, 0x4a7c, 0x11d2, \
   {0xbe, 0xe2, 0x00, 0x80, 0x5f, 0x8a, 0x8d, 0xbd} }


  typedef enum  {   
    eButtonVerticalAligment_Top,
    eButtonVerticalAligment_Center,
    eButtonVerticalAligment_Bottom
  } nsButtonVerticalAligment; 

  typedef enum  {   
    eButtonHorizontalAligment_Left,
    eButtonHorizontalAligment_Middle,
    eButtonHorizontalAligment_Right
  } nsButtonHorizontalAligment; 

//---------------------------------------------------------------------------
class nsIImageButton : public nsISupports
{

public:

 /**
  * Sets the label on the Image Button
  *
  */
  NS_IMETHOD SetLabel(const nsString& aString) = 0;

 /**
  * Gets the label on the Image Button
  *
  */
  NS_IMETHOD GetLabel(nsString& aString) = 0;

 /**
  * Gets the command for the Image Button
  *
  */
  NS_IMETHOD GetCommand(PRInt32 &aCommand) = 0;

 /**
  * Sets the command for the Image Button
  *
  */
  NS_IMETHOD SetCommand(PRInt32 aCommand) = 0;

 /**
  * Sets the description that is available when the mouse enters the control
  *
  */
  NS_IMETHOD SetRollOverDesc(const nsString& aString) = 0;

 /**
  * Gets the description that is available when the mouse enters the control
  *
  */
  NS_IMETHOD GetRollOverDesc(nsString& aString) = 0;

 /**
  * Sets the border highlight color
  *
  */
  NS_IMETHOD SetHighlightColor(const nscolor &aColor) = 0;

 /**
  * Gets the border highlight color
  *
  */
  NS_IMETHOD GetHighlightColor(nscolor &aColor) = 0;

 /**
  * Gets the Shadow highlight color
  *
  */
  NS_IMETHOD GetShadowColor(nscolor &aColor) = 0;

 /**
  * Sets the Shadow highlight color
  *
  */
  NS_IMETHOD SetShadowColor(const nscolor &aColor) = 0;

 /**
  * Sets the Dimensions of the Image in the button
  * (The dimensions should come from the image)
  *
  */
  NS_IMETHOD SetImageDimensions(const PRInt32 & aWidth, const PRInt32 & aHeight) = 0;

 /**
  * Sets the URLs for the Buttons images
  *
  */
  NS_IMETHOD SetImageURLs(const nsString& aUpURL,
                          const nsString& aPressedURL,
                          const nsString& aDisabledURL,
                          const nsString& aRollOverURL) = 0;
 /**
  * Sets the width of the border
  *
  */
  NS_IMETHOD SetBorderWidth(PRInt32 aWidth) = 0;

 /**
  * Sets the width of the space between the border and the text or image
  *
  */
  NS_IMETHOD SetBorderOffset(PRInt32 aWidth) = 0;

 /**
  * Sets whether the Border show be shown
  *
  */
  NS_IMETHOD SetShowBorder(PRBool aState) = 0;

 /**
  * Sets whether the Border show be shown
  *
  */
  NS_IMETHOD SetShowButtonBorder(PRBool aState) = 0;

 /**
  * Sets whether the text should be shown
  *
  */
  NS_IMETHOD SetShowText(PRBool aState) = 0;

 /**
  * Sets whether the image should be shown
  *
  */
  NS_IMETHOD SetShowImage(PRBool aState) = 0;

 /**
  * Sets the image's vertical alignment
  *
  */
  NS_IMETHOD SetImageVerticalAlignment(nsButtonVerticalAligment aAlign) = 0;

 /**
  * Sets the image's horizontal alignment
  *
  */
  NS_IMETHOD SetImageHorizontalAlignment(nsButtonHorizontalAligment aAlign) = 0;

 /**
  * Sets the text's vertical alignment
  *
  */
  NS_IMETHOD SetTextVerticalAlignment(nsButtonVerticalAligment aAlign) = 0;

 /**
  * Sets the text's horizontal alignment
  *
  */
  NS_IMETHOD SetTextHorizontalAlignment(nsButtonHorizontalAligment aAlign) = 0;

 /**
  * Show border at all times
  *
  */
  NS_IMETHOD SetAlwaysShowBorder(PRBool aState) = 0;

 /**
  * Show border at all times
  *
  */
  NS_IMETHOD SwapHighlightShadowColors() = 0;

 /**
  * Add Image Button Listener
  *
  */
  NS_IMETHOD AddListener(nsIImageButtonListener * aListener) = 0;

 /**
  * Removes Image Button Listener
  *
  */
  NS_IMETHOD RemoveListener(nsIImageButtonListener * aListener) = 0;

 /**
  * HandleGUI Events
  *
  */
  NS_IMETHOD_(nsEventStatus) HandleEvent(nsGUIEvent *aEvent) = 0;

 /**
  * Handle OnPaint
  *
  */
  NS_IMETHOD_(nsEventStatus) OnPaint(nsIRenderingContext& aRenderingContext,
                                     const nsRect& aDirtyRect) = 0;

};

#endif /* nsIImageButton_h___ */

