/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *  
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *  
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Scott Furman, fur@netscape.com
 *
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/27/2000   IBM Corp.       Added PR_CALLBACK for Optlink
 *                               use in OS2
 */

#ifndef _nsCachedNetData_h_
#define _nsCachedNetData_h_

#include "nsICachedNetData.h"
#include "nsCOMPtr.h"
#include "nsINetDataCacheRecord.h"
#include "prinrval.h"

class nsINetDataCache;
class nsIStreamAsFileObserver;
class nsIStreamAsFile;
class nsIArena;
class StreamAsFileObserverClosure;
class CacheMetaData;
class nsIFile;
// Number of recent access times recorded
#define MAX_K  3

/**
 * FIXME - add comment.  There are a lot of these data structures resident in
 * memory, so be careful about adding members unnecessarily.
 */
class nsCachedNetData : public nsICachedNetData {

public:
    NS_DECL_ISUPPORTS

    // nsICachedNetData methods
    NS_DECL_NSICACHEDNETDATA

    NS_METHOD Init(nsINetDataCacheRecord *aRecord, nsINetDataCache *aCache);

protected:

    // Bits for mFlags, below
    typedef enum {
        DIRTY               = 1 << 0, // Cache entry data needs to be flushed to database

    // ==== Flags that can be set by the protocol handler ====
        ALLOW_PARTIAL       = 1 << 1, // Protocol handler supports partial fetching
        UPDATE_IN_PROGRESS  = 1 << 2, // Protocol handler now modifying cache data

    // ==== Cache-entry state flags.  At most one of these flags can be set ====
        TRUNCATED_CONTENT   = 1 << 4, // Entry contains valid content, but it has
                                      //   been truncated by cache manager

        // A previously-used cache entry, which has been purged of all cached
        // content and protocol-private data.  This cache entry can be refilled
        // with new content or it may be retained in this vestigial state
        // because the usage statistics it contains will be used by the
        // replacement policy if the same URI is ever cached again.
        VESTIGIAL           = 1 << 5,

    // ==== Memory usage status bits.  At most one of these flags can be set ====
        RECYCLED            = 1 << 8, // Previously associated database record has
                                      //   been deleted; This cache entry is available
                                      //   for recycling.

        DORMANT             = 1 << 9, // No references to this cache entry, except by
                                      //   the cache manager itself
        
    // ==== Setter bits ====
        LAST_MODIFIED_KNOWN = 1 <<12, // Protocol handler called SetLastModifiedTime()
        EXPIRATION_KNOWN    = 1 <<13, // Protocol handler called SetExpirationTime()
        STALE_TIME_KNOWN    = 1 <<14, // Protocol handler called SetStaleTime()

    // ==== Useful flag combinations ====
        // Cache entry not eligible for eviction
        UNEVICTABLE         = VESTIGIAL | RECYCLED | UPDATE_IN_PROGRESS,
        
        // State flags that are in-memory only, i.e. not persistent 
        TRANSIENT_FLAGS     = DIRTY | RECYCLED | DORMANT
    } Flag;

    PRBool GetFlag(Flag aFlag) { return (mFlags & aFlag) != 0; }
    nsresult GetFlag(PRBool *aResult, Flag aFlag) { *aResult = GetFlag(aFlag); return NS_OK; }

    // Set a boolean flag for the cache entry
    nsresult SetFlag(PRBool aValue, Flag aFlag);
    nsresult SetFlag(Flag aFlag)   { return SetFlag(PR_TRUE,  aFlag); }
    nsresult ClearFlag(Flag aFlag) { return SetFlag(PR_FALSE, aFlag); }

    void ComputeProfit(PRUint32 aCurrentTime);
    static int PR_CALLBACK Compare(const void *a, const void *b, void *unused);

    void NoteAccess();
    void NoteUpdate();

    // Get underlying raw cache database record.
    nsresult GetRecord(nsINetDataCacheRecord* *aRecord);

    nsresult GetRecordID(PRInt32 *aRecordID);

    nsresult Evict(PRUint32 aTruncatedContentLength);
    
    nsresult GetFile(nsIFile* *aFileSpec);

