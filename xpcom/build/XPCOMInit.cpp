/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XPCOMModule.h"

#include "base/basictypes.h"

#include "mozilla/Atomics.h"
#include "mozilla/Poison.h"
#include "mozilla/RemoteDecoderManagerChild.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/XPCOM.h"
#include "mozJSComponentLoader.h"
#include "nsXULAppAPI.h"

#ifndef ANDROID
#  include "nsTerminator.h"
#endif

#include "nsXPCOMPrivate.h"
#include "nsXPCOMCIDInternal.h"

#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/CompositorBridgeParent.h"

#include "prlink.h"

#include "nsCycleCollector.h"
#include "nsObserverService.h"

#include "nsDebugImpl.h"
#include "nsSystemInfo.h"

#include "nsINIParserImpl.h"

#include "nsComponentManager.h"
#include "nsCategoryManagerUtils.h"
#include "nsIServiceManager.h"

#include "nsThreadManager.h"
#include "nsThreadPool.h"

#include "nsTimerImpl.h"
#include "TimerThread.h"

#include "nsThread.h"
#include "nsVersionComparatorImpl.h"

#include "nsIFile.h"
#include "nsLocalFile.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsCategoryManager.h"
#include "nsICategoryManager.h"
#include "nsMultiplexInputStream.h"

#include "nsAtomTable.h"
#include "nsISupportsImpl.h"

#include "nsSystemInfo.h"
#include "nsMemoryReporterManager.h"
#include "nsMessageLoop.h"
#include "nss.h"
#include "ssl.h"

#include <locale.h>
#include "mozilla/Services.h"
#include "mozilla/Omnijar.h"
#include "mozilla/ScriptPreloader.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/Telemetry.h"
#include "mozilla/BackgroundHangMonitor.h"

#include "mozilla/PoisonIOInterposer.h"
#include "mozilla/LateWriteChecks.h"

#include "mozilla/scache/StartupCache.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop.h"

#include "mozilla/ipc/BrowserProcessSubThread.h"
#include "mozilla/AvailableMemoryTracker.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/CountingAllocatorBase.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/ServoStyleConsts.h"

#include "mozilla/ipc/GeckoChildProcessHost.h"

#include "ogg/ogg.h"

#include "GeckoProfiler.h"

#include "jsapi.h"
#include "js/Initialization.h"

#include "gfxPlatform.h"

using namespace mozilla;
using base::AtExitManager;
using mozilla::ipc::BrowserProcessSubThread;

// From toolkit/library/rust/lib.rs
extern "C" void GkRust_Init();
extern "C" void GkRust_Shutdown();

namespace {

static AtExitManager* sExitManager;
static MessageLoop* sMessageLoop;
static bool sCommandLineWasInitialized;
static BrowserProcessSubThread* sIOThread;
static BackgroundHangMonitor* sMainHangMonitor;

} /* anonymous namespace */

// Registry Factory creation function defined in nsRegistry.cpp
// We hook into this function locally to create and register the registry
// Since noone outside xpcom needs to know about this and nsRegistry.cpp
// does not have a local include file, we are putting this definition
// here rather than in nsIRegistry.h
extern nsresult NS_RegistryGetFactory(nsIFactory** aFactory);
extern nsresult NS_CategoryManagerGetFactory(nsIFactory**);

#ifdef XP_WIN
extern nsresult CreateAnonTempFileRemover();
#endif

nsresult nsThreadManagerGetSingleton(nsISupports* aOuter, const nsIID& aIID,
                                     void** aInstancePtr) {
  NS_ASSERTION(aInstancePtr, "null outptr");
  if (NS_WARN_IF(aOuter)) {
    return NS_ERROR_NO_AGGREGATION;
  }

  return nsThreadManager::get().QueryInterface(aIID, aInstancePtr);
}

nsresult nsLocalFileConstructor(nsISupports* aOuter, const nsIID& aIID,
                                void** aInstancePtr) {
  return nsLocalFile::nsLocalFileConstructor(aOuter, aIID, aInstancePtr);
}

nsComponentManagerImpl* nsComponentManagerImpl::gComponentManager = nullptr;
bool gXPCOMShuttingDown = false;
bool gXPCOMThreadsShutDown = false;
char16_t* gGREBinPath = nullptr;

static NS_DEFINE_CID(kINIParserFactoryCID, NS_INIPARSERFACTORY_CID);

