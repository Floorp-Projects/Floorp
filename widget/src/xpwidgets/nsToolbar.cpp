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

//
// pinkerton ToDo:
// - remove dependence on toolbar manager and grippy
// - make this talk to DOM for its children.
// - rip out nsIToolbar stuff
//

#include "nsToolbar.h"
#include "nsHTToolbarDataModel.h"
#include "nsWidgetsCID.h"
#include "nspr.h"
#include "nsIWidget.h"
#include "nsIImageButton.h"
#include "nsIToolbarItemHolder.h"
#include "nsImageButton.h"
#include "nsRepository.h"
#include "nsIDeviceContext.h"
#include "nsCOMPtr.h"


static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCToolbarCID,  NS_TOOLBAR_CID);
static NS_DEFINE_IID(kCIToolbarIID, NS_ITOOLBAR_IID);
static NS_DEFINE_IID(kIToolbarIID, NS_ITOOLBAR_IID);

#if GRIPPYS_NOT_WIDGETS
#define TAB_WIDTH  9
#define TAB_HEIGHT 42
#endif

const PRInt32 gMaxInfoItems = 32;

NS_IMPL_ADDREF(nsToolbar)
NS_IMPL_RELEASE(nsToolbar)

static NS_DEFINE_IID(kIImageButtonIID, NS_IIMAGEBUTTON_IID);
static NS_DEFINE_IID(kImageButtonCID, NS_IMAGEBUTTON_CID);
static NS_DEFINE_IID(kIToolbarItemIID, NS_ITOOLBARITEM_IID);
static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);

static NS_DEFINE_IID(kIToolbarItemHolderIID, NS_ITOOLBARITEMHOLDER_IID);
static NS_DEFINE_IID(kToolbarItemHolderCID, NS_TOOLBARITEMHOLDER_CID);
static NS_DEFINE_IID(kIImageButtonListenerIID, NS_IIMAGEBUTTONLISTENER_IID);

static NS_DEFINE_IID(kIContentConnectorIID, NS_ICONTENTCONNECTOR_IID);





//------------------------------------------------------------
class ToolbarLayoutInfo {
public:
  nsCOMPtr<nsIToolbarItem> mItem;
  PRInt32          mGap;
  PRBool           mStretchable;

  ToolbarLayoutInfo(nsIToolbarItem * aItem, PRInt32 aGap, PRBool isStretchable) 
  {
    mItem = aItem;
    mGap  = aGap;
    mStretchable = isStretchable;
  }

};



//--------------------------------------------------------------------
//-- nsToolbar Constructor
//--------------------------------------------------------------------
nsToolbar::nsToolbar() : nsDataModelWidget(), nsIToolbar(),
	mDataModel(new nsHTToolbarDataModel)
{
  NS_INIT_REFCNT();

  mMargin     = 0;
  mWrapMargin = 15;
  mHGap       = 0;
  mVGap       = 0;

  mBorderType               = eToolbarBorderType_partial;
  mLastItemIsRightJustified = PR_FALSE;
  mNextLastItemIsStretchy   = PR_FALSE;
  mWrapItems                = PR_FALSE;
  mDoHorizontalLayout       = PR_TRUE;

#if GRIPPYS_NOT_WIDGETS
  mToolbarMgr = nsnull;
#endif

  //mItemDeque = new nsDeque(gItemInfoKiller);
  mItems = (ToolbarLayoutInfo **) new PRInt32[gMaxInfoItems];
  mNumItems = 0;
}

//--------------------------------------------------------------------
nsToolbar::~nsToolbar()
{
  delete mDataModel;
  
#if GRIPPYS_NOT_WIDGETS
  NS_IF_RELEASE(mToolbarMgr);
#endif

  PRInt32 i;
  for (i=0;i<mNumItems;i++) {
    delete mItems[i];
  }
  delete[] mItems;
}

//--------------------------------------------------------------------
nsresult nsToolbar::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{
  nsresult retval = NS_OK;
                                                                 
  if (NULL == aInstancePtr) {                                            
    retval = NS_ERROR_NULL_POINTER;
  }                                                                      
  else if (aIID.Equals(kCIToolbarIID)) {                                          
    *aInstancePtr = (void*) (nsIToolbar *)this;                                        
    AddRef();                                                            
  }   
  else if (aIID.Equals(kIContentConnectorIID)) {                                          
    *aInstancePtr = (void*) (nsIContentConnector *)this;                                        
    AddRef();                                                           
  }     
  else if (aIID.Equals(kIToolbarItemIID)) {                                          
    *aInstancePtr = (void*) (nsIToolbarItem *)this;                                        
    AddRef();                                                            
  }                                                                        
  else
   retval = nsDataModelWidget::QueryInterface(aIID, aInstancePtr);
  
  return retval;
}


