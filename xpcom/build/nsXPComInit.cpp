/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsIRegistry.h"
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsObserverService.h"
#include "nsObserver.h"
#include "nsProperties.h"
#include "nsIProperties.h"
#include "nsPersistentProperties.h"
#include "nsScriptableInputStream.h"

#include "nsMemoryImpl.h"
#include "nsErrorService.h"
#include "nsArena.h"
#include "nsByteBuffer.h"
#ifdef PAGE_MANAGER
#include "nsPageMgr.h"
#endif
#include "nsSupportsArray.h"
#include "nsSupportsPrimitives.h"
#include "nsUnicharBuffer.h"
#include "nsConsoleService.h"

#include "nsComponentManager.h"
#include "nsIServiceManager.h"
#include "nsGenericFactory.h"

#include "nsEventQueueService.h"
#include "nsEventQueue.h"

#include "nsIProxyObjectManager.h"
#include "nsProxyEventPrivate.h"  // access to the impl of nsProxyObjectManager for the generic factory registration.

#include "xptinfo.h"
#include "nsIInterfaceInfoManager.h"

#include "nsThread.h"

#include "nsFileSpecImpl.h"
#include "nsSpecialSystemDirectory.h"

#include "nsILocalFile.h"
#include "nsLocalFile.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsICategoryManager.h"

#include "nsAtomService.h"
#include "nsTraceRefcnt.h"

#ifdef GC_LEAK_DETECTOR
#include "nsLeakDetector.h"
#endif

#ifndef XPCOM_STANDALONE
// Include files that dont reside under xpcom
// Our goal was to make this zero. But... :-(
#include "nsICaseConversion.h"
#endif /* XPCOM_STANDALONE */

// base
static NS_DEFINE_CID(kMemoryCID, NS_MEMORY_CID);
static NS_DEFINE_CID(kConsoleServiceCID, NS_CONSOLESERVICE_CID);
static NS_DEFINE_CID(kErrorServiceCID, NS_ERRORSERVICE_CID);
// ds
static NS_DEFINE_CID(kArenaCID, NS_ARENA_CID);
static NS_DEFINE_CID(kByteBufferCID, NS_BYTEBUFFER_CID);
#ifdef PAGE_MANAGER
static NS_DEFINE_CID(kPageManagerCID, NS_PAGEMANAGER_CID);
#endif
static NS_DEFINE_CID(kPropertiesCID, NS_PROPERTIES_CID);
static NS_DEFINE_CID(kSupportsArrayCID, NS_SUPPORTSARRAY_CID);
static NS_DEFINE_CID(kUnicharBufferCID, NS_UNICHARBUFFER_CID);
// ds/nsISupportsPrimitives
static NS_DEFINE_CID(kSupportsIDCID, NS_SUPPORTS_ID_CID);
static NS_DEFINE_CID(kSupportsStringCID, NS_SUPPORTS_STRING_CID);
static NS_DEFINE_CID(kSupportsWStringCID, NS_SUPPORTS_WSTRING_CID);
static NS_DEFINE_CID(kSupportsPRBoolCID, NS_SUPPORTS_PRBOOL_CID);
static NS_DEFINE_CID(kSupportsPRUint8CID, NS_SUPPORTS_PRUINT8_CID);
static NS_DEFINE_CID(kSupportsPRUint16CID, NS_SUPPORTS_PRUINT16_CID);
static NS_DEFINE_CID(kSupportsPRUint32CID, NS_SUPPORTS_PRUINT32_CID);
static NS_DEFINE_CID(kSupportsPRUint64CID, NS_SUPPORTS_PRUINT64_CID);
static NS_DEFINE_CID(kSupportsPRTimeCID, NS_SUPPORTS_PRTIME_CID);
static NS_DEFINE_CID(kSupportsCharCID, NS_SUPPORTS_CHAR_CID);
static NS_DEFINE_CID(kSupportsPRInt16CID, NS_SUPPORTS_PRINT16_CID);
static NS_DEFINE_CID(kSupportsPRInt32CID, NS_SUPPORTS_PRINT32_CID);
static NS_DEFINE_CID(kSupportsPRInt64CID, NS_SUPPORTS_PRINT64_CID);
static NS_DEFINE_CID(kSupportsFloatCID, NS_SUPPORTS_FLOAT_CID);
static NS_DEFINE_CID(kSupportsDoubleCID, NS_SUPPORTS_DOUBLE_CID);
static NS_DEFINE_CID(kSupportsVoidCID, NS_SUPPORTS_VOID_CID);
// io
static NS_DEFINE_CID(kFileSpecCID, NS_FILESPEC_CID);
static NS_DEFINE_CID(kDirectoryIteratorCID, NS_DIRECTORYITERATOR_CID);
static NS_DEFINE_CID(kScriptableInputStreamCID, NS_SCRIPTABLEINPUTSTREAM_CID);

