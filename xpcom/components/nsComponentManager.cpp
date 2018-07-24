/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include "nscore.h"
#include "nsISupports.h"
#include "nspr.h"
#include "nsCRT.h" // for atoll

#include "nsCategoryManager.h"
#include "nsCOMPtr.h"
#include "nsComponentManager.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsCategoryManager.h"
#include "nsCategoryManagerUtils.h"
#include "mozilla/MemoryReporting.h"
#include "nsIConsoleService.h"
#include "nsIObserverService.h"
#include "nsISimpleEnumerator.h"
#include "nsIStringEnumerator.h"
#include "nsXPCOM.h"
#include "nsXPCOMPrivate.h"
#include "nsISupportsPrimitives.h"
#include "nsIClassInfo.h"
#include "nsLocalFile.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "prcmon.h"
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
#include "nsArrayEnumerator.h"
#include "nsStringEnumerator.h"
#include "mozilla/FileUtils.h"
#include "mozilla/URLPreloader.h"
#include "mozilla/UniquePtr.h"
#include "nsDataHashtable.h"

#include <new>     // for placement new

#include "mozilla/Omnijar.h"

#include "mozilla/Logging.h"
#include "LogModulePrefWatcher.h"

#ifdef MOZ_MEMORY
#include "mozmemory.h"
#endif

using namespace mozilla;

static LazyLogModule nsComponentManagerLog("nsComponentManager");

#if 0
 #define SHOW_DENIED_ON_SHUTDOWN
 #define SHOW_CI_ON_EXISTING_SERVICE
#endif

// Bloated registry buffer size to improve startup performance -- needs to
// be big enough to fit the entire file into memory or it'll thrash.
// 512K is big enough to allow for some future growth in the registry.
#define BIG_REGISTRY_BUFLEN   (512*1024)

// Common Key Names
const char xpcomComponentsKeyName[] = "software/mozilla/XPCOM/components";
const char xpcomKeyName[] = "software/mozilla/XPCOM";

// Common Value Names
const char fileSizeValueName[] = "FileSize";
const char lastModValueName[] = "LastModTimeStamp";
const char nativeComponentType[] = "application/x-mozilla-native";
const char staticComponentType[] = "application/x-mozilla-static";

NS_DEFINE_CID(kCategoryManagerCID, NS_CATEGORYMANAGER_CID);

#define UID_STRING_LENGTH 39

nsresult
nsGetServiceFromCategory::operator()(const nsIID& aIID,
                                     void** aInstancePtr) const
{
  nsresult rv;
  nsCString value;
  nsCOMPtr<nsICategoryManager> catman;
  nsComponentManagerImpl* compMgr = nsComponentManagerImpl::gComponentManager;
  if (!compMgr) {
    rv = NS_ERROR_NOT_INITIALIZED;
    goto error;
  }

  rv = compMgr->nsComponentManagerImpl::GetService(kCategoryManagerCID,
                                                   NS_GET_IID(nsICategoryManager),
                                                   getter_AddRefs(catman));
  if (NS_FAILED(rv)) {
    goto error;
  }

  /* find the contractID for category.entry */
  rv = catman->GetCategoryEntry(mCategory, mEntry, value);
  if (NS_FAILED(rv)) {
    goto error;
  }
  if (value.IsVoid()) {
    rv = NS_ERROR_SERVICE_NOT_AVAILABLE;
    goto error;
  }

  rv = compMgr->nsComponentManagerImpl::GetServiceByContractID(value.get(),
                                                               aIID,
                                                               aInstancePtr);
  if (NS_FAILED(rv)) {
error:
    *aInstancePtr = 0;
  }
  if (mErrorPtr) {
    *mErrorPtr = rv;
  }
  return rv;
}

// GetService and a few other functions need to exit their mutex mid-function
// without reentering it later in the block. This class supports that
// style of early-exit that MutexAutoUnlock doesn't.

namespace {

class MOZ_STACK_CLASS MutexLock
{
public:
  explicit MutexLock(SafeMutex& aMutex)
    : mMutex(aMutex)
    , mLocked(false)
  {
    Lock();
  }

  ~MutexLock()
  {
    if (mLocked) {
      Unlock();
    }
  }

  void Lock()
  {
    NS_ASSERTION(!mLocked, "Re-entering a mutex");
    mMutex.Lock();
    mLocked = true;
  }

  void Unlock()
  {
    NS_ASSERTION(mLocked, "Exiting a mutex that isn't held!");
    mMutex.Unlock();
    mLocked = false;
  }

private:
  SafeMutex& mMutex;
  bool mLocked;
};

} // namespace

// this is safe to call during InitXPCOM
static already_AddRefed<nsIFile>
GetLocationFromDirectoryService(const char* aProp)
{
  nsCOMPtr<nsIProperties> directoryService;
  nsDirectoryService::Create(nullptr,
                             NS_GET_IID(nsIProperties),
                             getter_AddRefs(directoryService));

  if (!directoryService) {
    return nullptr;
  }

  nsCOMPtr<nsIFile> file;
  nsresult rv = directoryService->Get(aProp,
                                      NS_GET_IID(nsIFile),
                                      getter_AddRefs(file));
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return file.forget();
}