static nsEventStatus PR_CALLBACK
HandleToolbarEvent(nsGUIEvent *aEvent)
{
  nsEventStatus result = nsEventStatus_eIgnore;
  nsCOMPtr<nsIContentConnector> toolbar ( aEvent->widget );
  if ( toolbar )
    result = toolbar->HandleEvent(aEvent);
  return result;
}


//
// Create
//
// Override to setup event listeners at widget creation time.
//
NS_METHOD
nsToolbar :: Create(nsIWidget *aParent,
                    const nsRect &aRect,
                    EVENT_CALLBACK aHandleEventFunction,
                    nsIDeviceContext *aContext,
                    nsIAppShell *aAppShell,
                    nsIToolkit *aToolkit,
                    nsWidgetInitData *aInitData)
{
  nsresult answer = ChildWindow::Create(aParent, aRect,
     aHandleEventFunction ? aHandleEventFunction : HandleToolbarEvent,
     aContext, aAppShell, aToolkit, aInitData);

  if (mDataModel)
	mDataModel->SetDataModelListener(this);

  return answer;
  
} // Create


//
// SetContentRoot
//
// Hook up the toolbar to the content model rooted at the given node
//
NS_METHOD
nsToolbar::SetContentRoot(nsIContent* pContent)
{
	if (mDataModel)
		mDataModel->SetContentRoot(pContent);

	return NS_OK;
}


//--------------------------------------------------------------------
NS_METHOD nsToolbar::AddItem(nsIToolbarItem* anItem, PRInt32 aLeftGap, PRBool aStretchable)
{
  mItems[mNumItems++] = new ToolbarLayoutInfo(anItem, aLeftGap, aStretchable);
  return NS_OK;    
}

//--------------------------------------------------------------------
NS_METHOD nsToolbar::InsertItemAt(nsIToolbarItem* anItem, 
                                  PRInt32         aLeftGap, 
                                  PRBool          aStretchable, 
                                  PRInt32         anIndex)
{

  if ((anIndex < 0 || anIndex > mNumItems-1) && !(anIndex == 0 && mNumItems == 0)) {
    return NS_ERROR_FAILURE;
  }

  if (mNumItems > 0) {
    // Shift them down to make room
    PRInt32 i;
    PRInt32 downToInx = anIndex + 1;
    for (i=mNumItems;i>downToInx;i--) {
      mItems[i] = mItems[i-1];
      
    }

    // Insert the new widget
    mItems[downToInx] = new ToolbarLayoutInfo(anItem, aLeftGap, aStretchable);
  } else {
    mItems[0] = new ToolbarLayoutInfo(anItem, aLeftGap, aStretchable);
  }
    mNumItems++;

  return NS_OK;    
}
//--------------------------------------------------------------------
NS_METHOD nsToolbar::GetItemAt(nsIToolbarItem*& anItem, PRInt32 anIndex)
{
  if (anIndex < 0 || anIndex > mNumItems-1) {
    anItem = nsnull;
    return NS_ERROR_FAILURE;
  }

  anItem = mItems[anIndex]->mItem;
  NS_ADDREF(anItem);
  return NS_OK;    
}

//--------------------------------------------------------------------
NS_METHOD nsToolbar::DoLayout()
{
  nsRect tbRect;
  nsWindow::GetBounds(tbRect);

  if (mDoHorizontalLayout) {
    DoHorizontalLayout(tbRect);
  } else {
    DoVerticalLayout(tbRect);
  }
  return NS_OK;
}

//--------------------------------------------------------------------
NS_METHOD nsToolbar::SetHorizontalLayout(PRBool aDoHorizontalLayout)
{
  mDoHorizontalLayout = aDoHorizontalLayout;
  return NS_OK;
}

//--------------------------------------------------------------------
void nsToolbar::GetMargins(PRInt32 &aX, PRInt32 &aY)
{

  switch (mBorderType) {
    case eToolbarBorderType_none:
      aX = 0;
      aY = 0;
      break;

    case eToolbarBorderType_partial:
      aX = 0;
      aY = mMargin;
      break;

    case eToolbarBorderType_full:
      aX = mMargin;
      aY = mMargin;
      break;

    default:
      aX = 0;
      aY = 0;
  } // switch 
}

