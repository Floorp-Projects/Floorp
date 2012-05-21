/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implementation of xptiInterfaceInfoManager. */

#include "xptiprivate.h"
#include "nsDependentString.h"
#include "nsString.h"
#include "nsISupportsArray.h"
#include "nsArrayEnumerator.h"
#include "mozilla/FunctionTimer.h"
#include "nsDirectoryService.h"
#include "mozilla/FileUtils.h"

using namespace mozilla;

NS_IMPL_THREADSAFE_ISUPPORTS2(xptiInterfaceInfoManager, 
                              nsIInterfaceInfoManager,
                              nsIInterfaceInfoSuperManager)

static xptiInterfaceInfoManager* gInterfaceInfoManager = nsnull;
#ifdef DEBUG
static int gCallCount = 0;
#endif

// static
xptiInterfaceInfoManager*
xptiInterfaceInfoManager::GetSingleton()
{
    if (!gInterfaceInfoManager) {
        NS_TIME_FUNCTION;

        gInterfaceInfoManager = new xptiInterfaceInfoManager();
        NS_ADDREF(gInterfaceInfoManager);
    }
    return gInterfaceInfoManager;
}

void
xptiInterfaceInfoManager::FreeInterfaceInfoManager()
{
    NS_IF_RELEASE(gInterfaceInfoManager);
}

xptiInterfaceInfoManager::xptiInterfaceInfoManager()
    :   mWorkingSet(),
        mResolveLock("xptiInterfaceInfoManager.mResolveLock"),
        mAdditionalManagersLock(
            "xptiInterfaceInfoManager.mAdditionalManagersLock")
{
}

xptiInterfaceInfoManager::~xptiInterfaceInfoManager()
{
    // We only do this on shutdown of the service.
    mWorkingSet.InvalidateInterfaceInfos();

    gInterfaceInfoManager = nsnull;
#ifdef DEBUG
    gCallCount = 0;
#endif
}

void
xptiInterfaceInfoManager::RegisterBuffer(char *buf, PRUint32 length)
{
    XPTState *state = XPT_NewXDRState(XPT_DECODE, buf, length);
    if (!state)
        return;

    XPTCursor cursor;
    if (!XPT_MakeCursor(state, XPT_HEADER, 0, &cursor)) {
        XPT_DestroyXDRState(state);
        return;
    }

    XPTHeader *header = nsnull;
    if (XPT_DoHeader(gXPTIStructArena, &cursor, &header)) {
        RegisterXPTHeader(header);
    }

    XPT_DestroyXDRState(state);
}

void
xptiInterfaceInfoManager::RegisterXPTHeader(XPTHeader* aHeader)
{
    if (aHeader->major_version >= XPT_MAJOR_INCOMPATIBLE_VERSION) {
        NS_ASSERTION(!aHeader->num_interfaces,"bad libxpt");
        LOG_AUTOREG(("      file is version %d.%d  Type file of version %d.0 or higher can not be read.\n", (int)header->major_version, (int)header->minor_version, (int)XPT_MAJOR_INCOMPATIBLE_VERSION));
    }

    xptiTypelibGuts* typelib = xptiTypelibGuts::Create(aHeader);

    ReentrantMonitorAutoEnter monitor(mWorkingSet.mTableReentrantMonitor);
    for(PRUint16 k = 0; k < aHeader->num_interfaces; k++)
        VerifyAndAddEntryIfNew(aHeader->interface_directory + k, k, typelib);
}

void
xptiInterfaceInfoManager::VerifyAndAddEntryIfNew(XPTInterfaceDirectoryEntry* iface,
                                                 PRUint16 idx,
                                                 xptiTypelibGuts* typelib)
{
    if (!iface->interface_descriptor)
        return;

    // The number of maximum methods is not arbitrary. It is the same value as
    // in xpcom/reflect/xptcall/public/genstubs.pl; do not change this value
    // without changing that one or you WILL see problems.
    if (iface->interface_descriptor->num_methods > 250 &&
            !(XPT_ID_IS_BUILTINCLASS(iface->interface_descriptor->flags))) {
        NS_ASSERTION(0, "Too many methods to handle for the stub, cannot load");
        fprintf(stderr, "ignoring too large interface: %s\n", iface->name);
        return;
    }
    
    mWorkingSet.mTableReentrantMonitor.AssertCurrentThreadIn();
    xptiInterfaceEntry* entry = mWorkingSet.mIIDTable.Get(iface->iid);
    if (entry) {
        // XXX validate this info to find possible inconsistencies
        LOG_AUTOREG(("      ignoring repeated interface: %s\n", iface->name));
        return;
    }
    
    // Build a new xptiInterfaceEntry object and hook it up. 

    entry = xptiInterfaceEntry::Create(iface->name,
                                       iface->iid,
                                       iface->interface_descriptor,
                                       typelib);
    if (!entry)
        return;

    //XXX  We should SetHeader too as part of the validation, no?
    entry->SetScriptableFlag(XPT_ID_IS_SCRIPTABLE(iface->interface_descriptor->flags));
    entry->SetBuiltinClassFlag(XPT_ID_IS_BUILTINCLASS(iface->interface_descriptor->flags));

    mWorkingSet.mIIDTable.Put(entry->IID(), entry);
    mWorkingSet.mNameTable.Put(entry->GetTheName(), entry);

    typelib->SetEntryAt(idx, entry);

    LOG_AUTOREG(("      added interface: %s\n", iface->name));
}

