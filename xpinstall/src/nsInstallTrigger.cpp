/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsSoftwareUpdate.h"
#include "nsXPInstallManager.h"
#include "nsInstallTrigger.h"
#include "nsInstallVersion.h"
#include "nsIDOMInstallTriggerGlobal.h"

#include "nscore.h"
#include "netCore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
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

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#include "VerReg.h"

#include "nsIContentHandler.h"
#include "nsIChannel.h"
#include "nsIURI.h"



nsInstallTrigger::nsInstallTrigger()
{
    mScriptObject   = nsnull;

    // make sure all the SoftwareUpdate initialization has happened
    nsCOMPtr<nsISoftwareUpdate> svc (do_GetService(NS_IXPINSTALLCOMPONENT_CONTRACTID));
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


#ifdef NS_DEBUG
    // XXX: if only the owner weren't always null this is what I'd want to do

    // Get the owner of the channel to perform permission checks.
    //
    // It's OK if owner is null, this means it was a top level
    // load and we want to allow installs in that case.
    nsCOMPtr<nsISupports>  owner;
    nsCOMPtr<nsIPrincipal> principal;
    nsCOMPtr<nsIURI>       ownerURI;

    channel->GetOwner( getter_AddRefs( owner ) );
    if ( owner )
    {
        principal = do_QueryInterface( owner );
        if ( principal )
        {
            principal->GetURI( getter_AddRefs( ownerURI ) );
        }
    }
#endif

    // Save the referrer if any, for permission checks
    PRBool trustReferrer = PR_FALSE;
    nsCOMPtr<nsIURI> referringURI;
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
    if ( httpChannel )
    {
        httpChannel->GetReferrer(getter_AddRefs(referringURI));

        // see if we should trust the referrer (which can be null):
        //  - we are an httpChannel (we are if we're here)
        //  - user has not turned off the feature
        PRInt32 referrerLevel = 0;
        nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
        if ( prefBranch)
        {
            rv = prefBranch->GetIntPref( (const char*)"network.http.sendRefererHeader",
                                         &referrerLevel );
            trustReferrer = ( NS_SUCCEEDED(rv) && (referrerLevel >= 2) );
        }
    }


    // Cancel the current request. nsXPInstallManager restarts the download
    // under its control (shared codepath with InstallTrigger)
    aRequest->Cancel(NS_BINDING_ABORTED);


    // Get the global object of the target window for StartSoftwareUpdate
    nsIScriptGlobalObject* globalObject = nsnull;
    nsCOMPtr<nsIScriptGlobalObjectOwner> globalObjectOwner = 
                                         do_QueryInterface(aWindowContext);
    if ( globalObjectOwner )
    {
        globalObject = globalObjectOwner->GetScriptGlobalObject();
    }
    if ( !globalObject )
        return NS_ERROR_INVALID_ARG;


    // We have what we need to start an XPInstall, now figure out if we are
    // going to honor this request based on PermissionManager settings
    PRBool enabled = PR_FALSE;

    if ( trustReferrer )
    {
        // easiest and most common case: base decision on http referrer
        //
        // NOTE: the XPI itself may be from elsewhere; the user can decide if
        // they trust the actual source when they get the install confirmation
        // dialog. The decision we're making here is whether the triggering
        // site is one which is allowed to annoy the user with modal dialogs

        enabled = AllowInstall( referringURI );
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

        enabled = AllowInstall( uri );
    }


    if ( enabled )
    {
        rv = StartSoftwareUpdate( globalObject,
                                  NS_ConvertUTF8toUTF16(urispec),
                                  0,
                                  &enabled);
    }
    else
    {
        // TODO: fire event signaling blocked Install attempt
        rv = NS_ERROR_ABORT;
    }

    return rv;
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
                aPermissionManager->Add( uri, XPI_PERMISSION, aPermission );
            }
            start = match+1;
        } while ( match > 0 );

        // save empty list, we don't need to do this again
        aPrefBranch->SetCharPref( aPref, "");
    }
}


