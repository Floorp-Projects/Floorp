/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 ci et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "mozilla/XPCOM.h"
#include "nsXULAppAPI.h"

#include "nsXPCOMPrivate.h"
#include "nsXPCOMCIDInternal.h"

#include "nsStaticComponents.h"
#include "prlink.h"

#include "nsObserverList.h"
#include "nsObserverService.h"
#include "nsProperties.h"
#include "nsPersistentProperties.h"
#include "nsScriptableInputStream.h"
#include "nsBinaryStream.h"
#include "nsStorageStream.h"
#include "nsPipe.h"
#include "nsScriptableBase64Encoder.h"

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

#include "nsThreadManager.h"
#include "nsThreadPool.h"

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
extern nsresult nsStringInputStreamConstructor(nsISupports *, REFNSIID, void **);

#include "nsAtomService.h"
#include "nsAtomTable.h"
#include "nsTraceRefcnt.h"

#include "nsHashPropertyBag.h"

#include "nsUnicharInputStream.h"
#include "nsVariant.h"

#include "nsUUIDGenerator.h"

#include "nsIOUtil.h"

#include "SpecialSystemDirectory.h"

#if defined(XP_WIN)
#include "nsWindowsRegKey.h"
#endif

#ifdef MOZ_WIDGET_COCOA
#include "nsMacUtilsImpl.h"
#endif

#include "nsSystemInfo.h"
#include "nsMemoryReporterManager.h"

#include <locale.h>
#include "mozilla/Services.h"
#include "mozilla/FunctionTimer.h"
#include "mozilla/Omnijar.h"
#include "mozilla/HangMonitor.h"
#include "mozilla/Telemetry.h"

#include "nsChromeRegistry.h"
#include "nsChromeProtocolHandler.h"

#include "mozilla/scache/StartupCache.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop.h"

#include "mozilla/ipc/BrowserProcessSubThread.h"
#include "mozilla/MapsMemoryReporter.h"
#include "mozilla/AvailableMemoryTracker.h"
#include "mozilla/ClearOnShutdown.h"

using base::AtExitManager;
using mozilla::ipc::BrowserProcessSubThread;

namespace {

static AtExitManager* sExitManager;
static MessageLoop* sMessageLoop;
static bool sCommandLineWasInitialized;
static BrowserProcessSubThread* sIOThread;

} /* anonymous namespace */

// Registry Factory creation function defined in nsRegistry.cpp
// We hook into this function locally to create and register the registry
// Since noone outside xpcom needs to know about this and nsRegistry.cpp
// does not have a local include file, we are putting this definition
// here rather than in nsIRegistry.h
extern nsresult NS_RegistryGetFactory(nsIFactory** aFactory);
extern nsresult NS_CategoryManagerGetFactory( nsIFactory** );

#ifdef XP_WIN
extern nsresult ScheduleMediaCacheRemover();
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR(nsProcess)

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

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsConsoleService, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAtomService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsExceptionService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTimerImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBinaryOutputStream)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBinaryInputStream)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsStorageStream)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsVersionComparatorImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScriptableBase64Encoder)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsVariant)

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsHashPropertyBag, Init)

NS_GENERIC_AGGREGATED_CONSTRUCTOR_INIT(nsProperties, Init)

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsUUIDGenerator, Init)

#ifdef MOZ_WIDGET_COCOA
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacUtilsImpl)
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsSystemInfo, Init)

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMemoryReporterManager, Init)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsIOUtil)

static nsresult
nsThreadManagerGetSingleton(nsISupports* outer,
                            const nsIID& aIID,
                            void* *aInstancePtr)
{
    NS_ASSERTION(aInstancePtr, "null outptr");
    NS_ENSURE_TRUE(!outer, NS_ERROR_NO_AGGREGATION);

    return nsThreadManager::get()->QueryInterface(aIID, aInstancePtr);
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsThreadPool)

static nsresult
nsXPTIInterfaceInfoManagerGetSingleton(nsISupports* outer,
                                       const nsIID& aIID,
                                       void* *aInstancePtr)
{
    NS_ASSERTION(aInstancePtr, "null outptr");
    NS_ENSURE_TRUE(!outer, NS_ERROR_NO_AGGREGATION);

    nsCOMPtr<nsIInterfaceInfoManager> iim
        (xptiInterfaceInfoManager::GetSingleton());
    if (!iim)
        return NS_ERROR_FAILURE;

    return iim->QueryInterface(aIID, aInstancePtr);
}

