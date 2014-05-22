/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      Added PR_CALLBACK for Optlink use in OS2
 */

#include <stdlib.h>
#include "nscore.h"
#include "nsISupports.h"
#include "nspr.h"
#include "nsCRT.h" // for atoll

// Arena used by component manager for storing contractid string, dll
// location strings and small objects
// CAUTION: Arena align mask needs to be defined before including plarena.h
//          currently from nsComponentManager.h
#define PL_ARENA_CONST_ALIGN_MASK 7
#define NS_CM_BLOCK_SIZE (1024 * 8)

#include "nsCategoryManager.h"
#include "nsCOMPtr.h"
#include "nsComponentManager.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsCategoryManager.h"
#include "nsCategoryManagerUtils.h"
#include "xptiprivate.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/XPTInterfaceInfoManager.h"
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
#include "nsXPIDLString.h"
#include "prcmon.h"
#include "xptinfo.h" // this after nsISupports, to pick up IID so that xpt stuff doesn't try to define it itself...
#include "nsThreadUtils.h"
#include "prthread.h"
#include "private/pprthred.h"
#include "nsTArray.h"
#include "prio.h"
#include "ManifestParser.h"
#include "mozilla/Services.h"

#include "nsManifestLineReader.h"
#include "mozilla/GenericFactory.h"
#include "nsSupportsPrimitives.h"
#include "nsArray.h"
#include "nsIMutableArray.h"
#include "nsArrayEnumerator.h"
#include "nsStringEnumerator.h"
#include "mozilla/FileUtils.h"
#include "nsNetUtil.h"

#include <new>     // for placement new

#include "mozilla/Omnijar.h"

#include "prlog.h"

using namespace mozilla;

PRLogModuleInfo* nsComponentManagerLog = nullptr;

// defined in nsStaticXULComponents.cpp to contain all the components in
// libxul.
extern mozilla::Module const *const *const kPStaticModules[];

#if 0 || defined (DEBUG_timeless)
 #define SHOW_DENIED_ON_SHUTDOWN
 #define SHOW_CI_ON_EXISTING_SERVICE
#endif

// Bloated registry buffer size to improve startup performance -- needs to
// be big enough to fit the entire file into memory or it'll thrash.
// 512K is big enough to allow for some future growth in the registry.
#define BIG_REGISTRY_BUFLEN   (512*1024)

// Common Key Names
const char xpcomComponentsKeyName[]="software/mozilla/XPCOM/components";
const char xpcomKeyName[]="software/mozilla/XPCOM";

// Common Value Names
const char fileSizeValueName[]="FileSize";
const char lastModValueName[]="LastModTimeStamp";
const char nativeComponentType[]="application/x-mozilla-native";
const char staticComponentType[]="application/x-mozilla-static";

NS_DEFINE_CID(kCategoryManagerCID, NS_CATEGORYMANAGER_CID);

#define UID_STRING_LENGTH 39

