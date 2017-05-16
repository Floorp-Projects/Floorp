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
#include "nsINativeMenuService.h"
#include "nsString.h"

class nsMenuBarX;
class nsMenuX;
class nsIWidget;
class nsIContent;

// ApplicationMenuDelegate is used to receive Cocoa notifications.
@interface ApplicationMenuDelegate : NSObject<NSMenuDelegate>
{
  nsMenuBarX* mApplicationMenu; // weak ref
}
- (id)initWithApplicationMenu:(nsMenuBarX*)aApplicationMenu;
@end

// The native menu service for creating native menu bars.
class nsNativeMenuServiceX : public nsINativeMenuService
{
public:
  NS_DECL_ISUPPORTS

  nsNativeMenuServiceX() {}

  NS_IMETHOD CreateNativeMenuBar(nsIWidget* aParent, nsIContent* aMenuBarNode) override;

protected:
  virtual ~nsNativeMenuServiceX() {}
};

// Objective-C class used to allow us to intervene with keyboard event handling.
// We allow mouse actions to work normally.
@interface GeckoNSMenu : NSMenu
{
}
- (BOOL)performSuperKeyEquivalent:(NSEvent*)theEvent;
@end

// Objective-C class used as action target for menu items
@interface NativeMenuItemTarget : NSObject
{
}
-(IBAction)menuItemHit:(id)sender;
@end

// Objective-C class used for menu items on the Services menu to allow Gecko
// to override their standard behavior in order to stop key equivalents from
// firing in certain instances.
@interface GeckoServicesNSMenuItem : NSMenuItem
{
}
- (id) target;
- (SEL) action;
- (void) _doNothing:(id)sender;
@end

// Objective-C class used as the Services menu so that Gecko can override the
// standard behavior of the Services menu in order to stop key equivalents
// from firing in certain instances.
@interface GeckoServicesNSMenu : NSMenu
{
}
- (void)addItem:(NSMenuItem *)newItem;
- (NSMenuItem *)addItemWithTitle:(NSString *)aString action:(SEL)aSelector keyEquivalent:(NSString *)keyEquiv;
- (void)insertItem:(NSMenuItem *)newItem atIndex:(NSInteger)index;
- (NSMenuItem *)insertItemWithTitle:(NSString *)aString action:(SEL)aSelector  keyEquivalent:(NSString *)keyEquiv atIndex:(NSInteger)index;
- (void) _overrideClassOfMenuItem:(NSMenuItem *)menuItem;
@end

// Once instantiated, this object lives until its DOM node or its parent window is destroyed.
// Do not hold references to this, they can become invalid any time the DOM node can be destroyed.
class nsMenuBarX : public nsMenuGroupOwnerX, public nsChangeObserver
{
public:
  nsMenuBarX();
  virtual ~nsMenuBarX();

  static NativeMenuItemTarget* sNativeEventTarget;
  static nsMenuBarX*           sLastGeckoMenuBarPainted;

  // The following content nodes have been removed from the menu system.
  // We save them here for use in command handling.
  nsCOMPtr<nsIContent> mAboutItemContent;
  nsCOMPtr<nsIContent> mPrefItemContent;
  nsCOMPtr<nsIContent> mQuitItemContent;

  // nsChangeObserver
  NS_DECL_CHANGEOBSERVER

  // nsMenuObjectX
  void*             NativeData() override {return (void*)mNativeMenu;}
  nsMenuObjectTypeX MenuObjectType() override {return eMenuBarObjectType;}

  // nsMenuBarX
  nsresult          Create(nsIWidget* aParent, nsIContent* aContent);
  void              SetParent(nsIWidget* aParent);
  uint32_t          GetMenuCount();
  bool              MenuContainsAppMenu();
  nsMenuX*          GetMenuAt(uint32_t aIndex);
  nsMenuX*          GetXULHelpMenu();
  void              SetSystemHelpMenu();
  nsresult          Paint();
  void              ForceUpdateNativeMenuAt(const nsAString& indexString);
  void              ForceNativeMenuReload(); // used for testing
  static char       GetLocalizedAccelKey(const char *shortcutID);
  static void       ResetNativeApplicationMenu();
  void              SetNeedsRebuild();
  void              ApplicationMenuOpened();
  bool              PerformKeyEquivalent(NSEvent* theEvent);

protected:
  void              ConstructNativeMenus();
  void              ConstructFallbackNativeMenus();
  nsresult          InsertMenuAtIndex(nsMenuX* aMenu, uint32_t aIndex);
  void              RemoveMenuAtIndex(uint32_t aIndex);
  void              HideItem(nsIDOMDocument* inDoc, const nsAString & inID, nsIContent** outHiddenNode);
  void              AquifyMenuBar();
  NSMenuItem*       CreateNativeAppMenuItem(nsMenuX* inMenu, const nsAString& nodeID, SEL action,
                                            int tag, NativeMenuItemTarget* target);
  nsresult          CreateApplicationMenu(nsMenuX* inMenu);

  nsTArray<mozilla::UniquePtr<nsMenuX>> mMenuArray;
  nsIWidget*         mParentWindow;        // [weak]
  GeckoNSMenu*       mNativeMenu;            // root menu, representing entire menu bar
  bool               mNeedsRebuild;
  ApplicationMenuDelegate* mApplicationMenuDelegate;
};

#endif // nsMenuBarX_h_
