/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "mozilla/AbstractThread.h"
#include "mozilla/Atomics.h"
#include "mozilla/Poison.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/XPCOM.h"
#include "nsXULAppAPI.h"

#include "nsXPCOMPrivate.h"
#include "nsXPCOMCIDInternal.h"

#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/dom/VideoDecoderManagerChild.h"

#include "prlink.h"

#include "nsCycleCollector.h"
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
#include "nsTraceRefcnt.h"
#include "nsErrorService.h"

#include "nsArray.h"
#include "nsINIParserImpl.h"
#include "nsSupportsPrimitives.h"
#include "nsConsoleService.h"

#include "nsComponentManager.h"
#include "nsCategoryManagerUtils.h"
#include "nsIServiceManager.h"

#include "nsThreadManager.h"
#include "nsThreadPool.h"

#include "xptinfo.h"
#include "nsIInterfaceInfoManager.h"
#include "xptiprivate.h"
#include "mozilla/XPTInterfaceInfoManager.h"

#include "nsTimerImpl.h"
#include "TimerThread.h"

#include "nsThread.h"
#include "nsProcess.h"
#include "nsEnvironment.h"
#include "nsVersionComparatorImpl.h"

#include "nsIFile.h"
#include "nsLocalFile.h"
#if defined(XP_UNIX)
#include "nsNativeCharsetUtils.h"
#endif
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsCategoryManager.h"
#include "nsICategoryManager.h"
#include "nsMultiplexInputStream.h"

#include "nsStringStream.h"
extern nsresult nsStringInputStreamConstructor(nsISupports*, REFNSIID, void**);

#include "nsAtomService.h"
#include "nsAtomTable.h"
#include "nsISupportsImpl.h"

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
#include "nsMemoryInfoDumper.h"
#include "nsSecurityConsoleMessage.h"
#include "nsMessageLoop.h"

#include "nsStatusReporterManager.h"

#include <locale.h>
#include "mozilla/Services.h"
#include "mozilla/Omnijar.h"
#include "mozilla/HangMonitor.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/Telemetry.h"
#include "mozilla/BackgroundHangMonitor.h"

#include "nsChromeRegistry.h"
#include "nsChromeProtocolHandler.h"
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
#include "mozilla/SystemMemoryReporter.h"
#include "mozilla/UniquePtr.h"

#include "mozilla/ipc/GeckoChildProcessHost.h"

#include "ogg/ogg.h"
#if defined(MOZ_VPX) && !defined(MOZ_VPX_NO_MEM_REPORTING)
#if defined(HAVE_STDINT_H)
// mozilla-config.h defines HAVE_STDINT_H, and then it's defined *again* in
// vpx_config.h (which we include via vpx_mem.h, below). This redefinition
// triggers a build warning on MSVC, so we have to #undef it first.
#undef HAVE_STDINT_H
#endif
#include "vpx_mem/vpx_mem.h"
#endif

#include "GeckoProfiler.h"

#include "jsapi.h"
#include "js/Initialization.h"

#include "gfxPlatform.h"

#if EXPOSE_INTL_API
#include "unicode/putil.h"
#endif

using namespace mozilla;
using base::AtExitManager;
using mozilla::ipc::BrowserProcessSubThread;

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

NS_GENERIC_FACTORY_CONSTRUCTOR(nsProcess)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsID)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsString)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsCString)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRBool)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRUint8)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRUint16)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRUint32)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRUint64)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRTime)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsChar)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRInt16)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRInt32)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRInt64)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsFloat)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsDouble)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsInterfacePointer)

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsConsoleService, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAtomService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTimer)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBinaryOutputStream)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBinaryInputStream)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsStorageStream)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsVersionComparatorImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScriptableBase64Encoder)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsVariantCC)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsHashPropertyBagCC)

NS_GENERIC_AGGREGATED_CONSTRUCTOR(nsProperties)

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsUUIDGenerator, Init)

#ifdef MOZ_WIDGET_COCOA
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacUtilsImpl)
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsSystemInfo, Init)

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMemoryReporterManager, Init)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsMemoryInfoDumper)

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsStatusReporterManager, Init)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsIOUtil)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsSecurityConsoleMessage)

