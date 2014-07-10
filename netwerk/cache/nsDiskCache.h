/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef _nsDiskCache_h_
#define _nsDiskCache_h_

#include "nsCacheEntry.h"

#ifdef XP_WIN
#include <winsock.h>  // for htonl/ntohl
#endif


class nsDiskCache {
public:
    enum {
            kCurrentVersion = 0x00010013      // format = 16 bits major version/16 bits minor version
    };

    enum { kData, kMetaData };

    // Stores the reason why the cache is corrupt.
    // Note: I'm only listing the enum values explicitly for easy mapping when
    // looking at telemetry data.
    enum CorruptCacheInfo {
      kNotCorrupt = 0,
      kInvalidArgPointer = 1,
      kUnexpectedError = 2,
      kOpenCacheMapError = 3,
      kBlockFilesShouldNotExist = 4,
      kOutOfMemory = 5,
      kCreateCacheSubdirectories = 6,
      kBlockFilesShouldExist = 7,
      kHeaderSizeNotRead = 8,
      kHeaderIsDirty = 9,
      kVersionMismatch = 10,
      kRecordsIncomplete = 11,
      kHeaderIncomplete = 12,
      kNotEnoughToRead = 13,
      kEntryCountIncorrect = 14,
      kCouldNotGetBlockFileForIndex = 15,
      kCouldNotCreateBlockFile = 16,
      kBlockFileSizeError = 17,
      kBlockFileBitMapWriteError = 18,
      kBlockFileSizeLessThanBitMap = 19,
      kBlockFileBitMapReadError = 20,
      kBlockFileEstimatedSizeError = 21,
      kFlushHeaderError = 22,
      kCacheCleanFilePathError = 23,
      kCacheCleanOpenFileError = 24,
      kCacheCleanTimerError = 25
    };

    // Parameter initval initializes internal state of hash function. Hash values are different
    // for the same text when different initval is used. It can be any random number.
    // 
    // It can be used for generating 64-bit hash value:
    //   (uint64_t(Hash(key, initval1)) << 32) | Hash(key, initval2)
    //
    // It can be also used to hash multiple strings:
    //   h = Hash(string1, 0);
    //   h = Hash(string2, h);
    //   ... 
    static PLDHashNumber    Hash(const char* key, PLDHashNumber initval=0);
    static nsresult         Truncate(PRFileDesc *  fd, uint32_t  newEOF);
};

#endif // _nsDiskCache_h_
