/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
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

#include "nsXPCOM.h"
#include "nsXPCOMPrivate.h"
#include "nscore.h"
#include "prlink.h"
#include "nsCOMPtr.h"
#include "nsObserverList.h"
#include "nsObserverService.h"
#include "nsProperties.h"
#include "nsIProperties.h"
#include "nsPersistentProperties.h"
#include "nsScriptableInputStream.h"
#include "nsBinaryStream.h"
#include "nsStorageStream.h"

#include "nsMemoryImpl.h"
#include "nsDebugImpl.h"
#include "nsTraceRefcntImpl.h"
#include "nsErrorService.h"
#include "nsByteBuffer.h"

#include "nsSupportsArray.h"
#include "nsArray.h"
#include "nsSupportsPrimitives.h"
#include "nsConsoleService.h"
#include "nsExceptionService.h"

#include "nsComponentManager.h"
#include "nsCategoryManagerUtils.h"
#include "nsIServiceManager.h"
#include "nsGenericFactory.h"

#include "nsEventQueueService.h"
#include "nsEventQueue.h"

#include "nsIProxyObjectManager.h"
#include "nsProxyEventPrivate.h"  // access to the impl of nsProxyObjectManager for the generic factory registration.

#include "xptinfo.h"
#include "nsIInterfaceInfoManager.h"

#include "nsTimerImpl.h"
#include "TimerThread.h"

#include "nsThread.h"
#include "nsProcess.h"
#include "nsEnvironment.h"

#include "nsEmptyEnumerator.h"

#include "nsILocalFile.h"
#include "nsLocalFile.h"
#if defined(XP_UNIX) || defined(XP_OS2)
#include "nsNativeCharsetUtils.h"
#endif
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsCategoryManager.h"
#include "nsICategoryManager.h"
#include "nsStringStream.h"
#include "nsMultiplexInputStream.h"

#include "nsFastLoadService.h"

#include "nsAtomService.h"
#include "nsAtomTable.h"
#include "nsTraceRefcnt.h"
#include "nsTimelineService.h"

#include "nsVariant.h"

#ifdef GC_LEAK_DETECTOR
#include "nsLeakDetector.h"
#endif
#include "nsRecyclingAllocator.h"

#include "SpecialSystemDirectory.h"

#include <locale.h>

// Registry Factory creation function defined in nsRegistry.cpp
// We hook into this function locally to create and register the registry
// Since noone outside xpcom needs to know about this and nsRegistry.cpp
// does not have a local include file, we are putting this definition
// here rather than in nsIRegistry.h
extern nsresult NS_RegistryGetFactory(nsIFactory** aFactory);
extern nsresult NS_CategoryManagerGetFactory( nsIFactory** );

#ifdef DEBUG
extern void _FreeAutoLockStatics();
#endif

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kMemoryCID, NS_MEMORY_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

NS_GENERIC_FACTORY_CONSTRUCTOR(nsProcess)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsEventQueueServiceImpl, Init)

#define NS_ENVIRONMENT_CLASSNAME "Environment Service"

#include "nsXPCOM.h"
// ds/nsISupportsPrimitives
#define NS_SUPPORTS_ID_CLASSNAME "Supports ID"
#define NS_SUPPORTS_CSTRING_CLASSNAME "Supports String"
#define NS_SUPPORTS_STRING_CLASSNAME "Supports WString"
#define NS_SUPPORTS_PRBOOL_CLASSNAME "Supports PRBool"
#define NS_SUPPORTS_PRUINT8_CLASSNAME "Supports PRUint8"
#define NS_SUPPORTS_PRUINT16_CLASSNAME "Supports PRUint16"
#define NS_SUPPORTS_PRUINT32_CLASSNAME "Supports PRUint32"
#define NS_SUPPORTS_PRUINT64_CLASSNAME "Supports PRUint64"
#define NS_SUPPORTS_PRTIME_CLASSNAME "Supports PRTime"
#define NS_SUPPORTS_CHAR_CLASSNAME "Supports Char"
#define NS_SUPPORTS_PRINT16_CLASSNAME "Supports PRInt16"
#define NS_SUPPORTS_PRINT32_CLASSNAME "Supports PRInt32"
#define NS_SUPPORTS_PRINT64_CLASSNAME "Supports PRInt64"
#define NS_SUPPORTS_FLOAT_CLASSNAME "Supports float"
#define NS_SUPPORTS_DOUBLE_CLASSNAME "Supports double"
#define NS_SUPPORTS_VOID_CLASSNAME "Supports void"
#define NS_SUPPORTS_INTERFACE_POINTER_CLASSNAME "Supports interface pointer"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsIDImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsStringImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsCStringImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRBoolImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRUint8Impl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRUint16Impl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRUint32Impl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRUint64Impl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRTimeImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsCharImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRInt16Impl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRInt32Impl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRInt64Impl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsFloatImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsDoubleImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsVoidImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsInterfacePointerImpl)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsArray)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsConsoleService)
NS_DECL_CLASSINFO(nsConsoleService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAtomService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsExceptionService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTimerImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTimerManager)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBinaryOutputStream)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBinaryInputStream)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsStorageStream)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsVariant)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsRecyclingAllocatorImpl)

