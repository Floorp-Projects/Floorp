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

#include "nsIToolbar.h"
#include "nsToolbarManager.h"
#include "nsIToolbarItemHolder.h"

#include "nsIRenderingContext.h"
#include "nsWidgetsCID.h"
#include "nsIWidget.h"
#include "nsIComponentManager.h"
#include "nsImageButton.h"


static NS_DEFINE_IID(kISupportsIID,        NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCToolbarManagerCID,  NS_TOOLBARMANAGER_CID);
static NS_DEFINE_IID(kCIToolbarManagerIID, NS_ITOOLBARMANAGER_IID);
static NS_DEFINE_IID(kIWidgetIID,          NS_IWIDGET_IID);
static NS_DEFINE_IID(kIImageButtonIID,     NS_IIMAGEBUTTON_IID);
static NS_DEFINE_IID(kImageButtonCID,      NS_IMAGEBUTTON_CID);
static NS_DEFINE_IID(kIToolbarItemHolderIID, NS_ITOOLBARITEMHOLDER_IID);
static NS_DEFINE_IID(kToolbarItemHolderCID,  NS_TOOLBARITEMHOLDER_CID);
static NS_DEFINE_IID(kIToolbarItemIID,       NS_ITOOLBARITEM_IID);
static NS_DEFINE_IID(kIToolbarIID,           NS_ITOOLBAR_IID);
static NS_DEFINE_IID(kCIImageButtonListenerIID, NS_IIMAGEBUTTONLISTENER_IID);
static NS_DEFINE_IID(kIContentConnectorIID, NS_ICONTENTCONNECTOR_IID);


NS_IMPL_ADDREF(nsToolbarManager)
NS_IMPL_RELEASE(nsToolbarManager)


// *** These values need to come from the 
// images instead of being hard coded
const int EXPAND_TAB_WIDTH = 56;
const int EXPAND_TAB_HEIGHT = 10;


const PRInt32 kMaxNumToolbars = 32;


//--------------------------------------------------------------------
//-- nsToolbarManager Constructor
//--------------------------------------------------------------------
nsToolbarManager::nsToolbarManager() : ChildWindow(), nsIToolbarManager(),
		mGrippyHilighted(kNoGrippyHilighted), mNumToolbars(0), mTabsCollapsed(0)
{
  NS_INIT_REFCNT();

  // XXX Needs to be changed to a Vector class
  mToolbars = new nsCOMPtr<nsIToolbar> [kMaxNumToolbars];
}

//--------------------------------------------------------------------
nsToolbarManager::~nsToolbarManager()
{
  delete [] mToolbars;
}

//--------------------------------------------------------------------
nsresult nsToolbarManager::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  if (aIID.Equals(kCIToolbarManagerIID)) {                                          
    *aInstancePtr = (void*) (nsIToolbarManager *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kIContentConnectorIID)) {                                          
    *aInstancePtr = (void*) (nsIContentConnector *)this;                                        
    AddRef();                                                           
    return NS_OK;                                                        
  }     
  return (ChildWindow::QueryInterface(aIID, aInstancePtr));
}


//
// [static] HandleToolbarMgrEvent
//
// Static event handler function to target events to the correct widget
//
static nsEventStatus PR_CALLBACK
HandleToolbarMgrEvent ( nsGUIEvent *aEvent )
{
  nsEventStatus result = nsEventStatus_eIgnore;
  nsCOMPtr<nsIContentConnector> manager(aEvent->widget);
  if ( manager )
    result = manager->HandleEvent(aEvent);
  return result;
}


//
// Create
//
// Override to setup event listeners at widget creation time.
//
NS_METHOD
nsToolbarManager :: Create(nsIWidget *aParent,
		                    const nsRect &aRect,
		                    EVENT_CALLBACK aHandleEventFunction,
		                    nsIDeviceContext *aContext,
		                    nsIAppShell *aAppShell,
		                    nsIToolkit *aToolkit,
		                    nsWidgetInitData *aInitData)
{
  nsresult answer = ChildWindow::Create(aParent, aRect,
									     aHandleEventFunction ? aHandleEventFunction : HandleToolbarMgrEvent,
									     aContext, aAppShell, aToolkit, aInitData);
#if PINK_NOT_YET_IMPLEMENTED
  if (mDataModel)
	mDataModel->SetDataModelListener(this);
#endif

  return answer;
  
} // Create


