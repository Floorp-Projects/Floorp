/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#include "nsComponentManagerUtils.h"
#include "nsSystemStatusBarCocoa.h"
#include "nsStandaloneNativeMenu.h"
#include "nsObjCExceptions.h"
#include "nsIDOMElement.h"

NS_IMPL_ISUPPORTS(nsSystemStatusBarCocoa, nsISystemStatusBar)

NS_IMETHODIMP
nsSystemStatusBarCocoa::AddItem(nsIDOMElement* aDOMElement)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsRefPtr<nsStandaloneNativeMenu> menu = new nsStandaloneNativeMenu();
  nsresult rv = menu->Init(aDOMElement);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsISupports> keyPtr = aDOMElement;
  mItems.Put(keyPtr, new StatusItem(menu));

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsSystemStatusBarCocoa::RemoveItem(nsIDOMElement* aDOMElement)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  mItems.Remove(aDOMElement);

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsSystemStatusBarCocoa::StatusItem::StatusItem(nsStandaloneNativeMenu* aMenu)
  : mMenu(aMenu)
{
  MOZ_COUNT_CTOR(nsSystemStatusBarCocoa::StatusItem);

  NSMenu* nativeMenu = nil;
  mMenu->GetNativeMenu(reinterpret_cast<void**>(&nativeMenu));

  mStatusItem = [[[NSStatusBar systemStatusBar] statusItemWithLength:NSSquareStatusItemLength] retain];
  [mStatusItem setMenu:nativeMenu];
  [mStatusItem setHighlightMode:YES];

  // We want the status item to get its image from menu item that mMenu was
  // initialized with. Icon loads are asynchronous, so we need to let the menu
  // know about the item so that it can update its icon as soon as it has
  // loaded.
  mMenu->SetContainerStatusBarItem(mStatusItem);
}

nsSystemStatusBarCocoa::StatusItem::~StatusItem()
{
  mMenu->SetContainerStatusBarItem(nil);
  [[NSStatusBar systemStatusBar] removeStatusItem:mStatusItem];
  [mStatusItem release];
  mStatusItem = nil;

  MOZ_COUNT_DTOR(nsSystemStatusBarCocoa::StatusItem);
}
