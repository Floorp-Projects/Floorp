/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsComponentManager_h__
#define nsComponentManager_h__

#include "nsXPCOM.h"

#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIMemoryReporter.h"
#include "nsIServiceManager.h"
#include "nsIFile.h"
#include "mozilla/ArenaAllocator.h"
#include "mozilla/Atomics.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Module.h"
#include "mozilla/Monitor.h"
#include "mozilla/UniquePtr.h"
#include "nsXULAppAPI.h"
#include "nsIFactory.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "PLDHashTable.h"
#include "prtime.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "nsCOMArray.h"
#include "nsTHashMap.h"
#include "nsInterfaceHashtable.h"
#include "nsClassHashtable.h"
#include "nsTArray.h"

#include "mozilla/Components.h"
#include "mozilla/Maybe.h"
#include "mozilla/Omnijar.h"
#include "mozilla/Attributes.h"

struct nsFactoryEntry;
struct PRThread;

#define NS_COMPONENTMANAGER_CID                      \
  { /* 91775d60-d5dc-11d2-92fb-00e09805570f */       \
    0x91775d60, 0xd5dc, 0x11d2, {                    \
      0x92, 0xfb, 0x00, 0xe0, 0x98, 0x05, 0x57, 0x0f \
    }                                                \
  }

////////////////////////////////////////////////////////////////////////////////

namespace {
class EntryWrapper;
}  // namespace

namespace mozilla {
namespace xpcom {

bool ProcessSelectorMatches(Module::ProcessSelector aSelector);
bool FastProcessSelectorMatches(Module::ProcessSelector aSelector);

}  // namespace xpcom
}  // namespace mozilla

class nsComponentManagerImpl final : public nsIComponentManager,
                                     public nsIServiceManager,
                                     public nsSupportsWeakReference,
                                     public nsIComponentRegistrar,
                                     public nsIInterfaceRequestor,
                                     public nsIMemoryReporter {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICOMPONENTMANAGER
  NS_DECL_NSICOMPONENTREGISTRAR
  NS_DECL_NSIMEMORYREPORTER

  static nsresult Create(REFNSIID aIID, void** aResult);

  nsresult RegistryLocationForFile(nsIFile* aFile, nsCString& aResult);
  nsresult FileForRegistryLocation(const nsCString& aLocation, nsIFile** aSpec);

  NS_DECL_NSISERVICEMANAGER

  // nsComponentManagerImpl methods:
  nsComponentManagerImpl();

  static nsComponentManagerImpl* gComponentManager;
  nsresult Init();

  nsresult Shutdown(void);

  nsresult FreeServices();

  already_AddRefed<nsIFactory> FindFactory(const nsCID& aClass);
  already_AddRefed<nsIFactory> FindFactory(const char* aContractID,
                                           uint32_t aContractIDLen);

  already_AddRefed<nsIFactory> LoadFactory(nsFactoryEntry* aEntry);

  nsTHashMap<nsIDPointerHashKey, nsFactoryEntry*> mFactories;
  nsTHashMap<nsCStringHashKey, nsFactoryEntry*> mContractIDs;

  mozilla::Monitor mLock MOZ_UNANNOTATED;

  mozilla::Maybe<EntryWrapper> LookupByCID(const nsID& aCID);
  mozilla::Maybe<EntryWrapper> LookupByCID(const mozilla::MonitorAutoLock&,
                                           const nsID& aCID);

  mozilla::Maybe<EntryWrapper> LookupByContractID(
      const nsACString& aContractID);
  mozilla::Maybe<EntryWrapper> LookupByContractID(
      const mozilla::MonitorAutoLock&, const nsACString& aContractID);

  nsresult GetService(mozilla::xpcom::ModuleID, const nsIID& aIID,
                      void** aResult);

  static bool JSLoaderReady() { return gComponentManager->mJSLoaderReady; }

  static void InitializeStaticModules();
  static void InitializeModuleLocations();

  struct ComponentLocation {
    NSLocationType type;
    mozilla::FileLocation location;
  };

  class ComponentLocationComparator {
   public:
    bool Equals(const ComponentLocation& aA,
                const ComponentLocation& aB) const {
      return (aA.type == aB.type && aA.location.Equals(aB.location));
    }
  };

  static nsTArray<ComponentLocation>* sModuleLocations;

  // Mutex not held
  void RegisterManifest(NSLocationType aType, mozilla::FileLocation& aFile,
                        bool aChromeOnly);

  struct ManifestProcessingContext {
    ManifestProcessingContext(NSLocationType aType,
                              mozilla::FileLocation& aFile, bool aChromeOnly)
        : mType(aType), mFile(aFile), mChromeOnly(aChromeOnly) {}

    ~ManifestProcessingContext() = default;

    NSLocationType mType;
    mozilla::FileLocation mFile;
    bool mChromeOnly;
  };

  void ManifestManifest(ManifestProcessingContext& aCx, int aLineNo,
                        char* const* aArgv);
  void ManifestCategory(ManifestProcessingContext& aCx, int aLineNo,
                        char* const* aArgv);

  void RereadChromeManifests(bool aChromeOnly = true);

  // Shutdown
  enum {
    NOT_INITIALIZED,
    NORMAL,
    SHUTDOWN_IN_PROGRESS,
    SHUTDOWN_COMPLETE
  } mStatus;

  struct PendingServiceInfo {
    const nsCID* cid;
    PRThread* thread;
  };

  inline PendingServiceInfo* AddPendingService(const nsCID& aServiceCID,
                                               PRThread* aThread);
  inline void RemovePendingService(mozilla::MonitorAutoLock& aLock,
                                   const nsCID& aServiceCID);
  inline PRThread* GetPendingServiceThread(const nsCID& aServiceCID) const;

  nsTArray<PendingServiceInfo> mPendingServices;

  bool mJSLoaderReady = false;

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

 private:
  ~nsComponentManagerImpl();

  nsresult GetServiceLocked(mozilla::Maybe<mozilla::MonitorAutoLock>& aLock,
                            EntryWrapper& aEntry, const nsIID& aIID,
                            void** aResult);
};

#define NS_MAX_FILENAME_LEN 1024

#define NS_ERROR_IS_DIR NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_XPCOM, 24)

struct nsFactoryEntry {
  // nsIComponentRegistrar.registerFactory support
  nsFactoryEntry(const nsCID& aClass, nsIFactory* aFactory);

  ~nsFactoryEntry() = default;

  already_AddRefed<nsIFactory> GetFactory();

  nsresult CreateInstance(const nsIID& aIID, void** aResult);

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

  const nsCID mCID;

  nsCOMPtr<nsIFactory> mFactory;
  nsCOMPtr<nsISupports> mServiceObject;
};

#endif  // nsComponentManager_h__
