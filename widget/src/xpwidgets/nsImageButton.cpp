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

#include "nsImageButton.h"
#include "nsWidgetsCID.h" 
#include "nspr.h"
#include "nsIButton.h"
#include "nsIFontMetrics.h"
#include "nsIDeviceContext.h"
#include "nsWidgetsCID.h"

#include "nsIAppShell.h"
#include "nsIWidget.h"
#include "nsITextWidget.h"
#include "nsIButton.h"
#include "nsIImageGroup.h"
#include "nsITimer.h"
#include "nsIThrobber.h"

static NS_DEFINE_IID(kISupportsIID,      NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCImageButtonCID,   NS_IMAGEBUTTON_CID);
static NS_DEFINE_IID(kCIImageButtonIID,  NS_IIMAGEBUTTON_IID);
//static NS_DEFINE_IID(kCButtonCID,        NS_BUTTON_CID);
//static NS_DEFINE_IID(kIButtonIID,      NS_IBUTTON_IID);
static NS_DEFINE_IID(kIImageObserverIID, NS_IIMAGEREQUESTOBSERVER_IID);

#define DEFAULT_WIDTH  50
#define DEFAULT_HEIGHT 50

NS_IMPL_ADDREF(nsImageButton)
NS_IMPL_RELEASE(nsImageButton)

//typedef enum {eAlignJustText, eAlignJustImage, eAlignBoth} eWhichAlignment;
const PRInt32 kAlignJustText  = 0;
const PRInt32 kAlignJustImage = 1;
const PRInt32 kAlignBoth      = 2;


//---------------------------------------------------------------
static nsEventStatus PR_CALLBACK
HandleImageButtonEvent(nsGUIEvent *aEvent)
{
  nsEventStatus    result = nsEventStatus_eIgnore;
  nsIImageButton * button;
	if (NS_OK == aEvent->widget->QueryInterface(kCIImageButtonIID,(void**)&button)) {
    result = button->HandleEvent(aEvent);
    NS_RELEASE(button);
  }
  return result;
}


//------------------------------------------------------------
nsImageButton::nsImageButton() : ChildWindow(), nsIImageButton(),
#ifdef XP_MAC		//temporary: we should use CSS for platform-specific rendering
                      mFont("geneva", 
                         NS_FONT_STYLE_NORMAL,
                         NS_FONT_VARIANT_NORMAL,
                         NS_FONT_WEIGHT_NORMAL,
                         0,
                         9)
#else
                      mFont("Sans", 
                         NS_FONT_STYLE_NORMAL,
                         NS_FONT_VARIANT_NORMAL,
                         NS_FONT_WEIGHT_NORMAL,
                         0,
                         13)
#endif
{
  NS_INIT_REFCNT();

  mUpImageRequest       = nsnull;
  mPressedImageRequest  = nsnull;
  mDisabledImageRequest = nsnull;
  mRollOverImageRequest = nsnull;

  mState = eButtonState_up;

  // From Canvas
  mBackgroundColor = NS_RGB(192,192,192);
  mForegroundColor = NS_RGB(0,0,0);

  mHighlightColor   = NS_RGB(255,255,255);
  mShadowColor      = NS_RGB(128,128,128);
  mShowButtonBorder = PR_FALSE;

  // XXX: We should probably auto-generate unique names here
  mLabel  = "Default Label";

  mImageGroup = nsnull;
  mImageRequest    = nsnull;
  mImageWidth      = 32;
  mImageHeight     = 32;

  mShowBorder      = PR_TRUE;

  mTextVAlignment  = eButtonVerticalAligment_Bottom;
  mTextHAlignment  = eButtonHorizontalAligment_Middle;

  mImageVAlignment = eButtonVerticalAligment_Top;
  mImageHAlignment = eButtonHorizontalAligment_Middle;

  mShowImage        = PR_TRUE;
  mShowText         = PR_TRUE;
  mAlwaysShowBorder = PR_FALSE;
  mBorderWidth      = 1;
  mBorderOffset     = 2;
  mTextGap          = 2;
  mCommand          = 0;

  mLabel.SetLength(0);
  mLabel.Append("Default");

  mNumListeners = 0;
}