//
// SetContentRoot
//
// Hook up the toolbar to the content model rooted at the given node
//
NS_METHOD
nsToolbarManager::SetContentRoot(nsIContent* pContent)
{
#if PINK_NOT_YET_IMPLEMENTED
	if (mDataModel)
		mDataModel->SetContentRoot(pContent);
#endif

	return NS_OK;
}


//
// HandleEvent
//
// Events come from the DOM and they wind up here. Handle them.
//
nsEventStatus
nsToolbarManager::HandleEvent(nsGUIEvent *aEvent) 
{
  if ( !aEvent )
    return nsEventStatus_eIgnore;
 
  switch ( aEvent->message ) {
    case NS_PAINT:
    {
    nsRect r;
   GetBounds(r);
    r.x = 0;
    r.y = 0;

	nsRect rect(r);

//	if (mDataModel) {
	  nsDrawingSurface ds;
      nsCOMPtr<nsIRenderingContext> ctx (NS_STATIC_CAST(nsPaintEvent*, aEvent)->renderingContext);

      ctx->CreateDrawingSurface(&r, 0, ds);
      ctx->SelectOffScreenDrawingSurface(ds);

      // fill in the bg area
      ctx->SetColor ( NS_RGB(0xDD,0xDD,0xDD) );
      ctx->FillRect ( rect );
      
      // Paint the grippy bar for each toolbar
      DrawGrippies(ctx);
      
      ctx->CopyOffScreenBits(ds, 0, 0, rect, NS_COPYBITS_USE_SOURCE_CLIP_REGION);
      ctx->DestroyDrawingSurface(ds);
//	}
    }
    break;
    
//  case NS_MOUSE_LEFT_CLICK:
    case NS_MOUSE_LEFT_BUTTON_UP:
      OnMouseLeftClick ( *NS_STATIC_CAST(nsMouseEvent*, aEvent) );
      break;
    
    case NS_MOUSE_MOVE:
      OnMouseMove ( aEvent->point );
      break;
      
    case NS_MOUSE_EXIT:
      OnMouseExit();
      break;

    default:
      break;
  } // switch on event
  
  return nsEventStatus_eIgnore;
  
} // HandleEvent



//--------------------------------------------------------------------
NS_METHOD nsToolbarManager::AddToolbar(nsIToolbar* aToolbar)
{
  mToolbars[mNumToolbars] = aToolbar;
  mTabs[mNumToolbars].mToolbar = aToolbar;  // hook this toolbar up to a grippy
  mTabs[mNumToolbars].mCollapsed = PR_FALSE;
  mNumToolbars++;
  
  return NS_OK;    
}

//--------------------------------------------------------------------
NS_METHOD nsToolbarManager::InsertToolbarAt(nsIToolbar* aToolbar, PRInt32 anIndex)
{
  if (anIndex < 0 || anIndex > mNumToolbars-1) {
    return NS_ERROR_FAILURE;
  }

  // Shift them down to make room
  PRInt32 i;
  PRInt32 downToInx = anIndex + 1;
  for (i=mNumToolbars;i>downToInx;i--) {
    mToolbars[i] = mToolbars[i-1];
  }

  // Insert the new widget
  mToolbars[downToInx] = aToolbar;
  mTabs[downToInx].mToolbar = aToolbar;  // hook this toolbar up to a grippy
  mTabs[mNumToolbars].mCollapsed = PR_FALSE;
  mNumToolbars++;

  return NS_OK;    
}


