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
#include "mozilla/Variant.h"
#include "nsISupports.h"
#include "nsMenuParentX.h"
#include "nsMenuBarX.h"
#include "nsMenuGroupOwnerX.h"
#include "nsMenuItemIconX.h"
#include "nsCOMPtr.h"
#include "nsChangeObserver.h"
#include "nsThreadUtils.h"

class nsMenuX;
class nsMenuItemX;
class nsIWidget;

// MenuDelegate is used to receive Cocoa notifications for setting
// up carbon events. Protocol is defined as of 10.6 SDK.
@interface MenuDelegate : NSObject <NSMenuDelegate> {
  nsMenuX* mGeckoMenu;  // weak ref
}
- (id)initWithGeckoMenu:(nsMenuX*)geckoMenu;
@property BOOL menuIsInMenubar;
@end

// Once instantiated, this object lives until its DOM node or its parent window is destroyed.
// Do not hold references to this, they can become invalid any time the DOM node can be destroyed.
class nsMenuX final : public nsMenuParentX,
                      public nsChangeObserver,
                      public nsMenuItemIconX::Listener {
 public:
  using MenuChild = mozilla::Variant<RefPtr<nsMenuX>, RefPtr<nsMenuItemX>>;

  // aParent is optional.
  nsMenuX(nsMenuParentX* aParent, nsMenuGroupOwnerX* aMenuGroupOwner, nsIContent* aContent);

  NS_INLINE_DECL_REFCOUNTING(nsMenuX)

  // If > 0, the OS is indexing all the app's menus (triggered by opening
  // Help menu on Leopard and higher).  There are some things that are
  // unsafe to do while this is happening.
  static int32_t sIndexingMenuLevel;

  NS_DECL_CHANGEOBSERVER

  // nsMenuItemIconX::Listener
  void IconUpdated() override;

  // Unregisters nsMenuX from the nsMenuGroupOwner, and nulls out the group owner pointer, on this
  // nsMenuX and also all nested nsMenuX and nsMenuItemX objects.
  // This is needed because nsMenuX is reference-counted and can outlive its owner, and the menu
  // group owner asserts that everything has been unregistered when it is destroyed.
  void DetachFromGroupOwnerRecursive();

  // Nulls out our reference to the parent.
  // This is needed because nsMenuX is reference-counted and can outlive its parent.
  void DetachFromParent() { mParent = nullptr; }

  mozilla::Maybe<MenuChild> GetItemAt(uint32_t aPos);
  uint32_t GetItemCount();

  mozilla::Maybe<MenuChild> GetVisibleItemAt(uint32_t aPos);
  nsresult GetVisibleItemCount(uint32_t& aCount);

  // Fires the popupshowing event and returns whether the handler allows the popup to open.
  // When calling this method, the caller must hold a strong reference to this object, because other
  // references to this object can be dropped during the handling of the DOM event.
  bool OnOpen();

  void PopupShowingEventWasSentAndApprovedExternally() { DidFirePopupShowing(); }

  // Called from the menu delegate during menuWillOpen.
  // Fires the popupshown event.
  // When calling this method, the caller must hold a strong reference to this object, because other
  // references to this object can be dropped during the handling of the DOM event.
  nsEventStatus MenuOpened();

  // Called from the menu delegate during menuDidClose.
  // When calling this method, the caller must hold a strong reference to this object, because other
  // references to this object can be dropped during the handling of the DOM event.
  void MenuClosed();

  // Called from mPendingAsyncMenuCloseRunnable asynchronously after MenuClosed(), so that it runs
  // after any potential menuItemHit calls for clicked menu items.
  // Fires popuphiding and popuphidden events.
  // When calling this method, the caller must hold a strong reference to this object, because other
  // references to this object can be dropped during the handling of the DOM event.
  void MenuClosedAsync();

  // Close the menu if it's open, and flush any pending popuphiding / popuphidden events.
  bool Close();

  // Called from the menu delegate during menu:willHighlightItem:.
  // If called with Nothing(), it means that no item is highlighted.
  // The index only accounts for visible items, i.e. items for which there exists an NSMenuItem* in
  // mNativeMenu.
  void OnHighlightedItemChanged(const mozilla::Maybe<uint32_t>& aNewHighlightedIndex);

  void SetRebuild(bool aMenuEvent);
  void SetupIcon();
  nsIContent* Content() { return mContent; }
  NSMenuItem* NativeNSMenuItem() { return mNativeMenuItem; }
  GeckoNSMenu* NativeNSMenu() { return mNativeMenu; }

  void SetIconListener(nsMenuItemIconX::Listener* aListener) { mIconListener = aListener; }
  void ClearIconListener() { mIconListener = nullptr; }

  // If aChild is one of our child menus, insert aChild's native menu item in our native menu at the
  // right location.
  void InsertChildNativeMenuItem(nsMenuX* aChild) override;

  // Remove aChild's native menu item froum our native menu.
  void RemoveChildNativeMenuItem(nsMenuX* aChild) override;

  void Dump(uint32_t aIndent) const;

  static bool IsXULHelpMenu(nsIContent* aMenuContent);

  class Observer {
   public:
    // Called when the menu opened, after popupshown.
    // No strong reference is held to the observer during the call.
    virtual void OnMenuOpened() = 0;

    // Called when the menu closed, after popuphidden.
    // No strong reference is held to the observer during the call.
    virtual void OnMenuClosed() = 0;
  };

  // Set an observer that gets notified of menu opening and closing.
  // The menu does not keep a strong reference the observer. The observer must
  // remove itself before it is destroyed.
  void SetObserver(Observer* aObserver) { mObserver = aObserver; }

  // Stop observing.
  void ClearObserver() { mObserver = nullptr; }

 protected:
  virtual ~nsMenuX();

  void RebuildMenu();
  nsresult RemoveAll();
  nsresult SetEnabled(bool aIsEnabled);
  nsresult GetEnabled(bool* aIsEnabled);
  already_AddRefed<nsIContent> GetMenuPopupContent();
  void AddMenuItem(RefPtr<nsMenuItemX>&& aMenuItem);
  void AddMenu(RefPtr<nsMenuX>&& aMenu);
  void LoadMenuItem(nsIContent* aMenuItemContent);
  void LoadSubMenu(nsIContent* aMenuContent);
  GeckoNSMenu* CreateMenuWithGeckoString(nsString& aMenuTitle);
  void UnregisterCommands();
  void DidFirePopupShowing();

  // Calculates the index at which aChild's NSMenuItem should be inserted into our NSMenu.
  // The order of NSMenuItems in the NSMenu is the same as the order of menu children in
  // mMenuChildren; the only difference is that mMenuChildren contains both visible and invisible
  // children, and the NSMenu only contains visible items. So the insertion index is equal to the
  // number of visible previous siblings of aChild in mMenuChildren.
  NSInteger CalculateNativeInsertionPoint(nsMenuX* aChild);

  // If mPendingAsyncMenuCloseRunnable is non-null, call MenuClosedAsync() to send out pending
  // popuphiding/popuphidden events.
  void FlushMenuClosedRunnable();

  nsCOMPtr<nsIContent> mContent;  // XUL <menu> or <menupopup>

  // Contains nsMenuX and nsMenuItemX objects
  nsTArray<MenuChild> mMenuChildren;

  nsString mLabel;
  uint32_t mVisibleItemsCount = 0;                     // cache
  nsMenuParentX* mParent = nullptr;                    // [weak]
  nsMenuGroupOwnerX* mMenuGroupOwner = nullptr;        // [weak]
  nsMenuItemIconX::Listener* mIconListener = nullptr;  // [weak]
  mozilla::UniquePtr<nsMenuItemIconX> mIcon;

  Observer* mObserver = nullptr;  // non-owning pointer to our observer

  // Non-null between a call to MenuClosed() and MenuClosedAsync().
  // This is asynchronous so that, if a menu item is clicked, we can fire popuphiding *after* we
  // execute the menu item command. The macOS menu system calls menuWillClose *before* it calls
  // menuItemHit.
  RefPtr<mozilla::CancelableRunnable> mPendingAsyncMenuCloseRunnable;

  GeckoNSMenu* mNativeMenu = nil;     // [strong]
  MenuDelegate* mMenuDelegate = nil;  // [strong]
  // nsMenuX objects should always have a valid native menu item.
  NSMenuItem* mNativeMenuItem = nil;  // [strong]

  // Nothing() if no item is highlighted. The index only accounts for visible items.
  mozilla::Maybe<uint32_t> mHighlightedItemIndex;

  bool mIsEnabled = true;
  bool mNeedsRebuild = true;

  // Whether the native NSMenu is open, from the macOS perspective.
  bool mIsOpen = false;

  // Whether the popup is open from Gecko's perspective, based on popupshowing / popuphiding events.
  bool mIsOpenForGecko = false;

  bool mVisible = true;

  // true between an OnOpen() call that returned true, and the subsequent call
  // to MenuOpened().
  bool mDidFirePopupshowingAndIsApprovedToOpen = false;
};

#endif  // nsMenuX_h_
