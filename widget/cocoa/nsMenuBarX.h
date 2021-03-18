/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMenuBarX_h_
#define nsMenuBarX_h_

#import <Cocoa/Cocoa.h>

#include "mozilla/UniquePtr.h"
#include "nsMenuBaseX.h"
#include "nsMenuGroupOwnerX.h"
#include "nsChangeObserver.h"
#include "nsString.h"

class nsMenuBarX;
class nsMenuX;
class nsIWidget;
class nsIContent;

namespace mozilla {
namespace dom {
class Document;
class Element;
}
}

// ApplicationMenuDelegate is used to receive Cocoa notifications.
@interface ApplicationMenuDelegate : NSObject <NSMenuDelegate> {
  nsMenuBarX* mApplicationMenu;  // weak ref
}
- (id)initWithApplicationMenu:(nsMenuBarX*)aApplicationMenu;
@end

// Objective-C class used to allow us to intervene with keyboard event handling.
// We allow mouse actions to work normally.
@interface GeckoNSMenu : NSMenu {
}
- (BOOL)performSuperKeyEquivalent:(NSEvent*)aEvent;
@end

// Objective-C class used as action target for menu items
@interface NativeMenuItemTarget : NSObject {
}
- (IBAction)menuItemHit:(id)aSender;
@end

// Objective-C class used for menu items on the Services menu to allow Gecko
// to override their standard behavior in order to stop key equivalents from
// firing in certain instances.
@interface GeckoServicesNSMenuItem : NSMenuItem {
}
- (id)target;
- (SEL)action;
- (void)_doNothing:(id)aSender;
@end

// Objective-C class used as the Services menu so that Gecko can override the
// standard behavior of the Services menu in order to stop key equivalents
// from firing in certain instances.
@interface GeckoServicesNSMenu : NSMenu {
}
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

// Once instantiated, this object lives until its DOM node or its parent window is destroyed.
// Do not hold references to this, they can become invalid any time the DOM node can be destroyed.
class nsMenuBarX : public nsMenuGroupOwnerX, public nsChangeObserver {
 public:
  nsMenuBarX();
  virtual ~nsMenuBarX();

  static NativeMenuItemTarget* sNativeEventTarget;
  static nsMenuBarX* sLastGeckoMenuBarPainted;

  // The following content nodes have been removed from the menu system.
  // We save them here for use in command handling.
  RefPtr<nsIContent> mAboutItemContent;
  RefPtr<nsIContent> mPrefItemContent;
  RefPtr<nsIContent> mQuitItemContent;

  // nsChangeObserver
  NS_DECL_CHANGEOBSERVER

  // nsMenuObjectX
  nsMenuObjectTypeX MenuObjectType() override { return eMenuBarObjectType; }

  // nsMenuBarX
  nsresult Create(mozilla::dom::Element* aElement);
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

 protected:
  void ConstructNativeMenus();
  void ConstructFallbackNativeMenus();
  void InsertMenuAtIndex(mozilla::UniquePtr<nsMenuX>&& aMenu, uint32_t aIndex);
  void RemoveMenuAtIndex(uint32_t aIndex);
  RefPtr<mozilla::dom::Element> HideItem(mozilla::dom::Document* aDocument, const nsAString& aID);
  void AquifyMenuBar();
  NSMenuItem* CreateNativeAppMenuItem(nsMenuX* aMenu, const nsAString& aNodeID, SEL aAction,
                                      int aTag, NativeMenuItemTarget* aTarget);
  void CreateApplicationMenu(nsMenuX* aMenu);

  nsTArray<mozilla::UniquePtr<nsMenuX>> mMenuArray;
  GeckoNSMenu* mNativeMenu;  // root menu, representing entire menu bar
  bool mNeedsRebuild;
  ApplicationMenuDelegate* mApplicationMenuDelegate;
};

#endif  // nsMenuBarX_h_
