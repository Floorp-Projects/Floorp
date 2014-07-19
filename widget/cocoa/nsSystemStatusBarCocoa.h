/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSystemStatusBarCocoa_h_
#define nsSystemStatusBarCocoa_h_

#include "nsISystemStatusBar.h"
#include "nsClassHashtable.h"
#include "nsAutoPtr.h"

class nsStandaloneNativeMenu;
@class NSStatusItem;

class nsSystemStatusBarCocoa : public nsISystemStatusBar
{
public:
  nsSystemStatusBarCocoa() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSISYSTEMSTATUSBAR

protected:
  virtual ~nsSystemStatusBarCocoa() {}

  struct StatusItem
  {
    StatusItem(nsStandaloneNativeMenu* aMenu);
    ~StatusItem();

  private:
    nsRefPtr<nsStandaloneNativeMenu> mMenu;
    NSStatusItem* mStatusItem;
  };

  nsClassHashtable<nsISupportsHashKey, StatusItem> mItems;
};

#endif // nsSystemStatusBarCocoa_h_
