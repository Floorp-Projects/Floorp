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

class nsCacheDeviceElement {
private:
    friend class nsCacheService;

    nsCacheDeviceElement(const char * id, nsCacheDevice * cacheDevice)
        : deviceID(id),
          device(cacheDevice)
    {
        PR_INIT_CLIST(&link);
    }

    ~nsCacheDeviceElement() {}

    PRCList *                     GetListNode(void)         { return &link; }
    static nsCacheDeviceElement * GetInstance(PRCList * qp) {
        return (nsCacheDeviceElement*)((char*)qp - offsetof(nsCacheDeviceElement, link));
    }

    PRCList        link;
    const char *   deviceID;
    nsCacheDevice *device;
};

#if 0
typedef struct {
    PLDHashNumber  keyHash;
    char *         clientID;
} nsCacheClientHashTableEntry;


class nsCacheClientHashTable
{
public:
    nsCacheClientHashTable();
    ~nsCacheClientHashTable();

    nsresult Init();
    void     AddClientID(const char *clientID);
    //** enumerate clientIDs

private:
    // PLDHashTable operation callbacks
    static const void *   GetKey( PLDHashTable *table, PLDHashEntryHdr *entry);

    static PLDHashNumber  HashKey( PLDHashTable *table, const void *key);

    static PRBool         MatchEntry( PLDHashTable *           table,
                                      const PLDHashEntryHdr *  entry,
                                      const void *             key);

    static void           MoveEntry( PLDHashTable *table,
                                     const PLDHashEntryHdr *from,
                                     PLDHashEntryHdr       *to);

    static void           ClearEntry( PLDHashTable *table, PLDHashEntryHdr *entry);

    static void           Finalize( PLDHashTable *table);

    // member variables
    static PLDHashTableOps ops;
    PLDHashTable           table;
    PRBool                 initialized;
};
#endif

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

    // service accessor for related classes (nsCacheSession, nsCacheEntry, etc.)
    static nsCacheService *
    GlobalInstance(void) { return gService; };

    nsresult       OpenCacheEntry(nsCacheSession *           session,
                                  const char *               clientKey, 
                                  nsCacheAccessMode          accessRequested,
                                  nsICacheEntryDescriptor ** result);

    nsresult       AsyncOpenCacheEntry(nsCacheSession *   session,
                                       const char *       key, 
                                       nsCacheAccessMode  accessRequested,
                                       nsICacheListener * listener);

    void           CloseDescriptor(nsCacheEntryDescriptor * descriptor);

private:

    nsresult       CommonOpenCacheEntry(nsCacheSession *   session,
                                        const char *       clientKey,
                                        nsCacheAccessMode  accessRequested,
                                        nsICacheListener * listener,
                                        nsCacheRequest **  request,
                                        nsCacheEntry **    entry);

    nsresult       ActivateEntry(nsCacheRequest * request, nsCacheEntry ** entry);

    nsresult       SearchCacheDevices(nsCacheEntry *entry, nsCacheDevice ** result);

    nsresult       Doom(nsCacheEntry * entry);


    void           DeactivateEntry(nsCacheEntry * entry);

    /**
     *  Data Members
     */

    enum {
        cacheServiceActiveMask    = 1
    };

    static nsCacheService * gService;  // there can be only one...

    PRLock*                 mCacheServiceLock;
    PRCList                 mDeviceList;
    //    nsCacheClientHashTable  mClientIDs;
    nsCacheEntryHashTable   mActiveEntries;
    PRCList                 mDoomedEntries;
};


#endif // _nsCacheService_h_
