/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

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

#include "nsRepository.h"
#include "nsIServiceManager.h"

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
         nsIScriptGlobalObject *global = aContext->GetGlobalObject();

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
                                const char * aWindowTarget, 
                                nsISupports* aWindowContext, 
                                nsIChannel * aChannel)
{
    nsresult rv = NS_OK;
    if (!aChannel) return NS_ERROR_NULL_POINTER;

    if (nsCRT::strcasecmp(aContentType, "application/x-xpinstall") == 0) {
        nsCOMPtr<nsIURI> uri;
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
        *aReturn = PR_FALSE;  /* no prefs manager.  set to false */
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
            nsXPITriggerItem* item = new nsXPITriggerItem(0,aURL.GetUnicode());
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
    *aReturn = EQUAL;  // assume failure.

    PRBool enabled;

    UpdateEnabled(&enabled);
    if (!enabled)
        return NS_OK;

    VERSION              cVersion;
    char*                tempCString;
    REGERR               status;
    nsInstallVersion     regNameVersion;
    
    tempCString = aRegName.ToNewCString();

    status = VR_GetVersion( tempCString, &cVersion );

    /* if we got the version */
    if ( status == REGERR_OK ) 
    {
        if ( VR_ValidateComponent( tempCString ) == REGERR_NOFILE ) 
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

    if (tempCString)
        Recycle(tempCString);
    
    return NS_OK;
}

NS_IMETHODIMP    
nsInstallTrigger::GetVersion(const nsString& component, nsString& version)
{
    PRBool enabled;

    UpdateEnabled(&enabled);
    if (!enabled)
        return NS_OK;

    VERSION              cVersion;
    char*                tempCString;
    REGERR               status;
    
    tempCString = component.ToNewCString();

    status = VR_GetVersion( tempCString, &cVersion );

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
    
    if (tempCString)
         Recycle(tempCString);

    return NS_OK;
}

