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


#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

#include "nspr.h"
#include "prlock.h"
#include "VerReg.h"
#include "nsSpecialSystemDirectory.h"

#include "nsInstall.h"
#include "nsSoftwareUpdateIIDs.h"
#include "nsSoftwareUpdate.h"
#include "nsSoftwareUpdateRun.h"
#include "nsInstallTrigger.h"
#include "nsInstallVersion.h"
#include "ScheduledTasks.h"

#include "nsTopProgressNotifier.h"
#include "nsLoggingProgressNotifier.h"
#include "nsInstallProgressDialog.h"

#include "nsIAppShellComponent.h"
#include "nsIRegistry.h"

/* For Javascript Namespace Access */
#include "nsDOMCID.h"
#include "nsIServiceManager.h"
#include "nsINameSpaceManager.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptNameSetRegistry.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIScriptExternalNameSet.h"

#include "nsIEventQueueService.h"
#include "nsProxyObjectManager.h"


////////////////////////////////////////////////////////////////////////////////
// Globals
////////////////////////////////////////////////////////////////////////////////
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

static NS_DEFINE_IID(kIScriptNameSetRegistryIID, NS_ISCRIPTNAMESETREGISTRY_IID);
static NS_DEFINE_IID(kCScriptNameSetRegistryCID, NS_SCRIPT_NAMESET_REGISTRY_CID);
static NS_DEFINE_IID(kIScriptExternalNameSetIID, NS_ISCRIPTEXTERNALNAMESET_IID);

static NS_DEFINE_IID(kISoftwareUpdate_IID, NS_ISOFTWAREUPDATE_IID);
static NS_DEFINE_IID(kSoftwareUpdate_CID,  NS_SoftwareUpdate_CID);

static NS_DEFINE_IID(kIInstallTrigger_IID, NS_IDOMINSTALLTRIGGERGLOBAL_IID);
static NS_DEFINE_IID(kInstallTrigger_CID, NS_SoftwareUpdateInstallTrigger_CID);

static NS_DEFINE_IID(kIInstallVersion_IID, NS_IDOMINSTALLVERSION_IID);
static NS_DEFINE_IID(kInstallVersion_CID, NS_SoftwareUpdateInstallVersion_CID);


nsSoftwareUpdate* nsSoftwareUpdate::mInstance = nsnull;
nsIFileSpec*      nsSoftwareUpdate::mProgramDir = nsnull;


nsSoftwareUpdate *
nsSoftwareUpdate::GetInstance()
{
    if (mInstance == nsnull) 
    {
        mInstance = new nsSoftwareUpdate();
        NS_IF_ADDREF(mInstance);
    }
    return mInstance;
}



