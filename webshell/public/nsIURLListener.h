/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef nsIURLListener_h___
#define nsIURLListener_h___

#include "nsISupports.h"
#include "nsIWebShell.h"

class nsIWebShell;

// Interface ID for nsIURLListener
#define NS_IURL_LISTENER_IID \
{0x8b42a211, 0xcdc1, 0x11d2, {0xbd, 0xbe, 0x0, 0x50, 0x4, 0xa, 0x9b, 0x44}}


class nsIURLListener : public nsISupports {
public:

  NS_IMETHOD WillLoadURL(nsIWebShell* aShell,
                         const PRUnichar* aURL,
                         nsLoadType aReason) = 0;

  NS_IMETHOD BeginLoadURL(nsIWebShell* aShell,
                          const PRUnichar* aURL) = 0;

  // XXX not yet implemented; should we?
  NS_IMETHOD ProgressLoadURL(nsIWebShell* aShell,
                             const PRUnichar* aURL,
                             PRInt32 aProgress,
                             PRInt32 aProgressMax) = 0;

  NS_IMETHOD EndLoadURL(nsIWebShell* aShell,
                        const PRUnichar* aURL,
                        PRInt32 aStatus) = 0;

};
// Return value from WillLoadURL
#define NS_WEB_SHELL_CANCEL_URL_LOAD      0xC0E70000


#endif /* nsIURLListener_h___ */
