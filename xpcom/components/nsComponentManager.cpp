/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
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
#define NS_CM_BLOCK_SIZE (1024)

#include "NSReg.h"
#include "nsAutoLock.h"
#include "nsCOMPtr.h"
#include "nsComponentManager.h"
#include "nsComponentManagerObsolete.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsICategoryManager.h"
#include "nsCategoryManager.h"
#include "nsCategoryManagerUtils.h"
#include "nsIComponentLoader.h"
#include "nsIEnumerator.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIModule.h"
#include "nsIObserverService.h"
#include "nsISimpleEnumerator.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsLocalFile.h"
#include "nsNativeComponentLoader.h"
#include "nsRegistry.h"
#include "nsXPIDLString.h"
#include "prcmon.h"
#include "xptinfo.h" // this after nsISupports, to pick up IID so that xpt stuff doesn't try to define it itself...

#include "nsInt64.h"
#include "nsManifestLineReader.h"

#include NEW_H     // for placement new


#ifdef XP_BEOS
#include <FindDirectory.h>
#include <Path.h>
#endif

// Logging of debug output
#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif
#include "prlog.h"

PRLogModuleInfo* nsComponentManagerLog = nsnull;

#if defined(DEBUG)
// #define SHOW_DENIED_ON_SHUTDOWN
#endif

// Loader Types
#define NS_LOADER_DATA_ALLOC_STEP 6

// Bloated registry buffer size to improve startup performance -- needs to
// be big enough to fit the entire file into memory or it'll thrash.
// 512K is big enough to allow for some future growth in the registry.
#define BIG_REGISTRY_BUFLEN   (512*1024)

// Common Key Names 
const char classIDKeyName[]="classID";
const char classesKeyName[]="contractID";
const char componentLoadersKeyName[]="componentLoaders";
const char componentsKeyName[]="components";
const char xpcomComponentsKeyName[]="software/mozilla/XPCOM/components";
const char xpcomKeyName[]="software/mozilla/XPCOM";

// Common Value Names
const char classIDValueName[]="ClassID";
const char classNameValueName[]="ClassName";
const char componentCountValueName[]="ComponentsCount";
const char componentTypeValueName[]="ComponentType";
const char contractIDValueName[]="ContractID";
const char fileSizeValueName[]="FileSize";
const char inprocServerValueName[]="InprocServer";
const char lastModValueName[]="LastModTimeStamp";
const char nativeComponentType[]="application/x-mozilla-native";
const char staticComponentType[]="application/x-mozilla-static";
const char versionValueName[]="VersionString";

const static char XPCOM_ABSCOMPONENT_PREFIX[] = "abs:";
const static char XPCOM_RELCOMPONENT_PREFIX[] = "rel:";
const char XPCOM_LIB_PREFIX[]          = "lib:";

const static char persistentRegistryFilename[]     = "compreg.dat";
const static char persistentRegistryTempFilename[] = "compreg.tmp";

// Nonexistent factory entry
// This is used to mark non-existent contractid mappings
static nsFactoryEntry * kNonExistentContractID = (nsFactoryEntry*) 1;


#define NS_EMPTY_IID                                 \
{                                                    \
    0x00000000,                                      \
    0x0000,                                          \
    0x0000,                                          \
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00} \
}

NS_DEFINE_CID(kEmptyCID, NS_EMPTY_IID);
NS_DEFINE_CID(kCategoryManagerCID, NS_CATEGORYMANAGER_CID);

// Set to true from NS_ShutdownXPCOM.
extern PRBool gXPCOMShuttingDown;

nsresult
nsCreateInstanceFromCategory::operator()(const nsIID& aIID, void** aInstancePtr) const
{
    /*
     * If I were a real man, I would consolidate this with
     * nsGetServiceFromContractID::operator().
     */
    nsresult rv;
    nsXPIDLCString value;
    nsCOMPtr<nsIComponentManager> compMgr;
    nsCOMPtr<nsICategoryManager> catman =
        do_GetService(kCategoryManagerCID, &rv);

    if (NS_FAILED(rv)) goto error;

    if (!mCategory || !mEntry) {
        // when categories have defaults, use that for null mEntry
        rv = NS_ERROR_NULL_POINTER;
        goto error;
    }

    /* find the contractID for category.entry */
    rv = catman->GetCategoryEntry(mCategory, mEntry,
                                  getter_Copies(value));
    if (NS_FAILED(rv)) goto error;
    if (!value) {
        rv = NS_ERROR_SERVICE_NOT_FOUND;
        goto error;
    }
    NS_GetComponentManager(getter_AddRefs(compMgr));
    if (!compMgr)
        return NS_ERROR_FAILURE;
    compMgr->CreateInstanceByContractID(value, 
                                        mOuter, 
                                        aIID, 
                                        aInstancePtr);
    if (NS_FAILED(rv)) {
    error:
        *aInstancePtr = 0;
    }

    *mErrorPtr = rv;
    return rv;
}


nsresult
nsGetServiceFromCategory::operator()(const nsIID& aIID, void** aInstancePtr) const
{
    nsresult rv;
    nsXPIDLCString value;
    nsCOMPtr<nsICategoryManager> catman = 
        do_GetService(kCategoryManagerCID, &rv);
    if (NS_FAILED(rv)) goto error;
    if (!mCategory || !mEntry) {
        // when categories have defaults, use that for null mEntry
        rv = NS_ERROR_NULL_POINTER;
        goto error;
    }
    /* find the contractID for category.entry */
    rv = catman->GetCategoryEntry(mCategory, mEntry,
                                  getter_Copies(value));
    if (NS_FAILED(rv)) goto error;
    if (!value) {
        rv = NS_ERROR_SERVICE_NOT_FOUND;
        goto error;
    }
    if (mServiceManager) {
        rv = mServiceManager->GetServiceByContractID(value, aIID, (void**)aInstancePtr);
    } else {
        nsCOMPtr<nsIServiceManager> mgr;
        NS_GetServiceManager(getter_AddRefs(mgr));
        if (mgr)
            rv = mgr->GetServiceByContractID(value, aIID, (void**)aInstancePtr);
    }
    if (NS_FAILED(rv)) {
    error:
        *aInstancePtr = 0;
    }
    *mErrorPtr = rv;
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// Arena helper functions
////////////////////////////////////////////////////////////////////////////////
static inline char *
ArenaStrdup(const char *s, PLArenaPool *arena)
{
    void *mem;
    // Include trailing null in the len
    PRInt32 len = strlen(s) + 1;
    PL_ARENA_ALLOCATE(mem, arena, len);
    if (mem)
        memcpy(mem, s, len);
    return NS_STATIC_CAST(char *, mem);
}

////////////////////////////////////////////////////////////////////////////////
// Hashtable Callbacks
////////////////////////////////////////////////////////////////////////////////

PRBool PR_CALLBACK
nsFactoryEntry_Destroy(nsHashKey *aKey, void *aData, void* closure);

PR_STATIC_CALLBACK(const void *)
factory_GetKey(PLDHashTable *aTable, PLDHashEntryHdr *aHdr)
{
    nsFactoryTableEntry* entry = NS_STATIC_CAST(nsFactoryTableEntry*, aHdr);

    return &entry->mFactoryEntry->cid;
}

PR_STATIC_CALLBACK(PLDHashNumber)
factory_HashKey(PLDHashTable *aTable, const void *aKey)
{
    const nsCID *cidp = NS_REINTERPRET_CAST(const nsCID*, aKey);

    return cidp->m0;
}

PR_STATIC_CALLBACK(PRBool)
factory_MatchEntry(PLDHashTable *aTable, const PLDHashEntryHdr *aHdr,
                   const void *aKey)
{
    const nsFactoryTableEntry* entry =
        NS_STATIC_CAST(const nsFactoryTableEntry*, aHdr);
    const nsCID *cidp = NS_REINTERPRET_CAST(const nsCID*, aKey);

    return (entry->mFactoryEntry->cid).Equals(*cidp);
}

PR_STATIC_CALLBACK(void)
factory_ClearEntry(PLDHashTable *aTable, PLDHashEntryHdr *aHdr)
{
    nsFactoryTableEntry* entry = NS_STATIC_CAST(nsFactoryTableEntry*, aHdr);
    // nsFactoryEntry is arena allocated. So we dont delete it.
    // We call the destructor by hand.
    entry->mFactoryEntry->~nsFactoryEntry();
    PL_DHashClearEntryStub(aTable, aHdr);
}

static PLDHashTableOps factory_DHashTableOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    factory_GetKey,
    factory_HashKey,
    factory_MatchEntry,
    PL_DHashMoveEntryStub,
    factory_ClearEntry,
    PL_DHashFinalizeStub,
};

PR_STATIC_CALLBACK(const void *)
contractID_GetKey(PLDHashTable *aTable, PLDHashEntryHdr *aHdr)
{
    nsContractIDTableEntry* entry = NS_STATIC_CAST(nsContractIDTableEntry*, aHdr);

    return entry->mContractID;
}

PR_STATIC_CALLBACK(PRBool)
contractID_MatchEntry(PLDHashTable *aTable, const PLDHashEntryHdr *aHdr,
                      const void *aKey)
{
    const nsContractIDTableEntry* entry =
        NS_STATIC_CAST(const nsContractIDTableEntry*, aHdr);

    const char* contractID = NS_REINTERPRET_CAST(const char*, aKey);

    return strcmp(entry->mContractID, contractID) == 0;
}

PR_STATIC_CALLBACK(void)
contractID_ClearEntry(PLDHashTable *aTable, PLDHashEntryHdr *aHdr)
{
    nsContractIDTableEntry* entry = NS_STATIC_CAST(nsContractIDTableEntry*, aHdr);
    if (entry->mFactoryEntry != kNonExistentContractID && 
        entry->mFactoryEntry->typeIndex == NS_COMPONENT_TYPE_SERVICE_ONLY && 
        entry->mFactoryEntry->cid.Equals(kEmptyCID)) {
        // this object is owned by the hash.
        // nsFactoryEntry is arena allocated. So we dont delete it.
        // We call the destructor by hand.
        entry->mFactoryEntry->~nsFactoryEntry();
    }

    // contractIDs are arena allocated. No need to free them.

    PL_DHashClearEntryStub(aTable, aHdr);
}

static PLDHashTableOps contractID_DHashTableOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    contractID_GetKey,
    PL_DHashStringKey,
    contractID_MatchEntry,
    PL_DHashMoveEntryStub,
    contractID_ClearEntry,
    PL_DHashFinalizeStub,
};

////////////////////////////////////////////////////////////////////////////////
// nsFactoryEntry
////////////////////////////////////////////////////////////////////////////////

MOZ_DECL_CTOR_COUNTER(nsFactoryEntry)
nsFactoryEntry::nsFactoryEntry(const nsCID &aClass, 
                               const char *aLocation,
                               int aType)
    : cid(aClass), typeIndex(aType)
{
    // Arena allocate the location string
    location = ArenaStrdup(aLocation, &nsComponentManagerImpl::gComponentManager->mArena);
}

nsFactoryEntry::nsFactoryEntry(const nsCID &aClass, nsIFactory *aFactory)
    : cid(aClass), typeIndex(NS_COMPONENT_TYPE_FACTORY_ONLY)
{
    factory = aFactory;
    location = nsnull;
}

// nsFactoryEntry is usually arena allocated including the strings it
// holds. So we call destructor by hand.
nsFactoryEntry::~nsFactoryEntry(void)
{
    // Release the reference to the factory
    factory = nsnull;

    // Release any service reference
    mServiceObject = nsnull;
}

nsresult
nsFactoryEntry::ReInit(const nsCID &aClass, const char *aLocation, int aType)
{
    NS_ENSURE_TRUE(typeIndex != NS_COMPONENT_TYPE_FACTORY_ONLY, NS_ERROR_INVALID_ARG);
    // cid has to match
    // SERVICE_ONLY entries can be promoted to an entry of another type
    NS_ENSURE_TRUE((typeIndex == NS_COMPONENT_TYPE_SERVICE_ONLY || cid.Equals(aClass)),
                   NS_ERROR_INVALID_ARG);

    // Arena allocate the location string
    location = ArenaStrdup(aLocation, &nsComponentManagerImpl::gComponentManager->mArena);

    typeIndex = aType;
    return NS_OK;
}

nsresult
nsFactoryEntry::ReInit(const nsCID &aClass, nsIFactory *aFactory)
{
    // cids has to match
    // SERVICE_ONLY entries can be promoted to an entry of another type
    NS_ENSURE_TRUE((typeIndex == NS_COMPONENT_TYPE_SERVICE_ONLY || cid.Equals(aClass)),
                   NS_ERROR_INVALID_ARG);
    // We are going to let native component be overridden by a factory.
    factory = aFactory;
    typeIndex = NS_COMPONENT_TYPE_FACTORY_ONLY;
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// Hashtable Enumeration
////////////////////////////////////////////////////////////////////////////////
typedef NS_CALLBACK(EnumeratorConverter)(PLDHashTable *table,
                                         const PLDHashEntryHdr *hdr,
                                         void *data,
                                         nsISupports **retval);

class PLDHashTableEnumeratorImpl : public nsIBidirectionalEnumerator, 
                                   public nsISimpleEnumerator
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIENUMERATOR
    NS_DECL_NSIBIDIRECTIONALENUMERATOR
    NS_DECL_NSISIMPLEENUMERATOR

    virtual ~PLDHashTableEnumeratorImpl();
    PLDHashTableEnumeratorImpl(PLDHashTable *table,
                               EnumeratorConverter converter,
                               void *converterData);
    PRInt32 Count() { return mCount; }
private:
    PLDHashTableEnumeratorImpl(); /* no implementation */
    NS_IMETHODIMP ReleaseElements();

    nsVoidArray   mElements;
    PRInt32       mCount, mCurrent;
    PRMonitor*    mMonitor;

    struct Closure {
        PRBool                        succeeded;
        EnumeratorConverter           converter;
        void                          *data;
        PLDHashTableEnumeratorImpl    *impl;
    };

    static PLDHashOperator PR_CALLBACK Enumerator(PLDHashTable *table,
                                                  PLDHashEntryHdr *hdr, 
                                                  PRUint32 number,
                                                  void *data);
};