//------------------------------------------------------------
nsImageButton::~nsImageButton()
{
  // XXX will be switching to a nsDeque
  PRInt32 i;
  for (i=0;i<mNumListeners;i++) {
    NS_RELEASE(mListeners[i]);
  }
}

//------------------------------------------------------------
NS_METHOD nsImageButton::Create(nsIWidget *aParent,
                                const nsRect &aRect,
                                EVENT_CALLBACK aHandleEventFunction,
                                nsIDeviceContext *aContext,
                                nsIAppShell *aAppShell,
                                nsIToolkit *aToolkit,
                                nsWidgetInitData *aInitData)
{
  EVENT_CALLBACK ec = (aHandleEventFunction != nsnull) ? 
    aHandleEventFunction : HandleImageButtonEvent;
  return ChildWindow::Create(aParent, aRect, ec,
     aContext, aAppShell, aToolkit, aInitData);
}


//------------------------------------------------------------
nsresult nsImageButton::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kCImageButtonCID);                         

  if (aIID.Equals(kCIImageButtonIID)) {                                          
    *aInstancePtr = (void*) (nsIImageButton *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kIImageObserverIID)) {
    *aInstancePtr = (void*)(nsIImageRequestObserver*)this;
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) (nsImageButton *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      

  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  return (nsWindow::QueryInterface(aIID, aInstancePtr));
}

//------------------------------------------------------------
nsresult nsImageButton::SetBounds(const nsRect &aBounds)
{
  nsWindow::Invalidate(PR_FALSE);
  return nsWindow::Resize(aBounds.x, aBounds.y, aBounds.width, aBounds.height, PR_FALSE);
}

//------------------------------------------------------------
nsIImageRequest * nsImageButton::GetImageForPainting()
{
  nsIImageRequest * img = nsnull;

  if ((mState & eButtonState_pressed) && (mPressedImageRequest != nsnull))
    img = mPressedImageRequest;

  else if ((mState & eButtonState_rollover) && (mRollOverImageRequest != nsnull))
    img = mRollOverImageRequest;

  else if ((mState & eButtonState_disabled) && (mDisabledImageRequest != nsnull))
    img = mDisabledImageRequest;

  else if (mUpImageRequest != nsnull) 
    img = mUpImageRequest;

  return img;
}

//------------------------------------------------------------
nsEventStatus nsImageButton::OnPaint(nsIRenderingContext& aRenderingContext,
                                     const nsRect       & aDirtyRect)
{
  PushState(aRenderingContext);
  nsRect   bounds;
  nsString label;

  GetLabel(label);
  GetBounds(bounds);
  bounds.x = 0;
  bounds.y = 0;

  nsRect imgRect;
  nsRect txtRect;
  PRInt32 which;
  PRInt32 width;
  PRInt32 height;

  nsIImageRequest * img = GetImageForPainting();

  PerformAlignment(bounds, img, imgRect, label, txtRect, which, width, height);

  PaintBackground(aRenderingContext, aDirtyRect, bounds, img,   imgRect);
  PaintForeground(aRenderingContext, aDirtyRect, bounds, label, txtRect);

  if (mShowBorder || mAlwaysShowBorder) {
    PaintBorder(aRenderingContext, aDirtyRect, bounds);
  }
  PopState(aRenderingContext);

  return nsEventStatus_eConsumeNoDefault;  
}

//-----------------------------------------------------------------------------
nsEventStatus nsImageButton::PaintBackground(nsIRenderingContext& aRenderingContext,
                                             const nsRect       & aDirtyRect,
                                             const nsRect       & aEntireRect,
                                             nsIImageRequest    * anImage,
                                             const nsRect       & aRect)
{
  aRenderingContext.SetColor(GetBackgroundColor());
  aRenderingContext.FillRect(aEntireRect);

  if (!mShowImage || anImage == nsnull) {
    return nsEventStatus_eIgnore;
  }

  nsIImage * img = anImage->GetImage();

  if (nsnull != img) {
    aRenderingContext.DrawImage(img, aRect.x, aRect.y);
  }

  NS_IF_RELEASE(img);

  return nsEventStatus_eIgnore;  
}

