/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsContentTreeOwner_h__
#define nsContentTreeOwner_h__

// Helper Classes
#include "nsCOMPtr.h"
#include "nsString.h"

// Interfaces Needed
#include "nsIBaseWindow.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWebBrowserChrome.h"

class nsXULWindow;
class nsSiteWindow2;

class nsContentTreeOwner : public nsIDocShellTreeOwner,
                                  public nsIBaseWindow,
                                  public nsIInterfaceRequestor,
                                  public nsIWebBrowserChrome
{
friend class nsXULWindow;
friend class nsSiteWindow2;

public:
   NS_DECL_ISUPPORTS

   NS_DECL_NSIBASEWINDOW
   NS_DECL_NSIDOCSHELLTREEOWNER
   NS_DECL_NSIINTERFACEREQUESTOR
   NS_DECL_NSIWEBBROWSERCHROME

protected:
   nsContentTreeOwner(PRBool fPrimary);
   virtual ~nsContentTreeOwner();

   void XULWindow(nsXULWindow* aXULWindow);
   nsXULWindow* XULWindow();

   NS_IMETHOD ApplyChromeFlags();

protected:
   nsXULWindow      *mXULWindow;
   nsSiteWindow2    *mSiteWindow2;
   PRBool            mPrimary;
   PRBool            mContentTitleSetting;
   nsString          mWindowTitleModifier;
   nsString          mTitleSeparator;
   nsString          mTitlePreface;
   nsString          mTitleDefault;

private:
   PRUint32          mChromeFlags; // don't use directly! use GetChromeFlags()
};

#endif /* nsContentTreeOwner_h__ */