// static
PLDHashOperator PR_CALLBACK
PLDHashTableEnumeratorImpl::Enumerator(PLDHashTable *table,
                                       PLDHashEntryHdr *hdr, 
                                       PRUint32 number,
                                       void *data)
{
    Closure *c = NS_REINTERPRET_CAST(Closure *, data);
    nsISupports *converted;
    if (NS_FAILED(c->converter(table, hdr, c->data, &converted)) ||
        !c->impl->mElements.AppendElement(converted)) {
        c->succeeded = PR_FALSE;
        return PL_DHASH_STOP;
    }

    c->succeeded = PR_TRUE;
    return PL_DHASH_NEXT;
}

PLDHashTableEnumeratorImpl::PLDHashTableEnumeratorImpl(PLDHashTable *table, 
                                                       EnumeratorConverter converter,
                                                       void *converterData) 
: mCurrent(0)
{
    mMonitor = nsAutoMonitor::NewMonitor("PLDHashTableEnumeratorImpl");
    NS_ASSERTION(mMonitor, "NULL Monitor");

    nsAutoMonitor mon(mMonitor);

    Closure c = { PR_FALSE, converter, converterData, this };
    mCount = PL_DHashTableEnumerate(table, Enumerator, &c);
    if (!c.succeeded) {
        ReleaseElements();
        mCount = 0;
    }
}

NS_IMPL_ISUPPORTS3(PLDHashTableEnumeratorImpl, 
                   nsIBidirectionalEnumerator,
                   nsIEnumerator,
                   nsISimpleEnumerator);

PLDHashTableEnumeratorImpl::~PLDHashTableEnumeratorImpl()
{
    (void) ReleaseElements();

    // Destroy the Lock
    if (mMonitor)
        nsAutoMonitor::DestroyMonitor(mMonitor);
}

NS_IMETHODIMP
PLDHashTableEnumeratorImpl::ReleaseElements()
{
    for (PRInt32 i = 0; i < mCount; i++) {
        nsISupports *supports = NS_REINTERPRET_CAST(nsISupports *,
                                                    mElements[i]);
        NS_IF_RELEASE(supports);
    }
    return NS_OK;
}

NS_IMETHODIMP
PL_NewDHashTableEnumerator(PLDHashTable *table,
                           EnumeratorConverter converter,
                           void *converterData, 
                           PLDHashTableEnumeratorImpl **retval)
{
    PLDHashTableEnumeratorImpl *impl =
        new PLDHashTableEnumeratorImpl(table, converter, converterData);

    if (!impl)
        return NS_ERROR_OUT_OF_MEMORY;

    if (impl->Count() == -1) {
        // conversion failed
        delete impl;
        return NS_ERROR_FAILURE;
    }

    NS_ADDREF(*retval = impl);
    return NS_OK;
}

NS_IMETHODIMP
PLDHashTableEnumeratorImpl::First()
{
    if (!mCount)
        return NS_ERROR_FAILURE;

    mCurrent = 0;
    return NS_OK;
}

NS_IMETHODIMP
PLDHashTableEnumeratorImpl::Last()
{
    if (!mCount)
        return NS_ERROR_FAILURE;
    mCurrent = mCount - 1;
    return NS_OK;
}

NS_IMETHODIMP
PLDHashTableEnumeratorImpl::Prev()
{
    if (!mCurrent)
        return NS_ERROR_FAILURE;

    mCurrent--;
    return NS_OK;
}

NS_IMETHODIMP
PLDHashTableEnumeratorImpl::Next()
{
    // If empty or we're past the end, or we are at the end return error
    if (!mCount || (mCurrent == mCount) || (++mCurrent == mCount))
        return NS_ERROR_FAILURE;

    return NS_OK;
}

NS_IMETHODIMP
PLDHashTableEnumeratorImpl::CurrentItem(nsISupports **retval)
{
    if (!mCount || mCurrent == mCount)
        return NS_ERROR_FAILURE;

    *retval = NS_REINTERPRET_CAST(nsISupports *, mElements[mCurrent]);
    if (*retval)
        NS_ADDREF(*retval);

    return NS_OK;
}

NS_IMETHODIMP
PLDHashTableEnumeratorImpl::IsDone()
{
    if (!mCount || (mCurrent == mCount))
        return NS_OK;

    return NS_ENUMERATOR_FALSE;
}

NS_IMETHODIMP 
PLDHashTableEnumeratorImpl::HasMoreElements(PRBool *_retval)
{
    if (!mCount || (mCurrent == mCount))
        *_retval = PR_FALSE;
    else
        *_retval = PR_TRUE;

    return NS_OK;
}

NS_IMETHODIMP 
PLDHashTableEnumeratorImpl::GetNext(nsISupports **_retval)
{
    nsresult rv = Next();
    if (NS_FAILED(rv)) return rv;

    return CurrentItem(_retval);
}

static NS_IMETHODIMP
ConvertFactoryEntryToCID(PLDHashTable *table,
                         const PLDHashEntryHdr *hdr,
                         void *data, nsISupports **retval)
{
    nsresult rv;
    nsCOMPtr<nsISupportsID> wrapper;

    nsComponentManagerImpl *cm = NS_STATIC_CAST(nsComponentManagerImpl *, data);

    rv = cm->CreateInstanceByContractID(NS_SUPPORTS_ID_CONTRACTID, nsnull,
           NS_GET_IID(nsISupportsID), getter_AddRefs(wrapper));

    NS_ENSURE_SUCCESS(rv, rv);

    const nsFactoryTableEntry *entry = 
        NS_REINTERPRET_CAST(const nsFactoryTableEntry *, hdr);
    if (entry) {
        nsFactoryEntry *fe = entry->mFactoryEntry;

        wrapper->SetData(&fe->cid);
        *retval = wrapper;
        NS_ADDREF(*retval);
        return NS_OK;
    }
    *retval = nsnull;

    return rv;
}

static NS_IMETHODIMP
ConvertContractIDKeyToString(PLDHashTable *table,
                             const PLDHashEntryHdr *hdr,
                             void *data, nsISupports **retval)
{
    nsresult rv;
    nsCOMPtr<nsISupportsCString> wrapper;

    nsComponentManagerImpl *cm = NS_STATIC_CAST(nsComponentManagerImpl *, data);

    rv = cm->CreateInstanceByContractID(NS_SUPPORTS_CSTRING_CONTRACTID, nsnull,
                NS_GET_IID(nsISupportsCString), getter_AddRefs(wrapper));

    NS_ENSURE_SUCCESS(rv, rv);

    const nsContractIDTableEntry *entry = 
        NS_REINTERPRET_CAST(const nsContractIDTableEntry *, hdr);

    wrapper->SetData(nsDependentCString(entry->mContractID));
    *retval = wrapper;
    NS_ADDREF(*retval);
    return NS_OK;    
}



////////////////////////////////////////////////////////////////////////////////
// nsComponentManagerImpl
////////////////////////////////////////////////////////////////////////////////


nsComponentManagerImpl::nsComponentManagerImpl()
    : 
    mMon(NULL), 
    mNativeComponentLoader(0),
    mStaticComponentLoader(0),
    mShuttingDown(NS_SHUTDOWN_NEVERHAPPENED), 
    mLoaderData(nsnull),
    mRegistryDirty(PR_FALSE)
{
    mFactories.ops = nsnull;
    mContractIDs.ops = nsnull;
}

nsresult nsComponentManagerImpl::Init(void) 
{
    PR_ASSERT(mShuttingDown != NS_SHUTDOWN_INPROGRESS);
    if (mShuttingDown == NS_SHUTDOWN_INPROGRESS)
        return NS_ERROR_FAILURE;

    mShuttingDown = NS_SHUTDOWN_NEVERHAPPENED;

    if (nsComponentManagerLog == nsnull)
    {
        nsComponentManagerLog = PR_NewLogModule("nsComponentManager");
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("xpcom-log-version : " NS_XPCOM_COMPONENT_MANAGER_VERSION_STRING));
    }

    // Initialize our arena
    PL_INIT_ARENA_POOL(&mArena, "ComponentManagerArena", NS_CM_BLOCK_SIZE);

    if (!mFactories.ops) {
        if (!PL_DHashTableInit(&mFactories, &factory_DHashTableOps,
                               0, sizeof(nsFactoryTableEntry),
                               1024)) {
            mFactories.ops = nsnull;
            return NS_ERROR_OUT_OF_MEMORY;
        }

        // Minimum alpha uses k=3 because nsFactoryTableEntry saves three
        // words compared to what a chained hash table requires.
        PL_DHashTableSetAlphaBounds(&mFactories,
                                    0.875,
                                    PL_DHASH_MIN_ALPHA(&mFactories, 3));
    }

    if (!mContractIDs.ops) {
        if (!PL_DHashTableInit(&mContractIDs, &contractID_DHashTableOps,
                               0, sizeof(nsContractIDTableEntry),
                               1024)) {
            mContractIDs.ops = nsnull;
            return NS_ERROR_OUT_OF_MEMORY;
        }

        // Minimum alpha uses k=2 because nsContractIDTableEntry saves two
        // words compared to what a chained hash table requires.
        PL_DHashTableSetAlphaBounds(&mContractIDs,
                                    0.875,
                                    PL_DHASH_MIN_ALPHA(&mContractIDs, 2));
    }
    if (mMon == nsnull) {
        mMon = nsAutoMonitor::NewMonitor("nsComponentManagerImpl");
        if (mMon == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    if (mNativeComponentLoader == nsnull) {
        /* Create the NativeComponentLoader */
        mNativeComponentLoader = new nsNativeComponentLoader();
        if (!mNativeComponentLoader)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(mNativeComponentLoader);

        nsresult rv = mNativeComponentLoader->Init(this, nsnull);
        if (NS_FAILED(rv))
            return rv;
    }

    // Add predefined loaders
    mLoaderData = (nsLoaderdata *) PR_Malloc(sizeof(nsLoaderdata) * NS_LOADER_DATA_ALLOC_STEP);
    if (!mLoaderData)
        return NS_ERROR_OUT_OF_MEMORY;
    mMaxNLoaderData = NS_LOADER_DATA_ALLOC_STEP;

    mNLoaderData = NS_COMPONENT_TYPE_NATIVE;
    mLoaderData[mNLoaderData].type = PL_strdup(nativeComponentType);
    mLoaderData[mNLoaderData].loader = mNativeComponentLoader;
    NS_ADDREF(mLoaderData[mNLoaderData].loader);
    mNLoaderData++;

#ifdef ENABLE_STATIC_COMPONENT_LOADER
    if (mStaticComponentLoader == nsnull) {
        extern nsresult NS_NewStaticComponentLoader(nsIComponentLoader **);
        NS_NewStaticComponentLoader(&mStaticComponentLoader);
        if (!mStaticComponentLoader)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    mLoaderData[mNLoaderData].type = PL_strdup(staticComponentType);
    mLoaderData[mNLoaderData].loader = mStaticComponentLoader;
    NS_ADDREF(mLoaderData[mNLoaderData].loader);
    mNLoaderData++;

    if (mStaticComponentLoader) {
        /* Init the static loader */
        mStaticComponentLoader->Init(this, nsnull);
    }
#endif

    nsCOMPtr<nsIProperties> directoryService;
    nsDirectoryService::Create(nsnull, 
                               NS_GET_IID(nsIProperties), 
                               getter_AddRefs(directoryService));  

    directoryService->Get(NS_XPCOM_COMPONENT_DIR, 
                          NS_GET_IID(nsIFile), 
                          getter_AddRefs(mComponentsDir));

    if (!mComponentsDir)
        return NS_ERROR_OUT_OF_MEMORY;

    nsCAutoString componentDescriptor;
    nsresult rv = mComponentsDir->GetNativePath(componentDescriptor);
    if (NS_FAILED(rv))
        return rv;

    mComponentsOffset = componentDescriptor.Length();

    NR_StartupRegistry();
    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
           ("nsComponentManager: Initialized."));

    return NS_OK;
}

nsresult nsComponentManagerImpl::Shutdown(void) 
{
    PR_ASSERT(mShuttingDown == NS_SHUTDOWN_NEVERHAPPENED);
    if (mShuttingDown != NS_SHUTDOWN_NEVERHAPPENED)
        return NS_ERROR_FAILURE;

    mShuttingDown = NS_SHUTDOWN_INPROGRESS;

    // Shutdown the component manager
    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, ("nsComponentManager: Beginning Shutdown."));

    PRInt32 i;

    // Write out our component data file.
    if (mRegistryDirty) {
        nsresult rv = WritePersistentRegistry();
        if (NS_FAILED(rv)) {
            PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, ("nsComponentManager: Could not write out perisistant registry."));
#ifdef DEBUG
            printf("Could not write out perisistant registry!\n");
#endif     
        }
    }    
    for (i = mAutoRegEntries.Count() - 1; i >= 0; i--) {
        AutoRegEntry* entry = NS_STATIC_CAST(AutoRegEntry*, mAutoRegEntries[i]);
        delete entry;
        mAutoRegEntries.RemoveElementAt(i);
    }

    // Release all cached factories
    if (mContractIDs.ops) {
        PL_DHashTableFinish(&mContractIDs);
        mContractIDs.ops = nsnull;
    }
    if (mFactories.ops) {
        PL_DHashTableFinish(&mFactories);
        mFactories.ops = nsnull;
    }
    // Unload libraries
    UnloadLibraries(nsnull, NS_Shutdown);

    // delete arena for strings and small objects
    PL_FinishArenaPool(&mArena);

    // This is were the nsFileSpec was deleted, so I am 
    // going to assign zero to 
    mComponentsDir = 0;

    mCategoryManager = 0;

    // Release all the component data - loaders and type strings
    for (i=0; i < mNLoaderData; i++) {
        NS_IF_RELEASE(mLoaderData[i].loader);
        PL_strfree((char *)mLoaderData[i].type);
    }
    PR_Free(mLoaderData);
    mLoaderData = nsnull;

    // we have an extra reference on this one, which is probably a good thing
    NS_IF_RELEASE(mNativeComponentLoader);
