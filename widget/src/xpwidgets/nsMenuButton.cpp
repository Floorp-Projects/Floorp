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

#include "nsMenuButton.h"
#include "nsWidgetsCID.h" 
#include "nspr.h"
#include "nsIFontMetrics.h"
#include "nsIDeviceContext.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"
#include "nsIWidget.h"

static NS_DEFINE_IID(kISupportsIID,      NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIWidgetIID,        NS_IWIDGET_IID);
static NS_DEFINE_IID(kCMenuButtonCID,   NS_MENUBUTTON_CID);
static NS_DEFINE_IID(kCIMenuButtonIID,  NS_IMENUBUTTON_IID);
static NS_DEFINE_IID(kIPopUpMenuIID, NS_IPOPUPMENU_IID);
static NS_DEFINE_IID(kPopUpMenuCID, NS_POPUPMENU_CID);
static NS_DEFINE_IID(kIMenuItemIID, NS_IMENUITEM_IID);
static NS_DEFINE_IID(kMenuItemCID, NS_MENUITEM_CID);

NS_IMPL_ADDREF(nsMenuButton)
NS_IMPL_RELEASE(nsMenuButton)


//------------------------------------------------------------
nsMenuButton::nsMenuButton() : nsIMenuButton(), nsImageButton()
{
  NS_INIT_REFCNT();

  mPopUpMenu      = nsnull;
  mMenuIsPoppedUp = PR_FALSE;
}

//------------------------------------------------------------
nsMenuButton::~nsMenuButton()
{
  NS_IF_RELEASE(mPopUpMenu);
}

//------------------------------------------------------------
nsresult nsMenuButton::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kCMenuButtonCID);                         

  if (aIID.Equals(kCIMenuButtonIID)) {                                          
    *aInstancePtr = (void*) (nsIMenuButton *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) (nsMenuButton *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  return (nsImageButton::QueryInterface(aIID, aInstancePtr));
}

//------------------------------------------------------------
void nsMenuButton::CreatePopUpMenu()
{
  if (nsnull != mPopUpMenu) {
    return;
  }

  // Create and place back button
  nsresult rv = nsComponentManager::CreateInstance(kPopUpMenuCID, nsnull, kIPopUpMenuIID,
                                             (void**)&mPopUpMenu);
  if (NS_OK == rv) {
    nsIWidget * menuParentWidget;
	  if (NS_OK != this->QueryInterface(kIWidgetIID,(void**)&menuParentWidget)) {
      return;
	  }

    nsIWidget * popupWidget;
    nsRect rect;
	  if (NS_OK == mPopUpMenu->QueryInterface(kIWidgetIID,(void**)&popupWidget)) {
	    popupWidget->Create(menuParentWidget, rect, nsnull, nsnull);
		  NS_RELEASE(popupWidget);
	  }
    NS_RELEASE(menuParentWidget);
  }


  return;
}

//------------------------------------------------------------
NS_METHOD nsMenuButton::GetPopUpMenu(nsIPopUpMenu *& aPopUpMenu)
{
  CreatePopUpMenu();

  NS_ADDREF(mPopUpMenu);
  aPopUpMenu = mPopUpMenu;
  return NS_OK;
}

//------------------------------------------------------------
NS_METHOD nsMenuButton::AddMenuItem(const nsString& aMenuLabel, PRInt32 aCommand)
{
  CreatePopUpMenu();

  nsIMenuItem * menuItem = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kMenuItemCID, nsnull,  kIMenuItemIID,  (void**)&menuItem);
  menuItem->Create(mPopUpMenu, aMenuLabel, aCommand);
  if (NS_OK == rv) {
    mPopUpMenu->AddItem(menuItem);
    NS_RELEASE(menuItem);
  }
  return NS_OK;
}

//------------------------------------------------------------
NS_METHOD nsMenuButton::InsertMenuItem(const nsString& aMenuLabel, PRInt32 aCommand, PRInt32 aPos)
{
  CreatePopUpMenu();
  return NS_OK;
}

//------------------------------------------------------------
NS_METHOD nsMenuButton::RemovesMenuItem(PRInt32 aPos)
{
  CreatePopUpMenu();
  return NS_OK;
}

//------------------------------------------------------------
NS_METHOD nsMenuButton::RemoveAllMenuItems(PRInt32 aPos)
{
  CreatePopUpMenu();
  return NS_OK;
}


//-----------------------------------------------------------------------------
nsEventStatus nsMenuButton::OnLeftButtonDown(nsGUIEvent *aEvent)
{
  mState |= eButtonState_pressed;
  Invalidate(PR_TRUE);

  nsRect rect;
  GetBounds(rect);

  if (mPopUpMenu) {
    mMenuIsPoppedUp = PR_TRUE;
    mPopUpMenu->ShowMenu(0, rect.height);
    mMenuIsPoppedUp = PR_FALSE;
  }
  return nsEventStatus_eIgnore;
}