//-----------------------------------------------------------------------------
nsEventStatus nsImageButton::PaintForeground(nsIRenderingContext& aRenderingContext,
                                             const nsRect       & aDirtyRect,
                                             const nsRect       & aEntireRect,
                                             const nsString     & aLabel,
                                             const nsRect       & aRect)
{
  if (!mShowText) {
    return nsEventStatus_eConsumeNoDefault;
  }

  //if (mShowText == eShowText_none)
  //  return nsEventStatus_eConsumeNoDefault;  

  // XXX: We really need to query system-wide colors via gfx system manager here?
  //      On windows, the calls are:
  //
  // GetSysColor(COLOR_BTNFACE), GetSysColor(COLOR_GRAYTEXT), GetSysColor(COLOR_GRAYTEXT)

  // Draw text based on justifications

  if ((mState & eButtonState_rollover))
    aRenderingContext.SetColor(NS_RGB(0,0,255));

  else if (mState & eButtonState_disabled)
    aRenderingContext.SetColor(NS_RGB(255, 255, 255));

  else
    aRenderingContext.SetColor(GetForegroundColor());

  if (mState & eButtonState_disabled) { 
    aRenderingContext.DrawString(aLabel, aLabel.Length(), aRect.x+1, aRect.y+1);
    aRenderingContext.SetColor(NS_RGB(128, 128, 128));
  }
  aRenderingContext.DrawString(aLabel, aLabel.Length(), aRect.x, aRect.y);


  return nsEventStatus_eConsumeNoDefault;  
}

//-----------------------------------------------------------------------------
nsEventStatus nsImageButton::OnMouseMove(nsGUIEvent *aEvent)
{ 
  return nsEventStatus_eIgnore;
}

//-----------------------------------------------------------------------------
nsEventStatus nsImageButton::OnMouseEnter(nsGUIEvent *aEvent)
{  
  SetCursor(eCursor_standard);

  mState |= eButtonState_rollover;

  Invalidate(PR_FALSE);
  return nsEventStatus_eIgnore;
}

//-----------------------------------------------------------------------------
nsEventStatus nsImageButton::OnMouseExit(nsGUIEvent *aEvent)
{  
  mState &= ~eButtonState_rollover;
  mState &= ~eButtonState_pressed;
  Invalidate(PR_FALSE);
  return nsEventStatus_eIgnore;//(nsWindow::OnMouseExit(aEvent));
}

//-----------------------------------------------------------------------------
nsEventStatus nsImageButton::OnLeftButtonDown(nsGUIEvent *aEvent)
{
  mState |= eButtonState_pressed;
  Invalidate(PR_FALSE);
  return nsEventStatus_eIgnore;
}

//-----------------------------------------------------------------------------
nsEventStatus nsImageButton::OnLeftButtonUp(nsGUIEvent *aEvent)
{
  mState &= ~eButtonState_pressed;
  Invalidate(PR_FALSE);
  return nsEventStatus_eIgnore;
}

//-----------------------------------------------------------------------------
nsresult nsImageButton::CreateImageGroup()
{
  nsresult res = NS_OK;

  if (mImageGroup != nsnull)
    return NS_OK;

  res = NS_NewImageGroup(&mImageGroup);

  if (NS_OK == res) 
  {
    
    nsIDeviceContext * deviceCtx = GetDeviceContext();

    mImageGroup->Init(deviceCtx, nsnull);

    NS_RELEASE(deviceCtx);
  }

  return res;
}


//-----------------------------------------------------------------------------
nsIImageRequest * nsImageButton::RequestImage(nsString aUrl)
{
  if (aUrl.Length() == 0) {
    return nsnull;
  }
  char * url = aUrl.ToNewCString();

  nsIImageRequest * request ;

  nscolor bgcolor = GetBackgroundColor();

  nsRect rect;
  GetBounds(rect);

  request = mImageGroup->GetImage(url,
                                 (nsIImageRequestObserver *)this,
                                 &bgcolor,
                                 mImageWidth,
                                 mImageHeight, 
                                 0);//nsImageLoadFlags_kSticky);

  //request->GetNaturalDimensions(&mImageWidth, &mImageHeight);

  delete[] url;

  return request;
}

