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

#include "nsBaseWidget.h"
#include "nsIAppShell.h"
#include "nsIDeviceContext.h"

#include "nsGfxCIID.h"
#include "nsWidgetsCID.h"

static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
static NS_DEFINE_IID(kIEnumeratorIID, NS_IENUMERATOR_IID);

NS_IMPL_ISUPPORTS(nsBaseWidget, kIWidgetIID)
NS_IMPL_ISUPPORTS(nsBaseWidget::Enumerator, kIEnumeratorIID)

//-------------------------------------------------------------------------
//
// Setup initial tooltip rectangles
//
//-------------------------------------------------------------------------

NS_METHOD nsBaseWidget::SetTooltips(PRUint32 aNumberOfTips,nsRect* aTooltipAreas[])
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-------------------------------------------------------------------------
//
// Update all tooltip rectangles
//
//-------------------------------------------------------------------------

NS_METHOD nsBaseWidget::UpdateTooltips(nsRect* aNewTips[])
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-------------------------------------------------------------------------
//
// Remove all tooltip rectangles
//
//-------------------------------------------------------------------------

NS_METHOD nsBaseWidget::RemoveTooltips()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-------------------------------------------------------------------------
//
// nsBaseWidget constructor
//
//-------------------------------------------------------------------------
nsBaseWidget::nsBaseWidget() :
  mBounds(0,0,0,0)
{
    mChildren      = nsnull;
    mEventCallback = nsnull;
    mToolkit       = nsnull;
    mAppShell      = nsnull;
    mMouseListener = nsnull;
    mEventListener = nsnull;
    /*mMenuListener  = nsnull;*/
    mClientData    = nsnull;
    mContext       = nsnull;
    mCursor        = eCursor_standard;
    mBorderStyle   = eBorderStyle_none;
}


//-------------------------------------------------------------------------
//
// nsBaseWidget destructor
//
//-------------------------------------------------------------------------
nsBaseWidget::~nsBaseWidget()
{
}

//-------------------------------------------------------------------------
//
// Basic create.
//
//-------------------------------------------------------------------------
void nsBaseWidget::BaseCreate(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
    if (nsnull == mToolkit) {
        if (nsnull != aToolkit) {
            mToolkit = (nsIToolkit*)aToolkit;
            NS_ADDREF(mToolkit);
        }
        else {
            if (nsnull != aParent) {
                mToolkit = (nsIToolkit*)(aParent->GetToolkit()); // the call AddRef's, we don't have to
            }
            // it's some top level window with no toolkit passed in.
            // Create a default toolkit with the current thread
            else {
               static NS_DEFINE_IID(kToolkitCID, NS_TOOLKIT_CID);
               static NS_DEFINE_IID(kToolkitIID, NS_ITOOLKIT_IID);

               nsresult res;
               res = nsRepository::CreateInstance(kToolkitCID, nsnull, kToolkitIID, (void **)&mToolkit);
               if (NS_OK != res)
                  NS_ASSERTION(PR_FALSE, "Can not create a toolkit in nsBaseWidget::Create");
               if (mToolkit)
	               mToolkit->Init(PR_GetCurrentThread());
            }
        }

    }

    mAppShell = aAppShell;
    NS_IF_ADDREF(mAppShell);

    // save the event callback function
    mEventCallback = aHandleEventFunction;

    // keep a reference to the device context
    if (aContext) {
        mContext = aContext;
        NS_ADDREF(mContext);
    }
    else {
      nsresult  res;

      static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);
      static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

      res = nsRepository::CreateInstance(kDeviceContextCID, nsnull, kDeviceContextIID, (void **)&mContext);

      if (NS_OK == res)
        mContext->Init(nsnull);
    }

    if (nsnull != aInitData) {
      PreCreateWidget(aInitData);
    }

    if (aParent) {
      aParent->AddChild(this);
    }

    // Force cursor to default setting
    mCursor = eCursor_select;
    SetCursor(eCursor_standard);
}

//-------------------------------------------------------------------------
//
// Accessor functions to get/set the client data
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsBaseWidget::GetClientData(void*& aClientData)
{
  aClientData = mClientData;
  return NS_OK;
}

