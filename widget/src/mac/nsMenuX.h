/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsMenuX_h__
#define nsMenuX_h__

#include "nsCOMPtr.h"
#include "nsIMenu.h"
#include "nsSupportsArray.h"
#include "nsIMenuListener.h"
#include "nsIChangeManager.h"
#include "nsWeakReference.h"

#include <Menus.h>
#include <UnicodeConverter.h>

class nsIMenuBar;
class nsIMenuListener;


//static PRInt16      mMacMenuIDCount;    // use GetUniqueMenuID()
extern PRInt16 mMacMenuIDCount;// = kMacMenuID;


class nsMenuX : public nsIMenu,
                public nsIMenuListener,
                public nsIChangeObserver,
                public nsSupportsWeakReference
{

public:
    nsMenuX();
    virtual ~nsMenuX();

    NS_DECL_ISUPPORTS
    NS_DECL_NSICHANGEOBSERVER

    // nsIMenuListener methods
    nsEventStatus MenuItemSelected(const nsMenuEvent & aMenuEvent); 
    nsEventStatus MenuSelected(const nsMenuEvent & aMenuEvent); 
    nsEventStatus MenuDeselected(const nsMenuEvent & aMenuEvent); 
    nsEventStatus MenuConstruct( const nsMenuEvent & aMenuEvent, nsIWidget * aParentWindow, 
                                void * menuNode, void * aWebShell);
    nsEventStatus MenuDestruct(const nsMenuEvent & aMenuEvent);
    nsEventStatus CheckRebuild(PRBool & aMenuEvent);
    nsEventStatus SetRebuild(PRBool aMenuEvent);

    // nsIMenu Methods
    NS_IMETHOD Create ( nsISupports * aParent, const nsAReadableString &aLabel, const nsAReadableString &aAccessKey, 
                        nsIChangeManager* aManager, nsIWebShell* aShell, nsIContent* aNode ) ;
    NS_IMETHOD GetParent(nsISupports *&aParent);
    NS_IMETHOD GetLabel(nsString &aText);
    NS_IMETHOD SetLabel(const nsAReadableString &aText);
    NS_IMETHOD GetAccessKey(nsString &aText);
    NS_IMETHOD SetAccessKey(const nsAReadableString &aText);
    NS_IMETHOD AddItem(nsISupports* aText);
    NS_IMETHOD AddSeparator();
    NS_IMETHOD GetItemCount(PRUint32 &aCount);
    NS_IMETHOD GetItemAt(const PRUint32 aPos, nsISupports *& aMenuItem);
    NS_IMETHOD InsertItemAt(const PRUint32 aPos, nsISupports * aMenuItem);
    NS_IMETHOD RemoveItem(const PRUint32 aPos);
    NS_IMETHOD RemoveAll();
    NS_IMETHOD GetNativeData(void** aData);
    NS_IMETHOD SetNativeData(void* aData);
    NS_IMETHOD AddMenuListener(nsIMenuListener * aMenuListener);
    NS_IMETHOD RemoveMenuListener(nsIMenuListener * aMenuListener);
    NS_IMETHOD GetMenuContent(nsIContent ** aMenuNode);
    NS_IMETHOD SetEnabled(PRBool aIsEnabled);
    NS_IMETHOD GetEnabled(PRBool* aIsEnabled);
    NS_IMETHOD IsHelpMenu(PRBool* aIsEnabled);

    // 
    NS_IMETHOD AddMenuItem(nsIMenuItem * aMenuItem);
    NS_IMETHOD AddMenu(nsIMenu * aMenu);

protected:
      // Determines how many menus are visible among the siblings that are before me.
      // It doesn't matter if I am visible.
    nsresult CountVisibleBefore ( PRUint32* outVisibleBefore ) ;

    // fetch the content node associated with the menupopup item
    void GetMenuPopupContent ( nsIContent** aResult ) ;

    // fire handlers for oncreate/ondestroy
    PRBool OnDestroy() ;
    PRBool OnCreate() ;
    PRBool OnDestroyed() ;
    PRBool OnCreated() ;

    void LoadMenuItem ( nsIMenu* pParentMenu, nsIContent* menuitemContent);  
    void LoadSubMenu( nsIMenu * pParentMenu, nsIContent* menuitemContent);  
    nsEventStatus HelpMenuConstruct( const nsMenuEvent & aMenuEvent, nsIWidget* aParentWindow, 
                                      void* unused, void* aWebShell);

    MenuHandle NSStringNewMenu(short menuID, nsString& menuTitle);

protected:
    nsString                    mLabel;
    PRUint32                    mNumMenuItems;
    nsSupportsArray             mMenuItemsArray;        // array holds refs

    nsISupports*                mParent;                // weak, my parent owns me
    // nsIMenu*                    mMenuParent;            
    // nsIMenuBar*                 mMenuBarParent;
    nsIChangeManager*           mManager;               // weak ref, it will outlive us
    nsWeakPtr                   mWebShellWeakRef;       // weak ref to webshell
    nsCOMPtr<nsIContent>        mMenuContent;           // the |menu| tag, strong ref
    nsCOMPtr<nsIMenuListener>   mListener;              // strong ref

    // MacSpecific
    PRInt16                     mMacMenuID;
    MenuHandle                  mMacMenuHandle;
    PRInt16                     mHelpMenuOSItemsCount;
    PRPackedBool                mIsHelpMenu;
    PRPackedBool                mIsEnabled;
    PRPackedBool                mDestroyHandlerCalled;
    PRPackedBool                mNeedsRebuild;
    PRPackedBool                mConstructed;
    PRPackedBool                mVisible;               // are we visible to the user?
};

#endif // nsMenuX_h__
