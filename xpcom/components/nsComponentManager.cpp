/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * ***** END LICENSE BLOCK *****
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

#include "nsAutoLock.h"
#include "nsCOMPtr.h"
#include "nsComponentManager.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsCategoryManager.h"
#include "nsCategoryManagerUtils.h"
#include "nsIEnumerator.h"
#include "xptiprivate.h"
#include "nsIConsoleService.h"
#include "nsIModule.h"
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

#include "nsInt64.h"
#include "nsManifestLineReader.h"

#include NEW_H     // for placement new


#ifdef XP_BEOS
#include <FindDirectory.h>
#include <Path.h>
#endif

#include "prlog.h"

NS_COM PRLogModuleInfo* nsComponentManagerLog = nsnull;

#if 0 || defined (DEBUG_timeless)
 #define SHOW_DENIED_ON_SHUTDOWN
 #define SHOW_CI_ON_EXISTING_SERVICE
 #define XPCOM_CHECK_PENDING_CIDS
#endif

// Bloated registry buffer size to improve startup performance -- needs to
// be big enough to fit the entire file into memory or it'll thrash.
// 512K is big enough to allow for some future growth in the registry.
#define BIG_REGISTRY_BUFLEN   (512*1024)

// Common Key Names
const char classIDKeyName[]="classID";
const char classesKeyName[]="contractID";
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
const static char XPCOM_GRECOMPONENT_PREFIX[] = "gre:";

static const char gIDFormat[] =
  "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}";


#define NS_EMPTY_IID                                 \
{                                                    \
    0x00000000,                                      \
    0x0000,                                          \
    0x0000,                                          \
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00} \
}

NS_DEFINE_CID(kEmptyCID, NS_EMPTY_IID);
NS_DEFINE_CID(kCategoryManagerCID, NS_CATEGORYMANAGER_CID);

#define UID_STRING_LENGTH 39

// Set to true from NS_ShutdownXPCOM.
extern PRBool gXPCOMShuttingDown;

static void GetIDString(const nsID& aCID, char buf[UID_STRING_LENGTH])
{
    PR_snprintf(buf, UID_STRING_LENGTH, gIDFormat,
                aCID.m0, (PRUint32) aCID.m1, (PRUint32) aCID.m2,
                (PRUint32) aCID.m3[0], (PRUint32) aCID.m3[1],
                (PRUint32) aCID.m3[2], (PRUint32) aCID.m3[3],
                (PRUint32) aCID.m3[4], (PRUint32) aCID.m3[5],
                (PRUint32) aCID.m3[6], (PRUint32) aCID.m3[7]);
}

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
        rv = NS_ERROR_SERVICE_NOT_AVAILABLE;
        goto error;
    }
    NS_GetComponentManager(getter_AddRefs(compMgr));
    if (!compMgr)
        return NS_ERROR_FAILURE;
    rv = compMgr->CreateInstanceByContractID(value, mOuter, aIID, aInstancePtr);
    if (NS_FAILED(rv)) {
    error:
        *aInstancePtr = 0;
    }

    if (mErrorPtr)
        *mErrorPtr = rv;
    return rv;
}


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
ArenaStrndup(const char *s, PRUint32 len, PLArenaPool *arena)
{
    void *mem;
    // Include trailing null in the len
    PL_ARENA_ALLOCATE(mem, arena, len+1);
    if (mem)
        memcpy(mem, s, len+1);
    return NS_STATIC_CAST(char *, mem);
}

char*
ArenaStrdup(const char *s, PLArenaPool *arena)
{
    return ArenaStrndup(s, strlen(s), arena);
}

////////////////////////////////////////////////////////////////////////////////
// Hashtable Callbacks
////////////////////////////////////////////////////////////////////////////////

PRBool PR_CALLBACK
nsFactoryEntry_Destroy(nsHashKey *aKey, void *aData, void* closure);

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

    return (entry->mFactoryEntry->mCid).Equals(*cidp);
}

PR_STATIC_CALLBACK(void)
factory_ClearEntry(PLDHashTable *aTable, PLDHashEntryHdr *aHdr)
{
    nsFactoryTableEntry* entry = NS_STATIC_CAST(nsFactoryTableEntry*, aHdr);
    // nsFactoryEntry is arena allocated. So we don't delete it.
    // We call the destructor by hand.
    entry->mFactoryEntry->~nsFactoryEntry();
    PL_DHashClearEntryStub(aTable, aHdr);
}

static const PLDHashTableOps factory_DHashTableOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    factory_HashKey,
    factory_MatchEntry,
    PL_DHashMoveEntryStub,
    factory_ClearEntry,
    PL_DHashFinalizeStub,
};

PR_STATIC_CALLBACK(void)
contractID_ClearEntry(PLDHashTable *aTable, PLDHashEntryHdr *aHdr)
{
    nsContractIDTableEntry* entry = NS_STATIC_CAST(nsContractIDTableEntry*, aHdr);
    if (entry->mFactoryEntry->mLoaderType == NS_LOADER_TYPE_INVALID &&
        entry->mFactoryEntry->mCid.Equals(kEmptyCID)) {
        // this object is owned by the hash.
        // nsFactoryEntry is arena allocated. So we don't delete it.
        // We call the destructor by hand.
        entry->mFactoryEntry->~nsFactoryEntry();
    }

    // contractIDs are arena allocated. No need to free them.

    PL_DHashClearEntryStub(aTable, aHdr);
}

static const PLDHashTableOps contractID_DHashTableOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    PL_DHashStringKey,
    PL_DHashMatchStringKey,
    PL_DHashMoveEntryStub,
    contractID_ClearEntry,
    PL_DHashFinalizeStub,
};

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

    PLDHashTableEnumeratorImpl(PLDHashTable *table,
                               EnumeratorConverter converter,
                               void *converterData);
    PRInt32 Count() { return mCount; }
private:
    PLDHashTableEnumeratorImpl(); /* no implementation */

    ~PLDHashTableEnumeratorImpl();
    void ReleaseElements();

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
                   nsISimpleEnumerator)

PLDHashTableEnumeratorImpl::~PLDHashTableEnumeratorImpl()
{
    ReleaseElements();

    // Destroy the Lock
    if (mMonitor)
        nsAutoMonitor::DestroyMonitor(mMonitor);
}

void
PLDHashTableEnumeratorImpl::ReleaseElements()
{
    for (PRInt32 i = 0; i < mCount; i++) {
        nsISupports *supports = NS_REINTERPRET_CAST(nsISupports *,
                                                    mElements[i]);
        NS_IF_RELEASE(supports);
    }
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

    NS_ADDREF(impl);

    if (impl->Count() == -1) {
        // conversion failed
        NS_RELEASE(impl);
        return NS_ERROR_FAILURE;
    }

    *retval = impl;
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
    if (!mCount || (mCurrent >= mCount - 1))
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

        wrapper->SetData(&fe->mCid);
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

    wrapper->SetData(nsDependentCString(entry->mContractID,
                                        entry->mContractIDLen));
    *retval = wrapper;
    NS_ADDREF(*retval);
    return NS_OK;
}

// this is safe to call during InitXPCOM
static nsresult GetLocationFromDirectoryService(const char* prop,
                                                nsIFile** aDirectory)
{
    nsCOMPtr<nsIProperties> directoryService;
    nsDirectoryService::Create(nsnull,
                               NS_GET_IID(nsIProperties),
                               getter_AddRefs(directoryService));

    if (!directoryService)
        return NS_ERROR_FAILURE;

    return directoryService->Get(prop,
                                 NS_GET_IID(nsIFile),
                                 (void**)aDirectory);
}


////////////////////////////////////////////////////////////////////////////////
// nsComponentManagerImpl
////////////////////////////////////////////////////////////////////////////////


nsComponentManagerImpl::nsComponentManagerImpl()
    :
    mMon(NULL),
    mShuttingDown(NS_SHUTDOWN_NEVERHAPPENED),
    mLoaderData(4),
    mRegistryDirty(PR_FALSE)
{
    mFactories.ops = nsnull;
    mContractIDs.ops = nsnull;
}

