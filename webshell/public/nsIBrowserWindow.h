/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#ifndef nsIBrowserWindow_h___
#define nsIBrowserWindow_h___

#include "nsweb.h"
#include "nsISupports.h"

class nsIFactory;

#define NS_IBROWSER_WINDOW_IID \
 { 0xa6cf905c, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

#define NS_BROWSER_WINDOW_CID \
 { 0xa6cf905d, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

// Chrome mask
#define NS_CHROME_TOOL_BAR_ON   0x1
#define NS_CHROME_STATUS_BAR_ON 0x2

/**
 * API to a "browser window". A browser window contains a toolbar, a web shell
 * and a status bar. The toolbar and status bar are optional and are
 * controlled by the chrome API.
 */
class nsIBrowserWindow : public nsISupports {
public:
  NS_IMETHOD Init(PRUint32 aChromeMask) = 0;

  NS_IMETHOD Show() = 0;

  NS_IMETHOD Hide() = 0;

  NS_IMETHOD ChangeChrome(PRUint32 aNewChromeMask) = 0;

  NS_IMETHOD GetChrome(PRUint32& aChromeMaskResult) = 0;

  // XXX minimize, maximize
  // XXX event control: enable/disable window close box, stick to glass, modal
};

extern "C" NS_WEB nsresult
NS_NewBrowserWindowFactory(nsIFactory** aFactory);

#endif /* nsIBrowserWindow_h___ */
