/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "nsSoftwareUpdate.h"
#include "nsXPInstallManager.h"
#include "nsInstallTrigger.h"
#include "nsInstallVersion.h"
#include "nsIDOMInstallTriggerGlobal.h"

#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"

#include "nsIPref.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#include "nsIDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsSpecialSystemDirectory.h"

#include "VerReg.h"

#include "nsIContentHandler.h"
#include "nsIChannel.h"
#include "nsIURI.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);

static NS_DEFINE_IID(kIInstallTrigger_IID, NS_IDOMINSTALLTRIGGERGLOBAL_IID);
static NS_DEFINE_IID(kIInstallTrigger_CID, NS_SoftwareUpdateInstallTrigger_CID);

static NS_DEFINE_IID(kPrefsIID, NS_IPREF_IID);
static NS_DEFINE_IID(kPrefsCID,  NS_PREF_CID);

nsInstallTrigger::nsInstallTrigger()
{
    mScriptObject   = nsnull;
    NS_INIT_ISUPPORTS();
}

nsInstallTrigger::~nsInstallTrigger()
{
}


NS_IMPL_THREADSAFE_ISUPPORTS3 (nsInstallTrigger,
                              nsIScriptObjectOwner,
                              nsIDOMInstallTriggerGlobal,
                              nsIContentHandler);