nsComponentManagerImpl* nsComponentManagerImpl::gComponentManager = NULL;
bool gXPCOMShuttingDown = false;

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kINIParserFactoryCID, NS_INIPARSERFACTORY_CID);
static NS_DEFINE_CID(kSimpleUnicharStreamFactoryCID, NS_SIMPLE_UNICHAR_STREAM_FACTORY_CID);

NS_DEFINE_NAMED_CID(NS_CHROMEREGISTRY_CID);
NS_DEFINE_NAMED_CID(NS_CHROMEPROTOCOLHANDLER_CID);

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsChromeRegistry,
                                         nsChromeRegistry::GetSingleton)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsChromeProtocolHandler)

#define NS_PERSISTENTPROPERTIES_CID NS_IPERSISTENTPROPERTIES_CID /* sigh */

static already_AddRefed<nsIFactory>
CreateINIParserFactory(const mozilla::Module& module,
                       const mozilla::Module::CIDEntry& entry)
{
    nsIFactory* f = new nsINIParserFactory();
    f->AddRef();
    return f;
}

static already_AddRefed<nsIFactory>
CreateUnicharStreamFactory(const mozilla::Module& module,
                           const mozilla::Module::CIDEntry& entry)
{
    return nsSimpleUnicharStreamFactory::GetInstance();
}

#define COMPONENT(NAME, Ctor) static NS_DEFINE_CID(kNS_##NAME##_CID, NS_##NAME##_CID);
#include "XPCOMModule.inc"
#undef COMPONENT

#define COMPONENT(NAME, Ctor) { &kNS_##NAME##_CID, false, NULL, Ctor },
const mozilla::Module::CIDEntry kXPCOMCIDEntries[] = {
    { &kComponentManagerCID, true, NULL, nsComponentManagerImpl::Create },
    { &kINIParserFactoryCID, false, CreateINIParserFactory },
    { &kSimpleUnicharStreamFactoryCID, false, CreateUnicharStreamFactory },
#include "XPCOMModule.inc"
    { &kNS_CHROMEREGISTRY_CID, false, NULL, nsChromeRegistryConstructor },
    { &kNS_CHROMEPROTOCOLHANDLER_CID, false, NULL, nsChromeProtocolHandlerConstructor },
    { NULL }
};
#undef COMPONENT

#define COMPONENT(NAME, Ctor) { NS_##NAME##_CONTRACTID, &kNS_##NAME##_CID },
const mozilla::Module::ContractIDEntry kXPCOMContracts[] = {
#include "XPCOMModule.inc"
    { NS_CHROMEREGISTRY_CONTRACTID, &kNS_CHROMEREGISTRY_CID },
    { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "chrome", &kNS_CHROMEPROTOCOLHANDLER_CID },
    { NS_INIPARSERFACTORY_CONTRACTID, &kINIParserFactoryCID },
    { NULL }
};
#undef COMPONENT

const mozilla::Module kXPCOMModule = { mozilla::Module::kVersion, kXPCOMCIDEntries, kXPCOMContracts };

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
    return NS_InitXPCOM2(result, binDirectory, nsnull);
}

