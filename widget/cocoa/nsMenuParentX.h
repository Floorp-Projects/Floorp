/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMenuParentX_h_
#define nsMenuParentX_h_

#include "mozilla/RefPtr.h"
#include "mozilla/Variant.h"

class nsMenuX;
class nsMenuBarX;
class nsMenuItemX;

// A base class for objects that can be the parent of an nsMenuX or nsMenuItemX.
class nsMenuParentX {
 public:
  using MenuChild = mozilla::Variant<RefPtr<nsMenuX>, RefPtr<nsMenuItemX>>;

  // XXXmstange double-check that this is still needed
  virtual nsMenuBarX* AsMenuBar() { return nullptr; }

  // If aChild is one of our child menus, insert aChild's native menu item in
  // our native menu at the right location.
  virtual void InsertChildNativeMenuItem(const MenuChild& aChild) = 0;

  // Remove aChild's native menu item froum our native menu.
  virtual void RemoveChildNativeMenuItem(const MenuChild& aChild) = 0;
};

#endif  // nsMenuParentX_h_
