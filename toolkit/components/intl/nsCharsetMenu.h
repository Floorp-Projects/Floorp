/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCharsetMenu_h___
#define nsCharsetMenu_h___

#define NS_CHARSETMENU_CID \
  {0x42c52b81, 0xa200, 0x11d3, { 0x9d, 0xb, 0x0, 0x50, 0x4, 0x0, 0x7, 0xb2}}

#define NS_CHARSETMENU_PID "charset-menu"

nsresult
NS_NewCharsetMenu(nsISupports* aOuter, const nsIID& aIID,
                  void** aResult);

#endif /* nsCharsetMenu_h___ */
