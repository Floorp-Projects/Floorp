/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <objc/objc-runtime.h>

#include "nsMenuBarX.h"
#include "nsMenuX.h"
#include "nsMenuItemX.h"
#include "nsMenuUtilsX.h"
#include "nsCocoaUtils.h"
#include "nsChildView.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsGkAtoms.h"
#include "nsObjCExceptions.h"
#include "nsThreadUtils.h"
#include "nsTouchBarNativeAPIDefines.h"

#include "nsIContent.h"
#include "nsIWidget.h"
#include "mozilla/dom/Document.h"
#include "nsIAppStartup.h"
#include "nsIStringBundle.h"
#include "nsToolkitCompsCID.h"

#include "mozilla/Components.h"
#include "mozilla/dom/Element.h"

using namespace mozilla;
using mozilla::dom::Element;

NativeMenuItemTarget* nsMenuBarX::sNativeEventTarget = nil;
nsMenuBarX* nsMenuBarX::sLastGeckoMenuBarPainted = nullptr;
NSMenu* sApplicationMenu = nil;
BOOL sApplicationMenuIsFallback = NO;
BOOL gSomeMenuBarPainted = NO;

// defined in nsCocoaWindow.mm.
extern BOOL sTouchBarIsInitialized;

// We keep references to the first quit and pref item content nodes we find, which
// will be from the hidden window. We use these when the document for the current
// window does not have a quit or pref item. We don't need strong refs here because
// these items are always strong ref'd by their owning menu bar (instance variable).
static nsIContent* sAboutItemContent = nullptr;
static nsIContent* sPrefItemContent = nullptr;
static nsIContent* sAccountItemContent = nullptr;
static nsIContent* sQuitItemContent = nullptr;

//
// ApplicationMenuDelegate Objective-C class
//

@implementation ApplicationMenuDelegate

- (id)initWithApplicationMenu:(nsMenuBarX*)aApplicationMenu {
  if ((self = [super init])) {
    mApplicationMenu = aApplicationMenu;
  }
  return self;
}

- (void)menuWillOpen:(NSMenu*)menu {
  mApplicationMenu->ApplicationMenuOpened();
}

- (void)menuDidClose:(NSMenu*)menu {
}

@end

