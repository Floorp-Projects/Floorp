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


#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"

#include "pratom.h"

#include "nsRepository.h"

#include "VerReg.h"

#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"

/* For Javascript Namespace Access */
#include "nsDOMCID.h"
#include "nsIServiceManager.h"
#include "nsINameSpaceManager.h"
#include "nsIScriptNameSetRegistry.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIScriptExternalNameSet.h"

/* Network */
#include "net.h"

#include "nsSoftwareUpdateIIDs.h"
#include "nsSoftwareUpdate.h"
#include "nsSoftwareUpdateStream.h"
#include "nsSoftwareUpdateRun.h"

#include "nsIDOMInstall.h"
#include "nsInstall.h"

#include "nsIDOMInstallTriggerGlobal.h"
#include "nsInstallTrigger.h"

#include "nsIDOMInstallVersion.h"
#include "nsInstallVersion.h"

#include "nsIDOMInstallFolder.h"
#include "nsInstallFolder.h"

////////////////////////////////////////////////////////////////////////////////
// Globals
////////////////////////////////////////////////////////////////////////////////
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);

static NS_DEFINE_IID(kIScriptNameSetRegistryIID, NS_ISCRIPTNAMESETREGISTRY_IID);
static NS_DEFINE_IID(kCScriptNameSetRegistryCID, NS_SCRIPT_NAMESET_REGISTRY_CID);
static NS_DEFINE_IID(kIScriptExternalNameSetIID, NS_ISCRIPTEXTERNALNAMESET_IID);

static NS_DEFINE_IID(kISoftwareUpdate_IID, NS_ISOFTWAREUPDATE_IID);
static NS_DEFINE_IID(kSoftwareUpdate_CID,  NS_SoftwareUpdate_CID);

static NS_DEFINE_IID(kIInstall_IID, NS_IDOMINSTALL_IID);
static NS_DEFINE_IID(kInstall_CID, NS_SoftwareUpdateInstall_CID);

static NS_DEFINE_IID(kIInstallTrigger_IID, NS_IDOMINSTALLTRIGGERGLOBAL_IID);
static NS_DEFINE_IID(kInstallTrigger_CID, NS_SoftwareUpdateInstallTrigger_CID);

static NS_DEFINE_IID(kIInstallVersion_IID, NS_IDOMINSTALLVERSION_IID);
static NS_DEFINE_IID(kInstallVersion_CID, NS_SoftwareUpdateInstallVersion_CID);

static NS_DEFINE_IID(kIInstallFolder_IID, NS_IDOMINSTALLFOLDER_IID);
static NS_DEFINE_IID(kInstallFolder_CID, NS_SoftwareUpdateInstallFolder_CID);

static PRInt32 gInstanceCnt = 0;
static PRInt32 gLockCnt     = 0;




nsSoftwareUpdate::nsSoftwareUpdate()
{
        NS_INIT_REFCNT();
}

nsSoftwareUpdate::~nsSoftwareUpdate()
{
}

NS_IMETHODIMP 
nsSoftwareUpdate::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
    if (aInstancePtr == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    // Always NULL result, in case of failure
    *aInstancePtr = NULL;

    if ( aIID.Equals(kISoftwareUpdate_IID) )
    {
        *aInstancePtr = (void*)(nsISoftwareUpdate*)this;
        AddRef();
        return NS_OK;
    }
    else if ( aIID.Equals(kISupportsIID) )
    {
        *aInstancePtr = (void*)(nsISupports*)this;
        AddRef();
        return NS_OK;
    }

     return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsSoftwareUpdate)
NS_IMPL_RELEASE(nsSoftwareUpdate)