//-----------------------------------------------------------------------------
nsresult nsImageButton::PushState(nsIRenderingContext& aRenderingContext)
{
  aRenderingContext.PushState();

  aRenderingContext.SetFont(mFont);

  return NS_OK;
}

//-----------------------------------------------------------------------------
PRBool nsImageButton::PopState(nsIRenderingContext& aRenderingContext)
{
  PRBool clipState;
  aRenderingContext.PopState(clipState);
  return clipState;
}

//-----------------------------------------------------------------------------
NS_METHOD nsImageButton::GetLabel(nsString & aString) 
{
  aString = mLabel;
  return NS_OK;
}

//-----------------------------------------------------------------------------
NS_METHOD nsImageButton::SetLabel(const nsString& aString)
{
  mLabel = aString;
  return NS_OK;
}

//-----------------------------------------------------------------------------
NS_METHOD nsImageButton::GetRollOverDesc(nsString & aString) 
{
  aString = mRollOverDesc;
  return NS_OK;
}

//-----------------------------------------------------------------------------
NS_METHOD nsImageButton::SetRollOverDesc(const nsString& aString)
{
  mRollOverDesc = aString;
  return NS_OK;
}

//-----------------------------------------------------------------------------
NS_METHOD nsImageButton::GetCommand(PRInt32 & aCommand) 
{
  aCommand = mCommand;
  return NS_OK;
}

//-----------------------------------------------------------------------------
NS_METHOD nsImageButton::SetCommand(PRInt32 aCommand) 
{
  mCommand = aCommand;
  return NS_OK;
}

//-----------------------------------------------------------------------------
NS_METHOD nsImageButton::GetHighlightColor(nscolor &aColor)
{
  aColor = mHighlightColor;
  return NS_OK;
}

//-----------------------------------------------------------------------------
NS_METHOD nsImageButton::SetHighlightColor(const nscolor &aColor)
{
  mHighlightColor = aColor;
  return NS_OK;
}

//-----------------------------------------------------------------------------
NS_METHOD nsImageButton::GetShadowColor(nscolor &aColor)
{
  aColor = mShadowColor;
  return NS_OK;
}

//-----------------------------------------------------------------------------
NS_METHOD nsImageButton::SetShadowColor(const nscolor &aColor)
{
  mShadowColor = aColor;
  return NS_OK;
}


//-----------------------------------------------------------------------------
nsEventStatus nsImageButton::PaintBorder(nsIRenderingContext& aRenderingContext,
                                         const nsRect& aDirtyRect,
                                         const nsRect& aEntireRect)
{
  nsRect rect(aEntireRect);
  if ((mState == eButtonState_up || (mState & eButtonState_disabled)) && !mAlwaysShowBorder) {
    return nsEventStatus_eConsumeNoDefault;
  }

  rect.x = 0;
  rect.y = 0;
  rect.width--; 
  rect.height--;

  if (mShowButtonBorder) {
    aRenderingContext.SetColor(NS_RGB(0,0,0));
    aRenderingContext.DrawRect(0,0, rect.width, rect.height);
    rect.x++; 
    rect.y++;
    rect.width  -= 2; 
    rect.height -= 2;
  }

  if (mState & eButtonState_pressed) {
    aRenderingContext.SetColor(mHighlightColor);
  } else {
    aRenderingContext.SetColor(mShadowColor);
  }
  aRenderingContext.DrawLine(rect.x, rect.height, rect.width, rect.height);
  aRenderingContext.DrawLine(rect.width, rect.y, rect.width, rect.height);

  if (mState & eButtonState_pressed) {
    aRenderingContext.SetColor(mShadowColor);
  } else {
    aRenderingContext.SetColor(mHighlightColor);
  }

  aRenderingContext.DrawLine(rect.x,rect.y, rect.width,rect.y);
  aRenderingContext.DrawLine(rect.x,rect.y, rect.x, rect.height);


  return nsEventStatus_eConsumeNoDefault;  
}

