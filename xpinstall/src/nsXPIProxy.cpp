/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Daniel Veditz <dveditz@netscape.com>
 */

#include "nsXPIProxy.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMNavigator.h"
#include "nsIDOMPluginArray.h"
#include "nsIServiceManager.h"
#include "nsIObserverService.h"
#include "nsObserverService.h"

nsXPIProxy::nsXPIProxy()
{
    NS_INIT_ISUPPORTS();
}

nsXPIProxy::~nsXPIProxy()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsXPIProxy, nsPIXPIProxy);

NS_IMETHODIMP
nsXPIProxy::RefreshPlugins(nsISupports *aWindow)
{
    if (!aWindow)
        return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsIDOMWindowInternal> win(do_QueryInterface(aWindow));
    if (!win)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMNavigator> nav;
    nsresult rv = win->GetNavigator(getter_AddRefs(nav));
    if (NS_FAILED(rv))
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMPluginArray> plugins;
    rv = nav->GetPlugins(getter_AddRefs(plugins));
    if (NS_FAILED(rv))
        return NS_ERROR_FAILURE;

    rv = plugins->Refresh(PR_TRUE);
    return rv;
}

NS_IMETHODIMP
nsXPIProxy::NotifyRestartNeeded()
{
    nsCOMPtr<nsIObserverService> obs(do_GetService(NS_OBSERVERSERVICE_CONTRACTID));
    if (obs)
        obs->NotifyObservers( nsnull, "xpinstall-restart", nsnull );

    return NS_OK;
}
