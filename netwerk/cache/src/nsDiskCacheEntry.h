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
 * The Original Code is nsMemoryCacheDevice.cpp, released
 * February 22, 2001.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gordon Sheridan  <gordon@netscape.com>
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

#ifndef _nsDiskCacheEntry_h_
#define _nsDiskCacheEntry_h_

#include "nsDiskCacheMap.h"

#include "nsCacheEntry.h"

#include "nsICacheVisitor.h"

#include "nspr.h"
#include "nscore.h"
#include "nsError.h"



/******************************************************************************
 *  nsDiskCacheEntry
 *****************************************************************************/
struct nsDiskCacheEntry {
    PRUint32        mHeaderVersion; // useful for stand-alone metadata files
    PRUint32        mMetaLocation;  // for verification
    PRInt32         mFetchCount;
    PRUint32        mLastFetched;
    PRUint32        mLastModified;
    PRUint32        mExpirationTime;
    PRUint32        mDataSize;
    PRUint32        mKeySize;       // includes terminating null byte
    PRUint32        mMetaDataSize;  // includes terminating null byte
    char            mKeyStart[1];   // start of key data
    //              mMetaDataStart = mKeyStart[mKeySize];

    PRUint32        Size()    { return offsetof(nsDiskCacheEntry,mKeyStart) + 
                                    mKeySize + mMetaDataSize;
                              }

    nsCacheEntry *  CreateCacheEntry(nsCacheDevice *  device);
                                     
    PRBool          CheckConsistency(PRUint32  size);

    void Swap()         // host to network (memory to disk)
    {
#if defined(IS_LITTLE_ENDIAN)   
        mHeaderVersion      = ::PR_htonl(mHeaderVersion);
        mMetaLocation       = ::PR_htonl(mMetaLocation);
        mFetchCount         = ::PR_htonl(mFetchCount);
        mLastFetched        = ::PR_htonl(mLastFetched);
        mLastModified       = ::PR_htonl(mLastModified);
        mExpirationTime     = ::PR_htonl(mExpirationTime);
        mDataSize           = ::PR_htonl(mDataSize);
        mKeySize            = ::PR_htonl(mKeySize);
        mMetaDataSize       = ::PR_htonl(mMetaDataSize);
#endif
    }
    
    void Unswap()       // network to host (disk to memory)
    {
#if defined(IS_LITTLE_ENDIAN)
        mHeaderVersion      = ::PR_ntohl(mHeaderVersion);
        mMetaLocation       = ::PR_ntohl(mMetaLocation);
        mFetchCount         = ::PR_ntohl(mFetchCount);
        mLastFetched        = ::PR_ntohl(mLastFetched);
        mLastModified       = ::PR_ntohl(mLastModified);
        mExpirationTime     = ::PR_ntohl(mExpirationTime);
        mDataSize           = ::PR_ntohl(mDataSize);
        mKeySize            = ::PR_ntohl(mKeySize);
        mMetaDataSize       = ::PR_ntohl(mMetaDataSize);
#endif
    }
};

nsDiskCacheEntry *  CreateDiskCacheEntry(nsDiskCacheBinding *  binding);



/******************************************************************************
 *  nsDiskCacheEntryInfo
 *****************************************************************************/
class nsDiskCacheEntryInfo : public nsICacheEntryInfo {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICACHEENTRYINFO

    nsDiskCacheEntryInfo(const char * deviceID, nsDiskCacheEntry * diskEntry)
        : mDeviceID(deviceID)
        , mDiskEntry(diskEntry)
    {
    }

    virtual ~nsDiskCacheEntryInfo() {}
    
    const char* Key() { return mDiskEntry->mKeyStart; }
    
private:
    const char *        mDeviceID;
    nsDiskCacheEntry *  mDiskEntry;
};


#endif /* _nsDiskCacheEntry_h_ */