NS_IMETHODIMP
nsSoftwareUpdate::Startup()
{
    /***************************************/
    /* Add us to the Javascript Name Space */
    /***************************************/
   
    //   FIX:  Only add the Trigger Object to the JS NameSpace.  Then when before we run
    //         the InstallScript, add our other objects to just that env.

    nsIScriptNameSetRegistry *registry;
    nsresult result = nsServiceManager::GetService(kCScriptNameSetRegistryCID,
                                                   kIScriptNameSetRegistryIID,
                                                  (nsISupports **)&registry);
    if (NS_OK == result) 
    {
        nsSoftwareUpdateNameSet* nameSet = new nsSoftwareUpdateNameSet();
        registry->AddExternalNameSet(nameSet);
        /* FIX - do we need to release this service?  When we do, it get deleted,and our name is lost. */
    }

    /***************************************/
    /* Register us with NetLib             */
    /***************************************/
        // FIX 
    
    
    /***************************************/
    /* Startup the Version Registry        */
    /***************************************/
    //FIX  we need an api that will get us this data
    
    VR_SetRegDirectory("C:\\temp\\");
    NR_StartupRegistry();   /* startup the registry; if already started, this will essentially be a noop */

    /***************************************/
    /* Stupid Hack to test js env*/
    /***************************************/

    RunInstallJS("c:\\temp\\test.js");

    DeleteScheduledNodes();
    
    return NS_OK;
}


NS_IMETHODIMP
nsSoftwareUpdate::Shutdown()
{
    NR_ShutdownRegistry();
    return NS_OK;
}


nsresult
nsSoftwareUpdate::DeleteScheduledNodes()
{
    return NS_OK;
}

/////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////
static PRInt32 gSoftwareUpdateInstanceCnt = 0;
static PRInt32 gSoftwareUpdateLock        = 0;

nsSoftwareUpdateFactory::nsSoftwareUpdateFactory(void)
{
    mRefCnt=0;
    PR_AtomicIncrement(&gSoftwareUpdateInstanceCnt);
}

nsSoftwareUpdateFactory::~nsSoftwareUpdateFactory(void)
{
    PR_AtomicDecrement(&gSoftwareUpdateInstanceCnt);
}

NS_IMETHODIMP 
nsSoftwareUpdateFactory::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
    if (aInstancePtr == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    // Always NULL result, in case of failure
    *aInstancePtr = NULL;

    if ( aIID.Equals(kISupportsIID) )
    {
        *aInstancePtr = (void*) this;
    }
    else if ( aIID.Equals(kIFactoryIID) )
    {
        *aInstancePtr = (void*) this;
    }

    if (aInstancePtr == NULL)
    {
        return NS_ERROR_NO_INTERFACE;
    }

    AddRef();
    return NS_OK;
}



NS_IMETHODIMP
nsSoftwareUpdateFactory::AddRef(void)
{
    return ++mRefCnt;
}


NS_IMETHODIMP
nsSoftwareUpdateFactory::Release(void)
{
    if (--mRefCnt ==0)
    {
        delete this;
        return 0; // Don't access mRefCnt after deleting!
    }

    return mRefCnt;
}

NS_IMETHODIMP
nsSoftwareUpdateFactory::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aResult == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    *aResult = NULL;

    nsSoftwareUpdate *inst = new nsSoftwareUpdate();

    if (inst == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult result =  inst->QueryInterface(aIID, aResult);

    if (result != NS_OK)
        delete inst;

    return result;
}

NS_IMETHODIMP
nsSoftwareUpdateFactory::LockFactory(PRBool aLock)
{
    if (aLock)
        PR_AtomicIncrement(&gSoftwareUpdateLock);
    else
        PR_AtomicDecrement(&gSoftwareUpdateLock);

    return NS_OK;
}



////////////////////////////////////////////////////////////////////////////////
// nsSoftwareUpdateNameSet
////////////////////////////////////////////////////////////////////////////////

nsSoftwareUpdateNameSet::nsSoftwareUpdateNameSet()
{
  NS_INIT_REFCNT();
}

nsSoftwareUpdateNameSet::~nsSoftwareUpdateNameSet()
{
}

NS_IMPL_ISUPPORTS(nsSoftwareUpdateNameSet, kIScriptExternalNameSetIID);