//--------------------------------------------------------------------
NS_METHOD nsToolbarManager::GetNumToolbars(PRInt32 & aNumToolbars) 
{
  aNumToolbars = mNumToolbars;
  return NS_OK;
}

//--------------------------------------------------------------------
NS_METHOD nsToolbarManager::GetToolbarAt(nsIToolbar*& aToolbar, PRInt32 anIndex) 
{
  if (anIndex < 0 || anIndex > mNumToolbars-1) {
    return NS_ERROR_FAILURE;
  }

  aToolbar = mToolbars[anIndex];
  NS_ADDREF(aToolbar);

  return NS_OK;
}


//
// CollapseToolbar
//
// Given the tab that was clicked on, collapse its corresponding toolbar. This
// assumes that the tab is expanded.
//
void
nsToolbarManager::CollapseToolbar ( TabInfo & inTab ) 
{
  nsCOMPtr<nsIWidget> widget ( inTab.mToolbar );
  if ( widget ) {
    // mark the tab as collapsed. We don't actually have to set the new
    // bounding rect because that will be done for us when the bars are
    // relaid out.
    inTab.mCollapsed = PR_TRUE;
    ++mTabsCollapsed;
    
    // hide the toolbar
    widget->Show(PR_FALSE);
    
    DoLayout();
    
    if ( mListener )
      mListener->NotifyToolbarManagerChangedSize(this);
  } 
  
} // CollapseToolbar


//
// ExpandToolbar
//
// Given the collapsed (horizontal) tab that was clicked on, expand its
// corresponding toolbar. This assumes the tab is collapsed.
//
void
nsToolbarManager::ExpandToolbar ( TabInfo & inTab ) 
{
  nsCOMPtr<nsIWidget> widget ( inTab.mToolbar );
  if ( widget ) {
    // mark the tab as expanded. We don't actually have to set the new
    // bounding rect because that will be done for us when the bars are
    // relaid out.
    inTab.mCollapsed = PR_FALSE;
    --mTabsCollapsed;
    
    // show the toolbar
    widget->Show(PR_TRUE);
    
    DoLayout();
    
    if ( mListener )
      mListener->NotifyToolbarManagerChangedSize(this);
  } 

} // ExpandToolbar


//--------------------------------------------------------------------
NS_METHOD nsToolbarManager::AddToolbarListener(nsIToolbarManagerListener * aListener) 
{
  mListener = aListener;
  return NS_OK;
}


//--------------------------------------------------------------------
NS_METHOD nsToolbarManager::SetCollapseTabURLs(const nsString& aUpURL,
                                                const nsString& aPressedURL,
                                                const nsString& aDisabledURL,
                                                const nsString& aRollOverURL) 
{
  mCollapseUpURL       = aUpURL;
  mCollapsePressedURL  = aPressedURL;
  mCollapseDisabledURL = aDisabledURL;
  mCollapseRollOverURL = aRollOverURL;

  return NS_OK;
}

//--------------------------------------------------------------------
NS_METHOD nsToolbarManager::GetCollapseTabURLs(nsString& aUpURL,
                                               nsString& aPressedURL,
                                               nsString& aDisabledURL,
                                               nsString& aRollOverURL) 
{
  aUpURL = mCollapseUpURL;
  aPressedURL = mCollapsePressedURL;
  aDisabledURL = mCollapseDisabledURL;
  aRollOverURL = mCollapseRollOverURL;

  return NS_OK;
}

//--------------------------------------------------------------------
NS_METHOD nsToolbarManager::SetExpandTabURLs(const nsString& aUpURL,
                                              const nsString& aPressedURL,
                                              const nsString& aDisabledURL,
                                              const nsString& aRollOverURL) 
{
  mExpandUpURL       = aUpURL;
  mExpandPressedURL  = aPressedURL;
  mExpandDisabledURL = aDisabledURL;
  mExpandRollOverURL = aRollOverURL;

  return NS_OK;
}