// components
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kGenericFactoryCID, NS_GENERICFACTORY_CID);
// threads
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kEventQueueCID, NS_EVENTQUEUE_CID);
static NS_DEFINE_CID(kThreadCID, NS_THREAD_CID);
static NS_DEFINE_CID(kThreadPoolCID, NS_THREADPOOL_CID);
// proxy
static NS_DEFINE_CID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);

// atoms
static NS_DEFINE_CID(kAtomServiceCID, NS_ATOMSERVICE_CID);

// ds/nsISupportsPrimitives
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsIDImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsStringImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsWStringImpl)
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

NS_GENERIC_FACTORY_CONSTRUCTOR(nsConsoleService);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAtomService);
    
////////////////////////////////////////////////////////////////////////////////
// XPCOM initialization
//
// To Control the order of initialization of these key components I am putting
// this function.
//
//  - nsServiceManager
//    - nsComponentManager
//      - nsRegistry
//
// Here are key points to remember:
//  - A global of all these need to exist. nsServiceManager is an independent object.
//    nsComponentManager uses both the globalServiceManager and its own registry.
//
//  - A static object of both the nsComponentManager and nsServiceManager
//    are in use. Hence InitXPCOM() gets triggered from both
//    NS_GetGlobale{Service/Component}Manager() calls.
//
//  - There exists no global Registry. Registry can be created from the component manager.
//

static nsresult
RegisterGenericFactory(nsIComponentManager* compMgr, const nsCID& cid, const char* className,
                       const char *contractid, nsIGenericFactory::ConstructorProcPtr constr)
{
    nsresult rv;
    nsIGenericFactory* fact;
    rv = NS_NewGenericFactory(&fact, constr);
    if (NS_FAILED(rv)) return rv;
    rv = compMgr->RegisterFactory(cid, className, contractid, fact, PR_TRUE);
    NS_RELEASE(fact);
    return rv;
}

// Globals in xpcom

nsComponentManagerImpl* nsComponentManagerImpl::gComponentManager = NULL;
#ifndef XPCOM_STANDALONE
nsICaseConversion *gCaseConv = NULL;
#endif /* XPCOM_STANDALONE */
nsIProperties     *gDirectoryService = NULL;
extern nsIServiceManager* gServiceManager;
extern PRBool gShuttingDown;


nsresult NS_COM NS_InitXPCOM(nsIServiceManager* *result, 
                             nsIFile* binDirectory)
{
    return NS_InitXPCOM2(nsnull, result, binDirectory);
}


