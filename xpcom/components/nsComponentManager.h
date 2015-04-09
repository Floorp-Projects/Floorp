/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsComponentManager_h__
#define nsComponentManager_h__

#include "nsXPCOM.h"

#include "xpcom-private.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIMemoryReporter.h"
#include "nsIServiceManager.h"
#include "nsIFile.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Module.h"
#include "mozilla/ModuleLoader.h"
#include "mozilla/Mutex.h"
#include "nsXULAppAPI.h"
#include "nsNativeModuleLoader.h"
#include "nsIFactory.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "pldhash.h"
#include "prtime.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsWeakReference.h"
#include "plarena.h"
#include "nsCOMArray.h"
#include "nsDataHashtable.h"
#include "nsInterfaceHashtable.h"
#include "nsClassHashtable.h"
#include "nsTArray.h"

#include "mozilla/Omnijar.h"
#include "mozilla/Attributes.h"

#ifdef MOZ_B2G_LOADER
#include "mozilla/FileLocation.h"
#endif

struct nsFactoryEntry;
class nsIServiceManager;
struct PRThread;

#define NS_COMPONENTMANAGER_CID                      \
{ /* 91775d60-d5dc-11d2-92fb-00e09805570f */         \
    0x91775d60,                                      \
    0xd5dc,                                          \
    0x11d2,                                          \
    {0x92, 0xfb, 0x00, 0xe0, 0x98, 0x05, 0x57, 0x0f} \
}

/* keys for registry use */
extern const char xpcomKeyName[];
extern const char xpcomComponentsKeyName[];
extern const char lastModValueName[];
extern const char fileSizeValueName[];
extern const char nativeComponentType[];
extern const char staticComponentType[];

#ifdef DEBUG
#define XPCOM_CHECK_PENDING_CIDS
#endif
////////////////////////////////////////////////////////////////////////////////

#if defined(MOZILLA_XPCOMRT_API)
extern const mozilla::Module kXPCOMRTModule;
extern const mozilla::Module kNeckoStandaloneModule;
extern const mozilla::Module kStunUDPSocketFilterHandlerModule;
#else
extern const mozilla::Module kXPCOMModule;
#endif

/**
 * This is a wrapper around mozilla::Mutex which provides runtime
 * checking for a deadlock where the same thread tries to lock a mutex while
 * it is already locked. This checking is present in both debug and release
 * builds.
 */
class SafeMutex
{
public:
  explicit SafeMutex(const char* aName)
    : mMutex(aName)
    , mOwnerThread(nullptr)
  {
  }

  ~SafeMutex() {}

  void Lock()
  {
    AssertNotCurrentThreadOwns();
    mMutex.Lock();
    MOZ_ASSERT(mOwnerThread == nullptr);
    mOwnerThread = PR_GetCurrentThread();
  }

  void Unlock()
  {
    MOZ_ASSERT(mOwnerThread == PR_GetCurrentThread());
    mOwnerThread = nullptr;
    mMutex.Unlock();
  }

  void AssertCurrentThreadOwns() const
  {
    // This method is a debug-only check
    MOZ_ASSERT(mOwnerThread == PR_GetCurrentThread());
  }

  MOZ_NEVER_INLINE void AssertNotCurrentThreadOwns() const
  {
    // This method is a release-mode check
    if (PR_GetCurrentThread() == mOwnerThread) {
      MOZ_CRASH();
    }
  }

private:
  mozilla::Mutex mMutex;
  volatile PRThread* mOwnerThread;
};

typedef mozilla::BaseAutoLock<SafeMutex> SafeMutexAutoLock;
typedef mozilla::BaseAutoUnlock<SafeMutex> SafeMutexAutoUnlock;

