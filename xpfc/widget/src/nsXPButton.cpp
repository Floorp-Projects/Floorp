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

#include "nsXPButton.h"
#include "nsxpfcCIID.h"
#include "nspr.h"
#include "nsIButton.h"
#include "nsWidgetsCID.h"
#include "nsXPFCToolkit.h"
#include "nsxpfcstrings.h"

#include "nsIAppShell.h"
#include "nsIWidget.h"
#include "nsITextWidget.h"
#include "nsIButton.h"
#include "nsIImageGroup.h"
#include "nsITimer.h"
#include "nsIThrobber.h"
#include "nsXPItem.h"
#include "nsIWebViewerContainer.h"
#include "nsViewsCID.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCXPButtonCID, NS_XP_BUTTON_CID);
static NS_DEFINE_IID(kCIXPButtonIID, NS_IXP_BUTTON_IID);
static NS_DEFINE_IID(kCButtonCID,    NS_BUTTON_CID);
static NS_DEFINE_IID(kInsButtonIID, NS_IBUTTON_IID);
static NS_DEFINE_IID(kViewCID,                    NS_VIEW_CID);

#define DEFAULT_WIDTH  50
#define DEFAULT_HEIGHT 50

nsXPButton :: nsXPButton(nsISupports* outer) : nsXPItem(outer)
{
  NS_INIT_REFCNT();

  mMiniHoverImageRequest = nsnull;
  mFullHoverImageRequest = nsnull;
  mMiniPressedImageRequest = nsnull;
  mFullPressedImageRequest = nsnull;

  mState = eButtonState_none;
}

nsXPButton :: ~nsXPButton()
{
  NS_IF_RELEASE(mMiniHoverImageRequest);
  NS_IF_RELEASE(mFullHoverImageRequest);
  NS_IF_RELEASE(mMiniPressedImageRequest);
  NS_IF_RELEASE(mFullPressedImageRequest);
}