NS_IMETHODIMP 
nsInstallTrigger::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
    NS_PRECONDITION(nsnull != aScriptObject, "null arg");
    nsresult res = NS_OK;
    
    if (nsnull == mScriptObject) 
    {
        nsIScriptGlobalObject *global = nsnull;
        aContext->GetGlobalObject(&global);

        res = NS_NewScriptInstallTriggerGlobal(  aContext, 
                                                (nsISupports *)(nsIDOMInstallTriggerGlobal*)this, 
                                                (nsISupports *)global, 
                                                &mScriptObject);
        NS_IF_RELEASE(global);

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
                                const char * aCommand, 
                                nsISupports* aWindowContext, 
                                nsIRequest* request)
{
    nsresult rv = NS_OK;
    if (!request) return NS_ERROR_NULL_POINTER;

    if (nsCRT::strcasecmp(aContentType, "application/x-xpinstall") == 0) {
        nsCOMPtr<nsIURI> uri;
        nsCOMPtr<nsIChannel> aChannel = do_QueryInterface(request);
        rv = aChannel->GetURI(getter_AddRefs(uri));
        if (NS_FAILED(rv)) return rv;

        if (uri) {    
            char* spec;
            uri->GetSpec(&spec);
            if (!spec)
                return NS_ERROR_NULL_POINTER;

            nsCOMPtr<nsIScriptGlobalObjectOwner> globalObjectOwner = do_QueryInterface(aWindowContext);
            if (globalObjectOwner)
            {
                nsCOMPtr<nsIScriptGlobalObject> globalObject;
                globalObjectOwner->GetScriptGlobalObject(getter_AddRefs(globalObject));
                if (globalObject)
                {
                    PRBool value;
                    rv = StartSoftwareUpdate(globalObject, NS_ConvertASCIItoUCS2(spec), 0, &value);
            
                    nsMemory::Free(spec);

                    if (NS_SUCCEEDED(rv) && value) 
                        return NS_OK;
                }
            }
        }
    }

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP    
nsInstallTrigger::UpdateEnabled(PRBool* aReturn)
{
    nsIPref * prefs;
    
    nsresult rv = nsServiceManager::GetService(kPrefsCID, 
                                               kPrefsIID,
                                               (nsISupports**) &prefs);


    if ( NS_SUCCEEDED(rv) )
    {
        rv = prefs->GetBoolPref( (const char*) XPINSTALL_ENABLE_PREF, aReturn);

        if (NS_FAILED(rv))
        {
            *aReturn = PR_FALSE;
        }

        NS_RELEASE(prefs);
    }
    else
    {
        // no prefs manager: we're in the install wizard and always work
        *aReturn = PR_TRUE;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsInstallTrigger::Install(nsIScriptGlobalObject* aGlobalObject, nsXPITriggerInfo* aTrigger, PRBool* aReturn)
{
    NS_ASSERTION(aReturn, "Invalid pointer arg");
    *aReturn = PR_FALSE;

    PRBool enabled;
    nsresult rv = UpdateEnabled(&enabled);
    if (NS_FAILED(rv) || !enabled) 
    {
        delete aTrigger;
        return NS_OK;
    }

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


    // make sure we're allowing installs
    PRBool enabled;
    nsresult rv = UpdateEnabled(&enabled);
    if (NS_FAILED(rv) || !enabled)
        return NS_OK;


    // The Install manager will delete itself when done, once we've called
    // InitManager. Before then **WE** must delete it
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
    PRBool enabled;
    nsresult rv = NS_ERROR_OUT_OF_MEMORY;
    *aReturn = PR_FALSE;

    UpdateEnabled(&enabled);
    if (!enabled)
        return NS_OK;

    // The Install manager will delete itself when done, once we've called
    // InitManager. Before then **WE** must delete it
    nsXPInstallManager *mgr = new nsXPInstallManager();
    if (mgr)
    {
        nsXPITriggerInfo* trigger = new nsXPITriggerInfo();
        if ( trigger )
        {
            nsXPITriggerItem* item = new nsXPITriggerItem(0,aURL.get());
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
nsInstallTrigger::InitRegistry(void)
{
    nsresult rv;

    NR_StartupRegistry();   /* startup the registry; if already started, this will essentially be a noop */
    nsCOMPtr<nsIProperties> directoryService = 
             do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
    
    if(!directoryService)
        return NS_ERROR_FAILURE;
    
    nsCOMPtr<nsILocalFile> dir;
    directoryService->Get(NS_XPCOM_CURRENT_PROCESS_DIR, NS_GET_IID(nsIFile), getter_AddRefs(dir));
    if (dir)
    {
        char* nativePath;
        dir->GetPath(&nativePath);
        // EVIL version registry does not take a nsIFile.;
        VR_SetRegDirectory( nativePath );
        if (nativePath)
            nsMemory::Free(nativePath);
            
    }
    return NS_OK;
}

NS_IMETHODIMP    
nsInstallTrigger::CompareVersion(const nsString& aRegName, nsIDOMInstallVersion* aVersion, PRInt32* aReturn)
{
    nsresult rv;
    *aReturn = EQUAL;  // assume failure.

    PRBool enabled;

    UpdateEnabled(&enabled);
    if (!enabled)
        return NS_OK;

    VERSION              cVersion;
    NS_ConvertUCS2toUTF8 regName(aRegName);
    REGERR               status;
    nsInstallVersion     regNameVersion;
    
    rv = InitRegistry();
    if(rv != NS_OK)
        return rv;

    status = VR_GetVersion( NS_CONST_CAST(char *, regName.get()), &cVersion );

    /* if we got the version */
    if ( status == REGERR_OK ) 
    {
        if ( VR_ValidateComponent( NS_CONST_CAST(char *, regName.get()) ) == REGERR_NOFILE ) 
        {
            regNameVersion.Init(0,0,0,0);
        }
        else
        {
            regNameVersion.Init(cVersion.major, 
                                cVersion.minor, 
                                cVersion.release, 
                                cVersion.build);
        }
    }
    else
        regNameVersion.Init(0,0,0,0);
        
    regNameVersion.CompareTo( aVersion, aReturn );

    return NS_OK;
}

NS_IMETHODIMP    
nsInstallTrigger::GetVersion(const nsString& component, nsString& version)
{
    nsresult rv;
    PRBool enabled;

    UpdateEnabled(&enabled);
    if (!enabled)
        return NS_OK;

    VERSION              cVersion;
    NS_ConvertUCS2toUTF8 regName(component);
    REGERR               status;
    
    rv = InitRegistry();
    if(rv != NS_OK)
        return rv;

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

