/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsSoftwareUpdate.h"
#include "nsXPInstallManager.h"
#include "nsInstallTrigger.h"
#include "nsIDOMInstallTriggerGlobal.h"

#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIScriptGlobalObject.h"

#include "nsIPref.h"

#include "nsRepository.h"
#include "nsIServiceManager.h"

#include "nsSpecialSystemDirectory.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);

static NS_DEFINE_IID(kIInstallTrigger_IID, NS_IDOMINSTALLTRIGGERGLOBAL_IID);
static NS_DEFINE_IID(kIInstallTrigger_CID, NS_SoftwareUpdateInstallTrigger_CID);






nsInstallTrigger::nsInstallTrigger()
{
    mScriptObject   = nsnull;
    NS_INIT_REFCNT();
}

nsInstallTrigger::~nsInstallTrigger()
{
}

NS_IMETHODIMP 
nsInstallTrigger::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
    if (aInstancePtr == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    // Always NULL result, in case of failure
    *aInstancePtr = NULL;

    if ( aIID.Equals(kIScriptObjectOwnerIID))
    {
        *aInstancePtr = (void*) ((nsIScriptObjectOwner*)this);
        AddRef();
        return NS_OK;
    }
    else if ( aIID.Equals(kIInstallTrigger_IID) )
    {
        *aInstancePtr = (void*) ((nsIDOMInstallTriggerGlobal*)this);
        AddRef();
        return NS_OK;
    }
    else if ( aIID.Equals(kISupportsIID) )
    {
        *aInstancePtr = (void*)(nsISupports*)(nsIScriptObjectOwner*)this;
        AddRef();
        return NS_OK;
    }

     return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsInstallTrigger)
NS_IMPL_RELEASE(nsInstallTrigger)



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

static NS_DEFINE_IID(kPrefsIID, NS_IPREF_IID);
static NS_DEFINE_IID(kPrefsCID,  NS_PREF_CID);

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
nsInstallTrigger::Install(nsXPITriggerInfo* aTrigger, PRBool* aReturn)
{
    nsresult rv;

    *aReturn = PR_FALSE;

    nsXPInstallManager *mgr = new nsXPInstallManager();
    if (mgr)
    {
        // The Install manager will delete itself when done
        rv = mgr->InitManager( aTrigger );
        if (NS_SUCCEEDED(rv))
            *aReturn = PR_TRUE;
    }
    else
        rv = NS_ERROR_OUT_OF_MEMORY;


    return rv;
}


NS_IMETHODIMP    
nsInstallTrigger::StartSoftwareUpdate(const nsString& aURL, PRInt32 aFlags, PRInt32* aReturn)
{
    nsresult rv = NS_ERROR_OUT_OF_MEMORY;

    // The Install manager will delete itself when done
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
                rv = mgr->InitManager( trigger );
            }
            else
            {
                rv = NS_ERROR_OUT_OF_MEMORY;
                delete trigger;
            }
        }
        else
            rv = NS_ERROR_OUT_OF_MEMORY;
    }
    else
        rv = NS_ERROR_OUT_OF_MEMORY;

    *aReturn = NS_OK;  // maybe we should do something more.
    return rv;
}

NS_IMETHODIMP    
nsInstallTrigger::ConditionalSoftwareUpdate(const nsString& aURL, const nsString& aRegName, PRInt32 aDiffLevel, const nsString& aVersion, PRInt32 aMode, PRInt32* aReturn)
{
    return NS_OK;
}

NS_IMETHODIMP    
nsInstallTrigger::ConditionalSoftwareUpdate(const nsString& aURL, const nsString& aRegName, PRInt32 aDiffLevel, nsIDOMInstallVersion* aVersion, PRInt32 aMode, PRInt32* aReturn)
{
    return NS_OK;
}

NS_IMETHODIMP    
nsInstallTrigger::ConditionalSoftwareUpdate(const nsString& aURL, const nsString& aRegName, nsIDOMInstallVersion* aVersion, PRInt32 aMode, PRInt32* aReturn)
{
    return NS_OK;
}

