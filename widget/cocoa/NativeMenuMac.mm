/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#include "NativeMenuMac.h"

#include "mozilla/Assertions.h"
#include "mozilla/dom/Element.h"
#include "nsISupports.h"
#include "nsGkAtoms.h"
#include "nsGkAtoms.h"
#include "nsMenuGroupOwnerX.h"
#include "nsMenuItemX.h"
#include "nsMenuUtilsX.h"
#include "nsObjCExceptions.h"

namespace mozilla {

using dom::Element;

namespace widget {

NativeMenuMac::NativeMenuMac(mozilla::dom::Element* aElement) : mContainerStatusBarItem(nil) {
  MOZ_RELEASE_ASSERT(aElement->IsAnyOfXULElements(nsGkAtoms::menu, nsGkAtoms::menupopup));
  mMenuGroupOwner = new nsMenuGroupOwnerX(aElement, nullptr);
  mMenu = MakeRefPtr<nsMenuX>(nullptr, mMenuGroupOwner, aElement);
  mMenu->SetIconListener(this);
  mMenu->SetupIcon();
}

NativeMenuMac::~NativeMenuMac() {
  mMenu->DetachFromGroupOwnerRecursive();
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

}  // namespace widget
}  // namespace mozilla
