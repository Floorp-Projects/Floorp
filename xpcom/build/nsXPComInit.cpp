/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 ci et: */
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
 *   Benjamin Smedberg <benjamin@smedbergs.us>
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
#include "nsXPCOMCIDInternal.h"
#include "nscore.h"
#include "nsIClassInfoImpl.h"
#include "nsStaticComponents.h"
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
#include "nsPipe.h"

#include "nsMemoryImpl.h"
#include "nsDebugImpl.h"
#include "nsTraceRefcntImpl.h"
#include "nsErrorService.h"
#include "nsByteBuffer.h"

#include "nsSupportsArray.h"
#include "nsArray.h"
#include "nsINIParserImpl.h"
#include "nsSupportsPrimitives.h"
#include "nsConsoleService.h"
#include "nsExceptionService.h"

#include "nsComponentManager.h"
#include "nsCategoryManagerUtils.h"
#include "nsIServiceManager.h"
#include "nsGenericFactory.h"

#include "nsThreadManager.h"
#include "nsThreadPool.h"

#include "nsIProxyObjectManager.h"
#include "nsProxyEventPrivate.h"  // access to the impl of nsProxyObjectManager for the generic factory registration.

#include "xptinfo.h"
#include "nsIInterfaceInfoManager.h"
#include "xptiprivate.h"

#include "nsTimerImpl.h"
#include "TimerThread.h"

#include "nsThread.h"
#include "nsProcess.h"
#include "nsEnvironment.h"
#include "nsVersionComparatorImpl.h"

#include "nsILocalFile.h"
#include "nsLocalFile.h"
#if defined(XP_UNIX) || defined(XP_OS2)
#include "nsNativeCharsetUtils.h"
#endif
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsCategoryManager.h"
#include "nsICategoryManager.h"
#include "nsMultiplexInputStream.h"

#include "nsStringStream.h"
extern NS_METHOD nsStringInputStreamConstructor(nsISupports *, REFNSIID, void **);
NS_DECL_CLASSINFO(nsStringInputStream)

#include "nsFastLoadService.h"

#include "nsAtomService.h"
#include "nsAtomTable.h"
#include "nsTraceRefcnt.h"
#include "nsTimelineService.h"

#include "nsHashPropertyBag.h"

#include "nsUnicharInputStream.h"
#include "nsVariant.h"

#include "nsUUIDGenerator.h"

#ifdef GC_LEAK_DETECTOR
#include "nsLeakDetector.h"
#endif
#include "nsRecyclingAllocator.h"

#include "SpecialSystemDirectory.h"

#if defined(XP_WIN) && !defined(WINCE)
#include "nsWindowsRegKey.h"
#endif

#ifdef XP_MACOSX
#include "nsMacUtilsImpl.h"
#endif

#include "nsSystemInfo.h"

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
static NS_DEFINE_CID(kINIParserFactoryCID, NS_INIPARSERFACTORY_CID);
static NS_DEFINE_CID(kSimpleUnicharStreamFactoryCID, NS_SIMPLE_UNICHAR_STREAM_FACTORY_CID);

NS_GENERIC_FACTORY_CONSTRUCTOR(nsProcess)

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
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsConsoleService, Init)
NS_DECL_CLASSINFO(nsConsoleService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAtomService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsExceptionService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTimerImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBinaryOutputStream)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBinaryInputStream)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsStorageStream)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsVersionComparatorImpl)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsVariant)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsRecyclingAllocatorImpl)

#ifdef MOZ_TIMELINE
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTimelineService)
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsHashPropertyBag, Init)

NS_GENERIC_AGGREGATED_CONSTRUCTOR_INIT(nsProperties, Init)

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsUUIDGenerator, Init)

#ifdef XP_MACOSX
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacUtilsImpl)
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsSystemInfo, Init)

