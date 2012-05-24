/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsCacheRequest_h_
#define _nsCacheRequest_h_

#include "nspr.h"
#include "mozilla/CondVar.h"
#include "mozilla/Mutex.h"
#include "nsCOMPtr.h"
#include "nsICache.h"
#include "nsICacheListener.h"
#include "nsCacheSession.h"
#include "nsCacheService.h"


class nsCacheRequest : public PRCList
{
    typedef mozilla::CondVar CondVar;
    typedef mozilla::MutexAutoLock MutexAutoLock;
    typedef mozilla::Mutex Mutex;

private:
    friend class nsCacheService;
    friend class nsCacheEntry;
    friend class nsProcessRequestEvent;

    nsCacheRequest( nsCString *           key, 
                    nsICacheListener *    listener,
                    nsCacheAccessMode     accessRequested,
                    bool                  blockingMode,
                    nsCacheSession *      session)
        : mKey(key),
          mInfo(0),
          mListener(listener),
          mLock("nsCacheRequest.mLock"),
          mCondVar(mLock, "nsCacheRequest.mCondVar")
    {
        MOZ_COUNT_CTOR(nsCacheRequest);
        PR_INIT_CLIST(this);
        SetAccessRequested(accessRequested);
        SetStoragePolicy(session->StoragePolicy());
        if (session->IsStreamBased())             MarkStreamBased();
        if (session->WillDoomEntriesIfExpired())  MarkDoomEntriesIfExpired();
        if (session->IsPrivate())                 MarkPrivate();
        if (blockingMode == nsICache::BLOCKING)    MarkBlockingMode();
        MarkWaitingForValidation();
        NS_IF_ADDREF(mListener);
    }
    
    ~nsCacheRequest()
    {
        MOZ_COUNT_DTOR(nsCacheRequest);
        delete mKey;
        NS_ASSERTION(PR_CLIST_IS_EMPTY(this), "request still on a list");

        if (mListener)
            nsCacheService::ReleaseObject_Locked(mListener, mThread);
    }
    
    /**
     * Simple Accessors
     */
    enum CacheRequestInfo {
        eStoragePolicyMask         = 0x000000FF,
        eStreamBasedMask           = 0x00000100,
        ePrivateMask               = 0x00000200,
        eDoomEntriesIfExpiredMask  = 0x00001000,
        eBlockingModeMask          = 0x00010000,
        eWaitingForValidationMask  = 0x00100000,
        eAccessRequestedMask       = 0xFF000000
    };

    void SetAccessRequested(nsCacheAccessMode mode)
    {
        NS_ASSERTION(mode <= 0xFF, "too many bits in nsCacheAccessMode");
        mInfo &= ~eAccessRequestedMask;
        mInfo |= mode << 24;
    }

    nsCacheAccessMode AccessRequested()
    {
        return (nsCacheAccessMode)((mInfo >> 24) & 0xFF);
    }

    void MarkStreamBased()      { mInfo |=  eStreamBasedMask; }
    bool IsStreamBased()      { return (mInfo & eStreamBasedMask) != 0; }


    void   MarkDoomEntriesIfExpired()   { mInfo |=  eDoomEntriesIfExpiredMask; }
    bool WillDoomEntriesIfExpired()   { return (0 != (mInfo & eDoomEntriesIfExpiredMask)); }
    
    void   MarkBlockingMode()           { mInfo |= eBlockingModeMask; }
    bool IsBlocking()                 { return (0 != (mInfo & eBlockingModeMask)); }
    bool IsNonBlocking()              { return !(mInfo & eBlockingModeMask); }

    void SetStoragePolicy(nsCacheStoragePolicy policy)
    {
        NS_ASSERTION(policy <= 0xFF, "too many bits in nsCacheStoragePolicy");
        mInfo &= ~eStoragePolicyMask;  // clear storage policy bits
        mInfo |= policy;         // or in new bits
    }

    nsCacheStoragePolicy StoragePolicy()
    {
        return (nsCacheStoragePolicy)(mInfo & eStoragePolicyMask);
    }

    void   MarkPrivate() { mInfo |= ePrivateMask; }
    void   MarkPublic() { mInfo &= ~ePrivateMask; }
    bool   IsPrivate() { return (mInfo & ePrivateMask) != 0; }

    void   MarkWaitingForValidation() { mInfo |=  eWaitingForValidationMask; }
    void   DoneWaitingForValidation() { mInfo &= ~eWaitingForValidationMask; }
    bool WaitingForValidation()
    {
        return (mInfo & eWaitingForValidationMask) != 0;
    }

    nsresult
    WaitForValidation(void)
    {
        if (!WaitingForValidation()) {   // flag already cleared
            MarkWaitingForValidation();  // set up for next time
            return NS_OK;                // early exit;
        }
        {
            MutexAutoLock lock(mLock);
            while (WaitingForValidation()) {
                mCondVar.Wait();
            }
            MarkWaitingForValidation();  // set up for next time
        }       
        return NS_OK;
    }

    void WakeUp(void) {
        DoneWaitingForValidation();
        MutexAutoLock lock(mLock);
        mCondVar.Notify();
    }

    /**
     * Data members
     */
    nsCString *                mKey;
    PRUint32                   mInfo;
    nsICacheListener *         mListener;  // strong ref
    nsCOMPtr<nsIThread>        mThread;
    Mutex                      mLock;
    CondVar                    mCondVar;
};

#endif // _nsCacheRequest_h_