static nsresult
nsThreadManagerGetSingleton(nsISupports* aOuter,
                            const nsIID& aIID,
                            void** aInstancePtr)
{
  NS_ASSERTION(aInstancePtr, "null outptr");
  if (NS_WARN_IF(aOuter)) {
    return NS_ERROR_NO_AGGREGATION;
  }

  return nsThreadManager::get().QueryInterface(aIID, aInstancePtr);
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsThreadPool)

static nsresult
nsXPTIInterfaceInfoManagerGetSingleton(nsISupports* aOuter,
                                       const nsIID& aIID,
                                       void** aInstancePtr)
{
  NS_ASSERTION(aInstancePtr, "null outptr");
  if (NS_WARN_IF(aOuter)) {
    return NS_ERROR_NO_AGGREGATION;
  }

  nsCOMPtr<nsIInterfaceInfoManager> iim(XPTInterfaceInfoManager::GetSingleton());
  if (!iim) {
    return NS_ERROR_FAILURE;
  }

  return iim->QueryInterface(aIID, aInstancePtr);
}

nsComponentManagerImpl* nsComponentManagerImpl::gComponentManager = nullptr;
bool gXPCOMShuttingDown = false;
bool gXPCOMThreadsShutDown = false;
char16_t* gGREBinPath = nullptr;

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kINIParserFactoryCID, NS_INIPARSERFACTORY_CID);

NS_DEFINE_NAMED_CID(NS_CHROMEREGISTRY_CID);
NS_DEFINE_NAMED_CID(NS_CHROMEPROTOCOLHANDLER_CID);

NS_DEFINE_NAMED_CID(NS_SECURITY_CONSOLE_MESSAGE_CID);

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsChromeRegistry,
                                         nsChromeRegistry::GetSingleton)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsChromeProtocolHandler)

#define NS_PERSISTENTPROPERTIES_CID NS_IPERSISTENTPROPERTIES_CID /* sigh */

static already_AddRefed<nsIFactory>
CreateINIParserFactory(const mozilla::Module& aModule,
                       const mozilla::Module::CIDEntry& aEntry)
{
  nsCOMPtr<nsIFactory> f = new nsINIParserFactory();
  return f.forget();
}

#define COMPONENT(NAME, Ctor) static NS_DEFINE_CID(kNS_##NAME##_CID, NS_##NAME##_CID);
#define COMPONENT_M(NAME, Ctor, Selector) static NS_DEFINE_CID(kNS_##NAME##_CID, NS_##NAME##_CID);
#include "XPCOMModule.inc"
#undef COMPONENT
#undef COMPONENT_M

#define COMPONENT(NAME, Ctor) { &kNS_##NAME##_CID, false, nullptr, Ctor },
#define COMPONENT_M(NAME, Ctor, Selector) { &kNS_##NAME##_CID, false, nullptr, Ctor, Selector },
const mozilla::Module::CIDEntry kXPCOMCIDEntries[] = {
  { &kComponentManagerCID, true, nullptr, nsComponentManagerImpl::Create, Module::ALLOW_IN_GPU_PROCESS },
  { &kINIParserFactoryCID, false, CreateINIParserFactory },
#include "XPCOMModule.inc"
  { &kNS_CHROMEREGISTRY_CID, false, nullptr, nsChromeRegistryConstructor },
  { &kNS_CHROMEPROTOCOLHANDLER_CID, false, nullptr, nsChromeProtocolHandlerConstructor },
  { &kNS_SECURITY_CONSOLE_MESSAGE_CID, false, nullptr, nsSecurityConsoleMessageConstructor },
  { nullptr }
};
#undef COMPONENT
#undef COMPONENT_M

#define COMPONENT(NAME, Ctor) { NS_##NAME##_CONTRACTID, &kNS_##NAME##_CID },
#define COMPONENT_M(NAME, Ctor, Selector) { NS_##NAME##_CONTRACTID, &kNS_##NAME##_CID, Selector },
const mozilla::Module::ContractIDEntry kXPCOMContracts[] = {
#include "XPCOMModule.inc"
  { NS_CHROMEREGISTRY_CONTRACTID, &kNS_CHROMEREGISTRY_CID },
  { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "chrome", &kNS_CHROMEPROTOCOLHANDLER_CID },
  { NS_INIPARSERFACTORY_CONTRACTID, &kINIParserFactoryCID },
  { NS_SECURITY_CONSOLE_MESSAGE_CONTRACTID, &kNS_SECURITY_CONSOLE_MESSAGE_CID },
  { nullptr }
};
#undef COMPONENT
#undef COMPONENT_M