nsresult nsComponentManagerImpl::Init(nsStaticModuleInfo const *aStaticModules,
                                      PRUint32 aStaticModuleCount)
{
    PR_ASSERT(mShuttingDown != NS_SHUTDOWN_INPROGRESS);
    if (mShuttingDown == NS_SHUTDOWN_INPROGRESS)
        return NS_ERROR_FAILURE;

    mShuttingDown = NS_SHUTDOWN_NEVERHAPPENED;

    if (nsComponentManagerLog == nsnull)
    {
        nsComponentManagerLog = PR_NewLogModule("nsComponentManager");
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

        // Minimum alpha uses k=2 because nsFactoryTableEntry saves two
        // words compared to what a chained hash table requires.
        PL_DHashTableSetAlphaBounds(&mFactories,
                                    0.875,
                                    PL_DHASH_MIN_ALPHA(&mFactories, 2));
    }

    if (!mContractIDs.ops) {
        if (!PL_DHashTableInit(&mContractIDs, &contractID_DHashTableOps,
                               0, sizeof(nsContractIDTableEntry),
                               1024)) {
            mContractIDs.ops = nsnull;
            return NS_ERROR_OUT_OF_MEMORY;
        }

        // Minimum alpha uses k=1 because nsContractIDTableEntry saves one
        // word compared to what a chained hash table requires.
#if 0
        PL_DHashTableSetAlphaBounds(&mContractIDs,
                                    0.875,
                                    PL_DHASH_MIN_ALPHA(&mContractIDs, 1));
#endif
    }

    if (!mAutoRegEntries.Init(32))
        return NS_ERROR_OUT_OF_MEMORY;

    if (mMon == nsnull) {
        mMon = nsAutoMonitor::NewMonitor("nsComponentManagerImpl");
        if (mMon == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    GetLocationFromDirectoryService(NS_XPCOM_COMPONENT_DIR, getter_AddRefs(mComponentsDir));
    if (!mComponentsDir)
        return NS_ERROR_OUT_OF_MEMORY;

    nsCAutoString componentDescriptor;
    nsresult rv = mComponentsDir->GetNativePath(componentDescriptor);
    if (NS_FAILED(rv))
        return rv;

    mComponentsOffset = componentDescriptor.Length();

    GetLocationFromDirectoryService(NS_GRE_COMPONENT_DIR, getter_AddRefs(mGREComponentsDir));
    if (mGREComponentsDir) {
        nsresult rv = mGREComponentsDir->GetNativePath(componentDescriptor);
        if (NS_FAILED(rv)) {
            NS_WARNING("No GRE component manager");
            return rv;
        }
        mGREComponentsOffset = componentDescriptor.Length();
    }

    GetLocationFromDirectoryService(NS_XPCOM_COMPONENT_REGISTRY_FILE,
                                    getter_AddRefs(mRegistryFile));

    if(!mRegistryFile) {
        NS_WARNING("No Component Registry file was found in the directory service");
        return NS_ERROR_FAILURE;
    }

    PR_LOG(nsComponentManagerLog, PR_LOG_DEBUG,
           ("nsComponentManager: Initialized."));

    rv = mNativeModuleLoader.Init();
    if (NS_FAILED(rv))
        return rv;

    rv = mStaticModuleLoader.Init(aStaticModules, aStaticModuleCount);
    if (NS_FAILED(rv))
        return rv;

    return NS_OK;
}

nsresult nsComponentManagerImpl::Shutdown(void)
{
    PR_ASSERT(mShuttingDown == NS_SHUTDOWN_NEVERHAPPENED);
    if (mShuttingDown != NS_SHUTDOWN_NEVERHAPPENED)
        return NS_ERROR_FAILURE;

    mShuttingDown = NS_SHUTDOWN_INPROGRESS;

    // Shutdown the component manager
    PR_LOG(nsComponentManagerLog, PR_LOG_DEBUG, ("nsComponentManager: Beginning Shutdown."));

    // Write out our component data file.
    if (mRegistryDirty) {
        nsresult rv = WritePersistentRegistry();
        if (NS_FAILED(rv)) {
            PR_LOG(nsComponentManagerLog, PR_LOG_ERROR, ("nsComponentManager: Could not write out perisistant registry."));
#ifdef DEBUG
            printf("Could not write out perisistant registry!\n");
#endif
        }
    }

    mAutoRegEntries.Clear();

    // Release all cached factories
    if (mContractIDs.ops) {
        PL_DHashTableFinish(&mContractIDs);
        mContractIDs.ops = nsnull;
    }
    if (mFactories.ops) {
        PL_DHashTableFinish(&mFactories);
        mFactories.ops = nsnull;
    }

    mLoaderData.Clear();

    // Free staticm modules
    mStaticModuleLoader.ReleaseModules();

    // Unload libraries
    mNativeModuleLoader.UnloadLibraries();

    // delete arena for strings and small objects
    PL_FinishArenaPool(&mArena);

    mComponentsDir = 0;

    mCategoryManager = 0;

    mShuttingDown = NS_SHUTDOWN_COMPLETE;

    PR_LOG(nsComponentManagerLog, PR_LOG_DEBUG, ("nsComponentManager: Shutdown complete."));

    return NS_OK;
}

nsComponentManagerImpl::~nsComponentManagerImpl()
{
    PR_LOG(nsComponentManagerLog, PR_LOG_DEBUG, ("nsComponentManager: Beginning destruction."));

    if (mShuttingDown != NS_SHUTDOWN_COMPLETE)
        Shutdown();

    if (mMon) {
        nsAutoMonitor::DestroyMonitor(mMon);
    }
    PR_LOG(nsComponentManagerLog, PR_LOG_DEBUG, ("nsComponentManager: Destroyed."));
}

NS_IMPL_THREADSAFE_ISUPPORTS7(nsComponentManagerImpl,
                              nsIComponentManager,
                              nsIServiceManager,
                              nsISupportsWeakReference,
                              nsIInterfaceRequestor,
                              nsIComponentRegistrar,
                              nsIServiceManagerObsolete,
                              nsIComponentManagerObsolete)


nsresult
nsComponentManagerImpl::GetInterface(const nsIID & uuid, void **result)
{
    NS_WARNING("This isn't supported");
    // fall through to QI as anything QIable is a superset of what can be
    // got via the GetInterface()
    return  QueryInterface(uuid, result);
}

////////////////////////////////////////////////////////////////////////////////
// nsComponentManagerImpl: Platform methods
////////////////////////////////////////////////////////////////////////////////

#define PERSISTENT_REGISTRY_VERSION_MINOR 5
#define PERSISTENT_REGISTRY_VERSION_MAJOR 0

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
    NS_ASSERTION(mComponentsDir, "nsComponentManager not initialized.");

    nsresult rv;

    // populate Category Manager. need to get this early so that we don't get
    // skipped by 'goto out'
    mCategoryManager = do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv))
        return rv;

    nsAutoMonitor mon(mMon);
    nsManifestLineReader reader;

    PRFileDesc* fd = nsnull;

    // Set From Init
    if (!mRegistryFile) {
        return NS_ERROR_FILE_NOT_FOUND;
    }

    nsCOMPtr<nsIFile> file;
    mRegistryFile->Clone(getter_AddRefs(file));
    if (!file)
        return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<nsILocalFile> localFile(do_QueryInterface(file));

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
    if (!nsDependentCString(values[0], lengths[0]).EqualsLiteral("Version"))
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

        //name,last_modification_date[,optionaldata]
        int parts = reader.ParseLine(values, lengths, 3);
        if (2 != parts)
            break;

        PRInt64 a = nsCRT::atoll(values[1]);

        nsCOMPtr<nsILocalFile> lf;
        rv = FileForRegistryLocation(nsDependentCString(values[0], lengths[0]),
                                     getter_AddRefs(lf));
        if (NS_FAILED(rv))
            continue;

        nsCOMPtr<nsIHashable> lfhash(do_QueryInterface(lf));
        if (!lf) {
            NS_ERROR("nsLocalFile does not implement nsIHashable");
            continue;
        }

        mAutoRegEntries.Put(lfhash, a);
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

        LoaderType loadertype = AddLoaderType(values[2]);
        if (loadertype == NS_LOADER_TYPE_INVALID) {
            NS_ERROR("Could not create LoaderType");
            continue;
        }

        void *mem;
        PL_ARENA_ALLOCATE(mem, &mArena, sizeof(nsFactoryEntry));
        if (!mem) {
            rv = NS_ERROR_OUT_OF_MEMORY;
            goto out;
        }

        nsFactoryEntry *entry =
            new (mem) nsFactoryEntry(aClass, loadertype, values[4]);

        if (!entry->mLocationKey) {
            rv = NS_ERROR_OUT_OF_MEMORY;
            goto out;
        }

        nsFactoryTableEntry* factoryTableEntry =
            NS_STATIC_CAST(nsFactoryTableEntry*,
                           PL_DHashTableOperate(&mFactories,
                                                &aClass,
                                                PL_DHASH_ADD));

        if (!factoryTableEntry) {
            rv = NS_ERROR_OUT_OF_MEMORY;
            goto out;
        }

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
        if (!cidEntry || cidEntry->mLoaderType == NS_LOADER_TYPE_INVALID)
            continue; //what should we really do?

        nsContractIDTableEntry* contractIDTableEntry =
                NS_STATIC_CAST(nsContractIDTableEntry*,
                               PL_DHashTableOperate(&mContractIDs,
                                                    values[0],
                                                    PL_DHASH_ADD));
        if (!contractIDTableEntry) {
            continue;
        }

        if (!contractIDTableEntry->mContractID) {
            char *contractID = ArenaStrndup(values[0], lengths[0], &mArena);
            if (!contractID) {
                rv = NS_ERROR_OUT_OF_MEMORY;
                goto out; 
            }
            contractIDTableEntry->mContractID = contractID;
            contractIDTableEntry->mContractIDLen = lengths[0];
        }

        contractIDTableEntry->mFactoryEntry = cidEntry;
    }