static NS_METHOD
nsThreadManagerGetSingleton(nsISupports* outer,
                            const nsIID& aIID,
                            void* *aInstancePtr)
{
    NS_ASSERTION(aInstancePtr, "null outptr");
    NS_ENSURE_TRUE(!outer, NS_ERROR_NO_AGGREGATION);

    return nsThreadManager::get()->QueryInterface(aIID, aInstancePtr);
}
NS_DECL_CLASSINFO(nsThreadManager)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsThreadPool)
NS_DECL_CLASSINFO(nsThreadPool)

static NS_METHOD
nsXPTIInterfaceInfoManagerGetSingleton(nsISupports* outer,
                                       const nsIID& aIID,
                                       void* *aInstancePtr)
{
    NS_ASSERTION(aInstancePtr, "null outptr");
    NS_ENSURE_TRUE(!outer, NS_ERROR_NO_AGGREGATION);

    nsCOMPtr<nsIInterfaceInfoManager> iim
        (xptiInterfaceInfoManager::GetInterfaceInfoManagerNoAddRef());
    if (!iim)
        return NS_ERROR_FAILURE;

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
    nsCOMPtr<nsIFile> file;
    rv = nsDirectoryService::gService->Get(NS_XPCOM_CURRENT_PROCESS_DIR, 
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
    rv = nsDirectoryService::gService->Get(NS_XPCOM_COMPONENT_REGISTRY_FILE,
                                           NS_GET_IID(nsIFile),
                                           getter_AddRefs(compregFile));

    
    if (NS_FAILED(rv)) {
        NS_WARNING("Getting NS_XPCOM_COMPONENT_REGISTRY_FILE failed");
        return PR_FALSE;
    }

    if (NS_FAILED(compregFile->Exists(&exists)) || !exists)
        return PR_TRUE;

    PRInt64 compregModTime, autoregModTime;
    compregFile->GetLastModifiedTime(&compregModTime);
    file->GetLastModifiedTime(&autoregModTime);

    return LL_CMP(autoregModTime, >, compregModTime);
}


nsComponentManagerImpl* nsComponentManagerImpl::gComponentManager = NULL;
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
    COMPONENT(VERSIONCOMPARATOR, nsVersionComparatorImplConstructor),
    COMPONENT(PIPE, nsPipeConstructor),

#define NS_PROPERTIES_CLASSNAME  "Properties"
    COMPONENT(PROPERTIES, nsPropertiesConstructor),

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

#define NS_XPCOMPROXY_CID NS_PROXYEVENT_MANAGER_CID
    COMPONENT(XPCOMPROXY, nsProxyObjectManager::Create),

    COMPONENT(TIMER, nsTimerImplConstructor),

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

    COMPONENT_CI_FLAGS(THREADMANAGER, nsThreadManagerGetSingleton,
                       nsThreadManager,
                       nsIClassInfo::THREADSAFE | nsIClassInfo::SINGLETON),
    COMPONENT_CI_FLAGS(THREADPOOL, nsThreadPoolConstructor,
                       nsThreadPool, nsIClassInfo::THREADSAFE),

    COMPONENT_CI_FLAGS(STRINGINPUTSTREAM, nsStringInputStreamConstructor,
                       nsStringInputStream, nsIClassInfo::THREADSAFE),
    COMPONENT(MULTIPLEXINPUTSTREAM, nsMultiplexInputStreamConstructor),

#ifndef MOZ_NO_FAST_LOAD
    COMPONENT(FASTLOADSERVICE, nsFastLoadService::Create),
#endif

    COMPONENT(VARIANT, nsVariantConstructor),
    COMPONENT(INTERFACEINFOMANAGER_SERVICE, nsXPTIInterfaceInfoManagerGetSingleton),

    COMPONENT(RECYCLINGALLOCATOR, nsRecyclingAllocatorImplConstructor),

#define NS_HASH_PROPERTY_BAG_CLASSNAME "Hashtable Property Bag"
    COMPONENT(HASH_PROPERTY_BAG, nsHashPropertyBagConstructor),

    COMPONENT(UUID_GENERATOR, nsUUIDGeneratorConstructor),

#if defined(XP_WIN) && !defined(WINCE)
    COMPONENT(WINDOWSREGKEY, nsWindowsRegKeyConstructor),
#endif

#ifdef XP_MACOSX
    COMPONENT(MACUTILSIMPL, nsMacUtilsImplConstructor),
#endif

    COMPONENT(SYSTEMINFO, nsSystemInfoConstructor),
};

