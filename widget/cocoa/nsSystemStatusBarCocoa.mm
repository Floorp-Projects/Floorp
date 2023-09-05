/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#include "nsComponentManagerUtils.h"
#include "nsSystemStatusBarCocoa.h"
#include "NativeMenuMac.h"
#include "nsObjCExceptions.h"
#include "mozilla/dom/Element.h"

using mozilla::dom::Element;
using mozilla::widget::NativeMenuMac;

NS_IMPL_ISUPPORTS(nsSystemStatusBarCocoa, nsISystemStatusBar)

NS_IMETHODIMP
nsSystemStatusBarCocoa::AddItem(Element* aElement) {
  if (!aElement->IsAnyOfXULElements(nsGkAtoms::menu, nsGkAtoms::menupopup)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<NativeMenuMac> menu = new NativeMenuMac(aElement);

  nsCOMPtr<nsISupports> keyPtr = aElement;
  mItems.InsertOrUpdate(keyPtr, mozilla::MakeUnique<StatusItem>(menu));

  return NS_OK;
}

NS_IMETHODIMP
nsSystemStatusBarCocoa::RemoveItem(Element* aElement) {
  mItems.Remove(aElement);
  return NS_OK;
}

nsSystemStatusBarCocoa::StatusItem::StatusItem(NativeMenuMac* aMenu)
    : mMenu(aMenu) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  MOZ_COUNT_CTOR(nsSystemStatusBarCocoa::StatusItem);

  mStatusItem = [[NSStatusBar.systemStatusBar
      statusItemWithLength:NSSquareStatusItemLength] retain];
  mStatusItem.menu = mMenu->NativeNSMenu();
  mStatusItem.highlightMode = YES;

  // We want the status item to get its image from menu item that mMenu was
  // initialized with. Icon loads are asynchronous, so we need to let the menu
  // know about the item so that it can update its icon as soon as it has
  // loaded.
  mMenu->SetContainerStatusBarItem(mStatusItem);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsSystemStatusBarCocoa::StatusItem::~StatusItem() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  mMenu->SetContainerStatusBarItem(nil);
  [NSStatusBar.systemStatusBar removeStatusItem:mStatusItem];
  [mStatusItem release];
  mStatusItem = nil;

  MOZ_COUNT_DTOR(nsSystemStatusBarCocoa::StatusItem);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}
