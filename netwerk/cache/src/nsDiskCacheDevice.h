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
 * The Original Code is nsDiskCacheDevice.h, released February 20, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan <gordon@netscape.com>
 *    Patrick C. Beard <beard@netscape.com>
 */

#ifndef _nsDiskCacheDevice_h_
#define _nsDiskCacheDevice_h_

#include "nsCacheDevice.h"
#include "nsDiskCacheBinding.h"
#include "nsDiskCacheBlockFile.h"
#include "nsDiskCacheEntry.h"

#include "nsILocalFile.h"
#include "nsIObserver.h"

class nsDiskCacheMap;


class nsDiskCacheDevice : public nsCacheDevice {
public:
    nsDiskCacheDevice();
    virtual ~nsDiskCacheDevice();

    static nsresult         Create(nsCacheDevice **result);

    virtual nsresult        Init();
    virtual nsresult        Shutdown();

    virtual const char *    GetDeviceID(void);
    virtual nsCacheEntry *  FindEntry(nsCString * key);
    virtual nsresult        DeactivateEntry(nsCacheEntry * entry);
    virtual nsresult        BindEntry(nsCacheEntry * entry);
    virtual void            DoomEntry( nsCacheEntry * entry );

    virtual nsresult OpenInputStreamForEntry(nsCacheEntry *    entry,
                                             nsCacheAccessMode mode,
                                             PRUint32          offset,
                                             nsIInputStream ** result);

    virtual nsresult OpenOutputStreamForEntry(nsCacheEntry *     entry,
                                              nsCacheAccessMode  mode,
                                              PRUint32           offset,
                                              nsIOutputStream ** result);

    virtual nsresult        GetFileForEntry(nsCacheEntry *    entry,
                                            nsIFile **        result);

    virtual nsresult        OnDataSizeChange(nsCacheEntry * entry, PRInt32 deltaSize);
    
    virtual nsresult        Visit(nsICacheVisitor * visitor);

    virtual nsresult        EvictEntries(const char * clientID);

    /**
     * Preference accessors
     */
    void                    SetCacheParentDirectory(nsILocalFile * parentDir);
    void                    SetCapacity(PRUint32  capacity);


/* private: */

    void                    getCacheDirectory(nsILocalFile ** result);
    PRUint32                getCacheCapacity();
    PRUint32                getCacheSize();
    PRUint32                getEntryCount();
    
    PRBool                  Initialized() { return mInitialized; }
    nsDiskCacheMap *        CacheMap()    { return mCacheMap; }
    
private:    
    /**
     *  Private methods
     */
    nsresult    InitializeCacheDirectory();
    nsresult    GetCacheTrashDirectory(nsIFile ** result);
    nsresult    EvictDiskCacheEntries(PRInt32  targetCapacity);
    
    /**
     *  Member variables
     */
    nsCOMPtr<nsILocalFile>  mCacheDirectory;
    nsDiskCacheBindery      mBindery;
    PRUint32                mCacheCapacity;     // XXX need soft/hard limits, currentTotal
    nsDiskCacheMap *        mCacheMap;
    PRPackedBool            mInitialized;
};

#endif // _nsDiskCacheDevice_h_