//-----------------------------------------------------------------------------
nsEventStatus nsImageButton::HandleEvent(nsGUIEvent *aEvent) 
{

  if (aEvent->message == NS_PAINT || aEvent->message == NS_SIZE)
  {
    switch(aEvent->message) 
    {
      case NS_PAINT:
      {

          nsEventStatus es ;
          nsRect rect;
          nsDrawingSurface ds = nsnull;
          nsIRenderingContext * ctx = ((nsPaintEvent*)aEvent)->renderingContext;

          /*nsRect widgetRect;
          GetBounds(widgetRect);
          widgetRect.x = 0;
          widgetRect.y = 0;
          ctx->SetClipRect(widgetRect, nsClipCombine_kReplace);
          */
          rect.x = ((nsPaintEvent *)aEvent)->rect->x;
          rect.y = ((nsPaintEvent *)aEvent)->rect->y;
          rect.width = ((nsPaintEvent *)aEvent)->rect->width;
          rect.height = ((nsPaintEvent *)aEvent)->rect->height;
          aEvent->widget->GetBounds(rect);
          rect.x = 0;
          rect.y = 0;
          ctx->CreateDrawingSurface(&rect, 0, ds);
          if (ds == nsnull) {
            return nsEventStatus_eConsumeNoDefault;
          }
          ctx->SelectOffScreenDrawingSurface(ds);

          es = OnPaint((*((nsPaintEvent*)aEvent)->renderingContext),(*((nsPaintEvent*)aEvent)->rect));

          ctx->CopyOffScreenBits(ds, 0, 0, rect, NS_COPYBITS_USE_SOURCE_CLIP_REGION);
          ctx->DestroyDrawingSurface(ds);

          return es;
      }
      break;

      /*case NS_SIZE: {
        nscoord x = ((nsSizeEvent*)aEvent)->windowSize->x;
        nscoord y = ((nsSizeEvent*)aEvent)->windowSize->y;
        nscoord w = ((nsSizeEvent*)aEvent)->windowSize->width;
        nscoord h = ((nsSizeEvent*)aEvent)->windowSize->height;

        OnResize(x,y,w,h);
      }
      break;*/

    }
    //PRInt32 i = 0;
    //for (i=0;i<mNumListeners;i++) {
    //  mListeners->NotifyImageButtonEvent(aEvent);
    //}
    return nsEventStatus_eIgnore;
  }

  if (aEvent->message == NS_DESTROY)
    return nsEventStatus_eIgnore;

  /*
   * XXX: Make this a HandleInputEvent...
   */

    switch(aEvent->message) {

      case NS_MOUSE_LEFT_BUTTON_DOWN:
        OnLeftButtonDown(aEvent);
        break;

      case NS_MOUSE_LEFT_BUTTON_UP:
        OnLeftButtonUp(aEvent);
        break;

      case NS_MOUSE_RIGHT_BUTTON_DOWN:
        OnLeftButtonDown(aEvent);
        break;

      case NS_MOUSE_RIGHT_BUTTON_UP:
        OnLeftButtonUp(aEvent);
        break;

      case NS_MOUSE_MOVE:
        OnMouseMove(aEvent);
        break;

      case NS_MOUSE_ENTER:
        OnMouseEnter(aEvent);
        break;
      
      case NS_MOUSE_EXIT:
        OnMouseExit(aEvent);
        break;

      case NS_KEY_UP:
        //OnKeyUp(aEvent);
        break;

      case NS_KEY_DOWN:
        //OnKeyDown(aEvent);
        break;

    } // switch


  PRInt32 i = 0;
  for (i=0;i<mNumListeners;i++) {
    mListeners[i]->NotifyImageButtonEvent(this, aEvent);
  }
  return nsEventStatus_eIgnore;
  
}