// this is a private helper
static nsresult 
EntryToInfo(xptiInterfaceEntry* entry, nsIInterfaceInfo **_retval)
{
    xptiInterfaceInfo* info;
    nsresult rv;

    if (!entry) {
        *_retval = nsnull;
        return NS_ERROR_FAILURE;    
    }

    rv = entry->GetInterfaceInfo(&info);
    if (NS_FAILED(rv))
        return rv;

    // Transfer the AddRef done by GetInterfaceInfo.
    *_retval = static_cast<nsIInterfaceInfo*>(info);
    return NS_OK;    
}

xptiInterfaceEntry*
xptiInterfaceInfoManager::GetInterfaceEntryForIID(const nsIID *iid)
{
    ReentrantMonitorAutoEnter monitor(mWorkingSet.mTableReentrantMonitor);
    return mWorkingSet.mIIDTable.Get(*iid);
}

/* nsIInterfaceInfo getInfoForIID (in nsIIDPtr iid); */
NS_IMETHODIMP xptiInterfaceInfoManager::GetInfoForIID(const nsIID * iid, nsIInterfaceInfo **_retval)
{
    NS_ASSERTION(iid, "bad param");
    NS_ASSERTION(_retval, "bad param");

    ReentrantMonitorAutoEnter monitor(mWorkingSet.mTableReentrantMonitor);
    xptiInterfaceEntry* entry = mWorkingSet.mIIDTable.Get(*iid);
    return EntryToInfo(entry, _retval);
}

/* nsIInterfaceInfo getInfoForName (in string name); */
NS_IMETHODIMP xptiInterfaceInfoManager::GetInfoForName(const char *name, nsIInterfaceInfo **_retval)
{
    NS_ASSERTION(name, "bad param");
    NS_ASSERTION(_retval, "bad param");

    ReentrantMonitorAutoEnter monitor(mWorkingSet.mTableReentrantMonitor);
    xptiInterfaceEntry* entry = mWorkingSet.mNameTable.Get(name);
    return EntryToInfo(entry, _retval);
}

/* nsIIDPtr getIIDForName (in string name); */
NS_IMETHODIMP xptiInterfaceInfoManager::GetIIDForName(const char *name, nsIID * *_retval)
{
    NS_ASSERTION(name, "bad param");
    NS_ASSERTION(_retval, "bad param");

    ReentrantMonitorAutoEnter monitor(mWorkingSet.mTableReentrantMonitor);
    xptiInterfaceEntry* entry = mWorkingSet.mNameTable.Get(name);
    if (!entry) {
        *_retval = nsnull;
        return NS_ERROR_FAILURE;    
    }

    return entry->GetIID(_retval);
}

/* string getNameForIID (in nsIIDPtr iid); */
NS_IMETHODIMP xptiInterfaceInfoManager::GetNameForIID(const nsIID * iid, char **_retval)
{
    NS_ASSERTION(iid, "bad param");
    NS_ASSERTION(_retval, "bad param");

    ReentrantMonitorAutoEnter monitor(mWorkingSet.mTableReentrantMonitor);
    xptiInterfaceEntry* entry = mWorkingSet.mIIDTable.Get(*iid);
    if (!entry) {
        *_retval = nsnull;
        return NS_ERROR_FAILURE;    
    }

    return entry->GetName(_retval);
}

static PLDHashOperator
xpti_ArrayAppender(const char* name, xptiInterfaceEntry* entry, void* arg)
{
    nsISupportsArray* array = (nsISupportsArray*) arg;

    nsCOMPtr<nsIInterfaceInfo> ii;
    if (NS_SUCCEEDED(EntryToInfo(entry, getter_AddRefs(ii))))
        array->AppendElement(ii);
    return PL_DHASH_NEXT;
}

/* nsIEnumerator enumerateInterfaces (); */
NS_IMETHODIMP xptiInterfaceInfoManager::EnumerateInterfaces(nsIEnumerator **_retval)
{
    // I didn't want to incur the size overhead of using nsHashtable just to
    // make building an enumerator easier. So, this code makes a snapshot of 
    // the table using an nsISupportsArray and builds an enumerator for that.
    // We can afford this transient cost.

    nsCOMPtr<nsISupportsArray> array;
    NS_NewISupportsArray(getter_AddRefs(array));
    if (!array)
        return NS_ERROR_UNEXPECTED;

    ReentrantMonitorAutoEnter monitor(mWorkingSet.mTableReentrantMonitor);
    mWorkingSet.mNameTable.EnumerateRead(xpti_ArrayAppender, array);

    return array->Enumerate(_retval);
}

