/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Townsend <dtownsend@oxymoronical.com>
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


#include "nsXPInstallManager.h"
#include "nsInstallTrigger.h"
#include "nsIDOMInstallTriggerGlobal.h"

#include "nscore.h"
#include "nsAutoPtr.h"
#include "netCore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsPIDOMWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"

#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIPermissionManager.h"
#include "nsIDocShell.h"
#include "nsNetUtil.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIPrincipal.h"
#include "nsIObserverService.h"
#include "nsIPropertyBag2.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#include "nsIContentHandler.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsXPIInstallInfo.h"


nsInstallTrigger::nsInstallTrigger()
{
    mScriptObject   = nsnull;
}

nsInstallTrigger::~nsInstallTrigger()
{
}


NS_IMPL_THREADSAFE_ISUPPORTS3 (nsInstallTrigger,
                              nsIScriptObjectOwner,
                              nsIDOMInstallTriggerGlobal,
                              nsIContentHandler)


NS_IMETHODIMP
nsInstallTrigger::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
    NS_PRECONDITION(nsnull != aScriptObject, "null arg");
    nsresult res = NS_OK;

    if (nsnull == mScriptObject)
    {
        res = NS_NewScriptInstallTriggerGlobal(aContext,
                                               (nsIDOMInstallTriggerGlobal*)this,
                                               aContext->GetGlobalObject(),
                                               &mScriptObject);
    }

    *aScriptObject = mScriptObject;
    return res;
}

NS_IMETHODIMP
nsInstallTrigger::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}




NS_IMETHODIMP
nsInstallTrigger::HandleContent(const char * aContentType,
                                nsIInterfaceRequestor* aWindowContext,
                                nsIRequest* aRequest)
{
    nsresult rv = NS_OK;
    if (!aRequest)
        return NS_ERROR_NULL_POINTER;

    if (nsCRT::strcasecmp(aContentType, "application/x-xpinstall") != 0)
    {
        // We only support content-type application/x-xpinstall
        return NS_ERROR_WONT_HANDLE_CONTENT;
    }

    // Save the URI so nsXPInstallManager can re-load it later
    nsCOMPtr<nsIURI> uri;
    nsCAutoString    urispec;
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
    if (channel)
    {
        rv = channel->GetURI(getter_AddRefs(uri));
        if (NS_SUCCEEDED(rv) && uri)
            rv = uri->GetSpec(urispec);
    }
    if (NS_FAILED(rv))
        return rv;
    if (urispec.IsEmpty())
        return NS_ERROR_ILLEGAL_VALUE;


    // Save the referrer if any, for permission checks
    NS_NAMED_LITERAL_STRING(referrerProperty, "docshell.internalReferrer");
    bool useReferrer = false;
    nsCOMPtr<nsIURI> referringURI;
    nsCOMPtr<nsIPropertyBag2> channelprops(do_QueryInterface(channel));

    if (channelprops)
    {
        // Get the referrer from the channel properties if we can (not all
        // channels support our internal-referrer property).
        //
        // It's possible docshell explicitly set a null referrer in the case
        // of typed, pasted, or bookmarked URLs and the like. In such a case
        // we get a success return value with null pointer.
        //
        // A null referrer is automatically whitelisted as an explicit user
        // action (though they'll still get the confirmation dialog). For a
        // missing referrer we go to our fall-back plan of using the XPI
        // location for whitelisting purposes.
        rv = channelprops->GetPropertyAsInterface(referrerProperty,
                                                  NS_GET_IID(nsIURI),
                                                  getter_AddRefs(referringURI));
        if (NS_SUCCEEDED(rv))
            useReferrer = PR_TRUE;
    }

    // Cancel the current request. nsXPInstallManager restarts the download
    // under its control (shared codepath with InstallTrigger)
    aRequest->Cancel(NS_BINDING_ABORTED);


    // Get the global object of the target window for StartSoftwareUpdate
    nsCOMPtr<nsIScriptGlobalObjectOwner> globalObjectOwner =
                                         do_QueryInterface(aWindowContext);
    nsIScriptGlobalObject* globalObject =
      globalObjectOwner ? globalObjectOwner->GetScriptGlobalObject() : nsnull;
    if ( !globalObject )
        return NS_ERROR_INVALID_ARG;


    nsCOMPtr<nsIURI> checkuri;

    if ( useReferrer )
    {
        // easiest and most common case: base decision on the page that
        // contained the link
        //
        // NOTE: the XPI itself may be from elsewhere; the user can decide if
        // they trust the actual source when they get the install confirmation
        // dialog. The decision we're making here is whether the triggering
        // site is one which is allowed to annoy the user with modal dialogs.

        checkuri = referringURI;
    }
    else
    {
        // Now we're stumbing in the dark. In the most likely case the user
        // simply clicked on an FTP link (no referrer) and it's perfectly
        // sane to use the current window.
        //
        // On the other hand the user might be opening a non-http XPI link
        // in an unrelated existing window (typed in location bar, bookmark,
        // dragged link ...) in which case the current window is irrelevant.
        // If we knew it was one of these explicit user actions we'd like to
        // allow it, but we have no way of knowing that here.
        //
        // But there's no way to distinguish the innocent cases from a clever
        // malicious site. If we used the target window then evil.com could
        // embed a presumed allowed site (e.g. mozilla.org) in a frame, then
        // change the location to the XPI and trigger the install. Or evil.com
        // could do the same thing in a new window (more work to get around
        // popup blocking, but possible).
        //
        // Our choices appear to be block this type of load entirely or to
        // trust only the install URI. The former is unacceptably restrictive,
        // the latter allows malicious sites to pester people with modal
        // dialogs. As long as the trusted sites don't host bad content that's
        // no worse than an endless stream of alert()s -- already possible.
        // If the trusted sites don't even have an ftp server then even this
        // level of annoyance is not possible.
        //
        // If a trusted site hosts an install with an exploitable flaw it
        // might be possible that a malicious site would attempt to trick
        // people into installing it, hoping to turn around and exploit it.
        // This is not entirely far-fetched (it's been done with ActiveX
        // controls) and will require community policing of the default
        // trusted sites.

        checkuri = uri;
    }

    nsAutoPtr<nsXPITriggerInfo> trigger(new nsXPITriggerInfo());
    nsAutoPtr<nsXPITriggerItem> item(new nsXPITriggerItem(0, NS_ConvertUTF8toUTF16(urispec).get(),
                                                          nsnull));
    if (trigger && item)
    {
        // trigger will own the item now
        trigger->Add(item.forget());
        nsCOMPtr<nsIDOMWindow> win = do_QueryInterface(globalObject);
        nsCOMPtr<nsIXPIInstallInfo> installInfo =
                              new nsXPIInstallInfo(win, checkuri, trigger, 0);
        if (installInfo)
        {
            // From here trigger is owned by installInfo until passed on to nsXPInstallManager
            trigger.forget();
            if (AllowInstall(checkuri))
            {
                return StartInstall(installInfo, nsnull);
            }
            else
            {
                nsCOMPtr<nsIObserverService> os =
                  mozilla::services::GetObserverService();
                if (os)
                    os->NotifyObservers(installInfo,
                                        "xpinstall-install-blocked",
                                        nsnull);
                return NS_ERROR_ABORT;
            }
        }
    }
    return NS_ERROR_OUT_OF_MEMORY;
}