nsSoftwareUpdate::nsSoftwareUpdate()
{
    NS_INIT_ISUPPORTS();

    mStubLockout = PR_FALSE;
    /***************************************/
    /* Create us a queue                   */
    /***************************************/
    mLock = PR_NewLock();
    mInstalling = PR_FALSE;
    mJarInstallQueue = new nsVoidArray();

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

    NR_StartupRegistry();   /* startup the registry; if already started, this will essentially be a noop */

    nsSpecialSystemDirectory appDir(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
    VR_SetRegDirectory( appDir.GetNativePathCString() );


    /***************************************/
    /* Perform Scheduled Tasks             */
    /***************************************/

/* XXX Temporary workaround: see bugs 8849, 8971 */
#ifndef XP_UNIX 
    PR_CreateThread(PR_USER_THREAD,
                    PerformScheduledTasks,
                    nsnull, 
                    PR_PRIORITY_NORMAL, 
                    PR_GLOBAL_THREAD, 
                    PR_UNJOINABLE_THREAD,
                    0);  
#endif


    /***************************************/
    /* Create a top level observer         */
    /***************************************/

    nsLoggingProgressNotifier *logger = new nsLoggingProgressNotifier();
    RegisterNotifier(logger);
}





nsSoftwareUpdate::~nsSoftwareUpdate()
{
    PR_Lock(mLock);
    if (mJarInstallQueue != nsnull)
    {
        nsInstallInfo* element;
        for (PRInt32 i=0; i < mJarInstallQueue->Count(); i++)
        {
            element = (nsInstallInfo*)mJarInstallQueue->ElementAt(i);
            //FIX:  need to add to registry....
            delete element;
        }

        mJarInstallQueue->Clear();
        delete (mJarInstallQueue);
        mJarInstallQueue = nsnull;
    }
    PR_Unlock(mLock);
    PR_DestroyLock(mLock);

    NR_ShutdownRegistry();

    NS_IF_RELEASE( mProgramDir );
}


//------------------------------------------------------------------------
//  nsISupports implementation
//------------------------------------------------------------------------
NS_IMPL_ADDREF( nsSoftwareUpdate );
NS_IMPL_RELEASE( nsSoftwareUpdate );


NS_IMETHODIMP
nsSoftwareUpdate::QueryInterface( REFNSIID anIID, void **anInstancePtr )
{
    nsresult rv = NS_OK;
    /* Check for place to return result. */
    
    if ( !anInstancePtr )
    {
        rv = NS_ERROR_NULL_POINTER;
    }
    else
    {
        /* Check for IIDs we support and cast this appropriately. */
        if ( anIID.Equals( nsISoftwareUpdate::GetIID() ) ) 
            *anInstancePtr = (void*) ( (nsISoftwareUpdate*)this );
        else if ( anIID.Equals( nsIAppShellComponent::GetIID() ) ) 
            *anInstancePtr = (void*) ( (nsIAppShellComponent*)this );
        else if (anIID.Equals( nsPIXPIStubHook::GetIID() ) )
            *anInstancePtr = (void*) ( (nsPIXPIStubHook*)this );
        else if ( anIID.Equals( kISupportsIID ) )
            *anInstancePtr = (void*) ( (nsISupports*) (nsISoftwareUpdate*) this );
        else
        {
            /* Not an interface we support. */
            *anInstancePtr = 0;
            rv = NS_NOINTERFACE;
        }
    }

    if (NS_SUCCEEDED(rv))
        NS_ADDREF_THIS();

    return rv;
}


NS_IMETHODIMP
nsSoftwareUpdate::Initialize( nsIAppShellService *anAppShell, nsICmdLineService  *aCmdLineService ) 
{
    mStubLockout = PR_TRUE;  // prevent use of nsPIXPIStubHook by browser

//    rv = nsServiceManager::RegisterService( NS_IXPINSTALLCOMPONENT_PROGID, ( (nsISupports*) (nsISoftwareUpdate*) this ) );
    return NS_OK;
}

NS_IMETHODIMP
nsSoftwareUpdate::Shutdown()
{
//    rv = nsServiceManager::UnregisterService( NS_IXPINSTALLCOMPONENT_PROGID );
    NS_IF_RELEASE(mInstance);
    return NS_OK;
}


NS_IMETHODIMP 
nsSoftwareUpdate::RegisterNotifier(nsIXPINotifier *notifier)
{
    // we are going to ignore the returned ID and enforce that once you 
    // register a notifier, you can not remove it.  This should at some
    // point be fixed.

    (void) mMasterNotifier.RegisterNotifier(notifier);
    
    return NS_OK;
}

NS_IMETHODIMP
nsSoftwareUpdate::GetMasterNotifier(nsIXPINotifier **notifier)
{
    NS_ASSERTION(notifier, "getter has invalid return pointer");
    if (!notifier)
        return NS_ERROR_NULL_POINTER;

    *notifier = &mMasterNotifier;
    return NS_OK;
}


NS_IMETHODIMP
nsSoftwareUpdate::SetActiveNotifier(nsIXPINotifier *notifier)
{
    mMasterNotifier.SetActiveNotifier(notifier);
    return NS_OK;
}

NS_IMETHODIMP
nsSoftwareUpdate::InstallJar(  nsIFileSpec* aLocalFile,
                               const PRUnichar* aURL,
                               const PRUnichar* aArguments,
                               long flags,
                               nsIXPINotifier* aNotifier)
{
    if ( !aLocalFile )
        return NS_ERROR_NULL_POINTER;

    nsInstallInfo *info =
        new nsInstallInfo( aLocalFile, aURL, aArguments, flags, aNotifier );
    
    if (!info)
        return NS_ERROR_OUT_OF_MEMORY;

    PR_Lock(mLock);
    mJarInstallQueue->AppendElement( info );
    PR_Unlock(mLock);
    RunNextInstall();

    return NS_OK;
}


NS_IMETHODIMP
nsSoftwareUpdate::InstallJarCallBack()
{
    PR_Lock(mLock);

    nsInstallInfo *nextInstall = (nsInstallInfo*)mJarInstallQueue->ElementAt(0);
    if (nextInstall != nsnull)
        delete nextInstall;

    mJarInstallQueue->RemoveElementAt(0);
    mInstalling = PR_FALSE;

    PR_Unlock(mLock);

    return RunNextInstall();
}


nsresult
nsSoftwareUpdate::RunNextInstall()
{
    nsresult        rv = NS_OK;
    nsInstallInfo*  info = nsnull;

    PR_Lock(mLock);
    if (!mInstalling) 
    {
        if ( mJarInstallQueue->Count() > 0 )
        {
            info = (nsInstallInfo*)mJarInstallQueue->ElementAt(0);

            if ( info )
                mInstalling = PR_TRUE;
            else 
            {
                // bogus elements got into the queue
                NS_ERROR("leaks remaining nsInstallInfos, please file bug!");
                rv = NS_ERROR_NULL_POINTER;
                VR_Close();
            }
        }
        else
        {
            // nothing more to do
            VR_Close();
        }
    }
    PR_Unlock(mLock);

    // make sure to RunInstall() outside of locked section due to callbacks
    if (info)
        RunInstall( info );

    return rv;
}


NS_IMETHODIMP
nsSoftwareUpdate::SetProgramDirectory(nsIFileSpec *aDir)
{
    if (mStubLockout)
        return NS_ERROR_ABORT;
    else if ( !aDir )
        return NS_ERROR_NULL_POINTER;

    // only allow once, it would be a mess if we've already started installing
    mStubLockout = PR_TRUE;

    // fix GetFolder return path
    mProgramDir = aDir;
    NS_ADDREF(mProgramDir);

    // setup version registry path
    char*    path;
    nsresult rv = aDir->GetNativePath( &path );
    if (NS_SUCCEEDED(rv))
    {
        VR_SetRegDirectory( path );
        nsCRT::free( path );
    }

    return rv;
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
            
//----------------------------------------------------------------------

// Functions used to create new instances of a given object by the
// generic factory.

static NS_IMETHODIMP      
CreateNewSoftwareUpdate(nsISupports* aOuter, REFNSIID aIID, void **aResult)
{                                                                  
    if (!aResult) {                                                
        return NS_ERROR_INVALID_POINTER;                           
    }                                                              
    if (aOuter) {                                                  
        *aResult = nsnull;                                         
        return NS_ERROR_NO_AGGREGATION;                              
    }                                                                
    nsSoftwareUpdate* inst = nsSoftwareUpdate::GetInstance();
    if (inst == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    
    nsresult rv = inst->QueryInterface(aIID, aResult);                        
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
    }                                                                
    return rv;                                                       
}


static NS_IMETHODIMP      
CreateNewInstallTrigger(nsISupports* aOuter, REFNSIID aIID, void **aResult)
{                                                                  
    if (!aResult) {                                                
        return NS_ERROR_INVALID_POINTER;                           
    }                                                              
    if (aOuter) {                                                  
        *aResult = nsnull;                                         
        return NS_ERROR_NO_AGGREGATION;                              
    }                                                                
    nsInstallTrigger* inst = new nsInstallTrigger();
    if (inst == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    
    NS_ADDREF(inst);
    nsresult rv = inst->QueryInterface(aIID, aResult);                        
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
    }  
    NS_RELEASE(inst);
    return rv;                                                       
}

static NS_IMETHODIMP      
CreateNewInstallVersion(nsISupports* aOuter, REFNSIID aIID, void **aResult)
{                                                                  
    if (!aResult) {                                                
        return NS_ERROR_INVALID_POINTER;                           
    }                                                              
    if (aOuter) {                                                  
        *aResult = nsnull;                                         
        return NS_ERROR_NO_AGGREGATION;                              
    }                                                                
    nsInstallVersion* inst = new nsInstallVersion();
    if (inst == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(inst);
    nsresult rv = inst->QueryInterface(aIID, aResult);                        
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
    }  
    NS_RELEASE(inst);             /* get rid of extra refcnt */ 
    return rv;                                                       
}

//----------------------------------------------------------------------

nsSoftwareUpdateModule::nsSoftwareUpdateModule()
    : mInitialized(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsSoftwareUpdateModule::~nsSoftwareUpdateModule()
{
    Shutdown();
}

NS_IMPL_ISUPPORTS(nsSoftwareUpdateModule, NS_GET_IID(nsIModule))

// Perform our one-time intialization for this module
nsresult
nsSoftwareUpdateModule::Initialize()
{
    if (mInitialized) {
        return NS_OK;
    }
    mInitialized = PR_TRUE;
    return NS_OK;
}

// Shutdown this module, releasing all of the module resources
void
nsSoftwareUpdateModule::Shutdown()
{
    // Release the factory objects
    mSoftwareUpdateFactory = nsnull;
    mInstallTriggerFactory = nsnull;
    mInstallVersionFactory = nsnull;
}

// Create a factory object for creating instances of aClass.
NS_IMETHODIMP
nsSoftwareUpdateModule::GetClassObject(nsIComponentManager *aCompMgr,
                               const nsCID& aClass,
                               const nsIID& aIID,
                               void** r_classObj)
{
    nsresult rv;

    // Defensive programming: Initialize *r_classObj in case of error below
    if (!r_classObj) {
        return NS_ERROR_INVALID_POINTER;
    }
    *r_classObj = NULL;

    // Do one-time-only initialization if necessary
    if (!mInitialized) {
        rv = Initialize();
        if (NS_FAILED(rv)) {
            // Initialization failed! yikes!
            return rv;
        }
    }

    // Choose the appropriate factory, based on the desired instance
    // class type (aClass).
    nsCOMPtr<nsIGenericFactory> fact;
    if (aClass.Equals(kSoftwareUpdate_CID)) {
        if (!mSoftwareUpdateFactory) {
            // Create and save away the factory object for creating
            // new instances of SoftwareUpdate. This way if we are called
            // again for the factory, we won't need to create a new
            // one.
            rv = NS_NewGenericFactory(getter_AddRefs(mSoftwareUpdateFactory),
                                      CreateNewSoftwareUpdate);
        }
        fact = mSoftwareUpdateFactory;
    }
    else if (aClass.Equals(kInstallTrigger_CID)) {
        if (!mInstallTriggerFactory) {
            // Create and save away the factory object for creating
            // new instances of InstallTrigger. This way if we are called
            // again for the factory, we won't need to create a new
            // one.
            rv = NS_NewGenericFactory(getter_AddRefs(mInstallTriggerFactory),
                                      CreateNewInstallTrigger);
        }
        fact = mInstallTriggerFactory;
    }
    else if (aClass.Equals(kInstallVersion_CID)) {
        if (!mInstallVersionFactory) {
            // Create and save away the factory object for creating
            // new instances of InstallVersion. This way if we are called
            // again for the factory, we won't need to create a new
            // one.
            rv = NS_NewGenericFactory(getter_AddRefs(mInstallVersionFactory),
                                      CreateNewInstallVersion);
        }
        fact = mInstallVersionFactory;
    }
    else {
        rv = NS_ERROR_FACTORY_NOT_REGISTERED;
#ifdef DEBUG
        char* cs = aClass.ToString();
        printf("+++ nsSoftwareUpdateModule: unable to create factory for %s\n", cs);
        nsCRT::free(cs);
#endif
    }

    if (fact) {
        rv = fact->QueryInterface(aIID, r_classObj);
    }

    return rv;
}

//----------------------------------------

struct Components {
    const char* mDescription;
    const nsID* mCID;
    const char* mProgID;
};

// The list of components we register
static Components gComponents[] = {
    { "SoftwareUpdate Component", &kSoftwareUpdate_CID,
      NS_IXPINSTALLCOMPONENT_PROGID, },
    { "InstallTrigger Component", &kInstallTrigger_CID,
      NS_INSTALLTRIGGERCOMPONENT_PROGID, },
    { "InstallVersion Component", &kInstallVersion_CID,
      NS_INSTALLVERSIONCOMPONENT_PROGID, },
};
#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))

NS_IMETHODIMP
nsSoftwareUpdateModule::RegisterSelf(nsIComponentManager *aCompMgr,
                             nsIFileSpec* aPath,
                             const char* registryLocation,
                             const char* componentType)
{
    nsresult rv = NS_OK;

#ifdef DEBUG
    printf("*** XPInstall is being registered\n");
#endif

    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                             cp->mProgID, aPath, PR_TRUE,
                                             PR_TRUE);
    if ( NS_SUCCEEDED( rv ) ) 
    {
        // get the registry
        nsIRegistry* registry;
        rv = nsServiceManager::GetService(NS_REGISTRY_PROGID,
                                          nsIRegistry::GetIID(),
                                          (nsISupports**)&registry);
        if ( NS_SUCCEEDED( rv ) ) 
        {
            registry->OpenWellKnownRegistry(nsIRegistry::ApplicationComponentRegistry);
            char buffer[256];
            char *cid = nsSoftwareUpdate::GetCID().ToString();
            PR_snprintf( buffer,
                         sizeof buffer,
                         "%s/%s",
                         NS_IAPPSHELLCOMPONENT_KEY,
                         cid ? cid : "unknown" );
            nsCRT::free(cid);

            nsRegistryKey key;
            rv = registry->AddSubtree( nsIRegistry::Common,
                                       buffer,
                                       &key );
            nsServiceManager::ReleaseService(NS_REGISTRY_PROGID, registry);
        }
        cp++;
        while (cp < end) {
            rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                                 cp->mProgID, aPath, PR_TRUE,
                                                 PR_TRUE);
            if (NS_FAILED(rv)) {
#ifdef DEBUG
                printf("nsSoftwareUpdateModule: unable to register %s component => %x\n",
                       cp->mDescription, rv);
#endif
                break;
            }
            cp++;
        }
    }

    return rv;
}

NS_IMETHODIMP
nsSoftwareUpdateModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                               nsIFileSpec* aPath,
                               const char* registryLocation)
{
#ifdef DEBUG
    printf("*** Unregistering SoftwareUpdate components\n");
#endif
    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        nsresult rv = aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsSoftwareUpdateModule: unable to unregister %s component => %x\n",
                   cp->mDescription, rv);
#endif
        }
        cp++;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsSoftwareUpdateModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
    if (!okToUnload) {
        return NS_ERROR_INVALID_POINTER;
    }
    *okToUnload = PR_FALSE;
    return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------

static nsSoftwareUpdateModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFileSpec* location,
                                          nsIModule** return_cobj)
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(return_cobj);
    NS_ENSURE_FALSE(gModule, NS_ERROR_FAILURE);

    // Create and initialize the module instance
    nsSoftwareUpdateModule *m = new nsSoftwareUpdateModule();
    if (!m) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // Increase refcnt and store away nsIModule interface to m in return_cobj
    rv = m->QueryInterface(NS_GET_IID(nsIModule), (void**)return_cobj);
    if (NS_FAILED(rv)) {
        delete m;
        m = nsnull;
    }
    gModule = m;                  // WARNING: Weak Reference
    return rv;
}


