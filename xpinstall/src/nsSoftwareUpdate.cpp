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
#include "nsRepository.h"


#include "pratom.h"
#include "nsVector.h"
#include "VerReg.h"
#include "nsSpecialSystemDirectory.h"

#include "nsInstall.h"
#include "nsSoftwareUpdateIIDs.h"
#include "nsSoftwareUpdate.h"
#include "nsSoftwareUpdateStream.h"
#include "nsSoftwareUpdateRun.h"
#include "nsInstallTrigger.h"
#include "nsInstallVersion.h"
#include "ScheduledTasks.h"

/* For Javascript Namespace Access */
#include "nsDOMCID.h"
#include "nsIServiceManager.h"
#include "nsINameSpaceManager.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptNameSetRegistry.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIScriptExternalNameSet.h"

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

static NS_DEFINE_IID(kIInstallTrigger_IID, NS_IDOMINSTALLTRIGGERGLOBAL_IID);
static NS_DEFINE_IID(kInstallTrigger_CID, NS_SoftwareUpdateInstallTrigger_CID);

static NS_DEFINE_IID(kIInstallVersion_IID, NS_IDOMINSTALLVERSION_IID);
static NS_DEFINE_IID(kInstallVersion_CID, NS_SoftwareUpdateInstallVersion_CID);




nsSoftwareUpdate* nsSoftwareUpdate::mInstance = NULL;

nsSoftwareUpdate::nsSoftwareUpdate()
{
    NS_INIT_ISUPPORTS();
    
    Startup();
}
nsSoftwareUpdate::~nsSoftwareUpdate()
{
    Shutdown();
}

nsSoftwareUpdate *nsSoftwareUpdate::GetInstance()
{
  if (mInstance == NULL) 
  {
    mInstance = new nsSoftwareUpdate();
  }
  return mInstance;
}


NS_IMPL_ISUPPORTS(nsSoftwareUpdate,nsISoftwareUpdate::GetIID());