// updateWhitelist
//
// Helper function called by nsInstallTrigger::AllowInstall().
// Interprets the pref as a comma-delimited list of hosts and adds each one
// to the permission manager using the given action. Clear pref when done.
static void updatePermissions( const char* aPref,
                               PRUint32 aPermission,
                               nsIPermissionManager* aPermissionManager,
                               nsIPrefBranch*        aPrefBranch)
{
    NS_PRECONDITION(aPref && aPermissionManager && aPrefBranch, "Null arguments!");

    nsXPIDLCString hostlist;
    nsresult rv = aPrefBranch->GetCharPref( aPref, getter_Copies(hostlist));
    if (NS_SUCCEEDED(rv) && !hostlist.IsEmpty())
    {
        nsCAutoString host;
        PRInt32 start=0, match=0;
        nsresult rv;
        nsCOMPtr<nsIURI> uri;

        do {
            match = hostlist.FindChar(',', start);

            host = Substring(hostlist, start, match-start);
            host.CompressWhitespace();
            host.Insert("http://", 0);

            rv = NS_NewURI(getter_AddRefs(uri), host);
            if (NS_SUCCEEDED(rv))
            {
                aPermissionManager->Add( uri, XPI_PERMISSION, aPermission, 
                                         nsIPermissionManager::EXPIRE_NEVER, 0 );
            }
            start = match+1;
        } while ( match > 0 );

        // save empty list, we don't need to do this again
        aPrefBranch->SetCharPref( aPref, "");
    }
}


