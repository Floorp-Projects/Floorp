
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */


#include "nscore.h"
#include "nsIGenericFactory.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsICategoryManager.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsIObserverService.h"

#include "nspr.h"
#include "prlock.h"
#include "nsXPIDLString.h"
#include "NSReg.h"
#include "VerReg.h"
#include "nsIDirectoryService.h"
#include "nsDirectoryServiceDefs.h"

#include "nsInstall.h"
#include "nsSoftwareUpdateIIDs.h"
#include "nsSoftwareUpdate.h"
#include "nsSoftwareUpdateRun.h"
#include "nsInstallTrigger.h"
#include "nsInstallVersion.h"
#include "ScheduledTasks.h"
#include "InstallCleanupDefines.h"

#include "nsTopProgressNotifier.h"
#include "nsLoggingProgressNotifier.h"

#include "nsIRegistry.h"
#include "nsBuildID.h"
#include "nsSpecialSystemDirectory.h"
#include "nsProcess.h"

/* For Javascript Namespace Access */
#include "nsDOMCID.h"
#include "nsIServiceManager.h"
#include "nsINameSpaceManager.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIScriptExternalNameSet.h"

#include "nsIEventQueueService.h"
#include "nsIProxyObjectManager.h"
#include "nsProxiedService.h"
#include "nsIChromeRegistry.h"

#include "nsCURILoader.h"

extern "C" void RunChromeInstallOnThread(void *data);

////////////////////////////////////////////////////////////////////////////////
// Globals
////////////////////////////////////////////////////////////////////////////////
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

static NS_DEFINE_CID(kCScriptNameSetRegistryCID, NS_SCRIPT_NAMESET_REGISTRY_CID);
static NS_DEFINE_CID(kInstallTrigger_CID, NS_SoftwareUpdateInstallTrigger_CID);

static NS_DEFINE_CID(kInstallVersion_CID, NS_SoftwareUpdateInstallVersion_CID);

static NS_DEFINE_CID(kChromeRegistryCID, NS_CHROMEREGISTRY_CID);
static NS_DEFINE_CID(knsRegistryCID, NS_REGISTRY_CID);

static NS_DEFINE_CID(kIProcessCID, NS_PROCESS_CID); 

nsSoftwareUpdate* nsSoftwareUpdate::mInstance = nsnull;
nsCOMPtr<nsIFile> nsSoftwareUpdate::mProgramDir = nsnull;
char*             nsSoftwareUpdate::mLogName = nsnull;
PRBool            nsSoftwareUpdate::mNeedCleanup = PR_FALSE;


nsSoftwareUpdate *
nsSoftwareUpdate::GetInstance()
{
    if (mInstance == nsnull) 
        mInstance = new nsSoftwareUpdate();

    NS_IF_ADDREF(mInstance);
    return mInstance;
}



nsSoftwareUpdate::nsSoftwareUpdate()
: mInstalling(PR_FALSE),
  mMasterListener(0),
  mReg(0)
{
    NS_INIT_ISUPPORTS();
    
    mLock = PR_NewLock();

    /***************************************/
    /* Startup the Version Registry        */
    /***************************************/

    NR_StartupRegistry();   /* startup the registry; if already started, this will essentially be a noop */
    

    nsresult rv;
    nsCOMPtr<nsIProperties> directoryService = 
             do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
    
    if(!directoryService) return;
    
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
    /***************************************/
    /* Add this as a shutdown observer     */
    /***************************************/
    nsCOMPtr<nsIObserverService> observerService = 
             do_GetService("@mozilla.org/observer-service;1", &rv);

    if (NS_SUCCEEDED(rv))
        observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
}


nsSoftwareUpdate::~nsSoftwareUpdate()
{
    PR_Lock(mLock);

    nsInstallInfo* element;
    for (PRInt32 i=0; i < mJarInstallQueue.Count(); i++)
    {
        element = (nsInstallInfo*)mJarInstallQueue.ElementAt(i);
        //FIX:  need to add to registry....
        delete element;
    }

    mJarInstallQueue.Clear();

    PR_Unlock(mLock);
    PR_DestroyLock(mLock);

    NR_ShutdownRegistry();

    NS_IF_RELEASE (mMasterListener);
    mInstance = nsnull;

    PR_FREEIF(mLogName);
}


//------------------------------------------------------------------------
//  nsISupports implementation
//------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS3(nsSoftwareUpdate,
                              nsISoftwareUpdate,
                              nsPIXPIStubHook,
                              nsIObserver);

