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

#ifndef nsXULWindow_h__
#define nsXULWindow_h__

// Local Includes
#include "nsChromeTreeOwner.h"
#include "nsContentTreeOwner.h"

// Helper classes
#include "nsCOMPtr.h"
#include "nsVoidArray.h"
#include "nsString.h"

// Interfaces needed
#include "nsIXULWindow.h"
#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsIWidget.h"
#include "nsIDocShellTreeItem.h"

// nsXULWindow

class nsXULWindow : public nsIXULWindow, public nsIBaseWindow
{
friend class nsChromeTreeOwner;
friend class nsContentTreeOwner;

public:
   NS_DECL_ISUPPORTS

   NS_DECL_NSIXULWINDOW
   NS_DECL_NSIBASEWINDOW

protected:
   nsXULWindow();
   virtual ~nsXULWindow();

   NS_IMETHOD EnsureChromeTreeOwner();
   NS_IMETHOD EnsureContentTreeOwner();

   NS_IMETHOD GetDOMElementFromDocShell(nsIDocShell* aDocShell, 
      nsIDOMElement** aDOMElement);
   NS_IMETHOD PersistPositionAndSize(PRBool aPosition, PRBool aSize);
   NS_IMETHOD ContentShellAdded(nsIDocShellTreeItem* aContentShell,
   PRBool aPrimary, const PRUnichar* aID);

protected:
   nsChromeTreeOwner*      mChromeTreeOwner;
   nsContentTreeOwner*     mContentTreeOwner;
   nsCOMPtr<nsIWidget>     mWindow;
   nsCOMPtr<nsIDocShell>   mDocShell;
   nsVoidArray             mContentShells;
};

// nsContentShellInfo

class nsContentShellInfo
{
public:
   nsContentShellInfo(const nsString& aID, PRBool aPrimary, nsIDocShellTreeItem* aContentShell);
   ~nsContentShellInfo();

public:
   nsAutoString                  id;   // The identifier of the content shell
   PRBool                        primary; // Signals the fact that the shell is primary
   nsCOMPtr<nsIDocShellTreeItem> child; // content shell
};

#endif /* nsXULWindow_h__ */
