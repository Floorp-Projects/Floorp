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

#ifndef nsMenu_h__
#define nsMenu_h__

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
class nsIDOMElement;


// temporary hack to get apple menu -- sfraser, approved saari
#define APPLE_MENU_HACK   1

#ifdef APPLE_MENU_HACK
extern const PRInt16 kMacMenuID;
extern const PRInt16 kAppleMenuID;
#endif /* APPLE_MENU_HACK */

//static PRInt16      mMacMenuIDCount;    // use GetUniqueMenuID()
 extern PRInt16 mMacMenuIDCount;// = kMacMenuID;


class nsMenu :  public nsIMenu,
                public nsIMenuListener,
                public nsIChangeObserver,
                public nsSupportsWeakReference
{

public:
  nsMenu();
  virtual ~nsMenu();

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
                        nsIChangeManager* aManager, nsIWebShell* aShell, nsIDOMNode* aNode ) ;
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
  NS_IMETHOD GetDOMNode(nsIDOMNode ** aMenuNode);
  NS_IMETHOD SetEnabled(PRBool aIsEnabled);
  NS_IMETHOD GetEnabled(PRBool* aIsEnabled);
  NS_IMETHOD IsHelpMenu(PRBool* aIsEnabled);
  
  // 
  NS_IMETHOD AddMenuItem(nsIMenuItem * aMenuItem);
  NS_IMETHOD AddMenu(nsIMenu * aMenu);

  // MacSpecific
  static PRInt16  GetUniqueMenuID()
                  {
                    if (mMacMenuIDCount == 32767)
                      mMacMenuIDCount = 256;
                    return mMacMenuIDCount++;
                  }

protected:
  nsString          mLabel;
  PRUint32          mNumMenuItems;
  nsSupportsArray   mMenuItemsArray;       // array holds refs

  nsIMenu*          mMenuParent;           // weak, my parent owns me
  nsIMenuBar*       mMenuBarParent;
  nsIChangeManager* mManager;             // weak ref, it will outlive us
  nsWeakPtr                  mWebShellWeakRef;    // weak ref to webshell
  nsCOMPtr<nsIDOMNode>       mDOMNode;     //strong ref
  nsCOMPtr<nsIMenuListener>  mListener;

  bool                  mConstructed;
  // MacSpecific
  PRInt16               mMacMenuID;
  MenuHandle            mMacMenuHandle;
  UnicodeToTextRunInfo  mUnicodeTextRunConverter;
  PRInt16               mHelpMenuOSItemsCount;
  PRPackedBool          mIsHelpMenu;
  PRPackedBool          mIsEnabled;
  PRPackedBool          mDestroyHandlerCalled;
  PRPackedBool          mNeedsRebuild;

  nsresult GetNextVisibleMenu(nsIMenu** outNextVisibleMenu);

    // fetch the content node associated with the menupopup item
  void GetMenuPopupElement ( nsIDOMNode** aResult ) ;

    // fire handlers for oncreate/ondestroy
  PRBool OnDestroy() ;
  PRBool OnCreate() ;
  
  void LoadMenuItem( nsIMenu * pParentMenu, nsIDOMElement * menuitemElement,
                      nsIDOMNode * menuitemNode, nsIWebShell * aWebShell);  
  void LoadSubMenu( nsIMenu * pParentMenu, nsIDOMElement * menuElement, nsIDOMNode * menuNode);  
  nsEventStatus HelpMenuConstruct( const nsMenuEvent & aMenuEvent, nsIWidget* aParentWindow, 
                                    void* menuNode, void* aWebShell);
  
  MenuHandle NSStringNewMenu(short menuID, nsString& menuTitle);

private:
  
};


#endif // nsMenu_h__
