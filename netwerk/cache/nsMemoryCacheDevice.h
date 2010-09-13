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
 * The Original Code is nsMemoryCacheDevice.h, released
 * February 20, 2001.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gordon Sheridan, 20-February-2001
 *   Brian Ryner <bryner@brianryner.com>
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

#ifndef _nsMemoryCacheDevice_h_
#define _nsMemoryCacheDevice_h_

#include "nsCacheDevice.h"
#include "pldhash.h"
#include "nsCacheEntry.h"


class nsMemoryCacheDeviceInfo;

/******************************************************************************
 * nsMemoryCacheDevice
 ******************************************************************************/
class nsMemoryCacheDevice : public nsCacheDevice
{
public:
    nsMemoryCacheDevice();
    virtual ~nsMemoryCacheDevice();

    virtual nsresult        Init();
    virtual nsresult        Shutdown();

    virtual const char *    GetDeviceID(void);

    virtual nsresult        BindEntry( nsCacheEntry * entry );
    virtual nsCacheEntry *  FindEntry( nsCString * key, PRBool *collision );
    virtual void            DoomEntry( nsCacheEntry * entry );
    virtual nsresult        DeactivateEntry( nsCacheEntry * entry );

    virtual nsresult OpenInputStreamForEntry(nsCacheEntry *     entry,
                                             nsCacheAccessMode  mode,
                                             PRUint32           offset,
                                             nsIInputStream **  result);

    virtual nsresult OpenOutputStreamForEntry(nsCacheEntry *     entry,
                                              nsCacheAccessMode  mode,
                                              PRUint32           offset,
                                              nsIOutputStream ** result);

    virtual nsresult GetFileForEntry( nsCacheEntry *    entry,
                                      nsIFile **        result );

    virtual nsresult OnDataSizeChange( nsCacheEntry * entry, PRInt32 deltaSize );

    virtual nsresult Visit( nsICacheVisitor * visitor );

    virtual nsresult EvictEntries(const char * clientID);
    
    void             SetCapacity(PRInt32  capacity);

    bool             EntryIsTooBig(PRInt64 entrySize);
private:
    friend class nsMemoryCacheDeviceInfo;
    enum      { DELETE_ENTRY        = PR_TRUE,
                DO_NOT_DELETE_ENTRY = PR_FALSE };

    void      AdjustMemoryLimits( PRInt32  softLimit, PRInt32  hardLimit);
    void      EvictEntry( nsCacheEntry * entry , PRBool deleteEntry);
    void      EvictEntriesIfNecessary();
    int       EvictionList(nsCacheEntry * entry, PRInt32  deltaSize);

#ifdef DEBUG
    void      CheckEntryCount();
#endif
    /*
     *  Data members
     */
    enum {
        kQueueCount = 24   // entries > 2^23 (8Mb) start in last queue
    };

    nsCacheEntryHashTable  mMemCacheEntries;
    PRBool                 mInitialized;

    PRCList                mEvictionList[kQueueCount];
    PRInt32                mEvictionThreshold;

    PRInt32                mHardLimit;
    PRInt32                mSoftLimit;

    PRInt32                mTotalSize;
    PRInt32                mInactiveSize;

    PRInt32                mEntryCount;
    PRInt32                mMaxEntryCount;

    // XXX what other stats do we want to keep?
};


/******************************************************************************
 * nsMemoryCacheDeviceInfo - used to call nsIVisitor for about:cache
 ******************************************************************************/
class nsMemoryCacheDeviceInfo : public nsICacheDeviceInfo {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICACHEDEVICEINFO

    nsMemoryCacheDeviceInfo(nsMemoryCacheDevice* device)
        :   mDevice(device)
    {
    }

    virtual ~nsMemoryCacheDeviceInfo() {}
    
private:
    nsMemoryCacheDevice* mDevice;
};


#endif // _nsMemoryCacheDevice_h_