const mozilla::Module kXPCOMModule = {
  mozilla::Module::kVersion, kXPCOMCIDEntries, kXPCOMContracts,
  nullptr,
  nullptr,
  nullptr,
  nullptr,
  Module::ALLOW_IN_GPU_PROCESS
};

// gDebug will be freed during shutdown.
static nsIDebug2* gDebug = nullptr;

EXPORT_XPCOM_API(nsresult)
NS_GetDebug(nsIDebug2** aResult)
{
  return nsDebugImpl::Create(nullptr,  NS_GET_IID(nsIDebug2), (void**)aResult);
}

EXPORT_XPCOM_API(nsresult)
NS_InitXPCOM(nsIServiceManager** aResult,
             nsIFile* aBinDirectory)
{
  return NS_InitXPCOM2(aResult, aBinDirectory, nullptr);
}

class ICUReporter final
  : public nsIMemoryReporter
  , public CountingAllocatorBase<ICUReporter>
{
public:
  NS_DECL_ISUPPORTS

  static void* Alloc(const void*, size_t aSize)
  {
    return CountingMalloc(aSize);
  }

  static void* Realloc(const void*, void* aPtr, size_t aSize)
  {
    return CountingRealloc(aPtr, aSize);
  }

  static void Free(const void*, void* aPtr)
  {
    return CountingFree(aPtr);
  }

private:
  NS_IMETHOD
  CollectReports(nsIHandleReportCallback* aHandleReport, nsISupports* aData,
                 bool aAnonymize) override
  {
    MOZ_COLLECT_REPORT(
      "explicit/icu", KIND_HEAP, UNITS_BYTES, MemoryAllocated(),
      "Memory used by ICU, a Unicode and globalization support library.");

    return NS_OK;
  }

  ~ICUReporter() {}
};

NS_IMPL_ISUPPORTS(ICUReporter, nsIMemoryReporter)

/* static */ template<> Atomic<size_t>
CountingAllocatorBase<ICUReporter>::sAmount(0);

class OggReporter final
  : public nsIMemoryReporter
  , public CountingAllocatorBase<OggReporter>
{
public:
  NS_DECL_ISUPPORTS

private:
  NS_IMETHOD
  CollectReports(nsIHandleReportCallback* aHandleReport, nsISupports* aData,
                 bool aAnonymize) override
  {
    MOZ_COLLECT_REPORT(
      "explicit/media/libogg", KIND_HEAP, UNITS_BYTES, MemoryAllocated(),
      "Memory allocated through libogg for Ogg, Theora, and related media "
      "files.");

    return NS_OK;
  }

  ~OggReporter() {}
};

NS_IMPL_ISUPPORTS(OggReporter, nsIMemoryReporter)

/* static */ template<> Atomic<size_t>
CountingAllocatorBase<OggReporter>::sAmount(0);

#ifdef MOZ_VPX
class VPXReporter final
  : public nsIMemoryReporter
  , public CountingAllocatorBase<VPXReporter>
{
public:
  NS_DECL_ISUPPORTS

private:
  NS_IMETHOD
  CollectReports(nsIHandleReportCallback* aHandleReport, nsISupports* aData,
                 bool aAnonymize) override
  {
    MOZ_COLLECT_REPORT(
      "explicit/media/libvpx", KIND_HEAP, UNITS_BYTES, MemoryAllocated(),
      "Memory allocated through libvpx for WebM media files.");

    return NS_OK;
  }

  ~VPXReporter() {}
};

NS_IMPL_ISUPPORTS(VPXReporter, nsIMemoryReporter)

/* static */ template<> Atomic<size_t>
CountingAllocatorBase<VPXReporter>::sAmount(0);
#endif /* MOZ_VPX */