EXPORT_XPCOM_API(nsresult)
NS_InitXPCOM2(nsIServiceManager* *result,
              nsIFile* binDirectory,
              nsIDirectoryServiceProvider* appFileLocationProvider)
{
    NS_TIME_FUNCTION;

    nsresult rv = NS_OK;

     // We are not shutting down
    gXPCOMShuttingDown = false;

    NS_TIME_FUNCTION_MARK("Next: AvailableMemoryTracker Init()");

    // Initialize the available memory tracker before other threads have had a
    // chance to start up, because the initialization is not thread-safe.
    mozilla::AvailableMemoryTracker::Init();

    NS_TIME_FUNCTION_MARK("Next: log init");

    NS_LogInit();

    NS_TIME_FUNCTION_MARK("Next: IPC init");

    // Set up chromium libs
    NS_ASSERTION(!sExitManager && !sMessageLoop, "Bad logic!");

    if (!AtExitManager::AlreadyRegistered()) {
        sExitManager = new AtExitManager();
        NS_ENSURE_STATE(sExitManager);
    }

    if (!MessageLoop::current()) {
        sMessageLoop = new MessageLoopForUI(MessageLoop::TYPE_MOZILLA_UI);
        NS_ENSURE_STATE(sMessageLoop);
    }

    if (XRE_GetProcessType() == GeckoProcessType_Default &&
        !BrowserProcessSubThread::GetMessageLoop(BrowserProcessSubThread::IO)) {
        scoped_ptr<BrowserProcessSubThread> ioThread(
            new BrowserProcessSubThread(BrowserProcessSubThread::IO));
        NS_ENSURE_TRUE(ioThread.get(), NS_ERROR_OUT_OF_MEMORY);

        base::Thread::Options options;
        options.message_loop_type = MessageLoop::TYPE_IO;
        NS_ENSURE_TRUE(ioThread->StartWithOptions(options), NS_ERROR_FAILURE);

        sIOThread = ioThread.release();
    }

    NS_TIME_FUNCTION_MARK("Next: thread manager init");

    // Establish the main thread here.
    rv = nsThreadManager::get()->Init();
    if (NS_FAILED(rv)) return rv;

    NS_TIME_FUNCTION_MARK("Next: timer startup");

    // Set up the timer globals/timer thread
    rv = nsTimerImpl::Startup();
    NS_ENSURE_SUCCESS(rv, rv);

#ifndef ANDROID
    NS_TIME_FUNCTION_MARK("Next: setlocale");

    // If the locale hasn't already been setup by our embedder,
    // get us out of the "C" locale and into the system 
    if (strcmp(setlocale(LC_ALL, NULL), "C") == 0)
        setlocale(LC_ALL, "");
#endif

#if defined(XP_UNIX) || defined(XP_OS2)
    NS_TIME_FUNCTION_MARK("Next: startup native charset utils");

    NS_StartupNativeCharsetUtils();
#endif

    NS_TIME_FUNCTION_MARK("Next: startup local file");

    NS_StartupLocalFile();

    StartupSpecialSystemDirectory();

    rv = nsDirectoryService::RealInit();
    if (NS_FAILED(rv))
        return rv;

    bool value;

    if (binDirectory)
    {
        rv = binDirectory->IsDirectory(&value);

        if (NS_SUCCEEDED(rv) && value) {
            nsDirectoryService::gService->Set(NS_XPCOM_INIT_CURRENT_PROCESS_DIR, binDirectory);
        }
    }

    if (appFileLocationProvider) {
        rv = nsDirectoryService::gService->RegisterProvider(appFileLocationProvider);
        if (NS_FAILED(rv)) return rv;
    }

    nsCOMPtr<nsIFile> xpcomLib;

    nsDirectoryService::gService->Get(NS_GRE_DIR,
                                      NS_GET_IID(nsIFile),
                                      getter_AddRefs(xpcomLib));

    if (xpcomLib) {
        xpcomLib->AppendNative(nsDependentCString(XPCOM_DLL));
        nsDirectoryService::gService->Set(NS_XPCOM_LIBRARY_FILE, xpcomLib);
    }
    
    NS_TIME_FUNCTION_MARK("Next: Omnijar init");

    if (!mozilla::Omnijar::IsInitialized()) {
        mozilla::Omnijar::Init();
    }

    if ((sCommandLineWasInitialized = !CommandLine::IsInitialized())) {
        NS_TIME_FUNCTION_MARK("Next: IPC command line init");

#ifdef OS_WIN
        CommandLine::Init(0, nsnull);
#else
        nsCOMPtr<nsIFile> binaryFile;
        nsDirectoryService::gService->Get(NS_XPCOM_CURRENT_PROCESS_DIR, 
                                          NS_GET_IID(nsIFile), 
                                          getter_AddRefs(binaryFile));
        NS_ENSURE_STATE(binaryFile);
        
        rv = binaryFile->AppendNative(NS_LITERAL_CSTRING("nonexistent-executable"));
        NS_ENSURE_SUCCESS(rv, rv);
        
        nsCString binaryPath;
        rv = binaryFile->GetNativePath(binaryPath);
        NS_ENSURE_SUCCESS(rv, rv);
        
        static char const *const argv = { strdup(binaryPath.get()) };
        CommandLine::Init(1, &argv);
#endif
    }

    NS_ASSERTION(nsComponentManagerImpl::gComponentManager == NULL, "CompMgr not null at init");

    NS_TIME_FUNCTION_MARK("Next: component manager init");

    // Create the Component/Service Manager
    nsComponentManagerImpl::gComponentManager = new nsComponentManagerImpl();
    NS_ADDREF(nsComponentManagerImpl::gComponentManager);
    
    rv = nsCycleCollector_startup();
    if (NS_FAILED(rv)) return rv;

    rv = nsComponentManagerImpl::gComponentManager->Init();
    if (NS_FAILED(rv))
    {
        NS_RELEASE(nsComponentManagerImpl::gComponentManager);
        return rv;
    }

    if (result) {
        NS_ADDREF(*result = nsComponentManagerImpl::gComponentManager);
    }

    NS_TIME_FUNCTION_MARK("Next: cycle collector startup");

    NS_TIME_FUNCTION_MARK("Next: interface info manager init");

    // The iimanager constructor searches and registers XPT files.
    // (We trigger the singleton's lazy construction here to make that happen.)
    (void) xptiInterfaceInfoManager::GetSingleton();

    NS_TIME_FUNCTION_MARK("Next: register category providers");

    // After autoreg, but before we actually instantiate any components,
    // add any services listed in the "xpcom-directory-providers" category
    // to the directory service.
    nsDirectoryService::gService->RegisterCategoryProviders();

    mozilla::scache::StartupCache::GetSingleton();
    mozilla::AvailableMemoryTracker::Activate();

    NS_TIME_FUNCTION_MARK("Next: create services from category");

    // Notify observers of xpcom autoregistration start
    NS_CreateServicesFromCategory(NS_XPCOM_STARTUP_CATEGORY, 
                                  nsnull,
                                  NS_XPCOM_STARTUP_OBSERVER_ID);
#ifdef XP_WIN
    ScheduleMediaCacheRemover();
#endif

    mozilla::MapsMemoryReporter::Init();

    mozilla::HangMonitor::Startup();

    mozilla::Telemetry::Init();

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
    return mozilla::ShutdownXPCOM(servMgr);
}