//--------------------------------------------------------------------
void nsToolbar::DoVerticalLayout(const nsRect& aTBRect)
{
  PRInt32 i;
  PRInt32 x;
  PRInt32 y;

  GetMargins(x, y);

  PRInt32 maxWidth = 0;

  // First layout all the items
  for (i=0;i<mNumItems;i++) {
    PRBool isVisible;
    mItems[i]->mItem->IsVisible(isVisible);
    if (isVisible) {
      PRInt32 width, height;

      if (NS_OK != mItems[i]->mItem->GetPreferredSize(width, height)) {
        nsRect rect;
        mItems[i]->mItem->GetBounds(rect);
        width  = rect.width;
        height = rect.height;
      }
      if (!mItems[i]->mStretchable) {
        maxWidth = maxWidth > height? maxWidth:height;
      }
       
      if (((y + height + mItems[i]->mGap) > aTBRect.height) && mWrapItems) {
        y = mWrapMargin; 
        x += maxWidth;
        maxWidth = 0;
      }

      PRInt32 xLoc;
      if (mWrapItems) {
        xLoc = x;
      } else {
        xLoc = ((aTBRect.width - width) / 2);
        xLoc = xLoc > -1 ? xLoc : mMargin;
      }
      // Gap is added before hand because it is the left hand gap
      y += mItems[i]->mGap;
      mItems[i]->mItem->SetBounds(xLoc, y, width, height, PR_FALSE);
      y += width;
    }
  }

  // Right justify the last item
  PRBool rightGotJustified = PR_FALSE;

  if (mNumItems > 1 && mLastItemIsRightJustified) {
    PRInt32 index = mNumItems-1;
    PRBool isVisible;
    mItems[index]->mItem->IsVisible(isVisible);
    if (isVisible) {
      PRInt32 width, height;
      if (NS_OK != mItems[index]->mItem->GetPreferredSize(width, height)) {
        nsRect rect;
        mItems[index]->mItem->GetBounds(rect);
        width  = rect.width;
        height = rect.height;
      }
      PRInt32 yLoc = aTBRect.height - height - mItems[index]->mGap - mMargin;
      PRInt32 xLoc;
      if (mWrapItems) {
        xLoc = x;
      } else {
        xLoc = (aTBRect.width - width) / 2;
        xLoc = xLoc > -1 ? xLoc : mMargin;
      }
      mItems[index]->mItem->SetBounds(xLoc, yLoc, width, height, PR_FALSE);
      rightGotJustified = PR_TRUE;
    }
  }

  // Make the next to the last item strechy
  if (mNumItems > 1 && mNextLastItemIsStretchy) {
    PRInt32 lastIndex     = mNumItems-1;
    PRInt32 nextLastIndex = mNumItems-2;

    if (!rightGotJustified) { // last item is not visible, so stretch to end
      nsRect nextLastRect;
      mItems[nextLastIndex]->mItem->GetBounds(nextLastRect);
      nextLastRect.height = aTBRect.height - nextLastRect.y - mMargin;
      mItems[nextLastIndex]->mItem->SetBounds(nextLastRect.x, nextLastRect.y, nextLastRect.width, nextLastRect.height, PR_TRUE);
    } else {

      PRBool isVisible;
      mItems[nextLastIndex]->mItem->IsVisible(isVisible);
      if (isVisible) { // stretch if visible
        nsRect lastRect;
        nsRect nextLastRect;

        mItems[lastIndex]->mItem->GetBounds(lastRect);
        mItems[nextLastIndex]->mItem->GetBounds(nextLastRect);

        nextLastRect.height = lastRect.y - nextLastRect.y - mItems[lastIndex]->mGap;
        mItems[nextLastIndex]->mItem->SetBounds(nextLastRect.x, nextLastRect.y, nextLastRect.width, nextLastRect.height, PR_TRUE);
      }
    }
  }

  for (i=0;i<mNumItems;i++) {
    if (mItems[i]->mStretchable) {
      nsRect rect;
      mItems[i]->mItem->GetBounds(rect);
      mItems[i]->mItem->SetBounds(rect.x, rect.y, rect.width, y+maxWidth, PR_TRUE);
    } else {
      mItems[i]->mItem->Repaint(PR_TRUE);
    }
  }

  Invalidate(PR_TRUE); // repaint toolbar
}