#ifdef ENABLE_STATIC_COMPONENT_LOADER
    NS_IF_RELEASE(mStaticComponentLoader);
#endif

    NR_ShutdownRegistry();

    mShuttingDown = NS_SHUTDOWN_COMPLETE;

    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, ("nsComponentManager: Shutdown complete."));

    return NS_OK;
}

nsComponentManagerImpl::~nsComponentManagerImpl()
{
    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, ("nsComponentManager: Beginning destruction."));

    if (mShuttingDown != NS_SHUTDOWN_COMPLETE)
        Shutdown();

    if (mMon) {
        nsAutoMonitor::DestroyMonitor(mMon);
    }
    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, ("nsComponentManager: Destroyed."));
}

NS_IMPL_THREADSAFE_ISUPPORTS8(nsComponentManagerImpl, 
                              nsIComponentManager, 
                              nsIServiceManager,
                              nsISupportsWeakReference, 
                              nsIInterfaceRequestor,
                              nsIComponentRegistrar,
                              nsIServiceManagerObsolete,
                              nsIComponentManagerObsolete,
                              nsIComponentLoaderManager)


nsresult
nsComponentManagerImpl::GetInterface(const nsIID & uuid, void **result)
{
    if (uuid.Equals(NS_GET_IID(nsIServiceManager)))
    {
        *result = NS_STATIC_CAST(nsIServiceManager*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }

    // fall through to QI as anything QIable is a superset of what canbe
    // got via the GetInterface()
    return  QueryInterface(uuid, result);
}

////////////////////////////////////////////////////////////////////////////////
// nsComponentManagerImpl: Platform methods
////////////////////////////////////////////////////////////////////////////////

#define PERSISTENT_REGISTRY_VERSION_MINOR 5
#define PERSISTENT_REGISTRY_VERSION_MAJOR 0


AutoRegEntry::AutoRegEntry(const char* name, PRInt64* modDate)
{
    mName = PL_strdup(name);
    mModDate = *modDate;
}
AutoRegEntry::~AutoRegEntry()
{
    if (mName) PL_strfree(mName);
}

PRBool 
AutoRegEntry::Modified(PRInt64 *date)
{
    return !LL_EQ(*date, mModDate);
}


static
PRBool ReadSectionHeader(nsManifestLineReader& reader, const char *token)
{
    while (1)
    {
        if (*reader.LinePtr() == '[')
        {
            char* p = reader.LinePtr() + (reader.LineLength() - 1);
            if (*p != ']')
                break;
            *p = 0;

            char* values[1];
            int lengths[1];
            if (2 != reader.ParseLine(values, lengths, 1))
                break;

            // ignore the leading '['
            if (0 != PL_strcmp(values[0]+1, token))
                break;

            return PR_TRUE;
        }

        if (!reader.NextLine())
            break;
    }
    return PR_FALSE;
}

nsresult
nsComponentManagerImpl::ReadPersistentRegistry()
{

    // populate Category Manager. need to get this early so that we don't get 
    // skipped by 'goto out'
    nsresult rv = GetService(kCategoryManagerCID, 
                             NS_GET_IID(nsICategoryManager), 
                             getter_AddRefs(mCategoryManager));    
    if (NS_FAILED(rv))
        return rv;

    nsAutoMonitor mon(mMon);
    nsManifestLineReader reader;

    if (!mComponentsDir)
        return NS_ERROR_FAILURE;  // this should have been set by Init().

    PRFileDesc* fd = nsnull;

    nsCOMPtr<nsIFile> componentReg;

    rv = mComponentsDir->Clone(getter_AddRefs(componentReg));
    if (NS_FAILED(rv))
        return rv;

    rv = componentReg->AppendNative(nsDependentCString(persistentRegistryFilename));
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(componentReg, &rv);
    if (NS_FAILED(rv))
        return rv;

    rv = localFile->OpenNSPRFileDesc(PR_RDONLY, 0444, &fd);
    if (NS_FAILED(rv))
        return rv;

    PRInt64 fileSize;
    rv = localFile->GetFileSize(&fileSize);
    if (NS_FAILED(rv))
    {
        PR_Close(fd);
        return rv;
    }

    PRInt32 flen = nsInt64(fileSize);
    if (flen == 0)
    {
        PR_Close(fd);
        NS_WARNING("Persistent Registry Empty!");
        return NS_OK; // ERROR CONDITION
    }

    char* registry = new char[flen+1];
    if (!registry)
        goto out;

    if (flen > PR_Read(fd, registry, flen))
    {
        rv = NS_ERROR_FAILURE;
        goto out;
    }
    registry[flen] = '\0';     

    reader.Init(registry, flen);

    if (ReadSectionHeader(reader, "HEADER"))
        goto out;

    if (!reader.NextLine())
        goto out;

    char* values[6];
    int lengths[6];

    // VersionLiteral,major,minor
    if (3 != reader.ParseLine(values, lengths, 3))
        goto out;

    // VersionLiteral
    if (0 != PL_strcmp(values[0], "Version"))
        goto out;

    // major
    if (PERSISTENT_REGISTRY_VERSION_MAJOR != atoi(values[1]))
        goto out;

    // minor
    if (PERSISTENT_REGISTRY_VERSION_MINOR != atoi(values[2]))
        goto out;

    if (ReadSectionHeader(reader, "COMPONENTS"))
        goto out;

    while (1)
    {
        if (!reader.NextLine())
            break;

        //name,last_modification_date
        if (2 != reader.ParseLine(values, lengths, 2))
            break;

        PRInt64 a = nsCRT::atoll(values[1]);
        AutoRegEntry *entry = new AutoRegEntry(values[0], &a);

        if (!entry)
            return NS_ERROR_OUT_OF_MEMORY;

        mAutoRegEntries.AppendElement(entry);
    }

    if (ReadSectionHeader(reader, "CLASSIDS"))
        goto out;

    while (1)
    {
        if (!reader.NextLine())
            break;

        // cid,contract_id,type,class_name,inproc_server
        if (5 != reader.ParseLine(values, lengths, 5))
            break;

        nsCID aClass;
        if (!aClass.Parse(values[0]))
            continue;

        int loadertype = GetLoaderType(values[2]);
        if (loadertype < 0) {
            loadertype = AddLoaderType(values[2]);
        }

        void *mem;
        PL_ARENA_ALLOCATE(mem, &mArena, sizeof(nsFactoryEntry));
        if (!mem)
            return NS_ERROR_OUT_OF_MEMORY;

        nsFactoryEntry *entry = new (mem) nsFactoryEntry(aClass, values[4], loadertype);

        nsFactoryTableEntry* factoryTableEntry =
            NS_STATIC_CAST(nsFactoryTableEntry*,
                           PL_DHashTableOperate(&mFactories, 
                                                &aClass,
                                                PL_DHASH_ADD));

        if (!factoryTableEntry)
            return NS_ERROR_OUT_OF_MEMORY;

        factoryTableEntry->mFactoryEntry = entry;

    }

    if (ReadSectionHeader(reader, "CONTRACTIDS"))
        goto out;

    while (1)
    {
        if (!reader.NextLine())
            break;

        //contractID,cid
        if (2 != reader.ParseLine(values, lengths, 2))
            break;

        nsCID aClass;
        if (!aClass.Parse(values[1]))
            continue;


        //need to find the location for this cid.
        nsFactoryEntry *cidEntry = GetFactoryEntry(aClass);
        if (!cidEntry || cidEntry->typeIndex < 0)
            continue; //what should we really do?

        nsContractIDTableEntry* contractIDTableEntry =
                NS_STATIC_CAST(nsContractIDTableEntry*,
                               PL_DHashTableOperate(&mContractIDs, 
                                                    values[0],
                                                    PL_DHASH_ADD));
        if (!contractIDTableEntry) {
            continue;
        }

        if (!contractIDTableEntry->mContractID)
            contractIDTableEntry->mContractID = ArenaStrdup(values[0], &mArena);

        contractIDTableEntry->mFactoryEntry = cidEntry;
    }

    if (ReadSectionHeader(reader, "CATEGORIES"))
        goto out;

    while (1)
    {
        if (!reader.NextLine())
            break;

        //type,name,value
        if (3 != reader.ParseLine(values, lengths, 3))
            break;

        mCategoryManager->AddCategoryEntry(values[0], 
                                           values[1], 
                                           values[2], 
                                           PR_TRUE, 
                                           PR_TRUE, 
                                           0);
    }

    mRegistryDirty = PR_FALSE;
out:
    if (fd)
        PR_Close(fd);

    if (registry)
        delete [] registry;

     return rv;
}

struct PersistentWriterArgs
{
    PRFileDesc *mFD;
    nsLoaderdata *mLoaderData;
};

PR_STATIC_CALLBACK(PLDHashOperator)
ContractIDWriter(PLDHashTable *table, 
                 PLDHashEntryHdr *hdr,
                 PRUint32 number, 
                 void *arg)
{
    char *contractID   = ((nsContractIDTableEntry*)hdr)->mContractID;
    nsFactoryEntry *factoryEntry = ((nsContractIDTableEntry*)hdr)->mFactoryEntry;
    if (factoryEntry == kNonExistentContractID || factoryEntry->typeIndex < 0)
        return PL_DHASH_NEXT;

    PRFileDesc* fd = ((PersistentWriterArgs*)arg)->mFD;

    char* cidString = factoryEntry->cid.ToString();
    if (cidString) {
        PR_fprintf(fd, "%s,%s\n", contractID, cidString); // what if this fails?
        PR_Free(cidString);
    }
    return PL_DHASH_NEXT;
}

PR_STATIC_CALLBACK(PLDHashOperator)
ClassIDWriter(PLDHashTable *table, 
              PLDHashEntryHdr *hdr,
              PRUint32 number, 
              void *arg)
{
    nsFactoryEntry *factoryEntry = ((nsFactoryTableEntry*)hdr)->mFactoryEntry;
    PRFileDesc* fd = ((PersistentWriterArgs*)arg)->mFD;
    nsLoaderdata *loaderData = ((PersistentWriterArgs*)arg)->mLoaderData;

    if (factoryEntry->typeIndex < 0)
        return PL_DHASH_NEXT;

    char* cidString = factoryEntry->cid.ToString();
    NS_ASSERTION(cidString, "null cidString!");
    if (!cidString) return PL_DHASH_NEXT;
    char *contractID = nsnull, *className = nsnull;

    nsCOMPtr<nsIClassInfo> classInfo = do_QueryInterface(factoryEntry->factory);
    if (classInfo)
    {
        classInfo->GetContractID(&contractID);
        classInfo->GetClassDescription(&className);
    }

    const char * loaderName = nsnull;
    if (factoryEntry->typeIndex)
        loaderName = loaderData[factoryEntry->typeIndex].type;

    char* location = factoryEntry->location;

    // cid,contract_id,type,class_name,inproc_server
    PR_fprintf(fd,
               "%s,%s,%s,%s,%s\n",
               cidString,
               (contractID ? contractID : ""),
               (loaderName ? loaderName : ""),
               (className  ? className  : ""), 
               (location   ? location   : "")); 

    PR_Free(cidString);
    if (contractID)
        PR_Free(contractID);
    if (className)
        PR_Free(className);

    return PL_DHASH_NEXT;
}

PR_STATIC_CALLBACK(PRBool)
AutoRegEntryWriter(void* aElement, void *aData)
{
    PRFileDesc* fd = (PRFileDesc*) aData;
    AutoRegEntry* entry = (AutoRegEntry*) aElement;

     //name,last_modification_date    
    PR_fprintf(fd,
               "%s,%lld\n",
               entry->GetName(),
               entry->GetDate());

    return PR_TRUE;
}

nsresult
nsComponentManagerImpl::WriteCategoryManagerToRegistry(PRFileDesc* fd)
{
    nsCOMPtr<nsICategoryManager> catMan;
    nsCOMPtr<nsISimpleEnumerator> outerEnum;
    nsCOMPtr<nsISimpleEnumerator> innerEnum;
    nsCOMPtr<nsISupports> supports;
    nsCOMPtr<nsISupportsCString> supStr;

    if (!mCategoryManager) {
        NS_WARNING("Could not access category manager.  Will not be able to save categories!");
        return NS_ERROR_UNEXPECTED;
    }

    nsresult rv = mCategoryManager->EnumerateCategories(getter_AddRefs(outerEnum));
    if (NS_FAILED(rv)) return rv;

    PRBool hasMore;
    while (NS_SUCCEEDED(outerEnum->HasMoreElements(&hasMore)) && hasMore) {

        if (NS_FAILED(outerEnum->GetNext(getter_AddRefs(supports))))
            continue;

       supStr = do_QueryInterface(supports);
        if (!supStr)
            continue;

        nsCAutoString categoryType;
        if (NS_FAILED(supStr->GetData(categoryType)))
            continue;

        rv = mCategoryManager->EnumerateCategory(categoryType.get(), getter_AddRefs(innerEnum));
        if (NS_FAILED(rv)) 
            continue;

        PRBool hasMore2;
        while (NS_SUCCEEDED(innerEnum->HasMoreElements(&hasMore2)) && hasMore2) {
            if (NS_FAILED(innerEnum->GetNext(getter_AddRefs(supports))))
                continue;

            supStr = do_QueryInterface(supports);
            if (!supStr)
                continue;

            nsCAutoString category;
            if (NS_FAILED(supStr->GetData(category)))
                continue;

            nsXPIDLCString value;
            rv = mCategoryManager->GetCategoryEntry(categoryType.get(), 
                                                    category.get(),
                                                    getter_Copies(value));

            if (NS_FAILED(rv)) continue;

            // categoryType, categoryName, value 
            PR_fprintf(fd,
                       "%s,%s,%s\n",
                       categoryType.get(),
                       category.get(),
                       value.get());
        }
    }
    return NS_OK;
}

nsresult
nsComponentManagerImpl::WritePersistentRegistry()
{
    if (!mComponentsDir)
        return NS_ERROR_FAILURE;  // this should have been set by Init().

    PRFileDesc* fd = nsnull;

    nsCOMPtr<nsIFile> componentReg;

    nsresult rv = mComponentsDir->Clone(getter_AddRefs(componentReg));       
    if (NS_FAILED(rv))
        return rv;

    rv = componentReg->AppendNative(nsDependentCString(persistentRegistryTempFilename));     
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(componentReg, &rv);
    if (NS_FAILED(rv))
        return rv;

    rv = localFile->OpenNSPRFileDesc(PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 0666, &fd);
    if (NS_FAILED(rv))
        return rv;

    if (!PR_fprintf(fd, "Generated File. Do not edit.\n"))
        goto out;

    if (!PR_fprintf(fd, "\n[HEADER]\nVersion,%d,%d\n",
                    PERSISTENT_REGISTRY_VERSION_MAJOR,
                    PERSISTENT_REGISTRY_VERSION_MINOR))
        goto out;

    if (!PR_fprintf(fd, "\n[COMPONENTS]\n"))
        goto out;

    mAutoRegEntries.EnumerateForwards(AutoRegEntryWriter, (void*)fd);

    PersistentWriterArgs args;
    args.mFD = fd;
    args.mLoaderData = mLoaderData;

    if (!PR_fprintf(fd, "\n[CLASSIDS]\n"))
        goto out;

    PL_DHashTableEnumerate(&mFactories, ClassIDWriter, (void*)&args);

    if (!PR_fprintf(fd, "\n[CONTRACTIDS]\n"))
        goto out;

    PL_DHashTableEnumerate(&mContractIDs, ContractIDWriter, (void*)&args);

    if (!PR_fprintf(fd, "\n[CATEGORIES]\n"))
        goto out;

    // slow slow slow slow....
    rv = WriteCategoryManagerToRegistry(fd);


out:
    if (fd)
        PR_Close(fd);

    // don't create the file is there was a problem????
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIFile> mainFile;
    rv = mComponentsDir->Clone(getter_AddRefs(mainFile));       
    if (NS_FAILED(rv))
        return rv;

    rv = mainFile->AppendNative(nsDependentCString(persistentRegistryFilename));     
    if (NS_FAILED(rv))
        return rv;

    PRBool exists;
    rv = mainFile->Exists(&exists);
    if (NS_FAILED(rv))
        return rv;

    if (exists) 
    {
        rv = mainFile->Remove(PR_FALSE);
        if (NS_FAILED(rv))
            return rv;
    }
    rv = componentReg->MoveToNative(nsnull, nsDependentCString(persistentRegistryFilename));
    mRegistryDirty = PR_FALSE;

    return rv;
}


////////////////////////////////////////////////////////////////////////////////
// Hash Functions
////////////////////////////////////////////////////////////////////////////////
nsresult 
nsComponentManagerImpl::HashContractID(const char *aContractID, const nsCID &aClass, nsFactoryEntry **pfe)
{
    nsIDKey cidKey(aClass);
    return HashContractID(aContractID, aClass, cidKey, pfe);
}


nsresult 
nsComponentManagerImpl::HashContractID(const char *aContractID, const nsCID &aClass, nsIDKey &cidKey, nsFactoryEntry **pfe)
{
    if (!aContractID)
    {
        return NS_ERROR_NULL_POINTER;
    }

    // Find the factory entry corresponding to the CID.
    nsFactoryEntry *entry = GetFactoryEntry(aClass, cidKey);
    if (!entry) {
        // Non existent. We use the special kNonExistentContractID to mark
        // that this contractid does not have a mapping.
        entry = kNonExistentContractID;
    }

    nsresult rv = HashContractID(aContractID, entry);
    if (NS_FAILED(rv))
        return rv;

    // Fill the entry out parameter
    if (pfe) *pfe = entry;
    return NS_OK;
}

nsresult 
nsComponentManagerImpl::HashContractID(const char *aContractID, nsFactoryEntry *fe)
{
    if (!aContractID)
        return NS_ERROR_NULL_POINTER;

    nsAutoMonitor mon(mMon);

    nsContractIDTableEntry* contractIDTableEntry =
        NS_STATIC_CAST(nsContractIDTableEntry*,
                       PL_DHashTableOperate(&mContractIDs, aContractID,
                                            PL_DHASH_ADD));
    if (!contractIDTableEntry)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ASSERTION(!contractIDTableEntry->mContractID || !strcmp(contractIDTableEntry->mContractID, aContractID), "contractid conflict");

    if (!contractIDTableEntry->mContractID)
        contractIDTableEntry->mContractID = ArenaStrdup(aContractID, &mArena);

    contractIDTableEntry->mFactoryEntry = fe;

    return NS_OK;
}

/**
 * LoadFactory()
 *
 * Given a FactoryEntry, this loads the dll if it has to, find the NSGetFactory
 * symbol, calls the routine to create a new factory and returns it to the
 * caller.
 *
 * No attempt is made to store the factory in any form anywhere.
 */
nsresult
nsComponentManagerImpl::LoadFactory(nsFactoryEntry *aEntry,
                                    nsIFactory **aFactory)
{

    if (!aFactory)
        return NS_ERROR_NULL_POINTER;
    *aFactory = nsnull;

    nsresult rv;
    rv = aEntry->GetFactory(aFactory, this);
    if (NS_FAILED(rv)) {
        PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
               ("nsComponentManager: FAILED to load factory from %s (%s)\n",
                (const char *)aEntry->location, mLoaderData[aEntry->typeIndex].type));
        return rv;
    }

    return NS_OK;
}

