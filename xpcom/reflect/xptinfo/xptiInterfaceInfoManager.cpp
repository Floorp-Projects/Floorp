/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implementation of xptiInterfaceInfoManager. */

#include "mozilla/XPTInterfaceInfoManager.h"

#include "mozilla/FileUtils.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/StaticPtr.h"

#include "xptiprivate.h"
#include "nsDependentString.h"
#include "nsString.h"
#include "nsArrayEnumerator.h"
#include "nsDirectoryService.h"
#include "nsIMemoryReporter.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS(
  XPTInterfaceInfoManager,
  nsIInterfaceInfoManager,
  nsIMemoryReporter)

static StaticRefPtr<XPTInterfaceInfoManager> gInterfaceInfoManager;

size_t
XPTInterfaceInfoManager::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
{
    size_t n = aMallocSizeOf(this);
    ReentrantMonitorAutoEnter monitor(mWorkingSet.mTableReentrantMonitor);
    // The entries themselves are allocated out of an arena accounted
    // for elsewhere, so don't measure them
    n += mWorkingSet.mIIDTable.ShallowSizeOfExcludingThis(aMallocSizeOf);
    n += mWorkingSet.mNameTable.ShallowSizeOfExcludingThis(aMallocSizeOf);
    return n;
}

MOZ_DEFINE_MALLOC_SIZE_OF(XPTIMallocSizeOf)

NS_IMETHODIMP
XPTInterfaceInfoManager::CollectReports(nsIHandleReportCallback* aHandleReport,
                                        nsISupports* aData, bool aAnonymize)
{
    size_t amount = SizeOfIncludingThis(XPTIMallocSizeOf);

    // Measure gXPTIStructArena here, too.  This is a bit grotty because it
    // doesn't belong to the XPTIInterfaceInfoManager, but there's no
    // obviously better place to measure it.
    amount += XPT_SizeOfArenaIncludingThis(gXPTIStructArena, XPTIMallocSizeOf);

    MOZ_COLLECT_REPORT(
        "explicit/xpti-working-set", KIND_HEAP, UNITS_BYTES, amount,
        "Memory used by the XPCOM typelib system.");

    return NS_OK;
}

// static
XPTInterfaceInfoManager*
XPTInterfaceInfoManager::GetSingleton()
{
    if (!gInterfaceInfoManager) {
        gInterfaceInfoManager = new XPTInterfaceInfoManager();
        gInterfaceInfoManager->InitMemoryReporter();
    }
    return gInterfaceInfoManager;
}

void
XPTInterfaceInfoManager::FreeInterfaceInfoManager()
{
    gInterfaceInfoManager = nullptr;
}

XPTInterfaceInfoManager::XPTInterfaceInfoManager()
    :   mWorkingSet(),
        mResolveLock("XPTInterfaceInfoManager.mResolveLock")
{
}

XPTInterfaceInfoManager::~XPTInterfaceInfoManager()
{
    // We only do this on shutdown of the service.
    mWorkingSet.InvalidateInterfaceInfos();

    UnregisterWeakMemoryReporter(this);
}

void
XPTInterfaceInfoManager::InitMemoryReporter()
{
    RegisterWeakMemoryReporter(this);
}

void
XPTInterfaceInfoManager::RegisterBuffer(char *buf, uint32_t length)
{
    XPTState state;
    XPT_InitXDRState(&state, buf, length);

    XPTCursor curs;
    NotNull<XPTCursor*> cursor = WrapNotNull(&curs);
    if (!XPT_MakeCursor(&state, XPT_HEADER, 0, cursor)) {
        return;
    }

    XPTHeader *header = nullptr;
    if (XPT_DoHeader(gXPTIStructArena, cursor, &header)) {
        RegisterXPTHeader(header);
    }
}

void
XPTInterfaceInfoManager::RegisterXPTHeader(const XPTHeader* aHeader)
{
    if (aHeader->mMajorVersion >= XPT_MAJOR_INCOMPATIBLE_VERSION) {
        MOZ_ASSERT(!aHeader->mNumInterfaces, "bad libxpt");
    }

    xptiTypelibGuts* typelib = xptiTypelibGuts::Create(aHeader);

    ReentrantMonitorAutoEnter monitor(mWorkingSet.mTableReentrantMonitor);
    for(uint16_t k = 0; k < aHeader->mNumInterfaces; k++)
        VerifyAndAddEntryIfNew(aHeader->mInterfaceDirectory + k, k, typelib);
}