//----------------------------------------------------------------
void nsImageButton::Notify(nsIImageRequest *aImageRequest,
                          nsIImage *aImage,
                          nsImageNotification aNotificationType,
                          PRInt32 aParam1, PRInt32 aParam2,
                          void *aParam3)
{
  if (aNotificationType == nsImageNotification_kImageComplete) {
    PRUint32 w,h;
    aImageRequest->GetNaturalDimensions(&w, &h);
    if (w > 0 && h > 0) {
      aImageRequest->GetNaturalDimensions(&mImageWidth, &mImageHeight);
    }

    Invalidate(PR_FALSE);
  }
  return ;
}

//----------------------------------------------------------------
void nsImageButton::NotifyError(nsIImageRequest *aImageRequest,
                               nsImageError aErrorType)
{
  return ;
}

//----------------------------------------------------------------
NS_METHOD nsImageButton::SetImageDimensions(const PRInt32 & aWidth, const PRInt32 & aHeight)
{
  mImageWidth  = aWidth;
  mImageHeight = aHeight;
  return NS_OK;
}

//----------------------------------------------------------------
NS_METHOD nsImageButton::GetImageDimensions(PRInt32 & aWidth, PRInt32 & aHeight)
{
  aWidth = mImageWidth ;
  aHeight = mImageHeight ;
  return NS_OK;
}

//----------------------------------------------------------------
NS_METHOD nsImageButton::SetImageURLs(const nsString& aUpURL,
                                      const nsString& aPressedURL,
                                      const nsString& aDisabledURL,
                                      const nsString& aRollOverURL)
{
  CreateImageGroup();

  mUpImageRequest       = RequestImage(aUpURL);
  mPressedImageRequest  = RequestImage(aPressedURL);
  mDisabledImageRequest = RequestImage(aDisabledURL);
  mRollOverImageRequest = RequestImage(aRollOverURL);
  return NS_OK;
}

//----------------------------------------------------------------
NS_METHOD nsImageButton::SetImageUpURL(const nsString& aUpURL)
{
  CreateImageGroup();
  mUpImageRequest = RequestImage(aUpURL);
  return NS_OK;
}

//----------------------------------------------------------------
NS_METHOD nsImageButton::SetImagePressedURL(const nsString& aPressedURL)
{
  CreateImageGroup();
  mPressedImageRequest = RequestImage(aPressedURL);
  return NS_OK;
}

//----------------------------------------------------------------
NS_METHOD nsImageButton::SetImageDisabledURL(const nsString& aDisabledURL)
{
  CreateImageGroup();
  mDisabledImageRequest = RequestImage(aDisabledURL);
  return NS_OK;
}

//----------------------------------------------------------------
NS_METHOD nsImageButton::SetImageRollOverURL(const nsString& aRollOverURL)
{
  CreateImageGroup();
  mRollOverImageRequest = RequestImage(aRollOverURL);
  return NS_OK;
}

//----------------------------------------------------------------
NS_METHOD nsImageButton::SetShowBorder(PRBool aState)
{
  mShowBorder = aState;
  return NS_OK;
}

//----------------------------------------------------------------
NS_METHOD nsImageButton::SetAlwaysShowBorder(PRBool aState)
{
  mAlwaysShowBorder = aState;
  return NS_OK;
}

//----------------------------------------------------------------
NS_METHOD nsImageButton::SwapHighlightShadowColors()
{
  nscolor highColor;
  nscolor shadeColor;

  GetHighlightColor(highColor);
  GetShadowColor(shadeColor);

  SetHighlightColor(shadeColor);
  SetShadowColor(highColor);

  return NS_OK;
}

//----------------------------------------------------------------
NS_METHOD nsImageButton::SetShowButtonBorder(PRBool aState)
{
  mShowButtonBorder = aState;
  return NS_OK;
}

//----------------------------------------------------------------
NS_METHOD nsImageButton::SetBorderWidth(PRInt32 aWidth)
{
  mBorderWidth = aWidth;
  return NS_OK;
}

