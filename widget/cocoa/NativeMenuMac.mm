/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#include "NativeMenuMac.h"

#include "mozilla/Assertions.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"

#include "MOZMenuOpeningCoordinator.h"
#include "nsISupports.h"
#include "nsGkAtoms.h"
#include "nsMenuGroupOwnerX.h"
#include "nsMenuItemX.h"
#include "nsMenuUtilsX.h"
#include "nsNativeThemeColors.h"
#include "nsObjCExceptions.h"
#include "nsThreadUtils.h"
#include "PresShell.h"
#include "nsCocoaUtils.h"
#include "nsIFrame.h"
#include "nsPresContext.h"
#include "nsDeviceContext.h"
#include "nsCocoaFeatures.h"

namespace mozilla {

using dom::Element;

namespace widget {

NativeMenuMac::NativeMenuMac(dom::Element* aElement)
    : mElement(aElement), mContainerStatusBarItem(nil) {
  MOZ_RELEASE_ASSERT(aElement->IsAnyOfXULElements(nsGkAtoms::menu, nsGkAtoms::menupopup));
  mMenuGroupOwner = new nsMenuGroupOwnerX(aElement, nullptr);
  mMenu = MakeRefPtr<nsMenuX>(nullptr, mMenuGroupOwner, aElement);
  mMenu->SetObserver(this);
  mMenu->SetIconListener(this);
  mMenu->SetupIcon();
}

NativeMenuMac::~NativeMenuMac() {
  mMenu->DetachFromGroupOwnerRecursive();
  mMenu->ClearObserver();
  mMenu->ClearIconListener();
}

static void UpdateMenu(nsMenuX* aMenu) {
  aMenu->MenuOpened();
  aMenu->MenuClosed();

  uint32_t itemCount = aMenu->GetItemCount();
  for (uint32_t i = 0; i < itemCount; i++) {
    nsMenuX::MenuChild menuObject = *aMenu->GetItemAt(i);
    if (menuObject.is<RefPtr<nsMenuX>>()) {
      UpdateMenu(menuObject.as<RefPtr<nsMenuX>>());
    }
  }
}

void NativeMenuMac::MenuWillOpen() {
  // Force an update on the mMenu by faking an open/close on all of
  // its submenus.
  UpdateMenu(mMenu.get());
}

bool NativeMenuMac::ActivateNativeMenuItemAt(const nsAString& aIndexString) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NSMenu* menu = mMenu->NativeNSMenu();

  nsMenuUtilsX::CheckNativeMenuConsistency(menu);

  NSString* locationString =
      [NSString stringWithCharacters:reinterpret_cast<const unichar*>(aIndexString.BeginReading())
                              length:aIndexString.Length()];
  NSMenuItem* item = nsMenuUtilsX::NativeMenuItemWithLocation(menu, locationString, false);

  // We can't perform an action on an item with a submenu, that will raise
  // an obj-c exception.
  if (item && !item.hasSubmenu) {
    NSMenu* parent = item.menu;
    if (parent) {
      // NSLog(@"Performing action for native menu item titled: %@\n",
      //       [[currentSubmenu itemAtIndex:targetIndex] title]);
      mozilla::AutoRestore<bool> autoRestore(
          nsMenuUtilsX::gIsSynchronouslyActivatingNativeMenuItemDuringTest);
      nsMenuUtilsX::gIsSynchronouslyActivatingNativeMenuItemDuringTest = true;
      [parent performActionForItemAtIndex:[parent indexOfItem:item]];
      return true;
    }
  }

  return false;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void NativeMenuMac::ForceUpdateNativeMenuAt(const nsAString& aIndexString) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NSString* locationString =
      [NSString stringWithCharacters:reinterpret_cast<const unichar*>(aIndexString.BeginReading())
                              length:aIndexString.Length()];
  NSArray<NSString*>* indexes = [locationString componentsSeparatedByString:@"|"];
  RefPtr<nsMenuX> currentMenu = mMenu.get();

