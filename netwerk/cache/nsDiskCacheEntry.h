/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsDiskCacheEntry_h_
#define _nsDiskCacheEntry_h_

#include "nsDiskCacheMap.h"

#include "nsCacheEntry.h"


/******************************************************************************
 *  nsDiskCacheEntry
 *****************************************************************************/
struct nsDiskCacheEntry {
    uint32_t        mHeaderVersion; // useful for stand-alone metadata files
    uint32_t        mMetaLocation;  // for verification
    int32_t         mFetchCount;
    uint32_t        mLastFetched;
    uint32_t        mLastModified;
    uint32_t        mExpirationTime;
    uint32_t        mDataSize;
    uint32_t        mKeySize;       // includes terminating null byte
    uint32_t        mMetaDataSize;  // includes terminating null byte
    // followed by key data (mKeySize bytes)
    // followed by meta data (mMetaDataSize bytes)

    uint32_t        Size()    { return sizeof(nsDiskCacheEntry) +
                                    mKeySize + mMetaDataSize;
                              }

    char*           Key()     { return reinterpret_cast<char*>(this) +
                                    sizeof(nsDiskCacheEntry);
                              }

    char*           MetaData()
                              { return Key() + mKeySize; }

    nsCacheEntry *  CreateCacheEntry(nsCacheDevice *  device);

    void Swap()         // host to network (memory to disk)
    {
#if defined(IS_LITTLE_ENDIAN)
        mHeaderVersion      = htonl(mHeaderVersion);
        mMetaLocation       = htonl(mMetaLocation);
        mFetchCount         = htonl(mFetchCount);
        mLastFetched        = htonl(mLastFetched);
        mLastModified       = htonl(mLastModified);
        mExpirationTime     = htonl(mExpirationTime);
        mDataSize           = htonl(mDataSize);
        mKeySize            = htonl(mKeySize);
        mMetaDataSize       = htonl(mMetaDataSize);
#endif
    }

    void Unswap()       // network to host (disk to memory)
    {
#if defined(IS_LITTLE_ENDIAN)
        mHeaderVersion      = ntohl(mHeaderVersion);
        mMetaLocation       = ntohl(mMetaLocation);
        mFetchCount         = ntohl(mFetchCount);
        mLastFetched        = ntohl(mLastFetched);
        mLastModified       = ntohl(mLastModified);
        mExpirationTime     = ntohl(mExpirationTime);
        mDataSize           = ntohl(mDataSize);
        mKeySize            = ntohl(mKeySize);
        mMetaDataSize       = ntohl(mMetaDataSize);
#endif
    }
};


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

    const char* Key() { return mDiskEntry->Key(); }

private:
    virtual ~nsDiskCacheEntryInfo() = default;

    const char *        mDeviceID;
    nsDiskCacheEntry *  mDiskEntry;
};


#endif /* _nsDiskCacheEntry_h_ */
