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

#ifndef nsImageButton_h___
#define nsImageButton_h___

#include "nsIImageButton.h"
#include "nsIImageRequest.h"
#include "nsIImageGroup.h"
#include "nsIRenderingContext.h"
#include "nsWindow.h"
#include "nsColor.h"
#include "nsFont.h"
#include "nsIImageObserver.h"
#include "nsIImageButtonListener.h"

#define eButtonState_up         1
#define eButtonState_rollover   2
#define eButtonState_pressed    4
#define eButtonState_disabled   8

//--------------------------------------------------------------
class nsImageButton : public ChildWindow,
                      public nsIImageButton,
                      public nsIImageRequestObserver
                   

{
public:
  nsImageButton();
  virtual ~nsImageButton();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Create(nsIWidget        *aParent,
                    const nsRect     &aRect,
                    EVENT_CALLBACK   aHandleEventFunction,
                    nsIDeviceContext *aContext,
                    nsIAppShell      *aAppShell = nsnull,
                    nsIToolkit       *aToolkit = nsnull,
                    nsWidgetInitData *aInitData = nsnull);

  NS_IMETHOD  SetBounds(const nsRect& aBounds);

  NS_IMETHOD_(nsEventStatus) OnPaint(nsIRenderingContext& aRenderingContext,
                                     const nsRect& aDirtyRect);

  NS_IMETHOD_(nsEventStatus) PaintBackground(nsIRenderingContext& aRenderingContext,
                                              const nsRect       & aDirtyRect,
                                              const nsRect       & aEntireRect,
                                              nsIImageRequest    * anImage,
                                              const nsRect       & aRect);

  NS_IMETHOD_(nsEventStatus) PaintForeground(nsIRenderingContext& aRenderingContext,
                                              const nsRect       & aDirtyRect,
                                              const nsRect       & aEntireRect,
                                              const nsString     & aLabel,
                                              const nsRect       & aRect);

  NS_IMETHOD_(nsEventStatus) PaintBorder(nsIRenderingContext& aRenderingContext,
                                         const nsRect       & aDirtyRect,
                                         const nsRect       & aEntireRect);


  NS_IMETHOD_(nsEventStatus) OnMouseEnter(nsGUIEvent *aEvent);
  NS_IMETHOD_(nsEventStatus) OnMouseExit(nsGUIEvent *aEvent);
  NS_IMETHOD_(nsEventStatus) OnMouseMove(nsGUIEvent *aEvent);
  NS_IMETHOD_(nsEventStatus) OnLeftButtonDown(nsGUIEvent *aEvent);
  NS_IMETHOD_(nsEventStatus) OnLeftButtonUp(nsGUIEvent *aEvent);

  NS_IMETHOD CreateImageGroup();
  NS_IMETHOD_(nsIImageRequest *) RequestImage(nsString aUrl);

  /**
   * Save canvas graphical state
   * @param aRenderingContext, rendering context to save state to
   * @result nsresult, NS_OK if successful
   */
  NS_IMETHOD PushState(nsIRenderingContext& aRenderingContext);

  /**
   * Get and and set RenderingContext to this graphical state
   * @param aRenderingContext, rendering context to get previously saved state from
   * @return if PR_TRUE, indicates that the clipping region after
   *         popping state is empty, else PR_FALSE
   */
  NS_IMETHOD_(PRBool) PopState(nsIRenderingContext& aRenderingContext);

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

  NS_IMETHOD_(nsEventStatus) HandleEvent(nsGUIEvent *aEvent);

  NS_IMETHOD SetImageDimensions(const PRInt32 & aWidth, const PRInt32 & aHeight);
  NS_IMETHOD GetImageDimensions(PRInt32 & aWidth, PRInt32 & aHeight);
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

  // nsIImageRequestObserver
  virtual void Notify(nsIImageRequest *aImageRequest,
                      nsIImage *aImage,
                      nsImageNotification aNotificationType,
                      PRInt32 aParam1, PRInt32 aParam2,
                      void *aParam3);

  virtual void NotifyError(nsIImageRequest *aImageRequest,
                           nsImageError aErrorType);
  // nsWindow
  NS_IMETHOD Enable(PRBool aState);

protected:
  nsIImageRequest * GetImageForPainting();

  void PerformAlignment(const nsRect & aRect, 
                        const nsIImageRequest * anImage, nsRect & aImgRect, 
                        const nsString & aText, nsRect & aTxtRect,
                        PRInt32 &aWhichAlignment, PRInt32 &aMaxWidth, PRInt32 &aMaxHeight);

  nsIImageRequest* mUpImageRequest;
  nsIImageRequest* mPressedImageRequest;
  nsIImageRequest* mDisabledImageRequest;
  nsIImageRequest* mRollOverImageRequest;

  PRUint32 mState;

  nsIImageRequest*     mImageRequest;
  nsIImageGroup*       mImageGroup;
  nsIRenderingContext *mRenderingContext;

  nsButtonVerticalAligment   mTextVAlignment;
  nsButtonHorizontalAligment mTextHAlignment;

  nsButtonVerticalAligment   mImageVAlignment;
  nsButtonHorizontalAligment mImageHAlignment;

  PRBool           mShowImage;
  PRBool           mShowText;

  nscolor         mBackgroundColor;
  nscolor         mForegroundColor;
  nscolor         mHighlightColor;
  nscolor         mShadowColor;
  nsFont          mFont;
  nsString        mLabel;
  nsString        mRollOverDesc;
  PRInt32         mCommand;

  PRInt32         mTextGap;
  PRUint32        mImageWidth;
  PRUint32        mImageHeight;
  PRBool          mShowBorder;        // If true border show on mouse over
  PRBool          mShowButtonBorder;  // controls just the black rect around the button
  PRBool          mAlwaysShowBorder;  // controls whether up/dwn border is always drawn
  PRInt32         mBorderWidth;
  PRInt32         mBorderOffset;

  // Temporary
  // XXX will be switching to a nsDeque
  nsIImageButtonListener * mListeners[32];
  PRInt32 mNumListeners;

};

#endif /* nsImageButton_h___ */
