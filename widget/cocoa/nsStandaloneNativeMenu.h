/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStandaloneNativeMenu_h_
#define nsStandaloneNativeMenu_h_

#include "nsMenuGroupOwnerX.h"
#include "nsMenuX.h"
#include "nsIStandaloneNativeMenu.h"

class nsStandaloneNativeMenu : public nsMenuGroupOwnerX, public nsIStandaloneNativeMenu
{
public:
  nsStandaloneNativeMenu();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSISTANDALONENATIVEMENU

  // nsMenuObjectX
  nsMenuObjectTypeX MenuObjectType() { return eStandaloneNativeMenuObjectType; }
  void * NativeData() { return mMenu != nullptr ? mMenu->NativeData() : nullptr; }
  virtual void IconUpdated() MOZ_OVERRIDE;

  nsMenuX * GetMenuXObject() { return mMenu; }

  // If this menu is the menu of a system status bar item (NSStatusItem),
  // let the menu know about the status item so that it can propagate
  // any icon changes to the status item.
  void SetContainerStatusBarItem(NSStatusItem* aItem);

protected:
  virtual ~nsStandaloneNativeMenu();

  nsMenuX * mMenu;
  NSStatusItem* mContainerStatusBarItem;
};

#endif