nsresult nsXPButton::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kCXPButtonCID);                         
  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) (nsXPButton *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kCIXPButtonIID)) {                                          
    *aInstancePtr = (void*) (nsIXPButton *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  return (nsXPItem::QueryInterface(aIID, aInstancePtr));
}

NS_IMPL_ADDREF(nsXPButton)
NS_IMPL_RELEASE(nsXPButton)

nsresult nsXPButton :: Init()
{

  nsresult res = nsXPItem::Init();

  LoadView(kViewCID);

  return res;
}

nsresult nsXPButton :: SetParameter(nsString& aKey, nsString& aValue)
{
  if (aKey.EqualsIgnoreCase(XPFC_STRING_MINIHOVERIMAGE)) {

    CreateImageGroup();

    mMiniHoverImageRequest = RequestImage(aValue);

  } else if (aKey.EqualsIgnoreCase(XPFC_STRING_FULLHOVERIMAGE)) {

    CreateImageGroup();

    mFullHoverImageRequest = RequestImage(aValue);

  } else if (aKey.EqualsIgnoreCase(XPFC_STRING_MINIPRESSEDIMAGE)) {

    CreateImageGroup();

    mMiniPressedImageRequest = RequestImage(aValue);

  } else if (aKey.EqualsIgnoreCase(XPFC_STRING_FULLPRESSEDIMAGE)) {

    CreateImageGroup();

    mFullPressedImageRequest = RequestImage(aValue);

  } else if (aKey.EqualsIgnoreCase(XPFC_STRING_COMMAND)) {

    // XXX: Hardcoded to LoadUrl?
    nsString command;

    aValue.Mid(command,8,aValue.Length()-8);

    SetCommand(command);    
    
  //} else if (aKey.EqualsIgnoreCase(XPFC_STRING_ENABLE)) {


  } else {

    return (nsXPItem::SetParameter(aKey, aValue));

  }

  return NS_OK;

}

nsresult nsXPButton :: GetClassPreferredSize(nsSize& aSize)
{
  aSize.width  = DEFAULT_WIDTH;
  aSize.height = DEFAULT_HEIGHT;
  return (NS_OK);
}

nsresult nsXPButton :: CreateView()
{
  nsresult res = NS_OK;
  return res;
}

nsresult nsXPButton :: SetLabel(nsString& aString)
{
  nsXPItem::SetLabel(aString);
  return NS_OK;
}

nsresult nsXPButton :: SetBounds(const nsRect &aBounds)
{
  return (nsXPItem::SetBounds(aBounds));
}

nsEventStatus nsXPButton :: OnPaint(nsIRenderingContext& aRenderingContext,
                                      const nsRect& aDirtyRect)
{
  PushState(aRenderingContext);
  PaintBackground(aRenderingContext,aDirtyRect);
  PaintForeground(aRenderingContext,aDirtyRect);
  PopState(aRenderingContext);

  return nsEventStatus_eConsumeNoDefault;  
}


nsEventStatus nsXPButton :: PaintBackground(nsIRenderingContext& aRenderingContext,
                                              const nsRect& aDirtyRect)
{

  nsRect rect;
  nsIImage * img = nsnull;

  GetBounds(rect);

  if (mShowImage == eShowImage_none)
  {
    aRenderingContext.SetColor(GetBackgroundColor());
    aRenderingContext.FillRect(rect);
    return nsEventStatus_eConsumeNoDefault;  
  }
  

  switch(mShowImage)
  {
    case eShowImage_mini:
      if ((mState & eButtonState_pressed) && (mMiniPressedImageRequest != nsnull))
        img = mMiniPressedImageRequest->GetImage();
      else if ((mState & eButtonState_hover) && (mMiniHoverImageRequest != nsnull))
        img = mMiniHoverImageRequest->GetImage();

      if ((img == nsnull) && (mMiniImageRequest != nsnull))
        img = mMiniImageRequest->GetImage();
      break;

    case eShowImage_full:
      if ((mState & eButtonState_pressed) && (mFullPressedImageRequest != nsnull))
        img = mFullPressedImageRequest->GetImage();
      else if ((mState & eButtonState_hover) && (mFullHoverImageRequest != nsnull))
        img = mFullHoverImageRequest->GetImage();

      if ((img == nsnull) && (mFullImageRequest != nsnull))
        img = mFullImageRequest->GetImage();
      break;

  }

  if (img == nsnull)
  {
    aRenderingContext.SetColor(GetBackgroundColor());
    aRenderingContext.FillRect(rect);
  }
  else
  {
    aRenderingContext.DrawImage(img, rect.x, rect.y);
  }


  NS_IF_RELEASE(img);

  return nsEventStatus_eConsumeNoDefault;  
}

nsEventStatus nsXPButton :: PaintForeground(nsIRenderingContext& aRenderingContext,
                                              const nsRect& aDirtyRect)
{
  if (mShowText == eShowText_none)
    return nsEventStatus_eConsumeNoDefault;  

  // XXX: We really need to query system-wide colors via gfx system manager here?
  //      On windows, the calls are:
  //
  // GetSysColor(COLOR_BTNFACE), GetSysColor(COLOR_GRAYTEXT), GetSysColor(COLOR_GRAYTEXT)

  if ((mState & eButtonState_hover))
    aRenderingContext.SetColor(NS_RGB(64, 128, 192));
  else
    aRenderingContext.SetColor(GetForegroundColor());

  // Draw text based on justifications

  nsRect bounds;
  nscoord x, y;
  nscoord string_height, string_width;
  nsString string = GetLabel();

  GetBounds(bounds);

  /*
   * compute the Metrics for the string
   */
  
  aRenderingContext.GetFontMetrics()->GetHeight(string_height);
  aRenderingContext.GetWidth(string,string_width);

  switch(mVerticalJustification)
  {

    case eTextJustification_top:
      y = bounds.y;
      break;

    case eTextJustification_bottom:
      y = bounds.y + bounds.height - string_height;
      break;

    default:
      y = ((bounds.height - string_height)>>1)+bounds.y;
      break;

  }

  switch(mHorizontalJustification)
  {

    case eTextJustification_left:
      x = bounds.x;
      break;

    case eTextJustification_right:
      x = bounds.x + bounds.width - string_width;
      break;

    default:
      x = ((bounds.width - string_width)>>1)+bounds.x;
      break;

  }
  
  aRenderingContext.DrawString(string,x,y,0);

  return nsEventStatus_eConsumeNoDefault;  
}

nsEventStatus nsXPButton::OnMouseMove(nsGUIEvent *aEvent)
{ 
  return (nsXPItem::OnMouseMove(aEvent));
}

nsEventStatus nsXPButton::OnMouseEnter(nsGUIEvent *aEvent)
{  
  mState |= eButtonState_hover;

  if (gXPFCToolkit->GetCanvasManager()->GetPressedCanvas() == ((nsIXPFCCanvas *)this))
    mState |= eButtonState_pressed;

  nsRect bounds;

  GetView()->GetBounds(bounds);

  // XXX: Need to convert coordinates to local views space.
  //      this means that GetBounds needs to look 'up' the
  //      view tree to figure out what view this canvas belongs
  //      to and convert thusly.
  //
  //      Alternatively, we could just give every canvas a view...
  //
  bounds.x = 0;
  bounds.y = 0;

  gXPFCToolkit->GetViewManager()->UpdateView(GetView(), bounds, NS_VMREFRESH_AUTO_DOUBLE_BUFFER | NS_VMREFRESH_NO_SYNC);
  return (nsXPItem::OnMouseEnter(aEvent));
}

nsEventStatus nsXPButton::OnMouseExit(nsGUIEvent *aEvent)
{  
  mState &= ~eButtonState_hover;
  mState &= ~eButtonState_pressed;
  nsRect bounds;

  GetView()->GetBounds(bounds);
  bounds.x = 0;
  bounds.y = 0;
  gXPFCToolkit->GetViewManager()->UpdateView(GetView(), bounds, NS_VMREFRESH_AUTO_DOUBLE_BUFFER | NS_VMREFRESH_NO_SYNC);
  return (nsXPItem::OnMouseExit(aEvent));
}

nsEventStatus nsXPButton :: OnLeftButtonDown(nsGUIEvent *aEvent)
{
  mState |= eButtonState_pressed;
  nsRect bounds;

  GetView()->GetBounds(bounds);
  bounds.x = 0;
  bounds.y = 0;
  gXPFCToolkit->GetViewManager()->UpdateView(GetView(), bounds, NS_VMREFRESH_AUTO_DOUBLE_BUFFER | NS_VMREFRESH_NO_SYNC);
  return (nsXPItem::OnLeftButtonDown(aEvent));
}

nsEventStatus nsXPButton :: OnLeftButtonUp(nsGUIEvent *aEvent)
{
  mState &= ~eButtonState_pressed;
  nsRect bounds;

  GetView()->GetBounds(bounds);
  bounds.x = 0;
  bounds.y = 0;
  gXPFCToolkit->GetViewManager()->UpdateView(GetView(), bounds, NS_VMREFRESH_AUTO_DOUBLE_BUFFER | NS_VMREFRESH_NO_SYNC);

  SendCommand();

  return (nsXPItem::OnLeftButtonUp(aEvent));
}

