/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMenuUtilsX_h_
#define nsMenuUtilsX_h_

#include "nscore.h"
#include "nsMenuBaseX.h"
#include "nsStringFwd.h"

#import <Cocoa/Cocoa.h>

class nsIContent;
class nsMenuBarX;

// Namespace containing utility functions used in our native menu implementation.
namespace nsMenuUtilsX {
void DispatchCommandTo(nsIContent* aTargetContent);
NSString* GetTruncatedCocoaLabel(const nsString& itemLabel);
uint8_t GeckoModifiersForNodeAttribute(const nsString& modifiersAttribute);
unsigned int MacModifiersForGeckoModifiers(uint8_t geckoModifiers);
nsMenuBarX* GetHiddenWindowMenuBar();   // returned object is not retained
NSMenuItem* GetStandardEditMenuItem();  // returned object is not retained
bool NodeIsHiddenOrCollapsed(nsIContent* aContent);
int CalculateNativeInsertionPoint(nsMenuObjectX* aParent, nsMenuObjectX* aChild);

// Find the menu item by following the path aLocationString from aRootMenu.
// aLocationString is a '|'-separated list of integers, where each integer is
// the index of the menu item in the menu.
// aIsMenuBar needs to be true if aRootMenu is the app's mainMenu, so that the
// app menu can be skipped during the search.
NSMenuItem* NativeMenuItemWithLocation(NSMenu* aRootMenu, NSString* aLocationString,
                                       bool aIsMenuBar);

}  // namespace nsMenuUtilsX

#endif  // nsMenuUtilsX_h_
