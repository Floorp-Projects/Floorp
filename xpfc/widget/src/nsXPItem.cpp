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

#include "nsXPItem.h"
#include "nsxpfcCIID.h"
#include "nspr.h"
#include "nsWidgetsCID.h"
#include "nsXPFCToolkit.h"
#include "nsxpfcstrings.h"

#include "nsIAppShell.h"
#include "nsIWidget.h"
#include "nsITextWidget.h"
#include "nsXPItem.h"
#include "nsIImageGroup.h"
#include "nsITimer.h"
#include "nsIThrobber.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCXPItemCID, NS_XP_ITEM_CID);
static NS_DEFINE_IID(kCIXPItemIID, NS_IXP_ITEM_IID);

#define DEFAULT_WIDTH  50
#define DEFAULT_HEIGHT 50

nsXPItem :: nsXPItem(nsISupports* outer) : nsXPFCCanvas(outer)
{
  NS_INIT_REFCNT();

  mVerticalJustification   = eTextJustification_center;
  mHorizontalJustification = eTextJustification_center;

  mShowImage = eShowImage_full;
  mShowText  = eShowText_full;

  mMiniImageRequest = nsnull;
  mFullImageRequest = nsnull;
}

nsXPItem :: ~nsXPItem()
{
  NS_IF_RELEASE(mMiniImageRequest);
  NS_IF_RELEASE(mFullImageRequest);
}

nsresult nsXPItem::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kCXPItemCID);                         
  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) (nsXPItem *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kCIXPItemIID)) {                                          
    *aInstancePtr = (void*) (nsIXPItem *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  return (nsXPFCCanvas::QueryInterface(aIID, aInstancePtr));
}

NS_IMPL_ADDREF(nsXPItem)
NS_IMPL_RELEASE(nsXPItem)

nsresult nsXPItem :: Init()
{

  nsresult res = nsXPFCCanvas::Init();    

  return res;
}

nsresult nsXPItem :: SetParameter(nsString& aKey, nsString& aValue)
{
  nsRect bounds;

  GetBounds(bounds);

  if (aKey.EqualsIgnoreCase(XPFC_STRING_MINIIMAGE)) {

    CreateImageGroup();

    mMiniImageRequest = RequestImage(aValue);

  } else if (aKey.EqualsIgnoreCase(XPFC_STRING_FULLIMAGE)) {

    CreateImageGroup();

    mFullImageRequest = RequestImage(aValue);

  } else if (aKey.EqualsIgnoreCase(XPFC_STRING_VALIGN)) {

    if (aValue.EqualsIgnoreCase(XPFC_STRING_LEFT))
      mVerticalJustification = eTextJustification_left;
    else if (aValue.EqualsIgnoreCase(XPFC_STRING_RIGHT))
      mVerticalJustification = eTextJustification_right;
    else if (aValue.EqualsIgnoreCase(XPFC_STRING_TOP))
      mVerticalJustification = eTextJustification_top;
    else if (aValue.EqualsIgnoreCase(XPFC_STRING_BOTTOM))
      mVerticalJustification = eTextJustification_bottom;
    else if (aValue.EqualsIgnoreCase(XPFC_STRING_CENTER))
      mVerticalJustification = eTextJustification_center;

  } else if (aKey.EqualsIgnoreCase(XPFC_STRING_HALIGN)) {

    if (aValue.EqualsIgnoreCase(XPFC_STRING_LEFT))
      mHorizontalJustification = eTextJustification_left;
    else if (aValue.EqualsIgnoreCase(XPFC_STRING_RIGHT))
      mHorizontalJustification = eTextJustification_right;
    else if (aValue.EqualsIgnoreCase(XPFC_STRING_TOP))
      mHorizontalJustification = eTextJustification_top;
    else if (aValue.EqualsIgnoreCase(XPFC_STRING_BOTTOM))
      mHorizontalJustification = eTextJustification_bottom;
    else if (aValue.EqualsIgnoreCase(XPFC_STRING_CENTER))
      mHorizontalJustification = eTextJustification_center;


  } else if (aKey.EqualsIgnoreCase(XPFC_STRING_ENABLE)) {


  }

  return (nsXPFCCanvas::SetParameter(aKey, aValue));
}

nsresult nsXPItem :: GetClassPreferredSize(nsSize& aSize)
{
  aSize.width  = DEFAULT_WIDTH;
  aSize.height = DEFAULT_HEIGHT;
  return (NS_OK);
}

nsresult nsXPItem :: CreateView()
{
  nsresult res = NS_OK;
  return res;
}

nsresult nsXPItem :: SetLabel(nsString& aString)
{
  nsXPFCCanvas::SetLabel(aString);
  return NS_OK;
}

nsresult nsXPItem :: SetBounds(const nsRect &aBounds)
{
  return (nsXPFCCanvas::SetBounds(aBounds));
}

nsEventStatus nsXPItem :: OnPaint(nsIRenderingContext& aRenderingContext,
                                      const nsRect& aDirtyRect)
{
  PushState(aRenderingContext);
  PaintBackground(aRenderingContext,aDirtyRect);
  PaintForeground(aRenderingContext,aDirtyRect);
  PopState(aRenderingContext);
  return nsEventStatus_eConsumeNoDefault;  
}


nsEventStatus nsXPItem :: PaintBackground(nsIRenderingContext& aRenderingContext,
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
      if (mMiniImageRequest != nsnull)
        img = mMiniImageRequest->GetImage();
      break;

    case eShowImage_full:
      if (mFullImageRequest != nsnull)
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

nsEventStatus nsXPItem :: PaintForeground(nsIRenderingContext& aRenderingContext,
                                              const nsRect& aDirtyRect)
{
  if (mShowText == eShowText_none)
    return nsEventStatus_eConsumeNoDefault;  

  // XXX: We really need to query system-wide colors via gfx system manager here?
  //      On windows, the calls are:
  //
  // GetSysColor(COLOR_BTNFACE), GetSysColor(COLOR_GRAYTEXT), GetSysColor(COLOR_GRAYTEXT)
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

