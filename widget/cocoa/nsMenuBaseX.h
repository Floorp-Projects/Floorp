/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMenuBaseX_h_
#define nsMenuBaseX_h_

#include "nsCOMPtr.h"
#include "nsIContent.h"

enum nsMenuObjectTypeX {
  eMenuBarObjectType,
  eSubmenuObjectType,
};

// A base class for objects that can be the parent of an nsMenuX or nsMenuItemX.
class nsMenuObjectX {
 public:
  virtual ~nsMenuObjectX() {}
  virtual nsMenuObjectTypeX MenuObjectType() = 0;
};

#endif  // nsMenuBaseX_h_
