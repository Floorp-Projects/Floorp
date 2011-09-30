/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike McCabe <mccabe@netscape.com>
 *   John Bandhauer <jband@netscape.com>
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
 * ***** END LICENSE BLOCK ***** */

/* Implementation of xptiInterfaceInfoManager. */

#include "xptiprivate.h"
#include "nsDependentString.h"
#include "nsString.h"
#include "nsISupportsArray.h"
#include "nsArrayEnumerator.h"
#include "mozilla/FunctionTimer.h"
#include "nsDirectoryService.h"

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

namespace {

struct AutoCloseFD
{
    AutoCloseFD()
        : mFD(NULL)
    { }
    ~AutoCloseFD() {
        if (mFD)
            PR_Close(mFD);
    }
    operator PRFileDesc*() {
        return mFD;
    }

    PRFileDesc** operator&() {
        NS_ASSERTION(!mFD, "Re-opening a file");
        return &mFD;
    }

    PRFileDesc* mFD;
};

} // anonymous namespace

XPTHeader* 
xptiInterfaceInfoManager::ReadXPTFile(nsILocalFile* aFile)
{
    AutoCloseFD fd;
    if (NS_FAILED(aFile->OpenNSPRFileDesc(PR_RDONLY, 0444, &fd)) || !fd)
        return NULL;

    PRFileInfo64 fileInfo;
    if (PR_SUCCESS != PR_GetOpenFileInfo64(fd, &fileInfo))
        return NULL;

    if (fileInfo.size > PRInt64(PR_INT32_MAX))
        return NULL;

    nsAutoArrayPtr<char> whole(new char[PRInt32(fileInfo.size)]);
    if (!whole)
        return nsnull;

    for (PRInt32 totalRead = 0; totalRead < fileInfo.size; ) {
        PRInt32 read = PR_Read(fd, whole + totalRead, PRInt32(fileInfo.size));
        if (read < 0)
            return NULL;
        totalRead += read;
    }

    XPTState* state = XPT_NewXDRState(XPT_DECODE, whole,
                                      PRInt32(fileInfo.size));

    XPTCursor cursor;
    if (!XPT_MakeCursor(state, XPT_HEADER, 0, &cursor)) {
        XPT_DestroyXDRState(state);
        return NULL;
    }
    
    XPTHeader *header = nsnull;
    if (!XPT_DoHeader(gXPTIStructArena, &cursor, &header)) {
        XPT_DestroyXDRState(state);
        return NULL;
    }

    XPT_DestroyXDRState(state);

    return header;
}

XPTHeader*
xptiInterfaceInfoManager::ReadXPTFileFromInputStream(nsIInputStream *stream)
{
    PRUint32 flen;
    stream->Available(&flen);
    
    nsAutoArrayPtr<char> whole(new char[flen]);
    if (!whole)
        return nsnull;

    for (PRUint32 totalRead = 0; totalRead < flen; ) {
        PRUint32 avail;
        PRUint32 read;

        if (NS_FAILED(stream->Available(&avail)))
            return NULL;

        if (avail > flen)
            return NULL;

        if (NS_FAILED(stream->Read(whole+totalRead, avail, &read)))
            return NULL;

        totalRead += read;
    }
    
    XPTState *state = XPT_NewXDRState(XPT_DECODE, whole, flen);
    if (!state)
        return NULL;
    
    XPTCursor cursor;
    if (!XPT_MakeCursor(state, XPT_HEADER, 0, &cursor)) {
        XPT_DestroyXDRState(state);
        return NULL;
    }
    
    XPTHeader *header = nsnull;
    if (!XPT_DoHeader(gXPTIStructArena, &cursor, &header)) {
        XPT_DestroyXDRState(state);
        return NULL;
    }

    XPT_DestroyXDRState(state);

    return header;
}

void
xptiInterfaceInfoManager::RegisterFile(nsILocalFile* aFile)
{
    XPTHeader* header = ReadXPTFile(aFile);
    if (!header)
        return;

    RegisterXPTHeader(header);
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
xptiInterfaceInfoManager::RegisterInputStream(nsIInputStream* aStream)
{
    XPTHeader* header = ReadXPTFileFromInputStream(aStream);
    if (header)
        RegisterXPTHeader(header);
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