//--------------------------------------------------------------------
void nsToolbar::DoHorizontalLayout(const nsRect& aTBRect)
{
  PRInt32 i;
  PRInt32 x;
  PRInt32 y;

  GetMargins(x, y);

  PRInt32 maxHeight = 0;

  // First layout all the items
  for (i=0;i<mNumItems;i++) {
    if (i == 10) {
      int x = 0;
    }
    PRBool isVisible;
    mItems[i]->mItem->IsVisible(isVisible);
    if (isVisible) {
      PRInt32 width, height;

      if (NS_OK != mItems[i]->mItem->GetPreferredSize(width, height)) {
        nsRect rect;
        mItems[i]->mItem->GetBounds(rect);
        width  = rect.width;
        height = rect.height;
      }
       
      if (((x + width + mItems[i]->mGap) > aTBRect.width) && mWrapItems) {
        x = mMargin + mWrapMargin; 
        y += maxHeight;
        maxHeight = 0;
      }
      if (!mItems[i]->mStretchable) {
        maxHeight = maxHeight > height? maxHeight:height;
      }

      PRInt32 yLoc;
      if (mWrapItems) {
        yLoc = y;
      } else {
        yLoc = ((aTBRect.height - height) / 2);
        yLoc = yLoc > -1 ? yLoc : mMargin;
      }
      // Gap is added before hand because it is the left hand gap
      // Don't set the bounds on the last item if it is right justified
      x += mItems[i]->mGap;
      if (((i == (mNumItems-1) && !mLastItemIsRightJustified)) || (i != (mNumItems-1))) {
        mItems[i]->mItem->SetBounds(x, yLoc, width, height, PR_FALSE);
      }
      x += width;
    }
  }

  // Right justify the last item
  PRBool rightGotJustified = PR_FALSE;

  if (mNumItems > 1 && mLastItemIsRightJustified) {
    PRInt32 index = mNumItems-1;
    PRBool isVisible;
    mItems[index]->mItem->IsVisible(isVisible);
    if (isVisible) {
      PRInt32 width, height;
      if (NS_OK != mItems[index]->mItem->GetPreferredSize(width, height)) {
        nsRect rect;
        mItems[index]->mItem->GetBounds(rect);
        width  = rect.width;
        height = rect.height;
      }
      PRInt32 xLoc = aTBRect.width - width - mItems[index]->mGap - mMargin;
      PRInt32 yLoc;
      if (mWrapItems) {
        yLoc = y;
      } else {
        yLoc = (aTBRect.height - height) / 2;
        yLoc = yLoc > -1 ? yLoc : mMargin;
      }
      mItems[index]->mItem->SetBounds(xLoc, yLoc, width, height, PR_FALSE);
      rightGotJustified = PR_TRUE;
    }
  }

  // Make the next to the last item strechy
  if (mNumItems > 1 && mNextLastItemIsStretchy) {
    PRInt32 lastIndex     = mNumItems-1;
    PRInt32 nextLastIndex = mNumItems-2;

    if (!rightGotJustified) { // last item is not visible, so stretch to end
      nsRect nextLastRect;
      mItems[nextLastIndex]->mItem->GetBounds(nextLastRect);
      nextLastRect.width = aTBRect.width - nextLastRect.x - mMargin;
      mItems[nextLastIndex]->mItem->SetBounds(nextLastRect.x, nextLastRect.y, nextLastRect.width, nextLastRect.height, PR_TRUE);
    } else {

      PRBool isVisible;
      mItems[nextLastIndex]->mItem->IsVisible(isVisible);
      if (isVisible) { // stretch if visible
        nsRect lastRect;
        nsRect nextLastRect;

        mItems[lastIndex]->mItem->GetBounds(lastRect);
        mItems[nextLastIndex]->mItem->GetBounds(nextLastRect);

        nextLastRect.width = lastRect.x - nextLastRect.x - mItems[lastIndex]->mGap;
        mItems[nextLastIndex]->mItem->SetBounds(nextLastRect.x, nextLastRect.y, nextLastRect.width, nextLastRect.height, PR_TRUE);
      }
    }
  }

  for (i=0;i<mNumItems;i++) {
    if (mItems[i]->mStretchable) {
      nsRect rect;
      mItems[i]->mItem->GetBounds(rect);
      mItems[i]->mItem->SetBounds(rect.x, rect.y, rect.width, y+maxHeight, PR_TRUE);
    } else {
      mItems[i]->mItem->Repaint(PR_TRUE);
    }
  }

  Invalidate(PR_TRUE); // repaint toolbar
}

