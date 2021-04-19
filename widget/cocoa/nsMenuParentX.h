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

  // Called when aChild becomes visible or hidden, so that the parent can insert
  // or remove the child's native menu item from its NSMenu and update its state
  // of visible items.
  virtual void MenuChildChangedVisibility(const MenuChild& aChild,
                                          bool aIsVisible) = 0;
};

#endif  // nsMenuParentX_h_