void
XPTInterfaceInfoManager::VerifyAndAddEntryIfNew(const XPTInterfaceDirectoryEntry* iface,
                                                uint16_t idx,
                                                xptiTypelibGuts* typelib)
{
    if (!iface->mInterfaceDescriptor)
        return;

    // The number of maximum methods is not arbitrary. It is the same value as
    // in xpcom/reflect/xptcall/genstubs.pl; do not change this value
    // without changing that one or you WILL see problems.
    if (iface->mInterfaceDescriptor->mNumMethods > 250 &&
            !iface->mInterfaceDescriptor->IsBuiltinClass()) {
        NS_ASSERTION(0, "Too many methods to handle for the stub, cannot load");
        fprintf(stderr, "ignoring too large interface: %s\n", iface->mName);
        return;
    }

    mWorkingSet.mTableReentrantMonitor.AssertCurrentThreadIn();
    xptiInterfaceEntry* entry = mWorkingSet.mIIDTable.Get(iface->mIID);
    if (entry) {
        // XXX validate this info to find possible inconsistencies
        return;
    }

    // Build a new xptiInterfaceEntry object and hook it up.

    entry = xptiInterfaceEntry::Create(iface, typelib);
    if (!entry)
        return;

    mWorkingSet.mIIDTable.Put(entry->IID(), entry);
    mWorkingSet.mNameTable.Put(entry->GetTheName(), entry);

    typelib->SetEntryAt(idx, entry);
}

// this is a private helper
static nsresult
EntryToInfo(xptiInterfaceEntry* entry, nsIInterfaceInfo **_retval)
{
    if (!entry) {
        *_retval = nullptr;
        return NS_ERROR_FAILURE;
    }

    RefPtr<xptiInterfaceInfo> info = entry->InterfaceInfo();
    info.forget(_retval);
    return NS_OK;
}

xptiInterfaceEntry*
XPTInterfaceInfoManager::GetInterfaceEntryForIID(const nsIID *iid)
{
    ReentrantMonitorAutoEnter monitor(mWorkingSet.mTableReentrantMonitor);
    return mWorkingSet.mIIDTable.Get(*iid);
}

NS_IMETHODIMP
XPTInterfaceInfoManager::GetInfoForIID(const nsIID * iid, nsIInterfaceInfo **_retval)
{
    NS_ASSERTION(iid, "bad param");
    NS_ASSERTION(_retval, "bad param");

    ReentrantMonitorAutoEnter monitor(mWorkingSet.mTableReentrantMonitor);
    xptiInterfaceEntry* entry = mWorkingSet.mIIDTable.Get(*iid);
    return EntryToInfo(entry, _retval);
}

NS_IMETHODIMP
XPTInterfaceInfoManager::GetInfoForName(const char *name, nsIInterfaceInfo **_retval)
{
    NS_ASSERTION(name, "bad param");
    NS_ASSERTION(_retval, "bad param");

    ReentrantMonitorAutoEnter monitor(mWorkingSet.mTableReentrantMonitor);
    xptiInterfaceEntry* entry = mWorkingSet.mNameTable.Get(name);
    return EntryToInfo(entry, _retval);
}

void
XPTInterfaceInfoManager::GetScriptableInterfaces(nsCOMArray<nsIInterfaceInfo>& aInterfaces)
{
    // I didn't want to incur the size overhead of using nsHashtable just to
    // make building an enumerator easier. So, this code makes a snapshot of
    // the table using an nsCOMArray and builds an enumerator for that.
    // We can afford this transient cost.

    ReentrantMonitorAutoEnter monitor(mWorkingSet.mTableReentrantMonitor);
    aInterfaces.SetCapacity(mWorkingSet.mNameTable.Count());
    for (auto iter = mWorkingSet.mNameTable.Iter(); !iter.Done(); iter.Next()) {
        xptiInterfaceEntry* entry = iter.UserData();
        if (entry->GetScriptableFlag()) {
            nsCOMPtr<nsIInterfaceInfo> ii = entry->InterfaceInfo();
            aInterfaces.AppendElement(ii);
        }
    }
}
