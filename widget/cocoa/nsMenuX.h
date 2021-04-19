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

class nsMenuXObserver {
 public:
  // Called when a menu in this menu subtree opens, before popupshowing.
  // No strong reference is held to the observer during the call.
  virtual void OnMenuWillOpen(mozilla::dom::Element* aPopupElement) = 0;

  // Called when a menu in this menu subtree opened, after popupshown.
  // No strong reference is held to the observer during the call.
  virtual void OnMenuDidOpen(mozilla::dom::Element* aPopupElement) = 0;

  // Called when a menu in this menu subtree closed, after popuphidden.
  // No strong reference is held to the observer during the call.
  virtual void OnMenuClosed(mozilla::dom::Element* aPopupElement) = 0;
};

// Once instantiated, this object lives until its DOM node or its parent window is destroyed.
// Do not hold references to this, they can become invalid any time the DOM node can be destroyed.
class nsMenuX final : public nsMenuParentX,
                      public nsChangeObserver,
                      public nsMenuItemIconX::Listener,
                      public nsMenuXObserver {
 public:
  using Observer = nsMenuXObserver;

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

  // nsMenuXObserver, to forward notifications from our children to our observer.
  void OnMenuWillOpen(mozilla::dom::Element* aPopupElement) override;
  void OnMenuDidOpen(mozilla::dom::Element* aPopupElement) override;
  void OnMenuClosed(mozilla::dom::Element* aPopupElement) override;

  bool IsVisible() const { return mVisible; }

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

  mozilla::Maybe<MenuChild> GetItemForElement(mozilla::dom::Element* aMenuChildElement);

  // Asynchronously runs the command event on aItem, and closes the menu.
  void ActivateItemAndClose(RefPtr<nsMenuItemX>&& aItem, NSEventModifierFlags aModifiers);

  bool IsOpenForGecko() const { return mIsOpenForGecko; }

  // Fires the popupshowing event and returns whether the handler allows the popup to open.
  // When calling this method, the caller must hold a strong reference to this object, because other
  // references to this object can be dropped during the handling of the DOM event.
  bool OnOpen();

  void PopupShowingEventWasSentAndApprovedExternally() { DidFirePopupShowing(); }

  // Called from the menu delegate during menuWillOpen, or to simulate opening.
  // Ignored if the menu is already considered open.
  // When calling this method, the caller must hold a strong reference to this object, because other
  // references to this object can be dropped during the handling of the DOM event.
  void MenuOpened();

  // Called from the menu delegate during menuDidClose, or to simulate closing.
  // Ignored if the menu is already considered closed.
  // When calling this method, the caller must hold a strong reference to this object, because other
  // references to this object can be dropped during the handling of the DOM event.
  void MenuClosed();

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

  // nsMenuParentX
  void MenuChildChangedVisibility(const MenuChild& aChild, bool aIsVisible) override;

  void Dump(uint32_t aIndent) const;

  static bool IsXULHelpMenu(nsIContent* aMenuContent);

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
  void WillInsertChild(const MenuChild& aChild);
  void WillRemoveChild(const MenuChild& aChild);
  void AddMenuChild(MenuChild&& aChild);
  void InsertMenuChild(MenuChild&& aChild);
  void RemoveMenuChild(const MenuChild& aChild);
  mozilla::Maybe<MenuChild> CreateMenuChild(nsIContent* aContent);
  RefPtr<nsMenuItemX> CreateMenuItem(nsIContent* aMenuItemContent);
  GeckoNSMenu* CreateMenuWithGeckoString(nsString& aMenuTitle);
  void DidFirePopupShowing();

  // Find the index at which aChild needs to be inserted into mMenuChildren such that mMenuChildren
  // remains in correct content order, i.e. the order in mMenuChildren is the same as the order of
  // the DOM children of our <menupopup>.
  size_t FindInsertionIndex(const MenuChild& aChild);

  // Calculates the index at which aChild's NSMenuItem should be inserted into our NSMenu.
  // The order of NSMenuItems in the NSMenu is the same as the order of menu children in
  // mMenuChildren; the only difference is that mMenuChildren contains both visible and invisible
  // children, and the NSMenu only contains visible items. So the insertion index is equal to the
  // number of visible previous siblings of aChild in mMenuChildren.
  NSInteger CalculateNativeInsertionPoint(const MenuChild& aChild);

  // Fires the popupshown event.
  void MenuOpenedAsync();

  // Called from mPendingAsyncMenuCloseRunnable asynchronously after MenuClosed(), so that it runs
  // after any potential menuItemHit calls for clicked menu items.
  // Fires popuphiding and popuphidden events.
  // When calling this method, the caller must hold a strong reference to this object, because other
  // references to this object can be dropped during the handling of the DOM event.
  void MenuClosedAsync();

  // If mPendingAsyncMenuOpenRunnable is non-null, call MenuOpenedAsync() to send out the pending
  // popupshown event.
  void FlushMenuOpenedRunnable();

  // If mPendingAsyncMenuCloseRunnable is non-null, call MenuClosedAsync() to send out pending
  // popuphiding/popuphidden events.
  void FlushMenuClosedRunnable();

  // Make sure the NSMenu contains at least one item, even if mVisibleItemsCount is zero.
  // Otherwise it won't open.
  void InsertPlaceholderIfNeeded();
  // Remove the placeholder before adding an item to mNativeNSMenu.
  void RemovePlaceholderIfPresent();

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

  // Non-null between a call to MenuOpened() and MenuOpenedAsync().
  RefPtr<mozilla::CancelableRunnable> mPendingAsyncMenuOpenRunnable;

  // Non-null between a call to MenuClosed() and MenuClosedAsync().
  // This is asynchronous so that, if a menu item is clicked, we can fire popuphiding *after* we
  // execute the menu item command. The macOS menu system calls menuWillClose *before* it calls
  // menuItemHit.
  RefPtr<mozilla::CancelableRunnable> mPendingAsyncMenuCloseRunnable;

  // Any runnables for running asynchronous command events.
  // These are only used during automated tests, via ActivateItemAndClose.
  // We keep track of them here so that we can ensure they're run before popuphiding/popuphidden.
  nsTArray<RefPtr<mozilla::CancelableRunnable>> mPendingCommandRunnables;

  GeckoNSMenu* mNativeMenu = nil;     // [strong]
  MenuDelegate* mMenuDelegate = nil;  // [strong]
  // nsMenuX objects should always have a valid native menu item.
  NSMenuItem* mNativeMenuItem = nil;  // [strong]

  // Nothing() if no item is highlighted. The index only accounts for visible items.
  mozilla::Maybe<uint32_t> mHighlightedItemIndex;

  bool mIsEnabled = true;
  bool mNeedsRebuild = true;

  // Whether the native NSMenu is considered open.
  // Also affected by MenuOpened() / MenuClosed() calls for simulated opening / closing.
  bool mIsOpen = false;

  // Whether the popup is open from Gecko's perspective, based on popupshowing / popuphiding events.
  bool mIsOpenForGecko = false;

  bool mVisible = true;

  // true between an OnOpen() call that returned true, and the subsequent call
  // to MenuOpened().
  bool mDidFirePopupshowingAndIsApprovedToOpen = false;
};

#endif  // nsMenuX_h_
