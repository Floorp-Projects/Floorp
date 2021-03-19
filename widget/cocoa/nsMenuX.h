/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMenuX_h_
#define nsMenuX_h_

#import <Cocoa/Cocoa.h>

#include "mozilla/EventForwards.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsMenuBaseX.h"
#include "nsMenuBarX.h"
#include "nsMenuGroupOwnerX.h"
#include "nsCOMPtr.h"
#include "nsChangeObserver.h"

class nsMenuX;
class nsMenuItemIconX;
class nsMenuItemX;
class nsIWidget;

// MenuDelegate is used to receive Cocoa notifications for setting
// up carbon events. Protocol is defined as of 10.6 SDK.
@interface MenuDelegate : NSObject <NSMenuDelegate> {
  nsMenuX* mGeckoMenu;  // weak ref
}
- (id)initWithGeckoMenu:(nsMenuX*)geckoMenu;
@end

// Once instantiated, this object lives until its DOM node or its parent window is destroyed.
// Do not hold references to this, they can become invalid any time the DOM node can be destroyed.
class nsMenuX final : public nsMenuObjectX, public nsChangeObserver {
 public:
  nsMenuX(nsMenuObjectX* aParent, nsMenuGroupOwnerX* aMenuGroupOwner, nsIContent* aContent);
  virtual ~nsMenuX();

  // If > 0, the OS is indexing all the app's menus (triggered by opening
  // Help menu on Leopard and higher).  There are some things that are
  // unsafe to do while this is happening.
  static int32_t sIndexingMenuLevel;

  NS_DECL_CHANGEOBSERVER

  // nsMenuObjectX
  nsMenuObjectTypeX MenuObjectType() override { return eSubmenuObjectType; }
  void IconUpdated() override;

  // nsMenuX
  nsresult Create(nsMenuObjectX* aParent, nsMenuGroupOwnerX* aMenuGroupOwner, nsIContent* aNode);

  // The returned object is an nsMenuX or an nsMenuItemX object
  nsMenuObjectX* GetItemAt(uint32_t aPos);
  uint32_t GetItemCount();

  // The returned object is an nsMenuX or an nsMenuItemX object
  nsMenuObjectX* GetVisibleItemAt(uint32_t aPos);
  nsresult GetVisibleItemCount(uint32_t& aCount);

  nsEventStatus MenuOpened();
  void MenuClosed();
  void SetRebuild(bool aMenuEvent);
  void SetupIcon();
  nsIContent* Content() { return mContent; }
  NSMenuItem* NativeNSMenuItem() { return mNativeMenuItem; }
  GeckoNSMenu* NativeNSMenu() { return mNativeMenu; }

  void Dump(uint32_t aIndent) const;

  static bool IsXULHelpMenu(nsIContent* aMenuContent);

 protected:
  void RebuildMenu();
  nsresult RemoveAll();
  nsresult SetEnabled(bool aIsEnabled);
  nsresult GetEnabled(bool* aIsEnabled);
  already_AddRefed<nsIContent> GetMenuPopupContent();
  bool OnOpen();
  void AddMenuItem(mozilla::UniquePtr<nsMenuItemX>&& aMenuItem);
  void AddMenu(mozilla::UniquePtr<nsMenuX>&& aMenu);
  void LoadMenuItem(nsIContent* aMenuItemContent);
  void LoadSubMenu(nsIContent* aMenuContent);
  GeckoNSMenu* CreateMenuWithGeckoString(nsString& aMenuTitle);

  nsCOMPtr<nsIContent> mContent;  // XUL <menu> or <menupopup>

  // Contains nsMenuX and nsMenuItemX objects
  nsTArray<mozilla::UniquePtr<nsMenuObjectX>> mMenuObjectsArray;

  nsString mLabel;
  uint32_t mVisibleItemsCount = 0;               // cache
  nsMenuObjectX* mParent = nullptr;              // [weak]
  nsMenuGroupOwnerX* mMenuGroupOwner = nullptr;  // [weak]
  mozilla::UniquePtr<nsMenuItemIconX> mIcon;
  GeckoNSMenu* mNativeMenu = nil;     // [strong]
  MenuDelegate* mMenuDelegate = nil;  // [strong]
  // nsMenuX objects should always have a valid native menu item.
  NSMenuItem* mNativeMenuItem = nil;  // [strong]
  bool mIsEnabled = true;
  bool mNeedsRebuild = true;
  bool mIsOpen = false;
  bool mVisible = true;
};

#endif  // nsMenuX_h_
