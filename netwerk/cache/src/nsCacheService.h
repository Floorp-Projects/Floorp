/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is nsCacheService.h, released February 10, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan  <gordon@netscape.com>
 *    Patrick C. Beard <beard@netscape.com>
 *    Darin Fisher     <darin@netscape.com>
 */


#ifndef _nsCacheService_h_
#define _nsCacheService_h_

#include "nspr.h"
#include "nsICacheService.h"
#include "nsCacheSession.h"
#include "nsCacheDevice.h"
#include "nsCacheEntry.h"
#include "nsIObserver.h"
#include "nsString.h"

class nsCacheRequest;


/******************************************************************************
 *  nsCacheService
 ******************************************************************************/

class nsCacheService : public nsICacheService, public nsIObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICACHESERVICE
    NS_DECL_NSIOBSERVER
    
    nsCacheService();
    virtual ~nsCacheService();

    // Define a Create method to be used with a factory:
    static NS_METHOD
    Create(nsISupports* outer, const nsIID& iid, void* *result);


    /**
     * Methods called by nsCacheSession
     */
    nsresult         OpenCacheEntry(nsCacheSession *           session,
                                    const char *               key,
                                    nsCacheAccessMode          accessRequested,
                                    nsICacheListener *         listener,
                                    nsICacheEntryDescriptor ** result);

    /**
     * Methods called by nsCacheEntryDescriptor
     */

    nsresult         OnDataSizeChange(nsCacheEntry * entry, PRInt32 deltaSize);

    nsresult         ValidateEntry(nsCacheEntry * entry);

    nsresult         GetTransportForEntry(nsCacheEntry *     entry,
                                          nsCacheAccessMode  mode,
                                          nsITransport **    result);

    void             CloseDescriptor(nsCacheEntryDescriptor * descriptor);

    nsresult         GetFileForEntry(nsCacheEntry *         entry,
                                     nsIFile **             result);

    /**
     * Methods called by any cache classes
     */

    static
    nsCacheService * GlobalInstance(void) { return gService; };

    nsresult         DoomEntry(nsCacheEntry * entry);

    nsresult         DoomEntry_Locked(nsCacheEntry * entry);

    /**
     * static utility methods
     */
    static nsresult  ClientID(const nsAReadableCString&  clientID, char **  result);
    static nsresult  ClientKey(const nsAReadableCString& clientKey, char ** result);

private:

    /**
     * Internal Methods
     */

    nsresult         CreateDiskDevice();

    nsresult         CreateRequest(nsCacheSession *   session,
                                   const char *       clientKey,
                                   nsCacheAccessMode  accessRequested,
                                   nsICacheListener * listener,
                                   nsCacheRequest **  request);

    nsresult         NotifyListener(nsCacheRequest *          request,
                                    nsICacheEntryDescriptor * descriptor,
                                    nsCacheAccessMode         accessGranted,
                                    nsresult                  error);

    nsresult         ActivateEntry(nsCacheRequest * request, nsCacheEntry ** entry);

    nsCacheDevice *  EnsureEntryHasDevice(nsCacheEntry * entry);

    nsCacheEntry *   SearchCacheDevices(nsCString * key, nsCacheStoragePolicy policy);

    void             DeactivateEntry(nsCacheEntry * entry);

    nsresult         ProcessRequest(nsCacheRequest * request,
                                    nsICacheEntryDescriptor ** result);

    nsresult         ProcessPendingRequests(nsCacheEntry * entry);

    void             ClearPendingRequests(nsCacheEntry * entry);
    void             ClearDoomList(void);
    void             ClearActiveEntries(void);

    static
    PLDHashOperator  CRT_CALL DeactivateAndClearEntry(PLDHashTable *    table,
                                                      PLDHashEntryHdr * hdr,
                                                      PRUint32          number,
                                                      void *            arg);

    /**
     *  Data Members
     */

    enum {
        cacheServiceActiveMask    = 1
    };

    static nsCacheService * gService;  // there can be only one...

    PRLock*                 mCacheServiceLock;
    
    nsCacheDevice *         mMemoryDevice;
    nsCacheDevice *         mDiskDevice;

    //    nsCacheClientHashTable  mClientIDs;
    nsCacheEntryHashTable   mActiveEntries;
    PRCList                 mDoomedEntries;

    // stats
    PRUint32                mTotalEntries;
    PRUint32                mCacheHits;
    PRUint32                mCacheMisses;
    PRUint32                mMaxKeyLength;
    PRUint32                mMaxDataSize;
    PRUint32                mMaxMetaSize;

    // Unexpected error totals
    PRUint32                mDeactivateFailures;
    PRUint32                mDeactivatedUnboundEntries;
};


#endif // _nsCacheService_h_