#ifdef XPCOM_CHECK_PENDING_CIDS
    {
/*
 * If you get Asserts when you define SHOW_CI_ON_EXISTING_SERVICE and want to
 * track down their cause, then you should add the contracts listed by the
 * assertion to abusedContracts. The next time you run your xpcom app, xpcom
 * will assert the first time the object associated with the contract is
 * instantiated (which in many cases is the source of the problem).
 *
 * If you're doing this then you might want to NOP and soft breakpoint the
 * lines labeled: NOP_AND_BREAK.
 *
 * Otherwise XPCOM will refuse to create the object for the caller, which
 * while reasonable at some level, will almost certainly cause the app to
 * stop functioning normally.
 */
        static char abusedContracts[][128] = {
        /*// Example contracts:
            "@mozilla.org/rdf/container;1",
            "@mozilla.org/intl/charsetalias;1",
            "@mozilla.org/locale/win32-locale;1",
            "@mozilla.org/widget/lookandfeel/win;1",
        // */
            0
        };
        for (int i=0; abusedContracts[i] && *abusedContracts[i]; i++) {
            nsFactoryEntry *entry = nsnull;
            nsContractIDTableEntry* contractIDTableEntry =
                NS_STATIC_CAST(nsContractIDTableEntry*,
                               PL_DHashTableOperate(&mContractIDs, abusedContracts[i],
                                                    PL_DHASH_LOOKUP));

            if (PL_DHASH_ENTRY_IS_BUSY(contractIDTableEntry)) {
                entry = contractIDTableEntry->mFactoryEntry;
                AddPendingCID(entry->mCid);
            }
        }
    }
#endif

    if (ReadSectionHeader(reader, "CATEGORIES"))
        goto out;

    mCategoryManager->SuppressNotifications(PR_TRUE);

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

    mCategoryManager->SuppressNotifications(PR_FALSE);

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
    nsTArray<nsLoaderdata> *mLoaderData;
};

PR_STATIC_CALLBACK(PLDHashOperator)
ContractIDWriter(PLDHashTable *table,
                 PLDHashEntryHdr *hdr,
                 PRUint32 number,
                 void *arg)
{
    char *contractID   = ((nsContractIDTableEntry*)hdr)->mContractID;
    nsFactoryEntry *factoryEntry = ((nsContractIDTableEntry*)hdr)->mFactoryEntry;

    // for now, we only save out the top most parent.
    while (factoryEntry->mParent)
        factoryEntry = factoryEntry->mParent;

    if (factoryEntry->mLoaderType == NS_LOADER_TYPE_INVALID)
        return PL_DHASH_NEXT;

    PRFileDesc* fd = ((PersistentWriterArgs*)arg)->mFD;

    char cidString[UID_STRING_LENGTH];
    GetIDString(factoryEntry->mCid, cidString);
    PR_fprintf(fd, "%s,%s\n", contractID, cidString); // what if this fails?
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
    nsTArray<nsLoaderdata> *loaderData = ((PersistentWriterArgs*)arg)->mLoaderData;

    // for now, we only save out the top most parent.
    while (factoryEntry->mParent)
        factoryEntry = factoryEntry->mParent;

    if (factoryEntry->mLoaderType == NS_LOADER_TYPE_INVALID) {
        return PL_DHASH_NEXT;
    }

    char cidString[UID_STRING_LENGTH];
    GetIDString(factoryEntry->mCid, cidString);

    char *contractID = nsnull, *className = nsnull;

    nsCOMPtr<nsIClassInfo> classInfo = do_QueryInterface(factoryEntry->mFactory);
    if (classInfo)
    {
        classInfo->GetContractID(&contractID);
        classInfo->GetClassDescription(&className);
    }

    const char *loaderName;
    switch (factoryEntry->mLoaderType) {
    case NS_LOADER_TYPE_STATIC:
        loaderName = staticComponentType;
        break;

    case NS_LOADER_TYPE_NATIVE:
        loaderName = nativeComponentType;
        break;

    default:
        loaderName = loaderData->ElementAt(factoryEntry->mLoaderType).type.get();
    }

    const char* location = factoryEntry->mLocationKey;

    // cid,contract_id,type,class_name,inproc_server
    PR_fprintf(fd,
               "%s,%s,%s,%s,%s\n",
               cidString,
               (contractID ? contractID : ""),
               (loaderName ? loaderName : ""),
               (className  ? className  : ""),
               (location   ? location   : ""));

    if (contractID)
        PR_Free(contractID);
    if (className)
        PR_Free(className);

    return PL_DHASH_NEXT;
}

static PLDHashOperator
AutoRegEntryWriter(nsIHashable *aKey, PRInt64 &aTimestamp, void* aClosure)
{
    PRFileDesc* fd = (PRFileDesc*) aClosure;

    nsCOMPtr<nsILocalFile> lf(do_QueryInterface(aKey));

    nsCAutoString location;
    nsComponentManagerImpl::gComponentManager->
        RegistryLocationForFile(lf, location);

    PR_fprintf(fd, "%s,%lld\n", location.get(), (PRInt64) aTimestamp);

    return PL_DHASH_NEXT;
}

nsresult
nsComponentManagerImpl::WritePersistentRegistry()
{
    if (!mRegistryFile)
        return NS_ERROR_FAILURE;  // this should have been set by Init().

    nsCOMPtr<nsIFile> file;
    mRegistryFile->Clone(getter_AddRefs(file));
    if (!file)
        return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<nsILocalFile> localFile(do_QueryInterface(file));

    nsCAutoString originalLeafName;
    localFile->GetNativeLeafName(originalLeafName);

    nsCAutoString leafName;
    leafName.Assign(originalLeafName + NS_LITERAL_CSTRING(".tmp"));

    localFile->SetNativeLeafName(leafName);

    PRFileDesc* fd = nsnull;
    // Owner and group can setup components, everyone else should be able to see but not poison them.
    nsresult rv = localFile->OpenNSPRFileDesc(PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 0664, &fd);
    if (NS_FAILED(rv))
        return rv;

    if (PR_fprintf(fd, "Generated File. Do not edit.\n") == (PRUint32) -1) {
        rv = NS_ERROR_UNEXPECTED;
        goto out;
    }

    if (PR_fprintf(fd, "\n[HEADER]\nVersion,%d,%d\n",
                   PERSISTENT_REGISTRY_VERSION_MAJOR,
                   PERSISTENT_REGISTRY_VERSION_MINOR) == (PRUint32) -1) {
        rv = NS_ERROR_UNEXPECTED;
        goto out;
    }

    if (PR_fprintf(fd, "\n[COMPONENTS]\n") == (PRUint32) -1) {
        rv = NS_ERROR_UNEXPECTED;
        goto out;
    }

    mAutoRegEntries.Enumerate(AutoRegEntryWriter, (void*)fd);

    PersistentWriterArgs args;
    args.mFD = fd;
    args.mLoaderData = &mLoaderData;

    if (PR_fprintf(fd, "\n[CLASSIDS]\n") == (PRUint32) -1) {
        rv = NS_ERROR_UNEXPECTED;
        goto out;
    }


    PL_DHashTableEnumerate(&mFactories, ClassIDWriter, (void*)&args);

    if (PR_fprintf(fd, "\n[CONTRACTIDS]\n") == (PRUint32) -1) {
        rv = NS_ERROR_UNEXPECTED;
        goto out;
    }


    PL_DHashTableEnumerate(&mContractIDs, ContractIDWriter, (void*)&args);

    if (PR_fprintf(fd, "\n[CATEGORIES]\n") == (PRUint32) -1) {
        rv = NS_ERROR_UNEXPECTED;
        goto out;
    }

    NS_ASSERTION(mCategoryManager, "nsComponentManager used initialized");

    rv = mCategoryManager->WriteCategoryManagerToRegistry(fd);

out:
    PR_Close(fd);

    // don't create the file is there was a problem????
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mRegistryFile)
        return NS_ERROR_NOT_INITIALIZED;

    PRBool exists;
    if(NS_FAILED(mRegistryFile->Exists(&exists)))
        return PR_FALSE;

    if(exists && NS_FAILED(mRegistryFile->Remove(PR_FALSE)))
        return PR_FALSE;

    nsCOMPtr<nsIFile> parent;
    mRegistryFile->GetParent(getter_AddRefs(parent));

    rv = localFile->MoveToNative(parent, originalLeafName);
    mRegistryDirty = PR_FALSE;

    return rv;
}


////////////////////////////////////////////////////////////////////////////////
// Hash Functions
////////////////////////////////////////////////////////////////////////////////
nsresult
nsComponentManagerImpl::HashContractID(const char *aContractID,
                                       PRUint32 aContractIDLen,
                                       nsFactoryEntry *fe)
{
    if(!aContractID || !aContractIDLen)
        return NS_ERROR_NULL_POINTER;

    nsAutoMonitor mon(mMon);

    nsContractIDTableEntry* contractIDTableEntry =
        NS_STATIC_CAST(nsContractIDTableEntry*,
                       PL_DHashTableOperate(&mContractIDs, aContractID,
                                            PL_DHASH_ADD));
    if (!contractIDTableEntry)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ASSERTION(!contractIDTableEntry->mContractID || !strcmp(contractIDTableEntry->mContractID, aContractID), "contractid conflict");

    if (!contractIDTableEntry->mContractID) {
        char *contractID = ArenaStrndup(aContractID, aContractIDLen, &mArena);
        if (!contractID)
            return NS_ERROR_OUT_OF_MEMORY;

        contractIDTableEntry->mContractID = contractID;
        contractIDTableEntry->mContractIDLen = aContractIDLen;
    }

    contractIDTableEntry->mFactoryEntry = fe;

    return NS_OK;
}

