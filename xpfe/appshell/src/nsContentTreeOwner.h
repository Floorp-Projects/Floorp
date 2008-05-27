/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsIWebBrowserChrome2.h"
#include "nsIWindowProvider.h"

class nsXULWindow;
class nsSiteWindow2;

class nsContentTreeOwner : public nsIDocShellTreeOwner,
                           public nsIBaseWindow,
                           public nsIInterfaceRequestor,
                           public nsIWebBrowserChrome2,
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
   NS_DECL_NSIWINDOWPROVIDER

protected:
   nsContentTreeOwner(PRBool fPrimary);
   virtual ~nsContentTreeOwner();

   void XULWindow(nsXULWindow* aXULWindow);
   nsXULWindow* XULWindow();

protected:
   nsXULWindow      *mXULWindow;
   nsSiteWindow2    *mSiteWindow2;
   PRBool            mPrimary;
   PRBool            mContentTitleSetting;
   nsString          mWindowTitleModifier;
   nsString          mTitleSeparator;
   nsString          mTitlePreface;
   nsString          mTitleDefault;
};

#endif /* nsContentTreeOwner_h__ */