nsresult
nsGetServiceFromCategory::operator()(const nsIID& aIID, void** aInstancePtr) const
{
    nsresult rv;
    nsXPIDLCString value;
    nsCOMPtr<nsICategoryManager> catman;
    nsComponentManagerImpl *compMgr = nsComponentManagerImpl::gComponentManager;
    if (!compMgr) {
        rv = NS_ERROR_NOT_INITIALIZED;
        goto error;
    }

    if (!mCategory || !mEntry) {
        // when categories have defaults, use that for null mEntry
        rv = NS_ERROR_NULL_POINTER;
        goto error;
    }

    rv = compMgr->nsComponentManagerImpl::GetService(kCategoryManagerCID,
                                                     NS_GET_IID(nsICategoryManager),
                                                     getter_AddRefs(catman));
    if (NS_FAILED(rv)) goto error;

    /* find the contractID for category.entry */
    rv = catman->GetCategoryEntry(mCategory, mEntry,
                                  getter_Copies(value));
    if (NS_FAILED(rv)) goto error;
    if (!value) {
        rv = NS_ERROR_SERVICE_NOT_AVAILABLE;
        goto error;
    }

    rv = compMgr->
        nsComponentManagerImpl::GetServiceByContractID(value,
                                                       aIID, aInstancePtr);
    if (NS_FAILED(rv)) {
    error:
        *aInstancePtr = 0;
    }
    if (mErrorPtr)
        *mErrorPtr = rv;
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// Arena helper functions
////////////////////////////////////////////////////////////////////////////////
char *
ArenaStrndup(const char *s, uint32_t len, PLArenaPool *arena)
{
    void *mem;
    // Include trailing null in the len
    PL_ARENA_ALLOCATE(mem, arena, len+1);
    if (mem)
        memcpy(mem, s, len+1);
    return static_cast<char *>(mem);
}

char*
ArenaStrdup(const char *s, PLArenaPool *arena)
{
    return ArenaStrndup(s, strlen(s), arena);
}

// GetService and a few other functions need to exit their mutex mid-function
// without reentering it later in the block. This class supports that
// style of early-exit that MutexAutoUnlock doesn't.

namespace {

class MOZ_STACK_CLASS MutexLock
{
public:
    MutexLock(SafeMutex& aMutex)
        : mMutex(aMutex)
        , mLocked(false)
    {
        Lock();
    }

    ~MutexLock()
    {
        if (mLocked)
            Unlock();
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

} // anonymous namespace

// this is safe to call during InitXPCOM
static already_AddRefed<nsIFile>
GetLocationFromDirectoryService(const char* prop)
{
    nsCOMPtr<nsIProperties> directoryService;
    nsDirectoryService::Create(nullptr,
                               NS_GET_IID(nsIProperties),
                               getter_AddRefs(directoryService));

    if (!directoryService)
        return nullptr;

    nsCOMPtr<nsIFile> file;
    nsresult rv = directoryService->Get(prop,
                                        NS_GET_IID(nsIFile),
                                        getter_AddRefs(file));
    if (NS_FAILED(rv))
        return nullptr;

    return file.forget();
}

static already_AddRefed<nsIFile>
CloneAndAppend(nsIFile* aBase, const nsACString& append)
{
    nsCOMPtr<nsIFile> f;
    aBase->Clone(getter_AddRefs(f));
    if (!f)
        return nullptr;

    f->AppendNative(append);
    return f.forget();
}

////////////////////////////////////////////////////////////////////////////////
// nsComponentManagerImpl
////////////////////////////////////////////////////////////////////////////////

nsresult
nsComponentManagerImpl::Create(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    if (!gComponentManager)
        return NS_ERROR_FAILURE;

    return gComponentManager->QueryInterface(aIID, aResult);
}

static const int CONTRACTID_HASHTABLE_INITIAL_SIZE = 2048;

nsComponentManagerImpl::nsComponentManagerImpl()
    : mFactories(CONTRACTID_HASHTABLE_INITIAL_SIZE)
    , mContractIDs(CONTRACTID_HASHTABLE_INITIAL_SIZE)
    , mLock("nsComponentManagerImpl.mLock")
    , mStatus(NOT_INITIALIZED)
{
}

nsTArray<const mozilla::Module*>* nsComponentManagerImpl::sStaticModules;

/* static */ void
nsComponentManagerImpl::InitializeStaticModules()
{
    if (sStaticModules)
        return;

    sStaticModules = new nsTArray<const mozilla::Module*>;
    for (const mozilla::Module *const *const *staticModules = kPStaticModules;
         *staticModules; ++staticModules)
        sStaticModules->AppendElement(**staticModules);
}

nsTArray<nsComponentManagerImpl::ComponentLocation>*
nsComponentManagerImpl::sModuleLocations;

/* static */ void
nsComponentManagerImpl::InitializeModuleLocations()
{
    if (sModuleLocations)
        return;

    sModuleLocations = new nsTArray<ComponentLocation>;
}

nsresult nsComponentManagerImpl::Init()
{
    PR_ASSERT(NOT_INITIALIZED == mStatus);

    if (nsComponentManagerLog == nullptr)
    {
        nsComponentManagerLog = PR_NewLogModule("nsComponentManager");
    }

    // Initialize our arena
    PL_INIT_ARENA_POOL(&mArena, "ComponentManagerArena", NS_CM_BLOCK_SIZE);

    nsCOMPtr<nsIFile> greDir =
        GetLocationFromDirectoryService(NS_GRE_DIR);
    nsCOMPtr<nsIFile> appDir =
        GetLocationFromDirectoryService(NS_XPCOM_CURRENT_PROCESS_DIR);

    InitializeStaticModules();
    InitializeModuleLocations();

    ComponentLocation* cl = sModuleLocations->InsertElementAt(0);
    nsCOMPtr<nsIFile> lf = CloneAndAppend(appDir, NS_LITERAL_CSTRING("chrome.manifest"));
    cl->type = NS_COMPONENT_LOCATION;
    cl->location.Init(lf);

    bool equals = false;
    appDir->Equals(greDir, &equals);
    if (!equals) {
        cl = sModuleLocations->InsertElementAt(0);
        cl->type = NS_COMPONENT_LOCATION;
        lf = CloneAndAppend(greDir, NS_LITERAL_CSTRING("chrome.manifest"));
        cl->location.Init(lf);
    }

    PR_LOG(nsComponentManagerLog, PR_LOG_DEBUG,
           ("nsComponentManager: Initialized."));

    nsresult rv = mNativeModuleLoader.Init();
    if (NS_FAILED(rv))
        return rv;

    nsCategoryManager::GetSingleton()->SuppressNotifications(true);

    RegisterModule(&kXPCOMModule, nullptr);

    for (uint32_t i = 0; i < sStaticModules->Length(); ++i)
        RegisterModule((*sStaticModules)[i], nullptr);

    nsRefPtr<nsZipArchive> appOmnijar = mozilla::Omnijar::GetReader(mozilla::Omnijar::APP);
    if (appOmnijar) {
        cl = sModuleLocations->InsertElementAt(1); // Insert after greDir
        cl->type = NS_COMPONENT_LOCATION;
        cl->location.Init(appOmnijar, "chrome.manifest");
    }
    nsRefPtr<nsZipArchive> greOmnijar = mozilla::Omnijar::GetReader(mozilla::Omnijar::GRE);
    if (greOmnijar) {
        cl = sModuleLocations->InsertElementAt(0);
        cl->type = NS_COMPONENT_LOCATION;
        cl->location.Init(greOmnijar, "chrome.manifest");
    }

    RereadChromeManifests(false);

    nsCategoryManager::GetSingleton()->SuppressNotifications(false);

    RegisterWeakMemoryReporter(this);

    // Unfortunately, we can't register the nsCategoryManager memory reporter
    // in its constructor (which is triggered by the GetSingleton() call
    // above) because the memory reporter manager isn't initialized at that
    // point.  So we wait until now.
    nsCategoryManager::GetSingleton()->InitMemoryReporter();

    mStatus = NORMAL;

    return NS_OK;
}

void
nsComponentManagerImpl::RegisterModule(const mozilla::Module* aModule,
                                       FileLocation* aFile)
{
    mLock.AssertNotCurrentThreadOwns();

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
            for (entry = aModule->mCIDs; entry->cid; ++entry)
                RegisterCIDEntryLocked(entry, m);
        }

        if (aModule->mContractIDs) {
            const mozilla::Module::ContractIDEntry* entry;
            for (entry = aModule->mContractIDs; entry->contractid; ++entry)
                RegisterContractIDLocked(entry);
            MOZ_ASSERT(!entry->cid, "Incorrectly terminated contract list");
        }
    }

    if (aModule->mCategoryEntries) {
        const mozilla::Module::CategoryEntry* entry;
        for (entry = aModule->mCategoryEntries; entry->category; ++entry)
            nsCategoryManager::GetSingleton()->
                AddCategoryEntry(entry->category,
                                 entry->entry,
                                 entry->value);
    }
}

static bool
ProcessSelectorMatches(Module::ProcessSelector selector)
{
    if (selector == Module::ANY_PROCESS) {
        return true;
    }

    GeckoProcessType type = XRE_GetProcessType();
    switch (selector) {
      case Module::MAIN_PROCESS_ONLY:
        return type == GeckoProcessType_Default;
      case Module::CONTENT_PROCESS_ONLY:
        return type == GeckoProcessType_Content;
      default:
        MOZ_CRASH("invalid process selector");
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

    nsFactoryEntry* f = mFactories.Get(*aEntry->cid);
    if (f) {
        NS_WARNING("Re-registering a CID?");

        char idstr[NSID_LENGTH];
        aEntry->cid->ToProvidedString(idstr);

        nsCString existing;
        if (f->mModule)
            existing = f->mModule->Description();
        else
            existing = "<unknown module>";
        SafeMutexAutoUnlock unlock(mLock);
        LogMessage("While registering XPCOM module %s, trying to re-register CID '%s' already registered by %s.",
                   aModule->Description().get(),
                   idstr,
                   existing.get());
        return;
    }

    f = new nsFactoryEntry(aEntry, aModule);
    mFactories.Put(*aEntry->cid, f);
}

void
nsComponentManagerImpl::RegisterContractIDLocked(
    const mozilla::Module::ContractIDEntry* aEntry)
{
    mLock.AssertCurrentThreadOwns();

    if (!ProcessSelectorMatches(aEntry->processSelector)) {
        return;
    }

    nsFactoryEntry* f = mFactories.Get(*aEntry->cid);
    if (!f) {
        NS_ERROR("No CID found when attempting to map contract ID");

        char idstr[NSID_LENGTH];
        aEntry->cid->ToProvidedString(idstr);

        LogMessage("Could not map contract ID '%s' to CID %s because no implementation of the CID is registered.",
                   aEntry->contractid,
                   idstr);
                   
        return;
    }

    mContractIDs.Put(nsDependentCString(aEntry->contractid), f);
}

static void
CutExtension(nsCString& path)
{
    int32_t dotPos = path.RFindChar('.');
    if (kNotFound == dotPos)
        path.Truncate();
    else
        path.Cut(0, dotPos + 1);
}

void
nsComponentManagerImpl::RegisterManifest(NSLocationType aType,
                                         FileLocation &aFile,
                                         bool aChromeOnly)
{
    uint32_t len;
    FileLocation::Data data;
    nsAutoArrayPtr<char> buf;
    nsresult rv = aFile.GetData(data);
    if (NS_SUCCEEDED(rv)) {
        rv = data.GetSize(&len);
    }
    if (NS_SUCCEEDED(rv)) {
        buf = new char[len + 1];
        rv = data.Copy(buf, len);
    }
    if (NS_SUCCEEDED(rv)) {
        buf[len] = '\0';
        ParseManifest(aType, aFile, buf, aChromeOnly);
    } else if (NS_BOOTSTRAPPED_LOCATION != aType) {
        nsCString uri;
        aFile.GetURIString(uri);
        LogMessage("Could not read chrome manifest '%s'.", uri.get());
    }
}

void
nsComponentManagerImpl::ManifestManifest(ManifestProcessingContext& cx, int lineno, char *const * argv)
{
    char* file = argv[0];
    FileLocation f(cx.mFile, file);
    RegisterManifest(cx.mType, f, cx.mChromeOnly);
}

void
nsComponentManagerImpl::ManifestBinaryComponent(ManifestProcessingContext& cx, int lineno, char *const * argv)
{
    if (cx.mFile.IsZip()) {
        NS_WARNING("Cannot load binary components from a jar.");
        LogMessageWithContext(cx.mFile, lineno,
                              "Cannot load binary components from a jar.");
        return;
    }

    FileLocation f(cx.mFile, argv[0]);
    nsCString uri;
    f.GetURIString(uri);

    if (mKnownModules.Get(uri)) {
        NS_WARNING("Attempting to register a binary component twice.");
        LogMessageWithContext(cx.mFile, lineno,
                              "Attempting to register a binary component twice.");
        return;
    }

    const mozilla::Module* m = mNativeModuleLoader.LoadModule(f);
    // The native module loader should report an error here, we don't have to
    if (!m)
        return;

    RegisterModule(m, &f);
}

void
nsComponentManagerImpl::ManifestXPT(ManifestProcessingContext& cx, int lineno, char *const * argv)
{
    FileLocation f(cx.mFile, argv[0]);
    uint32_t len;
    FileLocation::Data data;
    nsAutoArrayPtr<char> buf;
    nsresult rv = f.GetData(data);
    if (NS_SUCCEEDED(rv)) {
        rv = data.GetSize(&len);
    }
    if (NS_SUCCEEDED(rv)) {
        buf = new char[len];
        rv = data.Copy(buf, len);
    }
    if (NS_SUCCEEDED(rv)) {
        XPTInterfaceInfoManager::GetSingleton()->RegisterBuffer(buf, len);
    } else {
        nsCString uri;
        f.GetURIString(uri);
        LogMessage("Could not read '%s'.", uri.get());
    }
}

void
nsComponentManagerImpl::ManifestComponent(ManifestProcessingContext& cx, int lineno, char *const * argv)
{
    mLock.AssertNotCurrentThreadOwns();

    char* id = argv[0];
    char* file = argv[1];

    nsID cid;
    if (!cid.Parse(id)) {
        LogMessageWithContext(cx.mFile, lineno,
                              "Malformed CID: '%s'.", id);
        return;
    }

    // Precompute the hash/file data outside of the lock
    FileLocation fl(cx.mFile, file);
    nsCString hash;
    fl.GetURIString(hash);

    MutexLock lock(mLock);
    nsFactoryEntry* f = mFactories.Get(cid);
    if (f) {
        char idstr[NSID_LENGTH];
        cid.ToProvidedString(idstr);

        nsCString existing;
        if (f->mModule)
            existing = f->mModule->Description();
        else
            existing = "<unknown module>";

        lock.Unlock();

        LogMessageWithContext(cx.mFile, lineno,
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

    void* place;

    PL_ARENA_ALLOCATE(place, &mArena, sizeof(nsCID));
    nsID* permanentCID = static_cast<nsID*>(place);
    *permanentCID = cid;

    PL_ARENA_ALLOCATE(place, &mArena, sizeof(mozilla::Module::CIDEntry));
    mozilla::Module::CIDEntry* e = new (place) mozilla::Module::CIDEntry();
    e->cid = permanentCID;

    f = new nsFactoryEntry(e, km);
    mFactories.Put(cid, f);
}

void
nsComponentManagerImpl::ManifestContract(ManifestProcessingContext& cx, int lineno, char *const * argv)
{
    mLock.AssertNotCurrentThreadOwns();

    char* contract = argv[0];
    char* id = argv[1];

    nsID cid;
    if (!cid.Parse(id)) {
        LogMessageWithContext(cx.mFile, lineno,
                              "Malformed CID: '%s'.", id);
        return;
    }

    MutexLock lock(mLock);
    nsFactoryEntry* f = mFactories.Get(cid);
    if (!f) {
        lock.Unlock();
        LogMessageWithContext(cx.mFile, lineno,
                              "Could not map contract ID '%s' to CID %s because no implementation of the CID is registered.",
                              contract, id);
        return;
    }

    mContractIDs.Put(nsDependentCString(contract), f);
}

void
nsComponentManagerImpl::ManifestCategory(ManifestProcessingContext& cx, int lineno, char *const * argv)
{
    char* category = argv[0];
    char* key = argv[1];
    char* value = argv[2];

    nsCategoryManager::GetSingleton()->
        AddCategoryEntry(category, key, value);
}

void
nsComponentManagerImpl::RereadChromeManifests(bool aChromeOnly)
{
    for (uint32_t i = 0; i < sModuleLocations->Length(); ++i) {
        ComponentLocation& l = sModuleLocations->ElementAt(i);
        RegisterManifest(l.type, l.location, aChromeOnly);
    }
}

bool
nsComponentManagerImpl::KnownModule::EnsureLoader()
{
    if (!mLoader) {
        nsCString extension;
        mFile.GetURIString(extension);
        CutExtension(extension);
        mLoader = nsComponentManagerImpl::gComponentManager->LoaderForExtension(extension);
    }
    return !!mLoader;
}

bool
nsComponentManagerImpl::KnownModule::Load()
{
    if (mFailed)
        return false;
    if (!mModule) {
        if (!EnsureLoader())
            return false;

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
    if (mFile)
        mFile.GetURIString(s);
    else
        s = "<static module>";
    return s;
}

nsresult nsComponentManagerImpl::Shutdown(void)
{
    PR_ASSERT(NORMAL == mStatus);

    mStatus = SHUTDOWN_IN_PROGRESS;

    // Shutdown the component manager
    PR_LOG(nsComponentManagerLog, PR_LOG_DEBUG, ("nsComponentManager: Beginning Shutdown."));

    UnregisterWeakMemoryReporter(this);

    // Release all cached factories
    mContractIDs.Clear();
    mFactories.Clear(); // XXX release the objects, don't just clear
    mLoaderMap.Clear();
    mKnownModules.Clear();
    mKnownStaticModules.Clear();

    delete sStaticModules;
    delete sModuleLocations;

    // Unload libraries
    mNativeModuleLoader.UnloadLibraries();

    // delete arena for strings and small objects
    PL_FinishArenaPool(&mArena);

    mStatus = SHUTDOWN_COMPLETE;

    PR_LOG(nsComponentManagerLog, PR_LOG_DEBUG, ("nsComponentManager: Shutdown complete."));

    return NS_OK;
}

nsComponentManagerImpl::~nsComponentManagerImpl()
{
    PR_LOG(nsComponentManagerLog, PR_LOG_DEBUG, ("nsComponentManager: Beginning destruction."));

    if (SHUTDOWN_COMPLETE != mStatus)
        Shutdown();

    PR_LOG(nsComponentManagerLog, PR_LOG_DEBUG, ("nsComponentManager: Destroyed."));
}

NS_IMPL_ISUPPORTS(
    nsComponentManagerImpl,
    nsIComponentManager,
    nsIServiceManager,
    nsIComponentRegistrar,
    nsISupportsWeakReference,
    nsIInterfaceRequestor,
    nsIMemoryReporter)

nsresult
nsComponentManagerImpl::GetInterface(const nsIID & uuid, void **result)
{
    NS_WARNING("This isn't supported");
    // fall through to QI as anything QIable is a superset of what can be
    // got via the GetInterface()
    return  QueryInterface(uuid, result);
}

nsFactoryEntry *
nsComponentManagerImpl::GetFactoryEntry(const char *aContractID,
                                        uint32_t aContractIDLen)
{
    SafeMutexAutoLock lock(mLock);
    return mContractIDs.Get(nsDependentCString(aContractID, aContractIDLen));
}


nsFactoryEntry *
nsComponentManagerImpl::GetFactoryEntry(const nsCID &aClass)
{
    SafeMutexAutoLock lock(mLock);
    return mFactories.Get(aClass);
}

already_AddRefed<nsIFactory>
nsComponentManagerImpl::FindFactory(const nsCID& aClass)
{
    nsFactoryEntry* e = GetFactoryEntry(aClass);
    if (!e)
        return nullptr;

    return e->GetFactory();
}

already_AddRefed<nsIFactory>
nsComponentManagerImpl::FindFactory(const char *contractID,
                                    uint32_t aContractIDLen)
{
    nsFactoryEntry *entry = GetFactoryEntry(contractID, aContractIDLen);
    if (!entry)
        return nullptr;

    return entry->GetFactory();
}

/**
 * GetClassObject()
 *
 * Given a classID, this finds the singleton ClassObject that implements the CID.
 * Returns an interface of type aIID off the singleton classobject.
 */
NS_IMETHODIMP
nsComponentManagerImpl::GetClassObject(const nsCID &aClass, const nsIID &aIID,
                                       void **aResult)
{
    nsresult rv;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_DEBUG))
    {
        char *buf = aClass.ToString();
        PR_LogPrint("nsComponentManager: GetClassObject(%s)", buf);
        if (buf)
            NS_Free(buf);
    }
#endif

    PR_ASSERT(aResult != nullptr);

    nsCOMPtr<nsIFactory> factory = FindFactory(aClass);
    if (!factory)
        return NS_ERROR_FACTORY_NOT_REGISTERED;

    rv = factory->QueryInterface(aIID, aResult);

    PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
           ("\t\tGetClassObject() %s", NS_SUCCEEDED(rv) ? "succeeded" : "FAILED"));

    return rv;
}


NS_IMETHODIMP
nsComponentManagerImpl::GetClassObjectByContractID(const char *contractID,
                                                   const nsIID &aIID,
                                                   void **aResult)
{
    if (NS_WARN_IF(!aResult) ||
        NS_WARN_IF(!contractID))
        return NS_ERROR_INVALID_ARG;

    nsresult rv;


#ifdef PR_LOGGING
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_DEBUG))
    {
        PR_LogPrint("nsComponentManager: GetClassObject(%s)", contractID);
    }
#endif

    nsCOMPtr<nsIFactory> factory = FindFactory(contractID, strlen(contractID));
    if (!factory)
        return NS_ERROR_FACTORY_NOT_REGISTERED;

    rv = factory->QueryInterface(aIID, aResult);

    PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
           ("\t\tGetClassObject() %s", NS_SUCCEEDED(rv) ? "succeeded" : "FAILED"));

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
nsComponentManagerImpl::CreateInstance(const nsCID &aClass,
                                       nsISupports *aDelegate,
                                       const nsIID &aIID,
                                       void **aResult)
{
    // test this first, since there's no point in creating a component during
    // shutdown -- whether it's available or not would depend on the order it
    // occurs in the list
    if (gXPCOMShuttingDown) {
        // When processing shutdown, don't process new GetService() requests
#ifdef SHOW_DENIED_ON_SHUTDOWN
        nsXPIDLCString cid, iid;
        cid.Adopt(aClass.ToString());
        iid.Adopt(aIID.ToString());
        fprintf(stderr, "Creating new instance on shutdown. Denied.\n"
               "         CID: %s\n         IID: %s\n", cid.get(), iid.get());
#endif /* SHOW_DENIED_ON_SHUTDOWN */
        return NS_ERROR_UNEXPECTED;
    }

    if (aResult == nullptr)
    {
        return NS_ERROR_NULL_POINTER;
    }
    *aResult = nullptr;

    nsFactoryEntry *entry = GetFactoryEntry(aClass);

    if (!entry)
        return NS_ERROR_FACTORY_NOT_REGISTERED;

#ifdef SHOW_CI_ON_EXISTING_SERVICE
    if (entry->mServiceObject) {
        nsXPIDLCString cid;
        cid.Adopt(aClass.ToString());
        nsAutoCString message;
        message = NS_LITERAL_CSTRING("You are calling CreateInstance \"") +
                  cid + NS_LITERAL_CSTRING("\" when a service for this CID already exists!");
        NS_ERROR(message.get());
    }
#endif

    nsresult rv;
    nsCOMPtr<nsIFactory> factory = entry->GetFactory();
    if (factory)
    {
        rv = factory->CreateInstance(aDelegate, aIID, aResult);
        if (NS_SUCCEEDED(rv) && !*aResult) {
            NS_ERROR("Factory did not return an object but returned success!");
            rv = NS_ERROR_SERVICE_NOT_FOUND;
        }
    }
    else {
        // Translate error values
        rv = NS_ERROR_FACTORY_NOT_REGISTERED;
    }

#ifdef PR_LOGGING
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_WARNING))
    {
        char *buf = aClass.ToString();
        PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
               ("nsComponentManager: CreateInstance(%s) %s", buf,
                NS_SUCCEEDED(rv) ? "succeeded" : "FAILED"));
        if (buf)
            NS_Free(buf);
    }