void
nsSoftwareUpdate::Shutdown()
{
    if (mNeedCleanup)
    {
      // Create a non-blocking process to run the native platform cleanup utility
        nsresult rv;
        nsCOMPtr<nsILocalFile> pathToCleanupUtility;
        //Get the program directory
        nsCOMPtr<nsIProperties> directoryService = 
                 do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
        directoryService->Get(NS_OS_CURRENT_PROCESS_DIR, 
                              NS_GET_IID(nsIFile), 
                              getter_AddRefs(pathToCleanupUtility));
#if defined (XP_MAC)
        pathToCleanupUtility->Append(ESSENTIAL_FILES);
#endif
        //Create the Process framework
        pathToCleanupUtility->Append(CLEANUP_UTIL);
        nsCOMPtr<nsIProcess> cleanupProcess = do_CreateInstance(kIProcessCID);
        rv = cleanupProcess->Init(pathToCleanupUtility);
        if (NS_SUCCEEDED(rv))
        {
            //Run the cleanup utility as a NON-blocking process
            rv = cleanupProcess->Run(PR_FALSE, nsnull, 0, nsnull);
        }
    }
}

NS_IMETHODIMP nsSoftwareUpdate::Observe(nsISupports *aSubject, 
                                        const char *aTopic, 
                                        const PRUnichar *aData)
{
    if (!nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID))
      Shutdown();
     
    return NS_OK;
}

NS_IMETHODIMP 
nsSoftwareUpdate::RegisterListener(nsIXPIListener *aListener)
{
    // once you register a Listener, you can not remove it.
    // This should get changed at some point.

    if (!mMasterListener)
        CreateMasterListener();

    if (!mMasterListener)
        return NS_ERROR_FAILURE;
    
    mMasterListener->RegisterListener(aListener);
    return NS_OK;
}

NS_IMETHODIMP
nsSoftwareUpdate::GetMasterListener(nsIXPIListener **aListener)
{
    NS_ASSERTION(aListener, "getter has invalid return pointer");
    if (!aListener)
        return NS_ERROR_NULL_POINTER;

    if (!mMasterListener)
        CreateMasterListener();

    if (!mMasterListener)
        return NS_ERROR_FAILURE;

    NS_ADDREF (mMasterListener);
    *aListener = mMasterListener;
    return NS_OK;
}


NS_IMETHODIMP
nsSoftwareUpdate::SetActiveListener(nsIXPIListener *aListener)
{
    if (!mMasterListener)
        CreateMasterListener();

    if (!mMasterListener)
        return NS_ERROR_FAILURE;

    mMasterListener->SetActiveListener (aListener);
    return NS_OK;
}

void nsSoftwareUpdate::CreateMasterListener()
{
    mMasterListener = new nsTopProgressListener;
    if (mMasterListener)
    {
        NS_ADDREF(mMasterListener);

        nsLoggingProgressListener *logger = new nsLoggingProgressListener();
        mMasterListener->RegisterListener(logger);
    }
}

NS_IMETHODIMP
nsSoftwareUpdate::InstallJar(  nsIFile* aLocalFile,
                               const PRUnichar* aURL,
                               const PRUnichar* aArguments,
                               nsIDOMWindowInternal* aParentWindow,
                               PRUint32 flags,
                               nsIXPIListener* aListener)
{
    if ( !aLocalFile )
        return NS_ERROR_NULL_POINTER;

    // -- grab a proxied Chrome Registry now while we can
    nsresult rv;
    nsIChromeRegistry* chromeReg = nsnull;
    NS_WITH_ALWAYS_PROXIED_SERVICE( nsIChromeRegistry, 
                                    tmpReg,
                                    kChromeRegistryCID, 
                                    NS_UI_THREAD_EVENTQ, &rv);
    if (NS_SUCCEEDED(rv))
        chromeReg = tmpReg;

    // we want to call this with or without a chrome registry
    nsInstallInfo *info = new nsInstallInfo( 0, aLocalFile, aURL, aArguments,
                                             flags, aListener, aParentWindow, chromeReg );
    
    if (!info)
        return NS_ERROR_OUT_OF_MEMORY;

    PR_Lock(mLock);
    mJarInstallQueue.AppendElement( info );
    PR_Unlock(mLock);
    RunNextInstall();

    return NS_OK;
}


