/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* There are several ways to open a new window in Mozilla: directly, using
   nsAppShellService; indirectly, using the Open method on an extant
   window, the OpenWindow method on the nsWindowWatcher service, and this
   WindowCreator. I think that's all of them, for petessake.  But
   the conditions under which each should be used are definite, if
   not clear, and there is no overlap.

   Basically, you should never use nsAppShellService. That's the basic
   method that all others boil down to, and it creates partially initialized
   windows. Trust the other means of opening windows to use nsAppShellService
   and then finish the new window's initialization. nsAppShellService is
   also strictly a Mozilla-only service; it's unavailable (or should be)
   to embedding apps. So it's not merely inadvisable but also illegal
   for any code that may execute in an embedding context to use that service.

   Unless you're writing window opening code yourself, you want to use
   the Open method on an extant window, or lacking one of those, the
   OpenWindow method on the nsWindowWatcher service. (The former calls
   through to the latter.) Both methods are equally at home in Mozilla
   and embedding contexts. They differentiate between which kind of window
   to open in different ways, depending on whether there is an extant
   window.

   Lacking an extant window, it's this object, the nsWindowCreator,
   that allows new window creation and properly distinguishes between
   Mozilla and embedding contexts. This source file contains the
   Mozilla version, which calls through to nsAppShellService.
*/

#include "nsCOMPtr.h"
#include "nsAppShellCIDs.h"
#include "nsIAppShellService.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIServiceManager.h"
#include "nsIXULWindow.h"
#include "nsIWebBrowserChrome.h"
#include "nsWindowCreator.h"

static NS_DEFINE_CID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);

nsWindowCreator::nsWindowCreator() {
  NS_INIT_REFCNT();
}

nsWindowCreator::~nsWindowCreator() {
}

NS_IMPL_ISUPPORTS1(nsWindowCreator, nsIWindowCreator)

NS_IMETHODIMP
nsWindowCreator::CreateChromeWindow(nsIWebBrowserChrome *aParent,
                                    PRUint32 aChromeFlags,
                                    nsIWebBrowserChrome **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = 0;

  /* There is no valid way to get from an nsIWebBrowserChrome interface
     to the corresponding nsXULWindow, so we can't really service a request
     to make a window with a parent. This shouldn't be a problem, since
     convention suggests this method only be used when there is no parent
     window (otherwise, just call Open() on the parent). However, we
     should say something, just to be sure: */
  NS_ASSERTION(!aParent, "window creator reached with non-null parent");
  if (aParent)
    return NS_ERROR_INVALID_ARG;

  /* And you really shouldn't be making dependent windows without a parent.
     But unparented modal (and therefore dependent) windows happen
     in our codebase, so we allow it after some bellyaching: */
  if (aChromeFlags & nsIWebBrowserChrome::CHROME_DEPENDENT)
    NS_WARNING("dependent window created without a parent");

  nsCOMPtr<nsIAppShellService> appShell(do_GetService(kAppShellServiceCID));
  if (!appShell)
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIXULWindow> newWindow;
  appShell->CreateTopLevelWindow(0, 0, PR_FALSE, PR_FALSE,
    aChromeFlags, nsIAppShellService::SIZE_TO_CONTENT,
    nsIAppShellService::SIZE_TO_CONTENT, getter_AddRefs(newWindow));

  nsCOMPtr<nsIInterfaceRequestor> thing(do_QueryInterface(newWindow));
  if (thing)
    thing->GetInterface(NS_GET_IID(nsIWebBrowserChrome), (void **) _retval);

  return *_retval ? NS_OK : NS_ERROR_FAILURE;
}

