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

#include "nsToolbarItemHolder.h"
#include "nsWidgetsCID.h"
#include "nsIWidget.h"
#include "nsRepository.h"
#include "nsIToolbarItem.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

static NS_DEFINE_IID(kCToolbarItemHolderCID,  NS_TOOLBARITEMHOLDER_CID);
static NS_DEFINE_IID(kCIToolbarItemHolderIID, NS_ITOOLBARITEMHOLDER_IID);
static NS_DEFINE_IID(kIToolbarItemHolderIID, NS_ITOOLBARITEMHOLDER_IID);

static NS_DEFINE_IID(kCIToolbarItemIID, NS_ITOOLBARITEM_IID);

NS_IMPL_ADDREF(nsToolbarItemHolder)
NS_IMPL_RELEASE(nsToolbarItemHolder)

//static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);

//--------------------------------------------------------------------
//-- nsToolbarItemHolder Constructor
//--------------------------------------------------------------------
nsToolbarItemHolder::nsToolbarItemHolder() : nsIToolbarItemHolder(), nsIToolbarItem()
{
  NS_INIT_REFCNT();

}

//--------------------------------------------------------------------
nsToolbarItemHolder::~nsToolbarItemHolder()
{

}

//--------------------------------------------------------------------
nsresult nsToolbarItemHolder::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  
  if (aIID.Equals(kCIToolbarItemHolderIID)) {                                          
    *aInstancePtr = (void*) (nsIToolbarItemHolder *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kCIToolbarItemIID)) {                                          
    *aInstancePtr = (void*) (nsIToolbarItem *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  return mWidget->QueryInterface(aIID, aInstancePtr);
}




//----------------------------------------------------------
NS_METHOD nsToolbarItemHolder::SetWidget(nsIWidget * aWidget)
{
  mWidget = aWidget;
  return NS_OK;
}


//--------------------------------------------------------------------
NS_METHOD nsToolbarItemHolder::GetWidget(nsIWidget *&aWidget)
{
  aWidget = mWidget;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsToolbarItemHolder::Repaint(PRBool aIsSynchronous)

{
  if ( mWidget )
    mWidget->Invalidate(aIsSynchronous);
  return NS_OK;
}
    
//--------------------------------------------------------------------
NS_METHOD nsToolbarItemHolder::GetBounds(nsRect & aRect)
{
  if ( mWidget )
    mWidget->GetBounds(aRect);
  return NS_OK;
}

//--------------------------------------------------------------------
NS_METHOD nsToolbarItemHolder::SetBounds(PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint)
{
  if ( mWidget )
    mWidget->Resize(aWidth, aHeight, aRepaint);
  return NS_OK;
}
    
//-------------------------------------------------------------------------
NS_METHOD nsToolbarItemHolder::SetBounds(PRUint32 aX,
                                          PRUint32 aY,
                                          PRUint32 aWidth,
                                          PRUint32 aHeight,
                                          PRBool   aRepaint)
{
  if ( mWidget )
    mWidget->Resize(aX, aY, aWidth, aHeight, aRepaint);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsToolbarItemHolder::SetVisible(PRBool aState) 
{
  if (nsnull != mWidget) {
    mWidget->Show(aState);
  }
  return NS_OK;
}
//-------------------------------------------------------------------------
NS_METHOD nsToolbarItemHolder::IsVisible(PRBool & aState) 
{
  if (nsnull != mWidget) {
    return mWidget->IsVisible(aState);
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsToolbarItemHolder::SetLocation(PRUint32 aX, PRUint32 aY) 
{
  if (nsnull != mWidget) {
    mWidget->Move(aX, aY);
  }
  return NS_OK;
}

//--------------------------------------------------------------------
NS_METHOD nsToolbarItemHolder::GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight)
{
  if (nsnull != mWidget) {
    return mWidget->GetPreferredSize(aWidth, aHeight);
  }
  return NS_OK;
}

//--------------------------------------------------------------------
NS_METHOD nsToolbarItemHolder::SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight)
{
  if (nsnull != mWidget) {
    return mWidget->SetPreferredSize(aWidth, aHeight);
  }
  return NS_OK;
}

