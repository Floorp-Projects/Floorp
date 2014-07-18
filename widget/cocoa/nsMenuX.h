/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  static int32_t sIndexingMenuLevel;

  NS_DECL_CHANGEOBSERVER

  // nsMenuObjectX
  void*             NativeData()     {return (void*)mNativeMenu;}
  nsMenuObjectTypeX MenuObjectType() {return eSubmenuObjectType;}

  // nsMenuX
  nsresult       Create(nsMenuObjectX* aParent, nsMenuGroupOwnerX* aMenuGroupOwner, nsIContent* aNode);
  uint32_t       GetItemCount();
  nsMenuObjectX* GetItemAt(uint32_t aPos);
  nsresult       GetVisibleItemCount(uint32_t &aCount);
  nsMenuObjectX* GetVisibleItemAt(uint32_t aPos);
  nsEventStatus  MenuOpened();
  void           MenuClosed();
  void           SetRebuild(bool aMenuEvent);
  NSMenuItem*    NativeMenuItem();
  nsresult       SetupIcon();

  static bool    IsXULHelpMenu(nsIContent* aMenuContent);

protected:
  void           MenuConstruct();
  nsresult       RemoveAll();
  nsresult       SetEnabled(bool aIsEnabled);
  nsresult       GetEnabled(bool* aIsEnabled);
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
  uint32_t                  mVisibleItemsCount; // cache
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