static already_AddRefed<nsIFile>
CloneAndAppend(nsIFile* aBase, const nsACString& aAppend)
{
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

nsresult
nsComponentManagerImpl::Create(nsISupports* aOuter, REFNSIID aIID,
                               void** aResult)
{
  if (aOuter) {
    return NS_ERROR_NO_AGGREGATION;
  }

  if (!gComponentManager) {
    return NS_ERROR_FAILURE;
  }

  return gComponentManager->QueryInterface(aIID, aResult);
}

static const int CONTRACTID_HASHTABLE_INITIAL_LENGTH = 1024;

nsComponentManagerImpl::nsComponentManagerImpl()
  : mFactories(CONTRACTID_HASHTABLE_INITIAL_LENGTH)
  , mContractIDs(CONTRACTID_HASHTABLE_INITIAL_LENGTH)
  , mLock("nsComponentManagerImpl.mLock")
  , mStatus(NOT_INITIALIZED)
{
}

static nsTArray<const mozilla::Module*>* sExtraStaticModules;

/* NSMODULE_DEFN places NSModules in specific sections, as per Module.h.
 * The linker will group them all together, and we use tricks below to
 * find the start and end of the grouped list of NSModules.
 *
 * On Windows, all the symbols in the .kPStaticModules* sections are
 * grouped together, by lexical order of the section names. The NSModules
 * themselves are in .kPStaticModules$M. We use the section name
 * .kPStaticModules$A to add an empty entry that will be the first,
 * and the section name .kPStaticModules$Z for another empty entry that
 * will be the last. We make both null pointers, and skip them in the
 * AllStaticModules range-iterator.
 *
 * On ELF (Linux, BSDs, ...), as well as Mingw builds, the linker itself
 * provides symbols for the beginning and end of the consolidated section,
 * but it only does so for sections that can be represented as C identifiers,
 * so the section is named `kPStaticModules` rather than `.kPStaticModules`.
 *
 * We also use a linker script with BFD ld so that the sections end up
 * folded into the .data.rel.ro section, but that actually breaks the above
 * described behavior, so the linker script contains an additional trick
 * to still provide the __start and __stop symbols (the linker script
 * doesn't work with gold or lld).
 *
 * On Darwin, a similar setup is available through the use of some
 * synthesized symbols (section$...).
 *
 * On all platforms, the __stop_kPStaticModules symbol is past all NSModule
 * pointers.
 * On Windows, the __start_kPStaticModules symbol points to an empty pointer
 * preceding the first NSModule pointer. On other platforms, it points to the
 * first NSModule pointer.
 */

// Dummy class to define a range-iterator for the static modules.
class AllStaticModules {};

#if defined(_MSC_VER)

#  pragma section(".kPStaticModules$A", read)
NSMODULE_ASAN_BLACKLIST __declspec(allocate(".kPStaticModules$A"), dllexport)
extern mozilla::Module const* const __start_kPStaticModules = nullptr;

mozilla::Module const* const* begin(AllStaticModules& _) {
    return &__start_kPStaticModules + 1;
}

#  pragma section(".kPStaticModules$Z", read)
NSMODULE_ASAN_BLACKLIST __declspec(allocate(".kPStaticModules$Z"), dllexport)
extern mozilla::Module const* const __stop_kPStaticModules = nullptr;

#else

#  if defined(__ELF__) || (defined(_WIN32) && defined(__GNUC__))

extern "C" mozilla::Module const* const __start_kPStaticModules;
extern "C" mozilla::Module const* const __stop_kPStaticModules;

#  elif defined(__MACH__)

extern mozilla::Module const *const __start_kPStaticModules __asm("section$start$__DATA$.kPStaticModules");
extern mozilla::Module const* const __stop_kPStaticModules __asm("section$end$__DATA$.kPStaticModules");

#  else
#    error Do not know how to find NSModules.
#  endif

mozilla::Module const* const* begin(AllStaticModules& _) {
    return &__start_kPStaticModules;
}

#endif

mozilla::Module const* const* end(AllStaticModules& _) {
    return &__stop_kPStaticModules;
}

/* static */ void
nsComponentManagerImpl::InitializeStaticModules()
{
  if (sExtraStaticModules) {
    return;
  }

  sExtraStaticModules = new nsTArray<const mozilla::Module*>;
}

nsTArray<nsComponentManagerImpl::ComponentLocation>*
nsComponentManagerImpl::sModuleLocations;

/* static */ void
nsComponentManagerImpl::InitializeModuleLocations()
{
  if (sModuleLocations) {
    return;
  }

  sModuleLocations = new nsTArray<ComponentLocation>;
}

nsresult
nsComponentManagerImpl::Init()
{
  MOZ_ASSERT(NOT_INITIALIZED == mStatus);

  nsCOMPtr<nsIFile> greDir =
    GetLocationFromDirectoryService(NS_GRE_DIR);
  nsCOMPtr<nsIFile> appDir =
    GetLocationFromDirectoryService(NS_XPCOM_CURRENT_PROCESS_DIR);

  InitializeStaticModules();

  nsCategoryManager::GetSingleton()->SuppressNotifications(true);

  RegisterModule(&kXPCOMModule, nullptr);

  for (auto module : AllStaticModules()) {
    if (module) { // On local Windows builds, the list may contain null
                  // pointers from padding.
      RegisterModule(module, nullptr);
    }
  }

  for (uint32_t i = 0; i < sExtraStaticModules->Length(); ++i) {
    RegisterModule((*sExtraStaticModules)[i], nullptr);
  }

  bool loadChromeManifests = (XRE_GetProcessType() != GeckoProcessType_GPU);
  if (loadChromeManifests) {
    // The overall order in which chrome.manifests are expected to be treated
    // is the following:
    // - greDir
    // - greDir's omni.ja
    // - appDir
    // - appDir's omni.ja

    InitializeModuleLocations();
    ComponentLocation* cl = sModuleLocations->AppendElement();
    nsCOMPtr<nsIFile> lf = CloneAndAppend(greDir,
                                          NS_LITERAL_CSTRING("chrome.manifest"));
    cl->type = NS_APP_LOCATION;
    cl->location.Init(lf);

    RefPtr<nsZipArchive> greOmnijar =
      mozilla::Omnijar::GetReader(mozilla::Omnijar::GRE);
    if (greOmnijar) {
      cl = sModuleLocations->AppendElement();
      cl->type = NS_APP_LOCATION;
      cl->location.Init(greOmnijar, "chrome.manifest");
    }

    bool equals = false;
    appDir->Equals(greDir, &equals);
    if (!equals) {
      cl = sModuleLocations->AppendElement();
      cl->type = NS_APP_LOCATION;
      lf = CloneAndAppend(appDir, NS_LITERAL_CSTRING("chrome.manifest"));
      cl->location.Init(lf);
    }

    RefPtr<nsZipArchive> appOmnijar =
      mozilla::Omnijar::GetReader(mozilla::Omnijar::APP);
    if (appOmnijar) {
      cl = sModuleLocations->AppendElement();
      cl->type = NS_APP_LOCATION;
      cl->location.Init(appOmnijar, "chrome.manifest");
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

  return NS_OK;
}

static bool
ProcessSelectorMatches(Module::ProcessSelector aSelector)
{
  GeckoProcessType type = XRE_GetProcessType();
  if (type == GeckoProcessType_GPU) {
    return !!(aSelector & Module::ALLOW_IN_GPU_PROCESS);
  }

  if (aSelector & Module::MAIN_PROCESS_ONLY) {
    return type == GeckoProcessType_Default;
  }
  if (aSelector & Module::CONTENT_PROCESS_ONLY) {
    return type == GeckoProcessType_Content;
  }
  return true;
}

static const int kModuleVersionWithSelector = 51;

template<typename T>
static void
AssertNotMallocAllocated(T* aPtr)
{
#if defined(DEBUG) && defined(MOZ_MEMORY)
  jemalloc_ptr_info_t info;
  jemalloc_ptr_info((void*)aPtr, &info);
  MOZ_ASSERT(info.tag == TagUnknown);
#endif
}

template<typename T>
static void
AssertNotStackAllocated(T* aPtr)
{
  // The main thread's stack should be allocated at the top of our address
  // space. Anything stack allocated should be above us on the stack, and
  // therefore above our first argument pointer.
  // Only this is apparently not the case on Windows.
#ifndef XP_WIN
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(uintptr_t(aPtr) < uintptr_t(&aPtr));
#endif
}

static inline nsCString
AsLiteralCString(const char* aStr)
{
  AssertNotMallocAllocated(aStr);
  AssertNotStackAllocated(aStr);

  nsCString str;
  str.AssignLiteral(aStr, strlen(aStr));
  return str;
}

void
nsComponentManagerImpl::RegisterModule(const mozilla::Module* aModule,
                                       FileLocation* aFile)
{
  mLock.AssertNotCurrentThreadOwns();

  if (aModule->mVersion >= kModuleVersionWithSelector &&
      !ProcessSelectorMatches(aModule->selector))
  {
    return;
  }

  {
    // Scope the monitor so that we don't hold it while calling into the
    // category manager.
    MutexLock lock(mLock);

    KnownModule* m;
    if (aFile) {
      nsCString uri;
      aFile->GetURIString(uri);
      NS_ASSERTION(!mKnownModules.Get(uri),
                   "Must not register a binary module twice.");

      m = new KnownModule(aModule, *aFile);
      mKnownModules.Put(uri, m);
    } else {
      m = new KnownModule(aModule);
      mKnownStaticModules.AppendElement(m);
    }

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
          AsLiteralCString(entry->category),
          AsLiteralCString(entry->entry),
          AsLiteralCString(entry->value));
  }
}

void
nsComponentManagerImpl::RegisterCIDEntryLocked(
    const mozilla::Module::CIDEntry* aEntry,
    KnownModule* aModule)
{
  mLock.AssertCurrentThreadOwns();

  if (!ProcessSelectorMatches(aEntry->processSelector)) {
    return;
  }

  if (auto entry = mFactories.LookupForAdd(aEntry->cid)) {
    nsFactoryEntry* f = entry.Data();
    NS_WARNING("Re-registering a CID?");

    char idstr[NSID_LENGTH];
    aEntry->cid->ToProvidedString(idstr);

    nsCString existing;
    if (f->mModule) {
      existing = f->mModule->Description();
    } else {
      existing = "<unknown module>";
    }
    SafeMutexAutoUnlock unlock(mLock);
    LogMessage("While registering XPCOM module %s, trying to re-register CID '%s' already registered by %s.",
               aModule->Description().get(),
               idstr,
               existing.get());
  } else {
    entry.OrInsert([aEntry, aModule] () { return new nsFactoryEntry(aEntry, aModule); });
  }
}

void
nsComponentManagerImpl::RegisterContractIDLocked(
    const mozilla::Module::ContractIDEntry* aEntry)
{
  mLock.AssertCurrentThreadOwns();

  if (!ProcessSelectorMatches(aEntry->processSelector)) {
    return;
  }

  nsFactoryEntry* f = mFactories.Get(aEntry->cid);
  if (!f) {
    NS_WARNING("No CID found when attempting to map contract ID");

    char idstr[NSID_LENGTH];
    aEntry->cid->ToProvidedString(idstr);

    SafeMutexAutoUnlock unlock(mLock);
    LogMessage("Could not map contract ID '%s' to CID %s because no implementation of the CID is registered.",
               aEntry->contractid,
               idstr);

    return;
  }

  mContractIDs.Put(AsLiteralCString(aEntry->contractid), f);
}

static void
CutExtension(nsCString& aPath)
{
  int32_t dotPos = aPath.RFindChar('.');
  if (kNotFound == dotPos) {
    aPath.Truncate();
  } else {
    aPath.Cut(0, dotPos + 1);
  }
}

static void
DoRegisterManifest(NSLocationType aType,
                   FileLocation& aFile,
                   bool aChromeOnly)
{
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

void
nsComponentManagerImpl::RegisterManifest(NSLocationType aType,
                                         FileLocation& aFile,
                                         bool aChromeOnly)
{
  DoRegisterManifest(aType, aFile, aChromeOnly);
}

void
nsComponentManagerImpl::ManifestManifest(ManifestProcessingContext& aCx,
                                         int aLineNo, char* const* aArgv)
{
  char* file = aArgv[0];
  FileLocation f(aCx.mFile, file);
  RegisterManifest(aCx.mType, f, aCx.mChromeOnly);
}

void
nsComponentManagerImpl::ManifestBinaryComponent(ManifestProcessingContext& aCx,
                                                int aLineNo,
                                                char* const* aArgv)
{
  LogMessageWithContext(aCx.mFile, aLineNo,
                        "Binary XPCOM components are no longer supported.");
}

void
nsComponentManagerImpl::ManifestComponent(ManifestProcessingContext& aCx,
                                          int aLineNo, char* const* aArgv)
{
  mLock.AssertNotCurrentThreadOwns();

  char* id = aArgv[0];
  char* file = aArgv[1];

  nsID cid;
  if (!cid.Parse(id)) {
    LogMessageWithContext(aCx.mFile, aLineNo,
                          "Malformed CID: '%s'.", id);
    return;
  }

  // Precompute the hash/file data outside of the lock
  FileLocation fl(aCx.mFile, file);
  nsCString hash;
  fl.GetURIString(hash);

  MutexLock lock(mLock);
  nsFactoryEntry* f = mFactories.Get(&cid);
  if (f) {
    char idstr[NSID_LENGTH];
    cid.ToProvidedString(idstr);

    nsCString existing;
    if (f->mModule) {
      existing = f->mModule->Description();
    } else {
      existing = "<unknown module>";
    }

    lock.Unlock();

    LogMessageWithContext(aCx.mFile, aLineNo,
                          "Trying to re-register CID '%s' already registered by %s.",
                          idstr,
                          existing.get());
    return;
  }

  KnownModule* km;

  km = mKnownModules.Get(hash);
  if (!km) {
    km = new KnownModule(fl);
    mKnownModules.Put(hash, km);
  }

  void* place = mArena.Allocate(sizeof(nsCID));
  nsID* permanentCID = static_cast<nsID*>(place);
  *permanentCID = cid;

  place = mArena.Allocate(sizeof(mozilla::Module::CIDEntry));
  auto* e = new (KnownNotNull, place) mozilla::Module::CIDEntry();
  e->cid = permanentCID;

  mFactories.Put(permanentCID, new nsFactoryEntry(e, km));
}

void
nsComponentManagerImpl::ManifestContract(ManifestProcessingContext& aCx,
                                         int aLineNo, char* const* aArgv)
{
  mLock.AssertNotCurrentThreadOwns();

  char* contract = aArgv[0];
  char* id = aArgv[1];

  nsID cid;
  if (!cid.Parse(id)) {
    LogMessageWithContext(aCx.mFile, aLineNo,
                          "Malformed CID: '%s'.", id);
    return;
  }

  MutexLock lock(mLock);
  nsFactoryEntry* f = mFactories.Get(&cid);
  if (!f) {
    lock.Unlock();
    LogMessageWithContext(aCx.mFile, aLineNo,
                          "Could not map contract ID '%s' to CID %s because no implementation of the CID is registered.",
                          contract, id);
    return;
  }

  mContractIDs.Put(nsDependentCString(contract), f);
}

void
nsComponentManagerImpl::ManifestCategory(ManifestProcessingContext& aCx,
                                         int aLineNo, char* const* aArgv)
{
  char* category = aArgv[0];
  char* key = aArgv[1];
  char* value = aArgv[2];

  nsCategoryManager::GetSingleton()->
  AddCategoryEntry(nsDependentCString(category), nsDependentCString(key),
                   nsDependentCString(value));
}

void
nsComponentManagerImpl::RereadChromeManifests(bool aChromeOnly)
{
  for (uint32_t i = 0; i < sModuleLocations->Length(); ++i) {
    ComponentLocation& l = sModuleLocations->ElementAt(i);
    RegisterManifest(l.type, l.location, aChromeOnly);
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nullptr, "chrome-manifests-loaded", nullptr);
  }
}

bool
nsComponentManagerImpl::KnownModule::EnsureLoader()
{
  if (!mLoader) {
    nsCString extension;
    mFile.GetURIString(extension);
    CutExtension(extension);
    mLoader =
      nsComponentManagerImpl::gComponentManager->LoaderForExtension(extension);
  }
  return !!mLoader;
}

bool
nsComponentManagerImpl::KnownModule::Load()
{
  if (mFailed) {
    return false;
  }
  if (!mModule) {
    if (!EnsureLoader()) {
      return false;
    }

    mModule = mLoader->LoadModule(mFile);

    if (!mModule) {
      mFailed = true;
      return false;
    }
  }
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

nsCString
nsComponentManagerImpl::KnownModule::Description() const
{
  nsCString s;
  if (mFile) {
    mFile.GetURIString(s);
  } else {
    s = "<static module>";
  }
  return s;
}

nsresult nsComponentManagerImpl::Shutdown(void)
{
  MOZ_ASSERT(NORMAL == mStatus);

  mStatus = SHUTDOWN_IN_PROGRESS;

  // Shutdown the component manager
  MOZ_LOG(nsComponentManagerLog, LogLevel::Debug,
         ("nsComponentManager: Beginning Shutdown."));

  UnregisterWeakMemoryReporter(this);

  // Release all cached factories
  mContractIDs.Clear();
  mFactories.Clear(); // XXX release the objects, don't just clear
  mLoaderMap.Clear();
  mKnownModules.Clear();
  mKnownStaticModules.Clear();

  delete sExtraStaticModules;
  delete sModuleLocations;

  mStatus = SHUTDOWN_COMPLETE;

  MOZ_LOG(nsComponentManagerLog, LogLevel::Debug,
         ("nsComponentManager: Shutdown complete."));

  return NS_OK;
}

nsComponentManagerImpl::~nsComponentManagerImpl()
{
  MOZ_LOG(nsComponentManagerLog, LogLevel::Debug,
         ("nsComponentManager: Beginning destruction."));

  if (SHUTDOWN_COMPLETE != mStatus) {
    Shutdown();
  }

  MOZ_LOG(nsComponentManagerLog, LogLevel::Debug,
         ("nsComponentManager: Destroyed."));
}

NS_IMPL_ISUPPORTS(nsComponentManagerImpl,
                  nsIComponentManager,
                  nsIServiceManager,
                  nsIComponentRegistrar,
                  nsISupportsWeakReference,
                  nsIInterfaceRequestor,
                  nsIMemoryReporter)

nsresult
nsComponentManagerImpl::GetInterface(const nsIID& aUuid, void** aResult)
{
  NS_WARNING("This isn't supported");
  // fall through to QI as anything QIable is a superset of what can be
  // got via the GetInterface()
  return  QueryInterface(aUuid, aResult);
}

nsFactoryEntry*
nsComponentManagerImpl::GetFactoryEntry(const char* aContractID,
                                        uint32_t aContractIDLen)
{
  SafeMutexAutoLock lock(mLock);
  return mContractIDs.Get(nsDependentCString(aContractID, aContractIDLen));
}


nsFactoryEntry*
nsComponentManagerImpl::GetFactoryEntry(const nsCID& aClass)
{
  SafeMutexAutoLock lock(mLock);
  return mFactories.Get(&aClass);
}

already_AddRefed<nsIFactory>
nsComponentManagerImpl::FindFactory(const nsCID& aClass)
{
  nsFactoryEntry* e = GetFactoryEntry(aClass);
  if (!e) {
    return nullptr;
  }

  return e->GetFactory();
}

already_AddRefed<nsIFactory>
nsComponentManagerImpl::FindFactory(const char* aContractID,
                                    uint32_t aContractIDLen)
{
  nsFactoryEntry* entry = GetFactoryEntry(aContractID, aContractIDLen);
  if (!entry) {
    return nullptr;
  }

  return entry->GetFactory();
}

/**
 * GetClassObject()
 *
 * Given a classID, this finds the singleton ClassObject that implements the CID.
 * Returns an interface of type aIID off the singleton classobject.
 */
NS_IMETHODIMP
nsComponentManagerImpl::GetClassObject(const nsCID& aClass, const nsIID& aIID,
                                       void** aResult)
{
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

  MOZ_LOG(nsComponentManagerLog, LogLevel::Warning,
         ("\t\tGetClassObject() %s", NS_SUCCEEDED(rv) ? "succeeded" : "FAILED"));

  return rv;
}


NS_IMETHODIMP
nsComponentManagerImpl::GetClassObjectByContractID(const char* aContractID,
                                                   const nsIID& aIID,
                                                   void** aResult)
{
  if (NS_WARN_IF(!aResult) ||
      NS_WARN_IF(!aContractID)) {
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
         ("\t\tGetClassObjectByContractID() %s", NS_SUCCEEDED(rv) ? "succeeded" : "FAILED"));

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
nsComponentManagerImpl::CreateInstance(const nsCID& aClass,
                                       nsISupports* aDelegate,
                                       const nsIID& aIID,
                                       void** aResult)
{
  // test this first, since there's no point in creating a component during
  // shutdown -- whether it's available or not would depend on the order it
  // occurs in the list
  if (gXPCOMShuttingDown) {
    // When processing shutdown, don't process new GetService() requests
#ifdef SHOW_DENIED_ON_SHUTDOWN
    char cid[NSID_LENGTH], iid[NSID_LENGTH];
    aClass.ToProvidedString(cid);
    aIID.ToProvidedString(iid);
    fprintf(stderr, "Creating new instance on shutdown. Denied.\n"
            "         CID: %s\n         IID: %s\n", cid, iid);
#endif /* SHOW_DENIED_ON_SHUTDOWN */
    return NS_ERROR_UNEXPECTED;
  }

  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = nullptr;

  nsFactoryEntry* entry = GetFactoryEntry(aClass);

  if (!entry) {
    return NS_ERROR_FACTORY_NOT_REGISTERED;
  }

#ifdef SHOW_CI_ON_EXISTING_SERVICE
  if (entry->mServiceObject) {
    char cid[NSID_LENGTH];
    aClass.ToProvidedString(cid);
    nsAutoCString message;
    message = NS_LITERAL_CSTRING("You are calling CreateInstance \"") +
              nsDependentCString(cid) +
              NS_LITERAL_CSTRING("\" when a service for this CID already exists!");
    NS_ERROR(message.get());
  }
#endif

  nsresult rv;
  nsCOMPtr<nsIFactory> factory = entry->GetFactory();
  if (factory) {
    rv = factory->CreateInstance(aDelegate, aIID, aResult);
    if (NS_SUCCEEDED(rv) && !*aResult) {
      NS_ERROR("Factory did not return an object but returned success!");
      rv = NS_ERROR_SERVICE_NOT_FOUND;
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
 * implements the interface aIID and whose implementation has a contractID aContractID.
 *
 * This is only a convenience routine that turns around can calls the
 * CreateInstance() with classid and iid.
 */
NS_IMETHODIMP
nsComponentManagerImpl::CreateInstanceByContractID(const char* aContractID,
                                                   nsISupports* aDelegate,
                                                   const nsIID& aIID,
                                                   void** aResult)
{
  if (NS_WARN_IF(!aContractID)) {
    return NS_ERROR_INVALID_ARG;
  }

  // test this first, since there's no point in creating a component during
  // shutdown -- whether it's available or not would depend on the order it
  // occurs in the list
  if (gXPCOMShuttingDown) {
    // When processing shutdown, don't process new GetService() requests
#ifdef SHOW_DENIED_ON_SHUTDOWN
    char iid[NSID_LENGTH];
    aIID.ToProvidedString(iid);
    fprintf(stderr, "Creating new instance on shutdown. Denied.\n"
            "  ContractID: %s\n         IID: %s\n", aContractID, iid);
#endif /* SHOW_DENIED_ON_SHUTDOWN */
    return NS_ERROR_UNEXPECTED;
  }

  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = nullptr;

  nsFactoryEntry* entry = GetFactoryEntry(aContractID, strlen(aContractID));

  if (!entry) {
    return NS_ERROR_FACTORY_NOT_REGISTERED;
  }

#ifdef SHOW_CI_ON_EXISTING_SERVICE
  if (entry->mServiceObject) {
    nsAutoCString message;
    message =
      NS_LITERAL_CSTRING("You are calling CreateInstance \"") +
      nsDependentCString(aContractID) +
      NS_LITERAL_CSTRING("\" when a service for this CID already exists! "
                         "Add it to abusedContracts to track down the service consumer.");
    NS_ERROR(message.get());
  }
#endif

  nsresult rv;
  nsCOMPtr<nsIFactory> factory = entry->GetFactory();
  if (factory) {

    rv = factory->CreateInstance(aDelegate, aIID, aResult);
    if (NS_SUCCEEDED(rv) && !*aResult) {
      NS_ERROR("Factory did not return an object but returned success!");
      rv = NS_ERROR_SERVICE_NOT_FOUND;
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

nsresult
nsComponentManagerImpl::FreeServices()
{
  NS_ASSERTION(gXPCOMShuttingDown,
               "Must be shutting down in order to free all services");

  if (!gXPCOMShuttingDown) {
    return NS_ERROR_FAILURE;
  }

  for (auto iter = mFactories.Iter(); !iter.Done(); iter.Next()) {
    nsFactoryEntry* entry = iter.UserData();
    entry->mFactory = nullptr;
    entry->mServiceObject = nullptr;
  }

  return NS_OK;
}

// This should only ever be called within the monitor!
nsComponentManagerImpl::PendingServiceInfo*
nsComponentManagerImpl::AddPendingService(const nsCID& aServiceCID,
                                          PRThread* aThread)
{
  PendingServiceInfo* newInfo = mPendingServices.AppendElement();
  if (newInfo) {
    newInfo->cid = &aServiceCID;
    newInfo->thread = aThread;
  }
  return newInfo;
}

// This should only ever be called within the monitor!
void
nsComponentManagerImpl::RemovePendingService(const nsCID& aServiceCID)
{
  uint32_t pendingCount = mPendingServices.Length();
  for (uint32_t index = 0; index < pendingCount; ++index) {
    const PendingServiceInfo& info = mPendingServices.ElementAt(index);
    if (info.cid->Equals(aServiceCID)) {
      mPendingServices.RemoveElementAt(index);
      return;
    }
  }
}

// This should only ever be called within the monitor!
PRThread*
nsComponentManagerImpl::GetPendingServiceThread(const nsCID& aServiceCID) const
{
  uint32_t pendingCount = mPendingServices.Length();
  for (uint32_t index = 0; index < pendingCount; ++index) {
    const PendingServiceInfo& info = mPendingServices.ElementAt(index);
    if (info.cid->Equals(aServiceCID)) {
      return info.thread;
    }
  }
  return nullptr;
}

NS_IMETHODIMP
nsComponentManagerImpl::GetService(const nsCID& aClass,
                                   const nsIID& aIID,
                                   void** aResult)
{
  // test this first, since there's no point in returning a service during
  // shutdown -- whether it's available or not would depend on the order it
  // occurs in the list
  if (gXPCOMShuttingDown) {
    // When processing shutdown, don't process new GetService() requests
#ifdef SHOW_DENIED_ON_SHUTDOWN
    char cid[NSID_LENGTH], iid[NSID_LENGTH];
    aClass.ToProvidedString(cid);
    aIID.ToProvidedString(iid);
    fprintf(stderr, "Getting service on shutdown. Denied.\n"
            "         CID: %s\n         IID: %s\n", cid, iid);
#endif /* SHOW_DENIED_ON_SHUTDOWN */
    return NS_ERROR_UNEXPECTED;
  }

  // `service` must be released after the lock is released, so it must be
  // declared before the lock in this C++ block.
  nsCOMPtr<nsISupports> service;
  MutexLock lock(mLock);

  nsFactoryEntry* entry = mFactories.Get(&aClass);
  if (!entry) {
    return NS_ERROR_FACTORY_NOT_REGISTERED;
  }

  if (entry->mServiceObject) {
    lock.Unlock();
    return entry->mServiceObject->QueryInterface(aIID, aResult);
  }

  PRThread* currentPRThread = PR_GetCurrentThread();
  MOZ_ASSERT(currentPRThread, "This should never be null!");

  // Needed to optimize the event loop below.
  nsIThread* currentThread = nullptr;

  PRThread* pendingPRThread;
  while ((pendingPRThread = GetPendingServiceThread(aClass))) {
    if (pendingPRThread == currentPRThread) {
      NS_ERROR("Recursive GetService!");
      return NS_ERROR_NOT_AVAILABLE;
    }


    SafeMutexAutoUnlock unlockPending(mLock);

    if (!currentThread) {
      currentThread = NS_GetCurrentThread();
      MOZ_ASSERT(currentThread, "This should never be null!");
    }

    // This will process a single event or yield the thread if no event is
    // pending.
    if (!NS_ProcessNextEvent(currentThread, false)) {
      PR_Sleep(PR_INTERVAL_NO_WAIT);
    }
  }

  // It's still possible that the other thread failed to create the
  // service so we're not guaranteed to have an entry or service yet.
  if (entry->mServiceObject) {
    lock.Unlock();
    return entry->mServiceObject->QueryInterface(aIID, aResult);
  }

#ifdef DEBUG
  PendingServiceInfo* newInfo =
#endif
    AddPendingService(aClass, currentPRThread);
  NS_ASSERTION(newInfo, "Failed to add info to the array!");

  // We need to not be holding the service manager's lock while calling
  // CreateInstance, because it invokes user code which could try to re-enter
  // the service manager:

  nsresult rv;
  {
    SafeMutexAutoUnlock unlock(mLock);
    rv = CreateInstance(aClass, nullptr, aIID, getter_AddRefs(service));
  }
  if (NS_SUCCEEDED(rv) && !service) {
    NS_ERROR("Factory did not return an object but returned success");
    return NS_ERROR_SERVICE_NOT_FOUND;
  }

#ifdef DEBUG
  pendingPRThread = GetPendingServiceThread(aClass);
  MOZ_ASSERT(pendingPRThread == currentPRThread,
             "Pending service array has been changed!");
#endif
  RemovePendingService(aClass);

  if (NS_FAILED(rv)) {
    return rv;
  }

  NS_ASSERTION(!entry->mServiceObject, "Created two instances of a service!");

  entry->mServiceObject = service.forget();

  lock.Unlock();
  nsISupports** sresult = reinterpret_cast<nsISupports**>(aResult);
  *sresult = entry->mServiceObject;
  (*sresult)->AddRef();

  return NS_OK;
}

NS_IMETHODIMP
nsComponentManagerImpl::IsServiceInstantiated(const nsCID& aClass,
                                              const nsIID& aIID,
                                              bool* aResult)
{
  // Now we want to get the service if we already got it. If not, we don't want
  // to create an instance of it. mmh!

  // test this first, since there's no point in returning a service during
  // shutdown -- whether it's available or not would depend on the order it
  // occurs in the list
  if (gXPCOMShuttingDown) {
    // When processing shutdown, don't process new GetService() requests
#ifdef SHOW_DENIED_ON_SHUTDOWN
    char cid[NSID_LENGTH], iid[NSID_LENGTH];
    aClass.ToProvidedString(cid);
    aIID.ToProvidedString(iid);
    fprintf(stderr, "Checking for service on shutdown. Denied.\n"
            "         CID: %s\n         IID: %s\n", cid, iid);
#endif /* SHOW_DENIED_ON_SHUTDOWN */
    return NS_ERROR_UNEXPECTED;
  }

  nsresult rv = NS_OK;
  nsFactoryEntry* entry;

  {
    SafeMutexAutoLock lock(mLock);
    entry = mFactories.Get(&aClass);
  }

  if (entry && entry->mServiceObject) {
    nsCOMPtr<nsISupports> service;
    rv = entry->mServiceObject->QueryInterface(aIID, getter_AddRefs(service));
    *aResult = (service != nullptr);
  } else {
    *aResult = false;
  }

  return rv;
}

NS_IMETHODIMP
nsComponentManagerImpl::IsServiceInstantiatedByContractID(
    const char* aContractID,
    const nsIID& aIID,
    bool* aResult)
{
  // Now we want to get the service if we already got it. If not, we don't want
  // to create an instance of it. mmh!

  // test this first, since there's no point in returning a service during
  // shutdown -- whether it's available or not would depend on the order it
  // occurs in the list
  if (gXPCOMShuttingDown) {
    // When processing shutdown, don't process new GetService() requests
#ifdef SHOW_DENIED_ON_SHUTDOWN
    char iid[NSID_LENGTH];
    aIID.ToProvidedString(iid);
    fprintf(stderr, "Checking for service on shutdown. Denied.\n"
            "  ContractID: %s\n         IID: %s\n", aContractID, iid);
#endif /* SHOW_DENIED_ON_SHUTDOWN */
    return NS_ERROR_UNEXPECTED;
  }

  nsresult rv = NS_OK;
  nsFactoryEntry* entry;
  {
    SafeMutexAutoLock lock(mLock);
    entry = mContractIDs.Get(nsDependentCString(aContractID));
  }

  if (entry && entry->mServiceObject) {
    nsCOMPtr<nsISupports> service;
    rv = entry->mServiceObject->QueryInterface(aIID, getter_AddRefs(service));
    *aResult = (service != nullptr);
  } else {
    *aResult = false;
  }
  return rv;
}


NS_IMETHODIMP
nsComponentManagerImpl::GetServiceByContractID(const char* aContractID,
                                               const nsIID& aIID,
                                               void** aResult)
{
  // test this first, since there's no point in returning a service during
  // shutdown -- whether it's available or not would depend on the order it
  // occurs in the list
  if (gXPCOMShuttingDown) {
    // When processing shutdown, don't process new GetService() requests
#ifdef SHOW_DENIED_ON_SHUTDOWN
    char iid[NSID_LENGTH];
    aIID.ToProvidedString(iid);
    fprintf(stderr, "Getting service on shutdown. Denied.\n"
            "  ContractID: %s\n         IID: %s\n", aContractID, iid);
#endif /* SHOW_DENIED_ON_SHUTDOWN */
    return NS_ERROR_UNEXPECTED;
  }

  // `service` must be released after the lock is released, so it must be
  // declared before the lock in this C++ block.
  nsCOMPtr<nsISupports> service;
  MutexLock lock(mLock);

  nsFactoryEntry* entry = mContractIDs.Get(nsDependentCString(aContractID));
  if (!entry) {
    return NS_ERROR_FACTORY_NOT_REGISTERED;
  }

  if (entry->mServiceObject) {
    // We need to not be holding the service manager's monitor while calling
    // QueryInterface, because it invokes user code which could try to re-enter
    // the service manager, or try to grab some other lock/monitor/condvar
    // and deadlock, e.g. bug 282743.
    // `entry` is valid until XPCOM shutdown, so we can safely use it after
    // exiting the lock.
    lock.Unlock();
    return entry->mServiceObject->QueryInterface(aIID, aResult);
  }

  PRThread* currentPRThread = PR_GetCurrentThread();
  MOZ_ASSERT(currentPRThread, "This should never be null!");

  // Needed to optimize the event loop below.
  nsIThread* currentThread = nullptr;

  PRThread* pendingPRThread;
  while ((pendingPRThread = GetPendingServiceThread(*entry->mCIDEntry->cid))) {
    if (pendingPRThread == currentPRThread) {
      NS_ERROR("Recursive GetService!");
      return NS_ERROR_NOT_AVAILABLE;
    }

    SafeMutexAutoUnlock unlockPending(mLock);

    if (!currentThread) {
      currentThread = NS_GetCurrentThread();
      MOZ_ASSERT(currentThread, "This should never be null!");
    }

    // This will process a single event or yield the thread if no event is
    // pending.
    if (!NS_ProcessNextEvent(currentThread, false)) {
      PR_Sleep(PR_INTERVAL_NO_WAIT);
    }
  }

  if (currentThread && entry->mServiceObject) {
    // If we have a currentThread then we must have waited on another thread
    // to create the service. Grab it now if that succeeded.
    lock.Unlock();
    return entry->mServiceObject->QueryInterface(aIID, aResult);
  }

#ifdef DEBUG
  PendingServiceInfo* newInfo =
#endif
    AddPendingService(*entry->mCIDEntry->cid, currentPRThread);
  NS_ASSERTION(newInfo, "Failed to add info to the array!");

  // We need to not be holding the service manager's lock while calling
  // CreateInstance, because it invokes user code which could try to re-enter
  // the service manager:

  nsresult rv;
  {
    SafeMutexAutoUnlock unlock(mLock);
    rv = CreateInstanceByContractID(aContractID, nullptr, aIID,
                                    getter_AddRefs(service));
  }
  if (NS_SUCCEEDED(rv) && !service) {
    NS_ERROR("Factory did not return an object but returned success");
    return NS_ERROR_SERVICE_NOT_FOUND;
  }

#ifdef DEBUG
  pendingPRThread = GetPendingServiceThread(*entry->mCIDEntry->cid);
  MOZ_ASSERT(pendingPRThread == currentPRThread,
             "Pending service array has been changed!");
#endif
  RemovePendingService(*entry->mCIDEntry->cid);

  if (NS_FAILED(rv)) {
    return rv;
  }

  NS_ASSERTION(!entry->mServiceObject, "Created two instances of a service!");

  entry->mServiceObject = service.forget();

  lock.Unlock();

  nsISupports** sresult = reinterpret_cast<nsISupports**>(aResult);
  *sresult = entry->mServiceObject;
  (*sresult)->AddRef();

  return NS_OK;
}

already_AddRefed<mozilla::ModuleLoader>
nsComponentManagerImpl::LoaderForExtension(const nsACString& aExt)
{
  nsCOMPtr<mozilla::ModuleLoader> loader = mLoaderMap.Get(aExt);
  if (!loader) {
    loader = do_GetServiceFromCategory(NS_LITERAL_CSTRING("module-loader"),
                                       aExt);
    if (!loader) {
      return nullptr;
    }

    mLoaderMap.Put(aExt, loader);
  }

  return loader.forget();
}

NS_IMETHODIMP
nsComponentManagerImpl::RegisterFactory(const nsCID& aClass,
                                        const char* aName,
                                        const char* aContractID,
                                        nsIFactory* aFactory)
{
  if (!aFactory) {
    // If a null factory is passed in, this call just wants to reset
    // the contract ID to point to an existing CID entry.
    if (!aContractID) {
      return NS_ERROR_INVALID_ARG;
    }

    SafeMutexAutoLock lock(mLock);
    nsFactoryEntry* oldf = mFactories.Get(&aClass);
    if (!oldf) {
      return NS_ERROR_FACTORY_NOT_REGISTERED;
    }

    mContractIDs.Put(nsDependentCString(aContractID), oldf);
    return NS_OK;
  }

  nsAutoPtr<nsFactoryEntry> f(new nsFactoryEntry(aClass, aFactory));

  SafeMutexAutoLock lock(mLock);
  if (auto entry = mFactories.LookupForAdd(f->mCIDEntry->cid)) {
    return NS_ERROR_FACTORY_EXISTS;
  } else {
    if (aContractID) {
      mContractIDs.Put(nsDependentCString(aContractID), f);
    }
    entry.OrInsert([&f] () { return f.forget(); });
  }

  return NS_OK;
}

NS_IMETHODIMP
nsComponentManagerImpl::UnregisterFactory(const nsCID& aClass,
                                          nsIFactory* aFactory)
{
  // Don't release the dying factory or service object until releasing
  // the component manager monitor.
  nsCOMPtr<nsIFactory> dyingFactory;
  nsCOMPtr<nsISupports> dyingServiceObject;

  {
    SafeMutexAutoLock lock(mLock);
    auto entry = mFactories.Lookup(&aClass);
    nsFactoryEntry* f = entry ? entry.Data() : nullptr;
    if (!f || f->mFactory != aFactory) {
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
nsComponentManagerImpl::AutoRegister(nsIFile* aLocation)
{
  XRE_AddManifestLocation(NS_EXTENSION_LOCATION, aLocation);
  return NS_OK;
}

NS_IMETHODIMP
nsComponentManagerImpl::AutoUnregister(nsIFile* aLocation)
{
  NS_ERROR("AutoUnregister not implemented.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsComponentManagerImpl::RegisterFactoryLocation(const nsCID& aCID,
                                                const char* aClassName,
                                                const char* aContractID,
                                                nsIFile* aFile,
                                                const char* aLoaderStr,
                                                const char* aType)
{
  NS_ERROR("RegisterFactoryLocation not implemented.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsComponentManagerImpl::UnregisterFactoryLocation(const nsCID& aCID,
                                                  nsIFile* aFile)
{
  NS_ERROR("UnregisterFactoryLocation not implemented.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsComponentManagerImpl::IsCIDRegistered(const nsCID& aClass,
                                        bool* aResult)
{
  *aResult = (nullptr != GetFactoryEntry(aClass));
  return NS_OK;
}

NS_IMETHODIMP
nsComponentManagerImpl::IsContractIDRegistered(const char* aClass,
                                               bool* aResult)
{
  if (NS_WARN_IF(!aClass)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsFactoryEntry* entry = GetFactoryEntry(aClass, strlen(aClass));

  if (entry) {
    // UnregisterFactory might have left a stale nsFactoryEntry in
    // mContractIDs, so we should check to see whether this entry has
    // anything useful.
    *aResult = (bool(entry->mModule) ||
                bool(entry->mFactory) ||
                bool(entry->mServiceObject));
  } else {
    *aResult = false;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsComponentManagerImpl::EnumerateCIDs(nsISimpleEnumerator** aEnumerator)
{
  nsCOMArray<nsISupports> array;
  for (auto iter = mFactories.Iter(); !iter.Done(); iter.Next()) {
    const nsID* id = iter.Key();
    nsCOMPtr<nsISupportsID> wrapper = new nsSupportsID();
    wrapper->SetData(id);
    array.AppendObject(wrapper);
  }
  return NS_NewArrayEnumerator(aEnumerator, array);
}

NS_IMETHODIMP
nsComponentManagerImpl::EnumerateContractIDs(nsISimpleEnumerator** aEnumerator)
{
  auto* array = new nsTArray<nsCString>;
  for (auto iter = mContractIDs.Iter(); !iter.Done(); iter.Next()) {
    const nsACString& contract = iter.Key();
    array->AppendElement(contract);
  }

  nsCOMPtr<nsIUTF8StringEnumerator> e;
  nsresult rv = NS_NewAdoptingUTF8StringEnumerator(getter_AddRefs(e), array);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return CallQueryInterface(e, aEnumerator);
}

NS_IMETHODIMP
nsComponentManagerImpl::CIDToContractID(const nsCID& aClass,
                                        char** aResult)
{
  NS_ERROR("CIDTOContractID not implemented");
  return NS_ERROR_FACTORY_NOT_REGISTERED;
}

NS_IMETHODIMP
nsComponentManagerImpl::ContractIDToCID(const char* aContractID,
                                        nsCID** aResult)
{
  {
    SafeMutexAutoLock lock(mLock);
    nsFactoryEntry* entry = mContractIDs.Get(nsDependentCString(aContractID));
    if (entry) {
      *aResult = (nsCID*)moz_xmalloc(sizeof(nsCID));
      **aResult = *entry->mCIDEntry->cid;
      return NS_OK;
    }
  }
  *aResult = nullptr;
  return NS_ERROR_FACTORY_NOT_REGISTERED;
}

MOZ_DEFINE_MALLOC_SIZE_OF(ComponentManagerMallocSizeOf)

NS_IMETHODIMP
nsComponentManagerImpl::CollectReports(nsIHandleReportCallback* aHandleReport,
                                       nsISupports* aData, bool aAnonymize)
{
  MOZ_COLLECT_REPORT(
    "explicit/xpcom/component-manager", KIND_HEAP, UNITS_BYTES,
    SizeOfIncludingThis(ComponentManagerMallocSizeOf),
    "Memory used for the XPCOM component manager.");

  return NS_OK;
}

size_t
nsComponentManagerImpl::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
  const
{
  size_t n = aMallocSizeOf(this);

  n += mLoaderMap.ShallowSizeOfExcludingThis(aMallocSizeOf);

  n += mFactories.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = mFactories.ConstIter(); !iter.Done(); iter.Next()) {
    n += iter.Data()->SizeOfIncludingThis(aMallocSizeOf);
  }

  n += mContractIDs.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = mContractIDs.ConstIter(); !iter.Done(); iter.Next()) {
    // We don't measure the nsFactoryEntry data because it's owned by
    // mFactories (which is measured above).
    n += iter.Key().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }

  n += sExtraStaticModules->ShallowSizeOfIncludingThis(aMallocSizeOf);
  if (sModuleLocations) {
    n += sModuleLocations->ShallowSizeOfIncludingThis(aMallocSizeOf);
  }

  n += mKnownStaticModules.ShallowSizeOfExcludingThis(aMallocSizeOf);
  n += mKnownModules.ShallowSizeOfExcludingThis(aMallocSizeOf);

  n += mArena.SizeOfExcludingThis(aMallocSizeOf);

  n += mPendingServices.ShallowSizeOfExcludingThis(aMallocSizeOf);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mLoaderMap's keys and values
  // - mMon
  // - sModuleLocations' entries
  // - mKnownStaticModules' entries?
  // - mKnownModules' keys and values?

  return n;
}

////////////////////////////////////////////////////////////////////////////////
// nsFactoryEntry
////////////////////////////////////////////////////////////////////////////////

nsFactoryEntry::nsFactoryEntry(const mozilla::Module::CIDEntry* aEntry,
                               nsComponentManagerImpl::KnownModule* aModule)
  : mCIDEntry(aEntry)
  , mModule(aModule)
{
}

nsFactoryEntry::nsFactoryEntry(const nsCID& aCID, nsIFactory* aFactory)
  : mCIDEntry(nullptr)
  , mModule(nullptr)
  , mFactory(aFactory)
{
  auto* e = new mozilla::Module::CIDEntry();
  auto* cid = new nsCID;
  *cid = aCID;
  e->cid = cid;
  mCIDEntry = e;
}

nsFactoryEntry::~nsFactoryEntry()
{
  // If this was a RegisterFactory entry, we own the CIDEntry/CID
  if (!mModule) {
    delete mCIDEntry->cid;
    delete mCIDEntry;
  }
}

already_AddRefed<nsIFactory>
nsFactoryEntry::GetFactory()
{
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
      factory = mModule->Module()->getFactoryProc(*mModule->Module(),
                                                  *mCIDEntry);
    } else if (mCIDEntry->getFactoryProc) {
      factory = mCIDEntry->getFactoryProc(*mModule->Module(), *mCIDEntry);
    } else {
      NS_ASSERTION(mCIDEntry->constructorProc, "no getfactory or constructor");
      factory = new mozilla::GenericFactory(mCIDEntry->constructorProc);
    }
    if (!factory) {
      return nullptr;
    }

    SafeMutexAutoLock lock(nsComponentManagerImpl::gComponentManager->mLock);
    // Threads can race to set mFactory
    if (!mFactory) {
      factory.swap(mFactory);
    }
  }
  nsCOMPtr<nsIFactory> factory = mFactory;
  return factory.forget();
}

size_t
nsFactoryEntry::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf)
{
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

nsresult
NS_GetComponentManager(nsIComponentManager** aResult)
{
  if (!nsComponentManagerImpl::gComponentManager) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  NS_ADDREF(*aResult = nsComponentManagerImpl::gComponentManager);
  return NS_OK;
}

nsresult
NS_GetServiceManager(nsIServiceManager** aResult)
{
  if (!nsComponentManagerImpl::gComponentManager) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  NS_ADDREF(*aResult = nsComponentManagerImpl::gComponentManager);
  return NS_OK;
}


nsresult
NS_GetComponentRegistrar(nsIComponentRegistrar** aResult)
{
  if (!nsComponentManagerImpl::gComponentManager) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  NS_ADDREF(*aResult = nsComponentManagerImpl::gComponentManager);
  return NS_OK;
}

EXPORT_XPCOM_API(nsresult)
XRE_AddStaticComponent(const mozilla::Module* aComponent)
{
  nsComponentManagerImpl::InitializeStaticModules();
  sExtraStaticModules->AppendElement(aComponent);

  if (nsComponentManagerImpl::gComponentManager &&
      nsComponentManagerImpl::NORMAL ==
        nsComponentManagerImpl::gComponentManager->mStatus) {
    nsComponentManagerImpl::gComponentManager->RegisterModule(aComponent,
                                                              nullptr);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsComponentManagerImpl::AddBootstrappedManifestLocation(nsIFile* aLocation)
{
  nsString path;
  nsresult rv = aLocation->GetPath(path);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (Substring(path, path.Length() - 4).EqualsLiteral(".xpi")) {
    return XRE_AddJarManifestLocation(NS_BOOTSTRAPPED_LOCATION, aLocation);
  }

  nsCOMPtr<nsIFile> manifest =
    CloneAndAppend(aLocation, NS_LITERAL_CSTRING("chrome.manifest"));
  return XRE_AddManifestLocation(NS_BOOTSTRAPPED_LOCATION, manifest);
}

NS_IMETHODIMP
nsComponentManagerImpl::RemoveBootstrappedManifestLocation(nsIFile* aLocation)
{
  nsCOMPtr<nsIChromeRegistry> cr = mozilla::services::GetChromeRegistryService();
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
    nsCOMPtr<nsIFile> lf =
      CloneAndAppend(aLocation, NS_LITERAL_CSTRING("chrome.manifest"));
    elem.location.Init(lf);
  }

  // Remove reference.
  nsComponentManagerImpl::sModuleLocations->RemoveElement(elem,
                                                          ComponentLocationComparator());

  rv = cr->CheckForNewChrome();
  return rv;
}

NS_IMETHODIMP
nsComponentManagerImpl::GetManifestLocations(nsIArray** aLocations)
{
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
XRE_AddManifestLocation(NSLocationType aType, nsIFile* aLocation)
{
  nsComponentManagerImpl::InitializeModuleLocations();
  nsComponentManagerImpl::ComponentLocation* c =
    nsComponentManagerImpl::sModuleLocations->AppendElement();
  c->type = aType;
  c->location.Init(aLocation);

  if (nsComponentManagerImpl::gComponentManager &&
      nsComponentManagerImpl::NORMAL ==
        nsComponentManagerImpl::gComponentManager->mStatus) {
    nsComponentManagerImpl::gComponentManager->RegisterManifest(aType,
                                                                c->location,
                                                                false);
  }

  return NS_OK;
}

EXPORT_XPCOM_API(nsresult)
XRE_AddJarManifestLocation(NSLocationType aType, nsIFile* aLocation)
{
  nsComponentManagerImpl::InitializeModuleLocations();
  nsComponentManagerImpl::ComponentLocation* c =
    nsComponentManagerImpl::sModuleLocations->AppendElement();

  c->type = aType;
  c->location.Init(aLocation, "chrome.manifest");

  if (nsComponentManagerImpl::gComponentManager &&
      nsComponentManagerImpl::NORMAL ==
        nsComponentManagerImpl::gComponentManager->mStatus) {
    nsComponentManagerImpl::gComponentManager->RegisterManifest(aType,
                                                                c->location,
                                                                false);
  }

  return NS_OK;
}

// Expose some important global interfaces to rust for the rust xpcom API. These
// methods return a non-owning reference to the component manager, which should
// live for the lifetime of XPCOM.
extern "C" {

const nsIComponentManager*
Gecko_GetComponentManager()
{
  return nsComponentManagerImpl::gComponentManager;
}

const nsIServiceManager*
Gecko_GetServiceManager()
{
  return nsComponentManagerImpl::gComponentManager;
}

const nsIComponentRegistrar*
Gecko_GetComponentRegistrar()
{
  return nsComponentManagerImpl::gComponentManager;
}

}