nsFactoryEntry *
nsComponentManagerImpl::GetFactoryEntry(const char *aContractID)
{
    nsFactoryEntry *fe = nsnull;
    {

        nsAutoMonitor mon(mMon);

        nsContractIDTableEntry* contractIDTableEntry =
            NS_STATIC_CAST(nsContractIDTableEntry*,
                           PL_DHashTableOperate(&mContractIDs, aContractID,
                                                PL_DHASH_LOOKUP));


        if (PL_DHASH_ENTRY_IS_BUSY(contractIDTableEntry)) {
            fe = contractIDTableEntry->mFactoryEntry;
        }
    }   //exit monitor

    // If no mapping found, add a special non-existent mapping
    // so the next time around, we dont have to waste time doing the
    // same mapping over and over again
    if (!fe) {
        fe = kNonExistentContractID;
        HashContractID(aContractID, fe);
    }

    return fe;
}


nsFactoryEntry *
nsComponentManagerImpl::GetFactoryEntry(const nsCID &aClass)
{
    nsIDKey cidKey(aClass);
    return GetFactoryEntry(aClass, cidKey);
}


nsFactoryEntry *
nsComponentManagerImpl::GetFactoryEntry(const nsCID &aClass, nsIDKey &cidKey)
{
    nsFactoryEntry *entry = nsnull;
    {
        nsAutoMonitor mon(mMon);

        nsFactoryTableEntry* factoryTableEntry =
            NS_STATIC_CAST(nsFactoryTableEntry*,
                           PL_DHashTableOperate(&mFactories, &aClass,
                                                PL_DHASH_LOOKUP));

        if (PL_DHASH_ENTRY_IS_BUSY(factoryTableEntry)) {
            entry = factoryTableEntry->mFactoryEntry;
        }
    }   // exit monitor

    return entry;
}


/**
 * FindFactory()
 *
 * Given a classID, this finds the factory for this CID by first searching the
 * local CID<->factory mapping. Next it searches for a Dll that implements
 * this classID and calls LoadFactory() to create the factory.
 *
 * Again, no attempt is made at storing the factory.
 */
nsresult
nsComponentManagerImpl::FindFactory(const nsCID &aClass,
                                    nsIFactory **aFactory) 
{
    PR_ASSERT(aFactory != nsnull);

    nsFactoryEntry *entry = GetFactoryEntry(aClass);

    if (!entry)
        return NS_ERROR_FACTORY_NOT_REGISTERED;

    return entry->GetFactory(aFactory, this);
}


nsresult
nsComponentManagerImpl::FindFactory(const char *contractID,
                                    nsIFactory **aFactory) 
{
    PR_ASSERT(aFactory != nsnull);

    nsFactoryEntry *entry = GetFactoryEntry(contractID);

    if (!entry || entry == kNonExistentContractID)
        return NS_ERROR_FACTORY_NOT_REGISTERED;

    return entry->GetFactory(aFactory, this);
}

/**
 * GetClassObject()
 *
 * Given a classID, this finds the singleton ClassObject that implements the CID.
 * Returns an interface of type aIID off the singleton classobject.
 */
nsresult
nsComponentManagerImpl::GetClassObject(const nsCID &aClass, const nsIID &aIID,
                                       void **aResult) 
{
    nsresult rv;

    nsCOMPtr<nsIFactory> factory;

    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS))
    {
        char *buf = aClass.ToString();
        PR_LogPrint("nsComponentManager: GetClassObject(%s)", buf);
        if (buf)
            PR_Free(buf);
    }

    PR_ASSERT(aResult != nsnull);

    rv = FindFactory(aClass, getter_AddRefs(factory));
    if (NS_FAILED(rv)) return rv;

    rv = factory->QueryInterface(aIID, aResult);

    PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
           ("\t\tGetClassObject() %s", NS_SUCCEEDED(rv) ? "succeeded" : "FAILED"));

    return rv;
}


nsresult
nsComponentManagerImpl::GetClassObjectByContractID(const char *contractID, 
                                                   const nsIID &aIID,
                                                   void **aResult) 
{
    nsresult rv;

    nsCOMPtr<nsIFactory> factory;

    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS))
    {
        PR_LogPrint("nsComponentManager: GetClassObject(%s)", contractID);
    }

    PR_ASSERT(aResult != nsnull);

    rv = FindFactory(contractID, getter_AddRefs(factory));
    if (NS_FAILED(rv)) return rv;

    rv = factory->QueryInterface(aIID, aResult);

    PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
           ("\t\tGetClassObject() %s", NS_SUCCEEDED(rv) ? "succeeded" : "FAILED"));

    return rv;
}

/**
 * ContractIDToClassID()
 *
 * Mapping function from a ContractID to a classID. Directly talks to the registry.
 *
 */
nsresult
nsComponentManagerImpl::ContractIDToClassID(const char *aContractID, nsCID *aClass)
{
    NS_PRECONDITION(aContractID != nsnull, "null ptr");
    if (!aContractID)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aClass != nsnull, "null ptr");
    if (!aClass)
        return NS_ERROR_NULL_POINTER;

    nsresult rv = NS_ERROR_FACTORY_NOT_REGISTERED;

    nsFactoryEntry *fe = GetFactoryEntry(aContractID);
    if (fe && fe != kNonExistentContractID) {
        *aClass = fe->cid;
        rv = NS_OK;
    }

    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS)) {
        char *buf = 0;
        if (NS_SUCCEEDED(rv))
            buf = aClass->ToString();
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("nsComponentManager: ContractIDToClassID(%s)->%s", aContractID,
                NS_SUCCEEDED(rv) ? buf : "[FAILED]"));
        if (buf)
            PR_Free(buf);
    }

    return rv;
}

/**
 * CLSIDToContractID()
 *
 * Translates a classID to a {ContractID, Class Name}. Does direct registry
 * access to do the translation.
 *
 * NOTE: Since this isn't heavily used, we arent caching this.
 */
nsresult
nsComponentManagerImpl::CLSIDToContractID(const nsCID &aClass,
                                          char* *aClassName,
                                          char* *aContractID)
{
    NS_WARNING("Need to implement CLSIDToContractID");

    nsresult rv = NS_ERROR_FACTORY_NOT_REGISTERED;

    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS))
    {
        char *buf = aClass.ToString();
        PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
               ("nsComponentManager: CLSIDToContractID(%s)->%s", buf,
                NS_SUCCEEDED(rv) ? *aContractID : "[FAILED]"));
        if (buf)
            PR_Free(buf);
    }

    return rv;
}

/**
 * CreateInstance()
 *
 * Create an instance of an object that implements an interface and belongs
 * to the implementation aClass using the factory. The factory is immediately
 * released and not held onto for any longer.
 */
nsresult 
nsComponentManagerImpl::CreateInstance(const nsCID &aClass, 
                                       nsISupports *aDelegate,
                                       const nsIID &aIID,
                                       void **aResult)
{
    // test this first, since there's no point in creating a component during
    // shutdown -- whether it's available or not would depend on the order it
    // occurs in the list
    if (gXPCOMShuttingDown) {
        // When processing shutdown, dont process new GetService() requests
#ifdef SHOW_DENIED_ON_SHUTDOWN
        nsXPIDLCString cid, iid;
        cid.Adopt(aClass.ToString());
        iid.Adopt(aIID.ToString());
        fprintf(stderr, "Creating new instance on shutdown. Denied.\n"
               "         CID: %s\n         IID: %s\n", cid.get(), iid.get());
#endif /* SHOW_DENIED_ON_SHUTDOWN */
        return NS_ERROR_UNEXPECTED;
    }

    if (aResult == nsnull)
    {
        return NS_ERROR_NULL_POINTER;
    }
    *aResult = nsnull;

    nsIFactory *factory = nsnull;
    nsresult rv = FindFactory(aClass, &factory);
    if (NS_SUCCEEDED(rv))
    {
        rv = factory->CreateInstance(aDelegate, aIID, aResult);
        NS_RELEASE(factory);
    }
    else
    {
        // Translate error values
        rv = NS_ERROR_FACTORY_NOT_REGISTERED;
    }

    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS)) 
    {
        char *buf = aClass.ToString();
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("nsComponentManager: CreateInstance(%s) %s", buf,
                NS_SUCCEEDED(rv) ? "succeeded" : "FAILED"));
        if (buf)
            PR_Free(buf);
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
nsresult
nsComponentManagerImpl::CreateInstanceByContractID(const char *aContractID,
                                               nsISupports *aDelegate,
                                               const nsIID &aIID,
                                               void **aResult)
{
    // test this first, since there's no point in creating a component during
    // shutdown -- whether it's available or not would depend on the order it
    // occurs in the list
    if (gXPCOMShuttingDown) {
        // When processing shutdown, dont process new GetService() requests
#ifdef SHOW_DENIED_ON_SHUTDOWN
        nsXPIDLCString iid;
        iid.Adopt(aIID.ToString());
        fprintf(stderr, "Creating new instance on shutdown. Denied.\n"
               "  ContractID: %s\n         IID: %s\n", aContractID, iid.get());
#endif /* SHOW_DENIED_ON_SHUTDOWN */
        return NS_ERROR_UNEXPECTED;
    }

    if (aResult == nsnull)
    {
        return NS_ERROR_NULL_POINTER;
    }
    *aResult = nsnull;

    nsIFactory *factory = nsnull;
    nsresult rv = FindFactory(aContractID, &factory);
    if (NS_SUCCEEDED(rv))
    {
        rv = factory->CreateInstance(aDelegate, aIID, aResult);
        NS_RELEASE(factory);
    }
    else
    {
        // Translate error values
        rv = NS_ERROR_FACTORY_NOT_REGISTERED;
    }

    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
           ("nsComponentManager: CreateInstanceByContractID(%s) %s", aContractID,
            NS_SUCCEEDED(rv) ? "succeeded" : "FAILED"));

    return rv;
}

