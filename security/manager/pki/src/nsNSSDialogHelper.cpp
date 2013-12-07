/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSDialogHelper.h"
#include "nsIWindowWatcher.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsCxPusher.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

static const char kOpenDialogParam[] = "centerscreen,chrome,modal,titlebar";
static const char kOpenWindowParam[] = "centerscreen,chrome,titlebar";

nsresult
nsNSSDialogHelper::openDialog(
    nsIDOMWindow *window,
    const char *url,
    nsISupports *params,
    bool modal)
{
  nsresult rv;
  nsCOMPtr<nsIWindowWatcher> windowWatcher = 
           do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDOMWindow> parent = window;

  if (!parent) {
    windowWatcher->GetActiveWindow(getter_AddRefs(parent));
  }

  // We're loading XUL into this window, and it's happening on behalf of the
  // system, not on behalf of content. Make sure the initial about:blank window
  // gets a system principal, otherwise we'll bork when trying to wrap the
  // nsIKeyGenThread |arguments| property into the unprivileged scoope.
  MOZ_ASSERT(!strncmp("chrome://", url, strlen("chrome://")));
  nsCxPusher pusher;
  pusher.PushNull();

  nsCOMPtr<nsIDOMWindow> newWindow;
  rv = windowWatcher->OpenWindow(parent,
                                 url,
                                 "_blank",
                                 modal
                                 ? kOpenDialogParam
                                 : kOpenWindowParam,
                                 params,
                                 getter_AddRefs(newWindow));
  return rv;
}

