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
 * The Original Code is nsMemoryCacheDevice.h, released February 20, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan, 20-February-2001
 */

#ifndef _nsDiskCacheDevice_h_
#define _nsDiskCacheDevice_h_

#include "nsCacheDevice.h"
#include "nsDiskCacheEntry.h"

#include "nsILocalFile.h"
#include "nsIObserver.h"

class nsDiskCacheEntry;
class nsISupportsArray;

class nsDiskCacheDevice : public nsCacheDevice {
public:
    nsDiskCacheDevice();
    virtual ~nsDiskCacheDevice();

    nsresult  Init();

    static nsresult  Create(nsCacheDevice **result);

    virtual const char *    GetDeviceID(void);
    virtual nsCacheEntry *  FindEntry(nsCString * key);
    virtual nsresult        DeactivateEntry(nsCacheEntry * entry);
    virtual nsresult        BindEntry(nsCacheEntry * entry);
    virtual void            DoomEntry( nsCacheEntry * entry );

    virtual nsresult        GetTransportForEntry(nsCacheEntry * entry,
                                          nsCacheAccessMode mode,
                                          nsITransport ** result);

    virtual nsresult        GetFileForEntry(nsCacheEntry *    entry,
                                            nsIFile **        result);

    virtual nsresult        OnDataSizeChange(nsCacheEntry * entry, PRInt32 deltaSize);
    
    virtual nsresult        Visit(nsICacheVisitor * visitor);

/* private: */
    void                    setPrefsObserver(nsIObserver* observer);
    void                    getPrefsObserver(nsIObserver ** result);
    void                    setCacheDirectory(nsILocalFile* directory);
    void                    setCacheCapacity(PRUint32 capacity);
    PRUint32                getCacheCapacity();
    PRUint32                getCacheSize();

    nsresult getFileForKey(const char* key, PRBool meta, PRUint32 generation, nsIFile ** result);
    nsresult getFileForDiskCacheEntry(nsDiskCacheEntry * diskEntry, PRBool meta, nsIFile ** result);
    static nsresult getTransportForFile(nsIFile* file, nsCacheAccessMode mode, nsITransport ** result);

    nsresult visitEntries(nsICacheVisitor * visitory);
    
    nsresult readDiskCacheEntry(const char * key, nsDiskCacheEntry ** diskEntry);

    nsresult updateDiskCacheEntries();
    nsresult updateDiskCacheEntry(nsDiskCacheEntry * diskEntry);
    nsresult deleteDiskCacheEntry(nsDiskCacheEntry * diskEntry);
    
    nsresult scavengeDiskCacheEntries(nsDiskCacheEntry * diskEntry);

    nsresult scanDiskCacheEntries(nsISupportsArray ** result);
    nsresult evictDiskCacheEntries();
    
    nsresult writeCacheInfo();
    nsresult readCacheInfo();

private:
    nsCOMPtr<nsIObserver>       mPrefsObserver;
    nsCOMPtr<nsILocalFile>      mCacheDirectory;
    nsDiskCacheEntryHashTable   mBoundEntries;
    PRUint32                    mCacheCapacity;
    PRUint32                    mCacheSize;
};

#endif // _nsDiskCacheDevice_h_