// Check whether an Install is allowed. The launching URI can be null,
// in which case only the global pref-setting matters.
bool
nsInstallTrigger::AllowInstall(nsIURI* aLaunchURI)
{
    // Check the global setting.
    bool xpiEnabled = false;
    nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
    if ( !prefBranch)
    {
        return PR_TRUE; // no pref service in native install, it's OK
    }

    prefBranch->GetBoolPref( XPINSTALL_ENABLE_PREF, &xpiEnabled);
    if ( !xpiEnabled )
    {
        // globally turned off
        return PR_FALSE;
    }


    // Check permissions for the launching host if we have one
    nsCOMPtr<nsIPermissionManager> permissionMgr =
                            do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);

    if ( permissionMgr && aLaunchURI )
    {
        bool isChrome = false;
        bool isFile = false;
        aLaunchURI->SchemeIs( "chrome", &isChrome );
        aLaunchURI->SchemeIs( "file", &isFile );

        // file: and chrome: don't need whitelisted hosts
        if ( !isChrome && !isFile )
        {
            // check prefs for permission updates before testing URI
            updatePermissions( XPINSTALL_WHITELIST_ADD,
                               nsIPermissionManager::ALLOW_ACTION,
                               permissionMgr, prefBranch );
            updatePermissions( XPINSTALL_WHITELIST_ADD_36,
                               nsIPermissionManager::ALLOW_ACTION,
                               permissionMgr, prefBranch );
            updatePermissions( XPINSTALL_BLACKLIST_ADD,
                               nsIPermissionManager::DENY_ACTION,
                               permissionMgr, prefBranch );

            bool requireWhitelist = true;
            prefBranch->GetBoolPref( XPINSTALL_WHITELIST_REQUIRED, &requireWhitelist );

            PRUint32 permission = nsIPermissionManager::UNKNOWN_ACTION;
            permissionMgr->TestPermission( aLaunchURI, XPI_PERMISSION, &permission );

            if ( permission == nsIPermissionManager::DENY_ACTION )
            {
                xpiEnabled = PR_FALSE;
            }
            else if ( requireWhitelist &&
                      permission != nsIPermissionManager::ALLOW_ACTION )
            {
                xpiEnabled = PR_FALSE;
            }
        }
    }

    return xpiEnabled;
}


NS_IMETHODIMP
nsInstallTrigger::GetOriginatingURI(nsIScriptGlobalObject* aGlobalObject, nsIURI * *aUri)
{
    NS_ENSURE_ARG_POINTER(aGlobalObject);

    *aUri = nsnull;

    // find the current site
    nsCOMPtr<nsIDOMDocument> domdoc;
    nsCOMPtr<nsIDOMWindow> window(do_QueryInterface(aGlobalObject));
    if ( window )
    {
        window->GetDocument(getter_AddRefs(domdoc));
        nsCOMPtr<nsIDocument> doc(do_QueryInterface(domdoc));
        if ( doc )
            NS_ADDREF(*aUri = doc->GetDocumentURI());
    }
    return NS_OK;
}

NS_IMETHODIMP
nsInstallTrigger::UpdateEnabled(nsIScriptGlobalObject* aGlobalObject, bool aUseWhitelist, bool* aReturn)
{
    nsCOMPtr<nsIURI> uri;
    nsresult rv = GetOriginatingURI(aGlobalObject, getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);
    return UpdateEnabled(uri, aUseWhitelist, aReturn);
}

NS_IMETHODIMP
nsInstallTrigger::UpdateEnabled(nsIURI* aURI, bool aUseWhitelist, bool* aReturn)
{
    // disallow unless we successfully find otherwise
    *aReturn = PR_FALSE;

    if (!aUseWhitelist)
    {
        // simple global pref check
        nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
        if (prefBranch)
            prefBranch->GetBoolPref( XPINSTALL_ENABLE_PREF, aReturn);
    }
    else if (aURI)
    {
        *aReturn = AllowInstall(aURI);
    }

    return NS_OK;
}


NS_IMETHODIMP
nsInstallTrigger::StartInstall(nsIXPIInstallInfo* aInstallInfo, bool* aReturn)
{
    if (aReturn)
        *aReturn = PR_FALSE;

    nsXPInstallManager *mgr = new nsXPInstallManager();
    if (mgr)
    {
        nsresult rv = mgr->InitManagerWithInstallInfo(aInstallInfo);
        if (NS_SUCCEEDED(rv) && aReturn)
            *aReturn = PR_TRUE;
        return rv;
    }
    else
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }
}