//--------------------------------------------------------------------
NS_METHOD nsToolbar::SetLastItemIsRightJustified(const PRBool & aState)
{
  mLastItemIsRightJustified = aState;
  return NS_OK;    
}

//--------------------------------------------------------------------
NS_METHOD nsToolbar::SetNextLastItemIsStretchy(const PRBool & aState)
{
  mNextLastItemIsStretchy = aState;
  return NS_OK;    
}

//--------------------------------------------------------------------
NS_METHOD nsToolbar::GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight)
{
  nsRect rect;
  nsWindow::GetBounds(rect);

  if (mDoHorizontalLayout) {
    aWidth  = mMargin*2;
    aHeight = 0;
    PRInt32 i;
    for (i=0;i<mNumItems;i++) {
      PRBool isVisible;
      mItems[i]->mItem->IsVisible(isVisible);
      if (isVisible) {
        PRInt32 width;
        PRInt32 height;
        if (NS_OK == mItems[i]->mItem->GetPreferredSize(width, height)) {
          aWidth += width + mItems[i]->mGap;
          if (!mItems[i]->mStretchable) {
            aHeight = height > aHeight? height : aHeight;
          }
        } else {
          nsRect rect;
          mItems[i]->mItem->GetBounds(rect);
          aWidth += rect.width + mItems[i]->mGap;
          if (!mItems[i]->mStretchable) {
            aHeight = rect.height > aHeight? rect.height : aHeight;
          }
        }
      }
    }
    aWidth += mHGap;

    if (aHeight == 0) {
      aHeight = 32;
    }
    aHeight += (mMargin*2);
  } else {
    aHeight = mMargin*2;
    aWidth  = 0;
    PRInt32 i;
    for (i=0;i<mNumItems;i++) {
      PRBool isVisible;
      mItems[i]->mItem->IsVisible(isVisible);
      if (isVisible) {
        PRInt32 width;
        PRInt32 height;
        if (NS_OK == mItems[i]->mItem->GetPreferredSize(width, height)) {
          aHeight += height + mItems[i]->mGap;
          if (!mItems[i]->mStretchable) {
            aWidth = width > aWidth? width : aWidth;
          }
        } else {
          nsRect rect;
          mItems[i]->mItem->GetBounds(rect);
          aHeight += rect.height + mItems[i]->mGap;
          if (!mItems[i]->mStretchable) {
            aWidth = rect.width > aWidth? rect.width : aWidth;
          }
        }
      }
    }
    aHeight += mHGap;

    if (aWidth == 0) {
      aWidth = 32;
    }
  }

  return NS_OK;
}

//--------------------------------------------------------------------
NS_METHOD nsToolbar::SetHGap(PRInt32 aGap)
{
  mHGap = aGap;
  return NS_OK;    
}

//--------------------------------------------------------------------
NS_METHOD nsToolbar::SetVGap(PRInt32 aGap)
{
  mVGap = aGap;
  return NS_OK;    
}

//--------------------------------------------------------------------
NS_METHOD nsToolbar::SetMargin(PRInt32 aMargin)
{
  mMargin = aMargin;
  return NS_OK;    
}

//--------------------------------------------------------------------
NS_METHOD nsToolbar::SetBorderType(nsToolbarBorderType aBorderType)
{
  mBorderType = aBorderType;
  return NS_OK;
}

#if GRIPPYS_NOT_WIDGETS

//--------------------------------------------------------------------
NS_METHOD nsToolbar::SetToolbarManager(nsIToolbarManager * aToolbarManager)
{
  mToolbarMgr = aToolbarManager;
  NS_ADDREF(mToolbarMgr);

  return NS_OK;
}

//--------------------------------------------------------------------
NS_METHOD nsToolbar::GetToolbarManager(nsIToolbarManager *& aToolbarManager)
{
  aToolbarManager = mToolbarMgr;
  NS_ADDREF(aToolbarManager);
  return NS_OK;
}

#endif

//--------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_METHOD nsToolbar::Resize(PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint)
{
  nsresult result = nsWindow::Resize(aWidth, aHeight, aRepaint);
  DoLayout();
  return result;
}

    
//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_METHOD nsToolbar::Resize(PRUint32 aX,
                      PRUint32 aY,
                      PRUint32 aWidth,
                      PRUint32 aHeight,
                      PRBool   aRepaint)
{
  nsresult result = nsWindow::Resize(aX, aY, aWidth, aHeight, aRepaint);
  DoLayout();
  return result;
}

