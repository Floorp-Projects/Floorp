
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
#include "nsCOMPtr.h"
#include "nsCRT.h"

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

#include "nsTopProgressNotifier.h"
#include "nsLoggingProgressNotifier.h"

#include "nsIAppShellComponent.h"
#include "nsIRegistry.h"
#include "nsBuildID.h"

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
#include "nsIProxyObjectManager.h"
#include "nsProxiedService.h"
#include "nsIChromeRegistry.h"

#include "nsCURILoader.h"

extern "C" void RunChromeInstallOnThread(void *data);

////////////////////////////////////////////////////////////////////////////////
// Globals
////////////////////////////////////////////////////////////////////////////////
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

static NS_DEFINE_IID(kIScriptNameSetRegistryIID, NS_ISCRIPTNAMESETREGISTRY_IID);
static NS_DEFINE_CID(kCScriptNameSetRegistryCID, NS_SCRIPT_NAMESET_REGISTRY_CID);
static NS_DEFINE_IID(kIScriptExternalNameSetIID, NS_ISCRIPTEXTERNALNAMESET_IID);

static NS_DEFINE_IID(kISoftwareUpdate_IID, NS_ISOFTWAREUPDATE_IID);

static NS_DEFINE_IID(kIInstallTrigger_IID, NS_IDOMINSTALLTRIGGERGLOBAL_IID);
static NS_DEFINE_CID(kInstallTrigger_CID, NS_SoftwareUpdateInstallTrigger_CID);

static NS_DEFINE_IID(kIInstallVersion_IID, NS_IDOMINSTALLVERSION_IID);
static NS_DEFINE_CID(kInstallVersion_CID, NS_SoftwareUpdateInstallVersion_CID);

static NS_DEFINE_CID(kChromeRegistryCID, NS_CHROMEREGISTRY_CID);
static NS_DEFINE_CID(knsRegistryCID, NS_REGISTRY_CID);

nsSoftwareUpdate* nsSoftwareUpdate::mInstance = nsnull;
nsCOMPtr<nsIFile> nsSoftwareUpdate::mProgramDir = nsnull;
char*             nsSoftwareUpdate::mLogName = nsnull;

#if NOTIFICATION_ENABLE
#include "nsUpdateNotification.h"
static NS_DEFINE_CID(kUpdateNotificationCID, NS_XPI_UPDATE_NOTIFIER_CID);
nsIUpdateNotification*      nsSoftwareUpdate::mUpdateNotifier= nsnull;
#endif


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
  mStubLockout(PR_FALSE),
  mReg(0)
{
    NS_INIT_ISUPPORTS();
    mMasterListener = new nsTopProgressListener;
    NS_IF_ADDREF (mMasterListener);
    
    mLock = PR_NewLock();

    /***************************************/
    /* Startup the Version Registry        */
    /***************************************/

    NR_StartupRegistry();   /* startup the registry; if already started, this will essentially be a noop */
    

    nsresult rv;
    NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
    
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

    //NS_IF_RELEASE( mProgramDir );
    NS_IF_RELEASE (mMasterListener);
    mInstance = nsnull;

    PR_FREEIF(mLogName);
}


//------------------------------------------------------------------------
//  nsISupports implementation
//------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS3(nsSoftwareUpdate,
                              nsISoftwareUpdate,
                              nsIAppShellComponent,
                              nsPIXPIStubHook);

NS_IMETHODIMP
nsSoftwareUpdate::Initialize( nsIAppShellService *anAppShell, nsICmdLineService  *aCmdLineService ) 
{
    // Close the registry if open. We left it open through most of startup
    // so it wouldn't get opened and closed a lot by different services
    if (mReg) 
        NR_RegClose(mReg);

    // prevent use of nsPIXPIStubHook by browser
    mStubLockout = PR_TRUE;

    /***************************************/
    /* Add us to the Javascript Name Space */
    /***************************************/

    RegisterNameset();

    /***************************************/
    /* Register us with NetLib             */
    /***************************************/
        // FIX 


    /***************************************/
    /* Create a top level observer         */
    /***************************************/

    nsLoggingProgressListener *logger = new nsLoggingProgressListener();
    RegisterListener(logger);
    
#if NOTIFICATION_ENABLE
    /***************************************/
    /* Create a Update notification object */
    /***************************************/
    NS_IF_RELEASE(mUpdateNotifier);

    nsComponentManager::CreateInstance(kUpdateNotificationCID,
                                       nsnull,
                                       NS_GET_IID(nsIUpdateNotification),
                                       (void**)&mUpdateNotifier);

#endif
    return NS_OK;
}

