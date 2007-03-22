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
#include "nsCOMPtr.h"
#include "nsICache.h"
#include "nsICacheListener.h"
#include "nsCacheSession.h"
#include "nsCacheService.h"


class nsCacheRequest : public PRCList
{
private:
    friend class nsCacheService;
    friend class nsCacheEntry;

    nsCacheRequest( nsCString *           key, 
                    nsICacheListener *    listener,
                    nsCacheAccessMode     accessRequested,
                    PRBool                blockingMode,
                    nsCacheSession *      session)
        : mKey(key),
          mInfo(0),
          mListener(listener),
          mLock(nsnull),
          mCondVar(nsnull)
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
        if (mLock)    PR_DestroyLock(mLock);
        if (mCondVar) PR_DestroyCondVar(mCondVar);
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
    PRBool IsStreamBased()      { return (mInfo & eStreamBasedMask) != 0; }


    void   MarkDoomEntriesIfExpired()   { mInfo |=  eDoomEntriesIfExpiredMask; }
    PRBool WillDoomEntriesIfExpired()   { return (mInfo & eDoomEntriesIfExpiredMask); }
    
    void   MarkBlockingMode()           { mInfo |= eBlockingModeMask; }
    PRBool IsBlocking()                 { return  (mInfo & eBlockingModeMask); }
    PRBool IsNonBlocking()              { return !(mInfo & eBlockingModeMask); }

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
    PRBool WaitingForValidation()
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

        if (!mLock) {
            mLock = PR_NewLock();
            if (!mLock) return NS_ERROR_OUT_OF_MEMORY;

            NS_ASSERTION(!mCondVar,"we have mCondVar, but didn't have mLock?");
            mCondVar = PR_NewCondVar(mLock);
            if (!mCondVar) {
                PR_DestroyLock(mLock);
                return NS_ERROR_OUT_OF_MEMORY;
            }
        }
        PRStatus status = PR_SUCCESS;
        PR_Lock(mLock);
        while (WaitingForValidation() && (status == PR_SUCCESS) ) {
            status = PR_WaitCondVar(mCondVar, PR_INTERVAL_NO_TIMEOUT);
        }
        MarkWaitingForValidation();  // set up for next time
        PR_Unlock(mLock);
        
        NS_ASSERTION(status == PR_SUCCESS, "PR_WaitCondVar() returned PR_FAILURE?");
        if (status == PR_FAILURE)
            return NS_ERROR_UNEXPECTED;

        return NS_OK;
    }

    void WakeUp(void) {
        DoneWaitingForValidation();
        if (mLock) {
        PR_Lock(mLock);
        PR_NotifyCondVar(mCondVar);
        PR_Unlock(mLock);
        }
    }

    /**
     * Data members
     */
    nsCString *                mKey;
    PRUint32                   mInfo;
    nsICacheListener *         mListener;  // strong ref
    nsCOMPtr<nsIThread>        mThread;
    PRLock *                   mLock;
    PRCondVar *                mCondVar;
};

#endif // _nsCacheRequest_h_