NS_IMETHODIMP    
nsInstallTrigger::ConditionalSoftwareUpdate(const nsString& aURL, const nsString& aRegName, const nsString& aVersion, PRInt32 aMode, PRInt32* aReturn)
{
    return NS_OK;
}

NS_IMETHODIMP    
nsInstallTrigger::ConditionalSoftwareUpdate(const nsString& aURL, const nsString& aRegName, const nsString& aVersion, PRInt32* aReturn)
{
    return NS_OK;
}

NS_IMETHODIMP    
nsInstallTrigger::ConditionalSoftwareUpdate(const nsString& aURL, const nsString& aRegName, nsIDOMInstallVersion* aVersion, PRInt32* aReturn)
{
    return NS_OK;
}

NS_IMETHODIMP    
nsInstallTrigger::CompareVersion(const nsString& aRegName, PRInt32 aMajor, PRInt32 aMinor, PRInt32 aRelease, PRInt32 aBuild, PRInt32* aReturn)
{
    return NS_OK;
}

NS_IMETHODIMP    
nsInstallTrigger::CompareVersion(const nsString& aRegName, const nsString& aVersion, PRInt32* aReturn)
{
    return NS_OK;
}

NS_IMETHODIMP    
nsInstallTrigger::CompareVersion(const nsString& aRegName, nsIDOMInstallVersion* aVersion, PRInt32* aReturn)
{
    return NS_OK;
}



// this will take a nsIURI, and create a temporary file.  If it is local, we just us it.
 
void
nsInstallTrigger::CreateTempFileFromURL(const nsString& aURL, nsString& tempFileString)
{
    // Checking to see if the url is local

    if ( aURL.EqualsIgnoreCase("file://", 7) )
    {       
        tempFileString.SetString( nsNSPRPath(nsFileURL(aURL)) );
    }
    else
    {
        nsSpecialSystemDirectory tempFile(nsSpecialSystemDirectory::OS_TemporaryDirectory);
    
        PRInt32 result = aURL.RFindChar('/');
        if (result != -1)
        {    
            nsString jarName;
                       
            aURL.Right(jarName, (aURL.Length() - result) );
            
            PRInt32 argOffset = jarName.RFindChar('?');

            if (argOffset != -1)
            {
                // we need to remove ? and everything after it
                jarName.Truncate(argOffset);
            }
            
            
            tempFile += jarName;
        }
        else
        {   
            tempFile += "xpinstall.jar";
        }

        tempFile.MakeUnique();

        tempFileString.SetString( nsNSPRPath( nsFilePath(tempFile) ) );
    }
}


/////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////



nsInstallTriggerFactory::nsInstallTriggerFactory(void)
{
    NS_INIT_REFCNT();
}

nsInstallTriggerFactory::~nsInstallTriggerFactory(void)
{
    NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}

NS_IMETHODIMP
nsInstallTriggerFactory::QueryInterface(const nsIID &aIID, void **aResult)
{
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    // Always NULL result, in case of failure
    *aResult = nsnull;

    if (aIID.Equals(kISupportsIID)) {
        *aResult = NS_STATIC_CAST(nsISupports*, this);
        AddRef();
        return NS_OK;
    } else if (aIID.Equals(kIFactoryIID)) {
        *aResult = NS_STATIC_CAST(nsIFactory*, this);
        AddRef();
        return NS_OK;
    }

    return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsInstallTriggerFactory);
NS_IMPL_RELEASE(nsInstallTriggerFactory);

NS_IMETHODIMP
nsInstallTriggerFactory::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
   if (! aResult)
        return NS_ERROR_NULL_POINTER;

    *aResult = nsnull;

    nsresult rv;

    nsInstallTrigger *inst = new nsInstallTrigger();
    
    if (! inst)
        return NS_ERROR_OUT_OF_MEMORY;

    if (NS_FAILED(rv = inst->QueryInterface(aIID, aResult))) 
    {
        // We didn't get the right interface.
        NS_ERROR("didn't support the interface you wanted");
    }
    return rv;
}

    
NS_IMETHODIMP
nsInstallTriggerFactory::LockFactory(PRBool aLock)
{
    // Not implemented in simplest case.
    return NS_OK;
}
