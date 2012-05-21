/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsINativeMenuService_h_
#define nsINativeMenuService_h_

#include "nsISupports.h"

class nsIWidget;
class nsIContent;

// {90DF88F9-F084-4EF3-829A-49496E636DED}
#define NS_INATIVEMENUSERVICE_IID \
{ 0x90DF88F9, 0xF084, 0x4EF3, \
{ 0x82, 0x9A, 0x49, 0x49, 0x6E, 0x63, 0x6D, 0xED} }

class nsINativeMenuService : public nsISupports {
public:
  // Given a top-level window widget and a menu bar DOM node, sets up native
  // menus. Once created, native menus are controlled via the DOM, including
  // destruction.
  NS_IMETHOD CreateNativeMenuBar(nsIWidget* aParent, nsIContent* aMenuBarNode)=0;
};

#endif // nsINativeMenuService_h_
