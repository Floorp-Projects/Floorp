/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is nsCacheRequest.h, released
 * February 22, 2001.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gordon Sheridan, 22-February-2001
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
        return (nsCacheStoragePolicy)(mInfo & 0xFF);
    }

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