#undef COMPONENT

const int components_length = sizeof(components) / sizeof(components[0]);

// gDebug will be freed during shutdown.
static nsIDebug* gDebug = nsnull;

EXPORT_XPCOM_API(nsresult)
NS_GetDebug(nsIDebug** result)
{
    return nsDebugImpl::Create(nsnull, 
                               NS_GET_IID(nsIDebug), 
                               (void**) result);
}

EXPORT_XPCOM_API(nsresult)
NS_GetTraceRefcnt(nsITraceRefcnt** result)
{
    return nsTraceRefcntImpl::Create(nsnull, 
                                     NS_GET_IID(nsITraceRefcnt), 
                                     (void**) result);
}

EXPORT_XPCOM_API(nsresult)
NS_InitXPCOM(nsIServiceManager* *result,
                             nsIFile* binDirectory)
{
    return NS_InitXPCOM3(result, binDirectory, nsnull, nsnull, 0);
}

EXPORT_XPCOM_API(nsresult)
NS_InitXPCOM2(nsIServiceManager* *result,
                              nsIFile* binDirectory,
                              nsIDirectoryServiceProvider* appFileLocationProvider)
{
    return NS_InitXPCOM3(result, binDirectory, appFileLocationProvider, nsnull, 0);
}

EXPORT_XPCOM_API(nsresult)
NS_InitXPCOM3(nsIServiceManager* *result,
                              nsIFile* binDirectory,
                              nsIDirectoryServiceProvider* appFileLocationProvider,
                              nsStaticModuleInfo const *staticComponents,
                              PRUint32 componentCount)
{
    nsresult rv = NS_OK;

#ifdef MOZ_ENABLE_LIBXUL
    if (!staticComponents) {
        staticComponents = kPStaticModules;
        componentCount = kStaticModuleCount;
    }
#endif

     // We are not shutting down
    gXPCOMShuttingDown = PR_FALSE;

    NS_LogInit();

    // Establish the main thread here.
    rv = nsThreadManager::get()->Init();
    if (NS_FAILED(rv)) return rv;

    // Set up the timer globals/timer thread
    rv = nsTimerImpl::Startup();
    NS_ENSURE_SUCCESS(rv, rv);

#ifndef WINCE
    // If the locale hasn't already been setup by our embedder,
    // get us out of the "C" locale and into the system 
    if (strcmp(setlocale(LC_ALL, NULL), "C") == 0)
        setlocale(LC_ALL, "");
#endif

#if defined(XP_UNIX) || defined(XP_OS2)
    NS_StartupNativeCharsetUtils();
#endif
    NS_StartupLocalFile();

    StartupSpecialSystemDirectory();

    rv = nsDirectoryService::RealInit();
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIFile> xpcomLib;
            
    PRBool value;
    if (binDirectory)
    {
        rv = binDirectory->IsDirectory(&value);

        if (NS_SUCCEEDED(rv) && value) {
            nsDirectoryService::gService->Set(NS_XPCOM_INIT_CURRENT_PROCESS_DIR, binDirectory);
            binDirectory->Clone(getter_AddRefs(xpcomLib));
        }
    }
    else {
        nsDirectoryService::gService->Get(NS_XPCOM_CURRENT_PROCESS_DIR, 
                                          NS_GET_IID(nsIFile), 
                                          getter_AddRefs(xpcomLib));
    }

    if (xpcomLib) {
        xpcomLib->AppendNative(nsDependentCString(XPCOM_DLL));
        nsDirectoryService::gService->Set(NS_XPCOM_LIBRARY_FILE, xpcomLib);
    }
    
    if (appFileLocationProvider) {
        rv = nsDirectoryService::gService->RegisterProvider(appFileLocationProvider);
        if (NS_FAILED(rv)) return rv;
    }

    NS_ASSERTION(nsComponentManagerImpl::gComponentManager == NULL, "CompMgr not null at init");

    // Create the Component/Service Manager
    nsComponentManagerImpl *compMgr = new nsComponentManagerImpl();
    if (compMgr == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(compMgr);
    
    rv = compMgr->Init(staticComponents, componentCount);
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

    nsCOMPtr<nsIMemory> memory;
    NS_GetMemoryManager(getter_AddRefs(memory));
    rv = compMgr->RegisterService(kMemoryCID, memory);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->RegisterService(kComponentManagerCID, NS_STATIC_CAST(nsIComponentManager*, compMgr));
    if (NS_FAILED(rv)) return rv;

#ifdef GC_LEAK_DETECTOR
    rv = NS_InitLeakDetector();
    if (NS_FAILED(rv)) return rv;
#endif

    rv = nsCycleCollector_startup();
    if (NS_FAILED(rv)) return rv;

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

    nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(
        NS_STATIC_CAST(nsIComponentManager*,compMgr), &rv);
    if (registrar) {
        for (int i = 0; i < components_length; i++)
            RegisterGenericFactory(registrar, &components[i]);

        nsCOMPtr<nsIFactory> iniParserFactory(new nsINIParserFactory());
        if (iniParserFactory)
            registrar->RegisterFactory(kINIParserFactoryCID, 
                                       "nsINIParserFactory",
                                       NS_INIPARSERFACTORY_CONTRACTID, 
                                       iniParserFactory);

        registrar->
          RegisterFactory(kSimpleUnicharStreamFactoryCID,
                          "nsSimpleUnicharStreamFactory",
                          NS_SIMPLE_UNICHAR_STREAM_FACTORY_CONTRACTID,
                          nsSimpleUnicharStreamFactory::GetInstance());
    }

    // Pay the cost at startup time of starting this singleton.
    nsIInterfaceInfoManager* iim =
        xptiInterfaceInfoManager::GetInterfaceInfoManagerNoAddRef();

    if (CheckUpdateFile() || NS_FAILED(
        nsComponentManagerImpl::gComponentManager->ReadPersistentRegistry())) {
        // If the component registry is out of date, malformed, or incomplete,
        // autoregister the default component directories.
        (void) iim->AutoRegisterInterfaces();
        nsComponentManagerImpl::gComponentManager->AutoRegister(nsnull);
    }

    // After autoreg, but before we actually instantiate any components,
    // add any services listed in the "xpcom-directory-providers" category
    // to the directory service.
    nsDirectoryService::gService->RegisterCategoryProviders();

    // Initialize memory flusher
    nsMemoryImpl::InitFlusher();

    // Notify observers of xpcom autoregistration start
    NS_CreateServicesFromCategory(NS_XPCOM_STARTUP_OBSERVER_ID, 
                                  nsnull,
                                  NS_XPCOM_STARTUP_OBSERVER_ID);
    
    return NS_OK;
}


//
// NS_ShutdownXPCOM()
//
// The shutdown sequence for xpcom would be
//
// - Notify "xpcom-shutdown" for modules to release primary (root) references
// - Shutdown XPCOM timers
// - Notify "xpcom-shutdown-threads" for thread joins
// - Shutdown the event queues
// - Release the Global Service Manager
//   - Release all service instances held by the global service manager
//   - Release the Global Service Manager itself
// - Release the Component Manager
//   - Release all factories cached by the Component Manager
//   - Notify module loaders to shut down
//   - Unload Libraries
//   - Release Contractid Cache held by Component Manager
//   - Release dll abstraction held by Component Manager
//   - Release the Registry held by Component Manager
//   - Finally, release the component manager itself
//
EXPORT_XPCOM_API(nsresult)
NS_ShutdownXPCOM(nsIServiceManager* servMgr)
{
    NS_ENSURE_STATE(NS_IsMainThread());

    nsresult rv;
    nsCOMPtr<nsISimpleEnumerator> moduleLoaders;

    // Notify observers of xpcom shutting down
    {
        // Block it so that the COMPtr will get deleted before we hit
        // servicemanager shutdown

        nsCOMPtr<nsIThread> thread = do_GetCurrentThread();
        NS_ENSURE_STATE(thread);

        nsRefPtr<nsObserverService> observerService;
        CallGetService("@mozilla.org/observer-service;1",
                       (nsObserverService**) getter_AddRefs(observerService));

        if (observerService)
        {
            nsCOMPtr<nsIServiceManager> mgr;
            rv = NS_GetServiceManager(getter_AddRefs(mgr));
            if (NS_SUCCEEDED(rv))
            {
                (void) observerService->
                    NotifyObservers(mgr, NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                    nsnull);
            }
        }

        NS_ProcessPendingEvents(thread);

        if (observerService)
            (void) observerService->
                NotifyObservers(nsnull, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID,
                                nsnull);

        NS_ProcessPendingEvents(thread);

        // Shutdown the timer thread and all timers that might still be alive before
        // shutting down the component manager
        nsTimerImpl::Shutdown();

        NS_ProcessPendingEvents(thread);

        // Shutdown all remaining threads.  This method does not return until
        // all threads created using the thread manager (with the exception of
        // the main thread) have exited.
        nsThreadManager::get()->Shutdown();

        NS_ProcessPendingEvents(thread);

        // We save the "xpcom-shutdown-loaders" observers to notify after
        // the observerservice is gone.
        if (observerService) {
            observerService->
                EnumerateObservers(NS_XPCOM_SHUTDOWN_LOADERS_OBSERVER_ID,
                                   getter_AddRefs(moduleLoaders));

            observerService->Shutdown();
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

    nsProxyObjectManager::Shutdown();

    // Release the directory service
    NS_IF_RELEASE(nsDirectoryService::gService);

    nsCycleCollector_shutdown();

    if (moduleLoaders) {
        PRBool more;
        nsCOMPtr<nsISupports> el;
        while (NS_SUCCEEDED(moduleLoaders->HasMoreElements(&more)) &&
               more) {
            moduleLoaders->GetNext(getter_AddRefs(el));

            // Don't worry about weak-reference observers here: there is
            // no reason for weak-ref observers to register for
            // xpcom-shutdown-loaders

            nsCOMPtr<nsIObserver> obs(do_QueryInterface(el));
            if (obs)
                (void) obs->Observe(nsnull,
                                    NS_XPCOM_SHUTDOWN_LOADERS_OBSERVER_ID,
                                    nsnull);
        }

        moduleLoaders = nsnull;
    }

    // Shutdown nsLocalFile string conversion
    NS_ShutdownLocalFile();
#ifdef XP_UNIX
    NS_ShutdownNativeCharsetUtils();
#endif

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
    xptiInterfaceInfoManager::FreeInterfaceInfoManager();

    // Finally, release the component manager last because it unloads the
    // libraries:
    if (nsComponentManagerImpl::gComponentManager) {
      nsrefcnt cnt;
      NS_RELEASE2(nsComponentManagerImpl::gComponentManager, cnt);
      NS_ASSERTION(cnt == 0, "Component Manager being held past XPCOM shutdown.");
    }
    nsComponentManagerImpl::gComponentManager = nsnull;

#ifdef DEBUG
    _FreeAutoLockStatics();
#endif

    ShutdownSpecialSystemDirectory();

    NS_PurgeAtomTable();

    NS_IF_RELEASE(gDebug);

    NS_LogTerm();

#ifdef GC_LEAK_DETECTOR
    // Shutdown the Leak detector.
    NS_ShutdownLeakDetector();
#endif

    return NS_OK;
}
