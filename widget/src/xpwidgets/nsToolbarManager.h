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
 * Copyright (C); 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsToolbarManager_h___
#define nsToolbarManager_h___

#include "nsIImageRequest.h"
#include "nsIToolbarManager.h"
#include "nsIToolbarItem.h"
#include "nsIToolbar.h"
#include "nsIToolbarManagerListener.h"	// needed for VC4.2
#include "nsIContentConnector.h"
#include "nsWindow.h"

#include "nsCOMPtr.h"

class nsIWidget;

struct nsRect;


struct TabInfo {
  TabInfo ( ) : mCollapsed(PR_TRUE), mToolbarHeight(0) { };
  
  nsCOMPtr<nsIToolbar>  mToolbar;
  nsRect                mBoundingRect;
  PRBool                mCollapsed;
  unsigned int          mToolbarHeight;
};


//
// pinkerton's notes 
//
// The only access to the toolbars this manages should be through the DOM. As a result,
// we don't need a separate toolbar manager interface to the outside world besides the
// minimum required to be loaded by the loader (nsILoader or something). The
// |nsIToolbarManager| interface will probably go away soon.
//


//------------------------------------------------------------
class nsToolbarManager : public ChildWindow,
                         public nsIToolbarManager, //*** for now 
                         public nsIContentConnector
{
public:
    nsToolbarManager();
    virtual ~nsToolbarManager();

    NS_DECL_ISUPPORTS

	// nsIContentConnector Interface --------------------------------
	NS_IMETHOD SetContentRoot(nsIContent* pContent);
	NS_IMETHOD_(nsEventStatus) HandleEvent(nsGUIEvent *aEvent);

    NS_IMETHOD AddToolbar(nsIToolbar* aToolbar);
    NS_IMETHOD InsertToolbarAt(nsIToolbar* aToolbar, PRInt32 anIndex);
    NS_IMETHOD GetNumToolbars(PRInt32 & aNumToolbars);
    NS_IMETHOD GetToolbarAt(nsIToolbar*& aToolbar, PRInt32 anIndex); 
    NS_IMETHOD AddToolbarListener(nsIToolbarManagerListener * aListener); 
    NS_IMETHOD DoLayout();
    NS_IMETHOD GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight);
    NS_IMETHOD Resize(PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint);
    NS_IMETHOD Resize(PRUint32 aX,  PRUint32 aY, PRUint32 aWidth, PRUint32 aHeight, PRBool   aRepaint);
    NS_IMETHOD SetCollapseTabURLs(const nsString& aUpURL,
                                  const nsString& aPressedURL,
                                  const nsString& aDisabledURL,
                                  const nsString& aRollOverURL);
    NS_IMETHOD GetCollapseTabURLs(nsString& aUpURL,
                                  nsString& aPressedURL,
                                  nsString& aDisabledURL,
                                  nsString& aRollOverURL);

    NS_IMETHOD SetExpandTabURLs(const nsString& aUpURL,
                                const nsString& aPressedURL,
                                const nsString& aDisabledURL,
                                const nsString& aRollOverURL);

  // Override the widget creation method
  NS_IMETHOD Create(nsIWidget *aParent,
                            const nsRect &aRect,
                            EVENT_CALLBACK aHandleEventFunction,
                            nsIDeviceContext *aContext,
                            nsIAppShell *aAppShell,
                            nsIToolkit *aToolkit,
                            nsWidgetInitData *aInitData);

protected:
  enum { kNoGrippyHilighted = -1 } ;
  
  void DrawGrippies ( nsIRenderingContext* aContext ) const ;
  void DrawGrippy ( nsIRenderingContext* aContext, const nsRect & aBoundingRect, 
                     PRBool aDrawHilighted ) const ;

  void CollapseToolbar ( TabInfo & inTab ) ; 
  void ExpandToolbar ( TabInfo & inTab ) ; 

  void OnMouseMove ( nsPoint & aMouseLoc ) ;
  void OnMouseExit ( ) ;
  void OnMouseLeftClick ( nsMouseEvent & aEvent ) ;

  nsCOMPtr<nsIToolbarManagerListener> mListener;

  //*** this should be smart pointers ***
//nsToolbarDataModel* mDataModel;   // The data source from which everything to draw is obtained.

  // CollapseTab URLs
  nsString mCollapseUpURL;
  nsString mCollapsePressedURL;
  nsString mCollapseDisabledURL;
  nsString mCollapseRollOverURL;

  // Expand Tab URLs
  nsString mExpandUpURL;
  nsString mExpandPressedURL;
  nsString mExpandDisabledURL;
  nsString mExpandRollOverURL;

  // XXX these will be changed to Vectors
  nsCOMPtr<nsIToolbar> * mToolbars;
  PRInt32       mNumToolbars;

  TabInfo    mTabs[10];
  int        mGrippyHilighted; // used to indicate which grippy the mouse is inside
  int        mTabsCollapsed;   // how many tabs are collapsed?

  // Images for the pieces of the grippy panes. They are composites of a set of urls
  // which specify the top, middle, and bottom of the grippy.
  //*** should these be in an imageGroup?
  nsCOMPtr<nsIImageRequest> mGrippyNormalTop;		// Image for grippy in normal state
  nsCOMPtr<nsIImageRequest> mGrippyNormalMiddle;	// Image for grippy in normal state
  nsCOMPtr<nsIImageRequest> mGrippyNormalBottom;	// Image for grippy in normal state

  nsCOMPtr<nsIImageRequest> mGrippyRolloverTop;     // image for grippy in rollover state
  nsCOMPtr<nsIImageRequest> mGrippyRolloverMiddle;  // image for grippy in rollover state
  nsCOMPtr<nsIImageRequest> mGrippyRolloverBottom;  // image for grippy in rollover state
  
}; // class nsToolbarManager

#endif /* nsToolbarManager_h___ */