// Service Manager Impl
static
PLDHashOperator PR_CALLBACK
FreeServiceFactoryEntryEnumerate(PLDHashTable *aTable,
                                 PLDHashEntryHdr *aHdr,
                                 PRUint32 aNumber,
                                 void *aData)
{
    nsFactoryTableEntry* entry = NS_STATIC_CAST(nsFactoryTableEntry*, aHdr);

    if (!entry->mFactoryEntry || entry->mFactoryEntry == kNonExistentContractID) 
        return PL_DHASH_NEXT;

    nsFactoryEntry* factoryEntry = entry->mFactoryEntry;
    factoryEntry->mServiceObject = nsnull;
    return PL_DHASH_NEXT;
}

static
PLDHashOperator PR_CALLBACK
FreeServiceContractIDEntryEnumerate(PLDHashTable *aTable,
                                    PLDHashEntryHdr *aHdr,
                                    PRUint32 aNumber,
                                    void *aData)
{
    nsContractIDTableEntry* entry = NS_STATIC_CAST(nsContractIDTableEntry*, aHdr);

    if (!entry->mFactoryEntry || entry->mFactoryEntry == kNonExistentContractID) 
        return PL_DHASH_NEXT;

    nsFactoryEntry* factoryEntry = entry->mFactoryEntry;
    factoryEntry->mServiceObject = nsnull;
    return PL_DHASH_NEXT;
}

nsresult 
nsComponentManagerImpl::FreeServices()
{
    NS_ASSERTION(gXPCOMShuttingDown, "Must be shutting down in order to free all services");

    if (!gXPCOMShuttingDown)
        return NS_ERROR_FAILURE;

    if (mContractIDs.ops) {
        PL_DHashTableEnumerate(&mContractIDs, FreeServiceContractIDEntryEnumerate, nsnull);
    }


    if (mFactories.ops) {
        PL_DHashTableEnumerate(&mFactories, FreeServiceFactoryEntryEnumerate, nsnull);
    }

    return NS_OK;
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
        // When processing shutdown, dont process new GetService() requests
#ifdef SHOW_DENIED_ON_SHUTDOWN
        nsXPIDLCString cid, iid;
        cid.Adopt(aClass.ToString());
        iid.Adopt(aIID.ToString());
        fprintf(stderr, "Getting service on shutdown. Denied.\n"
               "         CID: %s\n         IID: %s\n", cid.get(), iid.get());
#endif /* SHOW_DENIED_ON_SHUTDOWN */
        return NS_ERROR_UNEXPECTED;
    }

    nsAutoMonitor mon(mMon);

    nsresult rv = NS_OK;
    nsIDKey key(aClass);
    nsFactoryEntry* entry = nsnull;
    nsFactoryTableEntry* factoryTableEntry =
        NS_STATIC_CAST(nsFactoryTableEntry*,
                       PL_DHashTableOperate(&mFactories, &aClass,
                                            PL_DHASH_LOOKUP));

    if (PL_DHASH_ENTRY_IS_BUSY(factoryTableEntry)) {
        entry = factoryTableEntry->mFactoryEntry;
    }

    if (entry && entry->mServiceObject) {
         return entry->mServiceObject->QueryInterface(aIID, result);
    }

    nsCOMPtr<nsISupports> service;
    // We need to not be holding the service manager's monitor while calling 
    // CreateInstance, because it invokes user code which could try to re-enter
    // the service manager:
    mon.Exit();

    rv = CreateInstance(aClass, nsnull, aIID, getter_AddRefs(service));

    mon.Enter();

    if (NS_FAILED(rv))
        return rv;

    if (!entry) { // second hash lookup for GetService
        nsFactoryTableEntry* factoryTableEntry =
            NS_STATIC_CAST(nsFactoryTableEntry*,
                           PL_DHashTableOperate(&mFactories, &aClass,
                                                PL_DHASH_LOOKUP));
        if (PL_DHASH_ENTRY_IS_BUSY(factoryTableEntry)) {
            entry = factoryTableEntry->mFactoryEntry;
        }
        NS_ASSERTION(entry, "we should have a factory entry since CI succeeded - we should not get here");
        if (!entry) return NS_ERROR_FAILURE; 
    }

    entry->mServiceObject = service; 
    *result = service.get();
    NS_ADDREF(NS_STATIC_CAST(nsISupports*, (*result))); 
    return rv;
}

NS_IMETHODIMP
nsComponentManagerImpl::RegisterService(const nsCID& aClass, nsISupports* aService)
{
    nsAutoMonitor mon(mMon);

    // check to see if we have a factory entry for the service
    nsIDKey key(aClass);
    nsFactoryEntry *entry = GetFactoryEntry(aClass, key);

    if (!entry) { // XXXdougt - should we require that all services register factories??  probably not.
        void *mem;
        PL_ARENA_ALLOCATE(mem, &mArena, sizeof(nsFactoryEntry));
        if (!mem)
            return NS_ERROR_OUT_OF_MEMORY;
        entry = new (mem) nsFactoryEntry(aClass, nsnull);

        entry->typeIndex = NS_COMPONENT_TYPE_SERVICE_ONLY;
        nsFactoryTableEntry* factoryTableEntry =
            NS_STATIC_CAST(nsFactoryTableEntry*,
                           PL_DHashTableOperate(&mFactories, &aClass,
                                                PL_DHASH_ADD));
        if (!factoryTableEntry)
            return NS_ERROR_OUT_OF_MEMORY;

        factoryTableEntry->mFactoryEntry = entry;
    }
    else {
        if (entry->mServiceObject)
            return NS_ERROR_FAILURE;
    }

    entry->mServiceObject = aService;
    return NS_OK;
}

NS_IMETHODIMP
nsComponentManagerImpl::UnregisterService(const nsCID& aClass)
{
    nsresult rv = NS_OK;

    nsFactoryEntry* entry = nsnull;

    nsAutoMonitor mon(mMon);

    nsFactoryTableEntry* factoryTableEntry =
        NS_STATIC_CAST(nsFactoryTableEntry*,
                       PL_DHashTableOperate(&mFactories, &aClass,
                                            PL_DHASH_LOOKUP));

    if (PL_DHASH_ENTRY_IS_BUSY(factoryTableEntry)) {
        entry = factoryTableEntry->mFactoryEntry;
    }

    if (!entry || !entry->mServiceObject)
        return NS_ERROR_SERVICE_NOT_FOUND;

    entry->mServiceObject = nsnull;
    return rv;
}

NS_IMETHODIMP
nsComponentManagerImpl::RegisterService(const char* aContractID, nsISupports* aService)
{

    nsAutoMonitor mon(mMon);

    // check to see if we have a factory entry for the service
    nsFactoryEntry *entry = GetFactoryEntry(aContractID);

    if (entry == kNonExistentContractID)
        entry = nsnull;

    if (!entry) { // XXXdougt - should we require that all services register factories??  probably not.
        void *mem;
        PL_ARENA_ALLOCATE(mem, &mArena, sizeof(nsFactoryEntry));
        if (!mem)
            return NS_ERROR_OUT_OF_MEMORY;
        entry = new (mem) nsFactoryEntry(kEmptyCID, nsnull);

        entry->typeIndex = NS_COMPONENT_TYPE_SERVICE_ONLY;

        nsContractIDTableEntry* contractIDTableEntry =
            NS_STATIC_CAST(nsContractIDTableEntry*,
                           PL_DHashTableOperate(&mContractIDs, aContractID,
                                                PL_DHASH_ADD));
        if (!contractIDTableEntry) {
            delete entry;
            return NS_ERROR_OUT_OF_MEMORY;
        }

        if (!contractIDTableEntry->mContractID)
            contractIDTableEntry->mContractID = ArenaStrdup(aContractID, &mArena);

        contractIDTableEntry->mFactoryEntry = entry;
    }
    else {
        if (entry->mServiceObject)
            return NS_ERROR_FAILURE;
    }

    entry->mServiceObject = aService;
    return NS_OK;
}


NS_IMETHODIMP 
nsComponentManagerImpl::IsServiceInstantiated(const nsCID & aClass,
                                              const nsIID& aIID,
                                              PRBool *result)
{
    // Now we want to get the service if we already got it. If not, we dont want
    // to create an instance of it. mmh!

    // test this first, since there's no point in returning a service during
    // shutdown -- whether it's available or not would depend on the order it
    // occurs in the list
    if (gXPCOMShuttingDown) {
        // When processing shutdown, dont process new GetService() requests
#ifdef SHOW_DENIED_ON_SHUTDOWN
        nsXPIDLCString cid, iid;
        cid.Adopt(aClass.ToString());
        iid.Adopt(aIID.ToString());
        fprintf(stderr, "Checking for service on shutdown. Denied.\n"
               "         CID: %s\n         IID: %s\n", cid.get(), iid.get());
#endif /* SHOW_DENIED_ON_SHUTDOWN */
        return NS_ERROR_UNEXPECTED;
    }

    nsresult rv = NS_ERROR_SERVICE_NOT_FOUND;
    nsFactoryEntry* entry = nsnull;
    nsFactoryTableEntry* factoryTableEntry =
        NS_STATIC_CAST(nsFactoryTableEntry*,
                       PL_DHashTableOperate(&mFactories, &aClass,
                                            PL_DHASH_LOOKUP));

    if (PL_DHASH_ENTRY_IS_BUSY(factoryTableEntry)) {
        entry = factoryTableEntry->mFactoryEntry;
    }

    if (entry && entry->mServiceObject) {
        nsCOMPtr<nsISupports> service;
        rv = entry->mServiceObject->QueryInterface(aIID, getter_AddRefs(service));
        *result = (service!=nsnull);
    }
    return rv;

}

NS_IMETHODIMP nsComponentManagerImpl::IsServiceInstantiatedByContractID(const char *aContractID, 
                                                                        const nsIID& aIID,
                                                                        PRBool *result)
{
    // Now we want to get the service if we already got it. If not, we dont want
    // to create an instance of it. mmh!

    // test this first, since there's no point in returning a service during
    // shutdown -- whether it's available or not would depend on the order it
    // occurs in the list
    if (gXPCOMShuttingDown) {
        // When processing shutdown, dont process new GetService() requests
#ifdef SHOW_DENIED_ON_SHUTDOWN
        nsXPIDLCString iid;
        iid.Adopt(aIID.ToString());
        fprintf(stderr, "Checking for service on shutdown. Denied.\n"
               "  ContractID: %s\n         IID: %s\n", aContractID, iid.get());
#endif /* SHOW_DENIED_ON_SHUTDOWN */
        return NS_ERROR_UNEXPECTED;
    }

    nsresult rv = NS_ERROR_SERVICE_NOT_FOUND;
    nsFactoryEntry *entry = nsnull;
    {
        nsAutoMonitor mon(mMon);

        nsContractIDTableEntry* contractIDTableEntry =
            NS_STATIC_CAST(nsContractIDTableEntry*,
                           PL_DHashTableOperate(&mContractIDs, aContractID,
                                                PL_DHASH_LOOKUP));

        if (PL_DHASH_ENTRY_IS_BUSY(contractIDTableEntry)) {
            entry = contractIDTableEntry->mFactoryEntry;
        }
    }   // exit monitor

    if (entry && entry != kNonExistentContractID && entry->mServiceObject) {
        nsCOMPtr<nsISupports> service;
        rv = entry->mServiceObject->QueryInterface(aIID, getter_AddRefs(service));
        *result = (service!=nsnull);
    }
    return rv;
}


NS_IMETHODIMP
nsComponentManagerImpl::UnregisterService(const char* aContractID)
{
    nsresult rv = NS_OK;

    nsAutoMonitor mon(mMon);

    nsFactoryEntry *entry = nsnull;
    nsContractIDTableEntry* contractIDTableEntry =
       NS_STATIC_CAST(nsContractIDTableEntry*,
                      PL_DHashTableOperate(&mContractIDs, aContractID,
                                           PL_DHASH_LOOKUP));

   if (PL_DHASH_ENTRY_IS_BUSY(contractIDTableEntry)) {
       entry = contractIDTableEntry->mFactoryEntry;
   }

   if (entry == nsnull || entry == kNonExistentContractID || entry->mServiceObject == nsnull)
        return NS_ERROR_SERVICE_NOT_FOUND;

   entry->mServiceObject = nsnull;
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
        // When processing shutdown, dont process new GetService() requests
#ifdef SHOW_DENIED_ON_SHUTDOWN
        nsXPIDLCString iid;
        iid.Adopt(aIID.ToString());
        fprintf(stderr, "Getting service on shutdown. Denied.\n"
               "  ContractID: %s\n         IID: %s\n", aContractID, iid.get());
#endif /* SHOW_DENIED_ON_SHUTDOWN */
        return NS_ERROR_UNEXPECTED;
    }

    nsAutoMonitor mon(mMon);

    nsresult rv = NS_OK;
    nsFactoryEntry *entry = nsnull;
    nsContractIDTableEntry* contractIDTableEntry =
        NS_STATIC_CAST(nsContractIDTableEntry*,
                       PL_DHashTableOperate(&mContractIDs, aContractID,
                                            PL_DHASH_LOOKUP));

    if (PL_DHASH_ENTRY_IS_BUSY(contractIDTableEntry)) {
        entry = contractIDTableEntry->mFactoryEntry;
    }

    if (entry && entry != kNonExistentContractID && entry->mServiceObject) {
        return entry->mServiceObject->QueryInterface(aIID, result);
    }

    nsCOMPtr<nsISupports> service;
    // We need to not be holding the service manager's monitor while calling 
    // CreateInstance, because it invokes user code which could try to re-enter
    // the service manager:
    mon.Exit();

    rv = CreateInstanceByContractID(aContractID, nsnull, aIID, getter_AddRefs(service));

    mon.Enter();

    if (NS_FAILED(rv))
        return rv;

    if (!entry) { // second hash lookup for GetService
        nsContractIDTableEntry* contractIDTableEntry =
            NS_STATIC_CAST(nsContractIDTableEntry*,
                           PL_DHashTableOperate(&mContractIDs, aContractID,
                                                PL_DHASH_LOOKUP));

        if (PL_DHASH_ENTRY_IS_BUSY(contractIDTableEntry)) {
            entry = contractIDTableEntry->mFactoryEntry;
        }
        NS_ASSERTION(entry, "we should have a factory entry since CI succeeded - we should not get here");
        if (!entry) return NS_ERROR_FAILURE; 
    }

    entry->mServiceObject = service;
    *result = service.get();
    NS_ADDREF(NS_STATIC_CAST(nsISupports*, *result));
    return rv;
}

