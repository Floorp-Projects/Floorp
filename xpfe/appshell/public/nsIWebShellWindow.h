/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsIWebShellWindow_h__
#define nsIWebShellWindow_h__

#include "nsISupports.h"

/* Forward declarations.... */
class nsIWebShell;
class nsIWidget;
class nsString;
class nsIDOMWindowInternal; 
class nsIPrompt;

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
  NS_IMETHOD GetDOMWindow(nsIDOMWindowInternal** aDOMWindow) = 0;
  NS_IMETHOD ConvertWebShellToDOMWindow(nsIWebShell* aShell, nsIDOMWindowInternal** aDOMWindow) = 0;

  NS_IMETHOD GetContentShellById(const nsString& anID, nsIWebShell** aResult) = 0;

  NS_IMETHOD LockUntilChromeLoad() = 0;
  NS_IMETHOD GetLockedState(PRBool& aResult) = 0;

  /**
   * Useful only during window initialization, this method knows whether
   * the window should try to load a default page, or just wait for
   * further instructions.
   * @param aYes returns PR_TRUE iff a default page should be loaded.
   * @return always NS_OK
   */
  NS_IMETHOD ShouldLoadDefaultPage(PRBool *aYes) = 0;

  NS_IMETHOD GetPrompter(nsIPrompt* *result) = 0;
};


#endif /* nsIWebShellWindow_h__ */
