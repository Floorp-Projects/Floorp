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

class nsCacheRequest;


/**
 *  nsCacheService
 */

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
    nsresult       OpenCacheEntry(nsCacheSession *           session,
                                  const char *               clientKey, 
                                  nsCacheAccessMode          accessRequested,
                                  nsICacheEntryDescriptor ** result);

    nsresult       AsyncOpenCacheEntry(nsCacheSession *   session,
                                       const char *       key, 
                                       nsCacheAccessMode  accessRequested,
                                       nsICacheListener * listener);

    /**
     * Methods called by nsCacheEntryDescriptor
     */

    nsresult       OnDataSizeChange(nsCacheEntry * entry, PRInt32 deltaSize);

    nsresult       ValidateEntry(nsCacheEntry * entry);

    nsresult       GetTransportForEntry(nsCacheEntry *     entry,
                                        nsCacheAccessMode  mode,
                                        nsITransport    ** result);

    void           CloseDescriptor(nsCacheEntryDescriptor * descriptor);


    /**
     * Methods called by any cache classes
     */

    static
    nsCacheService * GlobalInstance(void) { return gService; };

    nsresult         DoomEntry(nsCacheEntry * entry);

private:

    /**
     * Internal Methods
     */
    nsresult       CreateRequest(nsCacheSession *   session,
                                 const char *       clientKey,
                                 nsCacheAccessMode  accessRequested,
                                 nsICacheListener * listener,
                                 nsCacheRequest **  request);

    nsresult       ActivateEntry(nsCacheRequest * request, nsCacheEntry ** entry);
    nsresult       BindEntry(nsCacheEntry * entry);

    nsCacheEntry * SearchCacheDevices(nsCString * key, nsCacheStoragePolicy policy);

    nsresult       DoomEntry_Internal(nsCacheEntry * entry);


    void           DeactivateEntry(nsCacheEntry * entry);

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

    // Unexpected error totals
    PRUint32                mDeactivateFailures;
    PRUint32                mDeactivatedUnboundEntries;
};


/**
 * Cache Service Utility Functions
 */

// time conversion utils from nsCachedNetData.cpp

          // Convert PRTime to unix-style time_t, i.e. seconds since the epoch
PRUint32  ConvertPRTimeToSeconds(PRTime time64);

          // Convert unix-style time_t, i.e. seconds since the epoch, to PRTime
PRTime    ConvertSecondsToPRTime(PRUint32 seconds);

#endif // _nsCacheService_h_
