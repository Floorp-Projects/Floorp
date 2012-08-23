/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMenuUtilsX_h_
#define nsMenuUtilsX_h_

#include "nscore.h"
#include "nsEvent.h"
#include "nsMenuBaseX.h"

#import <Cocoa/Cocoa.h>

class nsIContent;
class nsString;
class nsMenuBarX;

// Namespace containing utility functions used in our native menu implementation.
namespace nsMenuUtilsX
{
  void          DispatchCommandTo(nsIContent* aTargetContent);
  NSString*     GetTruncatedCocoaLabel(const nsString& itemLabel);
  uint8_t       GeckoModifiersForNodeAttribute(const nsString& modifiersAttribute);
  unsigned int  MacModifiersForGeckoModifiers(uint8_t geckoModifiers);
  nsMenuBarX*   GetHiddenWindowMenuBar(); // returned object is not retained
  NSMenuItem*   GetStandardEditMenuItem(); // returned object is not retained
  bool          NodeIsHiddenOrCollapsed(nsIContent* inContent);
  int           CalculateNativeInsertionPoint(nsMenuObjectX* aParent, nsMenuObjectX* aChild);
}

#endif // nsMenuUtilsX_h_