NS_IMETHODIMP
nsSoftwareUpdate::Shutdown()
{
#if NOTIFICATION_ENABLED
    if (mUpdateNotifier)
    {
        mUpdateNotifier->DisplayUpdateDialog();
        NS_RELEASE(mUpdateNotifier);
    }
#endif

    // nothing to do here. Should we UnregisterService?
    return NS_OK;
}


NS_IMETHODIMP 
nsSoftwareUpdate::RegisterListener(nsIXPIListener *aListener)
{
    // we are going to ignore the returned ID and enforce that once you 
    // register a Listener, you can not remove it.  This should at some
    // point be fixed.

  if (mMasterListener)
  {
    mMasterListener->RegisterListener(aListener);
    return NS_OK;
  }
  else
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSoftwareUpdate::GetMasterListener(nsIXPIListener **aListener)
{
    NS_ASSERTION(aListener, "getter has invalid return pointer");
    if (!aListener)
        return NS_ERROR_NULL_POINTER;

    if (mMasterListener)
    {
      NS_ADDREF (mMasterListener);
      *aListener = mMasterListener;
      return NS_OK;
    }
    else
      return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsSoftwareUpdate::SetActiveListener(nsIXPIListener *aListener)
{
  if (mMasterListener)
  {
    mMasterListener->SetActiveListener (aListener);
    return NS_OK;
  }
  else
    return NS_ERROR_FAILURE;
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
    PRInt32 buildID = 0;
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


nsresult
nsSoftwareUpdate::RegisterNameset()
{
    nsresult rv;
    nsCOMPtr<nsIScriptNameSetRegistry> namesetService = 
        do_GetService( kCScriptNameSetRegistryCID, &rv );

    if (NS_SUCCEEDED(rv))
    {
        nsSoftwareUpdateNameSet* nameset = new nsSoftwareUpdateNameSet();
        // the NameSet service will AddRef this one
        namesetService->AddExternalNameSet( nameset );
    }

    return rv;
}


NS_IMETHODIMP
nsSoftwareUpdate::StubInitialize(nsIFile *aDir, const char* logName)
{
    if (mStubLockout)
        return NS_ERROR_ABORT;
    else if ( !aDir )
        return NS_ERROR_NULL_POINTER;


    // only allow once, it could be a mess if we've already started installing
    mStubLockout = PR_TRUE;

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

    // Create the logfile observer
    nsLoggingProgressListener *logger = new nsLoggingProgressListener();
    RegisterListener(logger);

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
        result = manager->RegisterGlobalName(NS_ConvertASCIItoUCS2("InstallVersion"), 
					     kIScriptObjectOwnerIID,
                                             kInstallVersion_CID, 
                                             PR_TRUE);
        
        if (NS_FAILED(result))  return result;
        
        result = manager->RegisterGlobalName(NS_ConvertASCIItoUCS2("InstallTrigger"), 
					     kIScriptObjectOwnerIID,
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

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsSoftwareUpdate,nsSoftwareUpdate::GetInstance);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsInstallTrigger);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsInstallVersion);
//----------------------------------------------------------------------



static NS_METHOD 
RegisterSoftwareUpdate( nsIComponentManager *aCompMgr,
                        nsIFile *aPath,
                        const char *registryLocation,
                        const char *componentType)
{
    // get the registry
    nsIRegistry* registry;
    nsresult rv = nsServiceManager::GetService(NS_REGISTRY_CONTRACTID,
                                               NS_GET_IID(nsIRegistry),
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
        nsServiceManager::ReleaseService(NS_REGISTRY_CONTRACTID, registry);
    }
    return rv;

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

#if NOTIFICATION_ENABLED 
    { "XPInstall Update Notifier", 
      NS_XPI_UPDATE_NOTIFIER_CID,
      NS_XPI_UPDATE_NOTIFIER_CONTRACTID, 
      nsXPINotifierImpl::New
    },
#endif

};



NS_IMPL_NSGETMODULE("nsSoftwareUpdate", components)