//----------------------------------------------------------------
NS_METHOD nsImageButton::SetBorderOffset(PRInt32 aOffset)
{
  mBorderOffset = aOffset;
  return NS_OK;
}

//----------------------------------------------------------------
NS_METHOD nsImageButton::SetShowText(PRBool aState)
{
  mShowText = aState;
  return NS_OK;
}

//----------------------------------------------------------------
NS_METHOD nsImageButton::SetShowImage(PRBool aState)
{
  mShowImage = aState;
  return NS_OK;
}

//----------------------------------------------------------------
NS_METHOD nsImageButton::SetImageVerticalAlignment(nsButtonVerticalAligment aAlign)
{
  mImageVAlignment = aAlign;
  return NS_OK;
}

//----------------------------------------------------------------
NS_METHOD nsImageButton::SetImageHorizontalAlignment(nsButtonHorizontalAligment aAlign)
{
  mImageHAlignment = aAlign;
  return NS_OK;
}

//----------------------------------------------------------------
NS_METHOD nsImageButton::SetTextVerticalAlignment(nsButtonVerticalAligment aAlign)
{
  mTextVAlignment = aAlign;
  return NS_OK;
}

//----------------------------------------------------------------
NS_METHOD nsImageButton::SetTextHorizontalAlignment(nsButtonHorizontalAligment aAlign)
{
  mTextHAlignment = aAlign;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsImageButton::AddListener(nsIImageButtonListener * aListener)
{
  // XXX will be switching to a nsDeque
  mListeners[mNumListeners++] = aListener;
  NS_ADDREF(aListener);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsImageButton::RemoveListener(nsIImageButtonListener * aListener)
{
  // XXX will be switching to a nsDeque
  PRInt32 i;
  for (i=0;i<mNumListeners;i++) {
    if (aListener == mListeners[i]) {
      // XXX remove it from list here
      NS_RELEASE(aListener);
      break;
    }
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsImageButton::GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight)
{
  if (mPreferredWidth != 0  || mPreferredHeight != 0) {
    aWidth  = mPreferredWidth;
    aHeight = mPreferredHeight;
  } else {
    nsRect imgRect;
    nsRect txtRect;
    nsRect rect;
    PRInt32 which;
 
    GetBounds(rect);
    PerformAlignment(rect, mUpImageRequest, imgRect, mLabel, txtRect, which, aWidth, aHeight);
    aWidth  += (mBorderWidth*2)+(mBorderOffset*2);
    aHeight += (mBorderWidth*2)+(mBorderOffset*2);
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsImageButton::SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight)
{
  mPreferredWidth  = aWidth;
  mPreferredHeight = aHeight;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsImageButton::Enable(PRBool aState)
{
  nsresult status = nsWindow::Enable(aState);

  if (aState) {
    mState &= ~eButtonState_disabled;
  } else {
    mState |= eButtonState_disabled;
  }

  return status;

}

//------------------------------------------------------------------------- 
void nsImageButton::PerformAlignment(const nsRect & aRect, 
                                     const nsIImageRequest * anImage, nsRect & aImgRect, 
                                     const nsString & aText, nsRect & aTxtRect,
                                     PRInt32 &aWhichAlignment, PRInt32 &aMaxWidth, PRInt32 &aMaxHeight) 
{ 
  nscoord string_height = 0; 
  nscoord string_width  = 0; 

  aImgRect.SetRect(0, 0, 0, 0); 
  aTxtRect.SetRect(0, 0, 0, 0); 

  aMaxWidth  = 0;
  aMaxHeight = 0;
  PRInt32 offset = mBorderWidth + mBorderOffset;

  // set up initial text size
  if (mShowText) { 
    nsIRenderingContext *cx;
    if (NS_OK != mContext->CreateRenderingContext(this, cx)) {
      return;
    }
    nsIFontMetrics* metrics; 
    if (NS_OK != mContext->GetMetricsFor(mFont, metrics)) {
      NS_RELEASE(cx);
      return;
    }
    cx->SetFont(metrics);
    metrics->GetHeight(string_height); 
    cx->GetWidth(aText, string_width); 
    NS_RELEASE(metrics); 
    NS_RELEASE(cx);

    aTxtRect.SetRect(offset,offset, string_width, string_height); // Align left 
    if (!mShowImage || anImage == nsnull) {
      aWhichAlignment = kAlignJustText;
      // Align horizontally
      if (mTextHAlignment == eButtonHorizontalAligment_Right) { 
        aTxtRect.x = aRect.width - aTxtRect.width - offset; 
      } if (mTextHAlignment == eButtonHorizontalAligment_Middle) { 
        aTxtRect.x = (aRect.width - aTxtRect.width) / 2; 
      } 
      // Align vertically
      if (mTextVAlignment == eButtonVerticalAligment_Bottom) { 
        aTxtRect.y = aRect.height - aTxtRect.height - offset; 
      } if (mTextVAlignment == eButtonVerticalAligment_Center) { 
        aTxtRect.y = (aRect.height - aTxtRect.height) / 2; 
      } 
      aMaxWidth  = string_width;
      aMaxHeight = string_height;
    }
  }

  // Set up initial image size
  if (mShowImage && anImage != nsnull) {
    aImgRect.SetRect(offset, offset, mImageWidth, mImageHeight); // Align left 
    if (!mShowText || aText.Length() == 0) {
      aWhichAlignment = kAlignJustImage;
      // Align horizontally
      if (mImageHAlignment == eButtonHorizontalAligment_Right) { 
        aImgRect.x = aRect.width - aImgRect.width - offset; 
      } if (mImageHAlignment == eButtonHorizontalAligment_Middle) { 
        aImgRect.x = (aRect.width - aImgRect.width) / 2; 
      } 

      // Align vertically
      if (mImageVAlignment == eButtonVerticalAligment_Bottom) { 
        aImgRect.y = aRect.height - aImgRect.height - offset; 
      } if (mImageVAlignment == eButtonVerticalAligment_Center) { 
        aImgRect.y = (aRect.height - aImgRect.height) / 2; 
      } 
      aMaxWidth  = mImageWidth;
      aMaxHeight = mImageHeight;
    }
  }

  aWhichAlignment = kAlignBoth;

  // Now align and layout both text and image
  // Text align is relative to the image, so it forces the image to an alignment
  if (mTextHAlignment == eButtonHorizontalAligment_Right) {
    aImgRect.x = offset;   
    aTxtRect.x = aImgRect.x + aImgRect.width + mTextGap; 
    aMaxWidth  = aTxtRect.x + aTxtRect.width - offset;
  } else if (mTextHAlignment == eButtonHorizontalAligment_Middle) { 
    aImgRect.x = (aRect.width - aImgRect.width) / 2; 
    aTxtRect.x = (aRect.width - aTxtRect.width) / 2; 
    aMaxWidth  = (aTxtRect.width > aImgRect.width? aTxtRect.width : aImgRect.width);
  } else {  // Text is on the Left
    aTxtRect.x = offset;   
    aImgRect.x = aTxtRect.x + aTxtRect.width + mTextGap; 
    aMaxWidth  = aImgRect.x + aImgRect.width - offset;
  } 

  // vertical
  if (mTextVAlignment == eButtonVerticalAligment_Bottom) {
    aImgRect.y = offset;   
    aTxtRect.y = aImgRect.y + aImgRect.height;// + mTextGap; 
    aMaxHeight  = aTxtRect.y + aTxtRect.height;
  } else if (mTextVAlignment == eButtonVerticalAligment_Center) { 
    aImgRect.y = (aRect.height - aImgRect.height) / 2; 
    aTxtRect.y = (aRect.height - aTxtRect.height) / 2; 
    aMaxHeight  = (aTxtRect.height > aImgRect.height? aTxtRect.height : aImgRect.height);
  } else {  // Text is on the Left
    aTxtRect.y = offset;   
    aImgRect.y = aTxtRect.y + aTxtRect.height;// + mTextGap; 
    aMaxHeight = aImgRect.y + aImgRect.height - offset;
  } 
  
} 
  