NS_IMETHODIMP
nsSoftwareUpdate::InstallChrome( PRUint32 aType,
                                 nsIFile* aFile,
                                 const PRUnichar* URL,
                                 const PRUnichar* aName,
                                 PRBool aSelect,
                                 nsIXPIListener* aListener)
{
    nsresult rv;
    NS_WITH_ALWAYS_PROXIED_SERVICE( nsIChromeRegistry,
                                    chromeReg,
                                    kChromeRegistryCID, 
                                    NS_UI_THREAD_EVENTQ, &rv);
    if (NS_FAILED(rv))
        return rv;

    nsInstallInfo *info = new nsInstallInfo( aType,
                                             aFile,
                                             URL,
                                             aName,
                                             (PRUint32)aSelect,
                                             aListener,
                                             nsnull, 
                                             chromeReg);
    if (!info)
        return NS_ERROR_OUT_OF_MEMORY;

    PR_CreateThread(PR_USER_THREAD,
                    RunChromeInstallOnThread,
                    (void*)info,
                    PR_PRIORITY_NORMAL,
                    PR_GLOBAL_THREAD,
                    PR_UNJOINABLE_THREAD,
                    0);

    return NS_OK;
}


NS_IMETHODIMP
nsSoftwareUpdate::InstallJarCallBack()
{
    PR_Lock(mLock);

    nsInstallInfo *nextInstall = (nsInstallInfo*)mJarInstallQueue.ElementAt(0);
    if (nextInstall != nsnull)
        delete nextInstall;

    mJarInstallQueue.RemoveElementAt(0);
    mInstalling = PR_FALSE;

    PR_Unlock(mLock);

    return RunNextInstall();
}


NS_IMETHODIMP
nsSoftwareUpdate::StartupTasks( PRBool *needAutoreg )
{
    PRBool  autoReg = PR_FALSE;
    RKEY    xpiRoot;
    REGERR  err;

    *needAutoreg = PR_TRUE;

    // First do any left-over file replacements and deletes

    // NOTE: we leave the registry open until later to prevent
    // having to load and unload it many times at startup

    if ( REGERR_OK == NR_RegOpen("", &mReg) )
    {
        // XXX get a return val and if not all replaced autoreg again later
        PerformScheduledTasks(mReg);

        // now look for an autoreg flag left behind by XPInstall
        err = NR_RegGetKey( mReg, ROOTKEY_COMMON, XPI_ROOT_KEY, &xpiRoot);
        if ( err == REGERR_OK )
        {
            char buf[8];
            err = NR_RegGetEntryString( mReg, xpiRoot, XPI_AUTOREG_VAL,
                                        buf, sizeof(buf) );

            if ( err == REGERR_OK && !strcmp( buf, "yes" ) )
                autoReg = PR_TRUE;
        }
    }

    // Also check for build number changes
    nsresult rv;
    PRInt32 buildID = -1;
    nsRegistryKey idKey = 0;
    nsCOMPtr<nsIRegistry> reg = do_GetService(knsRegistryCID,&rv);
    if (NS_SUCCEEDED(rv))
    {
        rv = reg->OpenWellKnownRegistry(nsIRegistry::ApplicationComponentRegistry);
        if (NS_SUCCEEDED(rv))
        {
            rv = reg->GetSubtree(nsIRegistry::Common,XPCOM_KEY,&idKey);
            if (NS_SUCCEEDED(rv))
            {
                rv = reg->GetInt( idKey, XPI_AUTOREG_VAL, &buildID );
            }
        }
    }

    // Autoregister if we found the XPInstall flag, the stored BuildID
    // is not the actual BuildID, or if we couldn't get the BuildID
    if ( autoReg || NS_FAILED(rv) || buildID != NS_BUILD_ID )
    {
        rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup,0);

        if (NS_SUCCEEDED(rv))
        {
            *needAutoreg = PR_FALSE;

            // Now store back into the registries so we don't do this again
            if ( autoReg )
                NR_RegSetEntryString( mReg, xpiRoot, XPI_AUTOREG_VAL, "no" );

            if ( buildID != NS_BUILD_ID && idKey != 0 )
                reg->SetInt( idKey, XPI_AUTOREG_VAL, NS_BUILD_ID );
        }
    }
    else
    {
        //We don't need to autoreg, we're up to date
        *needAutoreg = PR_FALSE;
#ifdef DEBUG
        // debug (developer) builds should always autoreg
        *needAutoreg = PR_TRUE;
#endif
    }

    return rv;
}