nsresult NS_COM NS_InitXPCOM2(const char* productName,
                             nsIServiceManager* *result, 
                             nsIFile* binDirectory
)
{
    nsresult rv = NS_OK;

    // Establish the main thread here.
    rv = nsIThread::SetMainThread();
    if (NS_FAILED(rv)) return rv;

    // Startup the memory manager
    rv = nsMemoryImpl::Startup();
    if (NS_FAILED(rv)) return rv;

    // 1. Create the Global Service Manager
    nsIServiceManager* servMgr = NULL;
    if (gServiceManager == NULL)
    {
        rv = NS_NewServiceManager(&servMgr);
        if (NS_FAILED(rv)) return rv;
        gServiceManager = servMgr;
        if (result)
    {
          NS_ADDREF(servMgr);
      *result = servMgr;
    }
    }

    // 2. Create the Component Manager and register with global service manager
    //    It is understood that the component manager can use the global service manager.
    nsComponentManagerImpl *compMgr = NULL;
    
    if (nsComponentManagerImpl::gComponentManager == NULL)
    {
        compMgr = new nsComponentManagerImpl();
        if (compMgr == NULL)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(compMgr);

        rv = nsDirectoryService::Create(nsnull, 
                                        NS_GET_IID(nsIProperties), 
                                        (void**)&gDirectoryService);
        
        if (NS_FAILED(rv))
            return rv;
        
        nsCOMPtr<nsIDirectoryService> dirService = do_QueryInterface(gDirectoryService);
        
        if (!dirService)
            return NS_ERROR_NO_INTERFACE;

        rv = dirService->Init(productName);
        
        if (NS_FAILED(rv))
            return rv;

        PRBool value;
                        
        if (binDirectory)
        {
            rv = binDirectory->IsDirectory(&value);

            if (NS_SUCCEEDED(rv) && value)
                gDirectoryService->Define(NS_XPCOM_INIT_CURRENT_PROCESS_DIR, binDirectory);

            //Since people are still using the nsSpecialSystemDirectory, we should init it.
            char* path;
            binDirectory->GetPath(&path);
            nsFileSpec spec(path);
            nsMemory::Free(path);
            
            nsSpecialSystemDirectory::Set(nsSpecialSystemDirectory::Moz_BinDirectory, &spec);
            
        }

        
        rv = compMgr->Init();
        if (NS_FAILED(rv))
        {
            NS_RELEASE(compMgr);
            return rv;
        }
        
        nsComponentManagerImpl::gComponentManager = compMgr;
    }

    nsCOMPtr<nsIMemory> memory = getter_AddRefs(nsMemory::GetGlobalMemoryService());
    rv = servMgr->RegisterService(kMemoryCID, memory);
    if (NS_FAILED(rv)) return rv;
    
    rv = servMgr->RegisterService(kComponentManagerCID, NS_STATIC_CAST(nsIComponentManager*, compMgr));
    if (NS_FAILED(rv)) return rv;
    
#ifdef GC_LEAK_DETECTOR
  rv = NS_InitLeakDetector();
    if (NS_FAILED(rv)) return rv;
#endif

    // 3. Register the global services with the component manager so that
    //    clients can create new objects.

    // Registry
    nsIFactory *registryFactory = NULL;
    rv = NS_RegistryGetFactory(&registryFactory);
    if (NS_FAILED(rv)) return rv;

    NS_DEFINE_CID(kRegistryCID, NS_REGISTRY_CID);

    rv = compMgr->RegisterFactory(kRegistryCID,
                                  NS_REGISTRY_CLASSNAME,
                                  NS_REGISTRY_CONTRACTID,
                                  registryFactory, PR_TRUE);
    NS_RELEASE(registryFactory);
    if (NS_FAILED(rv)) return rv;

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
      if ( NS_FAILED(rv) )
        return rv;
    }

    rv = RegisterGenericFactory(compMgr, kMemoryCID,
                                NS_MEMORY_CLASSNAME,
                                NS_MEMORY_CONTRACTID,
                                nsMemoryImpl::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kErrorServiceCID,
                                NS_ERRORSERVICE_NAME,
                                NS_ERRORSERVICE_CONTRACTID,
                                nsErrorService::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kArenaCID,
                                NS_ARENA_CLASSNAME,
                                NS_ARENA_CONTRACTID,
                                ArenaImpl::Create);
    if (NS_FAILED(rv)) return rv;

#if 0
    rv = RegisterGenericFactory(compMgr, kBufferCID,
                                NS_BUFFER_CLASSNAME,
                                NS_BUFFER_CONTRACTID,
                                nsBuffer::Create);
    if (NS_FAILED(rv)) return rv;
#endif

    rv = RegisterGenericFactory(compMgr, kByteBufferCID,
                                NS_BYTEBUFFER_CLASSNAME,
                                NS_BYTEBUFFER_CONTRACTID,
                                ByteBufferImpl::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kScriptableInputStreamCID,
                                NS_SCRIPTABLEINPUTSTREAM_CLASSNAME,
                                NS_SCRIPTABLEINPUTSTREAM_CONTRACTID,
                                nsScriptableInputStream::Create);
    if (NS_FAILED(rv)) return rv;

#ifdef PAGE_MANAGER
    rv = RegisterGenericFactory(compMgr, kPageManagerCID,
                                NS_PAGEMANAGER_CLASSNAME,
                                NS_PAGEMANAGER_CONTRACTID,
                                nsPageMgr::Create);
    if (NS_FAILED(rv)) return rv;
