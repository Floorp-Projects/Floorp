
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
#include "NSReg.h"
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
#include "nsProxyObjectManager.h"


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

static NS_DEFINE_CID(knsRegistryCID, NS_REGISTRY_CID);

nsSoftwareUpdate* nsSoftwareUpdate::mInstance = nsnull;
nsIFileSpec*      nsSoftwareUpdate::mProgramDir = nsnull;

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

    mLock = PR_NewLock();

    /***************************************/
    /* Startup the Version Registry        */
    /***************************************/

    NR_StartupRegistry();   /* startup the registry; if already started, this will essentially be a noop */

    nsSpecialSystemDirectory appDir(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
    VR_SetRegDirectory( appDir.GetNativePathCString() );
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

    NS_IF_RELEASE( mProgramDir );

    mInstance = nsnull;
}


//------------------------------------------------------------------------
//  nsISupports implementation
//------------------------------------------------------------------------
NS_IMPL_THREADSAFE_ADDREF( nsSoftwareUpdate );
NS_IMPL_THREADSAFE_RELEASE( nsSoftwareUpdate );


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
        if ( anIID.Equals( NS_GET_IID(nsISoftwareUpdate) ) ) 
            *anInstancePtr = (void*) ( (nsISoftwareUpdate*)this );
        else if ( anIID.Equals( NS_GET_IID(nsIAppShellComponent) ) ) 
            *anInstancePtr = (void*) ( (nsIAppShellComponent*)this );
        else if (anIID.Equals( NS_GET_IID(nsPIXPIStubHook) ) )
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

    nsLoggingProgressNotifier *logger = new nsLoggingProgressNotifier();
    RegisterNotifier(logger);
    
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
    mJarInstallQueue.AppendElement( info );
    PR_Unlock(mLock);
    RunNextInstall();

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
nsSoftwareUpdate::StubInitialize(nsIFileSpec *aDir)
{
    if (mStubLockout)
        return NS_ERROR_ABORT;
    else if ( !aDir )
        return NS_ERROR_NULL_POINTER;

    // only allow once, it could be a mess if we've already started installing
    mStubLockout = PR_TRUE;

    // fix GetFolder return path
    mProgramDir = aDir;
    NS_ADDREF(mProgramDir);

    // make sure registry updates go to the right place
    nsFileSpec instDir;
    if (NS_SUCCEEDED( aDir->GetFileSpec( &instDir ) ) )
        VR_SetRegDirectory( instDir.GetNativePathCString() );

    // Create the logfile observer
    nsLoggingProgressNotifier *logger = new nsLoggingProgressNotifier();
    RegisterNotifier(logger);

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
    nsresult rv = nsServiceManager::GetService(NS_REGISTRY_PROGID,
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
        nsServiceManager::ReleaseService(NS_REGISTRY_PROGID, registry);
    }
    return rv;

}


// The list of components we register
static nsModuleComponentInfo components[] = 
{
    { "SoftwareUpdate Component", 
       NS_SoftwareUpdate_CID,
       NS_IXPINSTALLCOMPONENT_PROGID,
       nsSoftwareUpdateConstructor,
       RegisterSoftwareUpdate
    },
	   
    { "InstallTrigger Component", 
       NS_SoftwareUpdateInstallTrigger_CID,
       NS_INSTALLTRIGGERCOMPONENT_PROGID, 
       nsInstallTriggerConstructor
    },
    
    { "InstallVersion Component", 
       NS_SoftwareUpdateInstallVersion_CID,
       NS_INSTALLVERSIONCOMPONENT_PROGID,
       nsInstallVersionConstructor 
    },

#if NOTIFICATION_ENABLED 
    { "XPInstall Update Notifier", 
      NS_XPI_UPDATE_NOTIFIER_CID,
      NS_XPI_UPDATE_NOTIFIER_PROGID, 
      nsXPINotifierImpl::New
    },
#endif

};



NS_IMPL_NSGETMODULE("nsSoftwareUpdate", components)