nsresult
nsSoftwareUpdate::RunNextInstall()
{
    nsresult        rv = NS_OK;
    nsInstallInfo*  info = nsnull;

    PR_Lock(mLock);

    // make sure master master listener exists
    if (!mMasterListener)
        CreateMasterListener();

    if (!mInstalling) 
    {
        if ( mJarInstallQueue.Count() > 0 )
        {
            info = (nsInstallInfo*)mJarInstallQueue.ElementAt(0);

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
nsSoftwareUpdate::StubInitialize(nsIFile *aDir, const char* logName)
{
    if ( !aDir )
        return NS_ERROR_NULL_POINTER;

    // fix GetFolder return path
    nsresult rv = aDir->Clone(getter_AddRefs(mProgramDir));

    // make sure registry updates go to the right place
    nsXPIDLCString tempPath;
    rv = aDir->GetPath(getter_Copies(tempPath));
    if (NS_SUCCEEDED(rv))
        VR_SetRegDirectory( tempPath );

    // Optionally set logfile leafname
    if (logName)
    {
        mLogName = PL_strdup(logName);
        if (!mLogName)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    return rv;
}


////////////////////////////////////////////////////////////////////////////////
// nsSoftwareUpdateNameSet
////////////////////////////////////////////////////////////////////////////////

nsSoftwareUpdateNameSet::nsSoftwareUpdateNameSet()
{
    NS_INIT_ISUPPORTS();
}

nsSoftwareUpdateNameSet::~nsSoftwareUpdateNameSet()
{
}

NS_IMPL_ISUPPORTS1(nsSoftwareUpdateNameSet, nsIScriptExternalNameSet)


NS_IMETHODIMP
nsSoftwareUpdateNameSet::InitializeNameSet(nsIScriptContext* aScriptContext)
{
    nsresult result = NS_OK;

    result = NS_InitInstallVersionClass(aScriptContext, nsnull);
    if (NS_FAILED(result)) return result;

    result = NS_InitInstallTriggerGlobalClass(aScriptContext, nsnull);

    return result;
}


//----------------------------------------------------------------------

// Functions used to create new instances of a given object by the
// generic factory.

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsSoftwareUpdate,
                                         nsSoftwareUpdate::GetInstance);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsInstallTrigger);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsInstallVersion);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSoftwareUpdateNameSet);

//----------------------------------------------------------------------

#define NS_SOFTWAREUPDATENAMESET_CID \
  { 0xcde48010, 0x9494, 0x4a73, \
  { 0x96, 0x9a, 0x26, 0x33, 0x50, 0x0, 0x70, 0xde }}

#define NS_SOFTWAREUPDATENAMESET_CONTRACTID \
  "@mozilla.org/xpinstall/softwareupdatenameset;1"

static NS_METHOD 
RegisterSoftwareUpdate( nsIComponentManager *aCompMgr,
                        nsIFile *aPath,
                        const char *registryLocation,
                        const char *componentType,
                        const nsModuleComponentInfo *info)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsICategoryManager> catman =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsXPIDLCString previous;
  rv = catman->AddCategoryEntry(JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY,
                                "InstallVersion",
                                NS_INSTALLVERSIONCOMPONENT_CONTRACTID,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = catman->AddCategoryEntry(JAVASCRIPT_GLOBAL_PROPERTY_CATEGORY,
                                "InstallTrigger",
                                NS_INSTALLTRIGGERCOMPONENT_CONTRACTID,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


// The list of components we register
static nsModuleComponentInfo components[] = 
{
    { "SoftwareUpdate Component", 
       NS_SoftwareUpdate_CID,
       NS_IXPINSTALLCOMPONENT_CONTRACTID,
       nsSoftwareUpdateConstructor,
       RegisterSoftwareUpdate
    },
   
    { "InstallTrigger Component", 
       NS_SoftwareUpdateInstallTrigger_CID,
       NS_INSTALLTRIGGERCOMPONENT_CONTRACTID, 
       nsInstallTriggerConstructor
    },
    
    { "InstallVersion Component", 
       NS_SoftwareUpdateInstallVersion_CID,
       NS_INSTALLVERSIONCOMPONENT_CONTRACTID,
       nsInstallVersionConstructor 
    },

    { "XPInstall Content Handler",
      NS_SoftwareUpdateInstallTrigger_CID,
      NS_CONTENT_HANDLER_CONTRACTID_PREFIX"application/x-xpinstall",
      nsInstallTriggerConstructor 
    },

    { "Software update nameset",
      NS_SOFTWAREUPDATENAMESET_CID,
      NS_SOFTWAREUPDATENAMESET_CONTRACTID,
      nsSoftwareUpdateNameSetConstructor 
    }
};



NS_IMPL_NSGETMODULE(nsSoftwareUpdate, components)