//-------------------------------------------------------------------------
NS_METHOD nsToolbar::Repaint(PRBool aIsSynchronous)

{
  Invalidate(aIsSynchronous);
  return NS_OK;
}
    
//--------------------------------------------------------------------
NS_METHOD nsToolbar::SetBounds(PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint)
{
  return Resize(aWidth, aHeight, aRepaint);
}
    
//-------------------------------------------------------------------------
NS_METHOD nsToolbar::SetBounds(PRUint32 aX,
                                          PRUint32 aY,
                                          PRUint32 aWidth,
                                          PRUint32 aHeight,
                                          PRBool   aRepaint)
{
  return Resize(aX, aY, aWidth, aHeight, aRepaint);
}

//-------------------------------------------------------------------------
NS_METHOD nsToolbar::SetVisible(PRBool aState) 
{
  nsWindow::Show(aState);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsToolbar::SetLocation(PRUint32 aX, PRUint32 aY) 
{
  nsWindow::Move(aX, aY);
  return NS_OK;
}


//-------------------------------------------------------------------------
NS_METHOD nsToolbar::IsVisible(PRBool & aState) 
{
  nsWindow::IsVisible(aState);
  return NS_OK;
}

//--------------------------------------------------------------------
NS_METHOD nsToolbar::SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight)
{
  nsWindow::SetPreferredSize(aWidth, aHeight);
  return NS_OK;
}

//-------------------------------------------------------------------
NS_METHOD nsToolbar::GetBounds(nsRect & aRect)
{
  nsWindow::GetBounds(aRect);
  return NS_OK;
}

//-----------------------------------------------------------------------------
nsEventStatus nsToolbar::OnPaint(nsIRenderingContext& aRenderingContext,
                                 const nsRect& aDirtyRect)
{
  nsresult res = NS_OK;
  nsRect r = aDirtyRect;

  aRenderingContext.SetColor(GetBackgroundColor());
  aRenderingContext.FillRect(r);
  r.width--;

  nsCOMPtr<nsIDeviceContext> dc ( dont_AddRef(GetDeviceContext()) );
  if ( !dc )
    return nsEventStatus_eIgnore;
    
  nsFont titleBarFont("MS Sans Serif", NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
					  400, NS_FONT_DECORATION_NONE,
					  12);
  nsBasicStyleInfo styleInfo(titleBarFont);
  mDataModel->GetToolbarStyle(dc, styleInfo);
  
  // If there is a background image, draw it.
  if ( styleInfo.BackgroundImage() )
    PaintBackgroundImage(aRenderingContext, styleInfo.BackgroundImage(), r);
  
  if (mBorderType != eToolbarBorderType_none) 
  {
    nsRect rect(r);
    // draw top & left
    aRenderingContext.SetColor(NS_RGB(255,255,255));
    aRenderingContext.DrawLine(0,0,rect.width,0);
    if (mBorderType == eToolbarBorderType_full) {
      aRenderingContext.DrawLine(0,0,0,rect.height);
    }

    // draw bottom & right
    aRenderingContext.SetColor(NS_RGB(128,128,128));
    aRenderingContext.DrawLine(0,rect.height-1,rect.width,rect.height-1);
    if (mBorderType == eToolbarBorderType_full) {
      aRenderingContext.DrawLine(rect.width,0,rect.width,rect.height);
    }
  }

  return nsEventStatus_eIgnore;
}

//-----------------------------------------------------------------------------
nsEventStatus nsToolbar::HandleEvent(nsGUIEvent *aEvent) 
{

  if (aEvent->message == NS_PAINT) 
  {
    nsRect r;
    aEvent->widget->GetBounds(r);
    r.x = 0;
    r.y = 0;
    nsCOMPtr<nsIRenderingContext> drawCtx(NS_STATIC_CAST(nsPaintEvent*, aEvent)->renderingContext);
    return (OnPaint(*drawCtx,r));
  }

  return nsEventStatus_eIgnore;
  
}


//-------------------------------------------------------------------
void nsToolbar::HandleDataModelEvent(int anEvent, nsHierarchicalDataItem* pItem)
{
	Invalidate(PR_FALSE);
}


//-------------------------------------------------------------------
NS_METHOD nsToolbar::SetWrapping(PRBool aDoWrap)
{
  mWrapItems = aDoWrap;
  return NS_OK;
}

