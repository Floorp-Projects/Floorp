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
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

#include "nspr.h"
#include "prlock.h"
#include "nsVector.h"
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
    if (mInstance == NULL) 
    {
        mInstance = new nsSoftwareUpdate();
    }
    return mInstance;
}



nsSoftwareUpdate::nsSoftwareUpdate()
{
#ifdef NS_DEBUG
    printf("XPInstall Component created\n");
#endif

    NS_INIT_ISUPPORTS();

    mStubLockout = PR_FALSE;
     /***************************************/
    /* Create us a queue                   */
    /***************************************/
    mLock = PR_NewLock();
    mInstalling = PR_FALSE;
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
#ifdef NS_DEBUG
    printf("*** XPInstall Component destroyed\n");
#endif

    PR_Lock(mLock);
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
    PR_Unlock(mLock);
    PR_DestroyLock(mLock);

    NR_ShutdownRegistry();

    if (mProgramDir)
        mProgramDir->Release();
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
        else if (anIID.Equals( nsPvtIXPIStubHook::GetIID() ) )
            *anInstancePtr = (void*) ( (nsPvtIXPIStubHook*)this );
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
    nsresult rv;

    mStubLockout = PR_TRUE;  // prevent use of nsPvtIXPIStubHook by browser

    rv = nsServiceManager::RegisterService( NS_IXPINSTALLCOMPONENT_PROGID, ( (nsISupports*) (nsISoftwareUpdate*) this ) );

    return rv;
}

NS_IMETHODIMP
nsSoftwareUpdate::Shutdown()
{
    nsresult rv;

    rv = nsServiceManager::UnregisterService( NS_IXPINSTALLCOMPONENT_PROGID );

    return rv;
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
    mJarInstallQueue->Add( info );
    PR_Unlock(mLock);
    RunNextInstall();

    return NS_OK;
}


NS_IMETHODIMP
nsSoftwareUpdate::InstallJarCallBack()
{
    PR_Lock(mLock);

    nsInstallInfo *nextInstall = (nsInstallInfo*)mJarInstallQueue->Get(0);
    if (nextInstall != nsnull)
        delete nextInstall;

    mJarInstallQueue->Remove(0);
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
        if ( mJarInstallQueue->GetSize() > 0 )
        {
            info = (nsInstallInfo*)mJarInstallQueue->Get(0);

            if ( info )
                mInstalling = PR_TRUE;
            else 
            {
                NS_ERROR("leaking all nsInstallInfos left in queue");
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

    // fix GetFolder return path
    mProgramDir = aDir;
    mProgramDir->AddRef();

    // setup version registry path
    char*    path;
    nsresult rv = aDir->GetNativePath( &path );
    if (NS_SUCCEEDED(rv))
    {
        VR_SetRegDirectory( path );
    }

    return rv;
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
NSCanUnload(nsISupports* aServMgr)
{
    return PR_FALSE;
}

extern "C" NS_EXPORT nsresult
NSRegisterSelf(nsISupports* aServMgr, const char *path)
{
    nsresult rv;

    nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
    if (NS_FAILED(rv)) return rv;

    nsIComponentManager* compMgr;
    rv = servMgr->GetService(kComponentManagerCID, 
                             nsIComponentManager::GetIID(), 
                             (nsISupports**)&compMgr);
    if (NS_FAILED(rv)) return rv;

#ifdef NS_DEBUG
    printf("*** XPInstall is being registered\n");
#endif

    rv = compMgr->RegisterComponent( kSoftwareUpdate_CID,
                                     NS_IXPINSTALLCOMPONENT_CLASSNAME,
                                     NS_IXPINSTALLCOMPONENT_PROGID,
                                     path,
                                     PR_TRUE,
                                     PR_TRUE );

    if (NS_FAILED(rv)) goto done;
    
    nsIRegistry *registry; 
    rv = servMgr->GetService( NS_REGISTRY_PROGID, 
                              nsIRegistry::GetIID(), 
                              (nsISupports**)&registry );

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
        delete [] cid;

        nsIRegistry::Key key;
        rv = registry->AddSubtree( nsIRegistry::Common,
                                   buffer,
                                   &key );
        servMgr->ReleaseService( NS_REGISTRY_PROGID, registry );
    }

    rv = compMgr->RegisterComponent(kInstallTrigger_CID, NULL, NULL, path, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) goto done;
    
    rv = compMgr->RegisterComponent(kInstallVersion_CID, NULL, NULL, path, PR_TRUE, PR_TRUE);
  
done:
    (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
    return rv;
}


extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports* aServMgr, const char *path)
{
nsresult rv;

    nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
    if (NS_FAILED(rv)) return rv;

    nsIComponentManager* compMgr;
    rv = servMgr->GetService(kComponentManagerCID, 
                             nsIComponentManager::GetIID(), 
                             (nsISupports**)&compMgr);
    if (NS_FAILED(rv)) return rv;

#ifdef NS_DEBUG
    printf("*** XPInstall is being unregistered\n");
#endif
    
    rv = compMgr->UnregisterComponent(kSoftwareUpdate_CID, path);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kInstallTrigger_CID, path);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kInstallVersion_CID, path);

  done:
    (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
    return rv;
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


