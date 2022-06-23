/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include "nscore.h"
#include "nsISupports.h"
#include "nspr.h"
#include "nsCRT.h"  // for atoll

#include "StaticComponents.h"

#include "nsCategoryManager.h"
#include "nsCOMPtr.h"
#include "nsComponentManager.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsCategoryManager.h"
#include "nsLayoutModule.h"
#include "mozilla/MemoryReporting.h"
#include "nsIObserverService.h"
#include "nsIStringEnumerator.h"
#include "nsXPCOM.h"
#include "nsXPCOMPrivate.h"
#include "nsISupportsPrimitives.h"
#include "nsLocalFile.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "prcmon.h"
#include "nsThreadManager.h"
#include "nsThreadUtils.h"
#include "prthread.h"
#include "private/pprthred.h"
#include "nsTArray.h"
#include "prio.h"
#include "ManifestParser.h"
#include "nsNetUtil.h"
#include "mozilla/Services.h"

#include "mozilla/GenericFactory.h"
#include "nsSupportsPrimitives.h"
#include "nsArray.h"
#include "nsIMutableArray.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/FileUtils.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/URLPreloader.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Variant.h"

#include <new>  // for placement new

#include "mozilla/Omnijar.h"

#include "mozilla/Logging.h"
#include "LogModulePrefWatcher.h"

#ifdef MOZ_MEMORY
#  include "mozmemory.h"
#endif

using namespace mozilla;
using namespace mozilla::xpcom;

static LazyLogModule nsComponentManagerLog("nsComponentManager");

#if 0
#  define SHOW_DENIED_ON_SHUTDOWN
#  define SHOW_CI_ON_EXISTING_SERVICE
#endif

namespace {

class AutoIDString : public nsAutoCStringN<NSID_LENGTH> {
 public:
  explicit AutoIDString(const nsID& aID) {
    SetLength(NSID_LENGTH - 1);
    aID.ToProvidedString(
        *reinterpret_cast<char(*)[NSID_LENGTH]>(BeginWriting()));
  }
};

}  // namespace

namespace mozilla {
namespace xpcom {

using ProcessSelector = Module::ProcessSelector;

// Note: These must be kept in sync with the ProcessSelector definition in
// Module.h.
bool ProcessSelectorMatches(ProcessSelector aSelector) {
  GeckoProcessType type = XRE_GetProcessType();
  if (type == GeckoProcessType_GPU) {
    return !!(aSelector & Module::ALLOW_IN_GPU_PROCESS);
  }

  if (type == GeckoProcessType_RDD) {
    return !!(aSelector & Module::ALLOW_IN_RDD_PROCESS);
  }

  if (type == GeckoProcessType_Socket) {
    return !!(aSelector & (Module::ALLOW_IN_SOCKET_PROCESS));
  }

  if (type == GeckoProcessType_VR) {
    return !!(aSelector & Module::ALLOW_IN_VR_PROCESS);
  }

  if (type == GeckoProcessType_Utility) {
    return !!(aSelector & Module::ALLOW_IN_UTILITY_PROCESS);
  }

  // Only allow XPCOM modules which can be loaded in all processes to be loaded
  // in the IPDLUnitTest process.
  if (type == GeckoProcessType_IPDLUnitTest) {
    return size_t(aSelector) == Module::kMaxProcessSelector;
  }

  if (aSelector & Module::MAIN_PROCESS_ONLY) {
    return type == GeckoProcessType_Default;
  }
  if (aSelector & Module::CONTENT_PROCESS_ONLY) {
    return type == GeckoProcessType_Content;
  }
  return true;
}

static bool gProcessMatchTable[Module::kMaxProcessSelector + 1];

bool FastProcessSelectorMatches(ProcessSelector aSelector) {
  return gProcessMatchTable[size_t(aSelector)];
}

}  // namespace xpcom
}  // namespace mozilla

namespace {

/**
 * A wrapper simple wrapper class, which can hold either a dynamic
 * nsFactoryEntry instance, or a static StaticModule entry, and transparently
 * forwards method calls to the wrapped object.
 *
 * This allows the same code to work with either static or dynamic modules
 * without caring about the difference.
 */
class MOZ_STACK_CLASS EntryWrapper final {
 public:
  explicit EntryWrapper(nsFactoryEntry* aEntry) : mEntry(aEntry) {}

  explicit EntryWrapper(const StaticModule* aEntry) : mEntry(aEntry) {}

#define MATCH(type, ifFactory, ifStatic)                     \
  struct Matcher {                                           \
    type operator()(nsFactoryEntry* entry) { ifFactory; }    \
    type operator()(const StaticModule* entry) { ifStatic; } \
  };                                                         \
  return mEntry.match((Matcher()))

  const nsID& CID() {
    MATCH(const nsID&, return *entry->mCIDEntry->cid, return entry->CID());
  }

  already_AddRefed<nsIFactory> GetFactory() {
    MATCH(already_AddRefed<nsIFactory>, return entry->GetFactory(),
          return entry->GetFactory());
  }

  /**
   * Creates an instance of the underlying component. This should be used in
   * preference to GetFactory()->CreateInstance() where appropriate, since it
   * side-steps the necessity of creating a nsIFactory instance for static
   * modules.
   */
  nsresult CreateInstance(const nsIID& aIID, void** aResult) {
    if (mEntry.is<nsFactoryEntry*>()) {
      return mEntry.as<nsFactoryEntry*>()->CreateInstance(aIID, aResult);
    }
    return mEntry.as<const StaticModule*>()->CreateInstance(aIID, aResult);
  }

  /**
   * Returns the cached service instance for this entry, if any. This should
   * only be accessed while mLock is held.
   */
  nsISupports* ServiceInstance() {
    MATCH(nsISupports*, return entry->mServiceObject,
          return entry->ServiceInstance());
  }
  void SetServiceInstance(already_AddRefed<nsISupports> aInst) {
    if (mEntry.is<nsFactoryEntry*>()) {
      mEntry.as<nsFactoryEntry*>()->mServiceObject = aInst;
    } else {
      return mEntry.as<const StaticModule*>()->SetServiceInstance(
          std::move(aInst));
    }
  }

  /**
   * Returns the description string for the module this entry belongs to. For
   * static entries, always returns "<unknown module>".
   */
  nsCString ModuleDescription() {
    MATCH(nsCString,
          return entry->mModule ? entry->mModule->Description()
                                : "<unknown module>"_ns,
          return "<unknown module>"_ns);
  }

 private:
  Variant<nsFactoryEntry*, const StaticModule*> mEntry;
};

}  // namespace

// this is safe to call during InitXPCOM
static already_AddRefed<nsIFile> GetLocationFromDirectoryService(
    const char* aProp) {
  nsCOMPtr<nsIProperties> directoryService;
  nsDirectoryService::Create(NS_GET_IID(nsIProperties),
                             getter_AddRefs(directoryService));

  if (!directoryService) {
    return nullptr;
  }

  nsCOMPtr<nsIFile> file;
  nsresult rv =
      directoryService->Get(aProp, NS_GET_IID(nsIFile), getter_AddRefs(file));
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return file.forget();
}

static already_AddRefed<nsIFile> CloneAndAppend(nsIFile* aBase,
                                                const nsACString& aAppend) {
  nsCOMPtr<nsIFile> f;
  aBase->Clone(getter_AddRefs(f));
  if (!f) {
    return nullptr;
  }

  f->AppendNative(aAppend);
  return f.forget();
}

////////////////////////////////////////////////////////////////////////////////
// nsComponentManagerImpl
////////////////////////////////////////////////////////////////////////////////

nsresult nsComponentManagerImpl::Create(REFNSIID aIID, void** aResult) {
  if (!gComponentManager) {
    return NS_ERROR_FAILURE;
  }

  return gComponentManager->QueryInterface(aIID, aResult);
}

static const int CONTRACTID_HASHTABLE_INITIAL_LENGTH = 8;

nsComponentManagerImpl::nsComponentManagerImpl()
    : mFactories(CONTRACTID_HASHTABLE_INITIAL_LENGTH),
      mContractIDs(CONTRACTID_HASHTABLE_INITIAL_LENGTH),
      mLock("nsComponentManagerImpl.mLock"),
      mStatus(NOT_INITIALIZED) {}