nsFactoryEntry *
nsComponentManagerImpl::GetFactoryEntry(const char *aContractID,
                                        PRUint32 aContractIDLen)
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

    return fe;
}


nsFactoryEntry *
nsComponentManagerImpl::GetFactoryEntry(const nsCID &aClass)
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
NS_IMETHODIMP
nsComponentManagerImpl::FindFactory(const nsCID &aClass,
                                    nsIFactory **aFactory)
{
    PR_ASSERT(aFactory != nsnull);

    nsFactoryEntry *entry = GetFactoryEntry(aClass);

    if (!entry)
        return NS_ERROR_FACTORY_NOT_REGISTERED;

    return entry->GetFactory(aFactory);
}


nsresult
nsComponentManagerImpl::FindFactory(const char *contractID,
                                    PRUint32 aContractIDLen,
                                    nsIFactory **aFactory)
{
    PR_ASSERT(aFactory != nsnull);

    nsFactoryEntry *entry = GetFactoryEntry(contractID, aContractIDLen);

    if (!entry)
        return NS_ERROR_FACTORY_NOT_REGISTERED;

    return entry->GetFactory(aFactory);
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

    nsCOMPtr<nsIFactory> factory;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_DEBUG))
    {
        char *buf = aClass.ToString();
        PR_LogPrint("nsComponentManager: GetClassObject(%s)", buf);
        if (buf)
            PR_Free(buf);
    }
#endif

    PR_ASSERT(aResult != nsnull);

    rv = FindFactory(aClass, getter_AddRefs(factory));
    if (NS_FAILED(rv)) return rv;

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
    nsresult rv;

    nsCOMPtr<nsIFactory> factory;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_DEBUG))
    {
        PR_LogPrint("nsComponentManager: GetClassObject(%s)", contractID);
    }
#endif

    PR_ASSERT(aResult != nsnull);

    rv = FindFactory(contractID, strlen(contractID), getter_AddRefs(factory));
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
NS_IMETHODIMP
nsComponentManagerImpl::ContractIDToClassID(const char *aContractID, nsCID *aClass)
{
    NS_PRECONDITION(aContractID != nsnull, "null ptr");
    if (!aContractID)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aClass != nsnull, "null ptr");
    if (!aClass)
        return NS_ERROR_NULL_POINTER;

    nsresult rv = NS_ERROR_FACTORY_NOT_REGISTERED;

    nsFactoryEntry *fe = GetFactoryEntry(aContractID, strlen(aContractID));
    if (fe) {
        *aClass = fe->mCid;
        rv = NS_OK;
    }
#ifdef PR_LOGGING
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_WARNING)) {
        char *buf = 0;
        if (NS_SUCCEEDED(rv))
            buf = aClass->ToString();
        PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
               ("nsComponentManager: ContractIDToClassID(%s)->%s", aContractID,
                NS_SUCCEEDED(rv) ? buf : "[FAILED]"));
        if (buf)
            PR_Free(buf);
    }
#endif
    return rv;
}

/**
 * CLSIDToContractID()
 *
 * Translates a classID to a {ContractID, Class Name}. Does direct registry
 * access to do the translation.
 *
 * NOTE: Since this isn't heavily used, we aren't caching this.
 */
NS_IMETHODIMP
nsComponentManagerImpl::CLSIDToContractID(const nsCID &aClass,
                                          char* *aClassName,
                                          char* *aContractID)
{
    NS_WARNING("Need to implement CLSIDToContractID");

    nsresult rv = NS_ERROR_FACTORY_NOT_REGISTERED;
#ifdef PR_LOGGING
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_WARNING))
    {
        char *buf = aClass.ToString();
        PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
               ("nsComponentManager: CLSIDToContractID(%s)->%s", buf,
                NS_SUCCEEDED(rv) ? *aContractID : "[FAILED]"));
        if (buf)
            PR_Free(buf);
    }
#endif
    return rv;
}

#ifdef XPCOM_CHECK_PENDING_CIDS

// This method must be called from within the mMon monitor
nsresult
nsComponentManagerImpl::AddPendingCID(const nsCID &aClass)
{
    int max = mPendingCIDs.Count();
    for (int index = 0; index < max; index++)
    {
        nsCID *cidp = (nsCID*) mPendingCIDs.ElementAt(index);
        NS_ASSERTION(cidp, "Bad CID in pending list");
        if (cidp->Equals(aClass)) {
            nsXPIDLCString cid;
            cid.Adopt(aClass.ToString());
            nsCAutoString message;
            message = NS_LITERAL_CSTRING("Creation of \"") +
                      cid + NS_LITERAL_CSTRING("\" in progress (Reentrant GS - see bug 194568)");
            // Note that you may see this assertion by near-simultaneous
            // calls to GetService on multiple threads.
            NS_WARNING(message.get());
            return NS_ERROR_NOT_AVAILABLE;
        }
    }
    mPendingCIDs.AppendElement((void*)&aClass);
    return NS_OK;
}

// This method must be called from within the mMon monitor
void
nsComponentManagerImpl::RemovePendingCID(const nsCID &aClass)
{
    mPendingCIDs.RemoveElement((void*)&aClass);
}
#endif
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

    if (aResult == nsnull)
    {
        return NS_ERROR_NULL_POINTER;
    }
    *aResult = nsnull;

    nsFactoryEntry *entry = GetFactoryEntry(aClass);

    if (!entry)
        return NS_ERROR_FACTORY_NOT_REGISTERED;

#ifdef SHOW_CI_ON_EXISTING_SERVICE
    if (entry->mServiceObject) {
        nsXPIDLCString cid;
        cid.Adopt(aClass.ToString());
        nsCAutoString message;
        message = NS_LITERAL_CSTRING("You are calling CreateInstance \"") +
                  cid + NS_LITERAL_CSTRING("\" when a service for this CID already exists!");
        NS_ERROR(message.get());
    }
#endif

    nsIFactory *factory = nsnull;
    nsresult rv = entry->GetFactory(&factory);

    if (NS_SUCCEEDED(rv))
    {
        rv = factory->CreateInstance(aDelegate, aIID, aResult);
        if (NS_SUCCEEDED(rv) && !*aResult) {
            NS_ERROR("Factory did not return an object but returned success!");
            rv = NS_ERROR_SERVICE_NOT_FOUND;
        }
        NS_RELEASE(factory);
    }
    else
    {
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
            PR_Free(buf);
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

    if (aResult == nsnull)
    {
        return NS_ERROR_NULL_POINTER;
    }
    *aResult = nsnull;

    nsFactoryEntry *entry = GetFactoryEntry(aContractID, strlen(aContractID));

    if (!entry)
        return NS_ERROR_FACTORY_NOT_REGISTERED;

#ifdef SHOW_CI_ON_EXISTING_SERVICE
    if (entry->mServiceObject) {
        nsCAutoString message;
        message =
          NS_LITERAL_CSTRING("You are calling CreateInstance \"") +
          nsDependentCString(aContractID) +
          NS_LITERAL_CSTRING("\" when a service for this CID already exists! "
            "Add it to abusedContracts to track down the service consumer.");
        NS_ERROR(message.get());
    }
#endif

    nsIFactory *factory = nsnull;
    nsresult rv = entry->GetFactory(&factory);

    if (NS_SUCCEEDED(rv))
    {

        rv = factory->CreateInstance(aDelegate, aIID, aResult);
        if (NS_SUCCEEDED(rv) && !*aResult) {
            NS_ERROR("Factory did not return an object but returned success!");
            rv = NS_ERROR_SERVICE_NOT_FOUND;
        }
        NS_RELEASE(factory);
    }
    else
    {
        // Translate error values
        rv = NS_ERROR_FACTORY_NOT_REGISTERED;
    }

    PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
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

    if (!entry->mFactoryEntry)
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

    if (!entry->mFactoryEntry)
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
        nsCOMPtr<nsISupports> supports = entry->mServiceObject;
        mon.Exit();
        return supports->QueryInterface(aIID, result);
    }

#ifdef XPCOM_CHECK_PENDING_CIDS
    rv = AddPendingCID(aClass);
    if (NS_FAILED(rv))
        return rv; // NOP_AND_BREAK
#endif
    nsCOMPtr<nsISupports> service;
    // We need to not be holding the service manager's monitor while calling
    // CreateInstance, because it invokes user code which could try to re-enter
    // the service manager:
    mon.Exit();

    rv = CreateInstance(aClass, nsnull, aIID, getter_AddRefs(service));

    mon.Enter();

#ifdef XPCOM_CHECK_PENDING_CIDS
    RemovePendingCID(aClass);
#endif

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
    if (!*result) {
        NS_ERROR("Factory did not return an object but returned success!");
        return NS_ERROR_SERVICE_NOT_FOUND;
    }
    NS_ADDREF(NS_STATIC_CAST(nsISupports*, (*result)));
    return rv;
}