namespace mozilla {

nsresult
ShutdownXPCOM(nsIServiceManager* servMgr)
{
    // Make sure the hang monitor is enabled for shutdown.
    HangMonitor::NotifyActivity();

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
            (void) observerService->
                NotifyObservers(nsnull, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID,
                                nsnull);

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
        mozilla::scache::StartupCache::DeleteSingleton();
        if (observerService)
            (void) observerService->
                NotifyObservers(nsnull, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID,
                                nsnull);

        nsCycleCollector_shutdownThreads();

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

        HangMonitor::NotifyActivity();

        // We save the "xpcom-shutdown-loaders" observers to notify after
        // the observerservice is gone.
        if (observerService) {
            observerService->
                EnumerateObservers(NS_XPCOM_SHUTDOWN_LOADERS_OBSERVER_ID,
                                   getter_AddRefs(moduleLoaders));

            observerService->Shutdown();
        }
    }

    // Free ClearOnShutdown()'ed smart pointers.  This needs to happen *after*
    // we've finished notifying observers of XPCOM shutdown, because shutdown
    // observers themselves might call ClearOnShutdown().
    mozilla::KillClearOnShutdown();

    // XPCOM is officially in shutdown mode NOW
    // Set this only after the observers have been notified as this
    // will cause servicemanager to become inaccessible.
    mozilla::services::Shutdown();

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

    // Release the directory service
    NS_IF_RELEASE(nsDirectoryService::gService);

    nsCycleCollector_shutdown();

    if (moduleLoaders) {
        bool more;
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
    nsCategoryManager::Destroy();

    NS_PurgeAtomTable();

    NS_IF_RELEASE(gDebug);

    if (sIOThread) {
        delete sIOThread;
        sIOThread = nsnull;
    }
    if (sMessageLoop) {
        delete sMessageLoop;
        sMessageLoop = nsnull;
    }
    if (sCommandLineWasInitialized) {
        CommandLine::Terminate();
        sCommandLineWasInitialized = false;
    }
    if (sExitManager) {
        delete sExitManager;
        sExitManager = nsnull;
    }

    Omnijar::CleanUp();

    HangMonitor::Shutdown();

    NS_LogTerm();

    return NS_OK;
}

} // namespace mozilla
