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
 * The Original Code is nsDiskCacheMap.h, released March 23, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Patrick C. Beard <beard@netscape.com>
 */

#ifndef _nsDiskCacheMap_h_

#include "nsDiskCache.h"
#include "nsError.h"

class nsIInputStream;
class nsIOutputStream;

// XXX initial capacity, enough for 8192 distint entries.
const PRUint32 kRecordsPerBucket = 256;
const PRUint32 kBucketsPerTable = (1 << 4);       // must be a power of 2!
 
class nsDiskCacheMap {
public:
    nsDiskCacheMap();
    ~nsDiskCacheMap();

    nsDiskCacheRecord* GetRecord(PRUint32 hashNumber);
    
    nsresult Read(nsIInputStream* input);
    nsresult Write(nsIOutputStream* output);
    
private:
    // XXX need a bitmap?
    struct nsDiskCacheBucket {
        nsDiskCacheRecord mRecords[kRecordsPerBucket];
    };
    nsDiskCacheBucket mBuckets[kBucketsPerTable];
};

#endif // _nsDiskCacheMap_h_
