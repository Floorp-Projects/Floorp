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

#include "nsICacheService.h"
#include "nsCacheSession.h"
#include "nsCacheDevice.h"
#include "nsCacheEntry.h"

#include "nspr.h"
#include "nsIObserver.h"
#include "nsString.h"
#include "nsIEventQueueService.h"
#include "nsProxiedService.h"

class nsCacheRequest;
class nsCacheProfilePrefObserver;
class nsDiskCacheDevice;
class nsMemoryCacheDevice;


/******************************************************************************
 *  nsCacheService
 ******************************************************************************/

class nsCacheService : public nsICacheService
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICACHESERVICE
    
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
                                    PRBool                     blockingMode,
                                    nsICacheListener *         listener,
                                    nsICacheEntryDescriptor ** result);

    nsresult         EvictEntriesForSession(nsCacheSession *   session);

    nsresult         EvictEntriesForClient(const char *          clientID,
                                           nsCacheStoragePolicy  storagePolicy);

    static nsresult  IsStorageEnabledForPolicy(nsCacheStoragePolicy  storagePolicy,
                                               PRBool *              result);

    /**
     * Methods called by nsCacheEntryDescriptor
     */
    nsresult         SetCacheElement(nsCacheEntry * entry, nsISupports * element);

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
    nsCacheService * GlobalInstance()  { return gService; };

    nsresult         DoomEntry(nsCacheEntry * entry);

    nsresult         DoomEntry_Locked(nsCacheEntry * entry);

    static
    void             ProxyObjectRelease(nsISupports * object, PRThread * thread);

    /**
     * Methods called by nsCacheProfilePrefObserver
     */
    static void      OnProfileShutdown(PRBool cleanse);
    static void      OnProfileChanged();

    static void      SetDiskCacheEnabled(PRBool  enabled);
    static void      SetDiskCacheCapacity(PRInt32  capacity);

    static void      SetMemoryCacheEnabled(PRBool  enabled);
    static void      SetMemoryCacheCapacity(PRInt32  capacity);

private:

    /**
     * Internal Methods
     */

    nsresult         CreateDiskDevice();
    nsresult         CreateMemoryDevice();

    nsresult         CreateRequest(nsCacheSession *   session,
                                   const char *       clientKey,
                                   nsCacheAccessMode  accessRequested,
                                   PRBool             blockingMode,
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

    nsresult         ProcessRequest(nsCacheRequest *           request,
                                    PRBool                     calledFromOpenCacheEntry,
                                    nsICacheEntryDescriptor ** result);

    nsresult         ProcessPendingRequests(nsCacheEntry * entry);

    void             ClearPendingRequests(nsCacheEntry * entry);
    void             ClearDoomList(void);
    void             ClearActiveEntries(void);
    void             DoomActiveEntries(void);

    static
    PLDHashOperator PR_CALLBACK  DeactivateAndClearEntry(PLDHashTable *    table,
                                                         PLDHashEntryHdr * hdr,
                                                         PRUint32          number,
                                                         void *            arg);
    static
    PLDHashOperator PR_CALLBACK  RemoveActiveEntry(PLDHashTable *    table,
                                                   PLDHashEntryHdr * hdr,
                                                   PRUint32          number,
                                                   void *            arg);
#if defined(PR_LOGGING)
    void LogCacheStatistics();
#endif

    /**
     *  Data Members
     */

    static nsCacheService *         gService;  // there can be only one...
    nsCOMPtr<nsIEventQueueService>  mEventQService;
    nsCOMPtr<nsIProxyObjectManager> mProxyObjectManager;
    
    nsCacheProfilePrefObserver *    mObserver;
    
    PRLock *                        mCacheServiceLock;
    
    PRBool                          mEnableMemoryDevice;
    PRBool                          mEnableDiskDevice;

    nsMemoryCacheDevice *           mMemoryDevice;
    nsDiskCacheDevice *             mDiskDevice;

    nsCacheEntryHashTable           mActiveEntries;
    PRCList                         mDoomedEntries;

    // stats
    
    PRUint32                        mTotalEntries;
    PRUint32                        mCacheHits;
    PRUint32                        mCacheMisses;
    PRUint32                        mMaxKeyLength;
    PRUint32                        mMaxDataSize;
    PRUint32                        mMaxMetaSize;

    // Unexpected error totals
    PRUint32                        mDeactivateFailures;
    PRUint32                        mDeactivatedUnboundEntries;
};


#endif // _nsCacheService_h_