NS_IMETHODIMP 
nsComponentManagerImpl::GetService(const nsCID& aClass, const nsIID& aIID,
                                     nsISupports* *result,
                                     nsIShutdownListener* shutdownListener)
{
    return GetService(aClass, aIID, (void**)result);
};

NS_IMETHODIMP 
nsComponentManagerImpl::GetService(const char* aContractID, const nsIID& aIID,
                                     nsISupports* *result,
                                     nsIShutdownListener* shutdownListener)
{
    return GetServiceByContractID(aContractID, aIID, (void**)result);
};


NS_IMETHODIMP 
nsComponentManagerImpl::ReleaseService(const nsCID& aClass, nsISupports* service,
                                         nsIShutdownListener* shutdownListener)
{
    NS_IF_RELEASE(service);
    return NS_OK;
};

NS_IMETHODIMP 
nsComponentManagerImpl::ReleaseService(const char* aContractID, nsISupports* service,
                                       nsIShutdownListener* shutdownListener)
{
    NS_IF_RELEASE(service);
    return NS_OK;
};

/*
 * I want an efficient way to allocate a buffer to the right size
 * and stick the prefix and dllName in, then be able to hand that buffer
 * off to the FactoryEntry.  Is that so wrong?
 *
 * *regName is allocated on success.
 *
 * This should live in nsNativeComponentLoader.cpp, I think.
 */
static nsresult
MakeRegistryName(const char *aDllName, const char *prefix, char **regName)
{
    char *registryName;

    PRUint32 len = strlen(prefix);

    PRUint32 registryNameLen = strlen(aDllName) + len;
    registryName = (char *)nsMemory::Alloc(registryNameLen + 1);

    // from here on it, we want len sans terminating NUL

    if (!registryName)
        return NS_ERROR_OUT_OF_MEMORY;

    memcpy(registryName, prefix, len);
    strcpy(registryName + len, aDllName); 
    registryName[registryNameLen] = '\0';
    *regName = registryName;

#ifdef DEBUG_shaver_off
    fprintf(stderr, "MakeRegistryName(%s, %s, &[%s])\n",
            aDllName, prefix, *regName);
#endif

    return NS_OK;
}

nsresult
nsComponentManagerImpl::RegistryNameForLib(const char *aLibName,
                                           char **aRegistryName)
{
    return MakeRegistryName(aLibName, XPCOM_LIB_PREFIX, aRegistryName);
}

nsresult
nsComponentManagerImpl::RegistryLocationForSpec(nsIFile *aSpec,
                                                char **aRegistryName)
{
    nsresult rv;

    if (!mComponentsDir) 
        return NS_ERROR_NOT_INITIALIZED;

    if (!aSpec) {
        *aRegistryName = PL_strdup("");
        return NS_OK;
    }

    PRBool containedIn;
    mComponentsDir->Contains(aSpec, PR_TRUE, &containedIn);

    nsCAutoString persistentDescriptor;

    if (containedIn){
        rv = aSpec->GetNativePath(persistentDescriptor);
        if (NS_FAILED(rv))
            return rv;

        const char* relativeLocation = persistentDescriptor.get() + mComponentsOffset + 1;

        rv = MakeRegistryName(relativeLocation, XPCOM_RELCOMPONENT_PREFIX, 
                              aRegistryName);
    } else {
        /* absolute names include volume info on Mac, so persistent descriptor */
        rv = aSpec->GetNativePath(persistentDescriptor);
        if (NS_FAILED(rv))
            return rv;
        rv = MakeRegistryName(persistentDescriptor.get(), XPCOM_ABSCOMPONENT_PREFIX,
                              aRegistryName);
    }

    return rv;
}

nsresult
nsComponentManagerImpl::SpecForRegistryLocation(const char *aLocation,
                                                nsIFile **aSpec)
{
    // i18n: assuming aLocation is encoded for the current locale

    nsresult rv;
    if (!aLocation || !aSpec)
        return NS_ERROR_NULL_POINTER;

    /* abs:/full/path/to/libcomponent.so */
    if (!strncmp(aLocation, XPCOM_ABSCOMPONENT_PREFIX, 4)) {

        nsLocalFile* file = new nsLocalFile;
        if (!file) return NS_ERROR_FAILURE;

        rv = file->InitWithNativePath(nsDependentCString((char *)aLocation + 4));
        file->QueryInterface(NS_GET_IID(nsILocalFile), (void**)aSpec);
        return rv;
    }

    if (!strncmp(aLocation, XPCOM_RELCOMPONENT_PREFIX, 4)) {

        if (!mComponentsDir)
            return NS_ERROR_NOT_INITIALIZED;

        nsILocalFile* file = nsnull;
        rv = mComponentsDir->Clone((nsIFile**)&file);       

        if (NS_FAILED(rv)) return rv;

        rv = file->AppendRelativeNativePath(nsDependentCString(aLocation + 4));
        *aSpec = file;
        return rv;
    }
    *aSpec = nsnull;
    return NS_ERROR_INVALID_ARG;
}

/**
 * RegisterFactory()
 *
 * Register a factory to be responsible for creation of implementation of
 * classID aClass. Plus creates as association of aClassName and aContractID
 * to the classID. If replace is PR_TRUE, we replace any existing registrations
 * with this one.
 *
 * Once registration is complete, we add the class to the factories cache
 * that we maintain. The factories cache is the ONLY place where these
 * registrations are ever kept.
 *
 * The other RegisterFunctions create a loader mapping and persistent
 * location, but we just slam it into the cache here.  And we don't call the
 * loader's OnRegister function, either.
 */
nsresult
nsComponentManagerImpl::RegisterFactory(const nsCID &aClass,
                                        const char *aClassName,
                                        const char *aContractID,
                                        nsIFactory *aFactory, 
                                        PRBool aReplace)
{
    nsFactoryEntry *entry = nsnull;
    nsIDKey key(aClass);

    nsAutoMonitor mon(mMon);

    entry = GetFactoryEntry(aClass, key);

    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS))
    {
        char *buf = aClass.ToString();
        PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS,
               ("nsComponentManager: RegisterFactory(%s, %s)", buf,
                (aContractID ? aContractID : "(null)")));
        if (buf)
            PR_Free(buf);
    }


    if (entry && !aReplace) {
        // Already registered
        PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
               ("\t\tFactory already registered."));
        return NS_ERROR_FACTORY_EXISTS;
    }

    if (entry) {
        entry->ReInit(aClass, aFactory);
    }
    else {
        void *mem;
        PL_ARENA_ALLOCATE(mem, &mArena, sizeof(nsFactoryEntry));
        if (!mem)
            return NS_ERROR_OUT_OF_MEMORY;
        entry = new (mem) nsFactoryEntry(aClass, aFactory);

        nsFactoryTableEntry* factoryTableEntry =
            NS_STATIC_CAST(nsFactoryTableEntry*,
                           PL_DHashTableOperate(&mFactories, &aClass,
                                                PL_DHASH_ADD));

        if (!factoryTableEntry)
            return NS_ERROR_OUT_OF_MEMORY;

        factoryTableEntry->mFactoryEntry = entry;
    }

    // Update the ContractID->CLSID Map
    if (aContractID) {
        nsresult rv = HashContractID(aContractID, entry);
        if (NS_FAILED(rv)) {
            PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
                   ("\t\tFactory register succeeded. "
                    "Hashing contractid (%s) FAILED.", aContractID));
            return rv;
        }
    }

    PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
           ("\t\tFactory register succeeded contractid=%s.",
            aContractID ? aContractID : "<none>"));

    return NS_OK;
}

nsresult
nsComponentManagerImpl::RegisterComponent(const nsCID &aClass,
                                          const char *aClassName,
                                          const char *aContractID,
                                          const char *aPersistentDescriptor,
                                          PRBool aReplace,
                                          PRBool aPersist)
{
    return RegisterComponentCommon(aClass, aClassName, aContractID,
                                   aPersistentDescriptor, aReplace, aPersist,
                                   nativeComponentType);
}

nsresult
nsComponentManagerImpl::RegisterComponentWithType(const nsCID &aClass,
                                                  const char *aClassName,
                                                  const char *aContractID,
                                                  nsIFile *aSpec,
                                                  const char *aLocation,
                                                  PRBool aReplace,
                                                  PRBool aPersist,
                                                  const char *aType)
{
    return RegisterComponentCommon(aClass, aClassName, aContractID, 
                                   aLocation,
                                   aReplace, aPersist,
                                   aType);
}

/*
 * Register a component, using whatever they stuck in the nsIFile.
 */
nsresult
nsComponentManagerImpl::RegisterComponentSpec(const nsCID &aClass,
                                              const char *aClassName,
                                              const char *aContractID,
                                              nsIFile *aLibrarySpec,
                                              PRBool aReplace,
                                              PRBool aPersist)
{
    nsXPIDLCString registryName;
    nsresult rv = RegistryLocationForSpec(aLibrarySpec, getter_Copies(registryName));
    if (NS_FAILED(rv))
        return rv;

    rv = RegisterComponentWithType(aClass, aClassName, aContractID, aLibrarySpec,
                                   registryName,
                                   aReplace, aPersist,
                                   nativeComponentType);
    return rv;
}

/*
 * Register a ``library'', which is a DLL location named by a simple filename
 * such as ``libnsappshell.so'', rather than a relative or absolute path.
 *
 * It implies application/x-moz-dll as the component type, and skips the
 * FindLoaderForType phase.
 */
nsresult
nsComponentManagerImpl::RegisterComponentLib(const nsCID &aClass,
                                             const char *aClassName,
                                             const char *aContractID,
                                             const char *aDllName,
                                             PRBool aReplace,
                                             PRBool aPersist)
{
    nsXPIDLCString registryName;
    nsresult rv = RegistryNameForLib(aDllName, getter_Copies(registryName));
    if (NS_FAILED(rv))
        return rv;
    return RegisterComponentCommon(aClass, aClassName, aContractID, registryName,
                                   aReplace, aPersist, nativeComponentType);
}

/*
 * Add a component to the known universe of components.

 * Once we enter this function, we own aRegistryName, and must free it
 * or hand it to nsFactoryEntry.  Common exit point ``out'' helps keep us
 * sane.
 */

nsresult
nsComponentManagerImpl::RegisterComponentCommon(const nsCID &aClass,
                                                const char *aClassName,
                                                const char *aContractID,
                                                const char *aRegistryName,
                                                PRBool aReplace,
                                                PRBool aPersist,
                                                const char *aType)
{
    nsIDKey key(aClass);
    nsAutoMonitor mon(mMon);

    nsFactoryEntry *entry = GetFactoryEntry(aClass);

    // Normalize proid and classname
    const char *contractID = (aContractID && *aContractID) ? aContractID : nsnull;
    const char *className = (aClassName && *aClassName) ? aClassName : nsnull;

    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS))
    {
        char *buf = aClass.ToString();
        PR_LOG(nsComponentManagerLog, PR_LOG_DEBUG,
               ("nsComponentManager: RegisterComponentCommon(%s, %s, %s, %s)",
                buf,
                contractID ? contractID : "(null)",
                aRegistryName, aType));
        if (buf)
            PR_Free(buf);
    }

    if (entry && !aReplace) {
        PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
               ("\t\tFactory already registered."));
        return NS_ERROR_FACTORY_EXISTS;
    }

    int typeIndex = GetLoaderType(aType);

    nsCOMPtr<nsIComponentLoader> loader;
    nsresult rv = GetLoaderForType(typeIndex, getter_AddRefs(loader));
    if (NS_FAILED(rv)) {
        PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
               ("\t\tgetting loader for %s FAILED\n", aType));
        return rv;
    }

    if (entry) {
        entry->ReInit(aClass, aRegistryName, typeIndex);
    }
    else {

        // Arena allocate the nsFactoryEntry
        void *mem;
        PL_ARENA_ALLOCATE(mem, &mArena, sizeof(nsFactoryEntry));
        if (!mem)
            return NS_ERROR_OUT_OF_MEMORY;

        mRegistryDirty = PR_TRUE;
        entry = new (mem) nsFactoryEntry(aClass, aRegistryName, typeIndex);
        if (!entry)
            return NS_ERROR_OUT_OF_MEMORY;

        nsFactoryTableEntry* factoryTableEntry =
            NS_STATIC_CAST(nsFactoryTableEntry*,
                           PL_DHashTableOperate(&mFactories, &aClass,
                                                PL_DHASH_ADD));

        if (!factoryTableEntry)
            return NS_ERROR_OUT_OF_MEMORY;

        factoryTableEntry->mFactoryEntry = entry;
    }

    // Update the ContractID->CLSID Map
    if (contractID) {
        rv = HashContractID(contractID, entry);
        if (NS_FAILED(rv)) {
            PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
                   ("\t\tHashContractID(%s) FAILED\n", contractID));
            return rv;
        }
    }
    return rv;
}


