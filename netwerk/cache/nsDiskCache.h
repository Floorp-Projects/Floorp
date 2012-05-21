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

    // Parameter initval initializes internal state of hash function. Hash values are different
    // for the same text when different initval is used. It can be any random number.
    // 
    // It can be used for generating 64-bit hash value:
    //   (PRUint64(Hash(key, initval1)) << 32) | Hash(key, initval2)
    //
    // It can be also used to hash multiple strings:
    //   h = Hash(string1, 0);
    //   h = Hash(string2, h);
    //   ... 
    static PLDHashNumber    Hash(const char* key, PLDHashNumber initval=0);
    static nsresult         Truncate(PRFileDesc *  fd, PRUint32  newEOF);
};

#endif // _nsDiskCache_h_
