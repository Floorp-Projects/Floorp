/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Josh Aas <josh@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsMenuX_h_
#define nsMenuX_h_

#include "nsCOMPtr.h"
#include "nsIMenu.h"
#include "nsIMenuListener.h"
#include "nsIChangeManager.h"
#include "nsWeakReference.h"
#include "nsMenuBarX.h"

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>


class nsIMenuBar;
class nsIMenuListener;
class nsMenuX;


// MenuDelegate is used to receive Cocoa notifications for
// setting up carbon events
@interface MenuDelegate : NSObject
{
  nsMenuX* mGeckoMenu; // weak ref
  EventHandlerRef mEventHandler;
  BOOL mHaveInstalledCarbonEvents;
}
- (id)initWithGeckoMenu:(nsMenuX*)geckoMenu;
@end


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
    nsEventStatus MenuConstruct(const nsMenuEvent & aMenuEvent, nsIWidget * aParentWindow, 
                                void * menuNode, void * aDocShell);
    nsEventStatus MenuDestruct(const nsMenuEvent & aMenuEvent);
    nsEventStatus CheckRebuild(PRBool & aMenuEvent);
    nsEventStatus SetRebuild(PRBool aMenuEvent);

    // nsIMenu Methods
    NS_IMETHOD Create (nsISupports * aParent, const nsAString &aLabel, const nsAString &aAccessKey, 
                       nsIChangeManager* aManager, nsIDocShell* aShell, nsIContent* aNode);
    NS_IMETHOD GetParent(nsISupports *&aParent);
    NS_IMETHOD GetLabel(nsString &aText);
    NS_IMETHOD SetLabel(const nsAString &aText);
    NS_IMETHOD GetAccessKey(nsString &aText);
    NS_IMETHOD SetAccessKey(const nsAString &aText);
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

    NS_IMETHOD AddMenuItem(nsIMenuItem * aMenuItem);
    NS_IMETHOD AddMenu(nsIMenu * aMenu);
    NS_IMETHOD ChangeNativeEnabledStatusForMenuItem(nsIMenuItem* aMenuItem, PRBool aEnabled);
    NS_IMETHOD GetMenuRefAndItemIndexForMenuItem(nsISupports* aMenuItem,
                                                 void**       aMenuRef,
                                                 PRUint16*    aMenuItemIndex);
    NS_IMETHOD SetupIcon();
    
protected:
    // Determines how many menus are visible among the siblings that are before me.
    // It doesn't matter if I am visible.
    nsresult CountVisibleBefore(PRUint32* outVisibleBefore);

    // fetch the content node associated with the menupopup item
    void GetMenuPopupContent(nsIContent** aResult);
    
    // fire handlers for oncreate/ondestroy
    PRBool OnDestroy();
    PRBool OnCreate();
    PRBool OnDestroyed();
    PRBool OnCreated();

    void LoadMenuItem(nsIMenu* pParentMenu, nsIContent* menuitemContent);  
    void LoadSubMenu(nsIMenu * pParentMenu, nsIContent* menuitemContent);
    void LoadSeparator(nsIContent* menuitemContent);

    NSMenu* CreateMenuWithGeckoString(nsString& menuTitle);

protected:
    nsString                    mLabel;
    nsCOMArray<nsISupports>     mMenuItemsArray;

    nsISupports*                mParent;                // weak, my parent owns me
    nsIChangeManager*           mManager;               // weak ref, it will outlive us [menubar]
    nsWeakPtr                   mDocShellWeakRef;       // weak ref to docshell
    nsCOMPtr<nsIContent>        mMenuContent;           // the |menu| tag, strong ref
    nsCOMPtr<nsIMenuListener>   mListener;              // strong ref

    // Mac specific
    PRInt16                     mMacMenuID;
    NSMenu*                     mMacMenu;               // strong ref, we own it
    MenuDelegate*               mMenuDelegate;          // strong ref, we keep this around to get events for us
    PRPackedBool                mIsEnabled;
    PRPackedBool                mDestroyHandlerCalled;
    PRPackedBool                mNeedsRebuild;
    PRPackedBool                mConstructed;
    PRPackedBool                mVisible;               // are we visible to the user?
    PRPackedBool                mXBLAttached;
};

#endif // nsMenuX_h_