//--------------------------------------------------------------------
NS_METHOD nsToolbarManager::DoLayout()
{
  PRInt32 i;
  PRInt32 x = 0;
  PRInt32 y = 1;

  nsRect tbRect;
  GetBounds(tbRect);

const kGrippyIndent = 10;

  // First layout all the items
  for (i=0;i<mNumToolbars;++i) {
    PRBool visible;
    mToolbars[i]->IsVisible(visible);
    if (visible) {
      nsCOMPtr<nsIWidget> widget (mToolbars[i]);
  	  if ( widget ) {
        PRInt32 width = 0, height = 0;
        PRBool isToolbarWrapped;
        mToolbars[i]->GetWrapping(isToolbarWrapped);
        if (isToolbarWrapped)
          mToolbars[i]->GetPreferredConstrainedSize(tbRect.width, tbRect.height, width, height);
        else
          widget->GetPreferredSize(width, height);
        
        widget->Resize(kGrippyIndent, y, tbRect.width - kGrippyIndent, height, PR_TRUE);
        mTabs[i].mBoundingRect = nsRect(0, y, 9, height);   // cache the location of this grippy for hit testing
        mTabs[i].mToolbarHeight = height;
        
        y += height;
      }
    }
  }

  // If there are any collapsed toolbars, we need to fix up the positions of the
  // tabs associated with them to lie horizontally (make them as wide as their
  // corresponding toolbar is tall).
  if (mTabsCollapsed > 0) {
    unsigned int horiz = 0;
    for ( i = 0; i < mNumToolbars; ++i ) {
      if ( mTabs[i].mCollapsed ) {
        mTabs[i].mBoundingRect = nsRect ( horiz, y, mTabs[i].mToolbarHeight, 9 );
        horiz += mTabs[i].mToolbarHeight;
      }
    }
  }
    
  return NS_OK;    
}

//--------------------------------------------------------------------
NS_METHOD nsToolbarManager::GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight)
{
  PRInt32 suggestedWidth  = aWidth;
  PRInt32 suggestedHeight = aHeight;

  // compute heights of all the toolbars
  aWidth  = 0;
  aHeight = 0;
  PRInt32 i;
  for (i=0;i<mNumToolbars;i++) {
    PRInt32     width;
    PRInt32     height;
    PRBool      visible;
    mToolbars[i]->IsVisible(visible);
    if (visible) {
      nsCOMPtr<nsIWidget> widget ( mToolbars[i] );
  	  if ( widget ) {
        PRBool isToolbarWrapped;
        mToolbars[i]->GetWrapping(isToolbarWrapped);
        if (isToolbarWrapped) {
          mToolbars[i]->GetPreferredConstrainedSize(suggestedWidth, suggestedHeight, width, height);
        } else {
          if (NS_OK != widget->GetPreferredSize(width, height)) {
            nsRect rect;
            widget->GetBounds(rect);
            width  = rect.width;
            height = rect.height;
          }
        }
      }
      aWidth = width > aWidth? width : aWidth;
      aHeight += height;
    }
  }

  // If there are any collapsed toolbars, make the toolbox bigger by a bit to allow room 
  // at the bottom for the collapsed tabs
  if (mTabsCollapsed > 0)
    aHeight += EXPAND_TAB_HEIGHT;

  return NS_OK;
}

//--------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_METHOD nsToolbarManager::Resize(PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint)
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
NS_METHOD nsToolbarManager::Resize(PRUint32 aX,
                      PRUint32 aY,
                      PRUint32 aWidth,
                      PRUint32 aHeight,
                      PRBool   aRepaint)
{
  nsresult result = nsWindow::Resize(aX, aY, aWidth, aHeight, aRepaint);
  DoLayout();
  return result;
}