#endif

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
nsComponentManagerImpl::CreateInstanceByContractID(const char *aContractID,
                                                   nsISupports *aDelegate,
                                                   const nsIID &aIID,
                                                   void **aResult)
{
    if (NS_WARN_IF(!aContractID))
        return NS_ERROR_INVALID_ARG;

    // test this first, since there's no point in creating a component during
    // shutdown -- whether it's available or not would depend on the order it
    // occurs in the list
    if (gXPCOMShuttingDown) {
        // When processing shutdown, don't process new GetService() requests
#ifdef SHOW_DENIED_ON_SHUTDOWN
        nsXPIDLCString iid;
        iid.Adopt(aIID.ToString());
        fprintf(stderr, "Creating new instance on shutdown. Denied.\n"
               "  ContractID: %s\n         IID: %s\n", aContractID, iid.get());
#endif /* SHOW_DENIED_ON_SHUTDOWN */
        return NS_ERROR_UNEXPECTED;
    }

    if (aResult == nullptr)
    {
        return NS_ERROR_NULL_POINTER;
    }
    *aResult = nullptr;

    nsFactoryEntry *entry = GetFactoryEntry(aContractID, strlen(aContractID));

    if (!entry)
        return NS_ERROR_FACTORY_NOT_REGISTERED;

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
    if (factory)
    {

        rv = factory->CreateInstance(aDelegate, aIID, aResult);
        if (NS_SUCCEEDED(rv) && !*aResult) {
            NS_ERROR("Factory did not return an object but returned success!");
            rv = NS_ERROR_SERVICE_NOT_FOUND;
        }
    }
    else {
        // Translate error values
        rv = NS_ERROR_FACTORY_NOT_REGISTERED;
    }

    PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
           ("nsComponentManager: CreateInstanceByContractID(%s) %s", aContractID,
            NS_SUCCEEDED(rv) ? "succeeded" : "FAILED"));

    return rv;
}