nsresult
nsSoftwareUpdate::Startup()
{
    /***************************************/
    /* Create us a queue                   */
    /***************************************/
    mInstalling = nsnull;

    mJarInstallQueue = new nsVector();

    /***************************************/
    /* Add us to the Javascript Name Space */
    /***************************************/
   
    new nsSoftwareUpdateNameSet();
    
    /***************************************/
    /* Register us with NetLib             */
    /***************************************/
        // FIX 
    
    
    /***************************************/
    /* Startup the Version Registry        */
    /***************************************/
    
    nsSpecialSystemDirectory appDir(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
    VR_SetRegDirectory(appDir.GetCString());
 
    NR_StartupRegistry();   /* startup the registry; if already started, this will essentially be a noop */

    /***************************************/
    /* Stupid Hack to test js env*/
    /***************************************/
    // FIX:  HACK HACK HACK!
#if 1  
    nsSpecialSystemDirectory jarFile(nsSpecialSystemDirectory::OS_TemporaryDirectory);
    jarFile += "test.jar";
    if (jarFile.Exists())
    {
        InstallJar(nsString(nsFileURL(jarFile).GetAsString()), "", "");
    }
#endif    
    /***************************************/
    /* Preform Scheduled Tasks             */
    /***************************************/
    
    DeleteScheduledFiles();
    ReplaceScheduledFiles();
        
    
    return NS_OK;
}


nsresult
nsSoftwareUpdate::Shutdown()
{
    if (mJarInstallQueue != nsnull)
    {
        PRUint32 i=0;
        for (; i < mJarInstallQueue->GetSize(); i++) 
        {
            nsInstallInfo* element = (nsInstallInfo*)mJarInstallQueue->Get(i);
            //FIX:  need to add to registry....
            delete element;
        }

        mJarInstallQueue->RemoveAll();
        delete (mJarInstallQueue);
        mJarInstallQueue = nsnull;
    }

    NR_ShutdownRegistry();
    return NS_OK;
}

NS_IMETHODIMP
nsSoftwareUpdate::InstallJar(  const nsString& fromURL, 
                               const nsString& flags, 
                               const nsString& args)
{
    nsInstallInfo *installInfo = new nsInstallInfo(fromURL, flags, args);
    InstallJar(installInfo);
    
    return NS_OK;
}



NS_IMETHODIMP
nsSoftwareUpdate::InstallJar(nsInstallInfo *installInfo)
{
    mJarInstallQueue->Add( installInfo );
    
    RunNextInstall();

    return NS_OK;
}


NS_IMETHODIMP
nsSoftwareUpdate::InstallJarCallBack()
{
    nsInstallInfo *nextInstall = (nsInstallInfo*)mJarInstallQueue->Get(0);
    
    if (nextInstall != nsnull)
        delete nextInstall;
    
    mJarInstallQueue->Remove(0);
    
    mInstalling = PR_FALSE;

    return RunNextInstall();

}


NS_IMETHODIMP
nsSoftwareUpdate::RunNextInstall()
{
    if (mInstalling == PR_TRUE)
        return NS_OK;

    mInstalling = PR_TRUE;

    //  check to see if there is anything in our queue
    if (mJarInstallQueue->GetSize() <= 0)
    {
        mInstalling = PR_FALSE;
        return NS_OK;
    }
    
    nsInstallInfo *nextInstall = (nsInstallInfo*)mJarInstallQueue->Get(0);
        
    if (nextInstall->IsMultipleTrigger() == PR_FALSE)
    {
        RunInstall( nextInstall );
    }
    else
    {
        ; // should we do something different?! 
    }
    
    return NS_OK;
}


/////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////
static PRInt32 gSoftwareUpdateLock        = 0;

nsSoftwareUpdateFactory::nsSoftwareUpdateFactory(void)
{
    NS_INIT_ISUPPORTS();
}

nsSoftwareUpdateFactory::~nsSoftwareUpdateFactory(void)
{
}

NS_IMPL_ISUPPORTS(nsSoftwareUpdateFactory,kIFactoryIID)

NS_IMETHODIMP
nsSoftwareUpdateFactory::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aResult == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    *aResult = NULL;

    nsSoftwareUpdate *inst = nsSoftwareUpdate::GetInstance();

    if (inst == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult result =  inst->QueryInterface(aIID, aResult);

    if (NS_FAILED(result)) 
    {
        *aResult = NULL;
    }

    NS_ADDREF(inst);  // Are we sure that we need to addref???

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

  nsIScriptNameSetRegistry *scriptNameSet;
  nsresult result = nsServiceManager::GetService(kCScriptNameSetRegistryCID,
                                                 kIScriptNameSetRegistryIID,
                                                (nsISupports **)&scriptNameSet);
    if (NS_SUCCEEDED(result)) 
    {
        scriptNameSet->AddExternalNameSet(this);
    }

}

nsSoftwareUpdateNameSet::~nsSoftwareUpdateNameSet()
{
}

NS_IMPL_ISUPPORTS(nsSoftwareUpdateNameSet, kIScriptExternalNameSetIID);


NS_IMETHODIMP
nsSoftwareUpdateNameSet::InitializeClasses(nsIScriptContext* aScriptContext)
{
    nsresult result = NS_OK;

    result = NS_InitInstallVersionClass(aScriptContext, nsnull);
    if (NS_FAILED(result)) return result;

    result = NS_InitInstallTriggerGlobalClass(aScriptContext, nsnull);

    return result;
}


NS_IMETHODIMP
nsSoftwareUpdateNameSet::AddNameSet(nsIScriptContext* aScriptContext)
{
    nsresult result = NS_OK;
    nsIScriptNameSpaceManager* manager;

    result = aScriptContext->GetNameSpaceManager(&manager);
    if (NS_SUCCEEDED(result)) 
    {
        result = manager->RegisterGlobalName("InstallVersion", 
                                             kInstallVersion_CID, 
                                             PR_TRUE);
        
        if (NS_FAILED(result))  return result;
        
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
NSCanUnload(nsISupports* serviceMgr)
{
    return PR_FALSE;
}

extern "C" NS_EXPORT nsresult
NSRegisterSelf(nsISupports* serviceMgr, const char *path)
{
    printf("*** XPInstall is being registered\n");
    
    nsRepository::RegisterComponent(kSoftwareUpdate_CID, NULL, NULL, path, PR_TRUE, PR_TRUE);
    nsRepository::RegisterComponent(kInstallTrigger_CID, NULL, NULL, path, PR_TRUE, PR_TRUE);
    nsRepository::RegisterComponent(kInstallVersion_CID, NULL, NULL, path, PR_TRUE, PR_TRUE);

    return NS_OK;
}

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports* serviceMgr, const char *path)
{
    printf("*** XPInstall is being unregistered\n");

    nsRepository::UnregisterFactory(kSoftwareUpdate_CID, path);
    nsRepository::UnregisterFactory(kInstallTrigger_CID, path);
    nsRepository::UnregisterFactory(kInstallVersion_CID, path);

    return NS_OK;
}



extern "C" NS_EXPORT nsresult
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{

    if (aFactory == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    *aFactory = NULL;
    nsISupports *inst;

    
    if (aClass.Equals(kInstallTrigger_CID) )
    {
        inst = new nsInstallTriggerFactory();
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

    if (NS_FAILED(res)) 
    {   
        delete inst;
    }

    return res;

}