static nsTArray<const mozilla::Module*>* sExtraStaticModules;

/* static */
void nsComponentManagerImpl::InitializeStaticModules() {
  if (sExtraStaticModules) {
    return;
  }

  sExtraStaticModules = new nsTArray<const mozilla::Module*>;
}

nsTArray<nsComponentManagerImpl::ComponentLocation>*
    nsComponentManagerImpl::sModuleLocations;

/* static */
void nsComponentManagerImpl::InitializeModuleLocations() {
  if (sModuleLocations) {
    return;
  }

  sModuleLocations = new nsTArray<ComponentLocation>;
}

nsresult nsComponentManagerImpl::Init() {
  {
    gProcessMatchTable[size_t(ProcessSelector::ANY_PROCESS)] =
        ProcessSelectorMatches(ProcessSelector::ANY_PROCESS);
    gProcessMatchTable[size_t(ProcessSelector::MAIN_PROCESS_ONLY)] =
        ProcessSelectorMatches(ProcessSelector::MAIN_PROCESS_ONLY);
    gProcessMatchTable[size_t(ProcessSelector::CONTENT_PROCESS_ONLY)] =
        ProcessSelectorMatches(ProcessSelector::CONTENT_PROCESS_ONLY);
    gProcessMatchTable[size_t(ProcessSelector::ALLOW_IN_GPU_PROCESS)] =
        ProcessSelectorMatches(ProcessSelector::ALLOW_IN_GPU_PROCESS);
    gProcessMatchTable[size_t(ProcessSelector::ALLOW_IN_VR_PROCESS)] =
        ProcessSelectorMatches(ProcessSelector::ALLOW_IN_VR_PROCESS);
    gProcessMatchTable[size_t(ProcessSelector::ALLOW_IN_SOCKET_PROCESS)] =
        ProcessSelectorMatches(ProcessSelector::ALLOW_IN_SOCKET_PROCESS);
    gProcessMatchTable[size_t(ProcessSelector::ALLOW_IN_RDD_PROCESS)] =
        ProcessSelectorMatches(ProcessSelector::ALLOW_IN_RDD_PROCESS);
    gProcessMatchTable[size_t(ProcessSelector::ALLOW_IN_GPU_AND_MAIN_PROCESS)] =
        ProcessSelectorMatches(ProcessSelector::ALLOW_IN_GPU_AND_MAIN_PROCESS);
    gProcessMatchTable[size_t(ProcessSelector::ALLOW_IN_GPU_AND_VR_PROCESS)] =
        ProcessSelectorMatches(ProcessSelector::ALLOW_IN_GPU_AND_VR_PROCESS);
    gProcessMatchTable[size_t(
        ProcessSelector::ALLOW_IN_GPU_AND_SOCKET_PROCESS)] =
        ProcessSelectorMatches(
            ProcessSelector::ALLOW_IN_GPU_AND_SOCKET_PROCESS);
    gProcessMatchTable[size_t(
        ProcessSelector::ALLOW_IN_GPU_VR_AND_SOCKET_PROCESS)] =
        ProcessSelectorMatches(
            ProcessSelector::ALLOW_IN_GPU_VR_AND_SOCKET_PROCESS);
    gProcessMatchTable[size_t(
        ProcessSelector::ALLOW_IN_RDD_AND_SOCKET_PROCESS)] =
        ProcessSelectorMatches(
            ProcessSelector::ALLOW_IN_RDD_AND_SOCKET_PROCESS);
    gProcessMatchTable[size_t(
        ProcessSelector::ALLOW_IN_GPU_RDD_AND_SOCKET_PROCESS)] =
        ProcessSelectorMatches(
            ProcessSelector::ALLOW_IN_GPU_RDD_AND_SOCKET_PROCESS);
    gProcessMatchTable[size_t(
        ProcessSelector::ALLOW_IN_GPU_RDD_SOCKET_AND_UTILITY_PROCESS)] =
        ProcessSelectorMatches(
            ProcessSelector::ALLOW_IN_GPU_RDD_SOCKET_AND_UTILITY_PROCESS);
    gProcessMatchTable[size_t(
        ProcessSelector::ALLOW_IN_GPU_RDD_VR_AND_SOCKET_PROCESS)] =
        ProcessSelectorMatches(
            ProcessSelector::ALLOW_IN_GPU_RDD_VR_AND_SOCKET_PROCESS);
    gProcessMatchTable[size_t(
        ProcessSelector::ALLOW_IN_GPU_RDD_VR_SOCKET_AND_UTILITY_PROCESS)] =
        ProcessSelectorMatches(
            ProcessSelector::ALLOW_IN_GPU_RDD_VR_SOCKET_AND_UTILITY_PROCESS);
  }

  MOZ_ASSERT(NOT_INITIALIZED == mStatus);

  nsCOMPtr<nsIFile> greDir = GetLocationFromDirectoryService(NS_GRE_DIR);
  nsCOMPtr<nsIFile> appDir =
      GetLocationFromDirectoryService(NS_XPCOM_CURRENT_PROCESS_DIR);

  InitializeStaticModules();

  nsCategoryManager::GetSingleton()->SuppressNotifications(true);

  for (uint32_t i = 0; i < sExtraStaticModules->Length(); ++i) {
    RegisterModule((*sExtraStaticModules)[i]);
  }

  auto* catMan = nsCategoryManager::GetSingleton();
  for (const auto& cat : gStaticCategories) {
    for (const auto& entry : cat) {
      if (entry.Active()) {
        catMan->AddCategoryEntry(cat.Name(), entry.Entry(), entry.Value());
      }
    }
  }

  bool loadChromeManifests;
  switch (XRE_GetProcessType()) {
    // We are going to assume that only a select few (see below) process types
    // want to load chrome manifests, and that any new process types will not
    // want to load them, because they're not going to be executing JS.
    case GeckoProcessType_RemoteSandboxBroker:
    default:
      loadChromeManifests = false;
      break;

    // XXX The check this code replaced implicitly let through all of these
    // process types, but presumably only the default (parent) and content
    // processes really need chrome manifests...?
    case GeckoProcessType_Default:
    case GeckoProcessType_Content:
    case GeckoProcessType_GMPlugin:
      loadChromeManifests = true;
      break;
  }

  if (loadChromeManifests) {
    // This needs to be called very early, before anything in nsLayoutModule is
    // used, and before any calls are made into the JS engine.
    nsLayoutModuleInitialize();

    mJSLoaderReady = true;

    // The overall order in which chrome.manifests are expected to be treated
    // is the following:
    // - greDir's omni.ja or greDir
    // - appDir's omni.ja or appDir

    InitializeModuleLocations();
    ComponentLocation* cl = sModuleLocations->AppendElement();
    cl->type = NS_APP_LOCATION;
    RefPtr<nsZipArchive> greOmnijar =
        mozilla::Omnijar::GetReader(mozilla::Omnijar::GRE);
    if (greOmnijar) {
      cl->location.Init(greOmnijar, "chrome.manifest");
    } else {
      nsCOMPtr<nsIFile> lf = CloneAndAppend(greDir, "chrome.manifest"_ns);
      cl->location.Init(lf);
    }

    RefPtr<nsZipArchive> appOmnijar =
        mozilla::Omnijar::GetReader(mozilla::Omnijar::APP);
    if (appOmnijar) {
      cl = sModuleLocations->AppendElement();
      cl->type = NS_APP_LOCATION;
      cl->location.Init(appOmnijar, "chrome.manifest");
    } else {
      bool equals = false;
      appDir->Equals(greDir, &equals);
      if (!equals) {
        cl = sModuleLocations->AppendElement();
        cl->type = NS_APP_LOCATION;
        nsCOMPtr<nsIFile> lf = CloneAndAppend(appDir, "chrome.manifest"_ns);
        cl->location.Init(lf);
      }
    }

    RereadChromeManifests(false);
  }

  nsCategoryManager::GetSingleton()->SuppressNotifications(false);

  RegisterWeakMemoryReporter(this);

  // NB: The logging preference watcher needs to be registered late enough in
  // startup that it's okay to use the preference system, but also as soon as
  // possible so that log modules enabled via preferences are turned on as
  // early as possible.
  //
  // We can't initialize the preference watcher when the log module manager is
  // initialized, as a number of things attempt to start logging before the
  // preference system is initialized.
  //
  // The preference system is registered as a component so at this point during
  // component manager initialization we know it is setup and we can register
  // for notifications.
  LogModulePrefWatcher::RegisterPrefWatcher();

  // Unfortunately, we can't register the nsCategoryManager memory reporter
  // in its constructor (which is triggered by the GetSingleton() call
  // above) because the memory reporter manager isn't initialized at that
  // point.  So we wait until now.
  nsCategoryManager::GetSingleton()->InitMemoryReporter();

  MOZ_LOG(nsComponentManagerLog, LogLevel::Debug,
          ("nsComponentManager: Initialized."));

  mStatus = NORMAL;

  MOZ_ASSERT(!XRE_IsContentProcess() ||
             CONTRACTID_HASHTABLE_INITIAL_LENGTH <= 8 ||
                 mFactories.Count() > CONTRACTID_HASHTABLE_INITIAL_LENGTH / 3,
             "Initial component hashtable size is too large");

  return NS_OK;
}

