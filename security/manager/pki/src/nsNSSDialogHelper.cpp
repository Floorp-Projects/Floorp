/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Terry Hayes <thayes@netscape.com>
 *  Javier Delgadillo <javi@netscape.com>
 */

#include "nsNSSDialogHelper.h"
#include "nsIWindowWatcher.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

const char* nsNSSDialogHelper::kDefaultOpenWindowParam = "centerscreen,chrome,modal,titlebar";

nsresult
nsNSSDialogHelper::openDialog(
    nsIDOMWindowInternal *window,
    const char *url,
    nsISupports *params)
{
  nsresult rv;
  nsCOMPtr<nsIWindowWatcher> windowWatcher = 
           do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsIDOMWindowInternal *parent = window;

  nsCOMPtr<nsIDOMWindowInternal> activeParent;
  if (!parent) {
    nsCOMPtr<nsIDOMWindow> active;
    windowWatcher->GetActiveWindow(getter_AddRefs(active));
    if (active) {
      active->QueryInterface(NS_GET_IID(nsIDOMWindowInternal), getter_AddRefs(activeParent));
      parent = activeParent;
    }
  }

  nsCOMPtr<nsIDOMWindow> newWindow;
  rv = windowWatcher->OpenWindow(parent,
                                 url,
                                 "_blank",
                                 nsNSSDialogHelper::kDefaultOpenWindowParam,
                                 params,
                                 getter_AddRefs(newWindow));
  return rv;
}