struct ArrayAndPrefix
{
    nsISupportsArray* array;
    const char*       prefix;
    PRUint32          length;
};

static PLDHashOperator
xpti_ArrayPrefixAppender(const char* keyname, xptiInterfaceEntry* entry, void* arg)
{
    ArrayAndPrefix* args = (ArrayAndPrefix*) arg;

    const char* name = entry->GetTheName();
    if (name != PL_strnstr(name, args->prefix, args->length))
        return PL_DHASH_NEXT;

    nsCOMPtr<nsIInterfaceInfo> ii;
    if (NS_SUCCEEDED(EntryToInfo(entry, getter_AddRefs(ii))))
        args->array->AppendElement(ii);
    return PL_DHASH_NEXT;
}

/* nsIEnumerator enumerateInterfacesWhoseNamesStartWith (in string prefix); */
NS_IMETHODIMP xptiInterfaceInfoManager::EnumerateInterfacesWhoseNamesStartWith(const char *prefix, nsIEnumerator **_retval)
{
    nsCOMPtr<nsISupportsArray> array;
    NS_NewISupportsArray(getter_AddRefs(array));
    if (!array)
        return NS_ERROR_UNEXPECTED;

    ReentrantMonitorAutoEnter monitor(mWorkingSet.mTableReentrantMonitor);
    ArrayAndPrefix args = {array, prefix, PL_strlen(prefix)};
    mWorkingSet.mNameTable.EnumerateRead(xpti_ArrayPrefixAppender, &args);

    return array->Enumerate(_retval);
}

/* void autoRegisterInterfaces (); */
NS_IMETHODIMP xptiInterfaceInfoManager::AutoRegisterInterfaces()
{
    NS_TIME_FUNCTION;

    return NS_ERROR_NOT_IMPLEMENTED;
}

/***************************************************************************/

/* void addAdditionalManager (in nsIInterfaceInfoManager manager); */
NS_IMETHODIMP xptiInterfaceInfoManager::AddAdditionalManager(nsIInterfaceInfoManager *manager)
{
    nsCOMPtr<nsIWeakReference> weakRef = do_GetWeakReference(manager);
    nsISupports* ptrToAdd = weakRef ? 
                    static_cast<nsISupports*>(weakRef) :
                    static_cast<nsISupports*>(manager);
    { // scoped lock...
        MutexAutoLock lock(mAdditionalManagersLock);
        if (mAdditionalManagers.IndexOf(ptrToAdd) != -1)
            return NS_ERROR_FAILURE;
        if (!mAdditionalManagers.AppendObject(ptrToAdd))
            return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
}

/* void removeAdditionalManager (in nsIInterfaceInfoManager manager); */
NS_IMETHODIMP xptiInterfaceInfoManager::RemoveAdditionalManager(nsIInterfaceInfoManager *manager)
{
    nsCOMPtr<nsIWeakReference> weakRef = do_GetWeakReference(manager);
    nsISupports* ptrToRemove = weakRef ? 
                    static_cast<nsISupports*>(weakRef) :
                    static_cast<nsISupports*>(manager);
    { // scoped lock...
        MutexAutoLock lock(mAdditionalManagersLock);
        if (!mAdditionalManagers.RemoveObject(ptrToRemove))
            return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

/* bool hasAdditionalManagers (); */
NS_IMETHODIMP xptiInterfaceInfoManager::HasAdditionalManagers(bool *_retval)
{
    *_retval = mAdditionalManagers.Count() > 0;
    return NS_OK;
}

/* nsISimpleEnumerator enumerateAdditionalManagers (); */
NS_IMETHODIMP xptiInterfaceInfoManager::EnumerateAdditionalManagers(nsISimpleEnumerator **_retval)
{
    MutexAutoLock lock(mAdditionalManagersLock);

    nsCOMArray<nsISupports> managerArray(mAdditionalManagers);
    /* Resolve all the weak references in the array. */
    for(PRInt32 i = managerArray.Count(); i--; ) {
        nsISupports *raw = managerArray.ObjectAt(i);
        if (!raw)
            return NS_ERROR_FAILURE;
        nsCOMPtr<nsIWeakReference> weakRef = do_QueryInterface(raw);
        if (weakRef) {
            nsCOMPtr<nsIInterfaceInfoManager> manager = 
                do_QueryReferent(weakRef);
            if (manager) {
                if (!managerArray.ReplaceObjectAt(manager, i))
                    return NS_ERROR_FAILURE;
            }
            else {
                // The manager is no more. Remove the element.
                mAdditionalManagers.RemoveObjectAt(i);
                managerArray.RemoveObjectAt(i);
            }
        }
    }
    
    return NS_NewArrayEnumerator(_retval, managerArray);
}
