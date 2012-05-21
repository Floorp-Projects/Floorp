/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "nsIWebBrowserChrome3.h"
#include "nsIWindowProvider.h"

class nsXULWindow;
class nsSiteWindow2;

class nsContentTreeOwner : public nsIDocShellTreeOwner,
                           public nsIBaseWindow,
                           public nsIInterfaceRequestor,
                           public nsIWebBrowserChrome3,
                           public nsIWindowProvider
{
friend class nsXULWindow;
friend class nsSiteWindow2;

public:
   NS_DECL_ISUPPORTS

   NS_DECL_NSIBASEWINDOW
   NS_DECL_NSIDOCSHELLTREEOWNER
   NS_DECL_NSIINTERFACEREQUESTOR
   NS_DECL_NSIWEBBROWSERCHROME
   NS_DECL_NSIWEBBROWSERCHROME2
   NS_DECL_NSIWEBBROWSERCHROME3
   NS_DECL_NSIWINDOWPROVIDER

protected:
   nsContentTreeOwner(bool fPrimary);
   virtual ~nsContentTreeOwner();

   void XULWindow(nsXULWindow* aXULWindow);
   nsXULWindow* XULWindow();

protected:
   nsXULWindow      *mXULWindow;
   nsSiteWindow2    *mSiteWindow2;
   bool              mPrimary;
   bool              mContentTitleSetting;
   nsString          mWindowTitleModifier;
   nsString          mTitleSeparator;
   nsString          mTitlePreface;
   nsString          mTitleDefault;
};

#endif /* nsContentTreeOwner_h__ */