NS_IMETHODIMP
nsComponentManagerImpl::RegisterService(const nsCID& aClass, nsISupports* aService)
{
    nsAutoMonitor mon(mMon);

    // check to see if we have a factory entry for the service
    nsFactoryEntry *entry = GetFactoryEntry(aClass);

    if (!entry) {
        void *mem;
        PL_ARENA_ALLOCATE(mem, &mArena, sizeof(nsFactoryEntry));
        if (!mem)
            return NS_ERROR_OUT_OF_MEMORY;
        entry = new (mem) nsFactoryEntry(aClass, (nsIFactory*) nsnull);

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
        return NS_ERROR_SERVICE_NOT_AVAILABLE;

    entry->mServiceObject = nsnull;
    return rv;
}

NS_IMETHODIMP
nsComponentManagerImpl::RegisterService(const char* aContractID,
                                        nsISupports* aService)
{

    nsAutoMonitor mon(mMon);

    // check to see if we have a factory entry for the service
    PRUint32 contractIDLen = strlen(aContractID);
    nsFactoryEntry *entry = GetFactoryEntry(aContractID, contractIDLen);

    if (!entry) {
        void *mem;
        PL_ARENA_ALLOCATE(mem, &mArena, sizeof(nsFactoryEntry));
        if (!mem)
            return NS_ERROR_OUT_OF_MEMORY;
        entry = new (mem) nsFactoryEntry(kEmptyCID, (nsIFactory*) nsnull);

        nsContractIDTableEntry* contractIDTableEntry =
            NS_STATIC_CAST(nsContractIDTableEntry*,
                           PL_DHashTableOperate(&mContractIDs, aContractID,
                                                PL_DHASH_ADD));
        if (!contractIDTableEntry) {
            delete entry;
            return NS_ERROR_OUT_OF_MEMORY;
        }

        if (!contractIDTableEntry->mContractID) {
            char *contractID = ArenaStrndup(aContractID, contractIDLen, &mArena);
            if (!contractID)
                return NS_ERROR_OUT_OF_MEMORY;

            contractIDTableEntry->mContractID = contractID;
            contractIDTableEntry->mContractIDLen = contractIDLen;
        }

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

    if (entry && entry->mServiceObject) {
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

   if (!entry || !entry->mServiceObject)
        return NS_ERROR_SERVICE_NOT_AVAILABLE;

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
        // When processing shutdown, don't process new GetService() requests
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

    if (entry) {
        if (entry->mServiceObject) {
            nsCOMPtr<nsISupports> serviceObject = entry->mServiceObject;

            // We need to not be holding the service manager's monitor while calling
            // QueryInterface, because it invokes user code which could try to re-enter
            // the service manager, or try to grab some other lock/monitor/condvar
            // and deadlock, e.g. bug 282743.
            mon.Exit();
            return serviceObject->QueryInterface(aIID, result);
        }
#ifdef XPCOM_CHECK_PENDING_CIDS
        rv = AddPendingCID(entry->mCid);
        if (NS_FAILED(rv))
            return rv; // NOP_AND_BREAK
#endif
    }

    nsCOMPtr<nsISupports> service;
    // We need to not be holding the service manager's monitor while calling
    // CreateInstance, because it invokes user code which could try to re-enter
    // the service manager:
    mon.Exit();

    rv = CreateInstanceByContractID(aContractID, nsnull, aIID, getter_AddRefs(service));

    mon.Enter();

#ifdef XPCOM_CHECK_PENDING_CIDS 
    if (entry)
        RemovePendingCID(entry->mCid);
#endif

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
}

NS_IMETHODIMP
nsComponentManagerImpl::GetService(const char* aContractID, const nsIID& aIID,
                                     nsISupports* *result,
                                     nsIShutdownListener* shutdownListener)
{
    return GetServiceByContractID(aContractID, aIID, (void**)result);
}


NS_IMETHODIMP
nsComponentManagerImpl::ReleaseService(const nsCID& aClass, nsISupports* service,
                                         nsIShutdownListener* shutdownListener)
{
    NS_IF_RELEASE(service);
    return NS_OK;
}

NS_IMETHODIMP
nsComponentManagerImpl::ReleaseService(const char* aContractID, nsISupports* service,
                                       nsIShutdownListener* shutdownListener)
{
    NS_IF_RELEASE(service);
    return NS_OK;
}

NS_IMETHODIMP
nsComponentManagerImpl::RegistryLocationForSpec(nsIFile *aSpec,
                                                char **aRegistryName)
{
    nsCAutoString location;
    nsresult rv = RegistryLocationForFile(aSpec, location);
    if (NS_SUCCEEDED(rv)) {
        *aRegistryName = ToNewCString(location);
        if (!*aRegistryName)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    return rv;
}

nsresult
nsComponentManagerImpl::RegistryLocationForFile(nsIFile* aFile,
                                                nsCString& aRegistryName)
{
    nsresult rv;

    if (!mComponentsDir)
        return NS_ERROR_NOT_INITIALIZED;

    // First check to see if this component is in the application
    // components directory
    PRBool containedIn;
    mComponentsDir->Contains(aFile, PR_TRUE, &containedIn);

    nsCAutoString nativePathString;

    if (containedIn){
        rv = aFile->GetNativePath(nativePathString);
        if (NS_FAILED(rv))
            return rv;

        aRegistryName.Assign(NS_LITERAL_CSTRING(XPCOM_RELCOMPONENT_PREFIX) +
                             Substring(nativePathString, mComponentsOffset + 1));
        return NS_OK;
    }

    // Next check to see if this component is in the GRE
    // components directory

    mGREComponentsDir->Contains(aFile, PR_TRUE, &containedIn);

    if (containedIn){
        rv = aFile->GetNativePath(nativePathString);
        if (NS_FAILED(rv))
            return rv;

        aRegistryName.Assign(NS_LITERAL_CSTRING(XPCOM_GRECOMPONENT_PREFIX) +
                             Substring(nativePathString, mGREComponentsOffset + 1));
        return NS_OK;
    }

    /* absolute names include volume info on Mac, so persistent descriptor */
    rv = aFile->GetNativePath(nativePathString);
    if (NS_FAILED(rv))
        return rv;

    aRegistryName.Assign(NS_LITERAL_CSTRING(XPCOM_ABSCOMPONENT_PREFIX) +
                         nativePathString);
    return NS_OK;
}

NS_IMETHODIMP
nsComponentManagerImpl::SpecForRegistryLocation(const char *aLocation,
                                                nsIFile **aSpec)
{
    return FileForRegistryLocation(nsDependentCString(aLocation),
                                   (nsILocalFile**) aSpec);
}

nsresult
nsComponentManagerImpl::FileForRegistryLocation(const nsCString &aLocation,
                                                nsILocalFile **aSpec)
{
    // i18n: assuming aLocation is encoded for the current locale

    nsresult rv;

    const nsDependentCSubstring prefix = Substring(aLocation, 0, 4);

    /* abs:/full/path/to/libcomponent.so */
    if (prefix.EqualsLiteral(XPCOM_ABSCOMPONENT_PREFIX)) {

        nsLocalFile* file = new nsLocalFile;
        if (!file) return NS_ERROR_FAILURE;

        rv = file->InitWithNativePath(Substring(aLocation, 4));
        file->QueryInterface(NS_GET_IID(nsILocalFile), (void**)aSpec);
        return rv;
    }

    if (prefix.EqualsLiteral(XPCOM_RELCOMPONENT_PREFIX)) {

        if (!mComponentsDir)
            return NS_ERROR_NOT_INITIALIZED;

        nsILocalFile* file = nsnull;
        rv = mComponentsDir->Clone((nsIFile**)&file);

        if (NS_FAILED(rv)) return rv;

        rv = file->AppendRelativeNativePath(Substring(aLocation, 4));
        *aSpec = file;
        return rv;
    }

    if (prefix.EqualsLiteral(XPCOM_GRECOMPONENT_PREFIX)) {

        if (!mGREComponentsDir)
            return NS_ERROR_NOT_INITIALIZED;

        nsILocalFile* file = nsnull;
        rv = mGREComponentsDir->Clone((nsIFile**)&file);

        if (NS_FAILED(rv)) return rv;

        rv = file->AppendRelativeNativePath(Substring(aLocation, 4));
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
NS_IMETHODIMP
nsComponentManagerImpl::RegisterFactory(const nsCID &aClass,
                                        const char *aClassName,
                                        const char *aContractID,
                                        nsIFactory *aFactory,
                                        PRBool aReplace)
{
    nsAutoMonitor mon(mMon);
#ifdef PR_LOGGING
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_WARNING))
    {
        char *buf = aClass.ToString();
        PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
               ("nsComponentManager: RegisterFactory(%s, %s)", buf,
                (aContractID ? aContractID : "(null)")));
        if (buf)
            PR_Free(buf);
    }
#endif
    nsFactoryEntry *entry = nsnull;
    nsFactoryTableEntry* factoryTableEntry = NS_STATIC_CAST(nsFactoryTableEntry*,
                                                            PL_DHashTableOperate(&mFactories,
                                                                                 &aClass,
                                                                                 PL_DHASH_ADD));

    if (!factoryTableEntry)
        return NS_ERROR_OUT_OF_MEMORY;


    if (PL_DHASH_ENTRY_IS_BUSY(factoryTableEntry)) {
        entry = factoryTableEntry->mFactoryEntry;
    }

    if (entry && !aReplace)
    {
        // Already registered
        PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
               ("\t\tFactory already registered."));
        return NS_ERROR_FACTORY_EXISTS;
    }

    void *mem;
    PL_ARENA_ALLOCATE(mem, &mArena, sizeof(nsFactoryEntry));
    if (!mem)
        return NS_ERROR_OUT_OF_MEMORY;

    entry = new (mem) nsFactoryEntry(aClass, aFactory, entry);

    factoryTableEntry->mFactoryEntry = entry;

    // Update the ContractID->CLSID Map
    if (aContractID) {
        nsresult rv = HashContractID(aContractID, strlen(aContractID), entry);
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

NS_IMETHODIMP
nsComponentManagerImpl::RegisterComponent(const nsCID &aClass,
                                          const char *aClassName,
                                          const char *aContractID,
                                          const char *aPersistentDescriptor,
                                          PRBool aReplace,
                                          PRBool aPersist)
{
    NS_ENSURE_ARG_POINTER(aPersistentDescriptor);
    return RegisterComponentCommon(aClass, aClassName,
                                   aContractID,
                                   aContractID ? strlen(aContractID) : 0,
                                   aPersistentDescriptor,
                                   strlen(aPersistentDescriptor),
                                   aReplace, aPersist,
                                   nativeComponentType);
}

NS_IMETHODIMP
nsComponentManagerImpl::RegisterComponentWithType(const nsCID &aClass,
                                                  const char *aClassName,
                                                  const char *aContractID,
                                                  nsIFile *aSpec,
                                                  const char *aLocation,
                                                  PRBool aReplace,
                                                  PRBool aPersist,
                                                  const char *aType)
{
    NS_ENSURE_ARG_POINTER(aLocation);
    return RegisterComponentCommon(aClass, aClassName,
                                   aContractID,
                                   aContractID ? strlen(aContractID) : 0,
                                   aLocation,
                                   strlen(aLocation),
                                   aReplace, aPersist,
                                   aType);
}

/*
 * Register a component, using whatever they stuck in the nsIFile.
 */
NS_IMETHODIMP
nsComponentManagerImpl::RegisterComponentSpec(const nsCID &aClass,
                                              const char *aClassName,
                                              const char *aContractID,
                                              nsIFile *aLibrarySpec,
                                              PRBool aReplace,
                                              PRBool aPersist)
{
    nsCAutoString registryName;
    nsresult rv = RegistryLocationForFile(aLibrarySpec,
                                          registryName);
    if (NS_FAILED(rv))
        return rv;

    rv = RegisterComponentWithType(aClass, aClassName,
                                   aContractID,
                                   aLibrarySpec,
                                   registryName.get(),
                                   aReplace, aPersist,
                                   nativeComponentType);
    return rv;
}

NS_IMETHODIMP
nsComponentManagerImpl::RegisterComponentLib(const nsCID &aClass,
                                             const char *aClassName,
                                             const char *aContractID,
                                             const char *aDllName,
                                             PRBool aReplace,
                                             PRBool aPersist)
{
    // deprecated and obsolete.
    return NS_ERROR_NOT_IMPLEMENTED;
}

/*
 * Add a component to the known universe of components.
 *
 * Once we enter this function, we own aRegistryName, and must free it
 * or hand it to nsFactoryEntry.  Common exit point ``out'' helps keep us
 * sane.
 */

nsresult
nsComponentManagerImpl::RegisterComponentCommon(const nsCID &aClass,
                                                const char *aClassName,
                                                const char *aContractID,
                                                PRUint32 aContractIDLen,
                                                const char *aRegistryName,
                                                PRUint32 aRegistryNameLen,
                                                PRBool aReplace,
                                                PRBool aPersist,
                                                const char *aType)
{
    nsresult rv;

    nsIDKey key(aClass);
    nsAutoMonitor mon(mMon);

    nsFactoryEntry *entry = GetFactoryEntry(aClass);

    // Normalize proid and classname
    const char *contractID = (aContractID && *aContractID) ? aContractID : nsnull;
#ifdef PR_LOGGING
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_WARNING))
    {
        char *buf = aClass.ToString();
        PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
               ("nsComponentManager: RegisterComponentCommon(%s, %s, %s, %s)",
                buf,
                contractID ? contractID : "(null)",
                aRegistryName, aType));
        if (buf)
            PR_Free(buf);
    }
#endif
    if (entry && !aReplace) {
        PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
               ("\t\tFactory already registered."));
        return NS_ERROR_FACTORY_EXISTS;
    }

    int typeIndex = GetLoaderType(aType);
    if (typeIndex == NS_LOADER_TYPE_INVALID)
        return NS_ERROR_OUT_OF_MEMORY;

    if (entry) {
        entry->ReInit(typeIndex, aRegistryName);
    }
    else {

        // Arena allocate the nsFactoryEntry
        void *mem;
        PL_ARENA_ALLOCATE(mem, &mArena, sizeof(nsFactoryEntry));
        if (!mem)
            return NS_ERROR_OUT_OF_MEMORY;

        mRegistryDirty = PR_TRUE;
        entry = new (mem) nsFactoryEntry(aClass,
                                         typeIndex,
                                         aRegistryName);
        if (!entry->mLocationKey)
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
        rv = HashContractID(contractID, aContractIDLen, entry);
        if (NS_FAILED(rv)) {
            PR_LOG(nsComponentManagerLog, PR_LOG_ERROR,
                   ("\t\tHashContractID(%s) FAILED\n", contractID));
            return rv;
        }
    }
    return NS_OK;
}


