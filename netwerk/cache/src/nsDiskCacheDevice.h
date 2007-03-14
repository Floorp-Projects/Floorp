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
 * The Original Code is nsDiskCacheDevice.h, released
 * February 20, 2001.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gordon Sheridan <gordon@netscape.com>
 *   Patrick C. Beard <beard@netscape.com>
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

#ifndef _nsDiskCacheDevice_h_
#define _nsDiskCacheDevice_h_

#include "nsCacheDevice.h"
#include "nsDiskCacheBinding.h"
#include "nsDiskCacheBlockFile.h"
#include "nsDiskCacheEntry.h"

#include "nsILocalFile.h"
#include "nsIObserver.h"
#include "nsCOMArray.h"

class nsDiskCacheMap;


class nsDiskCacheDevice : public nsCacheDevice {
public:
    nsDiskCacheDevice();
    virtual ~nsDiskCacheDevice();

    virtual nsresult        Init();
    virtual nsresult        Shutdown();

    virtual const char *    GetDeviceID(void);
    virtual nsCacheEntry *  FindEntry(nsCString * key, PRBool *collision);
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

    // XXX: This is here to support the offline cache, and can be removed
    // XXX: once it has its own cache implementation
    void                    SetCacheParentDirectoryAndName(nsILocalFile * parentDir,
                                                           const nsACString & str);

/* private: */

    void                    getCacheDirectory(nsILocalFile ** result);
    PRUint32                getCacheCapacity();
    PRUint32                getCacheSize();
    PRUint32                getEntryCount();
    
    nsDiskCacheMap *        CacheMap()    { return &mCacheMap; }
    
private:    
    /**
     *  Private methods
     */

    PRBool                  Initialized() { return mInitialized; }

    nsresult                Shutdown_Private(PRBool flush);

    nsresult                OpenDiskCache();
    nsresult                ClearDiskCache();

    nsresult                EvictDiskCacheEntries(PRUint32  targetCapacity);
    
    /**
     *  Member variables
     */
    nsCOMPtr<nsILocalFile>  mCacheDirectory;
    nsDiskCacheBindery      mBindery;
    PRUint32                mCacheCapacity;     // Unit is KiB's
    // XXX need soft/hard limits, currentTotal
    nsDiskCacheMap          mCacheMap;
    PRPackedBool            mInitialized;
};

#endif // _nsDiskCacheDevice_h_