//
// DrawGrippies
//
// Redraws all the grippies in the toolbox by iterating over each toolbar in the DOM 
// and figuring out how to draw the grippies based on size/visibility information
// 
void
nsToolbarManager :: DrawGrippies ( nsIRenderingContext* aContext ) const
{
//*** THIS IS THROWAWAY CODE RIGHT NOW BECAUSE TOOLBARS WILL BE FOUND IN THE DOM
  nsRect tbRect;
  int y = 0;
  for (int i=0;i<mNumToolbars;i++) {
    PRBool visible;
    mToolbars[i]->IsVisible(visible);
    if (visible) {
      nsCOMPtr<nsIWidget> widget ( mToolbars[i] );
  	  if ( widget ) {
        PRInt32 width = 0, height = 0;
        PRBool isToolbarWrapped;
        mToolbars[i]->GetWrapping(isToolbarWrapped);
        if (isToolbarWrapped)
          mToolbars[i]->GetPreferredConstrainedSize(tbRect.width, tbRect.height, width, height);
        else
          widget->GetPreferredSize(width, height);
        
        if ( ! mTabs[i].mCollapsed )
          DrawGrippy ( aContext, mTabs[i].mBoundingRect, PR_FALSE );
                
        y += height;
      }
    }
  }

} // DrawGrippies


//
// DrawGrippy
//
// Draw a single grippy in the given rectangle, either with or without rollover feedback.
//
void
nsToolbarManager :: DrawGrippy ( nsIRenderingContext* aContext, const nsRect & aBoundingRect, 
                                   PRBool aDrawHilighted ) const
{
  aContext->PushState();

  
  if ( aDrawHilighted ) {
    aContext->SetColor(NS_RGB(0,0,0xFF));  
  }
  else {
    aContext->SetColor(NS_RGB(0xdd,0xdd,0xdd));
  }

  aContext->FillRect(aBoundingRect);

  PRBool clipState;
  aContext->PopState(clipState);

} // DrawGrippy


//
// OnMouseMove
//
// Handle mouse move events for hilighting and unhilighting the grippies
//
void
nsToolbarManager :: OnMouseMove ( nsPoint & aMouseLoc )
{
	for ( int i = 0; i < mNumToolbars; ++i ) {
		if ( mTabs[i].mBoundingRect.Contains(aMouseLoc) ) {
			nsCOMPtr<nsIRenderingContext> ctx (dont_AddRef(GetRenderingContext()));
			if ( ctx ) {
				if ( i != mGrippyHilighted ) {
					// unhilight the old one
					if ( mGrippyHilighted != kNoGrippyHilighted )
						DrawGrippy ( ctx, mTabs[mGrippyHilighted].mBoundingRect, PR_FALSE );
				
					// hilight the new one and remember it
					DrawGrippy ( ctx, mTabs[i].mBoundingRect, PR_TRUE );
					mGrippyHilighted = i;
				} // if in a new tab
			} // if we got a rendering context.
		}
	} // for each toolbar

} // OnMouseMove


//
// OnMouseLeftClick
//
// Check if a click is in a grippy and expand/collapse appropriately.
//
void
nsToolbarManager :: OnMouseLeftClick ( nsMouseEvent & aEvent )
{
	for ( int i = 0; i < mNumToolbars; ++i ) {
		if ( mTabs[i].mBoundingRect.Contains(aEvent.point) ) {
			TabInfo & clickedTab = mTabs[i];			
			if ( clickedTab.mCollapsed )
				ExpandToolbar ( clickedTab );
			else
				CollapseToolbar ( clickedTab );
			
			// don't keep repeating this process since toolbars have now be
			// relaid out and a new toolbar may be under the current mouse
			// location!
			break;
		}
	}
	
} // OnMouseLeftClick


//
// OnMouseExit
//
// Update the grippies that may have been hilighted while the mouse was within the
// manager.
//
void
nsToolbarManager :: OnMouseExit ( )
{
	if ( mGrippyHilighted != kNoGrippyHilighted ) {
		nsCOMPtr<nsIRenderingContext> ctx (dont_AddRef(GetRenderingContext()));
		if ( ctx ) {
			DrawGrippy ( ctx, mTabs[mGrippyHilighted].mBoundingRect, PR_FALSE );
			mGrippyHilighted = kNoGrippyHilighted;
		}
	}
} // OnMouseExit
