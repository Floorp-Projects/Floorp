/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NativeMenuMac_h
#define NativeMenuMac_h

#include "mozilla/widget/NativeMenu.h"

#include "nsMenuItemIconX.h"
#include "nsMenuX.h"

class nsIContent;
class nsMenuGroupOwnerX;

namespace mozilla {

namespace dom {
class Element;
}

namespace widget {

class NativeMenuMac : public NativeMenu, public nsMenuItemIconX::Listener {
 public:
  explicit NativeMenuMac(mozilla::dom::Element* aElement);

  // nsMenuItemIconX::Listener
  void IconUpdated() override;

  NSMenu* NativeNSMenu() { return mMenu ? mMenu->NativeNSMenu() : nil; }
  void MenuWillOpen();

  // Returns whether a menu item was found at the specified path.
  bool ActivateNativeMenuItemAt(const nsAString& aIndexString);

  void ForceUpdateNativeMenuAt(const nsAString& aIndexString);
  void Dump();

  // If this menu is the menu of a system status bar item (NSStatusItem),
  // let the menu know about the status item so that it can propagate
  // any icon changes to the status item.
  void SetContainerStatusBarItem(NSStatusItem* aItem);

 protected:
  virtual ~NativeMenuMac();

  RefPtr<nsIContent> mContent;
  RefPtr<nsMenuGroupOwnerX> mMenuGroupOwner;
  RefPtr<nsMenuX> mMenu;
  NSStatusItem* mContainerStatusBarItem;
};

}  // namespace widget
}  // namespace mozilla

#endif