// Check whether an Install is allowed. The launching URI can be null,
// in which case only the global pref-setting matters.
PRBool
nsInstallTrigger::AllowInstall(nsIURI* aLaunchURI)
{
    // Check the global setting.
    PRBool xpiEnabled = PR_FALSE;
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
        PRBool isChrome = PR_FALSE;
        PRBool isFile = PR_FALSE;
        aLaunchURI->SchemeIs( "chrome", &isChrome );
        aLaunchURI->SchemeIs( "file", &isFile );

        // file: and chrome: don't need whitelisted hosts
        if ( !isChrome && !isFile )
        {
            // check prefs for permission updates before testing URI
            updatePermissions( XPINSTALL_WHITELIST_ADD,
                               nsIPermissionManager::ALLOW_ACTION,
                               permissionMgr, prefBranch );
            updatePermissions( XPINSTALL_BLACKLIST_ADD,
                               nsIPermissionManager::DENY_ACTION,
                               permissionMgr, prefBranch );

            PRBool requireWhitelist = PR_TRUE;
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
nsInstallTrigger::UpdateEnabled(nsIScriptGlobalObject* aGlobalObject, PRBool aUseWhitelist, PRBool* aReturn)
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
    else
    {
        NS_ENSURE_ARG_POINTER(aGlobalObject);

        // find the current site
        nsCOMPtr<nsIDOMDocument> domdoc;
        nsCOMPtr<nsIDOMWindow> window(do_QueryInterface(aGlobalObject));
        if ( window )
        {
            window->GetDocument(getter_AddRefs(domdoc));
            nsCOMPtr<nsIDocument> doc(do_QueryInterface(domdoc));
            if ( doc )
            {
                *aReturn = AllowInstall( doc->GetDocumentURI() );
            }
        }
    }

    return NS_OK;
}



NS_IMETHODIMP
nsInstallTrigger::Install(nsIScriptGlobalObject* aGlobalObject, nsXPITriggerInfo* aTrigger, PRBool* aReturn)
{
    NS_ASSERTION(aReturn, "Invalid pointer arg");
    *aReturn = PR_FALSE;

    nsresult rv;
    nsXPInstallManager *mgr = new nsXPInstallManager();
    if (mgr)
    {
        // The Install manager will delete itself when done
        rv = mgr->InitManager( aGlobalObject, aTrigger, 0 );
        if (NS_SUCCEEDED(rv))
            *aReturn = PR_TRUE;
    }
    else
    {
        delete aTrigger;
        rv = NS_ERROR_OUT_OF_MEMORY;
    }


    return rv;
}


NS_IMETHODIMP
nsInstallTrigger::InstallChrome(nsIScriptGlobalObject* aGlobalObject, PRUint32 aType, nsXPITriggerItem *aItem, PRBool* aReturn)
{
    NS_ENSURE_ARG_POINTER(aReturn);
    NS_ENSURE_ARG_POINTER(aItem);
    *aReturn = PR_FALSE;


    // The Install manager will delete itself when done, once we've called
    // InitManager. Before then **WE** must delete it
    nsresult rv = NS_ERROR_OUT_OF_MEMORY;
    nsXPInstallManager *mgr = new nsXPInstallManager();
    if (mgr)
    {
        nsXPITriggerInfo* trigger = new nsXPITriggerInfo();
        if ( trigger )
        {
            trigger->Add( aItem );

            // The Install manager will delete itself when done
            rv = mgr->InitManager( aGlobalObject, trigger, aType );
            *aReturn = PR_TRUE;
        }
        else
        {
            rv = NS_ERROR_OUT_OF_MEMORY;
            delete mgr;
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsInstallTrigger::StartSoftwareUpdate(nsIScriptGlobalObject* aGlobalObject, const nsString& aURL, PRInt32 aFlags, PRBool* aReturn)
{
    nsresult rv = NS_ERROR_OUT_OF_MEMORY;
    *aReturn = PR_FALSE;

    // The Install manager will delete itself when done, once we've called
    // InitManager. Before then **WE** must delete it
    nsXPInstallManager *mgr = new nsXPInstallManager();
    if (mgr)
    {
        nsXPITriggerInfo* trigger = new nsXPITriggerInfo();
        if ( trigger )
        {
            nsXPITriggerItem* item = new nsXPITriggerItem(0,aURL.get(),nsnull);
            if (item)
            {
                trigger->Add( item );
                // The Install manager will delete itself when done
                rv = mgr->InitManager(aGlobalObject, trigger, 0 );
                *aReturn = PR_TRUE;
            }
            else
            {
                rv = NS_ERROR_OUT_OF_MEMORY;
                delete trigger;
                delete mgr;
            }
        }
        else
        {
            rv = NS_ERROR_OUT_OF_MEMORY;
            delete mgr;
        }
    }

    return rv;
}


NS_IMETHODIMP
nsInstallTrigger::CompareVersion(const nsString& aRegName, PRInt32 aMajor, PRInt32 aMinor, PRInt32 aRelease, PRInt32 aBuild, PRInt32* aReturn)
{
    nsInstallVersion inVersion;
    inVersion.Init(aMajor, aMinor, aRelease, aBuild);

    return CompareVersion(aRegName, &inVersion, aReturn);
}

NS_IMETHODIMP
nsInstallTrigger::CompareVersion(const nsString& aRegName, const nsString& aVersion, PRInt32* aReturn)
{
    nsInstallVersion inVersion;
    inVersion.Init(aVersion);

    return CompareVersion(aRegName, &inVersion, aReturn);
}

NS_IMETHODIMP
nsInstallTrigger::CompareVersion(const nsString& aRegName, nsIDOMInstallVersion* aVersion, PRInt32* aReturn)
{
    *aReturn = NOT_FOUND;  // assume failure.

    VERSION              cVersion;
    NS_ConvertUCS2toUTF8 regName(aRegName);
    REGERR               status;
    nsInstallVersion     regNameVersion;

    status = VR_GetVersion( NS_CONST_CAST(char *, regName.get()), &cVersion );
    if ( status == REGERR_OK )
    {
        // we found the version
        if ( VR_ValidateComponent( NS_CONST_CAST(char *, regName.get()) ) != REGERR_NOFILE )
        {
            // and the installed file was not missing:  do the compare
            regNameVersion.Init(cVersion.major,
                                cVersion.minor,
                                cVersion.release,
                                cVersion.build);

            regNameVersion.CompareTo( aVersion, aReturn );
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsInstallTrigger::GetVersion(const nsString& component, nsString& version)
{
    VERSION              cVersion;
    NS_ConvertUCS2toUTF8 regName(component);
    REGERR               status;

    status = VR_GetVersion( NS_CONST_CAST(char *, regName.get()), &cVersion );

    version.Truncate();

    /* if we got the version */
    // XXX fix the right way after PR3 or RTM
    // if ( status == REGERR_OK && VR_ValidateComponent( tempCString ) == REGERR_OK)
    if ( status == REGERR_OK )
    {
        nsInstallVersion regNameVersion;

        regNameVersion.Init(cVersion.major,
                            cVersion.minor,
                            cVersion.release,
                            cVersion.build);

        regNameVersion.ToString(version);
    }

    return NS_OK;
}