//-------------------------------------------------------------------
NS_METHOD nsToolbar::GetWrapping(PRBool & aDoWrap)
{
  aDoWrap = mWrapItems;
  return NS_OK;
}

//-------------------------------------------------------------------
NS_METHOD nsToolbar::GetPreferredConstrainedSize(PRInt32& aSuggestedWidth, PRInt32& aSuggestedHeight, 
                                                 PRInt32& aWidth,          PRInt32& aHeight)
{
  nsRect rect;
  nsWindow::GetBounds(rect);

  PRInt32 rows        = 1;
  PRInt32 calcSize    = mMargin;
  PRInt32 maxSize     = 0;
  PRInt32 maxRowSize  = 0;
  PRInt32 currentSize = mMargin; // the current height of the "growing" toolbar

  PRInt32 i;
  // Loop throgh each item in the toolbar
  // Skip it if it is not visible
  for (i=0;i<mNumItems;i++) {
    PRBool isVisible;
    mItems[i]->mItem->IsVisible(isVisible);
    if (isVisible) {
      PRInt32 width;
      PRInt32 height;
      // Get the item's Preferred width, height
      if (NS_OK != mItems[i]->mItem->GetPreferredSize(width, height)) {
        nsRect rect;
        mItems[i]->mItem->GetBounds(rect);
        width = rect.width;
        height = rect.height;
      }

      // If it is greater than the suggested width than add 1 to the number of rows
      // and start the x over
      if (mDoHorizontalLayout) {
        if ((calcSize + width + mItems[i]->mGap) > aSuggestedWidth) {
          currentSize += maxRowSize;
          maxRowSize = 0;
          calcSize   = mMargin + mWrapMargin + width + mItems[i]->mGap;
        } else {
          calcSize += width + mItems[i]->mGap;
        }
        if (!mItems[i]->mStretchable) {
          maxRowSize = height > maxRowSize? height : maxRowSize;
        }
      } else { // vertical
        if (calcSize + height + mItems[i]->mGap > aSuggestedHeight) {
          currentSize += maxRowSize;
          maxRowSize   = 0;
          calcSize     = mMargin;
        } else {
          calcSize += height + mItems[i]->mGap;
        }
        if (!mItems[i]->mStretchable) {
          maxRowSize = width > maxRowSize? width : maxRowSize;
        }
      }
    } // isVisible
  }

  // Now set the width and height accordingly
  if (mDoHorizontalLayout) {
    aWidth  = aSuggestedWidth;
    aHeight = currentSize + mMargin + maxRowSize;
  } else {
    aHeight  = aSuggestedHeight;
    aWidth = currentSize + mMargin + maxRowSize;
  }

  return NS_OK;
}


#if GRIPPYS_NOT_WIDGETS

//-------------------------------------------------------------------
NS_METHOD nsToolbar::CreateTab(nsIWidget *& aTab)
{
  nsresult rv;

  // Create the generic toolbar holder for the tab widget
  nsIToolbarItemHolder * toolbarItemHolder;
  rv = nsRepository::CreateInstance(kToolbarItemHolderCID, nsnull, kIToolbarItemHolderIID,
                                    (void**)&toolbarItemHolder);
  if (NS_OK != rv) {
    return rv;
  }

  // Get the ToolbarItem interface for adding it to the toolbar
  nsIToolbarItem * toolbarItem;
	if (NS_OK != toolbarItemHolder->QueryInterface(kIToolbarItemIID,(void**)&toolbarItem)) {
    NS_RELEASE(toolbarItemHolder);
    return NS_OK;
  }

  nsRect rt(0, 0, TAB_WIDTH, TAB_HEIGHT);

  nsIImageButton * tab;
  rv = nsRepository::CreateInstance(kImageButtonCID, nsnull, kIImageButtonIID,
                                    (void**)&tab);
  if (NS_OK != rv) {
    NS_RELEASE(toolbarItem);
    NS_RELEASE(toolbarItemHolder);
    return rv;
  }
  
  // Get the nsIWidget interface for the tab so it can be added to the parent widget
  // and it can be put into the generic ToolbarItemHolder
  nsIWidget * widget = nsnull;
	if (NS_OK == tab->QueryInterface(kIWidgetIID,(void**)&widget)) {
	  widget->Create(this, rt, NULL, NULL);
	  widget->Show(PR_TRUE);
	  widget->SetClientData(NS_STATIC_CAST(void*, this));

	  widget->Resize(0, 1, TAB_WIDTH, TAB_HEIGHT, PR_FALSE);

    toolbarItemHolder->SetWidget(widget); // put the widget into the holder

    tab->SetBorderWidth(0);
    tab->SetBorderOffset(0);
    tab->SetShowBorder(PR_FALSE);
    tab->SetShowText(PR_FALSE);
    tab->SetLabel("");
    tab->SetImageDimensions(rt.width, rt.height);

    nsString up, pres, dis, roll;
    mToolbarMgr->GetCollapseTabURLs(up, pres, dis, roll);
    tab->SetImageURLs(up, pres, dis, roll);
    InsertItemAt(toolbarItem, 0, PR_TRUE, 0);  // add the item with zero gap, stretchable, zero position

    nsIImageButtonListener * listener = nsnull;
    if (NS_OK == mToolbarMgr->QueryInterface(kIImageButtonListenerIID,(void**)&listener)) {
      tab->AddListener(listener);
      NS_RELEASE(listener);
    }
        
    aTab = widget;
	}
  NS_RELEASE(tab);
  NS_RELEASE(toolbarItem);
  NS_RELEASE(toolbarItemHolder);

  return NS_OK;
}