nsresult
nsComponentManagerImpl::GetLoaderForType(int aType,
                                         nsIComponentLoader **aLoader)
{
    nsresult rv;

    // Make sure we have a valid type
    if (aType < 0 || aType >= mNLoaderData)
        return NS_ERROR_INVALID_ARG;

    *aLoader = mLoaderData[aType].loader;
    if (*aLoader) {
        NS_ADDREF(*aLoader);
        return NS_OK;
    }

    nsCOMPtr<nsIComponentLoader> loader;
    loader = do_GetServiceFromCategory("component-loader", mLoaderData[aType].type, &rv);
    if (NS_FAILED(rv))
        return rv;

    rv = loader->Init(this, nsnull);

    if (NS_SUCCEEDED(rv)) {
        mLoaderData[aType].loader = loader;
        NS_ADDREF(mLoaderData[aType].loader);
        *aLoader = loader;
        NS_ADDREF(*aLoader);
    }
    return rv;
}



// Convert a loader type string into an index into the component data
// array. Empty loader types are converted to NATIVE. Returns -1 if
// loader type cannot be determined.
int
nsComponentManagerImpl::GetLoaderType(const char *typeStr)
{
    if (!typeStr || !*typeStr) {
        // Empty type strings are NATIVE
        return NS_COMPONENT_TYPE_NATIVE;
    }

    for (int i=NS_COMPONENT_TYPE_NATIVE; i<mNLoaderData; i++) {
        if (!strcmp(typeStr, mLoaderData[i].type))
            return i;
    }
    // Not found
    return NS_COMPONENT_TYPE_FACTORY_ONLY;
}

// Add a loader type if not already known. Return the typeIndex
// if the loader type is either added or already there.
int
nsComponentManagerImpl::AddLoaderType(const char *typeStr)
{
    int typeIndex = GetLoaderType(typeStr);
    if (typeIndex >= 0) {
        return typeIndex;
    }

    // Add the loader type
    if (mNLoaderData >= mMaxNLoaderData) {
        NS_ASSERTION(mNLoaderData == mMaxNLoaderData,
                     "Memory corruption. nsComponentManagerImpl::mLoaderData array overrun.");
        // Need to increase our loader array
        nsLoaderdata *new_mLoaderData = (nsLoaderdata *) PR_Realloc(mLoaderData, (mMaxNLoaderData + NS_LOADER_DATA_ALLOC_STEP) * sizeof(nsLoaderdata));
        if (!new_mLoaderData)
            return NS_ERROR_OUT_OF_MEMORY;
        mLoaderData = new_mLoaderData;
        mMaxNLoaderData += NS_LOADER_DATA_ALLOC_STEP;
    }

    typeIndex = mNLoaderData;
    mLoaderData[typeIndex].type = PL_strdup(typeStr);
    if (!mLoaderData[typeIndex].type) {
        // mmh! no memory. return failure.
        return NS_ERROR_OUT_OF_MEMORY;
    }
    mLoaderData[typeIndex].loader = nsnull;
    mNLoaderData++;

    return typeIndex;
}

typedef struct
{
    const nsCID* cid;
    const char* regName;
    nsIFactory* factory;
} UnregisterConditions;

static PLDHashOperator PR_CALLBACK
DeleteFoundCIDs(PLDHashTable *aTable,
                PLDHashEntryHdr *aHdr,
                PRUint32 aNumber,
                void *aData)
{
    nsContractIDTableEntry* entry = NS_STATIC_CAST(nsContractIDTableEntry*, aHdr);

    if (!entry->mFactoryEntry || entry->mFactoryEntry == kNonExistentContractID) 
        return PL_DHASH_NEXT;

    UnregisterConditions* data = (UnregisterConditions*)aData;

    nsFactoryEntry* factoryEntry = entry->mFactoryEntry;
    if (data->cid->Equals(factoryEntry->cid) && 
        ((data->regName && !PL_strcasecmp(factoryEntry->location, data->regName)) ||
         (data->factory && data->factory == factoryEntry->factory.get())))
        return PL_DHASH_REMOVE;

    return PL_DHASH_NEXT;
}

void 
nsComponentManagerImpl::DeleteContractIDEntriesByCID(const nsCID* aClass, const char*registryName)
{
    UnregisterConditions aData;
    aData.cid     = aClass;
    aData.regName = registryName;
    aData.factory = nsnull;
    PL_DHashTableEnumerate(&mContractIDs, DeleteFoundCIDs, (void*)&aData);

}

void 
nsComponentManagerImpl::DeleteContractIDEntriesByCID(const nsCID* aClass, nsIFactory* factory)
{
    UnregisterConditions aData;
    aData.cid     = aClass;
    aData.regName = nsnull;
    aData.factory = factory;      
    PL_DHashTableEnumerate(&mContractIDs, DeleteFoundCIDs, (void*)&aData);
}

nsresult
nsComponentManagerImpl::UnregisterFactory(const nsCID &aClass,
                                          nsIFactory *aFactory)
{
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS)) 
    {
        char *buf = aClass.ToString();
        PR_LOG(nsComponentManagerLog, PR_LOG_DEBUG,
               ("nsComponentManager: UnregisterFactory(%s)", buf));
        if (buf)
            PR_Free(buf);
    }

    nsFactoryEntry *old;

    // first delete all contract id entries that are registered with this cid.      
    DeleteContractIDEntriesByCID(&aClass, aFactory);

    // next check to see if there is a CID registered
    nsIDKey key(aClass);
    nsresult rv = NS_ERROR_FACTORY_NOT_REGISTERED;
    old = GetFactoryEntry(aClass, key);

    if (old && (old->factory.get() == aFactory))
    {   
        nsAutoMonitor mon(mMon);
        PL_DHashTableOperate(&mFactories, &aClass, PL_DHASH_REMOVE);
        rv = NS_OK;
    }

    PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
           ("\t\tUnregisterFactory() %s",
            NS_SUCCEEDED(rv) ? "succeeded" : "FAILED"));
    return rv;
}

nsresult
nsComponentManagerImpl::UnregisterComponent(const nsCID &aClass,
                                            const char *registryName)
{
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_ALWAYS)) 
    {
        char *buf = aClass.ToString();
        PR_LOG(nsComponentManagerLog, PR_LOG_DEBUG,
               ("nsComponentManager: UnregisterComponent(%s)", buf));
        if (buf)
            PR_Free(buf);
    }

    NS_ENSURE_ARG_POINTER(registryName);
    nsFactoryEntry *old;

    // first delete all contract id entries that are registered with this cid.      
    DeleteContractIDEntriesByCID(&aClass, registryName);

    // next check to see if there is a CID registered
    nsIDKey key(aClass);
    old = GetFactoryEntry(aClass, key);
    if (old && old->location && !PL_strcasecmp(old->location, registryName))
    {   
        nsAutoMonitor mon(mMon);
        PL_DHashTableOperate(&mFactories, &aClass, PL_DHASH_REMOVE);
    }

    PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
           ("nsComponentManager: Factory unregister(%s) succeeded.", registryName));

    return NS_OK;
}

nsresult
nsComponentManagerImpl::UnregisterComponentSpec(const nsCID &aClass,
                                                nsIFile *aLibrarySpec)
{
    nsXPIDLCString registryName;
    nsresult rv = RegistryLocationForSpec(aLibrarySpec, getter_Copies(registryName));
    if (NS_FAILED(rv)) return rv;
    return UnregisterComponent(aClass, registryName);
}

// XXX Need to pass in aWhen and servicemanager
nsresult
nsComponentManagerImpl::FreeLibraries(void) 
{
    return UnloadLibraries(NS_STATIC_CAST(nsIServiceManager*, this), NS_Timer); // XXX when
}

