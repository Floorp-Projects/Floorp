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

/* used by XPInstall for actions that must be performed on the UI thread */

#include "nsXPIProxy.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIPluginManager.h"
#include "nsIServiceManager.h"
#include "nsIObserverService.h"
#include "nsIPromptService.h"


nsXPIProxy::nsXPIProxy()
{
}

nsXPIProxy::~nsXPIProxy()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsXPIProxy, nsPIXPIProxy);

NS_IMETHODIMP
nsXPIProxy::RefreshPlugins(PRBool aReloadPages)
{
    NS_DEFINE_CID(pluginManagerCID,NS_PLUGINMANAGER_CID);

    nsCOMPtr<nsIPluginManager> plugins(do_GetService(pluginManagerCID));

    if (!plugins)
        return NS_ERROR_FAILURE;

    return plugins->ReloadPlugins(aReloadPages);
}

NS_IMETHODIMP
nsXPIProxy::NotifyRestartNeeded()
{
    nsCOMPtr<nsIObserverService> obs(do_GetService("@mozilla.org/observer-service;1"));
    if (obs)
        obs->NotifyObservers( nsnull, "xpinstall-restart", nsnull );

    return NS_OK;
}

NS_IMETHODIMP
nsXPIProxy::Alert(const PRUnichar* aTitle, const PRUnichar* aText)
{
    nsCOMPtr<nsIPromptService> dialog(do_GetService("@mozilla.org/embedcomp/prompt-service;1"));
    
    if (!dialog)
        return NS_ERROR_FAILURE;

    return dialog->Alert( nsnull, aTitle, aText );
}

NS_IMETHODIMP
nsXPIProxy::Confirm(const PRUnichar* aTitle, const PRUnichar* aText, PRBool *aReturn)
{
    nsCOMPtr<nsIPromptService> dialog(do_GetService("@mozilla.org/embedcomp/prompt-service;1"));

    if (!dialog)
        return NS_ERROR_FAILURE;

    return dialog->Confirm( nsnull, aTitle, aText, aReturn );
}

