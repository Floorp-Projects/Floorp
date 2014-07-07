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

  NS_DECL_ISUPPORTS  
  NS_DECL_NSISTANDALONENATIVEMENU

  // nsMenuObjectX
  nsMenuObjectTypeX MenuObjectType() { return eStandaloneNativeMenuObjectType; }
  void * NativeData() { return mMenu != nullptr ? mMenu->NativeData() : nullptr; }

  nsMenuX * GetMenuXObject() { return mMenu; }

protected:
  virtual ~nsStandaloneNativeMenu();

  nsMenuX * mMenu;
};

#endif