static PLDHashOperator
FreeFactoryEntries(const nsID& aCID,
                   nsFactoryEntry* aEntry,
                   void* arg)
{
    aEntry->mFactory = nullptr;
    aEntry->mServiceObject = nullptr;
    return PL_DHASH_NEXT;
}

nsresult
nsComponentManagerImpl::FreeServices()
{
    NS_ASSERTION(gXPCOMShuttingDown, "Must be shutting down in order to free all services");

    if (!gXPCOMShuttingDown)
        return NS_ERROR_FAILURE;

    mFactories.EnumerateRead(FreeFactoryEntries, nullptr);
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
                                   void* *result)
{
    // test this first, since there's no point in returning a service during
    // shutdown -- whether it's available or not would depend on the order it
    // occurs in the list
    if (gXPCOMShuttingDown) {
        // When processing shutdown, don't process new GetService() requests
#ifdef SHOW_DENIED_ON_SHUTDOWN
        nsXPIDLCString cid, iid;
        cid.Adopt(aClass.ToString());
        iid.Adopt(aIID.ToString());
        fprintf(stderr, "Getting service on shutdown. Denied.\n"
               "         CID: %s\n         IID: %s\n", cid.get(), iid.get());
#endif /* SHOW_DENIED_ON_SHUTDOWN */
        return NS_ERROR_UNEXPECTED;
    }

    // `service` must be released after the lock is released, so it must be
    // declared before the lock in this C++ block.
    nsCOMPtr<nsISupports> service;
    MutexLock lock(mLock);

    nsFactoryEntry* entry = mFactories.Get(aClass);
    if (!entry)
        return NS_ERROR_FACTORY_NOT_REGISTERED;

    if (entry->mServiceObject) {
        lock.Unlock();
        return entry->mServiceObject->QueryInterface(aIID, result);
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
        return entry->mServiceObject->QueryInterface(aIID, result);
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

    if (NS_FAILED(rv))
        return rv;

    NS_ASSERTION(!entry->mServiceObject, "Created two instances of a service!");

    entry->mServiceObject = service.forget();

    lock.Unlock();
    nsISupports** sresult = reinterpret_cast<nsISupports**>(result);
    *sresult = entry->mServiceObject;
    (*sresult)->AddRef();

    return NS_OK;
}

NS_IMETHODIMP
nsComponentManagerImpl::IsServiceInstantiated(const nsCID & aClass,
                                              const nsIID& aIID,
                                              bool *result)
{
    // Now we want to get the service if we already got it. If not, we don't want
    // to create an instance of it. mmh!

    // test this first, since there's no point in returning a service during
    // shutdown -- whether it's available or not would depend on the order it
    // occurs in the list
    if (gXPCOMShuttingDown) {
        // When processing shutdown, don't process new GetService() requests
#ifdef SHOW_DENIED_ON_SHUTDOWN
        nsXPIDLCString cid, iid;
        cid.Adopt(aClass.ToString());
        iid.Adopt(aIID.ToString());
        fprintf(stderr, "Checking for service on shutdown. Denied.\n"
               "         CID: %s\n         IID: %s\n", cid.get(), iid.get());
#endif /* SHOW_DENIED_ON_SHUTDOWN */
        return NS_ERROR_UNEXPECTED;
    }

    nsresult rv = NS_ERROR_SERVICE_NOT_AVAILABLE;
    nsFactoryEntry* entry;

    {
        SafeMutexAutoLock lock(mLock);
        entry = mFactories.Get(aClass);
    }

    if (entry && entry->mServiceObject) {
        nsCOMPtr<nsISupports> service;
        rv = entry->mServiceObject->QueryInterface(aIID, getter_AddRefs(service));
        *result = (service!=nullptr);
    }

    return rv;
}

NS_IMETHODIMP nsComponentManagerImpl::IsServiceInstantiatedByContractID(const char *aContractID,
                                                                        const nsIID& aIID,
                                                                        bool *result)
{
    // Now we want to get the service if we already got it. If not, we don't want
    // to create an instance of it. mmh!

    // test this first, since there's no point in returning a service during
    // shutdown -- whether it's available or not would depend on the order it
    // occurs in the list
    if (gXPCOMShuttingDown) {
        // When processing shutdown, don't process new GetService() requests
#ifdef SHOW_DENIED_ON_SHUTDOWN
        nsXPIDLCString iid;
        iid.Adopt(aIID.ToString());
        fprintf(stderr, "Checking for service on shutdown. Denied.\n"
               "  ContractID: %s\n         IID: %s\n", aContractID, iid.get());
#endif /* SHOW_DENIED_ON_SHUTDOWN */
        return NS_ERROR_UNEXPECTED;
    }

    nsresult rv = NS_ERROR_SERVICE_NOT_AVAILABLE;
    nsFactoryEntry *entry;
    {
        SafeMutexAutoLock lock(mLock);
        entry = mContractIDs.Get(nsDependentCString(aContractID));
    }

    if (entry && entry->mServiceObject) {
        nsCOMPtr<nsISupports> service;
        rv = entry->mServiceObject->QueryInterface(aIID, getter_AddRefs(service));
        *result = (service!=nullptr);
    }
    return rv;
}


NS_IMETHODIMP
nsComponentManagerImpl::GetServiceByContractID(const char* aContractID,
                                               const nsIID& aIID,
                                               void* *result)
{
    // test this first, since there's no point in returning a service during
    // shutdown -- whether it's available or not would depend on the order it
    // occurs in the list
    if (gXPCOMShuttingDown) {
        // When processing shutdown, don't process new GetService() requests
#ifdef SHOW_DENIED_ON_SHUTDOWN
        nsXPIDLCString iid;
        iid.Adopt(aIID.ToString());
        fprintf(stderr, "Getting service on shutdown. Denied.\n"
               "  ContractID: %s\n         IID: %s\n", aContractID, iid.get());
#endif /* SHOW_DENIED_ON_SHUTDOWN */
        return NS_ERROR_UNEXPECTED;
    }

    // `service` must be released after the lock is released, so it must be
    // declared before the lock in this C++ block.
    nsCOMPtr<nsISupports> service;
    MutexLock lock(mLock);

    nsFactoryEntry *entry = mContractIDs.Get(nsDependentCString(aContractID));
    if (!entry)
        return NS_ERROR_FACTORY_NOT_REGISTERED;

    if (entry->mServiceObject) {
        // We need to not be holding the service manager's monitor while calling
        // QueryInterface, because it invokes user code which could try to re-enter
        // the service manager, or try to grab some other lock/monitor/condvar
        // and deadlock, e.g. bug 282743.
        // `entry` is valid until XPCOM shutdown, so we can safely use it after
        // exiting the lock.
        lock.Unlock();
        return entry->mServiceObject->QueryInterface(aIID, result);
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
        return entry->mServiceObject->QueryInterface(aIID, result);
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

    if (NS_FAILED(rv))
        return rv;

    NS_ASSERTION(!entry->mServiceObject, "Created two instances of a service!");

    entry->mServiceObject = service.forget();

    lock.Unlock();

    nsISupports** sresult = reinterpret_cast<nsISupports**>(result);
    *sresult = entry->mServiceObject;
    (*sresult)->AddRef();

    return NS_OK;
}

already_AddRefed<mozilla::ModuleLoader>
nsComponentManagerImpl::LoaderForExtension(const nsACString& aExt)
{
    nsCOMPtr<mozilla::ModuleLoader> loader = mLoaderMap.Get(aExt);
    if (!loader) {
        loader = do_GetServiceFromCategory("module-loader",
                                           PromiseFlatCString(aExt).get());
        if (!loader)
            return nullptr;

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
        if (!aContractID)
            return NS_ERROR_INVALID_ARG;

        SafeMutexAutoLock lock(mLock);
        nsFactoryEntry* oldf = mFactories.Get(aClass);
        if (!oldf)
            return NS_ERROR_FACTORY_NOT_REGISTERED;

        mContractIDs.Put(nsDependentCString(aContractID), oldf);
        return NS_OK;
    }

    nsAutoPtr<nsFactoryEntry> f(new nsFactoryEntry(aClass, aFactory));

    SafeMutexAutoLock lock(mLock);
    nsFactoryEntry* oldf = mFactories.Get(aClass);
    if (oldf)
        return NS_ERROR_FACTORY_EXISTS;

    if (aContractID)
        mContractIDs.Put(nsDependentCString(aContractID), f);

    mFactories.Put(aClass, f.forget());

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
        nsFactoryEntry* f = mFactories.Get(aClass);
        if (!f || f->mFactory != aFactory)
            return NS_ERROR_FACTORY_NOT_REGISTERED;

        mFactories.Remove(aClass);

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
    XRE_AddManifestLocation(NS_COMPONENT_LOCATION, aLocation);
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
nsComponentManagerImpl::IsCIDRegistered(const nsCID & aClass,
                                        bool *_retval)
{
    *_retval = (nullptr != GetFactoryEntry(aClass));
    return NS_OK;
}

NS_IMETHODIMP
nsComponentManagerImpl::IsContractIDRegistered(const char *aClass,
                                               bool *_retval)
{
    if (NS_WARN_IF(!aClass))
        return NS_ERROR_INVALID_ARG;

    nsFactoryEntry *entry = GetFactoryEntry(aClass, strlen(aClass));

    if (entry)
        *_retval = true;
    else
        *_retval = false;
    return NS_OK;
}

static PLDHashOperator
EnumerateCIDHelper(const nsID& id, nsFactoryEntry* entry, void* closure)
{
    nsCOMArray<nsISupports> *array = static_cast<nsCOMArray<nsISupports>*>(closure);
    nsCOMPtr<nsISupportsID> wrapper = new nsSupportsIDImpl();
    wrapper->SetData(&id);
    array->AppendObject(wrapper);
    return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsComponentManagerImpl::EnumerateCIDs(nsISimpleEnumerator **aEnumerator)
{
    nsCOMArray<nsISupports> array;
    mFactories.EnumerateRead(EnumerateCIDHelper, &array);

    return NS_NewArrayEnumerator(aEnumerator, array);
}

static PLDHashOperator
EnumerateContractsHelper(const nsACString& contract, nsFactoryEntry* entry, void* closure)
{
    nsTArray<nsCString>* array = static_cast<nsTArray<nsCString>*>(closure);
    array->AppendElement(contract);
    return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsComponentManagerImpl::EnumerateContractIDs(nsISimpleEnumerator **aEnumerator)
{
    nsTArray<nsCString>* array = new nsTArray<nsCString>;
    mContractIDs.EnumerateRead(EnumerateContractsHelper, array);

    nsCOMPtr<nsIUTF8StringEnumerator> e;
    nsresult rv = NS_NewAdoptingUTF8StringEnumerator(getter_AddRefs(e), array);
    if (NS_FAILED(rv))
        return rv;

    return CallQueryInterface(e, aEnumerator);
}

NS_IMETHODIMP
nsComponentManagerImpl::CIDToContractID(const nsCID & aClass,
                                        char **_retval)
{
    NS_ERROR("CIDTOContractID not implemented");
    return NS_ERROR_FACTORY_NOT_REGISTERED;
}

NS_IMETHODIMP
nsComponentManagerImpl::ContractIDToCID(const char *aContractID,
                                        nsCID * *_retval)
{
    {
        SafeMutexAutoLock lock(mLock);
        nsFactoryEntry* entry = mContractIDs.Get(nsDependentCString(aContractID));
        if (entry) {
            *_retval = (nsCID*) NS_Alloc(sizeof(nsCID));
            **_retval = *entry->mCIDEntry->cid;
            return NS_OK;
        }
    }
    *_retval = nullptr;
    return NS_ERROR_FACTORY_NOT_REGISTERED;
}

static size_t
SizeOfFactoriesEntryExcludingThis(nsIDHashKey::KeyType aKey,
                                  nsFactoryEntry* const &aData,
                                  MallocSizeOf aMallocSizeOf,
                                  void* aUserArg)
{
    return aData->SizeOfIncludingThis(aMallocSizeOf);
}

static size_t
SizeOfContractIDsEntryExcludingThis(nsCStringHashKey::KeyType aKey,
                                    nsFactoryEntry* const &aData,
                                    MallocSizeOf aMallocSizeOf,
                                    void* aUserArg)
{
    // We don't measure the nsFactoryEntry data because its owned by mFactories
    // (which measures them in SizeOfFactoriesEntryExcludingThis).
    return aKey.SizeOfExcludingThisMustBeUnshared(aMallocSizeOf);
}

MOZ_DEFINE_MALLOC_SIZE_OF(ComponentManagerMallocSizeOf)

NS_IMETHODIMP
nsComponentManagerImpl::CollectReports(nsIHandleReportCallback* aHandleReport,
                                       nsISupports* aData)
{
    return MOZ_COLLECT_REPORT(
        "explicit/xpcom/component-manager", KIND_HEAP, UNITS_BYTES,
        SizeOfIncludingThis(ComponentManagerMallocSizeOf),
        "Memory used for the XPCOM component manager.");
}

size_t
nsComponentManagerImpl::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
{
    size_t n = aMallocSizeOf(this);
    n += mLoaderMap.SizeOfExcludingThis(nullptr, aMallocSizeOf);
    n += mFactories.SizeOfExcludingThis(SizeOfFactoriesEntryExcludingThis, aMallocSizeOf);
    n += mContractIDs.SizeOfExcludingThis(SizeOfContractIDsEntryExcludingThis, aMallocSizeOf);

    n += sStaticModules->SizeOfIncludingThis(aMallocSizeOf);
    n += sModuleLocations->SizeOfIncludingThis(aMallocSizeOf);

    n += mKnownStaticModules.SizeOfExcludingThis(aMallocSizeOf);
    n += mKnownModules.SizeOfExcludingThis(nullptr, aMallocSizeOf);

    n += PL_SizeOfArenaPoolExcludingPool(&mArena, aMallocSizeOf);

    n += mPendingServices.SizeOfExcludingThis(aMallocSizeOf);

    // Measurement of the following members may be added later if DMD finds it is
    // worthwhile:
    // - mLoaderMap's keys and values
    // - mMon
    // - sStaticModules' entries
    // - sModuleLocations' entries
    // - mNativeModuleLoader
    // - mKnownStaticModules' entries?
    // - mKnownModules' keys and values?

    return n;
}

////////////////////////////////////////////////////////////////////////////////
// nsFactoryEntry
////////////////////////////////////////////////////////////////////////////////

nsFactoryEntry::nsFactoryEntry(const mozilla::Module::CIDEntry* entry,
                               nsComponentManagerImpl::KnownModule* module)
    : mCIDEntry(entry)
    , mModule(module)
{
}

nsFactoryEntry::nsFactoryEntry(const nsCID& aCID, nsIFactory* factory)
    : mCIDEntry(nullptr)
    , mModule(nullptr)
    , mFactory(factory)
{
    mozilla::Module::CIDEntry* e = new mozilla::Module::CIDEntry();
    nsCID* cid = new nsCID;
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
        if (!mModule)
            return nullptr;

        if (!mModule->Load())
            return nullptr;

        // Don't set mFactory directly, it needs to be locked
        nsCOMPtr<nsIFactory> factory;

        if (mModule->Module()->getFactoryProc) {
            factory = mModule->Module()->getFactoryProc(*mModule->Module(),
                                                        *mCIDEntry);
        }
        else if (mCIDEntry->getFactoryProc) {
            factory = mCIDEntry->getFactoryProc(*mModule->Module(), *mCIDEntry);
        }
        else {
            NS_ASSERTION(mCIDEntry->constructorProc, "no getfactory or constructor");
            factory = new mozilla::GenericFactory(mCIDEntry->constructorProc);
        }
        if (!factory)
            return nullptr;

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
NS_GetComponentManager(nsIComponentManager* *result)
{
    if (!nsComponentManagerImpl::gComponentManager)
        return NS_ERROR_NOT_INITIALIZED;

    NS_ADDREF(*result = nsComponentManagerImpl::gComponentManager);
    return NS_OK;
}

nsresult
NS_GetServiceManager(nsIServiceManager* *result)
{
    if (!nsComponentManagerImpl::gComponentManager)
        return NS_ERROR_NOT_INITIALIZED;

    NS_ADDREF(*result = nsComponentManagerImpl::gComponentManager);
    return NS_OK;
}


nsresult
NS_GetComponentRegistrar(nsIComponentRegistrar* *result)
{
    if (!nsComponentManagerImpl::gComponentManager)
        return NS_ERROR_NOT_INITIALIZED;

    NS_ADDREF(*result = nsComponentManagerImpl::gComponentManager);
    return NS_OK;
}

EXPORT_XPCOM_API(nsresult)
XRE_AddStaticComponent(const mozilla::Module* aComponent)
{
    nsComponentManagerImpl::InitializeStaticModules();
    nsComponentManagerImpl::sStaticModules->AppendElement(aComponent);

    if (nsComponentManagerImpl::gComponentManager &&
        nsComponentManagerImpl::NORMAL == nsComponentManagerImpl::gComponentManager->mStatus)
        nsComponentManagerImpl::gComponentManager->RegisterModule(aComponent, nullptr);

    return NS_OK;
}

NS_IMETHODIMP
nsComponentManagerImpl::AddBootstrappedManifestLocation(nsIFile* aLocation)
{
  nsString path;
  nsresult rv = aLocation->GetPath(path);
  if (NS_FAILED(rv))
    return rv;

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
  if (!cr)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIFile> manifest;
  nsString path;
  nsresult rv = aLocation->GetPath(path);
  if (NS_FAILED(rv))
    return rv;

  nsComponentManagerImpl::ComponentLocation elem;
  elem.type = NS_BOOTSTRAPPED_LOCATION;

  if (Substring(path, path.Length() - 4).EqualsLiteral(".xpi")) {
    elem.location.Init(aLocation, "chrome.manifest");
  } else {
    nsCOMPtr<nsIFile> lf = CloneAndAppend(aLocation, NS_LITERAL_CSTRING("chrome.manifest"));
    elem.location.Init(lf);
  }

  // Remove reference.
  nsComponentManagerImpl::sModuleLocations->RemoveElement(elem, ComponentLocationComparator());

  rv = cr->CheckForNewChrome();
  return rv;
}

NS_IMETHODIMP
nsComponentManagerImpl::GetManifestLocations(nsIArray **aLocations)
{
  NS_ENSURE_ARG_POINTER(aLocations);
  *aLocations = nullptr;

  if (!sModuleLocations)
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIMutableArray> locations = nsArray::Create();
  nsresult rv;
  for (uint32_t i = 0; i < sModuleLocations->Length(); ++i) {
    ComponentLocation& l = sModuleLocations->ElementAt(i);
    FileLocation loc = l.location;
    nsCString uriString;
    loc.GetURIString(uriString);
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), uriString);
    if (NS_SUCCEEDED(rv))
      locations->AppendElement(uri, false);
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
        nsComponentManagerImpl::NORMAL == nsComponentManagerImpl::gComponentManager->mStatus)
        nsComponentManagerImpl::gComponentManager->RegisterManifest(aType, c->location, false);

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
        nsComponentManagerImpl::NORMAL == nsComponentManagerImpl::gComponentManager->mStatus)
        nsComponentManagerImpl::gComponentManager->RegisterManifest(aType, c->location, false);

    return NS_OK;
}

