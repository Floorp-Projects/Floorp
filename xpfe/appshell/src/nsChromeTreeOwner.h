/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 */

#ifndef nsChromeTreeOwner_h__
#define nsChromeTreeOwner_h__

// Helper Classes
#include "nsCOMPtr.h"

// Interfaces Needed
#include "nsIBaseWindow.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWebProgressListener.h"
#include "nsWeakReference.h"

class nsXULWindow;

class nsChromeTreeOwner : public nsIDocShellTreeOwner,
                          public nsIBaseWindow, 
                          public nsIInterfaceRequestor,
                          public nsIWebProgressListener,
                          public nsSupportsWeakReference
{
friend class nsXULWindow;

public:
   NS_DECL_ISUPPORTS

   NS_DECL_NSIINTERFACEREQUESTOR
   NS_DECL_NSIBASEWINDOW
   NS_DECL_NSIDOCSHELLTREEOWNER
   NS_DECL_NSIWEBPROGRESSLISTENER

   static nsresult InitGlobals();
   static void     FreeGlobals();

protected:
   nsChromeTreeOwner();
   virtual ~nsChromeTreeOwner();

   void XULWindow(nsXULWindow* aXULWindow);
   nsXULWindow* XULWindow();

protected:
   nsXULWindow*      mXULWindow;
};

#endif /* nsChromeTreeOwner_h__ */