nsIModuleLoader*
nsComponentManagerImpl::LoaderForType(LoaderType aType)
{
    NS_ASSERTION(aType != NS_LOADER_TYPE_STATIC,
                 "Static component loader is special");

    if (aType == NS_LOADER_TYPE_INVALID)
        return nsnull;

    if (aType == NS_LOADER_TYPE_NATIVE)
        return &mNativeModuleLoader;

    NS_ASSERTION(aType >= 0 && aType < mLoaderData.Length(),
                 "LoaderType out of range");

    if (!mLoaderData[aType].loader) {
        nsCOMPtr<nsIModuleLoader> loader;
        loader = do_GetServiceFromCategory("module-loader",
                                           mLoaderData[aType].type.get());
        if (!loader)
            return nsnull;

        loader.swap(mLoaderData[aType].loader);
    }

    return mLoaderData[aType].loader;
}

void
nsComponentManagerImpl::GetAllLoaders()
{
    NS_ASSERTION(mCategoryManager, "nsComponentManager used uninitialized");

    nsCOMPtr<nsISimpleEnumerator> loaderEnum;
    mCategoryManager->EnumerateCategory("module-loader",
                                        getter_AddRefs(loaderEnum));
    nsCOMPtr<nsIUTF8StringEnumerator>
        loaderStrings(do_QueryInterface(loaderEnum));
    if (loaderStrings) {
        PRBool hasMore;
        while (NS_SUCCEEDED(loaderStrings->HasMore(&hasMore)) && hasMore) {
            nsCAutoString loaderType;
            if (NS_FAILED(loaderStrings->GetNext(loaderType)))
                continue;

            // We depend on the loader being created. Add the loader type and
            // create the loader object too.
            (void) LoaderForType(AddLoaderType(loaderType.get()));
        }
    }
}

// Convert a loader type string into an index into the component data
// array. Empty loader types are converted to NATIVE.
LoaderType
nsComponentManagerImpl::GetLoaderType(const char *typeStr)
{
    if (!typeStr || !*typeStr) {
        // Empty type strings are NATIVE
        return NS_LOADER_TYPE_NATIVE;
    }

    if (!strcmp(typeStr, staticComponentType))
        return NS_LOADER_TYPE_STATIC;

    if (!strcmp(typeStr, nativeComponentType))
        return NS_LOADER_TYPE_NATIVE;

    const nsDependentCString type(typeStr);

    for (int i=0; i < mLoaderData.Length(); ++i) {
        if (mLoaderData[i].type == type)
            return i;
    }

    return NS_LOADER_TYPE_INVALID;
}

// Add a loader type if not already known. Out the typeIndex
// if the loader type is either added or already there.
LoaderType
nsComponentManagerImpl::AddLoaderType(const char *typeStr)
{
    LoaderType typeIndex = GetLoaderType(typeStr);
    if (typeIndex != NS_LOADER_TYPE_INVALID)
        return typeIndex;

    // Add the loader type
    nsLoaderdata *elem = mLoaderData.AppendElement();
    if (!elem)
        return NS_LOADER_TYPE_INVALID;

    elem->type.Assign(typeStr);
    return mLoaderData.Length() - 1;
}

typedef struct
{
    const nsCID* cid;
    nsIFactory* factory;
} UnregisterConditions;

static PLDHashOperator PR_CALLBACK
DeleteFoundCIDs(PLDHashTable *aTable,
                PLDHashEntryHdr *aHdr,
                PRUint32 aNumber,
                void *aData)
{
    nsContractIDTableEntry* entry = NS_STATIC_CAST(nsContractIDTableEntry*, aHdr);

    if (!entry->mFactoryEntry)
        return PL_DHASH_NEXT;

    UnregisterConditions* data = (UnregisterConditions*)aData;

    nsFactoryEntry* factoryEntry = entry->mFactoryEntry;
    if (data->cid->Equals(factoryEntry->mCid) &&
        data->factory == factoryEntry->mFactory.get())
        return PL_DHASH_REMOVE;

    return PL_DHASH_NEXT;
}

