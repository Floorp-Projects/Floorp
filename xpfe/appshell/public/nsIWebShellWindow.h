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

#ifndef nsIWebShellWindow_h__
#define nsIWebShellWindow_h__

#include "nsISupports.h"


/* Forward declarations.... */
class nsIWebShell;
class nsIWidget;
class nsString;

// Interface ID for nsIWebShellWindow
#define NS_IWEBSHELL_WINDOW_IID \
 { 0x28dce479, 0xbf73, 0x11d2, { 0x96, 0xc8, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56}}

class nsIWebShellWindow : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IWEBSHELL_WINDOW_IID; return iid; }

  NS_IMETHOD Show(PRBool aShow) = 0;
  NS_IMETHOD ShowModal() = 0;
  NS_IMETHOD Close() = 0;
  NS_IMETHOD GetWebShell(nsIWebShell *& aWebShell) = 0;
  NS_IMETHOD GetWidget(nsIWidget *& aWidget) = 0;

  NS_IMETHOD AddWebShellInfo(const nsString& aID, const nsString& aName,
                             const nsString& aURL, nsIWebShell* aOpenerShell,
                             nsIWebShell* aChildShell) = 0;
};


#endif /* nsIWebShellWindow_h__ */
