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

#include "nsIToolbarManager.h"
#include "nsIToolbarItem.h"
#include "nsIContentConnector.h"
#include "nsWindow.h"
#include "nsIImageButtonListener.h"

class nsIToolbar;
class nsIImageButton;
class nsIWidget;
class nsIToolbarManagerListener;

struct nsRect;

typedef struct {
  nsIToolbar     * mToolbar;
  nsIImageButton * mTab;
} TabInfo;


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
                         public nsIContentConnector,
                         public nsIImageButtonListener

                  
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
    NS_IMETHOD GetTabIndex(nsIImageButton * aTab, PRInt32 &anIndex);
    NS_IMETHOD GetNumToolbars(PRInt32 & aNumToolbars);
    NS_IMETHOD GetToolbarAt(nsIToolbar*& aToolbar, PRInt32 anIndex); 
    NS_IMETHOD CollapseToolbar(nsIToolbar * aToolbar); 
    NS_IMETHOD ExpandToolbar(nsIToolbar * aToolbar); 
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

    // nsIImageButtonListener
    NS_IMETHOD NotifyImageButtonEvent(nsIImageButton * aImgBtn,
                                      nsGUIEvent     * aEvent);

protected:
  NS_METHOD  AddTabToManager(nsIToolbar *    aToolbar,
                             const nsString& aUpURL,
                             const nsString& aPressedURL,
                             const nsString& aDisabledURL,
                             const nsString& aRollOverURL);

  nsIToolbarManagerListener * mListener;

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
  nsIToolbar ** mToolbars;
  PRInt32       mNumToolbars;

  TabInfo    ** mTabs;
  PRInt32       mNumTabs;

  TabInfo    ** mTabsSave;
  PRInt32       mNumTabsSave;

};

#endif /* nsToolbarManager_h___ */