// Private implementation of unloading libraries
nsresult
nsComponentManagerImpl::UnloadLibraries(nsIServiceManager *serviceMgr, PRInt32 aWhen)
{
    nsresult rv = NS_OK;

    nsAutoMonitor mon(mMon);

    PR_LOG(nsComponentManagerLog, PR_LOG_ALWAYS, 
           ("nsComponentManager: Unloading Libraries."));

    // UnloadAll the loaders
    /* iterate over all known loaders and ask them to autoregister. */
    // Skip mNativeComponentLoader
    for (int i=NS_COMPONENT_TYPE_NATIVE + 1; i<mNLoaderData; i++) {
        if (mLoaderData[i].loader) {
            rv = mLoaderData[i].loader->UnloadAll(aWhen);
            if (NS_FAILED(rv))
                break;
        }
    }

    // UnloadAll the native loader
    rv = mNativeComponentLoader->UnloadAll(aWhen);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////

/**
 * AutoRegister(RegistrationInstant, const char *directory)
 *
 * Given a directory in the following format, this will ensure proper registration
 * of all components. No default directory is looked at.
 *
 *    Directory and fullname are what NSPR will accept. For eg.
 *      WIN    y:/home/dp/mozilla/dist/bin
 *      UNIX   /home/dp/mozilla/dist/bin
 *      MAC    /Hard drive/mozilla/dist/apprunner
 *
 * This will take care not loading already registered dlls, finding and
 * registering new dlls, re-registration of modified dlls
 *
 */

nsresult
nsComponentManagerImpl::AutoRegister(PRInt32 when, nsIFile *inDirSpec)
{
    return AutoRegisterImpl(when, inDirSpec);
}

nsresult
nsComponentManagerImpl::AutoRegisterImpl(PRInt32 when, 
                                         nsIFile *inDirSpec, 
                                         PRBool fileIsCompDir)
{
    nsCOMPtr<nsIFile> dir;
    nsresult rv;

#ifdef DEBUG
    // testing release behaviour
    if (getenv("XPCOM_NO_AUTOREG"))
        return NS_OK;
#endif
    if (inDirSpec) 
    {
        // Use supplied components' directory   
        dir = inDirSpec;
    } 
    else 
    {
        // Do default components directory

        nsCOMPtr<nsIProperties> directoryService;
        nsDirectoryService::Create(nsnull, 
                                   NS_GET_IID(nsIProperties), 
                                   getter_AddRefs(directoryService));  

        if (!directoryService) 
            return NS_ERROR_FAILURE;

        rv = directoryService->Get(NS_XPCOM_COMPONENT_DIR, 
                                   NS_GET_IID(nsIFile), 
                                   getter_AddRefs(dir));
        if (NS_FAILED(rv)) 
            return rv; // XXX translate error code?
    }

    nsCOMPtr<nsIInterfaceInfoManager> iim = 
        dont_AddRef(XPTI_GetInterfaceInfoManager());

    if (!iim)
        return NS_ERROR_UNEXPECTED;    

    // Notify observers of xpcom autoregistration start
    NS_CreateServicesFromCategory(NS_XPCOM_AUTOREGISTRATION_OBSERVER_ID, 
                                  nsnull,
                                  "start");

    /* do the native loader first, so we can find other loaders */
    rv = mNativeComponentLoader->AutoRegisterComponents((PRInt32)when, dir);
    if (NS_FAILED(rv)) return rv;

#ifdef ENABLE_STATIC_COMPONENT_LOADER
    rv = mStaticComponentLoader->AutoRegisterComponents((PRInt32)when, dir);
    if (NS_FAILED(rv)) return rv;
#endif

    /* do InterfaceInfoManager after native loader so it can use components. */
    rv = iim->AutoRegisterInterfaces();
    if (NS_FAILED(rv)) return rv;

    if (!mCategoryManager) {
        NS_WARNING("mCategoryManager is null");
        return NS_ERROR_UNEXPECTED;
	}

    nsCOMPtr<nsISimpleEnumerator> loaderEnum;
    rv = mCategoryManager->EnumerateCategory("component-loader",
                                   getter_AddRefs(loaderEnum));
    if (NS_FAILED(rv)) return rv;

    PRBool hasMore;
    while (NS_SUCCEEDED(loaderEnum->HasMoreElements(&hasMore)) && hasMore) {
        nsCOMPtr<nsISupports> supports;
        if (NS_FAILED(loaderEnum->GetNext(getter_AddRefs(supports))))
            continue;

        nsCOMPtr<nsISupportsCString> supStr = do_QueryInterface(supports);
        if (!supStr)
            continue;

        nsCAutoString loaderType;
        if (NS_FAILED(supStr->GetData(loaderType)))
            continue;

        // We depend on the loader being created. Add the loader type and
        // create the loader object too.
        nsCOMPtr<nsIComponentLoader> loader;
        GetLoaderForType(AddLoaderType(loaderType.get()), getter_AddRefs(loader));
    }

    /* iterate over all known loaders and ask them to autoregister. */
    /* XXX convert when to nsIComponentLoader::(when) properly */
    nsIFile *spec = dir.get();
    for (int i = NS_COMPONENT_TYPE_NATIVE + 1; i < mNLoaderData; i++) {
        if (!mLoaderData[i].loader) {
            rv = GetLoaderForType(i, &mLoaderData[i].loader);
            if (NS_FAILED(rv))
                continue;
        }
        rv = mLoaderData[i].loader->AutoRegisterComponents(when, spec);
        if (NS_FAILED(rv))
            break;
    }

    if (NS_SUCCEEDED(rv))
    {
        PRBool registered;
        do {
            registered = PR_FALSE;
            for (int i = NS_COMPONENT_TYPE_NATIVE; i < mNLoaderData; i++) {
                PRBool b = PR_FALSE;
                if (mLoaderData[i].loader) {
                    rv = mLoaderData[i].loader->RegisterDeferredComponents(when, &b);
                    if (NS_FAILED(rv))
                        continue;
                    registered |= b;
                }
            }
        } while (NS_SUCCEEDED(rv) && registered);
    }

    // Notify observers of xpcom autoregistration completion
    NS_CreateServicesFromCategory(NS_XPCOM_AUTOREGISTRATION_OBSERVER_ID, 
                                  nsnull,
                                  "end");    

    if (mRegistryDirty)
        FlushPersistentStore(PR_TRUE);
    return rv;
}

nsresult
nsComponentManagerImpl::AutoRegisterComponent(PRInt32 when,
                                              nsIFile *component)
{
    nsresult rv = NS_OK;
    /*
     * Do we have to give the native loader first crack at it?
     * I vote ``no''.
     */
    for (int i = 0; i < mNLoaderData; i++) {
        PRBool didRegister;
        if (!mLoaderData[i].loader) {
            rv = GetLoaderForType(i, &mLoaderData[i].loader);
            if (NS_FAILED(rv))
                continue;
        }
        rv = mLoaderData[i].loader->AutoRegisterComponent((int)when, component, &didRegister);
        if (NS_SUCCEEDED(rv) && didRegister)
            break;
    }
    return NS_FAILED(rv) ? NS_ERROR_FACTORY_NOT_REGISTERED : NS_OK;

}

nsresult
nsComponentManagerImpl::AutoUnregisterComponent(PRInt32 when,
                                                nsIFile *component)
{
    nsresult rv = NS_OK;
    for (int i = 0; i < mNLoaderData; i++) {
        PRBool didUnRegister;
        if (!mLoaderData[i].loader) {
            rv = GetLoaderForType(i, &mLoaderData[i].loader);
            if (NS_FAILED(rv))
                continue;
        }
        rv = mLoaderData[i].loader->AutoUnregisterComponent(when, component, &didUnRegister);
        if (NS_SUCCEEDED(rv) && didUnRegister)
            break;
    }
    return NS_FAILED(rv) ? NS_ERROR_FACTORY_NOT_REGISTERED : NS_OK;
}

nsresult
nsComponentManagerImpl::IsRegistered(const nsCID &aClass,
                                     PRBool *aRegistered)
{
    if (!aRegistered)
    {
        NS_ASSERTION(0, "null ptr");
        return NS_ERROR_NULL_POINTER;
    }
    *aRegistered = (nsnull != GetFactoryEntry(aClass));
    return NS_OK;
}

nsresult
nsComponentManagerImpl::EnumerateCLSIDs(nsIEnumerator** aEnumerator)
{
    NS_ASSERTION(aEnumerator != nsnull, "null ptr");
    if (!aEnumerator)
    {
        return NS_ERROR_NULL_POINTER;
    }
    *aEnumerator = nsnull;

    nsresult rv;

    PLDHashTableEnumeratorImpl *aEnum;
    rv = PL_NewDHashTableEnumerator(&mFactories,
                                    ConvertFactoryEntryToCID,
                                    (void*)this, 
                                    &aEnum);
    if (NS_FAILED(rv))
        return rv;

    *aEnumerator = NS_STATIC_CAST(nsIEnumerator*, aEnum);
    return NS_OK;
}

nsresult
nsComponentManagerImpl::EnumerateContractIDs(nsIEnumerator** aEnumerator)
{
    NS_ASSERTION(aEnumerator != nsnull, "null ptr");
    if (!aEnumerator)
    {
        return NS_ERROR_NULL_POINTER;
    }

    *aEnumerator = nsnull;

    nsresult rv;
    PLDHashTableEnumeratorImpl *aEnum;
    rv = PL_NewDHashTableEnumerator(&mContractIDs,
                                    ConvertContractIDKeyToString,
                                    (void*)this, 
                                    &aEnum);
    if (NS_FAILED(rv))
        return rv;

    *aEnumerator = NS_STATIC_CAST(nsIEnumerator*, aEnum);
    return NS_OK;
}

// nsIComponentRegistrar

NS_IMETHODIMP 
nsComponentManagerImpl::AutoRegister(nsIFile *aSpec)
{
    if (aSpec == nsnull)
        return AutoRegisterImpl(0, aSpec);

    PRBool directory;
    aSpec->IsDirectory(&directory);

    if (directory)
        return AutoRegisterImpl(0, aSpec, PR_FALSE);

    return AutoRegisterComponent(0, aSpec);
}

NS_IMETHODIMP 
nsComponentManagerImpl::AutoUnregister(nsIFile *aSpec)
{
    // unregistering a complete directory is not implmeneted yet...FIX
    if (aSpec == nsnull)
        return NS_ERROR_NOT_IMPLEMENTED;

    PRBool directory;
    aSpec->IsDirectory(&directory);

    if (directory)
        return NS_ERROR_NOT_IMPLEMENTED;

    return AutoUnregisterComponent(0, aSpec);
}

NS_IMETHODIMP 
nsComponentManagerImpl::RegisterFactory(const nsCID & aClass, 
                                        const char *aClassName, 
                                        const char *aContractID, 
                                        nsIFactory *aFactory)
{
    return RegisterFactory(aClass, 
                           aClassName,
                           aContractID,
                           aFactory, 
                           PR_TRUE);
}

NS_IMETHODIMP 
nsComponentManagerImpl::RegisterFactoryLocation(const nsCID & aClass, 
                                                const char *aClassName, 
                                                const char *aContractID, 
                                                nsIFile *aFile, 
                                                const char *loaderStr, 
                                                const char *aType)
{
    nsXPIDLCString registryName;

    if (!loaderStr)
    {
        nsresult rv = RegistryLocationForSpec(aFile, getter_Copies(registryName));
        if (NS_FAILED(rv))
            return rv;
    }

    nsresult rv;
    rv = RegisterComponentWithType(aClass, 
                                   aClassName, 
                                   aContractID, 
                                   aFile,
                                   (loaderStr ? loaderStr : registryName.get()),
                                   PR_TRUE, 
                                   PR_TRUE,
                                   (aType ? aType : nativeComponentType));
    return rv;
}

NS_IMETHODIMP 
nsComponentManagerImpl::UnregisterFactoryLocation(const nsCID & aClass, 
                                                  nsIFile *aFile)
{
    return UnregisterComponentSpec(aClass, aFile);
}

NS_IMETHODIMP 
nsComponentManagerImpl::IsCIDRegistered(const nsCID & aClass, 
                                        PRBool *_retval)
{
    return IsRegistered(aClass, _retval);
}

NS_IMETHODIMP 
nsComponentManagerImpl::IsContractIDRegistered(const char *aClass, 
                                               PRBool *_retval)
{
    nsFactoryEntry *entry = GetFactoryEntry(aClass);

    if (!entry || entry == kNonExistentContractID)
        *_retval = PR_FALSE;
    else
        *_retval = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP 
nsComponentManagerImpl::EnumerateCIDs(nsISimpleEnumerator **aEnumerator)
{
    NS_ASSERTION(aEnumerator != nsnull, "null ptr");

    if (!aEnumerator)
        return NS_ERROR_NULL_POINTER;

    *aEnumerator = nsnull;

    nsresult rv;
    PLDHashTableEnumeratorImpl *aEnum;
    rv = PL_NewDHashTableEnumerator(&mFactories,
                                    ConvertFactoryEntryToCID,
                                    (void*)this, 
                                    &aEnum);
    if (NS_FAILED(rv))
        return rv;

    *aEnumerator = NS_STATIC_CAST(nsISimpleEnumerator*, aEnum);
    return NS_OK;
}

NS_IMETHODIMP 
nsComponentManagerImpl::EnumerateContractIDs(nsISimpleEnumerator **aEnumerator)
{
    NS_ASSERTION(aEnumerator != nsnull, "null ptr");
    if (!aEnumerator)
        return NS_ERROR_NULL_POINTER;

    *aEnumerator = nsnull;

    nsresult rv;
    PLDHashTableEnumeratorImpl *aEnum;
    rv = PL_NewDHashTableEnumerator(&mContractIDs,
                                    ConvertContractIDKeyToString,
                                    (void*)this, 
                                    &aEnum);
    if (NS_FAILED(rv))
        return rv;

    *aEnumerator = NS_STATIC_CAST(nsISimpleEnumerator*, aEnum);
    return NS_OK;
}

NS_IMETHODIMP 
nsComponentManagerImpl::CIDToContractID(const nsCID & aClass, 
                                        char **_retval)
{
    return CLSIDToContractID(aClass,
                             nsnull,
                             _retval);
}

NS_IMETHODIMP 
nsComponentManagerImpl::ContractIDToCID(const char *aContractID, 
                                        nsCID * *_retval)
{
    *_retval = (nsCID*) nsMemory::Alloc(sizeof(nsCID));
    if (!*_retval)
       return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = ContractIDToClassID(aContractID, *_retval);
    if (NS_FAILED(rv)) {
        nsMemory::Free(*_retval);
        *_retval = nsnull;
    }
    return rv;
}

// end nsIComponentRegistrar




NS_IMETHODIMP 
nsComponentManagerImpl::HasFileChanged(nsIFile *file, const char *loaderString, PRInt64 modDate, PRBool *_retval)
{
    *_retval = PR_TRUE;

    nsXPIDLCString registryName;
    nsresult rv = RegistryLocationForSpec(file, getter_Copies(registryName));
    if (NS_FAILED(rv))
        return rv;

    PRInt32 count = mAutoRegEntries.Count();
    for (PRInt32 i = 0; i<count; i++)
    {
        AutoRegEntry* entry = (AutoRegEntry*) mAutoRegEntries.ElementAt(i);
        NS_ASSERTION(entry, "bad entry in array");

        if (!strcmp(registryName.get(), entry->GetName()))
        {
            // Found in our array.
            *_retval = entry->Modified(&modDate);
            return NS_OK;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP 
nsComponentManagerImpl::SaveFileInfo(nsIFile *file, const char *loaderString, PRInt64 modDate)
{
    mRegistryDirty = PR_TRUE;
    nsXPIDLCString registryName;
    nsresult rv = RegistryLocationForSpec(file, getter_Copies(registryName));
    if (NS_FAILED(rv))
        return rv;

    // check to see if exists in the array before adding it so that we don't have dups.
    PRInt32 count = mAutoRegEntries.Count();
    for (PRInt32 i = 0; i<count; i++)
    {
        AutoRegEntry* entry = (AutoRegEntry*) mAutoRegEntries.ElementAt(i);
        NS_ASSERTION(entry, "bad entry in array");

        if (!strcmp(registryName.get(), entry->GetName()))
        {
            entry->SetDate(&modDate);
            return NS_OK;
        }
    }

    AutoRegEntry *entry = new AutoRegEntry(registryName, &modDate);

    if (!entry)
        return NS_ERROR_OUT_OF_MEMORY;

    mAutoRegEntries.AppendElement(entry);
    return NS_OK;
}

NS_IMETHODIMP 
nsComponentManagerImpl::RemoveFileInfo(nsIFile *file, const char *loaderString)
{
    mRegistryDirty = PR_TRUE;
    nsXPIDLCString registryName;
    nsresult rv = RegistryLocationForSpec(file, getter_Copies(registryName));
    if (NS_FAILED(rv))
        return rv;

    PRInt32 count = mAutoRegEntries.Count();
    for (PRInt32 i = 0; i<count; i++)
    {
        AutoRegEntry* entry = (AutoRegEntry*) mAutoRegEntries.ElementAt(i);
        NS_ASSERTION(entry, "bad entry in array");

        if (!strcmp(registryName.get(), entry->GetName()))
        {
            mAutoRegEntries.RemoveElementAt(i);
            delete entry;
            return NS_OK;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP 
nsComponentManagerImpl::FlushPersistentStore(PRBool now)
{
    mRegistryDirty = PR_TRUE;
    if (now) 
        return WritePersistentRegistry();

    return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// Static Access Functions
////////////////////////////////////////////////////////////////////////////////

NS_COM nsresult
NS_GetGlobalComponentManager(nsIComponentManager* *result)
{
#ifdef DEBUG_dougt
    //    NS_WARNING("DEPRECATED FUNCTION: Use NS_GetComponentManager");
#endif
    nsresult rv = NS_OK;

    if (nsComponentManagerImpl::gComponentManager == nsnull)
    {
        // XPCOM needs initialization.
        rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
    }

    if (NS_SUCCEEDED(rv))
    {
        // NO ADDREF since this is never intended to be released.
        // See nsComponentManagerObsolete.h for the reason for such
        // casting uglyness  
        *result = (nsIComponentManager*)(void*)(nsIComponentManagerObsolete*) nsComponentManagerImpl::gComponentManager;
    }

    return rv;
}

NS_COM nsresult
NS_GetComponentManager(nsIComponentManager* *result)
{
    if (nsComponentManagerImpl::gComponentManager == nsnull)
    {
        // XPCOM needs initialization.
        nsresult rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
        if (NS_FAILED(rv))
            return rv;
    }

    *result = NS_STATIC_CAST(nsIComponentManager*, 
                             nsComponentManagerImpl::gComponentManager);
    NS_IF_ADDREF(*result);
    return NS_OK;
}

NS_COM nsresult
NS_GetServiceManager(nsIServiceManager* *result)
{
    nsresult rv = NS_OK;

    if (nsComponentManagerImpl::gComponentManager == nsnull)
    {
        // XPCOM needs initialization.
        rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
    }

    if (NS_FAILED(rv))
        return rv;


    *result = NS_STATIC_CAST(nsIServiceManager*, 
                             nsComponentManagerImpl::gComponentManager);
    NS_IF_ADDREF(*result);
    return NS_OK;
}


NS_COM nsresult
NS_GetComponentRegistrar(nsIComponentRegistrar* *result)
{
    nsresult rv = NS_OK;

    if (nsComponentManagerImpl::gComponentManager == nsnull)
    {
        // XPCOM needs initialization.
        rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
    }

    if (NS_FAILED(rv))
        return rv;

    *result = NS_STATIC_CAST(nsIComponentRegistrar*, 
                             nsComponentManagerImpl::gComponentManager);
    NS_IF_ADDREF(*result);
    return NS_OK;
}


// nsIComponentLoaderManager is not frozen, but is defined here
// so that I can use it internally in xpcom.
nsresult
NS_GetComponentLoaderManager(nsIComponentLoaderManager* *result)
{
    nsresult rv = NS_OK;

    if (nsComponentManagerImpl::gComponentManager == NULL)
    {
        // XPCOM needs initialization.
        rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
    }

    if (NS_FAILED(rv))
        return rv;

    *result = NS_STATIC_CAST(nsIComponentLoaderManager*, 
                             nsComponentManagerImpl::gComponentManager);
    NS_IF_ADDREF(*result);
    return NS_OK;
}
