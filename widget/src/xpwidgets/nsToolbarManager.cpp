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
#include "nsIToolbarManagerListener.h"
#include "nsIToolbarItemHolder.h"

#include "nsIRenderingContext.h"
#include "nsWidgetsCID.h"
#include "nsIWidget.h"
#include "nsIImageButton.h"
#include "nsRepository.h"
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
nsToolbarManager::nsToolbarManager() : ChildWindow(), nsIToolbarManager(), nsIImageButtonListener(),
		mGrippyHilighted(kNoGrippyHilighted), mNumToolbars(0), mNumTabsSave(0)
{
  NS_INIT_REFCNT();

  // XXX Needs to be changed to a Vector class
  mToolbars = new nsCOMPtr<nsIToolbar> [kMaxNumToolbars];
  mTabsSave = new TabInfo* [kMaxNumToolbars];

  PRInt32 i;
  for (i=0;i<kMaxNumToolbars;i++) {
    mTabsSave[i] = nsnull;
  }
}

//--------------------------------------------------------------------
nsToolbarManager::~nsToolbarManager()
{
  delete [] mToolbars;
  delete [] mTabsSave;
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
  if (aIID.Equals(kCIImageButtonListenerIID)) {                                          
    *aInstancePtr = (void*) (nsIImageButtonListener *)this;                                        
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

      //*** Paint the grippy bar for each toolbar
      DrawGrippies(ctx);
      
      ctx->CopyOffScreenBits(ds, 0, 0, rect, NS_COPYBITS_USE_SOURCE_CLIP_REGION);
      ctx->DestroyDrawingSurface(ds);
//	}
    }
    break;
    
    case NS_MOUSE_MOVE:
      OnMouseMove ( aEvent->point );
      break;
    
    case NS_MOUSE_EXIT:
      OnMouseExit();
      break;
      
  } // switch on event
  
  return nsEventStatus_eIgnore;
  
} // HandleEvent


#if GRIPPYS_NOT_WIDGETS
//--------------------------------------------------------------------
NS_METHOD nsToolbarManager::AddTabToManager(nsIToolbar *    aToolbar,
                                            const nsString& aUpURL,
                                            const nsString& aPressedURL,
                                            const nsString& aDisabledURL,
                                            const nsString& aRollOverURL)
{
DebugStr("\pnsToolbarManager::AddTabToManager");
  if (mNumTabsSave > 0) {
    TabInfo * tabInfo = mTabsSave[--mNumTabsSave];
    tabInfo->mToolbar = aToolbar;
    mTabs[mNumTabs++] = tabInfo;

    nsIWidget * widget;
	  if (NS_OK == tabInfo->mTab->QueryInterface(kIWidgetIID,(void**)&widget)) {
	    widget->Show(PR_TRUE);
      NS_RELEASE(widget);
    }
    return NS_OK;
  }

  nsIImageButton * tab;
  nsresult rv = nsRepository::CreateInstance(kImageButtonCID, nsnull, kIImageButtonIID,
                                    (void**)&tab);
  if (NS_OK != rv) {
    return rv;
  }

  nsRect rt(0, 0, EXPAND_TAB_WIDTH, EXPAND_TAB_HEIGHT);
  
	//nsIWidget* parent = nsnull;
	//if (aParent != nsnull)
  //  aParent->QueryInterface(kIWidgetIID,(void**)&parent);

  nsIWidget* 	parent;
	if (NS_OK != QueryInterface(kIWidgetIID,(void**)&parent)) {
    return NS_OK;
  }

  nsIWidget * widget;
	if (NS_OK == tab->QueryInterface(kIWidgetIID,(void**)&widget)) {
	  widget->Create(parent, rt, NULL, NULL);
	  widget->Show(PR_TRUE);

	  widget->Resize(0, 1, EXPAND_TAB_WIDTH, EXPAND_TAB_HEIGHT, PR_FALSE);
    widget->SetClientData((void *)parent);

    tab->SetBorderWidth(0);
    tab->SetBorderOffset(0);
    tab->SetShowBorder(PR_FALSE);
    tab->SetShowText(PR_FALSE);
    tab->SetLabel("");
    tab->SetImageDimensions(rt.width, rt.height);
    tab->SetImageURLs(aUpURL, aPressedURL, aDisabledURL, aRollOverURL);
    tab->AddListener(this);

    mTabs[mNumTabs]             = new TabInfo;
    mTabs[mNumTabs]->mTab       = tab;
    mTabs[mNumTabs++]->mToolbar = aToolbar;
	}


  return NS_OK;    
}

#endif


//--------------------------------------------------------------------
NS_METHOD nsToolbarManager::AddToolbar(nsIToolbar* aToolbar)
{
  mToolbars[mNumToolbars] = aToolbar;
  mTabs[mNumToolbars].mToolbar = aToolbar;  // hook this toolbar up to a grippy
  mNumToolbars++;
  
#if GRIPPYS_NOT_WIDGETS
  aToolbar->SetToolbarManager(this);
#endif

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
  mNumToolbars++;

#if GRIPPYS_NOT_WIDGETS
  aToolbar->SetToolbarManager(this);
#endif

  return NS_OK;    
}

