/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsCacheSession_h_
#define _nsCacheSession_h_

#include "nspr.h"
#include "nsError.h"
#include "nsCOMPtr.h"
#include "nsICacheSession.h"
#include "nsIFile.h"
#include "nsString.h"

class nsCacheSession : public nsICacheSession
{
    virtual ~nsCacheSession();

public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICACHESESSION
    
    nsCacheSession(const char * clientID, nsCacheStoragePolicy storagePolicy, bool streamBased);
    
    nsCString *           ClientID()      { return &mClientID; }

    enum SessionInfo {
        eStoragePolicyMask        = 0x000000FF,
        eStreamBasedMask          = 0x00000100,
        eDoomEntriesIfExpiredMask = 0x00001000,
        ePrivateMask              = 0x00010000
    };

    void   MarkStreamBased()  { mInfo |=  eStreamBasedMask; }
    void   ClearStreamBased() { mInfo &= ~eStreamBasedMask; }
    bool IsStreamBased()    { return (mInfo & eStreamBasedMask) != 0; }

    void   MarkDoomEntriesIfExpired()  { mInfo |=  eDoomEntriesIfExpiredMask; }
    void   ClearDoomEntriesIfExpired() { mInfo &= ~eDoomEntriesIfExpiredMask; }
    bool WillDoomEntriesIfExpired()  { return (0 != (mInfo & eDoomEntriesIfExpiredMask)); }

    void   MarkPrivate() { mInfo |= ePrivateMask; }
    void   MarkPublic() { mInfo &= ~ePrivateMask; }
    bool IsPrivate() { return (mInfo & ePrivateMask) != 0; }
    nsCacheStoragePolicy  StoragePolicy()
    {
        return (nsCacheStoragePolicy)(mInfo & eStoragePolicyMask);
    }

    void SetStoragePolicy(nsCacheStoragePolicy policy)
    {
        NS_ASSERTION(policy <= 0xFF, "too many bits in nsCacheStoragePolicy");
        mInfo &= ~eStoragePolicyMask; // clear storage policy bits
        mInfo |= policy;
    }

    nsIFile* ProfileDir() { return mProfileDir; }

private:
    nsCString               mClientID;
    uint32_t                mInfo;
    nsCOMPtr<nsIFile>       mProfileDir;
};

#endif // _nsCacheSession_h_
