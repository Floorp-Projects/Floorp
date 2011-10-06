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

#import <Cocoa/Cocoa.h>

#include "nsMenuBaseX.h"
#include "nsMenuBarX.h"
#include "nsMenuGroupOwnerX.h"
#include "nsCOMPtr.h"
#include "nsChangeObserver.h"
#include "nsAutoPtr.h"

class nsMenuX;
class nsMenuItemIconX;
class nsMenuItemX;
class nsIWidget;

// MenuDelegate is used to receive Cocoa notifications for setting
// up carbon events. Protocol is defined as of 10.6 SDK.
#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_6)
@interface MenuDelegate : NSObject < NSMenuDelegate >
#else
@interface MenuDelegate : NSObject
#endif
{
  nsMenuX* mGeckoMenu; // weak ref
}
- (id)initWithGeckoMenu:(nsMenuX*)geckoMenu;
@end

// Once instantiated, this object lives until its DOM node or its parent window is destroyed.
// Do not hold references to this, they can become invalid any time the DOM node can be destroyed.
class nsMenuX : public nsMenuObjectX,
                public nsChangeObserver
{
public:
  nsMenuX();
  virtual ~nsMenuX();

  // If > 0, the OS is indexing all the app's menus (triggered by opening
  // Help menu on Leopard and higher).  There are some things that are
  // unsafe to do while this is happening.
  static PRInt32 sIndexingMenuLevel;

  NS_DECL_CHANGEOBSERVER

  // nsMenuObjectX
  void*             NativeData()     {return (void*)mNativeMenu;}
  nsMenuObjectTypeX MenuObjectType() {return eSubmenuObjectType;}

  // nsMenuX
  nsresult       Create(nsMenuObjectX* aParent, nsMenuGroupOwnerX* aMenuGroupOwner, nsIContent* aNode);
  PRUint32       GetItemCount();
  nsMenuObjectX* GetItemAt(PRUint32 aPos);
  nsresult       GetVisibleItemCount(PRUint32 &aCount);
  nsMenuObjectX* GetVisibleItemAt(PRUint32 aPos);
  nsEventStatus  MenuOpened();
  void           MenuClosed();
  void           SetRebuild(bool aMenuEvent);
  NSMenuItem*    NativeMenuItem();

  static bool    IsXULHelpMenu(nsIContent* aMenuContent);

protected:
  void           MenuConstruct();
  nsresult       RemoveAll();
  nsresult       SetEnabled(bool aIsEnabled);
  nsresult       GetEnabled(bool* aIsEnabled);
  nsresult       SetupIcon();
  void           GetMenuPopupContent(nsIContent** aResult);
  bool           OnOpen();
  bool           OnClose();
  nsresult       AddMenuItem(nsMenuItemX* aMenuItem);
  nsresult       AddMenu(nsMenuX* aMenu);
  void           LoadMenuItem(nsIContent* inMenuItemContent);  
  void           LoadSubMenu(nsIContent* inMenuContent);
  GeckoNSMenu*   CreateMenuWithGeckoString(nsString& menuTitle);

  nsTArray< nsAutoPtr<nsMenuObjectX> > mMenuObjectsArray;
  nsString                  mLabel;
  PRUint32                  mVisibleItemsCount; // cache
  nsMenuObjectX*            mParent; // [weak]
  nsMenuGroupOwnerX*        mMenuGroupOwner; // [weak]
  // The icon object should never outlive its creating nsMenuX object.
  nsRefPtr<nsMenuItemIconX> mIcon;
  GeckoNSMenu*              mNativeMenu; // [strong]
  MenuDelegate*             mMenuDelegate; // [strong]
  // nsMenuX objects should always have a valid native menu item.
  NSMenuItem*               mNativeMenuItem; // [strong]
  bool                      mIsEnabled;
  bool                      mDestroyHandlerCalled;
  bool                      mNeedsRebuild;
  bool                      mConstructed;
  bool                      mVisible;
  bool                      mXBLAttached;
};

#endif // nsMenuX_h_