    void NoteDownloadTime(PRIntervalTime start, PRIntervalTime end);

    // placement new for arena-allocation
    void *operator new (size_t aSize, nsIArena *aArena);

    // placement new for recycling of arena-allocated nsCachedNetData instances
    void *operator new (size_t aSize, nsCachedNetData *aEntry);

    friend class nsReplacementPolicy;
    friend class nsCacheManager;
    friend class StreamAsFile;
    friend class nsCacheEntryTransport;
    friend class CacheOutputStream;
    friend class InterceptStreamListener;

protected:
    nsCachedNetData() { NS_INIT_REFCNT(); };
    virtual ~nsCachedNetData() {};

private:
    // Initialize internal fields of this nsCachedNetData instance from the
    // underlying raw cache database record.
    nsresult Deserialize(PRBool aDeserializeFlags);

    // Notify stream-as-file observers about change in cache entry status
    nsresult Notify(PRUint32 aMessage, nsresult aError);

    // Add/Remove stream-as-file observers
    nsresult AddObserver(nsIStreamAsFile *aStreamAsFile, nsIStreamAsFileObserver* aObserver);
    nsresult RemoveObserver(nsIStreamAsFileObserver* aObserver);

    // Mark cache entry to indicate a write out to the cache database is required
    void SetDirty() { mFlags |= DIRTY; }

    nsresult Resurrect(nsINetDataCacheRecord *aRecord);

    nsresult CommitFlags();

    CacheMetaData* FindTaggedMetaData(const char* aTag, PRBool aCreate);

private:
    
    // List of nsIStreamAsFileObserver's that will receive notification events
    // when the cache manager or a client desires to delete/truncate a cache
    // entry file.
    StreamAsFileObserverClosure* mObservers;

    // Protocol-specific meta-data, opaque to the cache manager
    CacheMetaData *mMetaData;

    // Next in chain for a single bucket in the replacement policy hash table
    // that maps from record ID to nsCachedNetData
    nsCachedNetData* mNext;
    
    // See flag bits, above
    // NOTE: 16 bit member is combined with members below for
    //       struct packing efficiency.  Do not change order of members!
    PRUint16    mFlags;

protected:

    // Number of nsCacheEntryTransport's referring to this record
    PRUint8     mTransportCount;

    // Below members are statistics kept per cache-entry, used to decide how
    // profitable it will be to evict a record from the cache relative to other
    // existing records.  Note: times are measured in *seconds* since the
    // 1/1/70 epoch, same as a unix time_t.

    // Number of accesses for this cache record
    // NOTE: 8 bit member is combined with members above for
    //       struct packing efficiency.  Do not change order of members!
    PRUint8     mNumAccesses;

    // A reference to the underlying, raw cache database record, either as a
    // pointer to an in-memory object or as a database record identifier
    union {
        nsINetDataCacheRecord* mRecord;

        // Database record ID of associated cache record.  See
        // nsINetDataCache::GetRecordByID().
        PRInt32  mRecordID;
    };

    // Weak link to parent cache
    nsINetDataCache* mCache;

    // Length of stored content, which may be less than storage consumed if
    // compression is used
    PRUint32 mLogicalLength;

    // Most recent cache entry access times, used to compute access frequency
    PRUint32 mAccessTime[MAX_K];
    
    // We use modification time of the original document for replacement policy
    // computations, i.e. to compute a document's age, but if we don't know it,
    // we use the time that the document was last written to the cache.
    // Document modification time, if known.

        PRUint32 mLastModifiedTime;

        // Time of last cache update for this doc
        PRUint32 mLastUpdateTime;

    union {
      // Time until which document is fresh, i.e. does not have to be validated
      // with server and, therefore, data in cache is guaranteed usable
      PRUint32 mExpirationTime;

      // Heuristic time at which cached document is likely to be out-of-date
      // with respect to canonical copy on server.  Used for cache replacement
      // policy, not for validation.
      PRUint32 mStaleTime;
    };

    // Download time per byte, measure roughly in units of KB/s
    float mDownloadRate;
    
    // Heuristic estimate of cache entry future benefits, based on above values
    float mProfit;
};

#endif // _nsCachedNetData_h_