static const int kModuleVersionWithSelector = 51;

template <typename T>
static void AssertNotMallocAllocated(T* aPtr) {
#if defined(DEBUG) && defined(MOZ_MEMORY)
  jemalloc_ptr_info_t info;
  jemalloc_ptr_info((void*)aPtr, &info);
  MOZ_ASSERT(info.tag == TagUnknown);
#endif
}

template <typename T>
static void AssertNotStackAllocated(T* aPtr) {
  // On all of our supported platforms, the stack grows down. Any address
  // located below the address of our argument is therefore guaranteed not to be
  // stack-allocated by the caller.
  //
  // For addresses above our argument, things get trickier. The main thread
  // stack is traditionally placed at the top of the program's address space,
  // but that is becoming less reliable as more and more systems adopt address
  // space layout randomization strategies, so we have to guess how much space
  // above our argument pointer we need to care about.
  //
  // On most systems, we're guaranteed at least several KiB at the top of each
  // stack for TLS. We'd probably be safe assuming at least 4KiB in the stack
  // segment above our argument address, but safer is... well, safer.
  //
  // For threads with huge stacks, it's theoretically possible that we could
  // wind up being passed a stack-allocated string from farther up the stack,
  // but this is a best-effort thing, so we'll assume we only care about the
  // immediate caller. For that case, max 2KiB per stack frame is probably a
  // reasonable guess most of the time, and is less than the ~4KiB that we
  // expect for TLS, so go with that to avoid the risk of bumping into heap
  // data just above the stack.
#ifdef DEBUG
  static constexpr size_t kFuzz = 2048;

  MOZ_ASSERT(uintptr_t(aPtr) < uintptr_t(&aPtr) ||
             uintptr_t(aPtr) > uintptr_t(&aPtr) + kFuzz);
#endif
}

static inline nsCString AsLiteralCString(const char* aStr) {
  AssertNotMallocAllocated(aStr);
  AssertNotStackAllocated(aStr);

  nsCString str;
  str.AssignLiteral(aStr, strlen(aStr));
  return str;
}

void nsComponentManagerImpl::RegisterModule(const mozilla::Module* aModule) {
  mLock.AssertNotCurrentThreadOwns();

  if (aModule->mVersion >= kModuleVersionWithSelector &&
      !ProcessSelectorMatches(aModule->selector)) {
    return;
  }

  {
    // Scope the monitor so that we don't hold it while calling into the
    // category manager.
    MonitorAutoLock lock(mLock);

    KnownModule* m = new KnownModule(aModule);
    mKnownStaticModules.AppendElement(m);

    if (aModule->mCIDs) {
      const mozilla::Module::CIDEntry* entry;
      for (entry = aModule->mCIDs; entry->cid; ++entry) {
        RegisterCIDEntryLocked(entry, m);
      }
    }

    if (aModule->mContractIDs) {
      const mozilla::Module::ContractIDEntry* entry;
      for (entry = aModule->mContractIDs; entry->contractid; ++entry) {
        RegisterContractIDLocked(entry);
      }
      MOZ_ASSERT(!entry->cid, "Incorrectly terminated contract list");
    }
  }

  if (aModule->mCategoryEntries) {
    const mozilla::Module::CategoryEntry* entry;
    for (entry = aModule->mCategoryEntries; entry->category; ++entry)
      nsCategoryManager::GetSingleton()->AddCategoryEntry(
          AsLiteralCString(entry->category), AsLiteralCString(entry->entry),
          AsLiteralCString(entry->value));
  }
}

void nsComponentManagerImpl::RegisterCIDEntryLocked(
    const mozilla::Module::CIDEntry* aEntry, KnownModule* aModule) {
  mLock.AssertCurrentThreadOwns();

  if (!ProcessSelectorMatches(aEntry->processSelector)) {
    return;
  }

#ifdef DEBUG
  // If we're still in the static initialization phase, check that we're not
  // registering something that was already registered.
  if (mStatus != NORMAL) {
    if (StaticComponents::LookupByCID(*aEntry->cid)) {
      MOZ_CRASH_UNSAFE_PRINTF(
          "While registering XPCOM module %s, trying to re-register CID '%s' "
          "already registered by a static component.",
          aModule->Description().get(), AutoIDString(*aEntry->cid).get());
    }
  }
#endif

  mFactories.WithEntryHandle(aEntry->cid, [&](auto&& entry) {
    mLock.AssertCurrentThreadOwns();
    if (entry) {
      nsFactoryEntry* f = entry.Data();
      NS_WARNING("Re-registering a CID?");

      nsCString existing;
      if (f->mModule) {
        existing = f->mModule->Description();
      } else {
        existing = "<unknown module>";
      }
      MonitorAutoUnlock unlock(mLock);
      LogMessage(
          "While registering XPCOM module %s, trying to re-register CID '%s' "
          "already registered by %s.",
          aModule->Description().get(), AutoIDString(*aEntry->cid).get(),
          existing.get());
    } else {
      entry.Insert(new nsFactoryEntry(aEntry, aModule));
    }
  });
}

void nsComponentManagerImpl::RegisterContractIDLocked(
    const mozilla::Module::ContractIDEntry* aEntry) {
  mLock.AssertCurrentThreadOwns();

  if (!ProcessSelectorMatches(aEntry->processSelector)) {
    return;
  }

#ifdef DEBUG
  // If we're still in the static initialization phase, check that we're not
  // registering something that was already registered.
  if (mStatus != NORMAL) {
    if (const StaticModule* module = StaticComponents::LookupByContractID(
            nsAutoCString(aEntry->contractid))) {
      MOZ_CRASH_UNSAFE_PRINTF(
          "Could not map contract ID '%s' to CID %s because it is already "
          "mapped to CID %s.",
          aEntry->contractid, AutoIDString(*aEntry->cid).get(),
          AutoIDString(module->CID()).get());
    }
  }
#endif

  nsFactoryEntry* f = mFactories.Get(aEntry->cid);
  if (!f) {
    NS_WARNING("No CID found when attempting to map contract ID");

    MonitorAutoUnlock unlock(mLock);
    LogMessage(
        "Could not map contract ID '%s' to CID %s because no implementation of "
        "the CID is registered.",
        aEntry->contractid, AutoIDString(*aEntry->cid).get());

    return;
  }

  mContractIDs.InsertOrUpdate(AsLiteralCString(aEntry->contractid), f);
}

static void DoRegisterManifest(NSLocationType aType, FileLocation& aFile,
                               bool aChromeOnly) {
  auto result = URLPreloader::Read(aFile);
  if (result.isOk()) {
    nsCString buf(result.unwrap());
    ParseManifest(aType, aFile, buf.BeginWriting(), aChromeOnly);
  } else if (NS_BOOTSTRAPPED_LOCATION != aType) {
    nsCString uri;
    aFile.GetURIString(uri);
    LogMessage("Could not read chrome manifest '%s'.", uri.get());
  }
}