//--------------------------------------------------------------------
NS_METHOD nsToolbarManager::GetTabIndex(nsIImageButton * aTab, PRInt32 &anIndex)
{
#if GRIPPYS_NOT_WIDGETS
  anIndex = 0;
  PRInt32 i = 0;
  while (i < mNumTabs) {
    if (mTabs[i]->mTab == aTab) {
      for (anIndex=0;anIndex<mNumToolbars;anIndex++) {
        if (mToolbars[anIndex] == mTabs[i]->mToolbar) {
          return NS_OK;
        }
      }
      return NS_ERROR_FAILURE;
    }
    i++;
  }
#endif
  return NS_ERROR_FAILURE;
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

//--------------------------------------------------------------------
NS_METHOD nsToolbarManager::CollapseToolbar(nsIToolbar * aToolbar) 
{
  nsCOMPtr<nsIWidget> widget ( aToolbar );
  if ( widget ) {
    widget->Show(PR_FALSE);
//  AddTabToManager(aToolbar, mExpandUpURL, mExpandPressedURL, mExpandDisabledURL, mExpandRollOverURL);
    if ( mListener )
      mListener->NotifyToolbarManagerChangedSize(this);
  }
 
  return NS_OK;
}

//--------------------------------------------------------------------
NS_METHOD nsToolbarManager::ExpandToolbar(nsIToolbar * aToolbar) 
{
#if GRIPPYS_NOT_WIDGETS
  PRInt32     inx   = 0;
  PRBool      found = PR_FALSE;
  nsIWidget * toolbarWidget;

	if (NS_OK == aToolbar->QueryInterface(kIWidgetIID,(void**)&toolbarWidget)) {
    PRInt32 i;
    for (i=0;i<mNumTabs;i++) {
      if (mTabs[i]->mToolbar == aToolbar) {
        inx = i;
        found = PR_TRUE;
        break;
      }
    }

    if (found) {
      PRInt32 i;
      TabInfo * tabInfo = mTabs[inx];
      mTabsSave[mNumTabsSave++] = tabInfo;
      tabInfo->mToolbar = nsnull;

      mNumTabs--;
      for (i=inx;i<mNumTabs;i++) {
        mTabs[i] = mTabs[i+1];
      }
      nsIWidget * tabWidget;
	    if (NS_OK == tabInfo->mTab->QueryInterface(kIWidgetIID,(void**)&tabWidget)) {
	      tabWidget->Show(PR_FALSE);
        NS_RELEASE(tabWidget);
      }
    }
    toolbarWidget->Show(PR_TRUE);
    //DoLayout();
    if (nsnull != mListener) {
      mListener->NotifyToolbarManagerChangedSize(this);
    }
    NS_RELEASE(toolbarWidget);
  }

#endif
  return NS_OK;
}

//--------------------------------------------------------------------
NS_METHOD nsToolbarManager::NotifyImageButtonEvent(nsIImageButton * aImgBtn,
                                                   nsGUIEvent     * aEvent)
{
#if GRIPPYS_NOT_WIDGETS
  if (aEvent->message != NS_MOUSE_LEFT_BUTTON_UP) {
    return NS_OK;
  }

  // First check to see if it is one of our tabs
  // meaning its a tollbar expansion
  PRInt32 index;
  if (NS_OK == GetTabIndex(aImgBtn, index)) {
    nsIToolbar * tb;
    GetToolbarAt(tb, index);
    if (nsnull != tb) {
      ExpandToolbar(tb);
      NS_RELEASE(tb);
    }
  }

  // If not then a toolbar is collapsing
  PRInt32 i;
  for (i=0;i<mNumToolbars;i++) {
    nsIToolbar     * toolbar = mToolbars[i];
    nsIToolbarItem * item;
    if (NS_OK == toolbar->GetItemAt(item, 0)) {
      nsIWidget * widget;
      if (NS_OK == item->QueryInterface(kIWidgetIID,(void**)&widget)) {
        if (widget == aEvent->widget) {
          CollapseToolbar(toolbar);
        }
        NS_RELEASE(widget);
      }
      NS_RELEASE(item);
    }
  }
#endif
  return NS_OK;
}

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
  for (i=0;i<mNumToolbars;i++) {
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
        
        y += height;
      }
    }
  }

#if GRIPPYS_NOT_WIDGETS
  nsRect rect;
  x = 0;
  for (i=0;i<mNumTabs;i++) {
    nsRect rect;
    nsIWidget * widget;
  	if (NS_OK == mTabs[i]->mTab->QueryInterface(kIWidgetIID,(void**)&widget)) {
      widget->GetBounds(rect);
      rect.x = x;
      rect.y = y;
      widget->Resize(rect.x, rect.y, rect.width, rect.height, PR_TRUE);
      x += rect.width;
      NS_RELEASE(widget);
    }
  }
#endif

  return NS_OK;    
}

//--------------------------------------------------------------------
NS_METHOD nsToolbarManager::GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight)
{
  PRInt32 suggestedWidth  = aWidth;
  PRInt32 suggestedHeight = aHeight;

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

  if (mNumToolbars > 0) {
    aHeight += EXPAND_TAB_HEIGHT;
  }

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
// OnMouseMove
//
// Handle mouse move events for hilighting and unhilighting the grippies
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
} // OnMouseMove