#ifdef MOZ_TIMELINE
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTimelineService)
#endif

static NS_METHOD
nsXPTIInterfaceInfoManagerGetSingleton(nsISupports* outer,
                                       const nsIID& aIID,
                                       void* *aInstancePtr)
{
    NS_ENSURE_ARG_POINTER(aInstancePtr);
    NS_ENSURE_TRUE(!outer, NS_ERROR_NO_AGGREGATION);

    nsCOMPtr<nsIInterfaceInfoManager> iim(dont_AddRef(XPTI_GetInterfaceInfoManager()));
    if (!iim) {
        return NS_ERROR_FAILURE;
    }

    return iim->QueryInterface(aIID, aInstancePtr);
}


PR_STATIC_CALLBACK(nsresult)
RegisterGenericFactory(nsIComponentRegistrar* registrar,
                       const nsModuleComponentInfo *info)
{
    nsresult rv;
    nsIGenericFactory* fact;
    rv = NS_NewGenericFactory(&fact, info);
    if (NS_FAILED(rv)) return rv;

    rv = registrar->RegisterFactory(info->mCID, 
                                    info->mDescription,
                                    info->mContractID, 
                                    fact);
    NS_RELEASE(fact);
    return rv;
}

// In order to support the installer, we need
// to be told out of band if we should cause
// an autoregister.  If the file ".autoreg" exists in the binary
// directory, we check its timestamp against the timestamp of the
// compreg.dat file.  If the .autoreg file is newer, we autoregister.
static PRBool CheckUpdateFile()
{
    nsresult rv;
    nsCOMPtr<nsIProperties> directoryService;
    nsDirectoryService::Create(nsnull, 
                               NS_GET_IID(nsIProperties), 
                               getter_AddRefs(directoryService));  
    
    if (!directoryService) 
        return PR_FALSE;

    nsCOMPtr<nsIFile> file;
    rv = directoryService->Get(NS_XPCOM_CURRENT_PROCESS_DIR, 
                               NS_GET_IID(nsIFile), 
                               getter_AddRefs(file));

    if (NS_FAILED(rv)) {
        NS_WARNING("Getting NS_XPCOM_CURRENT_PROCESS_DIR failed");
        return PR_FALSE;
    }

    file->AppendNative(nsDependentCString(".autoreg"));
    
    PRBool exists;
    file->Exists(&exists);
    if (!exists)
        return PR_FALSE;

    nsCOMPtr<nsIFile> compregFile;
    rv = directoryService->Get(NS_XPCOM_COMPONENT_REGISTRY_FILE,
                               NS_GET_IID(nsIFile),
                               getter_AddRefs(compregFile));

    
    if (NS_FAILED(rv)) {
        NS_WARNING("Getting NS_XPCOM_COMPONENT_REGISTRY_FILE failed");
        return PR_FALSE;
    }

    // Don't need to check whether compreg exists; if it doesn't
    // we won't even be here.

    PRInt64 compregModTime, autoregModTime;
    compregFile->GetLastModifiedTime(&compregModTime);
    file->GetLastModifiedTime(&autoregModTime);

    return LL_CMP(autoregModTime, >, compregModTime);
}


nsComponentManagerImpl* nsComponentManagerImpl::gComponentManager = NULL;
nsIProperties     *gDirectoryService = NULL;
PRBool gXPCOMShuttingDown = PR_FALSE;

// For each class that wishes to support nsIClassInfo, add a line like this
// NS_DECL_CLASSINFO(nsMyClass)