void nsComponentManagerImpl::RegisterManifest(NSLocationType aType,
                                              FileLocation& aFile,
                                              bool aChromeOnly) {
  DoRegisterManifest(aType, aFile, aChromeOnly);
}

void nsComponentManagerImpl::ManifestManifest(ManifestProcessingContext& aCx,
                                              int aLineNo, char* const* aArgv) {
  char* file = aArgv[0];
  FileLocation f(aCx.mFile, file);
  RegisterManifest(aCx.mType, f, aCx.mChromeOnly);
}

void nsComponentManagerImpl::ManifestCategory(ManifestProcessingContext& aCx,
                                              int aLineNo, char* const* aArgv) {
  char* category = aArgv[0];
  char* key = aArgv[1];
  char* value = aArgv[2];

  nsCategoryManager::GetSingleton()->AddCategoryEntry(
      nsDependentCString(category), nsDependentCString(key),
      nsDependentCString(value));
}

void nsComponentManagerImpl::RereadChromeManifests(bool aChromeOnly) {
  for (uint32_t i = 0; i < sModuleLocations->Length(); ++i) {
    ComponentLocation& l = sModuleLocations->ElementAt(i);
    RegisterManifest(l.type, l.location, aChromeOnly);
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nullptr, "chrome-manifests-loaded", nullptr);
  }
}

bool nsComponentManagerImpl::KnownModule::Load() {
  if (mFailed) {
    return false;
  }
  MOZ_ASSERT(mModule);
  if (!mLoaded) {
    if (mModule->loadProc) {
      nsresult rv = mModule->loadProc();
      if (NS_FAILED(rv)) {
        mFailed = true;
        return false;
      }
    }
    mLoaded = true;
  }
  return true;
}

nsCString nsComponentManagerImpl::KnownModule::Description() const {
  return "<static module>"_ns;
}

nsresult nsComponentManagerImpl::Shutdown(void) {
  MOZ_ASSERT(NORMAL == mStatus);

  mStatus = SHUTDOWN_IN_PROGRESS;

  // Shutdown the component manager
  MOZ_LOG(nsComponentManagerLog, LogLevel::Debug,
          ("nsComponentManager: Beginning Shutdown."));

  UnregisterWeakMemoryReporter(this);

  // Release all cached factories
  mContractIDs.Clear();
  mFactories.Clear();  // XXX release the objects, don't just clear
  mKnownStaticModules.Clear();

  StaticComponents::Shutdown();

  delete sExtraStaticModules;
  delete sModuleLocations;

  mStatus = SHUTDOWN_COMPLETE;

  MOZ_LOG(nsComponentManagerLog, LogLevel::Debug,
          ("nsComponentManager: Shutdown complete."));

  return NS_OK;
}

nsComponentManagerImpl::~nsComponentManagerImpl() {
  MOZ_LOG(nsComponentManagerLog, LogLevel::Debug,
          ("nsComponentManager: Beginning destruction."));

  if (SHUTDOWN_COMPLETE != mStatus) {
    Shutdown();
  }

  MOZ_LOG(nsComponentManagerLog, LogLevel::Debug,
          ("nsComponentManager: Destroyed."));
}

NS_IMPL_ISUPPORTS(nsComponentManagerImpl, nsIComponentManager,
                  nsIServiceManager, nsIComponentRegistrar,
                  nsISupportsWeakReference, nsIInterfaceRequestor,
                  nsIMemoryReporter)

nsresult nsComponentManagerImpl::GetInterface(const nsIID& aUuid,
                                              void** aResult) {
  NS_WARNING("This isn't supported");
  // fall through to QI as anything QIable is a superset of what can be
  // got via the GetInterface()
  return QueryInterface(aUuid, aResult);
}

Maybe<EntryWrapper> nsComponentManagerImpl::LookupByCID(const nsID& aCID) {
  return LookupByCID(MonitorAutoLock(mLock), aCID);
}

Maybe<EntryWrapper> nsComponentManagerImpl::LookupByCID(const MonitorAutoLock&,
                                                        const nsID& aCID) {
  if (const StaticModule* module = StaticComponents::LookupByCID(aCID)) {
    return Some(EntryWrapper(module));
  }
  if (nsFactoryEntry* entry = mFactories.Get(&aCID)) {
    return Some(EntryWrapper(entry));
  }
  return Nothing();
}

Maybe<EntryWrapper> nsComponentManagerImpl::LookupByContractID(
    const nsACString& aContractID) {
  return LookupByContractID(MonitorAutoLock(mLock), aContractID);
}

Maybe<EntryWrapper> nsComponentManagerImpl::LookupByContractID(
    const MonitorAutoLock&, const nsACString& aContractID) {
  if (const StaticModule* module =
          StaticComponents::LookupByContractID(aContractID)) {
    return Some(EntryWrapper(module));
  }
  if (nsFactoryEntry* entry = mContractIDs.Get(aContractID)) {
    // UnregisterFactory might have left a stale nsFactoryEntry in
    // mContractIDs, so we should check to see whether this entry has
    // anything useful.
    if (entry->mModule || entry->mFactory || entry->mServiceObject) {
      return Some(EntryWrapper(entry));
    }
  }
  return Nothing();
}

already_AddRefed<nsIFactory> nsComponentManagerImpl::FindFactory(
    const nsCID& aClass) {
  Maybe<EntryWrapper> e = LookupByCID(aClass);
  if (!e) {
    return nullptr;
  }

  return e->GetFactory();
}

already_AddRefed<nsIFactory> nsComponentManagerImpl::FindFactory(
    const char* aContractID, uint32_t aContractIDLen) {
  Maybe<EntryWrapper> entry =
      LookupByContractID(nsDependentCString(aContractID, aContractIDLen));
  if (!entry) {
    return nullptr;
  }

  return entry->GetFactory();
}

/**
 * GetClassObject()
 *
 * Given a classID, this finds the singleton ClassObject that implements the
 * CID. Returns an interface of type aIID off the singleton classobject.
 */
NS_IMETHODIMP
nsComponentManagerImpl::GetClassObject(const nsCID& aClass, const nsIID& aIID,
                                       void** aResult) {
  nsresult rv;

  if (MOZ_LOG_TEST(nsComponentManagerLog, LogLevel::Debug)) {
    char* buf = aClass.ToString();
    PR_LogPrint("nsComponentManager: GetClassObject(%s)", buf);
    if (buf) {
      free(buf);
    }
  }

  MOZ_ASSERT(aResult != nullptr);

  nsCOMPtr<nsIFactory> factory = FindFactory(aClass);
  if (!factory) {
    return NS_ERROR_FACTORY_NOT_REGISTERED;
  }

  rv = factory->QueryInterface(aIID, aResult);

  MOZ_LOG(
      nsComponentManagerLog, LogLevel::Warning,
      ("\t\tGetClassObject() %s", NS_SUCCEEDED(rv) ? "succeeded" : "FAILED"));

  return rv;
}