void
nsComponentManagerImpl::DeleteContractIDEntriesByCID(const nsCID* aClass, nsIFactory* factory)
{
    UnregisterConditions aData;
    aData.cid     = aClass;
    aData.factory = factory;
    PL_DHashTableEnumerate(&mContractIDs, DeleteFoundCIDs, (void*)&aData);
}

nsresult
nsComponentManagerImpl::UnregisterFactory(const nsCID &aClass,
                                          nsIFactory *aFactory)
{
#ifdef PR_LOGGING
    if (PR_LOG_TEST(nsComponentManagerLog, PR_LOG_WARNING))
    {
        char *buf = aClass.ToString();
        PR_LOG(nsComponentManagerLog, PR_LOG_WARNING,
               ("nsComponentManager: UnregisterFactory(%s)", buf));
        if (buf)
            PR_Free(buf);
    }
#endif
    nsFactoryEntry *old;

    // first delete all contract id entries that are registered with this cid.
    DeleteContractIDEntriesByCID(&aClass, aFactory);

    // next check to see if there is a CID registered
    nsresult rv = NS_ERROR_FACTORY_NOT_REGISTERED;
    old = GetFactoryEntry(aClass);

    if (old && (old->mFactory.get() == aFactory))
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

NS_IMETHODIMP
nsComponentManagerImpl::UnregisterComponent(const nsCID &aClass,
                                            const char *registryName)
{
    NS_ERROR("Don't call nsIComponentManagerObsolete.unregisterComponent");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsComponentManagerImpl::UnregisterComponentSpec(const nsCID &aClass,
                                                nsIFile *aLibrarySpec)
{
    NS_ERROR("Don't call nsIComponentManagerObsolete.unregisterComponentSpec");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsComponentManagerImpl::FreeLibraries(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

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

NS_IMETHODIMP
nsComponentManagerImpl::AutoRegister(PRInt32 when, nsIFile *inDirSpec)
{
    return AutoRegister(inDirSpec);
}

nsresult
nsComponentManagerImpl::AutoRegisterImpl(nsIFile   *inDirSpec,
                          nsCOMArray<nsILocalFile> &aLeftovers,
                          nsTArray<DeferredModule> &aDeferred)
{
    NS_ASSERTION(inDirSpec, "inDirSpec must not be null");

    nsresult rv;

    PRBool isDir;
    rv = inDirSpec->IsDirectory(&isDir);
    if (NS_FAILED(rv))
        return rv;

    if (isDir)
        return AutoRegisterDirectory(inDirSpec, aLeftovers, aDeferred);

    nsCOMPtr<nsILocalFile> lf(do_QueryInterface(inDirSpec));
    if (!lf)
        return NS_NOINTERFACE;

    rv = AutoRegisterComponent(lf, aDeferred);
    if (NS_FAILED(rv))
        aLeftovers.AppendObject(lf);
    return rv;
}

nsresult
nsComponentManagerImpl::AutoRegisterDirectory(nsIFile *inDirSpec,
                          nsCOMArray<nsILocalFile>    &aLeftovers,
                          nsTArray<DeferredModule>    &aDeferred)
{
    nsCOMPtr<nsISimpleEnumerator> entries;
    nsresult rv = inDirSpec->GetDirectoryEntries(getter_AddRefs(entries));
    if (NS_FAILED(rv))
        return rv;

    PRBool hasMore;
    nsCOMPtr<nsISupports> elem;

    while (NS_SUCCEEDED(entries->HasMoreElements(&hasMore)) && hasMore) {
        entries->GetNext(getter_AddRefs(elem));
        nsCOMPtr<nsILocalFile> lf(do_QueryInterface(elem));
        if (!lf) {
            NS_ERROR("Directory enumerator misbehaving");
            continue;
        }

        PRBool isDir;
        rv = lf->IsDirectory(&isDir);
        if (NS_FAILED(rv))
            continue;

        if (isDir)
            AutoRegisterDirectory(lf, aLeftovers, aDeferred);
        else {
            rv = AutoRegisterComponent(lf, aDeferred);
            if (NS_FAILED(rv))
                aLeftovers.AppendObject(lf);
        }
    }

    return NS_OK;
}

nsresult
nsComponentManagerImpl::AutoRegisterComponent(PRInt32 unused,
                                              nsIFile *component)
{
    nsCOMPtr<nsILocalFile> lf(do_QueryInterface(component));
    if (!lf)
        return NS_NOINTERFACE;

    GetAllLoaders();

    nsTArray<DeferredModule> deferred;

    nsresult rv = AutoRegisterComponent(lf, deferred);
    if (deferred.Length())
        return NS_ERROR_FACTORY_REGISTER_AGAIN;

    return rv;
}

nsresult
nsComponentManagerImpl::AutoRegisterComponent(nsILocalFile*  aComponentFile,
                                   nsTArray<DeferredModule> &aDeferred,
                                   LoaderType                minLoader)
{
    nsresult rv;

    NS_ASSERTION(minLoader < GetLoaderCount(), "Bad minLoader");

    nsCAutoString registryLocation;
    rv = RegistryLocationForFile(aComponentFile, registryLocation);
    if (NS_FAILED(rv))
        return rv;

    const nsDependentCSubstring extension = StringTail(registryLocation, 4);
    if (extension.LowerCaseEqualsLiteral(".dat") ||
        extension.LowerCaseEqualsLiteral(".xpt"))
        return NS_OK;

    nsCOMPtr<nsIHashable> lfhash(do_QueryInterface(aComponentFile));
    if (!lfhash) {
        NS_ERROR("localfile not implementing nsIHashable!");
        return NS_NOINTERFACE;
    }

    PRInt64 modTime = 0;
    if (NS_SUCCEEDED(aComponentFile->GetLastModifiedTime(&modTime))) {
        PRInt64 cachedModTime;
        if (mAutoRegEntries.Get(lfhash, &cachedModTime) &&
            cachedModTime == modTime)
            return NS_OK;
    }

    const char *registryType = nsnull;

    nsCOMPtr<nsIModule> module;

    if (minLoader == NS_LOADER_TYPE_NATIVE) {
        rv = mNativeModuleLoader.LoadModule(aComponentFile,
                                            getter_AddRefs(module));
        if (NS_SUCCEEDED(rv)) {
            if (!module) {
                NS_ERROR("Module loader succeeded without returning a module");
                return NS_ERROR_FAILURE;
            }
            registryType = nativeComponentType;
        }

        minLoader = 0;
    }

    if (!registryType) {
        for (; minLoader < GetLoaderCount(); ++minLoader) {
            nsIModuleLoader* loader = LoaderForType(minLoader);
            if (!loader)
                continue;

            rv = loader->LoadModule(aComponentFile, getter_AddRefs(module));
            if (NS_SUCCEEDED(rv)) {
                if (!module) {
                    NS_ERROR("Module loader succeeded without returning a module.");
                    return NS_ERROR_FAILURE;
                }
                registryType = StringForLoaderType(minLoader);
                break;
            }
        }
        if (!registryType) {
            return NS_ERROR_FAILURE;
        }
    }

    rv = module->RegisterSelf(this, aComponentFile, registryLocation.get(),
                              registryType);
    if (NS_ERROR_FACTORY_REGISTER_AGAIN == rv) {
        DeferredModule *d = aDeferred.AppendElement();
        if (!d)
            return NS_ERROR_OUT_OF_MEMORY;

        d->type = registryType;
        d->file = aComponentFile;
        d->location = registryLocation;
        d->module = module;
        d->modTime = modTime;
        return NS_OK;
    }

    if (NS_SUCCEEDED(rv) && modTime != 0)
        mAutoRegEntries.Put(lfhash, modTime);

    return rv;
}

void
nsComponentManagerImpl::LoadLeftoverComponents(
  nsCOMArray<nsILocalFile> &aLeftovers,
  nsTArray<DeferredModule> &aDeferred,
  LoaderType                minLoader)
{
    NS_ASSERTION(minLoader >= GetLoaderCount(), "Corrupted minLoader");

    GetAllLoaders();

    // If there aren't any new loaders found since we tried this last, there's
    // nothing left to do.
    if (GetLoaderCount() == minLoader)
        return;

    LoaderType curLoader = GetLoaderCount();

    for (PRInt32 i = aLeftovers.Count() - 1; i >= 0; --i) {
        nsresult rv = AutoRegisterComponent(aLeftovers[i], aDeferred,
                                            minLoader);
        if (NS_SUCCEEDED(rv))
            aLeftovers.RemoveObjectAt(i);
    }
    if (aLeftovers.Count())
        // recursively try this again until there are no new loaders found
        LoadLeftoverComponents(aLeftovers, aDeferred, curLoader);
}

void
nsComponentManagerImpl::LoadDeferredModules(nsTArray<DeferredModule> &aDeferred)
{
    // We keep looping through deferred components until one of
    // 1) they're all gone
    // 2) we loop through and none of them succeed

    PRInt32 lastCount = PR_INT32_MAX;
    while (aDeferred.Length() > 0 &&
           lastCount > aDeferred.Length()) {

        lastCount = aDeferred.Length();

        for (PRInt32 i = aDeferred.Length() - 1; i >= 0; --i) {
            DeferredModule &d = aDeferred[i];
            nsresult rv = d.module->RegisterSelf(this,
                                                 d.file,
                                                 d.location.get(),
                                                 d.type);
            if (NS_SUCCEEDED(rv) && d.modTime != 0) {
                nsCOMPtr<nsIHashable> lfhash(do_QueryInterface(d.file));
                if (lfhash)
                    mAutoRegEntries.Put(lfhash, d.modTime);
            }

            if (rv != NS_ERROR_FACTORY_REGISTER_AGAIN) {
                aDeferred.RemoveElementAt(i);
            }
        }
    }
}

NS_IMETHODIMP
nsComponentManagerImpl::AutoUnregisterComponent(PRInt32 /* unused */,
                                                nsIFile *component)
{
    nsresult rv;

    GetAllLoaders();

    nsCAutoString location;
    rv = RegistryLocationForFile(component, location);
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsILocalFile> lf(do_QueryInterface(component));
    if (!lf)
        return NS_NOINTERFACE;

    nsCOMPtr<nsIModule> module;
    rv = mNativeModuleLoader.LoadModule(lf, getter_AddRefs(module));
    if (NS_FAILED(rv)) {
        for (LoaderType i = 0; i < mLoaderData.Length(); ++i) {
            nsIModuleLoader* loader = LoaderForType(i);
            if (!loader)
                continue;

            if (NS_SUCCEEDED(loader->LoadModule(lf, getter_AddRefs(module))) &&
                module) {
                break;
            }
        }
    }

    if (!module)
        return NS_ERROR_FAILURE;

    rv = module->UnregisterSelf(this, lf, location.get());

    nsCOMPtr<nsIHashable> lfhash(do_QueryInterface(lf));
    mAutoRegEntries.Remove(lfhash);

    return rv;
}

NS_IMETHODIMP
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

NS_IMETHODIMP
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

NS_IMETHODIMP
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

static void
RegisterStaticModule(const char *key, nsIModule* module,
                     nsTArray<DeferredModule> &deferred)
{
    nsresult rv = module->
        RegisterSelf(nsComponentManagerImpl::gComponentManager,
                     nsnull, key, staticComponentType);

    if (NS_ERROR_FACTORY_REGISTER_AGAIN == rv) {
        DeferredModule *d = deferred.AppendElement();
        if (d) {
            d->type = staticComponentType;
            d->location = key;
        }
    }
}

static void
ReportLoadFailure(nsIFile* aFile, nsIConsoleService* aCS)
{
    nsAutoString message;
    aFile->GetPath(message);
    message.Insert(NS_LITERAL_STRING("Failed to load XPCOM component: "), 0);

    aCS->LogStringMessage(message.get());
}

NS_IMETHODIMP
nsComponentManagerImpl::AutoRegister(nsIFile *aSpec)
{
    nsresult rv;

    if (!mCategoryManager) {
        mCategoryManager = do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
        if (NS_FAILED(rv))
            return rv;
    }

    GetAllLoaders();

    // Notify observers of xpcom autoregistration start
    NS_CreateServicesFromCategory(NS_XPCOM_AUTOREGISTRATION_OBSERVER_ID,
                                  aSpec, "start");

    nsCOMArray<nsILocalFile> leftovers;
    nsTArray<DeferredModule> deferred;

    if (!aSpec)
        mStaticModuleLoader.EnumerateModules(RegisterStaticModule,
                                             deferred);

    LoaderType curLoader = GetLoaderCount();

    if (aSpec) {
        rv = AutoRegisterImpl(aSpec, leftovers, deferred);
    }
    else {
        // register static components, then GRE components, then
        // NS_XPCOM_COMPONENT_DIR, then NS_XPCOM_COMPONENT_DIR_LIST

        PRBool equals = PR_FALSE;

        if (mGREComponentsDir &&
            NS_SUCCEEDED(mGREComponentsDir->Equals(mComponentsDir, &equals)) &&
            !equals) {
            rv = AutoRegisterImpl(mGREComponentsDir, leftovers, deferred);
            if (NS_FAILED(rv)) {
                NS_WARNING("Couldn't register mGREComponentsDir");
            }
        }

        rv = AutoRegisterImpl(mComponentsDir, leftovers, deferred);
        if (NS_FAILED(rv)) {
            NS_WARNING("Couldn't register mComponentsDir");
        }

        nsCOMPtr<nsISimpleEnumerator> dirList;
        rv = nsDirectoryService::gService->Get(NS_XPCOM_COMPONENT_DIR_LIST,
                                               NS_GET_IID(nsISimpleEnumerator),
                                               getter_AddRefs(dirList));
        if (NS_SUCCEEDED(rv) && dirList) {
            PRBool hasMore;
            nsCOMPtr<nsISupports> elem;

            while (NS_SUCCEEDED(dirList->HasMoreElements(&hasMore)) &&
                   hasMore) {
                dirList->GetNext(getter_AddRefs(elem));
                nsCOMPtr<nsIFile> dir(do_QueryInterface(elem));
                if (dir) {
                    AutoRegisterImpl(dir, leftovers, deferred);
                }
            }
        }

        rv = NS_OK;
    }

    if (NS_SUCCEEDED(rv)) {
        // We may have found new component loaders in the meantime, try to go
        // back and load the leftovers.
        if (leftovers.Count())
            LoadLeftoverComponents(leftovers, deferred, curLoader);

        // If a module said its loading should be deferred, register it now
        if (deferred.Length())
            LoadDeferredModules(deferred);

        nsCOMPtr<nsIConsoleService>
            cs(do_GetService(NS_CONSOLESERVICE_CONTRACTID));

        if (cs) {
            for (PRInt32 i = 0; i < leftovers.Count(); ++i) {
                ReportLoadFailure(leftovers[i], cs);
            }
        }
    }

    // Notify observers of xpcom autoregistration completion
    NS_CreateServicesFromCategory(NS_XPCOM_AUTOREGISTRATION_OBSERVER_ID,
                                  aSpec,
                                  "end");

    if (mRegistryDirty)
        WritePersistentRegistry();

    return rv;
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
    nsCAutoString registryName;

    if (!loaderStr)
    {
        nsresult rv = RegistryLocationForFile(aFile, registryName);
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
    nsFactoryEntry *entry = GetFactoryEntry(aClass, strlen(aClass));

    if (entry)
        *_retval = PR_TRUE;
    else
        *_retval = PR_FALSE;
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

////////////////////////////////////////////////////////////////////////////////
// nsFactoryEntry
////////////////////////////////////////////////////////////////////////////////

nsFactoryEntry::nsFactoryEntry(const nsCID    &aClass,
                               LoaderType      aLoaderType,
                               const char     *aLocationKey,
                               nsFactoryEntry *aParent) :
    mCid(aClass),
    mLoaderType(aLoaderType),
    mLocationKey(
      ArenaStrdup(aLocationKey,
                  &nsComponentManagerImpl::gComponentManager->mArena)),
    mParent(aParent)
{
}

void
nsFactoryEntry::ReInit(LoaderType  aLoaderType,
                       const char *aLocationKey)
{
    mLoaderType = aLoaderType;

    if (!mLocationKey || strcmp(mLocationKey, aLocationKey)) {
        mLocationKey =
            ArenaStrdup(aLocationKey,
                        &nsComponentManagerImpl::gComponentManager->mArena);
    }
}

nsresult
nsFactoryEntry::GetFactory(nsIFactory **aFactory)
{
    if (!mFactory) {
        nsresult rv;

        if (mLoaderType == NS_LOADER_TYPE_INVALID)
            return NS_ERROR_FAILURE;

        nsCOMPtr<nsIModule> module;

        if (mLoaderType == NS_LOADER_TYPE_STATIC) {
            rv = nsComponentManagerImpl::gComponentManager->
                mStaticModuleLoader.
                GetModuleFor(mLocationKey,
                             getter_AddRefs(module));
        }
        else {
            nsCOMPtr<nsILocalFile> moduleFile;
            rv = nsComponentManagerImpl::gComponentManager->
                FileForRegistryLocation(nsDependentCString(mLocationKey),
                                        getter_AddRefs(moduleFile));
            NS_ENSURE_SUCCESS(rv, rv);

            nsIModuleLoader* loader =
                nsComponentManagerImpl::gComponentManager->
                    LoaderForType(mLoaderType);
            if (!loader)
                return NS_ERROR_FAILURE;

            rv = loader->LoadModule(moduleFile,
                                    getter_AddRefs(module));
        }

        if (NS_FAILED(rv))
            return rv;

        if (!module) {
            NS_ERROR("Module returned success but no module!");
            return NS_ERROR_FAILURE;
        }

        rv = module->
            GetClassObject(nsComponentManagerImpl::gComponentManager,
                           mCid,
                           NS_GET_IID(nsIFactory),
                           getter_AddRefs(mFactory));
        if (NS_FAILED(rv))
            return rv;

        NS_ASSERTION(mFactory,
                     "Loader says it succeeded; got null factory!");
        if (!mFactory)
            return NS_ERROR_UNEXPECTED;
    }
    *aFactory = mFactory.get();
    NS_ADDREF(*aFactory);
    return NS_OK;
}

nsFactoryEntry::~nsFactoryEntry()
{
    // nsFactoryEntry is arena-allocated. So we don't delete it;
    // call the destructor by hand.

    if (mParent)
        mParent->~nsFactoryEntry();
}

////////////////////////////////////////////////////////////////////////////////
// Static Access Functions
////////////////////////////////////////////////////////////////////////////////

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