//-----------------------------------------------------------------------------
nsEventStatus nsMenuButton::PaintBorder(nsIRenderingContext & aRenderingContext,
                                         const nsRect       & aDirtyRect,
                                         const nsRect       & aEntireRect)
{
  nsRect rect(aEntireRect);
  if ((mState == eButtonState_up || (mState & eButtonState_disabled)) && !mAlwaysShowBorder && !mMenuIsPoppedUp) {
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

  if ((mState & eButtonState_pressed) || mMenuIsPoppedUp) {
    aRenderingContext.SetColor(mHighlightColor);
  } else {
    aRenderingContext.SetColor(mShadowColor);
  }
  aRenderingContext.DrawLine(rect.x, rect.height, rect.width, rect.height);
  aRenderingContext.DrawLine(rect.width, rect.y, rect.width, rect.height);

  if ((mState & eButtonState_pressed) || mMenuIsPoppedUp) {
    aRenderingContext.SetColor(mShadowColor);
  } else {
    aRenderingContext.SetColor(mHighlightColor);
  }

  aRenderingContext.DrawLine(rect.x,rect.y, rect.width,rect.y);
  aRenderingContext.DrawLine(rect.x,rect.y, rect.x, rect.height);


  return nsEventStatus_eConsumeNoDefault;  
}



//------------------------------------------------------------
nsresult nsMenuButton::SetBounds(const nsRect &aBounds)
{
  return nsImageButton::SetBounds(aBounds);
}

//------------------------------------------------------------
nsEventStatus nsMenuButton::OnPaint(nsIRenderingContext& aRenderingContext,
                                       const nsRect& aDirtyRect)
{
  // draw the button, as normal
  nsEventStatus rv = nsImageButton::OnPaint(aRenderingContext, aDirtyRect);
  
  // draw the triangle in the top right corner to indicate this is a dropdown,
  // but only if the dirty rect contains that area
  if ( aDirtyRect.YMost() > mBounds.YMost() - 11 ) {
    aRenderingContext.PushState();
    nscolor triangleColor = 0; 
    if ( mState & eButtonState_disabled )
      triangleColor = nscolor(NS_RGB(0xaa,0xaa,0xaa));   //*** this should go to l&f object
    aRenderingContext.SetColor(triangleColor);

    // it would be great if I could just use a polygon here, but this
    // way guarantees it will be a nice triangle shape on all gfx platforms
    int lineWidth = 10;
    int hStart = mBounds.width - 14;
    for (int i = 5; lineWidth >= 0; ++i, lineWidth -= 2, hStart += 1)
    {
      int hEnd = hStart + lineWidth;
      aRenderingContext.DrawLine(hStart, i, hEnd, i);
    }
   
    PRBool ignored;
    aRenderingContext.PopState(ignored);
  }
  return rv;
}


//-----------------------------------------------------------------------------
nsEventStatus nsMenuButton::OnMouseMove(nsGUIEvent *aEvent)
{ 
  return nsImageButton::OnMouseMove(aEvent);
}

//-----------------------------------------------------------------------------
nsEventStatus nsMenuButton::OnMouseEnter(nsGUIEvent *aEvent)
{  
  return nsImageButton::OnMouseEnter(aEvent);
}

//-----------------------------------------------------------------------------
nsEventStatus nsMenuButton::OnMouseExit(nsGUIEvent *aEvent)
{  
  return nsImageButton::OnMouseExit(aEvent);
}

//-----------------------------------------------------------------------------
nsEventStatus nsMenuButton::OnLeftButtonUp(nsGUIEvent *aEvent)
{
  return nsImageButton::OnLeftButtonUp(aEvent);
}

//-----------------------------------------------------------------------------
NS_METHOD nsMenuButton::GetLabel(nsString & aString) 
{
  return nsImageButton::GetLabel(aString);
}

//-----------------------------------------------------------------------------
NS_METHOD nsMenuButton::SetLabel(const nsString& aString)
{
  return nsImageButton::SetLabel(aString);
}

//-----------------------------------------------------------------------------
NS_METHOD nsMenuButton::GetRollOverDesc(nsString & aString) 
{
  return nsImageButton::GetRollOverDesc(aString);
}

//-----------------------------------------------------------------------------
NS_METHOD nsMenuButton::SetRollOverDesc(const nsString& aString)
{
  return nsImageButton::SetRollOverDesc(aString);
}

//-----------------------------------------------------------------------------
NS_METHOD nsMenuButton::GetCommand(PRInt32 & aCommand) 
{
  return nsImageButton::GetCommand(aCommand);
}

//-----------------------------------------------------------------------------
NS_METHOD nsMenuButton::SetCommand(PRInt32 aCommand) 
{
  return nsImageButton::SetCommand(aCommand);
}


//-----------------------------------------------------------------------------
NS_METHOD nsMenuButton::GetHighlightColor(nscolor &aColor)
{
  return nsImageButton::GetHighlightColor(aColor);
}

//-----------------------------------------------------------------------------
NS_METHOD nsMenuButton::SetHighlightColor(const nscolor &aColor)
{
  return nsImageButton::SetHighlightColor(aColor);
}

//-----------------------------------------------------------------------------
NS_METHOD nsMenuButton::GetShadowColor(nscolor &aColor)
{
  return nsImageButton::GetShadowColor(aColor);
}

//-----------------------------------------------------------------------------
NS_METHOD nsMenuButton::SetShadowColor(const nscolor &aColor)
{
  return nsImageButton::SetShadowColor(aColor);
}

//-----------------------------------------------------------------------------
nsEventStatus nsMenuButton::HandleEvent(nsGUIEvent *aEvent) 
{
  return nsImageButton::HandleEvent(aEvent);
}

//----------------------------------------------------------------
NS_METHOD nsMenuButton::SetImageDimensions(const PRInt32 & aWidth, const PRInt32 & aHeight)
{
  return nsImageButton::SetImageDimensions(aWidth, aHeight);
}

//----------------------------------------------------------------
NS_METHOD nsMenuButton::SetImageURLs(const nsString& aUpURL,
                                      const nsString& aPressedURL,
                                      const nsString& aDisabledURL,
                                      const nsString& aRollOverURL)
{
  return nsImageButton::SetImageURLs(aUpURL, aPressedURL, aDisabledURL, aRollOverURL);
}

//----------------------------------------------------------------
NS_METHOD nsMenuButton::SetImageUpURL(const nsString& aUpURL)
{
  return nsImageButton::SetImageUpURL(aUpURL);
}

//----------------------------------------------------------------
NS_METHOD nsMenuButton::SetImagePressedURL(const nsString& aPressedURL)
{
  return nsImageButton::SetImagePressedURL(aPressedURL);
}

//----------------------------------------------------------------
NS_METHOD nsMenuButton::SetImageDisabledURL(const nsString& aDisabledURL)
{
  return nsImageButton::SetImageDisabledURL(aDisabledURL);
}

//----------------------------------------------------------------
NS_METHOD nsMenuButton::SetImageRollOverURL(const nsString& aRollOverURL)
{
  return nsImageButton::SetImageRollOverURL(aRollOverURL);
}

//----------------------------------------------------------------
NS_METHOD nsMenuButton::SetShowBorder(PRBool aState)
{
  return nsImageButton::SetShowBorder(aState);
}

//----------------------------------------------------------------
NS_METHOD nsMenuButton::SetAlwaysShowBorder(PRBool aState)
{
  return nsImageButton::SetAlwaysShowBorder(aState);
}

//----------------------------------------------------------------
NS_METHOD nsMenuButton::SwapHighlightShadowColors()
{
  return nsImageButton::SwapHighlightShadowColors();
}

//----------------------------------------------------------------
NS_METHOD nsMenuButton::SetShowButtonBorder(PRBool aState)
{
  return nsImageButton::SetShowButtonBorder(aState);
}

//----------------------------------------------------------------
NS_METHOD nsMenuButton::SetBorderWidth(PRInt32 aWidth)
{
  return nsImageButton::SetBorderWidth(aWidth);
}

//----------------------------------------------------------------
NS_METHOD nsMenuButton::SetBorderOffset(PRInt32 aOffset)
{
  return nsImageButton::SetBorderOffset(aOffset);
}

//----------------------------------------------------------------
NS_METHOD nsMenuButton::SetShowText(PRBool aState)
{
  return nsImageButton::SetShowText(aState);
}

//----------------------------------------------------------------
NS_METHOD nsMenuButton::SetShowImage(PRBool aState)
{
  return nsImageButton::SetShowImage(aState);
}

//----------------------------------------------------------------
NS_METHOD nsMenuButton::SetImageVerticalAlignment(nsButtonVerticalAligment aAlign)
{
  return nsImageButton::SetImageVerticalAlignment(aAlign);
}

//----------------------------------------------------------------
NS_METHOD nsMenuButton::SetImageHorizontalAlignment(nsButtonHorizontalAligment aAlign)
{
  return nsImageButton::SetImageHorizontalAlignment(aAlign);
}

//----------------------------------------------------------------
NS_METHOD nsMenuButton::SetTextVerticalAlignment(nsButtonVerticalAligment aAlign)
{
  return nsImageButton::SetTextVerticalAlignment(aAlign);
}

//----------------------------------------------------------------
NS_METHOD nsMenuButton::SetTextHorizontalAlignment(nsButtonHorizontalAligment aAlign)
{
  return nsImageButton::SetTextHorizontalAlignment(aAlign);
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuButton::AddListener(nsIImageButtonListener * aListener)
{
  return nsImageButton::AddListener(aListener);
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuButton::RemoveListener(nsIImageButtonListener * aListener)
{
  return nsImageButton::RemoveListener(aListener);
}


//-------------------------------------------------------------------------
NS_METHOD nsMenuButton::GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight)
{
  return nsImageButton::GetPreferredSize(aWidth, aHeight);
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuButton::SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight)
{
  return nsImageButton::SetPreferredSize(aWidth, aHeight);
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuButton::Enable(PRBool aState)
{
  return nsImageButton::Enable(aState);
}