NS_IMETHODIMP
nsComponentManagerImpl::GetClassObjectByContractID(const char* aContractID,
                                                   const nsIID& aIID,
                                                   void** aResult) {
  if (NS_WARN_IF(!aResult) || NS_WARN_IF(!aContractID)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;

  MOZ_LOG(nsComponentManagerLog, LogLevel::Debug,
          ("nsComponentManager: GetClassObjectByContractID(%s)", aContractID));

  nsCOMPtr<nsIFactory> factory = FindFactory(aContractID, strlen(aContractID));
  if (!factory) {
    return NS_ERROR_FACTORY_NOT_REGISTERED;
  }

  rv = factory->QueryInterface(aIID, aResult);

  MOZ_LOG(nsComponentManagerLog, LogLevel::Warning,
          ("\t\tGetClassObjectByContractID() %s",
           NS_SUCCEEDED(rv) ? "succeeded" : "FAILED"));

  return rv;
}

/**
 * CreateInstance()
 *
 * Create an instance of an object that implements an interface and belongs
 * to the implementation aClass using the factory. The factory is immediately
 * released and not held onto for any longer.
 */
NS_IMETHODIMP
nsComponentManagerImpl::CreateInstance(const nsCID& aClass, const nsIID& aIID,
                                       void** aResult) {
  // test this first, since there's no point in creating a component during
  // shutdown -- whether it's available or not would depend on the order it
  // occurs in the list
  if (gXPCOMShuttingDown) {
    // When processing shutdown, don't process new GetService() requests
#ifdef SHOW_DENIED_ON_SHUTDOWN
    fprintf(stderr,
            "Creating new instance on shutdown. Denied.\n"
            "         CID: %s\n         IID: %s\n",
            AutoIDString(aClass).get(), AutoIDString(aIID).get());
#endif /* SHOW_DENIED_ON_SHUTDOWN */
    return NS_ERROR_UNEXPECTED;
  }

  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = nullptr;

  Maybe<EntryWrapper> entry = LookupByCID(aClass);

  if (!entry) {
    return NS_ERROR_FACTORY_NOT_REGISTERED;
  }

#ifdef SHOW_CI_ON_EXISTING_SERVICE
  if (entry->ServiceInstance()) {
    nsAutoCString message;
    message = "You are calling CreateInstance \""_ns + AutoIDString(aClass) +
              "\" when a service for this CID already exists!"_ns;
    NS_ERROR(message.get());
  }
#endif

  nsresult rv;
  nsCOMPtr<nsIFactory> factory = entry->GetFactory();
  if (factory) {
    rv = factory->CreateInstance(aIID, aResult);
    if (NS_SUCCEEDED(rv) && !*aResult) {
      NS_ERROR("Factory did not return an object but returned success!");
      rv = NS_ERROR_SERVICE_NOT_AVAILABLE;
    }
  } else {
    // Translate error values
    rv = NS_ERROR_FACTORY_NOT_REGISTERED;
  }

  if (MOZ_LOG_TEST(nsComponentManagerLog, LogLevel::Warning)) {
    char* buf = aClass.ToString();
    MOZ_LOG(nsComponentManagerLog, LogLevel::Warning,
            ("nsComponentManager: CreateInstance(%s) %s", buf,
             NS_SUCCEEDED(rv) ? "succeeded" : "FAILED"));
    if (buf) {
      free(buf);
    }
  }

  return rv;
}

/**
 * CreateInstanceByContractID()
 *
 * A variant of CreateInstance() that creates an instance of the object that
 * implements the interface aIID and whose implementation has a contractID
 * aContractID.
 *
 * This is only a convenience routine that turns around can calls the
 * CreateInstance() with classid and iid.
 */
NS_IMETHODIMP
nsComponentManagerImpl::CreateInstanceByContractID(const char* aContractID,
                                                   const nsIID& aIID,
                                                   void** aResult) {
  if (NS_WARN_IF(!aContractID)) {
    return NS_ERROR_INVALID_ARG;
  }

  // test this first, since there's no point in creating a component during
  // shutdown -- whether it's available or not would depend on the order it
  // occurs in the list
  if (gXPCOMShuttingDown) {
    // When processing shutdown, don't process new GetService() requests
#ifdef SHOW_DENIED_ON_SHUTDOWN
    fprintf(stderr,
            "Creating new instance on shutdown. Denied.\n"
            "  ContractID: %s\n         IID: %s\n",
            aContractID, AutoIDString(aIID).get());
#endif /* SHOW_DENIED_ON_SHUTDOWN */
    return NS_ERROR_UNEXPECTED;
  }

  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = nullptr;

  Maybe<EntryWrapper> entry =
      LookupByContractID(nsDependentCString(aContractID));

  if (!entry) {
    return NS_ERROR_FACTORY_NOT_REGISTERED;
  }

#ifdef SHOW_CI_ON_EXISTING_SERVICE
  if (entry->ServiceInstance()) {
    nsAutoCString message;
    message =
        "You are calling CreateInstance \""_ns +
        nsDependentCString(aContractID) +
        nsLiteralCString(
            "\" when a service for this CID already exists! "
            "Add it to abusedContracts to track down the service consumer.");
    NS_ERROR(message.get());
  }
#endif

  nsresult rv;
  nsCOMPtr<nsIFactory> factory = entry->GetFactory();
  if (factory) {
    rv = factory->CreateInstance(aIID, aResult);
    if (NS_SUCCEEDED(rv) && !*aResult) {
      NS_ERROR("Factory did not return an object but returned success!");
      rv = NS_ERROR_SERVICE_NOT_AVAILABLE;
    }
  } else {
    // Translate error values
    rv = NS_ERROR_FACTORY_NOT_REGISTERED;
  }

  MOZ_LOG(nsComponentManagerLog, LogLevel::Warning,
          ("nsComponentManager: CreateInstanceByContractID(%s) %s", aContractID,
           NS_SUCCEEDED(rv) ? "succeeded" : "FAILED"));

  return rv;
}

nsresult nsComponentManagerImpl::FreeServices() {
  NS_ASSERTION(gXPCOMShuttingDown,
               "Must be shutting down in order to free all services");

  if (!gXPCOMShuttingDown) {
    return NS_ERROR_FAILURE;
  }

  for (nsFactoryEntry* entry : mFactories.Values()) {
    entry->mFactory = nullptr;
    entry->mServiceObject = nullptr;
  }

  for (const auto& module : gStaticModules) {
    module.SetServiceInstance(nullptr);
  }

  return NS_OK;
}

// This should only ever be called within the monitor!
nsComponentManagerImpl::PendingServiceInfo*
nsComponentManagerImpl::AddPendingService(const nsCID& aServiceCID,
                                          PRThread* aThread) {
  PendingServiceInfo* newInfo = mPendingServices.AppendElement();
  if (newInfo) {
    newInfo->cid = &aServiceCID;
    newInfo->thread = aThread;
  }
  return newInfo;
}

// This should only ever be called within the monitor!
void nsComponentManagerImpl::RemovePendingService(MonitorAutoLock& aLock,
                                                  const nsCID& aServiceCID) {
  uint32_t pendingCount = mPendingServices.Length();
  for (uint32_t index = 0; index < pendingCount; ++index) {
    const PendingServiceInfo& info = mPendingServices.ElementAt(index);
    if (info.cid->Equals(aServiceCID)) {
      mPendingServices.RemoveElementAt(index);
      aLock.NotifyAll();
      return;
    }
  }
}

// This should only ever be called within the monitor!
PRThread* nsComponentManagerImpl::GetPendingServiceThread(
    const nsCID& aServiceCID) const {
  uint32_t pendingCount = mPendingServices.Length();
  for (uint32_t index = 0; index < pendingCount; ++index) {
    const PendingServiceInfo& info = mPendingServices.ElementAt(index);
    if (info.cid->Equals(aServiceCID)) {
      return info.thread;
    }
  }
  return nullptr;
}

nsresult nsComponentManagerImpl::GetServiceLocked(Maybe<MonitorAutoLock>& aLock,
                                                  EntryWrapper& aEntry,
                                                  const nsIID& aIID,
                                                  void** aResult) {
  MOZ_ASSERT(aLock.isSome());
  if (!aLock.isSome()) {
    return NS_ERROR_INVALID_ARG;
  }

  if (auto* service = aEntry.ServiceInstance()) {
    aLock.reset();
    return service->QueryInterface(aIID, aResult);
  }

  PRThread* currentPRThread = PR_GetCurrentThread();
  MOZ_ASSERT(currentPRThread, "This should never be null!");

  PRThread* pendingPRThread;
  while ((pendingPRThread = GetPendingServiceThread(aEntry.CID()))) {
    if (pendingPRThread == currentPRThread) {
      NS_ERROR("Recursive GetService!");
      return NS_ERROR_NOT_AVAILABLE;
    }

    aLock->Wait();
  }

  // It's still possible that the other thread failed to create the
  // service so we're not guaranteed to have an entry or service yet.
  if (auto* service = aEntry.ServiceInstance()) {
    aLock.reset();
    return service->QueryInterface(aIID, aResult);
  }

  DebugOnly<PendingServiceInfo*> newInfo =
      AddPendingService(aEntry.CID(), currentPRThread);
  NS_ASSERTION(newInfo, "Failed to add info to the array!");

  // We need to not be holding the service manager's lock while calling
  // CreateInstance, because it invokes user code which could try to re-enter
  // the service manager:

  nsCOMPtr<nsISupports> service;
  auto cleanup = MakeScopeExit([&]() {
    // `service` must be released after the lock is released, so if we fail and
    // still have a reference, release the lock before releasing it.
    if (service) {
      MOZ_ASSERT(aLock.isSome());
      aLock.reset();
      service = nullptr;
    }
  });
  nsresult rv;
  mLock.AssertCurrentThreadOwns();
  {
    MonitorAutoUnlock unlock(mLock);
    AUTO_PROFILER_MARKER_TEXT(
        "GetService", OTHER, MarkerStack::Capture(),
        nsDependentCString(nsIDToCString(aEntry.CID()).get()));
    rv = aEntry.CreateInstance(aIID, getter_AddRefs(service));
  }
  if (NS_SUCCEEDED(rv) && !service) {
    NS_ERROR("Factory did not return an object but returned success");
    return NS_ERROR_SERVICE_NOT_AVAILABLE;
  }

#ifdef DEBUG
  pendingPRThread = GetPendingServiceThread(aEntry.CID());
  MOZ_ASSERT(pendingPRThread == currentPRThread,
             "Pending service array has been changed!");
#endif
  MOZ_ASSERT(aLock.isSome());
  RemovePendingService(*aLock, aEntry.CID());

  if (NS_FAILED(rv)) {
    return rv;
  }

  NS_ASSERTION(!aEntry.ServiceInstance(),
               "Created two instances of a service!");

  aEntry.SetServiceInstance(service.forget());

  aLock.reset();

  *aResult = do_AddRef(aEntry.ServiceInstance()).take();
  return NS_OK;
}

NS_IMETHODIMP
nsComponentManagerImpl::GetService(const nsCID& aClass, const nsIID& aIID,
                                   void** aResult) {
  // test this first, since there's no point in returning a service during
  // shutdown -- whether it's available or not would depend on the order it
  // occurs in the list
  if (gXPCOMShuttingDown) {
    // When processing shutdown, don't process new GetService() requests
#ifdef SHOW_DENIED_ON_SHUTDOWN
    fprintf(stderr,
            "Getting service on shutdown. Denied.\n"
            "         CID: %s\n         IID: %s\n",
            AutoIDString(aClass).get(), AutoIDString(aIID).get());
#endif /* SHOW_DENIED_ON_SHUTDOWN */
    return NS_ERROR_UNEXPECTED;
  }

  Maybe<MonitorAutoLock> lock(std::in_place, mLock);

  Maybe<EntryWrapper> entry = LookupByCID(*lock, aClass);
  if (!entry) {
    return NS_ERROR_FACTORY_NOT_REGISTERED;
  }

  return GetServiceLocked(lock, *entry, aIID, aResult);
}

nsresult nsComponentManagerImpl::GetService(ModuleID aId, const nsIID& aIID,
                                            void** aResult) {
  const auto& entry = gStaticModules[size_t(aId)];

  // test this first, since there's no point in returning a service during
  // shutdown -- whether it's available or not would depend on the order it
  // occurs in the list
  if (gXPCOMShuttingDown) {
    // When processing shutdown, don't process new GetService() requests
#ifdef SHOW_DENIED_ON_SHUTDOWN
    fprintf(stderr,
            "Getting service on shutdown. Denied.\n"
            "         CID: %s\n         IID: %s\n",
            AutoIDString(entry.CID()).get(), AutoIDString(aIID).get());
#endif /* SHOW_DENIED_ON_SHUTDOWN */
    return NS_ERROR_UNEXPECTED;
  }

  Maybe<MonitorAutoLock> lock(std::in_place, mLock);

  Maybe<EntryWrapper> wrapper;
  if (entry.Overridable()) {
    // If we expect this service to be overridden by test code, we need to look
    // it up by contract ID every time.
    wrapper = LookupByContractID(*lock, entry.ContractID());
    if (!wrapper) {
      return NS_ERROR_FACTORY_NOT_REGISTERED;
    }
  } else if (!entry.Active()) {
    return NS_ERROR_FACTORY_NOT_REGISTERED;
  } else {
    wrapper.emplace(&entry);
  }
  return GetServiceLocked(lock, *wrapper, aIID, aResult);
}

NS_IMETHODIMP
nsComponentManagerImpl::IsServiceInstantiated(const nsCID& aClass,
                                              const nsIID& aIID,
                                              bool* aResult) {
  // Now we want to get the service if we already got it. If not, we don't want
  // to create an instance of it. mmh!

  // test this first, since there's no point in returning a service during
  // shutdown -- whether it's available or not would depend on the order it
  // occurs in the list
  if (gXPCOMShuttingDown) {
    // When processing shutdown, don't process new GetService() requests
#ifdef SHOW_DENIED_ON_SHUTDOWN
    fprintf(stderr,
            "Checking for service on shutdown. Denied.\n"
            "         CID: %s\n         IID: %s\n",
            AutoIDString(aClass).get(), AutoIDString(aIID).get());
#endif /* SHOW_DENIED_ON_SHUTDOWN */
    return NS_ERROR_UNEXPECTED;
  }

  if (Maybe<EntryWrapper> entry = LookupByCID(aClass)) {
    if (auto* service = entry->ServiceInstance()) {
      nsCOMPtr<nsISupports> instance;
      nsresult rv = service->QueryInterface(aIID, getter_AddRefs(instance));
      *aResult = (instance != nullptr);
      return rv;
    }
  }

  *aResult = false;
  return NS_OK;
}

NS_IMETHODIMP
nsComponentManagerImpl::IsServiceInstantiatedByContractID(
    const char* aContractID, const nsIID& aIID, bool* aResult) {
  // Now we want to get the service if we already got it. If not, we don't want
  // to create an instance of it. mmh!

  // test this first, since there's no point in returning a service during
  // shutdown -- whether it's available or not would depend on the order it
  // occurs in the list
  if (gXPCOMShuttingDown) {
    // When processing shutdown, don't process new GetService() requests
#ifdef SHOW_DENIED_ON_SHUTDOWN
    fprintf(stderr,
            "Checking for service on shutdown. Denied.\n"
            "  ContractID: %s\n         IID: %s\n",
            aContractID, AutoIDString(aIID).get());
#endif /* SHOW_DENIED_ON_SHUTDOWN */
    return NS_ERROR_UNEXPECTED;
  }

  if (Maybe<EntryWrapper> entry =
          LookupByContractID(nsDependentCString(aContractID))) {
    if (auto* service = entry->ServiceInstance()) {
      nsCOMPtr<nsISupports> instance;
      nsresult rv = service->QueryInterface(aIID, getter_AddRefs(instance));
      *aResult = (instance != nullptr);
      return rv;
    }
  }

  *aResult = false;
  return NS_OK;
}

NS_IMETHODIMP
nsComponentManagerImpl::GetServiceByContractID(const char* aContractID,
                                               const nsIID& aIID,
                                               void** aResult) {
  // test this first, since there's no point in returning a service during
  // shutdown -- whether it's available or not would depend on the order it
  // occurs in the list
  if (gXPCOMShuttingDown) {
    // When processing shutdown, don't process new GetService() requests
#ifdef SHOW_DENIED_ON_SHUTDOWN
    fprintf(stderr,
            "Getting service on shutdown. Denied.\n"
            "  ContractID: %s\n         IID: %s\n",
            aContractID, AutoIDString(aIID).get());
#endif /* SHOW_DENIED_ON_SHUTDOWN */
    return NS_ERROR_UNEXPECTED;
  }

  AUTO_PROFILER_LABEL_DYNAMIC_CSTR_NONSENSITIVE("GetServiceByContractID", OTHER,
                                                aContractID);
  Maybe<MonitorAutoLock> lock(std::in_place, mLock);

  Maybe<EntryWrapper> entry =
      LookupByContractID(*lock, nsDependentCString(aContractID));
  if (!entry) {
    return NS_ERROR_FACTORY_NOT_REGISTERED;
  }

  return GetServiceLocked(lock, *entry, aIID, aResult);
}

NS_IMETHODIMP
nsComponentManagerImpl::RegisterFactory(const nsCID& aClass, const char* aName,
                                        const char* aContractID,
                                        nsIFactory* aFactory) {
  if (!aFactory) {
    // If a null factory is passed in, this call just wants to reset
    // the contract ID to point to an existing CID entry.
    if (!aContractID) {
      return NS_ERROR_INVALID_ARG;
    }

    nsDependentCString contractID(aContractID);

    MonitorAutoLock lock(mLock);
    nsFactoryEntry* oldf = mFactories.Get(&aClass);
    if (oldf) {
      StaticComponents::InvalidateContractID(contractID);
      mContractIDs.InsertOrUpdate(contractID, oldf);
      return NS_OK;
    }

    if (StaticComponents::LookupByCID(aClass)) {
      // If this is the CID of a static module, just reset the invalid bit of
      // the static entry for this contract ID, and assume it points to the
      // correct class.
      if (StaticComponents::InvalidateContractID(contractID, false)) {
        mContractIDs.Remove(contractID);
        return NS_OK;
      }
    }
    return NS_ERROR_FACTORY_NOT_REGISTERED;
  }

  auto f = MakeUnique<nsFactoryEntry>(aClass, aFactory);

  MonitorAutoLock lock(mLock);
  return mFactories.WithEntryHandle(f->mCIDEntry->cid, [&](auto&& entry) {
    if (entry) {
      return NS_ERROR_FACTORY_EXISTS;
    }
    if (StaticComponents::LookupByCID(*f->mCIDEntry->cid)) {
      return NS_ERROR_FACTORY_EXISTS;
    }
    if (aContractID) {
      nsDependentCString contractID(aContractID);
      mContractIDs.InsertOrUpdate(contractID, f.get());
      // We allow dynamically-registered contract IDs to override static
      // entries, so invalidate any static entry for this contract ID.
      StaticComponents::InvalidateContractID(contractID);
    }
    entry.Insert(f.release());

    return NS_OK;
  });
}

NS_IMETHODIMP
nsComponentManagerImpl::UnregisterFactory(const nsCID& aClass,
                                          nsIFactory* aFactory) {
  // Don't release the dying factory or service object until releasing
  // the component manager monitor.
  nsCOMPtr<nsIFactory> dyingFactory;
  nsCOMPtr<nsISupports> dyingServiceObject;

  {
    MonitorAutoLock lock(mLock);
    auto entry = mFactories.Lookup(&aClass);
    nsFactoryEntry* f = entry ? entry.Data() : nullptr;
    if (!f || f->mFactory != aFactory) {
      // Note: We do not support unregistering static factories.
      return NS_ERROR_FACTORY_NOT_REGISTERED;
    }

    entry.Remove();

    // This might leave a stale contractid -> factory mapping in
    // place, so null out the factory entry (see
    // nsFactoryEntry::GetFactory)
    f->mFactory.swap(dyingFactory);
    f->mServiceObject.swap(dyingServiceObject);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsComponentManagerImpl::AutoRegister(nsIFile* aLocation) {
  XRE_AddManifestLocation(NS_EXTENSION_LOCATION, aLocation);
  return NS_OK;
}

NS_IMETHODIMP
nsComponentManagerImpl::AutoUnregister(nsIFile* aLocation) {
  NS_ERROR("AutoUnregister not implemented.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsComponentManagerImpl::RegisterFactoryLocation(
    const nsCID& aCID, const char* aClassName, const char* aContractID,
    nsIFile* aFile, const char* aLoaderStr, const char* aType) {
  NS_ERROR("RegisterFactoryLocation not implemented.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsComponentManagerImpl::UnregisterFactoryLocation(const nsCID& aCID,
                                                  nsIFile* aFile) {
  NS_ERROR("UnregisterFactoryLocation not implemented.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsComponentManagerImpl::IsCIDRegistered(const nsCID& aClass, bool* aResult) {
  *aResult = LookupByCID(aClass).isSome();
  return NS_OK;
}

NS_IMETHODIMP
nsComponentManagerImpl::IsContractIDRegistered(const char* aClass,
                                               bool* aResult) {
  if (NS_WARN_IF(!aClass)) {
    return NS_ERROR_INVALID_ARG;
  }

  Maybe<EntryWrapper> entry = LookupByContractID(nsDependentCString(aClass));

  *aResult = entry.isSome();
  return NS_OK;
}

NS_IMETHODIMP
nsComponentManagerImpl::GetContractIDs(nsTArray<nsCString>& aResult) {
  aResult = ToTArray<nsTArray<nsCString>>(mContractIDs.Keys());

  for (const auto& entry : gContractEntries) {
    if (!entry.Invalid()) {
      aResult.AppendElement(entry.ContractID());
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsComponentManagerImpl::CIDToContractID(const nsCID& aClass, char** aResult) {
  NS_ERROR("CIDTOContractID not implemented");
  return NS_ERROR_FACTORY_NOT_REGISTERED;
}

NS_IMETHODIMP
nsComponentManagerImpl::ContractIDToCID(const char* aContractID,
                                        nsCID** aResult) {
  {
    MonitorAutoLock lock(mLock);
    Maybe<EntryWrapper> entry =
        LookupByContractID(lock, nsDependentCString(aContractID));
    if (entry) {
      *aResult = (nsCID*)moz_xmalloc(sizeof(nsCID));
      **aResult = entry->CID();
      return NS_OK;
    }
  }
  *aResult = nullptr;
  return NS_ERROR_FACTORY_NOT_REGISTERED;
}

MOZ_DEFINE_MALLOC_SIZE_OF(ComponentManagerMallocSizeOf)

NS_IMETHODIMP
nsComponentManagerImpl::CollectReports(nsIHandleReportCallback* aHandleReport,
                                       nsISupports* aData, bool aAnonymize) {
  MOZ_COLLECT_REPORT("explicit/xpcom/component-manager", KIND_HEAP, UNITS_BYTES,
                     SizeOfIncludingThis(ComponentManagerMallocSizeOf),
                     "Memory used for the XPCOM component manager.");

  return NS_OK;
}

size_t nsComponentManagerImpl::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(this);

  n += mFactories.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (const auto& data : mFactories.Values()) {
    n += data->SizeOfIncludingThis(aMallocSizeOf);
  }

  n += mContractIDs.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (const auto& key : mContractIDs.Keys()) {
    // We don't measure the nsFactoryEntry data because it's owned by
    // mFactories (which is measured above).
    n += key.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }

  n += sExtraStaticModules->ShallowSizeOfIncludingThis(aMallocSizeOf);
  if (sModuleLocations) {
    n += sModuleLocations->ShallowSizeOfIncludingThis(aMallocSizeOf);
  }

  n += mKnownStaticModules.ShallowSizeOfExcludingThis(aMallocSizeOf);

  n += mPendingServices.ShallowSizeOfExcludingThis(aMallocSizeOf);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mMon
  // - sModuleLocations' entries
  // - mKnownStaticModules' entries?

  return n;
}

////////////////////////////////////////////////////////////////////////////////
// nsFactoryEntry
////////////////////////////////////////////////////////////////////////////////

nsFactoryEntry::nsFactoryEntry(const mozilla::Module::CIDEntry* aEntry,
                               nsComponentManagerImpl::KnownModule* aModule)
    : mCIDEntry(aEntry), mModule(aModule) {}

nsFactoryEntry::nsFactoryEntry(const nsCID& aCID, nsIFactory* aFactory)
    : mCIDEntry(nullptr), mModule(nullptr), mFactory(aFactory) {
  auto* e = new mozilla::Module::CIDEntry();
  auto* cid = new nsCID;
  *cid = aCID;
  e->cid = cid;
  mCIDEntry = e;
}

nsFactoryEntry::~nsFactoryEntry() {
  // If this was a RegisterFactory entry, we own the CIDEntry/CID
  if (!mModule) {
    delete mCIDEntry->cid;
    delete mCIDEntry;
  }
}

already_AddRefed<nsIFactory> nsFactoryEntry::GetFactory() {
  nsComponentManagerImpl::gComponentManager->mLock.AssertNotCurrentThreadOwns();

  if (!mFactory) {
    // RegisterFactory then UnregisterFactory can leave an entry in mContractIDs
    // pointing to an unusable nsFactoryEntry.
    if (!mModule) {
      return nullptr;
    }

    if (!mModule->Load()) {
      return nullptr;
    }

    // Don't set mFactory directly, it needs to be locked
    nsCOMPtr<nsIFactory> factory;

    if (mModule->Module()->getFactoryProc) {
      factory =
          mModule->Module()->getFactoryProc(*mModule->Module(), *mCIDEntry);
    } else if (mCIDEntry->getFactoryProc) {
      factory = mCIDEntry->getFactoryProc(*mModule->Module(), *mCIDEntry);
    } else {
      NS_ASSERTION(mCIDEntry->constructorProc, "no getfactory or constructor");
      factory = new mozilla::GenericFactory(mCIDEntry->constructorProc);
    }
    if (!factory) {
      return nullptr;
    }

    MonitorAutoLock lock(nsComponentManagerImpl::gComponentManager->mLock);
    // Threads can race to set mFactory
    if (!mFactory) {
      factory.swap(mFactory);
    }
  }
  nsCOMPtr<nsIFactory> factory = mFactory;
  return factory.forget();
}

nsresult nsFactoryEntry::CreateInstance(const nsIID& aIID, void** aResult) {
  nsCOMPtr<nsIFactory> factory = GetFactory();
  NS_ENSURE_TRUE(factory, NS_ERROR_FAILURE);
  return factory->CreateInstance(aIID, aResult);
}

size_t nsFactoryEntry::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) {
  size_t n = aMallocSizeOf(this);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mCIDEntry;
  // - mModule;
  // - mFactory;
  // - mServiceObject;

  return n;
}

////////////////////////////////////////////////////////////////////////////////
// Static Access Functions
////////////////////////////////////////////////////////////////////////////////

nsresult NS_GetComponentManager(nsIComponentManager** aResult) {
  if (!nsComponentManagerImpl::gComponentManager) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  NS_ADDREF(*aResult = nsComponentManagerImpl::gComponentManager);
  return NS_OK;
}

nsresult NS_GetServiceManager(nsIServiceManager** aResult) {
  if (!nsComponentManagerImpl::gComponentManager) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  NS_ADDREF(*aResult = nsComponentManagerImpl::gComponentManager);
  return NS_OK;
}

nsresult NS_GetComponentRegistrar(nsIComponentRegistrar** aResult) {
  if (!nsComponentManagerImpl::gComponentManager) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  NS_ADDREF(*aResult = nsComponentManagerImpl::gComponentManager);
  return NS_OK;
}

EXPORT_XPCOM_API(nsresult)
XRE_AddStaticComponent(const mozilla::Module* aComponent) {
  nsComponentManagerImpl::InitializeStaticModules();
  sExtraStaticModules->AppendElement(aComponent);

  if (nsComponentManagerImpl::gComponentManager &&
      nsComponentManagerImpl::NORMAL ==
          nsComponentManagerImpl::gComponentManager->mStatus) {
    nsComponentManagerImpl::gComponentManager->RegisterModule(aComponent);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsComponentManagerImpl::AddBootstrappedManifestLocation(nsIFile* aLocation) {
  NS_ENSURE_ARG_POINTER(aLocation);

  nsString path;
  nsresult rv = aLocation->GetPath(path);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (Substring(path, path.Length() - 4).EqualsLiteral(".xpi")) {
    return XRE_AddJarManifestLocation(NS_BOOTSTRAPPED_LOCATION, aLocation);
  }

  nsCOMPtr<nsIFile> manifest = CloneAndAppend(aLocation, "chrome.manifest"_ns);
  return XRE_AddManifestLocation(NS_BOOTSTRAPPED_LOCATION, manifest);
}

NS_IMETHODIMP
nsComponentManagerImpl::RemoveBootstrappedManifestLocation(nsIFile* aLocation) {
  NS_ENSURE_ARG_POINTER(aLocation);

  nsCOMPtr<nsIChromeRegistry> cr = mozilla::services::GetChromeRegistry();
  if (!cr) {
    return NS_ERROR_FAILURE;
  }

  nsString path;
  nsresult rv = aLocation->GetPath(path);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsComponentManagerImpl::ComponentLocation elem;
  elem.type = NS_BOOTSTRAPPED_LOCATION;

  if (Substring(path, path.Length() - 4).EqualsLiteral(".xpi")) {
    elem.location.Init(aLocation, "chrome.manifest");
  } else {
    nsCOMPtr<nsIFile> lf = CloneAndAppend(aLocation, "chrome.manifest"_ns);
    elem.location.Init(lf);
  }

  // Remove reference.
  nsComponentManagerImpl::sModuleLocations->RemoveElement(
      elem, ComponentLocationComparator());

  rv = cr->CheckForNewChrome();
  return rv;
}

NS_IMETHODIMP
nsComponentManagerImpl::GetComponentJSMs(nsIUTF8StringEnumerator** aJSMs) {
  nsCOMPtr<nsIUTF8StringEnumerator> result =
      StaticComponents::GetComponentJSMs();
  result.forget(aJSMs);
  return NS_OK;
}

NS_IMETHODIMP
nsComponentManagerImpl::GetComponentESModules(
    nsIUTF8StringEnumerator** aESModules) {
  nsCOMPtr<nsIUTF8StringEnumerator> result =
      StaticComponents::GetComponentESModules();
  result.forget(aESModules);
  return NS_OK;
}

NS_IMETHODIMP
nsComponentManagerImpl::GetManifestLocations(nsIArray** aLocations) {
  NS_ENSURE_ARG_POINTER(aLocations);
  *aLocations = nullptr;

  if (!sModuleLocations) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsCOMPtr<nsIMutableArray> locations = nsArray::Create();
  nsresult rv;
  for (uint32_t i = 0; i < sModuleLocations->Length(); ++i) {
    ComponentLocation& l = sModuleLocations->ElementAt(i);
    FileLocation loc = l.location;
    nsCString uriString;
    loc.GetURIString(uriString);
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), uriString);
    if (NS_SUCCEEDED(rv)) {
      locations->AppendElement(uri);
    }
  }

  locations.forget(aLocations);
  return NS_OK;
}

EXPORT_XPCOM_API(nsresult)
XRE_AddManifestLocation(NSLocationType aType, nsIFile* aLocation) {
  nsComponentManagerImpl::InitializeModuleLocations();
  nsComponentManagerImpl::ComponentLocation* c =
      nsComponentManagerImpl::sModuleLocations->AppendElement();
  c->type = aType;
  c->location.Init(aLocation);

  if (nsComponentManagerImpl::gComponentManager &&
      nsComponentManagerImpl::NORMAL ==
          nsComponentManagerImpl::gComponentManager->mStatus) {
    nsComponentManagerImpl::gComponentManager->RegisterManifest(
        aType, c->location, false);
  }

  return NS_OK;
}

EXPORT_XPCOM_API(nsresult)
XRE_AddJarManifestLocation(NSLocationType aType, nsIFile* aLocation) {
  nsComponentManagerImpl::InitializeModuleLocations();
  nsComponentManagerImpl::ComponentLocation* c =
      nsComponentManagerImpl::sModuleLocations->AppendElement();

  c->type = aType;
  c->location.Init(aLocation, "chrome.manifest");

  if (nsComponentManagerImpl::gComponentManager &&
      nsComponentManagerImpl::NORMAL ==
          nsComponentManagerImpl::gComponentManager->mStatus) {
    nsComponentManagerImpl::gComponentManager->RegisterManifest(
        aType, c->location, false);
  }

  return NS_OK;
}

// Expose some important global interfaces to rust for the rust xpcom API. These
// methods return a non-owning reference to the component manager, which should
// live for the lifetime of XPCOM.
extern "C" {

const nsIComponentManager* Gecko_GetComponentManager() {
  return nsComponentManagerImpl::gComponentManager;
}

const nsIServiceManager* Gecko_GetServiceManager() {
  return nsComponentManagerImpl::gComponentManager;
}

const nsIComponentRegistrar* Gecko_GetComponentRegistrar() {
  return nsComponentManagerImpl::gComponentManager;
}
}