nsMenuBarX::nsMenuBarX(mozilla::dom::Element* aElement)
    : mNeedsRebuild(false), mApplicationMenuDelegate(nil) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  mMenuGroupOwner = new nsMenuGroupOwnerX(aElement, this);
  mMenuGroupOwner->RegisterForLocaleChanges();
  mNativeMenu = [[GeckoNSMenu alloc] initWithTitle:@"MainMenuBar"];

  mContent = aElement;

  if (mContent) {
    AquifyMenuBar();
    mMenuGroupOwner->RegisterForContentChanges(mContent, this);
    ConstructNativeMenus();
  } else {
    ConstructFallbackNativeMenus();
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsMenuBarX::~nsMenuBarX() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (nsMenuBarX::sLastGeckoMenuBarPainted == this) {
    nsMenuBarX::sLastGeckoMenuBarPainted = nullptr;
  }

  // the quit/pref items of a random window might have been used if there was no
  // hidden window, thus we need to invalidate the weak references.
  if (sAboutItemContent == mAboutItemContent) {
    sAboutItemContent = nullptr;
  }
  if (sQuitItemContent == mQuitItemContent) {
    sQuitItemContent = nullptr;
  }
  if (sPrefItemContent == mPrefItemContent) {
    sPrefItemContent = nullptr;
  }
  if (sAccountItemContent == mAccountItemContent) {
    sAccountItemContent = nullptr;
  }

  mMenuGroupOwner->UnregisterForLocaleChanges();

  // make sure we unregister ourselves as a content observer
  if (mContent) {
    mMenuGroupOwner->UnregisterForContentChanges(mContent);
  }

  for (nsMenuX* menu : mMenuArray) {
    menu->DetachFromGroupOwnerRecursive();
    menu->DetachFromParent();
  }

  if (mApplicationMenuDelegate) {
    [mApplicationMenuDelegate release];
  }

  [mNativeMenu release];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsMenuBarX::ConstructNativeMenus() {
  for (nsIContent* menuContent = mContent->GetFirstChild(); menuContent;
       menuContent = menuContent->GetNextSibling()) {
    if (menuContent->IsXULElement(nsGkAtoms::menu)) {
      InsertMenuAtIndex(MakeRefPtr<nsMenuX>(this, mMenuGroupOwner, menuContent->AsElement()),
                        GetMenuCount());
    }
  }
}

void nsMenuBarX::ConstructFallbackNativeMenus() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (sApplicationMenu) {
    // Menu has already been built.
    return;
  }

  nsCOMPtr<nsIStringBundle> stringBundle;

  nsCOMPtr<nsIStringBundleService> bundleSvc = do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  bundleSvc->CreateBundle("chrome://global/locale/fallbackMenubar.properties",
                          getter_AddRefs(stringBundle));

  if (!stringBundle) {
    return;
  }

  nsAutoString labelUTF16;
  nsAutoString keyUTF16;

  const char* labelProp = "quitMenuitem.label";
  const char* keyProp = "quitMenuitem.key";

  stringBundle->GetStringFromName(labelProp, labelUTF16);
  stringBundle->GetStringFromName(keyProp, keyUTF16);

  NSString* labelStr = [NSString stringWithUTF8String:NS_ConvertUTF16toUTF8(labelUTF16).get()];
  NSString* keyStr = [NSString stringWithUTF8String:NS_ConvertUTF16toUTF8(keyUTF16).get()];

  if (!nsMenuBarX::sNativeEventTarget) {
    nsMenuBarX::sNativeEventTarget = [[NativeMenuItemTarget alloc] init];
  }

  sApplicationMenu = [[[[NSApp mainMenu] itemAtIndex:0] submenu] retain];
  if (!mApplicationMenuDelegate) {
    mApplicationMenuDelegate = [[ApplicationMenuDelegate alloc] initWithApplicationMenu:this];
  }
  sApplicationMenu.delegate = mApplicationMenuDelegate;
  NSMenuItem* quitMenuItem = [[[NSMenuItem alloc] initWithTitle:labelStr
                                                         action:@selector(menuItemHit:)
                                                  keyEquivalent:keyStr] autorelease];
  quitMenuItem.target = nsMenuBarX::sNativeEventTarget;
  quitMenuItem.tag = eCommand_ID_Quit;
  [sApplicationMenu addItem:quitMenuItem];
  sApplicationMenuIsFallback = YES;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

uint32_t nsMenuBarX::GetMenuCount() { return mMenuArray.Length(); }

bool nsMenuBarX::MenuContainsAppMenu() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  return (mNativeMenu.numberOfItems > 0 && [mNativeMenu itemAtIndex:0].submenu == sApplicationMenu);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsMenuBarX::InsertMenuAtIndex(RefPtr<nsMenuX>&& aMenu, uint32_t aIndex) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // If we've only yet created a fallback global Application menu (using
  // ContructFallbackNativeMenus()), destroy it before recreating it properly.
  if (sApplicationMenu && sApplicationMenuIsFallback) {
    ResetNativeApplicationMenu();
  }
  // If we haven't created a global Application menu yet, do it.
  if (!sApplicationMenu) {
    CreateApplicationMenu(aMenu.get());

    // Hook the new Application menu up to the menu bar.
    NSMenu* mainMenu = NSApp.mainMenu;
    NS_ASSERTION(mainMenu.numberOfItems > 0,
                 "Main menu does not have any items, something is terribly wrong!");
    [mainMenu itemAtIndex:0].submenu = sApplicationMenu;
  }

  // add menu to array that owns our menus
  mMenuArray.InsertElementAt(aIndex, aMenu);

  // hook up submenus
  RefPtr<nsIContent> menuContent = aMenu->Content();
  if (menuContent->GetChildCount() > 0 && !nsMenuUtilsX::NodeIsHiddenOrCollapsed(menuContent)) {
    MenuChildChangedVisibility(MenuChild(aMenu), true);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsMenuBarX::RemoveMenuAtIndex(uint32_t aIndex) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mMenuArray.Length() <= aIndex) {
    NS_ERROR("Attempting submenu removal with bad index!");
    return;
  }

  RefPtr<nsMenuX> menu = mMenuArray[aIndex];
  mMenuArray.RemoveElementAt(aIndex);

  menu->DetachFromGroupOwnerRecursive();
  menu->DetachFromParent();

  // Our native menu and our internal menu object array might be out of sync.
  // This happens, for example, when a submenu is hidden. Because of this we
  // should not assume that a native submenu is hooked up.
  NSMenuItem* nativeMenuItem = menu->NativeNSMenuItem();
  int nativeMenuItemIndex = [mNativeMenu indexOfItem:nativeMenuItem];
  if (nativeMenuItemIndex != -1) {
    [mNativeMenu removeItemAtIndex:nativeMenuItemIndex];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsMenuBarX::ObserveAttributeChanged(mozilla::dom::Document* aDocument, nsIContent* aContent,
                                         nsAtom* aAttribute) {}

void nsMenuBarX::ObserveContentRemoved(mozilla::dom::Document* aDocument, nsIContent* aContainer,
                                       nsIContent* aChild, nsIContent* aPreviousSibling) {
  nsINode* parent = NODE_FROM(aContainer, aDocument);
  MOZ_ASSERT(parent);
  const Maybe<uint32_t> index = parent->ComputeIndexOf(aPreviousSibling);
  MOZ_ASSERT(*index != UINT32_MAX);
  RemoveMenuAtIndex(index.isSome() ? *index + 1u : 0u);
}

void nsMenuBarX::ObserveContentInserted(mozilla::dom::Document* aDocument, nsIContent* aContainer,
                                        nsIContent* aChild) {
  InsertMenuAtIndex(MakeRefPtr<nsMenuX>(this, mMenuGroupOwner, aChild),
                    aContainer->ComputeIndexOf(aChild).valueOr(UINT32_MAX));
}

void nsMenuBarX::ForceUpdateNativeMenuAt(const nsAString& aIndexString) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NSString* locationString =
      [NSString stringWithCharacters:reinterpret_cast<const unichar*>(aIndexString.BeginReading())
                              length:aIndexString.Length()];
  NSArray* indexes = [locationString componentsSeparatedByString:@"|"];
  unsigned int indexCount = indexes.count;
  if (indexCount == 0) {
    return;
  }

  RefPtr<nsMenuX> currentMenu = nullptr;
  int targetIndex = [[indexes objectAtIndex:0] intValue];
  int visible = 0;
  uint32_t length = mMenuArray.Length();
  // first find a menu in the menu bar
  for (unsigned int i = 0; i < length; i++) {
    RefPtr<nsMenuX> menu = mMenuArray[i];
    if (!nsMenuUtilsX::NodeIsHiddenOrCollapsed(menu->Content())) {
      visible++;
      if (visible == (targetIndex + 1)) {
        currentMenu = std::move(menu);
        break;
      }
    }
  }

  if (!currentMenu) {
    return;
  }

  // fake open/close to cause lazy update to happen so submenus populate
  currentMenu->MenuOpened();
  currentMenu->MenuClosed();

  // now find the correct submenu
  for (unsigned int i = 1; currentMenu && i < indexCount; i++) {
    targetIndex = [[indexes objectAtIndex:i] intValue];
    visible = 0;
    length = currentMenu->GetItemCount();
    for (unsigned int j = 0; j < length; j++) {
      Maybe<nsMenuX::MenuChild> targetMenu = currentMenu->GetItemAt(j);
      if (!targetMenu) {
        return;
      }
      RefPtr<nsIContent> content = targetMenu->match(
          [](const RefPtr<nsMenuX>& aMenu) { return aMenu->Content(); },
          [](const RefPtr<nsMenuItemX>& aMenuItem) { return aMenuItem->Content(); });
      if (!nsMenuUtilsX::NodeIsHiddenOrCollapsed(content)) {
        visible++;
        if (targetMenu->is<RefPtr<nsMenuX>>() && visible == (targetIndex + 1)) {
          currentMenu = targetMenu->as<RefPtr<nsMenuX>>();
          // fake open/close to cause lazy update to happen
          currentMenu->MenuOpened();
          currentMenu->MenuClosed();
          break;
        }
      }
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Calling this forces a full reload of the menu system, reloading all native
// menus and their items.
// Without this testing is hard because changes to the DOM affect the native
// menu system lazily.
void nsMenuBarX::ForceNativeMenuReload() {
  // tear down everything
  while (GetMenuCount() > 0) {
    RemoveMenuAtIndex(0);
  }

  // construct everything
  ConstructNativeMenus();
}

nsMenuX* nsMenuBarX::GetMenuAt(uint32_t aIndex) {
  if (mMenuArray.Length() <= aIndex) {
    NS_ERROR("Requesting menu at invalid index!");
    return nullptr;
  }
  return mMenuArray[aIndex].get();
}

nsMenuX* nsMenuBarX::GetXULHelpMenu() {
  // The Help menu is usually (always?) the last one, so we start there and
  // count back.
  for (int32_t i = GetMenuCount() - 1; i >= 0; --i) {
    nsMenuX* aMenu = GetMenuAt(i);
    if (aMenu && nsMenuX::IsXULHelpMenu(aMenu->Content())) {
      return aMenu;
    }
  }
  return nil;
}

// On SnowLeopard and later we must tell the OS which is our Help menu.
// Otherwise it will only add Spotlight for Help (the Search item) to our
// Help menu if its label/title is "Help" -- i.e. if the menu is in English.
// This resolves bugs 489196 and 539317.
void nsMenuBarX::SetSystemHelpMenu() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  nsMenuX* xulHelpMenu = GetXULHelpMenu();
  if (xulHelpMenu) {
    NSMenu* helpMenu = xulHelpMenu->NativeNSMenu();
    if (helpMenu) {
      NSApp.helpMenu = helpMenu;
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsresult nsMenuBarX::Paint() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Don't try to optimize anything in this painting by checking
  // sLastGeckoMenuBarPainted because the menubar can be manipulated by
  // native dialogs and sheet code and other things besides this paint method.

  // We have to keep the same menu item for the Application menu so we keep
  // passing it along.
  NSMenu* outgoingMenu = NSApp.mainMenu;
  NS_ASSERTION(outgoingMenu.numberOfItems > 0,
               "Main menu does not have any items, something is terribly wrong!");

  NSMenuItem* appMenuItem = [[outgoingMenu itemAtIndex:0] retain];
  [outgoingMenu removeItemAtIndex:0];
  [mNativeMenu insertItem:appMenuItem atIndex:0];
  [appMenuItem release];

  // Set menu bar and event target.
  NSApp.mainMenu = mNativeMenu;
  SetSystemHelpMenu();
  nsMenuBarX::sLastGeckoMenuBarPainted = this;

  gSomeMenuBarPainted = YES;

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

/* static */
void nsMenuBarX::ResetNativeApplicationMenu() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [sApplicationMenu removeAllItems];
  [sApplicationMenu release];
  sApplicationMenu = nil;
  sApplicationMenuIsFallback = NO;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsMenuBarX::SetNeedsRebuild() { mNeedsRebuild = true; }

void nsMenuBarX::ApplicationMenuOpened() {
  if (mNeedsRebuild) {
    if (!mMenuArray.IsEmpty()) {
      ResetNativeApplicationMenu();
      CreateApplicationMenu(mMenuArray[0].get());
    }
    mNeedsRebuild = false;
  }
}

bool nsMenuBarX::PerformKeyEquivalent(NSEvent* aEvent) {
  return [mNativeMenu performSuperKeyEquivalent:aEvent];
}

void nsMenuBarX::MenuChildChangedVisibility(const MenuChild& aChild, bool aIsVisible) {
  MOZ_RELEASE_ASSERT(aChild.is<RefPtr<nsMenuX>>(), "nsMenuBarX only has nsMenuX children");
  const RefPtr<nsMenuX>& child = aChild.as<RefPtr<nsMenuX>>();
  NSMenuItem* item = child->NativeNSMenuItem();
  if (aIsVisible) {
    NSInteger insertionPoint = CalculateNativeInsertionPoint(child);
    [mNativeMenu insertItem:child->NativeNSMenuItem() atIndex:insertionPoint];
  } else if ([mNativeMenu indexOfItem:item] != -1) {
    [mNativeMenu removeItem:item];
  }
}

NSInteger nsMenuBarX::CalculateNativeInsertionPoint(nsMenuX* aChild) {
  NSInteger insertionPoint = MenuContainsAppMenu() ? 1 : 0;
  for (auto& currMenu : mMenuArray) {
    if (currMenu == aChild) {
      return insertionPoint;
    }
    // Only count items that are inside a menu.
    // XXXmstange Not sure what would cause free-standing items. Maybe for collapsed/hidden menus?
    // In that case, an nsMenuX::IsVisible() method would be better.
    if (currMenu->NativeNSMenuItem().menu) {
      insertionPoint++;
    }
  }
  return insertionPoint;
}

// Hide the item in the menu by setting the 'hidden' attribute. Returns it so
// the caller can hang onto it if they so choose.
RefPtr<Element> nsMenuBarX::HideItem(mozilla::dom::Document* aDocument, const nsAString& aID) {
  RefPtr<Element> menuElement = aDocument->GetElementById(aID);
  if (menuElement) {
    menuElement->SetAttr(kNameSpaceID_None, nsGkAtoms::hidden, u"true"_ns, false);
  }
  return menuElement;
}

// Do what is necessary to conform to the Aqua guidelines for menus.
void nsMenuBarX::AquifyMenuBar() {
  RefPtr<mozilla::dom::Document> domDoc = mContent->GetComposedDoc();
  if (domDoc) {
    // remove the "About..." item and its separator
    HideItem(domDoc, u"aboutSeparator"_ns);
    mAboutItemContent = HideItem(domDoc, u"aboutName"_ns);
    if (!sAboutItemContent) {
      sAboutItemContent = mAboutItemContent;
    }

    // remove quit item and its separator
    HideItem(domDoc, u"menu_FileQuitSeparator"_ns);
    mQuitItemContent = HideItem(domDoc, u"menu_FileQuitItem"_ns);
    if (!sQuitItemContent) {
      sQuitItemContent = mQuitItemContent;
    }

    // remove prefs item and its separator, but save off the pref content node
    // so we can invoke its command later.
    HideItem(domDoc, u"menu_PrefsSeparator"_ns);
    mPrefItemContent = HideItem(domDoc, u"menu_preferences"_ns);
    if (!sPrefItemContent) {
      sPrefItemContent = mPrefItemContent;
    }

    // remove Account Settings item.
    mAccountItemContent = HideItem(domDoc, u"menu_accountmgr"_ns);
    if (!sAccountItemContent) {
      sAccountItemContent = mAccountItemContent;
    }

    // hide items that we use for the Application menu
    HideItem(domDoc, u"menu_mac_services"_ns);
    HideItem(domDoc, u"menu_mac_hide_app"_ns);
    HideItem(domDoc, u"menu_mac_hide_others"_ns);
    HideItem(domDoc, u"menu_mac_show_all"_ns);
    HideItem(domDoc, u"menu_mac_touch_bar"_ns);
  }
}

// for creating menu items destined for the Application menu
NSMenuItem* nsMenuBarX::CreateNativeAppMenuItem(nsMenuX* aMenu, const nsAString& aNodeID,
                                                SEL aAction, int aTag,
                                                NativeMenuItemTarget* aTarget) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  RefPtr<mozilla::dom::Document> doc = aMenu->Content()->GetUncomposedDoc();
  if (!doc) {
    return nil;
  }

  RefPtr<mozilla::dom::Element> menuItem = doc->GetElementById(aNodeID);
  if (!menuItem) {
    return nil;
  }

  // Check collapsed rather than hidden since the app menu items are always
  // hidden in AquifyMenuBar.
  if (menuItem->AttrValueIs(kNameSpaceID_None, nsGkAtoms::collapsed, nsGkAtoms::_true,
                            eCaseMatters)) {
    return nil;
  }

  // Get information from the gecko menu item
  nsAutoString label;
  nsAutoString modifiers;
  nsAutoString key;
  menuItem->GetAttr(nsGkAtoms::label, label);
  menuItem->GetAttr(nsGkAtoms::modifiers, modifiers);
  menuItem->GetAttr(nsGkAtoms::key, key);

  // Get more information about the key equivalent. Start by
  // finding the key node we need.
  NSString* keyEquiv = nil;
  unsigned int macKeyModifiers = 0;
  if (!key.IsEmpty()) {
    RefPtr<Element> keyElement = doc->GetElementById(key);
    if (keyElement) {
      // first grab the key equivalent character
      nsAutoString keyChar(u" "_ns);
      keyElement->GetAttr(kNameSpaceID_None, nsGkAtoms::key, keyChar);
      if (!keyChar.EqualsLiteral(" ")) {
        keyEquiv = [[NSString stringWithCharacters:reinterpret_cast<const unichar*>(keyChar.get())
                                            length:keyChar.Length()] lowercaseString];
      }
      // now grab the key equivalent modifiers
      nsAutoString modifiersStr;
      keyElement->GetAttr(kNameSpaceID_None, nsGkAtoms::modifiers, modifiersStr);
      uint8_t geckoModifiers = nsMenuUtilsX::GeckoModifiersForNodeAttribute(modifiersStr);
      macKeyModifiers = nsMenuUtilsX::MacModifiersForGeckoModifiers(geckoModifiers);
    }
  }
  // get the label into NSString-form
  NSString* labelString =
      [NSString stringWithCharacters:reinterpret_cast<const unichar*>(label.get())
                              length:label.Length()];

  if (!labelString) {
    labelString = @"";
  }
  if (!keyEquiv) {
    keyEquiv = @"";
  }

  // put together the actual NSMenuItem
  NSMenuItem* newMenuItem = [[NSMenuItem alloc] initWithTitle:labelString
                                                       action:aAction
                                                keyEquivalent:keyEquiv];

  newMenuItem.tag = aTag;
  newMenuItem.target = aTarget;
  newMenuItem.keyEquivalentModifierMask = macKeyModifiers;
  newMenuItem.representedObject = mMenuGroupOwner->GetRepresentedObject();

  return newMenuItem;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// build the Application menu shared by all menu bars
void nsMenuBarX::CreateApplicationMenu(nsMenuX* aMenu) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // At this point, the application menu is the application menu from
  // the nib in cocoa widgets. We do not have a way to create an application
  // menu manually, so we grab the one from the nib and use that.
  sApplicationMenu = [[NSApp.mainMenu itemAtIndex:0].submenu retain];

  /*
    We support the following menu items here:

    Menu Item                DOM Node ID             Notes

    ========================
    = About This App       = <- aboutName
    ========================
    = Preferences...       = <- menu_preferences
    = Account Settings     = <- menu_accountmgr      Only on Thunderbird
    ========================
    = Services     >       = <- menu_mac_services    <- (do not define key equivalent)
    ========================
    = Hide App             = <- menu_mac_hide_app
    = Hide Others          = <- menu_mac_hide_others
    = Show All             = <- menu_mac_show_all
    ========================
    = Customize Touch Bar… = <- menu_mac_touch_bar
    ========================
    = Quit                 = <- menu_FileQuitItem
    ========================

    If any of them are ommitted from the application's DOM, we just don't add
    them. We always add a "Quit" item, but if an app developer does not provide a
    DOM node with the right ID for the Quit item, we add it in English. App
    developers need only add each node with a label and a key equivalent (if they
    want one). Other attributes are optional. Like so:

    <menuitem id="menu_preferences"
           label="&preferencesCmdMac.label;"
             key="open_prefs_key"/>

    We need to use this system for localization purposes, until we have a better way
    to define the Application menu to be used on Mac OS X.
  */

  if (sApplicationMenu) {
    if (!mApplicationMenuDelegate) {
      mApplicationMenuDelegate = [[ApplicationMenuDelegate alloc] initWithApplicationMenu:this];
    }
    sApplicationMenu.delegate = mApplicationMenuDelegate;

    // This code reads attributes we are going to care about from the DOM elements

    NSMenuItem* itemBeingAdded = nil;
    BOOL addAboutSeparator = FALSE;
    BOOL addPrefsSeparator = FALSE;

    // Add the About menu item
    itemBeingAdded = CreateNativeAppMenuItem(aMenu, u"aboutName"_ns, @selector(menuItemHit:),
                                             eCommand_ID_About, nsMenuBarX::sNativeEventTarget);
    if (itemBeingAdded) {
      [sApplicationMenu addItem:itemBeingAdded];
      [itemBeingAdded release];
      itemBeingAdded = nil;

      addAboutSeparator = TRUE;
    }

    // Add separator if either the About item or software update item exists
    if (addAboutSeparator) {
      [sApplicationMenu addItem:[NSMenuItem separatorItem]];
    }

    // Add the Preferences menu item
    itemBeingAdded = CreateNativeAppMenuItem(aMenu, u"menu_preferences"_ns, @selector(menuItemHit:),
                                             eCommand_ID_Prefs, nsMenuBarX::sNativeEventTarget);
    if (itemBeingAdded) {
      [sApplicationMenu addItem:itemBeingAdded];
      [itemBeingAdded release];
      itemBeingAdded = nil;

      addPrefsSeparator = TRUE;
    }

    // Add the Account Settings menu item. This is Thunderbird only
    itemBeingAdded = CreateNativeAppMenuItem(aMenu, u"menu_accountmgr"_ns, @selector(menuItemHit:),
                                             eCommand_ID_Account, nsMenuBarX::sNativeEventTarget);
    if (itemBeingAdded) {
      [sApplicationMenu addItem:itemBeingAdded];
      [itemBeingAdded release];
      itemBeingAdded = nil;
    }

    // Add separator after Preferences menu
    if (addPrefsSeparator) {
      [sApplicationMenu addItem:[NSMenuItem separatorItem]];
    }

    // Add Services menu item
    itemBeingAdded = CreateNativeAppMenuItem(aMenu, u"menu_mac_services"_ns, nil, 0, nil);
    if (itemBeingAdded) {
      [sApplicationMenu addItem:itemBeingAdded];

      // set this menu item up as the Mac OS X Services menu
      NSMenu* servicesMenu = [[GeckoServicesNSMenu alloc] initWithTitle:@""];
      itemBeingAdded.submenu = servicesMenu;
      NSApp.servicesMenu = servicesMenu;

      [itemBeingAdded release];
      itemBeingAdded = nil;

      // Add separator after Services menu
      [sApplicationMenu addItem:[NSMenuItem separatorItem]];
    }

    BOOL addHideShowSeparator = FALSE;

    // Add menu item to hide this application
    itemBeingAdded =
        CreateNativeAppMenuItem(aMenu, u"menu_mac_hide_app"_ns, @selector(menuItemHit:),
                                eCommand_ID_HideApp, nsMenuBarX::sNativeEventTarget);
    if (itemBeingAdded) {
      [sApplicationMenu addItem:itemBeingAdded];
      [itemBeingAdded release];
      itemBeingAdded = nil;

      addHideShowSeparator = TRUE;
    }

    // Add menu item to hide other applications
    itemBeingAdded =
        CreateNativeAppMenuItem(aMenu, u"menu_mac_hide_others"_ns, @selector(menuItemHit:),
                                eCommand_ID_HideOthers, nsMenuBarX::sNativeEventTarget);
    if (itemBeingAdded) {
      [sApplicationMenu addItem:itemBeingAdded];
      [itemBeingAdded release];
      itemBeingAdded = nil;

      addHideShowSeparator = TRUE;
    }

    // Add menu item to show all applications
    itemBeingAdded =
        CreateNativeAppMenuItem(aMenu, u"menu_mac_show_all"_ns, @selector(menuItemHit:),
                                eCommand_ID_ShowAll, nsMenuBarX::sNativeEventTarget);
    if (itemBeingAdded) {
      [sApplicationMenu addItem:itemBeingAdded];
      [itemBeingAdded release];
      itemBeingAdded = nil;

      addHideShowSeparator = TRUE;
    }

    // Add a separator after the hide/show menus if at least one exists
    if (addHideShowSeparator) {
      [sApplicationMenu addItem:[NSMenuItem separatorItem]];
    }

    BOOL addTouchBarSeparator = NO;

    // Add Touch Bar customization menu item.
    itemBeingAdded =
        CreateNativeAppMenuItem(aMenu, u"menu_mac_touch_bar"_ns, @selector(menuItemHit:),
                                eCommand_ID_TouchBar, nsMenuBarX::sNativeEventTarget);

    if (itemBeingAdded) {
      [sApplicationMenu addItem:itemBeingAdded];
      // We hide the menu item on Macs that don't have a Touch Bar.
      if (!sTouchBarIsInitialized) {
        [itemBeingAdded setHidden:YES];
      } else {
        addTouchBarSeparator = YES;
      }
      [itemBeingAdded release];
      itemBeingAdded = nil;
    }

    // Add a separator after the Touch Bar menu item if it exists
    if (addTouchBarSeparator) {
      [sApplicationMenu addItem:[NSMenuItem separatorItem]];
    }

    // Add quit menu item
    itemBeingAdded =
        CreateNativeAppMenuItem(aMenu, u"menu_FileQuitItem"_ns, @selector(menuItemHit:),
                                eCommand_ID_Quit, nsMenuBarX::sNativeEventTarget);
    if (itemBeingAdded) {
      [sApplicationMenu addItem:itemBeingAdded];
      [itemBeingAdded release];
      itemBeingAdded = nil;
    } else {
      // the current application does not have a DOM node for "Quit". Add one
      // anyway, in English.
      NSMenuItem* defaultQuitItem = [[[NSMenuItem alloc] initWithTitle:@"Quit"
                                                                action:@selector(menuItemHit:)
                                                         keyEquivalent:@"q"] autorelease];
      defaultQuitItem.target = nsMenuBarX::sNativeEventTarget;
      defaultQuitItem.tag = eCommand_ID_Quit;
      [sApplicationMenu addItem:defaultQuitItem];
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

//
// Objective-C class used to allow us to have keyboard commands
// look like they are doing something but actually do nothing.
// We allow mouse actions to work normally.
//

// Controls whether or not native menu items should invoke their commands.
static BOOL gMenuItemsExecuteCommands = YES;

@implementation GeckoNSMenu

// Keyboard commands should not cause menu items to invoke their
// commands when there is a key window because we'd rather send
// the keyboard command to the window. We still have the menus
// go through the mechanics so they'll give the proper visual
// feedback.
- (BOOL)performKeyEquivalent:(NSEvent*)aEvent {
  // We've noticed that Mac OS X expects this check in subclasses before
  // calling NSMenu's "performKeyEquivalent:".
  //
  // There is no case in which we'd need to do anything or return YES
  // when we have no items so we can just do this check first.
  if (self.numberOfItems <= 0) {
    return NO;
  }

  NSWindow* keyWindow = NSApp.keyWindow;

  // If there is no key window then just behave normally. This
  // probably means that this menu is associated with Gecko's
  // hidden window.
  if (!keyWindow) {
    return [super performKeyEquivalent:aEvent];
  }

  NSResponder* firstResponder = keyWindow.firstResponder;

  gMenuItemsExecuteCommands = NO;
  [super performKeyEquivalent:aEvent];
  gMenuItemsExecuteCommands = YES;  // return to default

  // Return YES if we invoked a command and there is now no key window or we changed
  // the first responder. In this case we do not want to propagate the event because
  // we don't want it handled again.
  if (!NSApp.keyWindow || NSApp.keyWindow.firstResponder != firstResponder) {
    return YES;
  }

  // Return NO so that we can handle the event via NSView's "keyDown:".
  return NO;
}

- (BOOL)performSuperKeyEquivalent:(NSEvent*)aEvent {
  return [super performKeyEquivalent:aEvent];
}

@end

//
// Objective-C class used as action target for menu items
//

@implementation NativeMenuItemTarget

// called when some menu item in this menu gets hit
- (IBAction)menuItemHit:(id)aSender {
  if (!gMenuItemsExecuteCommands) {
    return;
  }

  if (![aSender isKindOfClass:[NSMenuItem class]]) {
    return;
  }

  NSMenuItem* nativeMenuItem = (NSMenuItem*)aSender;
  NSInteger tag = nativeMenuItem.tag;

  nsMenuGroupOwnerX* menuGroupOwner = nullptr;
  nsMenuBarX* menuBar = nullptr;
  MOZMenuItemRepresentedObject* representedObject = nativeMenuItem.representedObject;

  if (representedObject) {
    menuGroupOwner = representedObject.menuGroupOwner;
    if (!menuGroupOwner) {
      return;
    }
    menuBar = menuGroupOwner->GetMenuBar();
  }

  // Notify containing menu about the fact that a menu item will be activated.
  NSMenu* menu = nativeMenuItem.menu;
  if ([menu.delegate isKindOfClass:[MenuDelegate class]]) {
    [(MenuDelegate*)menu.delegate menu:menu willActivateItem:nativeMenuItem];
  }

  // Get the modifier flags and button for this menu item activation. The menu system does not pass
  // an NSEvent to our action selector, but we can query the current NSEvent instead. The current
  // NSEvent can be a key event or a mouseup event, depending on how the menu item is activated.
  NSEventModifierFlags modifierFlags = NSApp.currentEvent ? NSApp.currentEvent.modifierFlags : 0;
  mozilla::MouseButton button = NSApp.currentEvent
                                    ? nsCocoaUtils::ButtonForEvent(NSApp.currentEvent)
                                    : mozilla::MouseButton::ePrimary;

  // Do special processing if this is for an app-global command.
  if (tag == eCommand_ID_About) {
    nsIContent* mostSpecificContent = sAboutItemContent;
    if (menuBar && menuBar->mAboutItemContent) {
      mostSpecificContent = menuBar->mAboutItemContent;
    }
    nsMenuUtilsX::DispatchCommandTo(mostSpecificContent, modifierFlags, button);
    return;
  }
  if (tag == eCommand_ID_Prefs) {
    nsIContent* mostSpecificContent = sPrefItemContent;
    if (menuBar && menuBar->mPrefItemContent) {
      mostSpecificContent = menuBar->mPrefItemContent;
    }
    nsMenuUtilsX::DispatchCommandTo(mostSpecificContent, modifierFlags, button);
    return;
  }
  if (tag == eCommand_ID_Account) {
    nsIContent* mostSpecificContent = sAccountItemContent;
    if (menuBar && menuBar->mAccountItemContent) {
      mostSpecificContent = menuBar->mAccountItemContent;
    }
    nsMenuUtilsX::DispatchCommandTo(mostSpecificContent, modifierFlags, button);
    return;
  }
  if (tag == eCommand_ID_HideApp) {
    [NSApp hide:aSender];
    return;
  }
  if (tag == eCommand_ID_HideOthers) {
    [NSApp hideOtherApplications:aSender];
    return;
  }
  if (tag == eCommand_ID_ShowAll) {
    [NSApp unhideAllApplications:aSender];
    return;
  }
  if (tag == eCommand_ID_TouchBar) {
    [NSApp toggleTouchBarCustomizationPalette:aSender];
    return;
  }
  if (tag == eCommand_ID_Quit) {
    nsIContent* mostSpecificContent = sQuitItemContent;
    if (menuBar && menuBar->mQuitItemContent) {
      mostSpecificContent = menuBar->mQuitItemContent;
    }
    // If we have some content for quit we execute it. Otherwise we send a native app terminate
    // message. If you want to stop a quit from happening, provide quit content and return
    // the event as unhandled.
    if (mostSpecificContent) {
      nsMenuUtilsX::DispatchCommandTo(mostSpecificContent, modifierFlags, button);
    } else {
      nsCOMPtr<nsIAppStartup> appStartup = mozilla::components::AppStartup::Service();
      if (appStartup) {
        bool userAllowedQuit = true;
        appStartup->Quit(nsIAppStartup::eAttemptQuit, 0, &userAllowedQuit);
      }
    }
    return;
  }

  // given the commandID, look it up in our hashtable and dispatch to
  // that menu item.
  if (menuGroupOwner) {
    nsMenuItemX* menuItem = menuGroupOwner->GetMenuItemForCommandID(static_cast<uint32_t>(tag));
    if (menuItem) {
      menuItem->DoCommand(modifierFlags, button);
    }
  }
}

@end

// Objective-C class used for menu items on the Services menu to allow Gecko
// to override their standard behavior in order to stop key equivalents from
// firing in certain instances. When gMenuItemsExecuteCommands is NO, we return
// a dummy target and action instead of the actual target and action.

@implementation GeckoServicesNSMenuItem

- (id)target {
  id realTarget = super.target;
  if (gMenuItemsExecuteCommands) {
    return realTarget;
  }
  return realTarget ? self : nil;
}

- (SEL)action {
  SEL realAction = super.action;
  if (gMenuItemsExecuteCommands) {
    return realAction;
  }
  return realAction ? @selector(_doNothing:) : nullptr;
}

- (void)_doNothing:(id)aSender {
}

@end

// Objective-C class used as the Services menu so that Gecko can override the
// standard behavior of the Services menu in order to stop key equivalents
// from firing in certain instances.

@implementation GeckoServicesNSMenu

- (void)addItem:(NSMenuItem*)aNewItem {
  [self _overrideClassOfMenuItem:aNewItem];
  [super addItem:aNewItem];
}

- (NSMenuItem*)addItemWithTitle:(NSString*)aString
                         action:(SEL)aSelector
                  keyEquivalent:(NSString*)aKeyEquiv {
  NSMenuItem* newItem = [super addItemWithTitle:aString action:aSelector keyEquivalent:aKeyEquiv];
  [self _overrideClassOfMenuItem:newItem];
  return newItem;
}

- (void)insertItem:(NSMenuItem*)aNewItem atIndex:(NSInteger)aIndex {
  [self _overrideClassOfMenuItem:aNewItem];
  [super insertItem:aNewItem atIndex:aIndex];
}

- (NSMenuItem*)insertItemWithTitle:(NSString*)aString
                            action:(SEL)aSelector
                     keyEquivalent:(NSString*)aKeyEquiv
                           atIndex:(NSInteger)aIndex {
  NSMenuItem* newItem = [super insertItemWithTitle:aString
                                            action:aSelector
                                     keyEquivalent:aKeyEquiv
                                           atIndex:aIndex];
  [self _overrideClassOfMenuItem:newItem];
  return newItem;
}

- (void)_overrideClassOfMenuItem:(NSMenuItem*)aMenuItem {
  if ([aMenuItem class] == [NSMenuItem class]) {
    object_setClass(aMenuItem, [GeckoServicesNSMenuItem class]);
  }
}

@end