class nsComponentManagerImpl final
  : public nsIComponentManager
  , public nsIServiceManager
  , public nsSupportsWeakReference
  , public nsIComponentRegistrar
  , public nsIInterfaceRequestor
  , public nsIMemoryReporter
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICOMPONENTMANAGER
  NS_DECL_NSICOMPONENTREGISTRAR
  NS_DECL_NSIMEMORYREPORTER

  static nsresult Create(nsISupports* aOuter, REFNSIID aIID, void** aResult);

  nsresult RegistryLocationForFile(nsIFile* aFile,
                                   nsCString& aResult);
  nsresult FileForRegistryLocation(const nsCString& aLocation,
                                   nsIFile** aSpec);

  NS_DECL_NSISERVICEMANAGER

  // nsComponentManagerImpl methods:
  nsComponentManagerImpl();

  static nsComponentManagerImpl* gComponentManager;
  nsresult Init();

  nsresult Shutdown(void);

  nsresult FreeServices();

  already_AddRefed<mozilla::ModuleLoader> LoaderForExtension(const nsACString& aExt);
  nsInterfaceHashtable<nsCStringHashKey, mozilla::ModuleLoader> mLoaderMap;

  already_AddRefed<nsIFactory> FindFactory(const nsCID& aClass);
  already_AddRefed<nsIFactory> FindFactory(const char* aContractID,
                                           uint32_t aContractIDLen);

  already_AddRefed<nsIFactory> LoadFactory(nsFactoryEntry* aEntry);

  nsFactoryEntry* GetFactoryEntry(const char* aContractID,
                                  uint32_t aContractIDLen);
  nsFactoryEntry* GetFactoryEntry(const nsCID& aClass);

  nsDataHashtable<nsIDHashKey, nsFactoryEntry*> mFactories;
  nsDataHashtable<nsCStringHashKey, nsFactoryEntry*> mContractIDs;

  SafeMutex mLock;

  static void InitializeStaticModules();
  static void InitializeModuleLocations();

  struct ComponentLocation
  {
    NSLocationType type;
    mozilla::FileLocation location;
  };

  class ComponentLocationComparator
  {
  public:
    bool Equals(const ComponentLocation& aA, const ComponentLocation& aB) const
    {
      return (aA.type == aB.type && aA.location.Equals(aB.location));
    }
  };

  static nsTArray<const mozilla::Module*>* sStaticModules;
  static nsTArray<ComponentLocation>* sModuleLocations;

  nsNativeModuleLoader mNativeModuleLoader;

  class KnownModule
  {
  public:
    /**
     * Static or binary module.
     */
    KnownModule(const mozilla::Module* aModule, mozilla::FileLocation& aFile)
      : mModule(aModule)
      , mFile(aFile)
      , mLoaded(false)
      , mFailed(false)
    {
    }

    explicit KnownModule(const mozilla::Module* aModule)
      : mModule(aModule)
      , mLoaded(false)
      , mFailed(false)
    {
    }

    explicit KnownModule(mozilla::FileLocation& aFile)
      : mModule(nullptr)
      , mFile(aFile)
      , mLoader(nullptr)
      , mLoaded(false)
      , mFailed(false)
    {
    }

    ~KnownModule()
    {
      if (mLoaded && mModule->unloadProc) {
        mModule->unloadProc();
      }
    }

    bool EnsureLoader();
    bool Load();

    const mozilla::Module* Module() const { return mModule; }

    /**
     * For error logging, get a description of this module, either the
     * file path, or <static module>.
     */
    nsCString Description() const;

  private:
    const mozilla::Module* mModule;
    mozilla::FileLocation mFile;
    nsCOMPtr<mozilla::ModuleLoader> mLoader;
    bool mLoaded;
    bool mFailed;
  };

  // The KnownModule is kept alive by these members, it is
  // referenced by pointer from the factory entries.
  nsTArray<nsAutoPtr<KnownModule>> mKnownStaticModules;
  // The key is the URI string of the module
  nsClassHashtable<nsCStringHashKey, KnownModule> mKnownModules;

  // Mutex not held
  void RegisterModule(const mozilla::Module* aModule,
                      mozilla::FileLocation* aFile);


  // Mutex held
  void RegisterCIDEntryLocked(const mozilla::Module::CIDEntry* aEntry,
                              KnownModule* aModule);
  void RegisterContractIDLocked(const mozilla::Module::ContractIDEntry* aEntry);

  // Mutex not held
  void RegisterManifest(NSLocationType aType, mozilla::FileLocation& aFile,
                        bool aChromeOnly);

  struct ManifestProcessingContext
  {
    ManifestProcessingContext(NSLocationType aType,
                              mozilla::FileLocation& aFile, bool aChromeOnly)
      : mType(aType)
      , mFile(aFile)
      , mChromeOnly(aChromeOnly)
    {
    }

    ~ManifestProcessingContext() {}

    NSLocationType mType;
    mozilla::FileLocation mFile;
    bool mChromeOnly;
  };

  void ManifestManifest(ManifestProcessingContext& aCx, int aLineNo,
                        char* const* aArgv);
  void ManifestBinaryComponent(ManifestProcessingContext& aCx, int aLineNo,
                               char* const* aArgv);
  void ManifestXPT(ManifestProcessingContext& aCx, int aLineNo,
                   char* const* aArgv);
  void ManifestComponent(ManifestProcessingContext& aCx, int aLineNo,
                         char* const* aArgv);
  void ManifestContract(ManifestProcessingContext& aCx, int aLineNo,
                        char* const* aArgv);
  void ManifestCategory(ManifestProcessingContext& aCx, int aLineNo,
                        char* const* aArgv);

  void RereadChromeManifests(bool aChromeOnly = true);

  // Shutdown
  enum
  {
    NOT_INITIALIZED,
    NORMAL,
    SHUTDOWN_IN_PROGRESS,
    SHUTDOWN_COMPLETE
  } mStatus;

  PLArenaPool   mArena;

  struct PendingServiceInfo
  {
    const nsCID* cid;
    PRThread* thread;
  };

  inline PendingServiceInfo* AddPendingService(const nsCID& aServiceCID,
                                               PRThread* aThread);
  inline void RemovePendingService(const nsCID& aServiceCID);
  inline PRThread* GetPendingServiceThread(const nsCID& aServiceCID) const;

  nsTArray<PendingServiceInfo> mPendingServices;

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

