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

#ifndef nsIXULWindowCallbacks_h__
#define nsIXULWindowCallbacks_h__

#include "nsISupports.h"
class nsIWebShell;

// Interface ID for nsIXULWindowCallbacks_h__
// {ACE54DD1-CCF5-11d2-81BC-0060083A0BCF}
#define NS_IXULWINDOWCALLBACKS_IID \
 { 0xace54dd1, 0xccf5, 0x11d2, { 0x81, 0xbc, 0x0, 0x60, 0x8, 0x3a, 0xb, 0xcf } }


class nsIXULWindowCallbacks : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IXULWINDOWCALLBACKS_IID; return iid; }

  // this method will be called by the window creation code after
  // the UI has been loaded, before the window is visible, and before
  // the equivalent JavaScript (XUL "onConstruction" attribute) callback.
  NS_IMETHOD ConstructBeforeJavaScript(nsIWebShell *aWebShell) = 0;

  // like ConstructBeforeJavaScript, but called after the onConstruction callback
  NS_IMETHOD ConstructAfterJavaScript(nsIWebShell *aWebShell) = 0;
};


#endif /* nsIXULWindowCallbacks_h__ */
