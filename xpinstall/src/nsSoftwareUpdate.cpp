/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nscore.h"
#include "nsIGenericFactory.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
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
#include "nsAppDirectoryServiceDefs.h"

#include "nsInstall.h"
#include "nsSoftwareUpdateIIDs.h"
#include "nsSoftwareUpdate.h"
#include "nsSoftwareUpdateRun.h"
#include "nsInstallTrigger.h"
#include "nsInstallVersion.h"
#include "ScheduledTasks.h"
#include "InstallCleanupDefines.h"
#include "nsXPInstallManager.h"

#include "nsTopProgressNotifier.h"
#include "nsLoggingProgressNotifier.h"

#include "nsProcess.h"

/* For Javascript Namespace Access */
#include "nsDOMCID.h"
#include "nsIServiceManager.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIScriptExternalNameSet.h"

#include "nsIChromeRegistry.h"

#include "nsCURILoader.h"

extern "C" void PR_CALLBACK RunChromeInstallOnThread(void *data);

////////////////////////////////////////////////////////////////////////////////
// Globals
////////////////////////////////////////////////////////////////////////////////

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
    directoryService->Get(NS_XPCOM_CURRENT_PROCESS_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(dir));
    if (dir)
    {
        nsCAutoString nativePath;
        dir->GetNativePath(nativePath);
        // EVIL version registry does not take a nsIFile.;
        VR_SetRegDirectory( nativePath.get() );

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
                              nsIObserver)

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

        if (nsSoftwareUpdate::GetProgramDirectory()) // In the stub installer
        {
            nsCOMPtr<nsIFile> tmp;
            rv = nsSoftwareUpdate::GetProgramDirectory()->Clone(getter_AddRefs(tmp));
            pathToCleanupUtility = do_QueryInterface(tmp);
        }
        else
        {
            rv = directoryService->Get(NS_APP_INSTALL_CLEANUP_DIR,
                                      NS_GET_IID(nsIFile),
                                      getter_AddRefs(pathToCleanupUtility));
        }

        NS_ASSERTION(pathToCleanupUtility,"No path to cleanup utility in nsSoftwareUpdate::Shutdown()");

        //Create the Process framework
        pathToCleanupUtility->AppendNative(CLEANUP_UTIL);
        nsCOMPtr<nsIProcess> cleanupProcess = do_CreateInstance(NS_PROCESS_CONTRACTID);
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
                               nsIPrincipal* aPrincipal,
                               PRUint32 flags,
                               nsIXPIListener* aListener)
{
    if ( !aLocalFile )
        return NS_ERROR_NULL_POINTER;

    // we want to call this with or without a chrome registry
    nsInstallInfo *info = new nsInstallInfo( 0, aLocalFile, aURL, aArguments, aPrincipal,
                                             flags, aListener);

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
    nsInstallInfo *info = new nsInstallInfo( aType,
                                             aFile,
                                             URL,
                                             aName,
                                             nsnull,
                                             (PRUint32)aSelect,
                                             aListener);
    if (!info)
        return NS_ERROR_OUT_OF_MEMORY;

    if (!info->GetChromeRegistry() ||
#ifdef MOZ_XUL_APP
        !info->GetExtensionManager()
#else
        info->GetFileJARSpec().IsEmpty()
#endif
        ) {
      delete info;
      return NS_ERROR_FAILURE;
    }

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

    if (mJarInstallQueue.Count() != 0) // paranoia
    {
        nsInstallInfo *nextInstall = (nsInstallInfo*)mJarInstallQueue.ElementAt(0);
        if (nextInstall != nsnull)
            delete nextInstall;

        mJarInstallQueue.RemoveElementAt(0);
    }
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
    nsCAutoString tempPath;
    rv = aDir->GetNativePath(tempPath);
    if (NS_SUCCEEDED(rv))
        VR_SetRegDirectory( tempPath.get() );

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
                                         nsSoftwareUpdate::GetInstance)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsInstallTrigger)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsInstallVersion)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsXPInstallManager)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSoftwareUpdateNameSet)

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
static const nsModuleComponentInfo components[] =
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
    },

    { "XPInstallManager Component",
      NS_XPInstallManager_CID,
      NS_XPINSTALLMANAGERCOMPONENT_CONTRACTID,
      nsXPInstallManagerConstructor
    }
};



NS_IMPL_NSGETMODULE(nsSoftwareUpdate, components)

