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

NS_IMPL_ADDREF(nsToolbarManager)
NS_IMPL_RELEASE(nsToolbarManager)

// XXX These values need to come from the 
// images instead of being hard coded
#define EXPAND_TAB_WIDTH  56
#define EXPAND_TAB_HEIGHT 10


const PRInt32 kMaxNumToolbars = 32;

//--------------------------------------------------------------------
//-- nsToolbarManager Constructor
//--------------------------------------------------------------------
nsToolbarManager::nsToolbarManager() : ChildWindow(), nsIToolbarManager(), nsIImageButtonListener()
{
  NS_INIT_REFCNT();

  mListener    = nsnull;
  mNumToolbars = 0;
  mNumTabs     = 0;
  mNumTabsSave = 0;

  // XXX Needs to be changed to a Vector class
  mToolbars = new nsIToolbar* [kMaxNumToolbars];
  mTabs     = new TabInfo* [kMaxNumToolbars];
  mTabsSave = new TabInfo* [kMaxNumToolbars];

  PRInt32 i;
  for (i=0;i<kMaxNumToolbars;i++) {
    mToolbars[i] = nsnull;
    mTabs[i]     = nsnull;
    mTabsSave[i] = nsnull;
  }
}

//--------------------------------------------------------------------
nsToolbarManager::~nsToolbarManager()
{
  NS_IF_RELEASE(mListener);
  PRInt32 i;
  for (i=0;i<kMaxNumToolbars;i++) {
    NS_RELEASE(mToolbars[i]);
  }
}

//--------------------------------------------------------------------
nsresult nsToolbarManager::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kCToolbarManagerCID);  

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
  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) (nsToolbarManager *)this;                                        
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

//-----------------------------------------------------
/*static nsEventStatus PR_CALLBACK
HandleTabEvent(nsGUIEvent *aEvent)
{
  nsEventStatus result = nsEventStatus_eIgnore;

  nsIImageButton * button;
	if (NS_OK == aEvent->widget->QueryInterface(kIImageButtonIID,(void**)&button)) {
    result = button->HandleEvent(aEvent);

    if (aEvent->message == NS_MOUSE_LEFT_BUTTON_UP) {
      nsIWidget * widget;
      aEvent->widget->GetClientData((void *&)widget);
      if (nsnull != widget) {
        nsIToolbarManager * tbMgr;
	      if (NS_OK == widget->QueryInterface(kCIToolbarManagerIID,(void**)&tbMgr)) {
          PRInt32 index;
          if (NS_OK == tbMgr->GetTabIndex(button, index)) {
            nsIToolbar * tb;
            tbMgr->GetToolbarAt(tb, index);
            if (nsnull != tb) {
              tbMgr->ExpandToolbar(tb);
              NS_RELEASE(tb);
            }
          }
          NS_RELEASE(tbMgr);
        }
      }
    }

    NS_RELEASE(button);
  }

  return result;
}*/

//--------------------------------------------------------------------
NS_METHOD nsToolbarManager::AddTabToManager(nsIToolbar *    aToolbar,
                                            const nsString& aUpURL,
                                            const nsString& aPressedURL,
                                            const nsString& aDisabledURL,
                                            const nsString& aRollOverURL)
{
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


//--------------------------------------------------------------------
NS_METHOD nsToolbarManager::AddToolbar(nsIToolbar* aToolbar)
{
  mToolbars[mNumToolbars] = aToolbar;
  mNumToolbars++;

  // XXX should check here to make sure it isn't already added
  aToolbar->SetToolbarManager(this);
  NS_ADDREF(aToolbar);

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
  mNumToolbars++;
  aToolbar->SetToolbarManager(this);
  NS_ADDREF(aToolbar);

  return NS_OK;    
}

//--------------------------------------------------------------------
NS_METHOD nsToolbarManager::GetTabIndex(nsIImageButton * aTab, PRInt32 &anIndex)
{
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
  nsIWidget * widget;
	if (NS_OK == aToolbar->QueryInterface(kIWidgetIID,(void**)&widget)) {
    widget->Show(PR_FALSE);
    AddTabToManager(aToolbar, mExpandUpURL, mExpandPressedURL, mExpandDisabledURL, mExpandRollOverURL);
    if (nsnull != mListener) {
      mListener->NotifyToolbarManagerChangedSize(this);
    }
    NS_RELEASE(widget);
  }
 
  return NS_OK;
}

//--------------------------------------------------------------------
NS_METHOD nsToolbarManager::ExpandToolbar(nsIToolbar * aToolbar) 
{
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


  return NS_OK;
}

//--------------------------------------------------------------------
NS_METHOD nsToolbarManager::NotifyImageButtonEvent(nsIImageButton * aImgBtn,
                                                   nsGUIEvent     * aEvent)
{
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

  return NS_OK;
}

//--------------------------------------------------------------------
NS_METHOD nsToolbarManager::AddToolbarListener(nsIToolbarManagerListener * aListener) 
{
  mListener = aListener;
  NS_ADDREF(aListener); 

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

  // First layout all the items
  for (i=0;i<mNumToolbars;i++) {
    PRBool visible;
    mToolbars[i]->IsVisible(visible);
    if (visible) {
      nsIWidget * widget;
  	  if (NS_OK == mToolbars[i]->QueryInterface(kIWidgetIID,(void**)&widget)) {
        PRInt32 width, height;
        PRBool isToolbarWrapped;
        mToolbars[i]->GetWrapping(isToolbarWrapped);
        if (isToolbarWrapped) {
          mToolbars[i]->GetPreferredConstrainedSize(tbRect.width, tbRect.height, width, height);
       } else {
          widget->GetPreferredSize(width, height);
        }
        widget->Resize(0, y, tbRect.width, height, PR_TRUE);
        y += height;
        NS_RELEASE(widget);
      }
    }
  }

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
    nsIWidget * widget;
    PRBool      visible;
    mToolbars[i]->IsVisible(visible);
    if (visible) {
  	  if (NS_OK == mToolbars[i]->QueryInterface(kIWidgetIID,(void**)&widget)) {
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
        NS_RELEASE(widget);
      }
      aWidth = width > aWidth? width : aWidth;
      aHeight += height;
    }
  }

  if (mNumTabs > 0) {
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