static bool sInitializedJS = false;

// Note that on OSX, aBinDirectory will point to .app/Contents/Resources/browser
EXPORT_XPCOM_API(nsresult)
NS_InitXPCOM2(nsIServiceManager** aResult,
              nsIFile* aBinDirectory,
              nsIDirectoryServiceProvider* aAppFileLocationProvider)
{
  static bool sInitialized = false;
  if (sInitialized) {
    return NS_ERROR_FAILURE;
  }

  sInitialized = true;

  mozPoisonValueInit();

  NS_LogInit();

  NS_InitAtomTable();

  mozilla::LogModule::Init();

  nsresult rv = NS_OK;

  // We are not shutting down
  gXPCOMShuttingDown = false;

  // Initialize the available memory tracker before other threads have had a
  // chance to start up, because the initialization is not thread-safe.
  mozilla::AvailableMemoryTracker::Init();

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
    UniquePtr<BrowserProcessSubThread> ioThread = MakeUnique<BrowserProcessSubThread>(BrowserProcessSubThread::IO);

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

#if defined(XP_UNIX)
  NS_StartupNativeCharsetUtils();
#endif

  NS_StartupLocalFile();

  nsDirectoryService::RealInit();

  bool value;

  if (aBinDirectory) {
    rv = aBinDirectory->IsDirectory(&value);

    if (NS_SUCCEEDED(rv) && value) {
      nsDirectoryService::gService->Set(NS_XPCOM_INIT_CURRENT_PROCESS_DIR,
                                        aBinDirectory);
    }
  }

  if (aAppFileLocationProvider) {
    rv = nsDirectoryService::gService->RegisterProvider(aAppFileLocationProvider);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  nsCOMPtr<nsIFile> xpcomLib;
  nsDirectoryService::gService->Get(NS_GRE_BIN_DIR,
                                    NS_GET_IID(nsIFile),
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

    static char const* const argv = { strdup(binaryPath.get()) };
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
  ogg_set_mem_functions(OggReporter::CountingMalloc,
                        OggReporter::CountingCalloc,
                        OggReporter::CountingRealloc,
                        OggReporter::CountingFree);

#if defined(MOZ_VPX) && !defined(MOZ_VPX_NO_MEM_REPORTING)
  // And for VPX.
  vpx_mem_set_functions(VPXReporter::CountingMalloc,
                        VPXReporter::CountingCalloc,
                        VPXReporter::CountingRealloc,
                        VPXReporter::CountingFree,
                        memcpy,
                        memset,
                        memmove);
#endif

#if EXPOSE_INTL_API && defined(MOZ_ICU_DATA_ARCHIVE)
  nsCOMPtr<nsIFile> greDir;
  nsDirectoryService::gService->Get(NS_GRE_DIR,
                                    NS_GET_IID(nsIFile),
                                    getter_AddRefs(greDir));
  MOZ_ASSERT(greDir);
  nsAutoCString nativeGREPath;
  greDir->GetNativePath(nativeGREPath);
  u_setDataDirectory(nativeGREPath.get());
#endif

  // Initialize the JS engine.
  const char* jsInitFailureReason = JS_InitWithFailureDiagnostic();
  if (jsInitFailureReason) {
    NS_RUNTIMEABORT(jsInitFailureReason);
  }
  sInitializedJS = true;
  
  // Init AbstractThread.
  AbstractThread::InitStatics();

  rv = nsComponentManagerImpl::gComponentManager->Init();
  if (NS_FAILED(rv)) {
    NS_RELEASE(nsComponentManagerImpl::gComponentManager);
    return rv;
  }

  if (aResult) {
    NS_ADDREF(*aResult = nsComponentManagerImpl::gComponentManager);
  }

  // The iimanager constructor searches and registers XPT files.
  // (We trigger the singleton's lazy construction here to make that happen.)
  (void)XPTInterfaceInfoManager::GetSingleton();

  // After autoreg, but before we actually instantiate any components,
  // add any services listed in the "xpcom-directory-providers" category
  // to the directory service.
  nsDirectoryService::gService->RegisterCategoryProviders();

  // Init SharedThreadPool (which needs the service manager).
  SharedThreadPool::InitStatics();

  // Force layout to spin up so that nsContentUtils is available for cx stack
  // munging.  Note that layout registers a number of static atoms, and also
  // seals the static atom table, so NS_RegisterStaticAtom may not be called
  // beyond this point.
  nsCOMPtr<nsISupports> componentLoader =
    do_GetService("@mozilla.org/moz/jsloader;1");

  mozilla::scache::StartupCache::GetSingleton();
  mozilla::AvailableMemoryTracker::Activate();

  // Notify observers of xpcom autoregistration start
  NS_CreateServicesFromCategory(NS_XPCOM_STARTUP_CATEGORY,
                                nullptr,
                                NS_XPCOM_STARTUP_OBSERVER_ID);
#ifdef XP_WIN
  CreateAnonTempFileRemover();
#endif

  // We only want the SystemMemoryReporter running in one process, because it
  // profiles the entire system.  The main process is the obvious place for
  // it.
  if (XRE_IsParentProcess()) {
    mozilla::SystemMemoryReporter::Init();
  }

  // The memory reporter manager is up and running -- register our reporters.
  RegisterStrongMemoryReporter(new ICUReporter());
  RegisterStrongMemoryReporter(new OggReporter());
#ifdef MOZ_VPX
  RegisterStrongMemoryReporter(new VPXReporter());
#endif

  mozilla::Telemetry::Init();

  mozilla::HangMonitor::Startup();
  mozilla::BackgroundHangMonitor::Startup();

  const MessageLoop* const loop = MessageLoop::current();
  sMainHangMonitor = new mozilla::BackgroundHangMonitor(
    loop->thread_name().c_str(),
    loop->transient_hang_timeout(),
    loop->permanent_hang_timeout());

  return NS_OK;
}

EXPORT_XPCOM_API(nsresult)
NS_InitMinimalXPCOM()
{
  mozPoisonValueInit();
  NS_SetMainThread();
  mozilla::TimeStamp::Startup();
  NS_LogInit();
  NS_InitAtomTable();
  mozilla::LogModule::Init();

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

  AbstractThread::InitStatics();
  SharedThreadPool::InitStatics();
  mozilla::Telemetry::Init();
  mozilla::HangMonitor::Startup();
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
NS_ShutdownXPCOM(nsIServiceManager* aServMgr)
{
  return mozilla::ShutdownXPCOM(aServMgr);
}

namespace mozilla {

void
SetICUMemoryFunctions()
{
  static bool sICUReporterInitialized = false;
  if (!sICUReporterInitialized) {
    if (!JS_SetICUMemoryFunctions(ICUReporter::Alloc, ICUReporter::Realloc,
                                  ICUReporter::Free)) {
      MOZ_CRASH("JS_SetICUMemoryFunctions failed.");
    }
    sICUReporterInitialized = true;
  }
}

nsresult
ShutdownXPCOM(nsIServiceManager* aServMgr)
{
  // Make sure the hang monitor is enabled for shutdown.
  HangMonitor::NotifyActivity();

  if (!NS_IsMainThread()) {
    MOZ_CRASH("Shutdown on wrong thread");
  }

  nsresult rv;
  nsCOMPtr<nsISimpleEnumerator> moduleLoaders;

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
      observerService->NotifyObservers(nullptr,
                                       NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID,
                                       nullptr);

      nsCOMPtr<nsIServiceManager> mgr;
      rv = NS_GetServiceManager(getter_AddRefs(mgr));
      if (NS_SUCCEEDED(rv)) {
        mozilla::KillClearOnShutdown(ShutdownPhase::Shutdown);
        observerService->NotifyObservers(mgr, NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                         nullptr);
      }
    }

    // This must happen after the shutdown of media and widgets, which
    // are triggered by the NS_XPCOM_SHUTDOWN_OBSERVER_ID notification.
    NS_ProcessPendingEvents(thread);
    gfxPlatform::ShutdownLayersIPC();
    mozilla::dom::VideoDecoderManagerChild::Shutdown();

    mozilla::scache::StartupCache::DeleteSingleton();
    if (observerService)
    {
      mozilla::KillClearOnShutdown(ShutdownPhase::ShutdownThreads);
      observerService->NotifyObservers(nullptr,
                                       NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID,
                                       nullptr);
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

    HangMonitor::NotifyActivity();

    // Late-write checks needs to find the profile directory, so it has to
    // be initialized before mozilla::services::Shutdown or (because of
    // xpcshell tests replacing the service) modules being unloaded.
    mozilla::InitLateWriteChecks();

    // We save the "xpcom-shutdown-loaders" observers to notify after
    // the observerservice is gone.
    if (observerService) {
      mozilla::KillClearOnShutdown(ShutdownPhase::ShutdownLoaders);
      observerService->EnumerateObservers(NS_XPCOM_SHUTDOWN_LOADERS_OBSERVER_ID,
                                          getter_AddRefs(moduleLoaders));

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

  if (moduleLoaders) {
    bool more;
    nsCOMPtr<nsISupports> el;
    while (NS_SUCCEEDED(moduleLoaders->HasMoreElements(&more)) && more) {
      moduleLoaders->GetNext(getter_AddRefs(el));

      // Don't worry about weak-reference observers here: there is
      // no reason for weak-ref observers to register for
      // xpcom-shutdown-loaders

      // FIXME: This can cause harmless writes from sqlite committing
      // log files. We have to ignore them before we can move
      // the mozilla::PoisonWrite call before this point. See bug
      // 834945 for the details.
      nsCOMPtr<nsIObserver> obs(do_QueryInterface(el));
      if (obs) {
        obs->Observe(nullptr, NS_XPCOM_SHUTDOWN_LOADERS_OBSERVER_ID, nullptr);
      }
    }

    moduleLoaders = nullptr;
  }

  bool shutdownCollect;
#ifdef NS_FREE_PERMANENT_DATA
  shutdownCollect = true;
#else
  shutdownCollect = !!PR_GetEnv("MOZ_CC_RUN_DURING_SHUTDOWN");
#endif
  nsCycleCollector_shutdown(shutdownCollect);

  PROFILER_MARKER("Shutdown xpcom");
  // If we are doing any shutdown checks, poison writes.
  if (gShutdownChecks != SCM_NOTHING) {
#ifdef XP_MACOSX
    mozilla::OnlyReportDirtyWrites();
#endif /* XP_MACOSX */
    mozilla::BeginLateWriteChecks();
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
  } else {
    NS_WARNING("Component Manager was never created ...");
  }

#ifdef MOZ_GECKO_PROFILER
  // In optimized builds we don't do shutdown collections by default, so
  // uncollected (garbage) objects may keep the nsXPConnect singleton alive,
  // and its XPCJSContext along with it. However, we still destroy various
  // bits of state in JS_ShutDown(), so we need to make sure the profiler
  // can't access them when it shuts down. This call nulls out the
  // JS pseudo-stack's internal reference to the main thread JSContext,
  // duplicating the call in XPCJSContext::~XPCJSContext() in case that
  // never fired.
  if (PseudoStack* stack = profiler_get_pseudo_stack()) {
    stack->sampleContext(nullptr);
  }
#endif

  if (sInitializedJS) {
    // Shut down the JS engine.
    JS_ShutDown();
    sInitializedJS = false;
  }

  // Release our own singletons
  // Do this _after_ shutting down the component manager, because the
  // JS component loader will use XPConnect to call nsIModule::canUnload,
  // and that will spin up the InterfaceInfoManager again -- bad mojo
  XPTInterfaceInfoManager::FreeInterfaceInfoManager();

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

  NS_ShutdownAtomTable();

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

  HangMonitor::Shutdown();

  delete sMainHangMonitor;
  sMainHangMonitor = nullptr;

  BackgroundHangMonitor::Shutdown();

  NS_LogTerm();

#if defined(MOZ_WIDGET_GONK)
  // This _exit(0) call is intended to be temporary, to get shutdown leak
  // checking working on non-B2G platforms.
  // On debug B2G, the child process crashes very late.  Instead, just
  // give up so at least we exit cleanly. See bug 1071866.
  if (XRE_IsContentProcess()) {
      NS_WARNING("Exiting child process early!");
      _exit(0);
  }
#endif

  return NS_OK;
}

} // namespace mozilla
