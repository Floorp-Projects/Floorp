/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStandaloneNativeMenu_h_
#define nsStandaloneNativeMenu_h_

#include "nsIStandaloneNativeMenu.h"
#include "NativeMenuMac.h"

class nsStandaloneNativeMenu : public nsIStandaloneNativeMenu {
 public:
  nsStandaloneNativeMenu();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTANDALONENATIVEMENU

  RefPtr<mozilla::widget::NativeMenuMac> GetNativeMenu() { return mMenu; }

 protected:
  virtual ~nsStandaloneNativeMenu();

  RefPtr<mozilla::widget::NativeMenuMac> mMenu;
};

#endif
