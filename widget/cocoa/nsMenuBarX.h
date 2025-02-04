/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMenuBarX_h_
#define nsMenuBarX_h_

#import <Cocoa/Cocoa.h>

#include "mozilla/UniquePtr.h"
#include "mozilla/WeakPtr.h"

#include "nsISupports.h"
#include "nsMenuParentX.h"
#include "nsChangeObserver.h"
#include "nsTArray.h"
#include "nsString.h"

class nsMenuBarX;
class nsMenuGroupOwnerX;
class nsMenuX;
class nsIWidget;
class nsIContent;

namespace mozilla {
namespace dom {
class Document;
class Element;
}  // namespace dom
}  // namespace mozilla

// ApplicationMenuDelegate is used to receive Cocoa notifications.
@interface ApplicationMenuDelegate : NSObject <NSMenuDelegate> {
  nsMenuBarX* mApplicationMenu;  // weak ref
}
- (id)initWithApplicationMenu:(nsMenuBarX*)aApplicationMenu;
@end

// Objective-C class used for menu items to allow Gecko to override their
// standard behavior in order to stop key equivalents from firing in certain
// instances.
@interface GeckoNSMenuItem : NSMenuItem {
}
- (id)target;
- (SEL)action;
- (void)_doNothing:(id)aSender;
@end

// Objective-C class used to allow us to intervene with keyboard event handling,
// particularly to stop key equivalents from firing in certain instances. We
// allow mouse actions to work normally.
@interface GeckoNSMenu : NSMenu {
}
- (BOOL)performSuperKeyEquivalent:(NSEvent*)aEvent;
- (void)addItem:(NSMenuItem*)aNewItem;
- (NSMenuItem*)addItemWithTitle:(NSString*)aString
                         action:(SEL)aSelector
                  keyEquivalent:(NSString*)aKeyEquiv;
- (void)insertItem:(NSMenuItem*)aNewItem atIndex:(NSInteger)aIndex;
- (NSMenuItem*)insertItemWithTitle:(NSString*)aString
                            action:(SEL)aSelector
                     keyEquivalent:(NSString*)aKeyEquiv
                           atIndex:(NSInteger)aIndex;
- (void)_overrideClassOfMenuItem:(NSMenuItem*)aMenuItem;
@end

// Objective-C class used as action target for menu items.
@interface NativeMenuItemTarget : NSObject {
}
- (IBAction)menuItemHit:(id)aSender;
@end

// Once instantiated, this object lives until its DOM node or its parent window
// is destroyed. Do not hold references to this, they can become invalid any
// time the DOM node can be destroyed.
class nsMenuBarX : public nsMenuParentX,
                   public nsChangeObserver,
                   public mozilla::SupportsWeakPtr {
 public:
  explicit nsMenuBarX(mozilla::dom::Element* aElement);

  NS_INLINE_DECL_REFCOUNTING(nsMenuBarX)

  static NativeMenuItemTarget* sNativeEventTarget;
  static nsMenuBarX* sLastGeckoMenuBarPainted;

  // The following content nodes have been removed from the menu system.
  // We save them here for use in command handling.
  RefPtr<nsIContent> mAboutItemContent;
  RefPtr<nsIContent> mPrefItemContent;
  RefPtr<nsIContent> mAccountItemContent;
  RefPtr<nsIContent> mQuitItemContent;

  // nsChangeObserver
  NS_DECL_CHANGEOBSERVER

  // nsMenuParentX
  nsMenuBarX* AsMenuBar() override { return this; }

  // nsMenuBarX
  uint32_t GetMenuCount();
  bool MenuContainsAppMenu();
  nsMenuX* GetMenuAt(uint32_t aIndex);
  nsMenuX* GetXULHelpMenu();
  void SetSystemHelpMenu();
  nsresult Paint();
  void ForceUpdateNativeMenuAt(const nsAString& aIndexString);
  void ForceNativeMenuReload();  // used for testing
  static void ResetNativeApplicationMenu();
  void SetNeedsRebuild();
  void ApplicationMenuOpened();
  bool PerformKeyEquivalent(NSEvent* aEvent);
  GeckoNSMenu* NativeNSMenu() { return mNativeMenu; }

  // nsMenuParentX
  void MenuChildChangedVisibility(const MenuChild& aChild,
                                  bool aIsVisible) override;

 protected:
  virtual ~nsMenuBarX();

  void ConstructNativeMenus();
  void ConstructFallbackNativeMenus();
  void InsertMenuAtIndex(RefPtr<nsMenuX>&& aMenu, uint32_t aIndex);
  void RemoveMenuAtIndex(uint32_t aIndex);
  RefPtr<mozilla::dom::Element> HideItem(mozilla::dom::Document* aDocument,
                                         const nsAString& aID);
  void AquifyMenuBar();
  NSMenuItem* CreateNativeAppMenuItem(nsMenuX* aMenu, const nsAString& aNodeID,
                                      SEL aAction, int aTag,
                                      NativeMenuItemTarget* aTarget);
  void CreateApplicationMenu(nsMenuX* aMenu);

  // Calculates the index at which aChild's NSMenuItem should be inserted into
  // our NSMenu. The order of NSMenuItems in the NSMenu is the same as the order
  // of nsMenuX objects in mMenuArray; there are two differences:
  //  - mMenuArray contains both visible and invisible menus, and the NSMenu
  //  only contains visible
  //    menus.
  //  - Our NSMenu may also contain an item for the app menu, whereas mMenuArray
  //  never does.
  // So the insertion index is equal to the number of visible previous siblings
  // of aChild in mMenuArray, plus one if the app menu is present.
  NSInteger CalculateNativeInsertionPoint(nsMenuX* aChild);

  RefPtr<nsIContent> mContent;
  RefPtr<nsMenuGroupOwnerX> mMenuGroupOwner;
  nsTArray<RefPtr<nsMenuX>> mMenuArray;
  GeckoNSMenu* mNativeMenu;  // root menu, representing entire menu bar
  bool mNeedsRebuild;
  ApplicationMenuDelegate* mApplicationMenuDelegate;
};

#endif  // nsMenuBarX_h_