#endif

    rv = RegisterGenericFactory(compMgr, kPropertiesCID,
                                NS_PROPERTIES_CLASSNAME,
                                NS_PROPERTIES_CONTRACTID,
                                nsProperties::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kPersistentPropertiesCID,
                                NS_PERSISTENTPROPERTIES_CLASSNAME,
                                NS_PERSISTENTPROPERTIES_CONTRACTID,
                                nsPersistentProperties::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsArrayCID,
                                NS_SUPPORTSARRAY_CLASSNAME,
                                NS_SUPPORTSARRAY_CONTRACTID,
                                nsSupportsArray::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kConsoleServiceCID,
                                NS_CONSOLESERVICE_CLASSNAME,
                                NS_CONSOLESERVICE_CONTRACTID,
                                nsConsoleServiceConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kAtomServiceCID,
                                NS_ATOMSERVICE_CLASSNAME,
                                NS_ATOMSERVICE_CONTRACTID,
                                nsAtomServiceConstructor);
    if (NS_FAILED(rv)) return rv;
    
    rv = RegisterGenericFactory(compMgr, nsObserver::GetCID(), 
                                NS_OBSERVER_CLASSNAME,
                                NS_OBSERVER_CONTRACTID,
                                nsObserver::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, nsObserverService::GetCID(),
                                NS_OBSERVERSERVICE_CLASSNAME,
                                NS_OBSERVERSERVICE_CONTRACTID,
                                nsObserverService::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kGenericFactoryCID,
                                NS_GENERICFACTORY_CLASSNAME,
                                NS_GENERICFACTORY_CONTRACTID,
                                nsGenericFactory::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kEventQueueServiceCID,
                                NS_EVENTQUEUESERVICE_CLASSNAME,
                                NS_EVENTQUEUESERVICE_CONTRACTID,
                                nsEventQueueServiceImpl::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kEventQueueCID,
                                NS_EVENTQUEUE_CLASSNAME,
                                NS_EVENTQUEUE_CONTRACTID,
                                nsEventQueueImpl::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kThreadCID,
                                NS_THREAD_CLASSNAME,
                                NS_THREAD_CONTRACTID,
                                nsThread::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kThreadPoolCID,
                                NS_THREADPOOL_CLASSNAME,
                                NS_THREADPOOL_CONTRACTID,
                                nsThreadPool::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, 
                                kProxyObjectManagerCID, 
                                NS_XPCOMPROXY_CLASSNAME,
                                NS_XPCOMPROXY_CONTRACTID,
                                nsProxyObjectManager::Create);
    if (NS_FAILED(rv)) return rv;


    rv = RegisterGenericFactory(compMgr, kSupportsIDCID,
                                NS_SUPPORTS_ID_CLASSNAME,
                                NS_SUPPORTS_ID_CONTRACTID,
                                nsSupportsIDImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsStringCID,
                                NS_SUPPORTS_STRING_CLASSNAME,
                                NS_SUPPORTS_STRING_CONTRACTID,
                                nsSupportsStringImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsWStringCID,
                                NS_SUPPORTS_WSTRING_CLASSNAME,
                                NS_SUPPORTS_WSTRING_CONTRACTID,
                                nsSupportsWStringImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsPRBoolCID,
                                NS_SUPPORTS_PRBOOL_CLASSNAME,
                                NS_SUPPORTS_PRBOOL_CONTRACTID,
                                nsSupportsPRBoolImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsPRUint8CID,
                                NS_SUPPORTS_PRUINT8_CLASSNAME,
                                NS_SUPPORTS_PRUINT8_CONTRACTID,
                                nsSupportsPRUint8ImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsPRUint16CID,
                                NS_SUPPORTS_PRUINT16_CLASSNAME,
                                NS_SUPPORTS_PRUINT16_CONTRACTID,
                                nsSupportsPRUint16ImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsPRUint32CID,
                                NS_SUPPORTS_PRUINT32_CLASSNAME,
                                NS_SUPPORTS_PRUINT32_CONTRACTID,
                                nsSupportsPRUint32ImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsPRUint64CID,
                                NS_SUPPORTS_PRUINT64_CLASSNAME,
                                NS_SUPPORTS_PRUINT64_CONTRACTID,
                                nsSupportsPRUint64ImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsPRTimeCID,
                                NS_SUPPORTS_PRTIME_CLASSNAME,
                                NS_SUPPORTS_PRTIME_CONTRACTID,
                                nsSupportsPRTimeImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsCharCID,
                                NS_SUPPORTS_CHAR_CLASSNAME,
                                NS_SUPPORTS_CHAR_CONTRACTID,
                                nsSupportsCharImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsPRInt16CID,
                                NS_SUPPORTS_PRINT16_CLASSNAME,
                                NS_SUPPORTS_PRINT16_CONTRACTID,
                                nsSupportsPRInt16ImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsPRInt32CID,
                                NS_SUPPORTS_PRINT32_CLASSNAME,
                                NS_SUPPORTS_PRINT32_CONTRACTID,
                                nsSupportsPRInt32ImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsPRInt64CID,
                                NS_SUPPORTS_PRINT64_CLASSNAME,
                                NS_SUPPORTS_PRINT64_CONTRACTID,
                                nsSupportsPRInt64ImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsFloatCID,
                                NS_SUPPORTS_FLOAT_CLASSNAME,
                                NS_SUPPORTS_FLOAT_CONTRACTID,
                                nsSupportsFloatImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsDoubleCID,
                                NS_SUPPORTS_DOUBLE_CLASSNAME,
                                NS_SUPPORTS_DOUBLE_CONTRACTID,
                                nsSupportsDoubleImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsVoidCID,
                                NS_SUPPORTS_VOID_CLASSNAME,
                                NS_SUPPORTS_VOID_CONTRACTID,
                                nsSupportsVoidImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, nsLocalFile::GetCID(),
                                NS_LOCAL_FILE_CLASSNAME,
                                NS_LOCAL_FILE_CONTRACTID,
                                nsLocalFile::nsLocalFileConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, nsDirectoryService::GetCID(),
                                NS_DIRECTORY_SERVICE_CLASSNAME,
                                NS_DIRECTORY_SERVICE_CONTRACTID,
                                nsDirectoryService::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kFileSpecCID,
                                NS_FILESPEC_CLASSNAME,
                                NS_FILESPEC_CONTRACTID,
                                nsFileSpecImpl::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kDirectoryIteratorCID,
                                NS_DIRECTORYITERATOR_CLASSNAME,
                                NS_DIRECTORYITERATOR_CONTRACTID,
                                nsDirectoryIteratorImpl::Create);
    if (NS_FAILED(rv)) return rv;

    // Prepopulate registry for performance
    // Ignore return value. It is ok if this fails.
    nsComponentManagerImpl::gComponentManager->PlatformPrePopulateRegistry();

    // Pay the cost at startup time of starting this singleton.
    nsIInterfaceInfoManager* iim = XPTI_GetInterfaceInfoManager();
    NS_IF_RELEASE(iim);

    return rv;
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
    nsrefcnt cnt;

    // Notify observers of xpcom shutting down
    nsresult rv = NS_OK;
    {
        // Block it so that the COMPtr will get deleted before we hit
        // servicemanager shutdown
        NS_WITH_SERVICE (nsIObserverService, observerService, NS_OBSERVERSERVICE_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv))
        {
            nsIServiceManager *mgr;    // NO COMPtr as we dont release the service manager
            rv = nsServiceManager::GetGlobalServiceManager(&mgr);
            if (NS_SUCCEEDED(rv))
            {
                nsAutoString topic;
                topic.AssignWithConversion(NS_XPCOM_SHUTDOWN_OBSERVER_ID);
                (void) observerService->Notify(mgr, topic.GetUnicode(), nsnull);
            }
        }
    }

    // XPCOM is officially in shutdown mode NOW
    // Set this only after the observers have been notified as this
    // will cause servicemanager to become inaccessible.
    gShuttingDown = PR_TRUE;

    // We may have AddRef'd for the caller of NS_InitXPCOM, so release it
    // here again:
    NS_IF_RELEASE(servMgr);

    // Shutdown global servicemanager
    nsServiceManager::ShutdownGlobalServiceManager(NULL);

