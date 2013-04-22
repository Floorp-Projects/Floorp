/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_XPTInterfaceInfoManager_h_
#define mozilla_XPTInterfaceInfoManager_h_

#include "nsIInterfaceInfoManager.h"

#include "mozilla/Mutex.h"
#include "mozilla/ReentrantMonitor.h"
#include "nsDataHashtable.h"

template<typename T> class nsCOMArray;
class XPTHeader;
class XPTInterfaceDirectoryEntry;
class xptiInterfaceEntry;
class xptiInterfaceInfo;
class xptiTypelibGuts;

namespace mozilla {

class XPTInterfaceInfoManager MOZ_FINAL
    : public nsIInterfaceInfoManager
{
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINTERFACEINFOMANAGER

public:
    // GetSingleton() is infallible
    static XPTInterfaceInfoManager* GetSingleton();
    static void FreeInterfaceInfoManager();

    void GetScriptableInterfaces(nsCOMArray<nsIInterfaceInfo>& aInterfaces);

    void RegisterBuffer(char *buf, uint32_t length);

    static Mutex& GetResolveLock()
    {
        return GetSingleton()->mResolveLock;
    }

    xptiInterfaceEntry* GetInterfaceEntryForIID(const nsIID *iid);

    size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf);

    static int64_t GetXPTIWorkingSetSize();

private:
    XPTInterfaceInfoManager();
    ~XPTInterfaceInfoManager();

    void RegisterXPTHeader(XPTHeader* aHeader);
                          
    // idx is the index of this interface in the XPTHeader
    void VerifyAndAddEntryIfNew(XPTInterfaceDirectoryEntry* iface,
                                uint16_t idx,
                                xptiTypelibGuts* typelib);

private:

    class xptiWorkingSet
    {
    public:
        xptiWorkingSet();
        ~xptiWorkingSet();

        bool IsValid() const;

        void InvalidateInterfaceInfos();
        void ClearHashTables();

        // utility methods...

        enum {NOT_FOUND = 0xffffffff};

        // Directory stuff...

        uint32_t GetDirectoryCount();
        nsresult GetCloneOfDirectoryAt(uint32_t i, nsIFile** dir);
        nsresult GetDirectoryAt(uint32_t i, nsIFile** dir);
        bool     FindDirectory(nsIFile* dir, uint32_t* index);
        bool     FindDirectoryOfFile(nsIFile* file, uint32_t* index);
        bool     DirectoryAtMatchesPersistentDescriptor(uint32_t i, const char* desc);

    private:
        uint32_t        mFileCount;
        uint32_t        mMaxFileCount;

    public:
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

}

#endif
