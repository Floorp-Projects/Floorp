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
 *    Gordon Sheridan, 10-February-2001
 */


#ifndef _nsCacheService_h_
#define _nsCacheService_h_

#include "nspr.h"
#include "nsICacheService.h"
#include "nsCacheDevice.h"
#include "nsCacheEntry.h"
#include "nsCacheRequest.h"


class nsCacheDeviceElement {
public:
    nsCacheDeviceElement(PRUint32 id, nsCacheDevice * cacheDevice)
        : deviceID(id),
          device(cacheDevice)
    {
        PR_INIT_CLIST(&link);
    }

    ~nsCacheDeviceElement() {}

private:
    friend class nsCacheService;



    PRCList *                     GetListNode(void)         { return &link; }
    static nsCacheDeviceElement * GetInstance(PRCList * qp) {
        return (nsCacheDeviceElement*)((char*)qp - offsetof(nsCacheDeviceElement, link));
    }

    PRCList        link;
    PRUint32       deviceID;
    nsCacheDevice *device;
};


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

private:

    nsresult       ActivateEntry(nsCacheRequest * request, nsCacheEntry ** entry);
    nsCacheEntry * SearchActiveEntries(const nsCString * key);
    nsresult       SearchCacheDevices(nsCacheEntry *entry, nsCacheDevice ** result);

    nsresult       CommonOpenCacheEntry(const char *clientID, const char *clientKey,
                                        PRUint32  accessRequested, PRBool  streamBased,
                                        nsCacheRequest **request, nsCacheEntry **entry);

    nsresult       OpenCacheEntry(const char *clientID, const char *clientKey, 
                                  PRUint32  accessRequested, PRBool  streamBased,
                                  nsICacheEntryDescriptor **result);

    nsresult       AsyncOpenCacheEntry(const char *  clientID, const char *  key, 
                                       PRUint32  accessRequested, PRBool streamBased, 
                                       nsICacheListener *listener);

    /**
     *  Data Members
     */
    static nsCacheService * gService;  // "There can be only one..."

    enum {
        cacheServiceActiveMask    = 1
    };

    PRLock*                 mCacheServiceLock;

    PRCList                 mDeviceList;
    nsCacheClientHashTable  mClientIDs;
    nsCacheEntryHashTable   mActiveEntries;
    PRCList                 mDoomedEntries;
};



#endif // _nsCacheService_h_