#ifndef XPCOM_STANDALONE
    // Release the global case converter
    NS_IF_RELEASE(gCaseConv);
#endif /* XPCOM_STANDALONE */
    
    // Release the directory service
    NS_IF_RELEASE(gDirectoryService);

    // Shutdown xpcom. This will release all loaders and cause others holding
    // a refcount to the component manager to release it.
    rv = (nsComponentManagerImpl::gComponentManager)->Shutdown();
    NS_ASSERTION(NS_SUCCEEDED(rv), "Component Manager shutdown failed.");

    // Release our own singletons
    // Do this _after_ shutting down the component manager, because the
    // JS component loader will use XPConnect to call nsIModule::canUnload,
    // and that will spin up the InterfaceInfoManager again -- bad mojo
    XPTI_FreeInterfaceInfoManager();

    // Finally, release the component manager last because it unloads the
    // libraries:
    NS_RELEASE2(nsComponentManagerImpl::gComponentManager, cnt);
    NS_WARN_IF_FALSE(cnt == 0, "Component Manager being held past XPCOM shutdown.");

#ifdef DEBUG
    extern void _FreeAutoLockStatics();
    _FreeAutoLockStatics();
#endif

    nsMemoryImpl::Shutdown();
    nsThread::Shutdown();
    NS_PurgeAtomTable();

#if (defined(DEBUG) || defined(NS_BUILD_REFCNT_LOGGING))
    nsTraceRefcnt::DumpStatistics();
    nsTraceRefcnt::ResetStatistics();
#endif

#ifdef GC_LEAK_DETECTOR
    // Shutdown the Leak detector.
    NS_ShutdownLeakDetector();
#endif

    return NS_OK;
}