NS_IMETHODIMP nsBaseWidget::SetClientData(void* aClientData)
{
  mClientData = aClientData;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Close this nsBaseWidget
//
//-------------------------------------------------------------------------
NS_METHOD nsBaseWidget::Destroy()
{
  // disconnect from the parent
  nsIWidget *parent = GetParent();
  if (parent) {
    parent->RemoveChild(this);
    NS_RELEASE(parent);
  }
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get this nsBaseWidget parent
//
//-------------------------------------------------------------------------
nsIWidget* nsBaseWidget::GetParent(void)
{
  return nsnull;
}

//-------------------------------------------------------------------------
//
// Get this nsBaseWidget's list of children
//
//-------------------------------------------------------------------------
nsIEnumerator* nsBaseWidget::GetChildren()
{
  if (mChildren) {
    // Reset the current position to 0
    mChildren->First();
    nsISupports   * child;
    if (!NS_SUCCEEDED(mChildren->CurrentItem(&child))) {
      return NULL;
    }
    // Make a copy of our enumerator
    Enumerator * children = new Enumerator;
    NS_ADDREF(children);
    do
    {
      nsIWidget *widget;
      if (!NS_SUCCEEDED(mChildren->CurrentItem(&child))) {
        delete children;
        return NULL;
      }
      if (NS_OK == child->QueryInterface(kIWidgetIID, (void**)&widget)) {
        children->Append(widget);
        NS_IF_RELEASE(widget);
      }
      NS_IF_RELEASE(child);
    }
    while (NS_SUCCEEDED(mChildren->Next()));

    return (nsIEnumerator*)children;
  }

  return NULL;
}


//-------------------------------------------------------------------------
//
// Add a child to the list of children
//
//-------------------------------------------------------------------------
void nsBaseWidget::AddChild(nsIWidget* aChild)
{
  if (!mChildren) {
    mChildren = new Enumerator;
  }

  mChildren->Append(aChild);
}


//-------------------------------------------------------------------------
//
// Remove a child from the list of children
//
//-------------------------------------------------------------------------
void nsBaseWidget::RemoveChild(nsIWidget* aChild)
{
  if (mChildren) {
    mChildren->Remove(aChild);
  }
}
    
//-------------------------------------------------------------------------
//
// Get the foreground color
//
//-------------------------------------------------------------------------
nscolor nsBaseWidget::GetForegroundColor(void)
{
    return mForeground;
}

    
//-------------------------------------------------------------------------
//
// Set the foreground color
//
//-------------------------------------------------------------------------
NS_METHOD nsBaseWidget::SetForegroundColor(const nscolor &aColor)
{
    mForeground = aColor;
    return NS_OK;
}

    
//-------------------------------------------------------------------------
//
// Get the background color
//
//-------------------------------------------------------------------------
nscolor nsBaseWidget::GetBackgroundColor(void)
{
    return mBackground;
}

//-------------------------------------------------------------------------
//
// Set the background color
//
//-------------------------------------------------------------------------
NS_METHOD nsBaseWidget::SetBackgroundColor(const nscolor &aColor)
{
    mBackground = aColor;
    return NS_OK;
}
     
//-------------------------------------------------------------------------
//
// Get this component cursor
//
//-------------------------------------------------------------------------
nsCursor nsBaseWidget::GetCursor()
{
   return mCursor;
}

NS_METHOD nsBaseWidget::SetCursor(nsCursor aCursor)
{
  mCursor = aCursor; 
  return NS_OK;
}
    
//-------------------------------------------------------------------------
//
// Create a rendering context from this nsBaseWidget
//
//-------------------------------------------------------------------------
nsIRenderingContext* nsBaseWidget::GetRenderingContext()
{
  nsRect  bounds;
  nsIRenderingContext *renderingCtx = NULL;
  nsresult  res;

  static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
  static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);

  res = nsRepository::CreateInstance(kRenderingContextCID, nsnull, kRenderingContextIID, (void **)&renderingCtx);

  if (NS_OK == res)
    renderingCtx->Init(mContext, this);

  NS_ASSERTION(NULL != renderingCtx, "Null rendering context");
  
  return renderingCtx;
}

//-------------------------------------------------------------------------
//
// Return the toolkit this widget was created on
//
//-------------------------------------------------------------------------
nsIToolkit* nsBaseWidget::GetToolkit()
{
  NS_IF_ADDREF(mToolkit);
  return mToolkit;
}


//-------------------------------------------------------------------------
//
// Return the used device context
//
//-------------------------------------------------------------------------
nsIDeviceContext* nsBaseWidget::GetDeviceContext() 
{
  NS_IF_ADDREF(mContext);
  return mContext; 
}

//-------------------------------------------------------------------------
//
// Return the App Shell
//
//-------------------------------------------------------------------------

nsIAppShell *nsBaseWidget::GetAppShell()
{
    NS_IF_ADDREF(mAppShell);
    return mAppShell;
}


//-------------------------------------------------------------------------
//
// Destroy the window
//
//-------------------------------------------------------------------------
void nsBaseWidget::OnDestroy()
{
    // release references to children, device context, toolkit, and app shell
    NS_IF_RELEASE(mChildren);
    NS_IF_RELEASE(mContext);
    NS_IF_RELEASE(mToolkit);
    NS_IF_RELEASE(mAppShell);
}


//-------------------------------------------------------------------------
//
// Constructor
//
//-------------------------------------------------------------------------

nsBaseWidget::Enumerator::Enumerator()
{
  mRefCnt = 1;
  mCurrentPosition = 0;
}


//-------------------------------------------------------------------------
//
// Destructor
//
//-------------------------------------------------------------------------
nsBaseWidget::Enumerator::~Enumerator()
{   
  // We add ref'd when adding the child widgets, so we need to release
  // the references now
  for (PRInt32 i = 0; i < mChildren.Count(); i++) {
    nsIWidget*  widget = (nsIWidget*)mChildren.ElementAt(i);

    NS_ASSERTION(nsnull != widget, "null widget pointer");
    NS_IF_RELEASE(widget);
  }
}


//enumerator interfaces
nsresult
nsBaseWidget::Enumerator::Next()
{
  if (mCurrentPosition < (mChildren.Count() -1) )
    mCurrentPosition ++;
  else
    return NS_ERROR_FAILURE;
  return NS_OK;
}


 
nsresult
nsBaseWidget::Enumerator::Prev()
{
  if (mCurrentPosition > 0 )
    mCurrentPosition --;
  else
    return NS_ERROR_FAILURE;
  return NS_OK;
}



nsresult
nsBaseWidget::Enumerator::CurrentItem(nsISupports **aItem)
{

  if (!aItem)
    return NS_ERROR_NULL_POINTER;
  if (mCurrentPosition >= 0 && mCurrentPosition < mChildren.Count() ) {
    nsIWidget*  widget = (nsIWidget*)mChildren.ElementAt(mCurrentPosition);
    NS_IF_ADDREF(widget);
    *aItem = (nsISupports *)widget;
  }
  else
    return NS_ERROR_FAILURE;

  return NS_OK;
}



nsresult
nsBaseWidget::Enumerator::First()
{
  if (mChildren.Count()) {
    mCurrentPosition = 0;
    return NS_OK;
  }
  else
    return NS_ERROR_FAILURE;

  return NS_OK;
}



nsresult
nsBaseWidget::Enumerator::Last()
{
  if (mChildren.Count() ) {
    mCurrentPosition = mChildren.Count()-1;
    return NS_OK;
  }
  else
    return NS_ERROR_FAILURE;

  return NS_OK;
}



nsresult
nsBaseWidget::Enumerator::IsDone()
{
  if ((mCurrentPosition == (mChildren.Count() -1)) || mChildren.Count() <= 0 ){ //empty lists always return done
    return NS_OK;
  }
  else {
    return NS_COMFALSE;
  }
  return NS_OK;
}




//-------------------------------------------------------------------------
//
// Append an element 
//
//-------------------------------------------------------------------------
void nsBaseWidget::Enumerator::Append(nsIWidget* aWidget)
{
  NS_PRECONDITION(aWidget, "Null widget");
  if (nsnull != aWidget) {
    mChildren.AppendElement(aWidget);
    NS_ADDREF(aWidget);
  }
}


//-------------------------------------------------------------------------
//
// Remove an element 
//
//-------------------------------------------------------------------------
void nsBaseWidget::Enumerator::Remove(nsIWidget* aWidget)
{
  if (mChildren.RemoveElement(aWidget)) {
    NS_RELEASE(aWidget);
  }
}

NS_METHOD nsBaseWidget::SetBorderStyle(nsBorderStyle aBorderStyle) 
{
  mBorderStyle = aBorderStyle; 
  return NS_OK;
} 

NS_METHOD nsBaseWidget::SetTitle(const nsString& aTitle) 
{
  return NS_OK;
} 

/**
* Processes a mouse pressed event
*
**/
NS_METHOD nsBaseWidget::AddMouseListener(nsIMouseListener * aListener)
{
  NS_PRECONDITION(mMouseListener == nsnull, "Null mouse listener");
  mMouseListener = aListener;
  return NS_OK;
}

/**
* Processes a mouse pressed event
*
**/
NS_METHOD nsBaseWidget::AddEventListener(nsIEventListener * aListener)
{
  NS_PRECONDITION(mEventListener == nsnull, "Null mouse listener");
  mEventListener = aListener;
  return NS_OK;
}

/**
* Processes a menu event
*
**/
/*
NS_METHOD nsBaseWidget::AddMenuListener(nsIMenuListener * aListener)
{
  NS_PRECONDITION(mMenuListener == nsnull, "Null menu listener");
  mMenuListener = aListener;
  return NS_OK;
}
*/

/**
* If the implementation of nsWindow supports borders this method MUST be overridden
*
**/
NS_METHOD nsBaseWidget::GetClientBounds(nsRect &aRect)
{
  return GetBounds(aRect);
}

/**
* If the implementation of nsWindow supports borders this method MUST be overridden
*
**/
NS_METHOD nsBaseWidget::GetBounds(nsRect &aRect)
{
  aRect = mBounds;
  return NS_OK;
}

/**
* If the implementation of nsWindow supports borders this method MUST be overridden
*
**/
NS_METHOD nsBaseWidget::GetBoundsAppUnits(nsRect &aRect, float aAppUnits)
{
  aRect = mBounds;
  // Convert to twips
  aRect.x      = nscoord((PRFloat64)aRect.x * aAppUnits);
  aRect.y      = nscoord((PRFloat64)aRect.y * aAppUnits);
  aRect.width  = nscoord((PRFloat64)aRect.width * aAppUnits); 
  aRect.height = nscoord((PRFloat64)aRect.height * aAppUnits);
  return NS_OK;
}

/**
* 
*
**/
NS_METHOD nsBaseWidget::SetBounds(const nsRect &aRect)
{
  mBounds = aRect;

  return NS_OK;
}



/**
* Calculates the border width and height  
*
**/
NS_METHOD nsBaseWidget::GetBorderSize(PRInt32 &aWidth, PRInt32 &aHeight)
{
  nsRect rectWin;
  nsRect rect;
  GetBounds(rectWin);
  GetClientBounds(rect);

  aWidth  = (rectWin.width - rect.width) / 2;
  aHeight = (rectWin.height - rect.height) / 2;

  return NS_OK;
}


/**
* Calculates the border width and height  
*
**/
void nsBaseWidget::DrawScaledRect(nsIRenderingContext& aRenderingContext, const nsRect & aRect, float aScale, float aAppUnits)
{
  nsRect rect = aRect;

  float x = (float)rect.x;
  float y = (float)rect.y;
  float w = (float)rect.width;
  float h = (float)rect.height;
  float twoAppUnits = aAppUnits * 2.0f;

  for (int i=0;i<int(aScale);i++) {
    rect.x      = nscoord(x);
    rect.y      = nscoord(y);
    rect.width  = nscoord(w);
    rect.height = nscoord(h);
    aRenderingContext.DrawRect(rect);
    x += aAppUnits; 
    y += aAppUnits;
    w -= twoAppUnits; 
    h -= twoAppUnits;
  }
}

/**
* Calculates the border width and height  
*
**/
void nsBaseWidget::DrawScaledLine(nsIRenderingContext& aRenderingContext, 
                                  nscoord aSX, 
                                  nscoord aSY, 
                                  nscoord aEX, 
                                  nscoord aEY, 
                                  float   aScale, 
                                  float   aAppUnits,
                                  PRBool  aIsHorz)
{
  float sx = (float)aSX;
  float sy = (float)aSY;
  float ex = (float)aEX;
  float ey = (float)aEY;
  float twoAppUnits = aAppUnits * 2.0f;

  for (int i=0;i<int(aScale);i++) {
    aSX = nscoord(sx);
    aSY = nscoord(sy);
    aEX = nscoord(ex);
    aEY = nscoord(ey);
    aRenderingContext.DrawLine(aSX, aSY, aEX, aEY);
    if (aIsHorz) {
      sy += aAppUnits; 
      ey += aAppUnits;
    } else {
      sx += aAppUnits; 
      ex += aAppUnits;
    }
  }
}

/**
* Paints default border (XXX - this should be done by CSS)
*
**/
NS_METHOD nsBaseWidget::Paint(nsIRenderingContext& aRenderingContext,
                              const nsRect&        aDirtyRect)
{
  nsRect rect;
  float  appUnits;
  float  scale;
  nsIDeviceContext * context;
  aRenderingContext.GetDeviceContext(context);

  context->GetCanonicalPixelScale(scale);
  context->GetDevUnitsToAppUnits(appUnits);

  GetBoundsAppUnits(rect, appUnits);
  aRenderingContext.SetColor(NS_RGB(0,0,0));

  DrawScaledRect(aRenderingContext, rect, scale, appUnits);

  NS_RELEASE(context);
  return NS_OK;
}