NS_IMETHODIMP
nsSoftwareUpdateNameSet::InitializeClasses(nsIScriptContext* aScriptContext)
{
    nsresult result = NS_OK;

    result = NS_InitInstallClass(aScriptContext, nsnull);
    if (result != NS_OK) return result;
    
    result = NS_InitInstallVersionClass(aScriptContext, nsnull);
    if (result != NS_OK) return result;

    result = NS_InitInstallFolderClass(aScriptContext, nsnull);
    if (result != NS_OK) return result;

    result = NS_InitInstallTriggerGlobalClass(aScriptContext, nsnull);

    return result;
}


NS_IMETHODIMP
nsSoftwareUpdateNameSet::AddNameSet(nsIScriptContext* aScriptContext)
{
    nsresult result = NS_OK;
    nsIScriptNameSpaceManager* manager;

    result = aScriptContext->GetNameSpaceManager(&manager);
    if (NS_OK == result) 
    {

        result = manager->RegisterGlobalName("Install", 
                                             kInstall_CID, 
                                             PR_TRUE);

        if (result != NS_OK) return result;
        
        result = manager->RegisterGlobalName("InstallVersion", 
                                             kInstallVersion_CID, 
                                             PR_TRUE);
        
        if (result != NS_OK) return result;
        
        result = manager->RegisterGlobalName("InstallFolder", 
                                             kInstallFolder_CID, 
                                             PR_TRUE);
        
        if (result != NS_OK) return result;
        

        result = manager->RegisterGlobalName("InstallTrigger", 
                                             kInstallTrigger_CID, 
                                             PR_FALSE);

        

        
    }
    
    if (manager != nsnull)
        NS_RELEASE(manager);

    return result;
}

////////////////////////////////////////////////////////////////////////////////
// DLL Entry Points:
////////////////////////////////////////////////////////////////////////////////

extern "C" NS_EXPORT PRBool
NSCanUnload(void)
{
    return PRBool (gInstanceCnt == 0 && gLockCnt == 0);
}

extern "C" NS_EXPORT nsresult
NSRegisterSelf(const char *path)
{
    nsRepository::RegisterFactory(kSoftwareUpdate_CID, path, PR_TRUE, PR_TRUE);
    nsRepository::RegisterFactory(kInstall_CID, path, PR_TRUE, PR_TRUE);
    nsRepository::RegisterFactory(kInstallTrigger_CID, path, PR_TRUE, PR_TRUE);
    nsRepository::RegisterFactory(kInstallVersion_CID, path, PR_TRUE, PR_TRUE);
    nsRepository::RegisterFactory(kInstallFolder_CID, path, PR_TRUE, PR_TRUE);
    return NS_OK;
}

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(const char *path)
{
    nsRepository::UnregisterFactory(kSoftwareUpdate_CID, path);
    nsRepository::UnregisterFactory(kInstall_CID, path);
    nsRepository::UnregisterFactory(kInstallTrigger_CID, path);
    nsRepository::UnregisterFactory(kInstallVersion_CID, path);
    nsRepository::UnregisterFactory(kInstallFolder_CID, path);

    return NS_OK;
}



extern "C" NS_EXPORT nsresult
NSGetFactory(const nsCID &aClass, nsISupports* serviceMgr, nsIFactory **aFactory)
{

    if (aFactory == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    *aFactory = NULL;
    nsISupports *inst;


    if ( aClass.Equals(kInstall_CID) )
    {
        inst = new nsInstallFactory();        
    }
    else if (aClass.Equals(kInstallTrigger_CID) )
    {
        inst = new nsInstallTriggerFactory();
    }
    else if (aClass.Equals(kInstallFolder_CID) )
    {
        inst = new nsInstallFolderFactory();
    }
    else if (aClass.Equals(kInstallVersion_CID) )
    {
        inst = new nsInstallVersionFactory();
    }
    else if (aClass.Equals(kSoftwareUpdate_CID) )
    {
        inst = new nsSoftwareUpdateFactory();
    }
    else
    {
        return NS_ERROR_ILLEGAL_VALUE;
    }


    if (inst == NULL)
    {   
        return NS_ERROR_OUT_OF_MEMORY;
    }


    nsresult res = inst->QueryInterface(kIFactoryIID, (void**) aFactory);

    if (res != NS_OK)
    {   
        delete inst;
    }

    return res;

}