#endif


//
// PaintBackgroundImage
//
// Given a rendering context and a bg image, this will tile the image across the
// background.
// NOTE: When the toolbar becomes a frame, we should get all this for free so this
//       code can probably go away.
//
void 
nsToolbar::PaintBackgroundImage(nsIRenderingContext& ctx,
									  nsIImage* bgImage, const nsRect& constraintRect,
								      int xSrcOffset, int ySrcOffset)
{
	// This code gets a bit intense. Will comment heavily.

	int imageWidth = bgImage->GetWidth();	// The dimensions of the background image being tiled.
	int imageHeight = bgImage->GetHeight();

	int totalWidth = constraintRect.width;   // The dimensions of the space we're
	int totalHeight = constraintRect.height; // drawing into.

	if (imageWidth <= 0 || imageHeight <= 0) // Don't draw anything if we don't have a sane image.
		return;

	int xSize = imageWidth - xSrcOffset;	// The dimensions of the actual tile we'll end
	int ySize = imageHeight - ySrcOffset;	// up drawing.  A subset of the full BG image.
	
	xSize = (xSize > totalWidth) ? totalWidth : xSize;		
	ySize = (ySize > totalHeight) ? totalHeight : ySize;

	int rightMostPoint = constraintRect.x + constraintRect.width;	// Edges of the space we're
	int bottomMostPoint = constraintRect.y + constraintRect.height; // drawing into.

	int xDstOffset = constraintRect.x;	// Top-left coordinates in the space where
	int yDstOffset = constraintRect.y;  // we'll be drawing.  Where we'll place the tile.

	int initXOffset = xSrcOffset;
	
	// Tile vertically until we move out of the constraining rect.
	while (yDstOffset < bottomMostPoint)
	{
		// Tile horizontally until we move out of the constraining rect.
		while (xDstOffset < rightMostPoint)
		{
			// Draw the subimage.  Pull the subimage from the larger image
			// and then draw it.
			ctx.DrawImage(bgImage, nsRect(xSrcOffset, ySrcOffset, xSize, ySize),
						   nsRect(xDstOffset, yDstOffset, xSize, ySize));

			// The next subimage will be as much of the full BG image as can fit in the
			// constraining rect.  If we're at the edge, we don't draw quite as much.
			xSrcOffset = 0;
			xDstOffset += xSize;
			xSize = (xDstOffset + imageWidth) > rightMostPoint ? imageWidth - (xDstOffset + imageWidth) + rightMostPoint : imageWidth;
		}

		xSrcOffset = initXOffset;	// Start of all rows will be at the same initial x offset.
		xDstOffset = constraintRect.x; // Reset our x-position for drawing the next row.
		xSize = (xDstOffset + imageWidth) > rightMostPoint ? rightMostPoint - xDstOffset : imageWidth;

		// Determine the height of the next row.  Will be as much of the BG image
		// as can fit in the constraining rect. If we're at the bottom edge, we don't
		// draw quite as much.
		ySrcOffset = 0;
		yDstOffset += ySize;
		ySize = (yDstOffset + imageHeight) > bottomMostPoint ? bottomMostPoint - yDstOffset : imageHeight;
	}
}