static already_AddRefed<nsIFactory> CreateINIParserFactory(
    const mozilla::Module& aModule, const mozilla::Module::CIDEntry& aEntry) {
  nsCOMPtr<nsIFactory> f = new nsINIParserFactory();
  return f.forget();
}

const mozilla::Module::CIDEntry kXPCOMCIDEntries[] = {
    {&kINIParserFactoryCID, false, CreateINIParserFactory}, {nullptr}};

const mozilla::Module::ContractIDEntry kXPCOMContracts[] = {
    {NS_INIPARSERFACTORY_CONTRACTID, &kINIParserFactoryCID}, {nullptr}};

const mozilla::Module kXPCOMModule = {
    mozilla::Module::kVersion,
    kXPCOMCIDEntries,
    kXPCOMContracts,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    Module::ALLOW_IN_GPU_RDD_VR_AND_SOCKET_PROCESS};

// gDebug will be freed during shutdown.
static nsIDebug2* gDebug = nullptr;

EXPORT_XPCOM_API(nsresult)
NS_GetDebug(nsIDebug2** aResult) {
  return nsDebugImpl::Create(nullptr, NS_GET_IID(nsIDebug2), (void**)aResult);
}

class ICUReporter final : public nsIMemoryReporter,
                          public CountingAllocatorBase<ICUReporter> {
 public:
  NS_DECL_ISUPPORTS

  static void* Alloc(const void*, size_t aSize) {
    return CountingMalloc(aSize);
  }

  static void* Realloc(const void*, void* aPtr, size_t aSize) {
    return CountingRealloc(aPtr, aSize);
  }

  static void Free(const void*, void* aPtr) { return CountingFree(aPtr); }

 private:
  NS_IMETHOD
  CollectReports(nsIHandleReportCallback* aHandleReport, nsISupports* aData,
                 bool aAnonymize) override {
    MOZ_COLLECT_REPORT(
        "explicit/icu", KIND_HEAP, UNITS_BYTES, MemoryAllocated(),
        "Memory used by ICU, a Unicode and globalization support library.");

    return NS_OK;
  }

  ~ICUReporter() {}
};

NS_IMPL_ISUPPORTS(ICUReporter, nsIMemoryReporter)

/* static */ template <>
CountingAllocatorBase<ICUReporter>::AmountType
    CountingAllocatorBase<ICUReporter>::sAmount(0);

class OggReporter final : public nsIMemoryReporter,
                          public CountingAllocatorBase<OggReporter> {
 public:
  NS_DECL_ISUPPORTS

 private:
  NS_IMETHOD
  CollectReports(nsIHandleReportCallback* aHandleReport, nsISupports* aData,
                 bool aAnonymize) override {
    MOZ_COLLECT_REPORT(
        "explicit/media/libogg", KIND_HEAP, UNITS_BYTES, MemoryAllocated(),
        "Memory allocated through libogg for Ogg, Theora, and related media "
        "files.");

    return NS_OK;
  }

  ~OggReporter() {}
};

NS_IMPL_ISUPPORTS(OggReporter, nsIMemoryReporter)

/* static */ template <>
CountingAllocatorBase<OggReporter>::AmountType
    CountingAllocatorBase<OggReporter>::sAmount(0);

static bool sInitializedJS = false;