#define COMPONENT(NAME, Ctor)                                                  \
 { NS_##NAME##_CLASSNAME, NS_##NAME##_CID, NS_##NAME##_CONTRACTID, Ctor }

#define COMPONENT_CI(NAME, Ctor, Class)                                        \
 { NS_##NAME##_CLASSNAME, NS_##NAME##_CID, NS_##NAME##_CONTRACTID, Ctor,       \
   NULL, NULL, NULL, NS_CI_INTERFACE_GETTER_NAME(Class), NULL,                 \
   &NS_CLASSINFO_NAME(Class) }

#define COMPONENT_CI_FLAGS(NAME, Ctor, Class, Flags)                           \
 { NS_##NAME##_CLASSNAME, NS_##NAME##_CID, NS_##NAME##_CONTRACTID, Ctor,       \
   NULL, NULL, NULL, NS_CI_INTERFACE_GETTER_NAME(Class), NULL,                 \
   &NS_CLASSINFO_NAME(Class), Flags }

static const nsModuleComponentInfo components[] = {
    COMPONENT(MEMORY, nsMemoryImpl::Create),
    COMPONENT(DEBUG,  nsDebugImpl::Create),
#define NS_ERRORSERVICE_CLASSNAME NS_ERRORSERVICE_NAME
    COMPONENT(ERRORSERVICE, nsErrorService::Create),

    COMPONENT(BYTEBUFFER, ByteBufferImpl::Create),
    COMPONENT(SCRIPTABLEINPUTSTREAM, nsScriptableInputStream::Create),
    COMPONENT(BINARYINPUTSTREAM, nsBinaryInputStreamConstructor),
    COMPONENT(BINARYOUTPUTSTREAM, nsBinaryOutputStreamConstructor),
    COMPONENT(STORAGESTREAM, nsStorageStreamConstructor),

#define NS_PROPERTIES_CLASSNAME  "Properties"
    COMPONENT(PROPERTIES, nsProperties::Create),

#define NS_PERSISTENTPROPERTIES_CID NS_IPERSISTENTPROPERTIES_CID /* sigh */
    COMPONENT(PERSISTENTPROPERTIES, nsPersistentProperties::Create),

    COMPONENT(SUPPORTSARRAY, nsSupportsArray::Create),
    COMPONENT(ARRAY, nsArrayConstructor),
    COMPONENT_CI_FLAGS(CONSOLESERVICE, nsConsoleServiceConstructor,
                       nsConsoleService,
                       nsIClassInfo::THREADSAFE | nsIClassInfo::SINGLETON),
    COMPONENT(EXCEPTIONSERVICE, nsExceptionServiceConstructor),
    COMPONENT(ATOMSERVICE, nsAtomServiceConstructor),
#ifdef MOZ_TIMELINE
    COMPONENT(TIMELINESERVICE, nsTimelineServiceConstructor),
#endif
    COMPONENT(OBSERVERSERVICE, nsObserverService::Create),
    COMPONENT(GENERICFACTORY, nsGenericFactory::Create),
    COMPONENT(EVENTQUEUESERVICE, nsEventQueueServiceImplConstructor),
    COMPONENT(EVENTQUEUE, nsEventQueueImpl::Create),
    COMPONENT(THREAD, nsThread::Create),

#define NS_XPCOMPROXY_CID NS_PROXYEVENT_MANAGER_CID
    COMPONENT(XPCOMPROXY, nsProxyObjectManager::Create),

    COMPONENT(TIMER, nsTimerImplConstructor),
    COMPONENT(TIMERMANAGER, nsTimerManagerConstructor),

#define COMPONENT_SUPPORTS(TYPE, Type)                                         \
  COMPONENT(SUPPORTS_##TYPE, nsSupports##Type##ImplConstructor)

    COMPONENT_SUPPORTS(ID, ID),
    COMPONENT_SUPPORTS(STRING, String),
    COMPONENT_SUPPORTS(CSTRING, CString),
    COMPONENT_SUPPORTS(PRBOOL, PRBool),
    COMPONENT_SUPPORTS(PRUINT8, PRUint8),
    COMPONENT_SUPPORTS(PRUINT16, PRUint16),
    COMPONENT_SUPPORTS(PRUINT32, PRUint32),
    COMPONENT_SUPPORTS(PRUINT64, PRUint64),
    COMPONENT_SUPPORTS(PRTIME, PRTime),
    COMPONENT_SUPPORTS(CHAR, Char),
    COMPONENT_SUPPORTS(PRINT16, PRInt16),
    COMPONENT_SUPPORTS(PRINT32, PRInt32),
    COMPONENT_SUPPORTS(PRINT64, PRInt64),
    COMPONENT_SUPPORTS(FLOAT, Float),
    COMPONENT_SUPPORTS(DOUBLE, Double),
    COMPONENT_SUPPORTS(VOID, Void),
    COMPONENT_SUPPORTS(INTERFACE_POINTER, InterfacePointer),

#undef COMPONENT_SUPPORTS
#define NS_LOCAL_FILE_CLASSNAME "Local File Specification"
    COMPONENT(LOCAL_FILE, nsLocalFile::nsLocalFileConstructor),
#define NS_DIRECTORY_SERVICE_CLASSNAME  "nsIFile Directory Service"
    COMPONENT(DIRECTORY_SERVICE, nsDirectoryService::Create),
    COMPONENT(PROCESS, nsProcessConstructor),
    COMPONENT(ENVIRONMENT, nsEnvironment::Create),

    COMPONENT(STRINGINPUTSTREAM, nsStringInputStreamConstructor),
    COMPONENT(MULTIPLEXINPUTSTREAM, nsMultiplexInputStreamConstructor),

    COMPONENT(FASTLOADSERVICE, nsFastLoadService::Create),
    COMPONENT(VARIANT, nsVariantConstructor),
    COMPONENT(INTERFACEINFOMANAGER_SERVICE, nsXPTIInterfaceInfoManagerGetSingleton),

    COMPONENT(RECYCLINGALLOCATOR, nsRecyclingAllocatorImplConstructor),
};

#undef COMPONENT

const int components_length = sizeof(components) / sizeof(components[0]);

// gMemory will be freed during shutdown.
static nsIMemory* gMemory = nsnull;
nsresult NS_COM NS_GetMemoryManager(nsIMemory* *result)
{
    nsresult rv = NS_OK;
    if (!gMemory)
    {
        rv = nsMemoryImpl::Create(nsnull,
                                  NS_GET_IID(nsIMemory),
                                  (void**)&gMemory);
    }
    NS_IF_ADDREF(*result = gMemory);
    return rv;
}

// gDebug will be freed during shutdown.
static nsIDebug* gDebug = nsnull;
nsresult NS_COM NS_GetDebug(nsIDebug** result)
{
    nsresult rv = NS_OK;
    if (!gDebug)
    {
        rv = nsDebugImpl::Create(nsnull, 
                                 NS_GET_IID(nsIDebug), 
                                 (void**)&gDebug);
    }
    NS_IF_ADDREF(*result = gDebug);
    return rv;
}

#ifdef NS_BUILD_REFCNT_LOGGING
// gTraceRefcnt will be freed during shutdown.
static nsITraceRefcnt* gTraceRefcnt = nsnull;
#endif

nsresult NS_COM NS_GetTraceRefcnt(nsITraceRefcnt** result)
{
#ifdef NS_BUILD_REFCNT_LOGGING
    nsresult rv = NS_OK;
    if (!gTraceRefcnt)
    {
        rv = nsTraceRefcntImpl::Create(nsnull, 
                                       NS_GET_IID(nsITraceRefcnt), 
                                       (void**)&gTraceRefcnt);
    }
    NS_IF_ADDREF(*result = gTraceRefcnt);
    return rv;
#else
    return NS_ERROR_NOT_INITIALIZED;
#endif
}

nsresult NS_COM NS_InitXPCOM(nsIServiceManager* *result,
                             nsIFile* binDirectory)
{
    return NS_InitXPCOM2(result, binDirectory, nsnull);
}

nsresult NS_COM NS_InitXPCOM2(nsIServiceManager* *result,
                              nsIFile* binDirectory,
                              nsIDirectoryServiceProvider* appFileLocationProvider)
{
    nsresult rv = NS_OK;

     // We are not shutting down
    gXPCOMShuttingDown = PR_FALSE;

#ifdef NS_BUILD_REFCNT_LOGGING
    nsTraceRefcntImpl::Startup();
#endif

    // Establish the main thread here.
    rv = nsIThread::SetMainThread();
    if (NS_FAILED(rv)) return rv;

    // Set up the timer globals/timer thread
    rv = nsTimerImpl::Startup();
    NS_ENSURE_SUCCESS(rv, rv);

    // Startup the memory manager
    rv = nsMemoryImpl::Startup();
    if (NS_FAILED(rv)) return rv;

    // If the locale hasn't already been setup by our embedder,
    // get us out of the "C" locale and into the system 
    if (strcmp(setlocale(LC_ALL, NULL), "C") == 0)
        setlocale(LC_ALL, "");

#if defined(XP_UNIX) || defined(XP_OS2)
    NS_StartupNativeCharsetUtils();
#endif
    NS_StartupLocalFile();

    StartupSpecialSystemDirectory();

    // Start the directory service so that the component manager init can use it.
    rv = nsDirectoryService::Create(nsnull,
                                    NS_GET_IID(nsIProperties),
                                    (void**)&gDirectoryService);
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIDirectoryService> dirService = do_QueryInterface(gDirectoryService, &rv);
    if (NS_FAILED(rv))
        return rv;
    rv = dirService->Init();
    if (NS_FAILED(rv))
        return rv;

    // Create the Component/Service Manager
    nsComponentManagerImpl *compMgr = NULL;

    if (nsComponentManagerImpl::gComponentManager == NULL)
    {
        compMgr = new nsComponentManagerImpl();
        if (compMgr == NULL)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(compMgr);
        
        nsCOMPtr<nsIFile> xpcomLib;
                
        PRBool value;
        if (binDirectory)
        {
            rv = binDirectory->IsDirectory(&value);

            if (NS_SUCCEEDED(rv) && value) {
                gDirectoryService->Set(NS_XPCOM_INIT_CURRENT_PROCESS_DIR, binDirectory);
                binDirectory->Clone(getter_AddRefs(xpcomLib));
            }
        }
        else {
            gDirectoryService->Get(NS_XPCOM_CURRENT_PROCESS_DIR, 
                                   NS_GET_IID(nsIFile), 
                                   getter_AddRefs(xpcomLib));
        }

        if (xpcomLib) {
            xpcomLib->AppendNative(nsDependentCString(XPCOM_DLL));
            gDirectoryService->Set(NS_XPCOM_LIBRARY_FILE, xpcomLib);
        }
        
        if (appFileLocationProvider) {
            rv = dirService->RegisterProvider(appFileLocationProvider);
            if (NS_FAILED(rv)) return rv;
        }

        rv = compMgr->Init();
        if (NS_FAILED(rv))
        {
            NS_RELEASE(compMgr);
            return rv;
        }

        nsComponentManagerImpl::gComponentManager = compMgr;

        if (result) {
            nsIServiceManager *serviceManager =
                NS_STATIC_CAST(nsIServiceManager*, compMgr);

            NS_ADDREF(*result = serviceManager);
        }
    }

    nsCOMPtr<nsIMemory> memory;
    NS_GetMemoryManager(getter_AddRefs(memory));
    // dougt - these calls will be moved into a new interface when nsIComponentManager is frozen.
    rv = compMgr->RegisterService(kMemoryCID, memory);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->RegisterService(kComponentManagerCID, NS_STATIC_CAST(nsIComponentManager*, compMgr));
    if (NS_FAILED(rv)) return rv;

#ifdef GC_LEAK_DETECTOR
  rv = NS_InitLeakDetector();
    if (NS_FAILED(rv)) return rv;
#endif

    // 2. Register the global services with the component manager so that
    //    clients can create new objects.

    // Category Manager
    {
      nsCOMPtr<nsIFactory> categoryManagerFactory;
      if ( NS_FAILED(rv = NS_CategoryManagerGetFactory(getter_AddRefs(categoryManagerFactory))) )
        return rv;

      NS_DEFINE_CID(kCategoryManagerCID, NS_CATEGORYMANAGER_CID);

      rv = compMgr->RegisterFactory(kCategoryManagerCID,
                                    NS_CATEGORYMANAGER_CLASSNAME,
                                    NS_CATEGORYMANAGER_CONTRACTID,
                                    categoryManagerFactory,
                                    PR_TRUE);
      if ( NS_FAILED(rv) ) return rv;
    }

    // what I want to do here is QI for a Component Registration Manager.  Since this
    // has not been invented yet, QI to the obsolete manager.  Kids, don't do this at home.
    nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(
        NS_STATIC_CAST(nsIComponentManager*,compMgr), &rv);
    if (registrar) {
        for (int i = 0; i < components_length; i++)
            RegisterGenericFactory(registrar, &components[i]);
    }
    rv = nsComponentManagerImpl::gComponentManager->ReadPersistentRegistry();
#ifdef DEBUG    
    if (NS_FAILED(rv)) {
        printf("No Persistent Registry Found.\n");        
    }
#endif

    if ( NS_FAILED(rv) || CheckUpdateFile()) {
        // if we find no persistent registry, we will try to autoregister
        // the default components directory.
        nsComponentManagerImpl::gComponentManager->AutoRegister(nsnull);

        // If the application is using a GRE, then, 
        // auto register components in the GRE directory as well.
        //
        // The application indicates that it's using an GRE by
        // returning a valid nsIFile when queried (via appFileLocProvider)
        // for the NS_GRE_DIR atom as shown below
        //

        if ( appFileLocationProvider ) {
            nsCOMPtr<nsIFile> greDir;
            PRBool persistent = PR_TRUE;

            appFileLocationProvider->GetFile(NS_GRE_DIR, &persistent, getter_AddRefs(greDir));

            if (greDir) {
#ifdef DEBUG_dougt
                printf("start - Registering GRE components\n");
#endif
                rv = gDirectoryService->Get(NS_GRE_COMPONENT_DIR,
                                            NS_GET_IID(nsIFile),
                                            getter_AddRefs(greDir));
                if (NS_FAILED(rv)) {
                    NS_ERROR("Could not get GRE components directory!");
                    return rv;
                }

                // If the GRE contains any loaders, we want to know about it so that we can cause another
                // autoregistration of the applications component directory.
                int loaderCount = nsComponentManagerImpl::gComponentManager->GetLoaderCount();
                rv = nsComponentManagerImpl::gComponentManager->AutoRegister(greDir);
                
                if (loaderCount != nsComponentManagerImpl::gComponentManager->GetLoaderCount()) 
                    nsComponentManagerImpl::gComponentManager->AutoRegisterNonNativeComponents(nsnull);        

#ifdef DEBUG_dougt
                printf("end - Registering GRE components\n");
#endif          
                if (NS_FAILED(rv)) {
                    NS_ERROR("Could not AutoRegister GRE components");
                    return rv;
                }
            }
        }

        //
        // If additional component directories have been specified, then
        // register them as well.
        //

        nsCOMPtr<nsISimpleEnumerator> dirList;
        gDirectoryService->Get(NS_XPCOM_COMPONENT_DIR_LIST,
                               NS_GET_IID(nsISimpleEnumerator),
                               getter_AddRefs(dirList));
        if (dirList) {
            PRBool hasMore;
            while (NS_SUCCEEDED(dirList->HasMoreElements(&hasMore)) && hasMore) {
                nsCOMPtr<nsISupports> elem;
                dirList->GetNext(getter_AddRefs(elem));
                if (elem) {
                    nsCOMPtr<nsIFile> dir = do_QueryInterface(elem);
                    if (dir)
                        nsComponentManagerImpl::gComponentManager->AutoRegister(dir);

                    // XXX should we worry about new component loaders being
                    // XXX defined by this process?
                }
            }
        }


        // Make sure the compreg file's mod time is current.
        nsCOMPtr<nsIFile> compregFile;
        rv = gDirectoryService->Get(NS_XPCOM_COMPONENT_REGISTRY_FILE,
                                    NS_GET_IID(nsIFile),
                                    getter_AddRefs(compregFile));
        compregFile->SetLastModifiedTime(PR_Now() / 1000);
    }
    
    // Pay the cost at startup time of starting this singleton.
    nsIInterfaceInfoManager* iim = XPTI_GetInterfaceInfoManager();
    NS_IF_RELEASE(iim);

    // Notify observers of xpcom autoregistration start
    NS_CreateServicesFromCategory(NS_XPCOM_STARTUP_OBSERVER_ID, 
                                  nsnull,
                                  NS_XPCOM_STARTUP_OBSERVER_ID);
    
    return NS_OK;
}


static nsVoidArray* gExitRoutines;

static void CallExitRoutines()
{
    if (!gExitRoutines)
        return;

    PRInt32 count = gExitRoutines->Count();
    for (PRInt32 i = 0; i < count; i++) {
        XPCOMExitRoutine func = (XPCOMExitRoutine) gExitRoutines->ElementAt(i);
        func();
    }
    gExitRoutines->Clear();
    delete gExitRoutines;
    gExitRoutines = nsnull;
}

nsresult NS_COM
NS_RegisterXPCOMExitRoutine(XPCOMExitRoutine exitRoutine, PRUint32 priority)
{
    // priority are not used right now.  It will need to be implemented as more
    // classes are moved into the glue library --dougt
    if (!gExitRoutines) {
        gExitRoutines = new nsVoidArray();
        if (!gExitRoutines) {
            NS_WARNING("Failed to allocate gExitRoutines");
            return NS_ERROR_FAILURE;
        }
    }

    PRBool okay = gExitRoutines->AppendElement((void*)exitRoutine);
    return okay ? NS_OK : NS_ERROR_FAILURE;
}

nsresult NS_COM
NS_UnregisterXPCOMExitRoutine(XPCOMExitRoutine exitRoutine)
{
    if (!gExitRoutines)
        return NS_ERROR_FAILURE;

    PRBool okay = gExitRoutines->RemoveElement((void*)exitRoutine);
    return okay ? NS_OK : NS_ERROR_FAILURE;
}


//
// NS_ShutdownXPCOM()
//
// The shutdown sequence for xpcom would be
//
// - Release the Global Service Manager
//   - Release all service instances held by the global service manager
//   - Release the Global Service Manager itself
// - Release the Component Manager
//   - Release all factories cached by the Component Manager
//   - Unload Libraries
//   - Release Contractid Cache held by Component Manager
//   - Release dll abstraction held by Component Manager
//   - Release the Registry held by Component Manager
//   - Finally, release the component manager itself
//
nsresult NS_COM NS_ShutdownXPCOM(nsIServiceManager* servMgr)
{

    // Notify observers of xpcom shutting down
    nsresult rv = NS_OK;
    {
        // Block it so that the COMPtr will get deleted before we hit
        // servicemanager shutdown
        nsCOMPtr<nsIObserverService> observerService =
                 do_GetService("@mozilla.org/observer-service;1", &rv);
        if (NS_SUCCEEDED(rv))
        {
            nsCOMPtr<nsIServiceManager> mgr;
            rv = NS_GetServiceManager(getter_AddRefs(mgr));
            if (NS_SUCCEEDED(rv))
            {
                (void) observerService->NotifyObservers(mgr,
                                                        NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                                        nsnull);
            }
        }
    }

    // grab the event queue so that we can process events one last time before exiting
    nsCOMPtr <nsIEventQueue> currentQ;
    {
        nsCOMPtr<nsIEventQueueService> eventQService =
                 do_GetService(kEventQueueServiceCID, &rv);

        if (eventQService) {
            eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(currentQ));
        }
    }
    // XPCOM is officially in shutdown mode NOW
    // Set this only after the observers have been notified as this
    // will cause servicemanager to become inaccessible.
    gXPCOMShuttingDown = PR_TRUE;

#ifdef DEBUG_dougt
    fprintf(stderr, "* * * * XPCOM shutdown. Access will be denied * * * * \n");
#endif
    // We may have AddRef'd for the caller of NS_InitXPCOM, so release it
    // here again:
    NS_IF_RELEASE(servMgr);

    // Shutdown global servicemanager
    if (nsComponentManagerImpl::gComponentManager) {
        nsComponentManagerImpl::gComponentManager->FreeServices();
    }
    nsServiceManager::ShutdownGlobalServiceManager(nsnull);

    if (currentQ) {
        currentQ->ProcessPendingEvents();
        currentQ = 0;
    }
    
    nsProxyObjectManager::Shutdown();

    // Release the directory service
    NS_IF_RELEASE(gDirectoryService);

    // Shutdown nsLocalFile string conversion
    NS_ShutdownLocalFile();
#ifdef XP_UNIX
    NS_ShutdownNativeCharsetUtils();
#endif

    // Shutdown the timer thread and all timers that might still be alive before
    // shutting down the component manager
    nsTimerImpl::Shutdown();

    CallExitRoutines();

    // Shutdown xpcom. This will release all loaders and cause others holding
    // a refcount to the component manager to release it.
    if (nsComponentManagerImpl::gComponentManager) {
        rv = (nsComponentManagerImpl::gComponentManager)->Shutdown();
        NS_ASSERTION(NS_SUCCEEDED(rv), "Component Manager shutdown failed.");
    } else
        NS_WARNING("Component Manager was never created ...");

    // Release our own singletons
    // Do this _after_ shutting down the component manager, because the
    // JS component loader will use XPConnect to call nsIModule::canUnload,
    // and that will spin up the InterfaceInfoManager again -- bad mojo
    XPTI_FreeInterfaceInfoManager();

    // Finally, release the component manager last because it unloads the
    // libraries:
    if (nsComponentManagerImpl::gComponentManager) {
      nsrefcnt cnt;
      NS_RELEASE2(nsComponentManagerImpl::gComponentManager, cnt);
      NS_WARN_IF_FALSE(cnt == 0, "Component Manager being held past XPCOM shutdown.");
    }
    nsComponentManagerImpl::gComponentManager = nsnull;

#ifdef DEBUG
    _FreeAutoLockStatics();
#endif

    ShutdownSpecialSystemDirectory();

    EmptyEnumeratorImpl::Shutdown();
    nsMemoryImpl::Shutdown();
    NS_IF_RELEASE(gMemory);

    nsThread::Shutdown();
    NS_PurgeAtomTable();

    NS_IF_RELEASE(gDebug);

#ifdef NS_BUILD_REFCNT_LOGGING
    nsTraceRefcntImpl::DumpStatistics();
    nsTraceRefcntImpl::ResetStatistics();
    nsTraceRefcntImpl::Shutdown();
#endif

#ifdef GC_LEAK_DETECTOR
    // Shutdown the Leak detector.
    NS_ShutdownLeakDetector();
#endif

    return NS_OK;
}

#define GET_FUNC(_tag, _decl, _name)                        \
  functions->_tag = (_decl) PR_FindSymbol(xpcomLib, _name); \
  if (!functions->_tag) goto end

nsresult NS_COM PR_CALLBACK
NS_GetFrozenFunctions(XPCOMFunctions *functions, const char* libraryPath)
{
    if (!functions)
        return NS_ERROR_OUT_OF_MEMORY;

    if (functions->version != XPCOM_GLUE_VERSION)
        return NS_ERROR_FAILURE;

    PRLibrary *xpcomLib = PR_LoadLibrary(libraryPath);
    if (!xpcomLib)
        return NS_ERROR_FAILURE;

    nsresult rv = NS_ERROR_FAILURE;

    GET_FUNC(init,                  InitFunc,                       "NS_InitXPCOM2");
    GET_FUNC(shutdown,              ShutdownFunc,                   "NS_ShutdownXPCOM");
    GET_FUNC(getServiceManager,     GetServiceManagerFunc,          "NS_GetServiceManager");
    GET_FUNC(getComponentManager,   GetComponentManagerFunc,        "NS_GetComponentManager");
    GET_FUNC(getComponentRegistrar, GetComponentRegistrarFunc,      "NS_GetComponentRegistrar");
    GET_FUNC(getMemoryManager,      GetMemoryManagerFunc,           "NS_GetMemoryManager");
    GET_FUNC(newLocalFile,          NewLocalFileFunc,               "NS_NewLocalFile");
    GET_FUNC(newNativeLocalFile,    NewNativeLocalFileFunc,         "NS_NewNativeLocalFile");
    GET_FUNC(registerExitRoutine,   RegisterXPCOMExitRoutineFunc,   "NS_RegisterXPCOMExitRoutine");
    GET_FUNC(unregisterExitRoutine, UnregisterXPCOMExitRoutineFunc, "NS_UnregisterXPCOMExitRoutine");

    // these functions were added post 1.4 (need to check size of |functions|)
    if (functions->size > offsetof(XPCOMFunctions, getTraceRefcnt)) {
        GET_FUNC(getDebug,          GetDebugFunc,                   "NS_GetDebug");
        GET_FUNC(getTraceRefcnt,    GetTraceRefcntFunc,             "NS_GetTraceRefcnt");
    }

    // these functions were added post 1.6 (need to check size of |functions|)
    if (functions->size > offsetof(XPCOMFunctions, cstringCloneData)) {
        GET_FUNC(stringContainerInit,    StringContainerInitFunc,        "NS_StringContainerInit");
        GET_FUNC(stringContainerFinish,  StringContainerFinishFunc,      "NS_StringContainerFinish");
        GET_FUNC(stringGetData,          StringGetDataFunc,              "NS_StringGetData");
        GET_FUNC(stringSetData,          StringSetDataFunc,              "NS_StringSetData");
        GET_FUNC(stringSetDataRange,     StringSetDataRangeFunc,         "NS_StringSetDataRange");
        GET_FUNC(stringCopy,             StringCopyFunc,                 "NS_StringCopy");
        GET_FUNC(cstringContainerInit,   CStringContainerInitFunc,       "NS_CStringContainerInit");
        GET_FUNC(cstringContainerFinish, CStringContainerFinishFunc,     "NS_CStringContainerFinish");
        GET_FUNC(cstringGetData,         CStringGetDataFunc,             "NS_CStringGetData");
        GET_FUNC(cstringSetData,         CStringSetDataFunc,             "NS_CStringSetData");
        GET_FUNC(cstringSetDataRange,    CStringSetDataRangeFunc,        "NS_CStringSetDataRange");
        GET_FUNC(cstringCopy,            CStringCopyFunc,                "NS_CStringCopy");
        GET_FUNC(cstringToUTF16,         CStringToUTF16,                 "NS_CStringToUTF16");
        GET_FUNC(utf16ToCString,         UTF16ToCString,                 "NS_UTF16ToCString");
        GET_FUNC(stringCloneData,        StringCloneDataFunc,            "NS_StringCloneData");
        GET_FUNC(cstringCloneData,       CStringCloneDataFunc,           "NS_CStringCloneData");
    }

    rv = NS_OK;
end:
    PR_UnloadLibrary(xpcomLib); // the library is refcnt'ed above by the caller.
    return rv;
}
