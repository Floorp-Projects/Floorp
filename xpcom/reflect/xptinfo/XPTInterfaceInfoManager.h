/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_XPTInterfaceInfoManager_h_
#define mozilla_XPTInterfaceInfoManager_h_

#include "nsIInterfaceInfoManager.h"
#include "nsIMemoryReporter.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/Mutex.h"
#include "mozilla/ReentrantMonitor.h"
#include "nsDataHashtable.h"

template<typename T> class nsCOMArray;
class nsIMemoryReporter;
struct XPTInterfaceDescriptor;
class xptiInterfaceEntry;
class xptiInterfaceInfo;
class xptiTypelibGuts;

namespace mozilla {

class XPTInterfaceInfoManager final
    : public nsIInterfaceInfoManager
    , public nsIMemoryReporter
{
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIINTERFACEINFOMANAGER
    NS_DECL_NSIMEMORYREPORTER

public:
    // GetSingleton() is infallible
    static XPTInterfaceInfoManager* GetSingleton();
    static void FreeInterfaceInfoManager();

    void GetScriptableInterfaces(nsCOMArray<nsIInterfaceInfo>& aInterfaces);

    static Mutex& GetResolveLock()
    {
        return GetSingleton()->mResolveLock;
    }

    xptiInterfaceEntry* GetInterfaceEntryForIID(const nsIID *iid);

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

private:
    XPTInterfaceInfoManager();
    ~XPTInterfaceInfoManager();

    void InitMemoryReporter();

    // idx is the index of this interface in the XPTHeader
    void VerifyAndAddEntryIfNew(const XPTInterfaceDescriptor* iface,
                                uint16_t idx,
                                xptiTypelibGuts* typelib);

private:

    class xptiWorkingSet
    {
    public:
        xptiWorkingSet();
        ~xptiWorkingSet();

        void InvalidateInterfaceInfos();

        // XXX make these private with accessors
        // mTableMonitor must be held across:
        //  * any read from or write to mIIDTable or mNameTable
        //  * any writing to the links between an xptiInterfaceEntry
        //    and its xptiInterfaceInfo (mEntry/mInfo)
        mozilla::ReentrantMonitor mTableReentrantMonitor;
        nsDataHashtable<nsIDHashKey, xptiInterfaceEntry*> mIIDTable;
        nsDataHashtable<nsDepCharHashKey, xptiInterfaceEntry*> mNameTable;
    };

    // XXX xptiInterfaceInfo want's to poke at the working set itself
    friend class ::xptiInterfaceInfo;
    friend class ::xptiInterfaceEntry;
    friend class ::xptiTypelibGuts;

    xptiWorkingSet               mWorkingSet;
    Mutex                        mResolveLock;
};

} // namespace mozilla

#endif