// Note that on OSX, aBinDirectory will point to .app/Contents/Resources/browser
EXPORT_XPCOM_API(nsresult)
NS_InitXPCOM(nsIServiceManager** aResult, nsIFile* aBinDirectory,
             nsIDirectoryServiceProvider* aAppFileLocationProvider) {
  static bool sInitialized = false;
  if (sInitialized) {
    return NS_ERROR_FAILURE;
  }

  sInitialized = true;

  mozPoisonValueInit();

  NS_LogInit();

  NS_InitAtomTable();

  // We don't have the arguments by hand here.  If logging has already been
  // initialized by a previous call to LogModule::Init with the arguments
  // passed, passing (0, nullptr) is alright here.
  mozilla::LogModule::Init(0, nullptr);

  GkRust_Init();

  nsresult rv = NS_OK;

  // We are not shutting down
  gXPCOMShuttingDown = false;

#ifdef XP_UNIX
  // Discover the current value of the umask, and save it where
  // nsSystemInfo::Init can retrieve it when necessary.  There is no way
  // to read the umask without changing it, and the setting is process-
  // global, so this must be done while we are still single-threaded; the
  // nsSystemInfo object is typically created much later, when some piece
  // of chrome JS wants it.  The system call is specified as unable to fail.
  nsSystemInfo::gUserUmask = ::umask(0777);
  ::umask(nsSystemInfo::gUserUmask);
#endif

  // Set up chromium libs
  NS_ASSERTION(!sExitManager && !sMessageLoop, "Bad logic!");

  if (!AtExitManager::AlreadyRegistered()) {
    sExitManager = new AtExitManager();
  }

  MessageLoop* messageLoop = MessageLoop::current();
  if (!messageLoop) {
    sMessageLoop = new MessageLoopForUI(MessageLoop::TYPE_MOZILLA_PARENT);
    sMessageLoop->set_thread_name("Gecko");
    // Set experimental values for main thread hangs:
    // 128ms for transient hangs and 8192ms for permanent hangs
    sMessageLoop->set_hang_timeouts(128, 8192);
  } else if (messageLoop->type() == MessageLoop::TYPE_MOZILLA_CHILD) {
    messageLoop->set_thread_name("Gecko_Child");
    messageLoop->set_hang_timeouts(128, 8192);
  }

  if (XRE_IsParentProcess() &&
      !BrowserProcessSubThread::GetMessageLoop(BrowserProcessSubThread::IO)) {
    UniquePtr<BrowserProcessSubThread> ioThread =
        MakeUnique<BrowserProcessSubThread>(BrowserProcessSubThread::IO);

    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_IO;
    if (NS_WARN_IF(!ioThread->StartWithOptions(options))) {
      return NS_ERROR_FAILURE;
    }

    sIOThread = ioThread.release();
  }

  // Establish the main thread here.
  rv = nsThreadManager::get().Init();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Init the SystemGroup for dispatching main thread runnables.
  SystemGroup::InitStatic();

  // Set up the timer globals/timer thread
  rv = nsTimerImpl::Startup();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#ifndef ANDROID
  // If the locale hasn't already been setup by our embedder,
  // get us out of the "C" locale and into the system
  if (strcmp(setlocale(LC_ALL, nullptr), "C") == 0) {
    setlocale(LC_ALL, "");
  }
#endif

  nsDirectoryService::RealInit();

  bool value;

  if (aBinDirectory) {
    rv = aBinDirectory->IsDirectory(&value);

    if (NS_SUCCEEDED(rv) && value) {
      nsDirectoryService::gService->SetCurrentProcessDirectory(aBinDirectory);
    }
  }

  if (aAppFileLocationProvider) {
    rv = nsDirectoryService::gService->RegisterProvider(
        aAppFileLocationProvider);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  nsCOMPtr<nsIFile> xpcomLib;
  nsDirectoryService::gService->Get(NS_GRE_BIN_DIR, NS_GET_IID(nsIFile),
                                    getter_AddRefs(xpcomLib));
  MOZ_ASSERT(xpcomLib);

  // set gGREBinPath
  nsAutoString path;
  xpcomLib->GetPath(path);
  gGREBinPath = ToNewUnicode(path);

  xpcomLib->AppendNative(nsDependentCString(XPCOM_DLL));
  nsDirectoryService::gService->Set(NS_XPCOM_LIBRARY_FILE, xpcomLib);

  if (!mozilla::Omnijar::IsInitialized()) {
    mozilla::Omnijar::Init();
  }

  if ((sCommandLineWasInitialized = !CommandLine::IsInitialized())) {
#ifdef OS_WIN
    CommandLine::Init(0, nullptr);
#else
    nsCOMPtr<nsIFile> binaryFile;
    nsDirectoryService::gService->Get(NS_XPCOM_CURRENT_PROCESS_DIR,
                                      NS_GET_IID(nsIFile),
                                      getter_AddRefs(binaryFile));
    if (NS_WARN_IF(!binaryFile)) {
      return NS_ERROR_FAILURE;
    }

    rv = binaryFile->AppendNative(NS_LITERAL_CSTRING("nonexistent-executable"));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCString binaryPath;
    rv = binaryFile->GetNativePath(binaryPath);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    static char const* const argv = {strdup(binaryPath.get())};
    CommandLine::Init(1, &argv);
#endif
  }

  NS_ASSERTION(nsComponentManagerImpl::gComponentManager == nullptr,
               "CompMgr not null at init");

  // Create the Component/Service Manager
  nsComponentManagerImpl::gComponentManager = new nsComponentManagerImpl();
  NS_ADDREF(nsComponentManagerImpl::gComponentManager);

  // Global cycle collector initialization.
  if (!nsCycleCollector_init()) {
    return NS_ERROR_UNEXPECTED;
  }

  // And start it up for this thread too.
  nsCycleCollector_startup();

  // Register ICU memory functions.  This really shouldn't be necessary: the
  // JS engine should do this on its own inside JS_Init, and memory-reporting
  // code should call a JSAPI function to observe ICU memory usage.  But we
  // can't define the alloc/free functions in the JS engine, because it can't
  // depend on the XPCOM-based memory reporting goop.  So for now, we have
  // this oddness.
  mozilla::SetICUMemoryFunctions();

  // Do the same for libogg.
  ogg_set_mem_functions(
      OggReporter::CountingMalloc, OggReporter::CountingCalloc,
      OggReporter::CountingRealloc, OggReporter::CountingFree);

  // Initialize the JS engine.
  const char* jsInitFailureReason = JS_InitWithFailureDiagnostic();
  if (jsInitFailureReason) {
    MOZ_CRASH_UNSAFE(jsInitFailureReason);
  }
  sInitializedJS = true;

  rv = nsComponentManagerImpl::gComponentManager->Init();
  if (NS_FAILED(rv)) {
    NS_RELEASE(nsComponentManagerImpl::gComponentManager);
    return rv;
  }

  if (aResult) {
    NS_ADDREF(*aResult = nsComponentManagerImpl::gComponentManager);
  }

  // After autoreg, but before we actually instantiate any components,
  // add any services listed in the "xpcom-directory-providers" category
  // to the directory service.
  nsDirectoryService::gService->RegisterCategoryProviders();

  // Init SharedThreadPool (which needs the service manager).
  SharedThreadPool::InitStatics();

  mozilla::ScriptPreloader::GetSingleton();
  mozilla::scache::StartupCache::GetSingleton();
  mozilla::AvailableMemoryTracker::Init();

  // Notify observers of xpcom autoregistration start
  NS_CreateServicesFromCategory(NS_XPCOM_STARTUP_CATEGORY, nullptr,
                                NS_XPCOM_STARTUP_OBSERVER_ID);
#ifdef XP_WIN
  CreateAnonTempFileRemover();
#endif

  // The memory reporter manager is up and running -- register our reporters.
  RegisterStrongMemoryReporter(new ICUReporter());
  RegisterStrongMemoryReporter(new OggReporter());

  mozilla::Telemetry::Init();

  mozilla::BackgroundHangMonitor::Startup();

  const MessageLoop* const loop = MessageLoop::current();
  sMainHangMonitor = new mozilla::BackgroundHangMonitor(
      loop->thread_name().c_str(), loop->transient_hang_timeout(),
      loop->permanent_hang_timeout());

  return NS_OK;
}

EXPORT_XPCOM_API(nsresult)
NS_InitMinimalXPCOM() {
  mozPoisonValueInit();
  NS_SetMainThread();
  mozilla::TimeStamp::Startup();
  NS_LogInit();
  NS_InitAtomTable();

  // We don't have the arguments by hand here.  If logging has already been
  // initialized by a previous call to LogModule::Init with the arguments
  // passed, passing (0, nullptr) is alright here.
  mozilla::LogModule::Init(0, nullptr);

  GkRust_Init();

  nsresult rv = nsThreadManager::get().Init();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Init the SystemGroup for dispatching main thread runnables.
  SystemGroup::InitStatic();

  // Set up the timer globals/timer thread.
  rv = nsTimerImpl::Startup();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Create the Component/Service Manager
  nsComponentManagerImpl::gComponentManager = new nsComponentManagerImpl();
  NS_ADDREF(nsComponentManagerImpl::gComponentManager);

  rv = nsComponentManagerImpl::gComponentManager->Init();
  if (NS_FAILED(rv)) {
    NS_RELEASE(nsComponentManagerImpl::gComponentManager);
    return rv;
  }

  // Global cycle collector initialization.
  if (!nsCycleCollector_init()) {
    return NS_ERROR_UNEXPECTED;
  }

  SharedThreadPool::InitStatics();
  mozilla::Telemetry::Init();
  mozilla::BackgroundHangMonitor::Startup();

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
NS_ShutdownXPCOM(nsIServiceManager* aServMgr) {
  return mozilla::ShutdownXPCOM(aServMgr);
}

namespace mozilla {

void SetICUMemoryFunctions() {
  static bool sICUReporterInitialized = false;
  if (!sICUReporterInitialized) {
    if (!JS_SetICUMemoryFunctions(ICUReporter::Alloc, ICUReporter::Realloc,
                                  ICUReporter::Free)) {
      MOZ_CRASH("JS_SetICUMemoryFunctions failed.");
    }
    sICUReporterInitialized = true;
  }
}

nsresult ShutdownXPCOM(nsIServiceManager* aServMgr) {
  // Make sure the hang monitor is enabled for shutdown.
  BackgroundHangMonitor().NotifyActivity();

  if (!NS_IsMainThread()) {
    MOZ_CRASH("Shutdown on wrong thread");
  }

  nsresult rv;

  // Notify observers of xpcom shutting down
  {
    // Block it so that the COMPtr will get deleted before we hit
    // servicemanager shutdown

    nsCOMPtr<nsIThread> thread = do_GetCurrentThread();
    if (NS_WARN_IF(!thread)) {
      return NS_ERROR_UNEXPECTED;
    }

    RefPtr<nsObserverService> observerService;
    CallGetService("@mozilla.org/observer-service;1",
                   (nsObserverService**)getter_AddRefs(observerService));

    if (observerService) {
      mozilla::KillClearOnShutdown(ShutdownPhase::WillShutdown);
      observerService->NotifyObservers(
          nullptr, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID, nullptr);

      nsCOMPtr<nsIServiceManager> mgr;
      rv = NS_GetServiceManager(getter_AddRefs(mgr));
      if (NS_SUCCEEDED(rv)) {
        mozilla::KillClearOnShutdown(ShutdownPhase::Shutdown);
        observerService->NotifyObservers(mgr, NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                         nullptr);
      }

#ifndef ANDROID
      mozilla::XPCOMShutdownNotified();
#endif
    }

    // This must happen after the shutdown of media and widgets, which
    // are triggered by the NS_XPCOM_SHUTDOWN_OBSERVER_ID notification.
    NS_ProcessPendingEvents(thread);
    gfxPlatform::ShutdownLayersIPC();
    mozilla::RemoteDecoderManagerChild::Shutdown();

    mozilla::scache::StartupCache::DeleteSingleton();
    if (observerService) {
      mozilla::KillClearOnShutdown(ShutdownPhase::ShutdownThreads);
      observerService->NotifyObservers(
          nullptr, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID, nullptr);
    }

    gXPCOMThreadsShutDown = true;
    NS_ProcessPendingEvents(thread);

    // Shutdown the timer thread and all timers that might still be alive before
    // shutting down the component manager
    nsTimerImpl::Shutdown();

    NS_ProcessPendingEvents(thread);

    // Shutdown all remaining threads.  This method does not return until
    // all threads created using the thread manager (with the exception of
    // the main thread) have exited.
    nsThreadManager::get().Shutdown();

    NS_ProcessPendingEvents(thread);

    BackgroundHangMonitor().NotifyActivity();

    // Late-write checks needs to find the profile directory, so it has to
    // be initialized before mozilla::services::Shutdown or (because of
    // xpcshell tests replacing the service) modules being unloaded.
    mozilla::InitLateWriteChecks();

    if (observerService) {
      mozilla::KillClearOnShutdown(ShutdownPhase::ShutdownLoaders);
      observerService->Shutdown();
    }
  }

  // Free ClearOnShutdown()'ed smart pointers.  This needs to happen *after*
  // we've finished notifying observers of XPCOM shutdown, because shutdown
  // observers themselves might call ClearOnShutdown().
  mozilla::KillClearOnShutdown(ShutdownPhase::ShutdownFinal);

  // XPCOM is officially in shutdown mode NOW
  // Set this only after the observers have been notified as this
  // will cause servicemanager to become inaccessible.
  mozilla::services::Shutdown();

  // We may have AddRef'd for the caller of NS_InitXPCOM, so release it
  // here again:
  NS_IF_RELEASE(aServMgr);

  // Shutdown global servicemanager
  if (nsComponentManagerImpl::gComponentManager) {
    nsComponentManagerImpl::gComponentManager->FreeServices();
  }

  // Release the directory service
  nsDirectoryService::gService = nullptr;

  free(gGREBinPath);
  gGREBinPath = nullptr;

  // FIXME: This can cause harmless writes from sqlite committing
  // log files. We have to ignore them before we can move
  // the mozilla::PoisonWrite call before this point. See bug
  // 834945 for the details.
  mozJSComponentLoader::Unload();

  // Clear the profiler's JS context before cycle collection. The profiler will
  // notify the JS engine that it can let go of any data it's holding on to for
  // profiling purposes.
  PROFILER_CLEAR_JS_CONTEXT();

  bool shutdownCollect;
#ifdef NS_FREE_PERMANENT_DATA
  shutdownCollect = true;
#else
  shutdownCollect = !!PR_GetEnv("MOZ_CC_RUN_DURING_SHUTDOWN");
#endif
  nsCycleCollector_shutdown(shutdownCollect);

  // There can be code trying to refer to global objects during the final cc
  // shutdown. This is the phase for such global objects to correctly release.
  mozilla::KillClearOnShutdown(ShutdownPhase::ShutdownPostLastCycleCollection);

  PROFILER_ADD_MARKER("Shutdown xpcom", OTHER);
  // If we are doing any shutdown checks, poison writes.
  if (gShutdownChecks != SCM_NOTHING) {
#ifdef XP_MACOSX
    mozilla::OnlyReportDirtyWrites();
#endif /* XP_MACOSX */
    mozilla::BeginLateWriteChecks();
  }

  // Shutdown xpcom. This will release all loaders and cause others holding
  // a refcount to the component manager to release it.
  if (nsComponentManagerImpl::gComponentManager) {
    rv = (nsComponentManagerImpl::gComponentManager)->Shutdown();
    NS_ASSERTION(NS_SUCCEEDED(rv), "Component Manager shutdown failed.");
  } else {
    NS_WARNING("Component Manager was never created ...");
  }

  if (sInitializedJS) {
    // Shut down the JS engine.
    JS_ShutDown();
    sInitializedJS = false;
  }

  // After all threads have been joined and the component manager has been shut
  // down, any remaining objects that could be holding NSS resources (should)
  // have been released, so we can safely shut down NSS.
  if (NSS_IsInitialized()) {
    SSL_ClearSessionCache();
    if (NSS_Shutdown() != SECSuccess) {
      // If you're seeing this crash and/or warning, some NSS resources are
      // still in use (see bugs 1417680 and 1230312). Set the environment
      // variable 'MOZ_IGNORE_NSS_SHUTDOWN_LEAKS' to some value to ignore this.
#if defined(DEBUG) && !defined(ANDROID)
      if (!getenv("MOZ_IGNORE_NSS_SHUTDOWN_LEAKS")) {
        MOZ_CRASH("NSS_Shutdown failed");
      } else {
        NS_WARNING("NSS_Shutdown failed");
      }
#else
      NS_WARNING("NSS_Shutdown failed");
#endif
    }
  }

  // Finally, release the component manager last because it unloads the
  // libraries:
  if (nsComponentManagerImpl::gComponentManager) {
    nsrefcnt cnt;
    NS_RELEASE2(nsComponentManagerImpl::gComponentManager, cnt);
    NS_ASSERTION(cnt == 0, "Component Manager being held past XPCOM shutdown.");
  }
  nsComponentManagerImpl::gComponentManager = nullptr;
  nsCategoryManager::Destroy();

  // Shut down SystemGroup for main thread dispatching.
  SystemGroup::Shutdown();

  GkRust_Shutdown();

#ifdef NS_FREE_PERMANENT_DATA
  // By the time we're shutting down, there may still be async parse tasks going
  // on in the Servo thread-pool. This is fairly uncommon, though not
  // impossible. CSS parsing heavily uses the atom table, so obviously it's not
  // fine to get rid of it.
  //
  // In leak-checking / ASAN / etc. builds, shut down the servo thread-pool,
  // which will wait for all the work to be done. For other builds, we don't
  // really want to wait on shutdown for possibly slow tasks. So just leak the
  // atom table in those.
  Servo_ShutdownThreadPool();
  NS_ShutdownAtomTable();
#endif

  NS_IF_RELEASE(gDebug);

  delete sIOThread;
  sIOThread = nullptr;

  delete sMessageLoop;
  sMessageLoop = nullptr;

  if (sCommandLineWasInitialized) {
    CommandLine::Terminate();
    sCommandLineWasInitialized = false;
  }

  delete sExitManager;
  sExitManager = nullptr;

  Omnijar::CleanUp();

  BackgroundHangMonitor::Shutdown();

  delete sMainHangMonitor;
  sMainHangMonitor = nullptr;

  NS_LogTerm();

  return NS_OK;
}

}  // namespace mozilla