#ifdef MOZ_B2G_LOADER
  // Preload XPT interface info for B2G loader.
  // This function is called before XPCOM has been initialized.
  static void PreloadXPT(nsIFile* aFile);
#endif

#ifdef MOZ_B2G_LOADER
  // Parsing functions of directives of manifest for XPT only parsing.
  struct XPTOnlyManifestProcessingContext
  {
    XPTOnlyManifestProcessingContext(mozilla::FileLocation& aFile)
      : mFile(aFile)
    {
    }

    ~XPTOnlyManifestProcessingContext() {}

    mozilla::FileLocation mFile;
  };
  static void XPTOnlyManifestManifest(XPTOnlyManifestProcessingContext& aCx,
                                      int aLineNo, char* const* aArgv);
  static void XPTOnlyManifestXPT(XPTOnlyManifestProcessingContext& aCx,
                                 int aLineNo, char* const* aArgv);
#endif

private:
  ~nsComponentManagerImpl();
};


#define NS_MAX_FILENAME_LEN     1024

#define NS_ERROR_IS_DIR NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XPCOM, 24)

struct nsFactoryEntry
{
  nsFactoryEntry(const mozilla::Module::CIDEntry* aEntry,
                 nsComponentManagerImpl::KnownModule* aModule);

  // nsIComponentRegistrar.registerFactory support
  nsFactoryEntry(const nsCID& aClass, nsIFactory* aFactory);

  ~nsFactoryEntry();

  already_AddRefed<nsIFactory> GetFactory();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

  const mozilla::Module::CIDEntry* mCIDEntry;
  nsComponentManagerImpl::KnownModule* mModule;

  nsCOMPtr<nsIFactory>   mFactory;
  nsCOMPtr<nsISupports>  mServiceObject;
};

#endif // nsComponentManager_h__