  // now find the correct submenu
  unsigned int indexCount = indexes.count;
  for (unsigned int i = 1; currentMenu && i < indexCount; i++) {
    int targetIndex = [indexes objectAtIndex:i].intValue;
    int visible = 0;
    uint32_t length = currentMenu->GetItemCount();
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
          break;
        }
      }
    }
  }

  // fake open/close to cause lazy update to happen
  currentMenu->MenuOpened();
  currentMenu->MenuClosed();

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void NativeMenuMac::IconUpdated() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mContainerStatusBarItem) {
    NSImage* menuImage = mMenu->NativeNSMenuItem().image;
    if (menuImage) {
      [menuImage setTemplate:YES];
    }
    mContainerStatusBarItem.image = menuImage;
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void NativeMenuMac::SetContainerStatusBarItem(NSStatusItem* aItem) {
  mContainerStatusBarItem = aItem;
  IconUpdated();
}

void NativeMenuMac::Dump() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  mMenu->Dump(0);
  nsMenuUtilsX::DumpNativeMenu(mMenu->NativeNSMenu());

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void NativeMenuMac::OnMenuWillOpen(dom::Element* aPopupElement) {
  if (aPopupElement == mElement) {
    return;
  }

  // Our caller isn't keeping us alive, so make sure we stay alive throughout this function in case
  // one of the observer notifications destroys us.
  RefPtr<NativeMenuMac> kungFuDeathGrip(this);

  for (NativeMenu::Observer* observer : mObservers.Clone()) {
    observer->OnNativeSubMenuWillOpen(aPopupElement);
  }
}

void NativeMenuMac::OnMenuDidOpen(dom::Element* aPopupElement) {
  // Our caller isn't keeping us alive, so make sure we stay alive throughout this function in case
  // one of the observer notifications destroys us.
  RefPtr<NativeMenuMac> kungFuDeathGrip(this);

  for (NativeMenu::Observer* observer : mObservers.Clone()) {
    if (aPopupElement == mElement) {
      observer->OnNativeMenuOpened();
    } else {
      observer->OnNativeSubMenuDidOpen(aPopupElement);
    }
  }
}

void NativeMenuMac::OnMenuWillActivateItem(dom::Element* aPopupElement,
                                           dom::Element* aMenuItemElement) {
  // Our caller isn't keeping us alive, so make sure we stay alive throughout this function in case
  // one of the observer notifications destroys us.
  RefPtr<NativeMenuMac> kungFuDeathGrip(this);

  for (NativeMenu::Observer* observer : mObservers.Clone()) {
    observer->OnNativeMenuWillActivateItem(aMenuItemElement);
  }
}

void NativeMenuMac::OnMenuClosed(dom::Element* aPopupElement) {
  // Our caller isn't keeping us alive, so make sure we stay alive throughout this function in case
  // one of the observer notifications destroys us.
  RefPtr<NativeMenuMac> kungFuDeathGrip(this);

  for (NativeMenu::Observer* observer : mObservers.Clone()) {
    if (aPopupElement == mElement) {
      observer->OnNativeMenuClosed();
    } else {
      observer->OnNativeSubMenuClosed(aPopupElement);
    }
  }
}

static NSView* NativeViewForContent(nsIContent* aContent) {
  dom::Document* doc = aContent->GetUncomposedDoc();
  if (!doc) {
    return nil;
  }

  PresShell* presShell = doc->GetPresShell();
  if (!presShell) {
    return nil;
  }

  nsIFrame* frame = presShell->GetRootFrame();
  if (!frame) {
    return nil;
  }

  nsIWidget* widget = frame->GetNearestWidget();
  return (NSView*)widget->GetNativeData(NS_NATIVE_WIDGET);
}

static NSAppearance* NativeAppearanceForContent(nsIContent* aContent) {
  nsIFrame* f = aContent->GetPrimaryFrame();
  if (!f) {
    return nil;
  }
  return NSAppearanceForColorScheme(LookAndFeel::ColorSchemeForFrame(f));
}

void NativeMenuMac::ShowAsContextMenu(nsPresContext* aPc, const CSSIntPoint& aPosition) {
  auto cssToDesktopScale =
      aPc->CSSToDevPixelScale() / aPc->DeviceContext()->GetDesktopToDeviceScale();
  const DesktopPoint desktopPoint = aPosition * cssToDesktopScale;

  mMenu->PopupShowingEventWasSentAndApprovedExternally();

  NSMenu* menu = mMenu->NativeNSMenu();
  NSView* view = NativeViewForContent(mMenu->Content());
  NSAppearance* appearance = NativeAppearanceForContent(mMenu->Content());
  NSPoint locationOnScreen = nsCocoaUtils::GeckoPointToCocoaPoint(desktopPoint);

  // Let the MOZMenuOpeningCoordinator do the actual opening, so that this ShowAsContextMenu call
  // does not spawn a nested event loop, which would be surprising to our callers.
  mOpeningHandle = [MOZMenuOpeningCoordinator.sharedInstance asynchronouslyOpenMenu:menu
                                                                   atScreenPosition:locationOnScreen
                                                                            forView:view
                                                                     withAppearance:appearance];
}

bool NativeMenuMac::Close() {
  if (mOpeningHandle) {
    // In case the menu was trying to open, but this Close() call interrupted it, cancel opening.
    [MOZMenuOpeningCoordinator.sharedInstance cancelAsynchronousOpening:mOpeningHandle];
  }
  return mMenu->Close();
}

RefPtr<nsMenuX> NativeMenuMac::GetOpenMenuContainingElement(dom::Element* aElement) {
  nsTArray<RefPtr<dom::Element>> submenuChain;
  RefPtr<dom::Element> currentElement = aElement->GetParentElement();
  while (currentElement && currentElement != mElement) {
    if (currentElement->IsXULElement(nsGkAtoms::menu)) {
      submenuChain.AppendElement(currentElement);
    }
    currentElement = currentElement->GetParentElement();
  }
  if (!currentElement) {
    // aElement was not a descendent of mElement. Refuse to activate the item.
    return nullptr;
  }

  // Traverse submenuChain from shallow to deep, to find the nsMenuX that contains aElement.
  submenuChain.Reverse();
  RefPtr<nsMenuX> menu = mMenu;
  for (const auto& submenu : submenuChain) {
    if (!menu->IsOpenForGecko()) {
      // Refuse to descend into closed menus.
      return nullptr;
    }
    Maybe<nsMenuX::MenuChild> menuChild = menu->GetItemForElement(submenu);
    if (!menuChild || !menuChild->is<RefPtr<nsMenuX>>()) {
      // Couldn't find submenu.
      return nullptr;
    }
    menu = menuChild->as<RefPtr<nsMenuX>>();
  }

  if (!menu->IsOpenForGecko()) {
    // Refuse to descend into closed menus.
    return nullptr;
  }
  return menu;
}

static NSEventModifierFlags ConvertModifierFlags(Modifiers aModifiers) {
  NSEventModifierFlags flags = 0;
  if (aModifiers & MODIFIER_CONTROL) {
    flags |= NSEventModifierFlagControl;
  }
  if (aModifiers & MODIFIER_ALT) {
    flags |= NSEventModifierFlagOption;
  }
  if (aModifiers & MODIFIER_SHIFT) {
    flags |= NSEventModifierFlagShift;
  }
  if (aModifiers & MODIFIER_META) {
    flags |= NSEventModifierFlagCommand;
  }
  return flags;
}

void NativeMenuMac::ActivateItem(dom::Element* aItemElement, Modifiers aModifiers, int16_t aButton,
                                 ErrorResult& aRv) {
  RefPtr<nsMenuX> menu = GetOpenMenuContainingElement(aItemElement);
  if (!menu) {
    aRv.ThrowInvalidStateError("Menu containing menu item is not open");
    return;
  }
  Maybe<nsMenuX::MenuChild> child = menu->GetItemForElement(aItemElement);
  if (!child || !child->is<RefPtr<nsMenuItemX>>()) {
    aRv.ThrowInvalidStateError("Could not find the supplied menu item");
    return;
  }

  RefPtr<nsMenuItemX> item = std::move(child->as<RefPtr<nsMenuItemX>>());
  if (!item->IsVisible()) {
    aRv.ThrowInvalidStateError("Menu item is not visible");
    return;
  }

  NSMenuItem* nativeItem = [item->NativeNSMenuItem() retain];

  // First, initiate the closing of the NSMenu.
  // This synchronously calls the menu delegate's menuDidClose handler. So menuDidClose is
  // what runs first; this matches the order of events for user-initiated menu item activation.
  // This call doesn't immediately hide the menu; the menu only hides once the stack unwinds
  // from NSMenu's nested "tracking" event loop.
  [mMenu->NativeNSMenu() cancelTrackingWithoutAnimation];

  // Next, call OnWillActivateItem. This also matches the order of calls that happen when a user
  // activates a menu item in the real world: -[MenuDelegate menu:willActivateItem:] runs after
  // menuDidClose.
  menu->OnWillActivateItem(nativeItem);

  // Finally, call ActivateItemAfterClosing. This also mimics the order in the real world:
  // menuItemHit is called after menu:willActivateItem:.
  menu->ActivateItemAfterClosing(std::move(item), ConvertModifierFlags(aModifiers), aButton);

  // Tell our native event loop that it should not process any more work before
  // unwinding the stack, so that we can get out of the menu's nested event loop
  // as fast as possible. This was needed to fix spurious failures in tests, where
  // a call to cancelTrackingWithoutAnimation was ignored if more native events were
  // processed before the event loop was exited. As a result, the menu stayed open
  // forever and the test never finished.
  MOZMenuOpeningCoordinator.needToUnwindForMenuClosing = YES;

  [nativeItem release];
}

void NativeMenuMac::OpenSubmenu(dom::Element* aMenuElement) {
  if (RefPtr<nsMenuX> menu = GetOpenMenuContainingElement(aMenuElement)) {
    Maybe<nsMenuX::MenuChild> item = menu->GetItemForElement(aMenuElement);
    if (item && item->is<RefPtr<nsMenuX>>()) {
      item->as<RefPtr<nsMenuX>>()->MenuOpened();
    }
  }
}

void NativeMenuMac::CloseSubmenu(dom::Element* aMenuElement) {
  if (RefPtr<nsMenuX> menu = GetOpenMenuContainingElement(aMenuElement)) {
    Maybe<nsMenuX::MenuChild> item = menu->GetItemForElement(aMenuElement);
    if (item && item->is<RefPtr<nsMenuX>>()) {
      item->as<RefPtr<nsMenuX>>()->MenuClosed();
    }
  }
}

RefPtr<Element> NativeMenuMac::Element() { return mElement; }

}  // namespace widget
}  // namespace mozilla
