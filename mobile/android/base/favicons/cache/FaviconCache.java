/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.favicons.cache;

import android.graphics.Bitmap;
import android.util.Log;
import org.mozilla.gecko.favicons.Favicons;

import java.util.Iterator;
import java.util.LinkedList;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Semaphore;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Implements a Least-Recently-Used cache for Favicons, keyed by Favicon URL.
 *
 * When a favicon at a particular URL is decoded, it will yield one or more bitmaps.
 * While in memory, these bitmaps are stored in a list, sorted in ascending order of size, in a
 * FaviconsForURL object.
 * The collection of FaviconsForURL objects currently in the cache is stored in mBackingMap, keyed
 * by favicon URL.
 *
 * FaviconsForURL provides a method for obtaining the smallest icon larger than a given size - the
 * most appropriate icon for a particular size.
 * It also distinguishes between "primary" favicons (Ones that have merely been extracted from a
 * file downloaded from the website) and "secondary" favicons (Ones that have been computed locally
 * as resized versions of primary favicons.).
 *
 * FaviconsForURL is also responsible for storing URL-specific, as opposed to favicon-specific,
 * information. For the purposes of this cache, the simplifying assumption that the dominant colour
 * for all favicons served from a particular favicon URL shall be the same is made. (To violate this
 * would mandate serving an ICO or similar file with multiple radically different images in it - an
 * ill-advised and extremely uncommon use-case, for sure.)
 * The dominant colour information is updated as the element is being added to the cache - typically
 * on the background thread.
 * Also present here are the download timestamp and isFailed flag. Upon failure, the flag is set.
 * A constant exists in this file to specify the maximum time permitted between failures before
 * a retry is again permitted.
 *
 * TODO: Expiry of Favicons from the favicon database cache is not implemented. (Bug 914296)
 *
 * A typical request to the cache will consist of a Favicon URL and a target size. The FaviconsForURL
 * object for that URL will be obtained, queried for a favicon matching exactly the needed size, and
 * if successful, the result is returned.
 * If unsuccessful, the object is asked to return the smallest available primary favicon larger than
 * the target size. If this step works, the result is downscaled to create a new secondary favicon,
 * which is then stored (So subsequent requests will succeed at the first step) and returned.
 * If that step fails, the object finally walks backwards through its sequence of favicons until it
 * finds the largest primary favicon smaller than the target. This is then upscaled by a maximum of
 * 2x towards the target size, and the result cached and returned as above.
 *
 * The bitmaps themselves are encapsulated inside FaviconCacheElement objects. These objects contain,
 * as well as the bitmap, a pointer to the encapsulating FaviconsForURL object (Used by the LRU
 * culler), the size of the encapsulated image, a flag indicating if this is a primary favicon, and
 * a flag indicating if the entry is invalid.
 * All FaviconCacheElement objects are tracked in the mOrdering LinkedList. This is used to record
 * LRU information about FaviconCacheElements. In particular, the most recently used FaviconCacheElement
 * will be at the start of the list, the least recently used at the end of the list.
 *
 * When the cache runs out of space, it removes FaviconCacheElements starting from the end of the list
 * until a sufficient amount of space has been freed.
 * When a secondary favicon is removed in this way, it is simply deleted from its parent FaviconsForURLs
 * object's list of available favicons.
 * The backpointer field on the FaviconCacheElement is used to remove the element from the encapsulating
 * FaviconsForURL object, when this is required.
 * When a primary favicon is removed, its invalid flag is set to true and its bitmap payload is set
 * to null (So it is available for freeing by the garbage collector). This reduces the memory footprint
 * of the icon to essentially zero, but keeps track of which primary favicons exist for this favicon
 * URL.
 * If a subsequent request comes in for that favicon URL, it is then known that a primary of those
 * dimensions is available, just that it is not in the cache. The system is then able to load the
 * primary back into the cache from the database (Where the entirety of the initially encapsulating
 * container-formatted image file is stored).
 * If this were not done, then when processing requests after the culling of primary favicons it would
 * be impossible to distinguish between the nonexistence of a primary and the nonexistence of a primary
 * in the cache without querying the database.
 *
 * The implementation is safe to use from multiple threads and, while is it not entirely strongly
 * consistent all of the time, you almost certainly don't care.
 * The thread-safety implementation used is approximately MRSW with semaphores. An extra semaphore
 * is used to grant mutual exclusion over reordering operations from reader threads (Who thus gain
 * a quasi-writer status to do such limited mutation as is necessary).
 *
 * Reads which race with writes are liable to not see the ongoing write. The cache may return a
 * stale or now-removed value to the caller. Returned values are never invalid, even in the face
 * of concurrent reading and culling.
 */
public class FaviconCache {
    private static final String LOGTAG = "FaviconCache";

    // The number of spaces to allocate for favicons in each node.
    private static final int NUM_FAVICON_SIZES = 4;

    // Dimensions of the largest favicon to store in the cache. Everything is downscaled to this.
    public final int mMaxCachedWidth;

    // Retry failed favicons after 20 minutes.
    public static final long FAILURE_RETRY_MILLISECONDS = 1000 * 60 * 20;

    // Map relating Favicon URLs with objects representing decoded favicons.
    // Since favicons may be container formats holding multiple icons, the underlying type holds a
    // sorted list of bitmap payloads in ascending order of size. The underlying type may be queried
    // for the least larger payload currently present.
    private final ConcurrentHashMap<String, FaviconsForURL> mBackingMap = new ConcurrentHashMap<String, FaviconsForURL>();

    // A linked list used to implement a queue, defining the LRU properties of the cache. Elements
    // contained within the various FaviconsForURL objects are held here, the least recently used
    // of which at the end of the list. When space needs to be reclaimed, the appropriate bitmap is
    // culled.
    private final LinkedList<FaviconCacheElement> mOrdering = new LinkedList<FaviconCacheElement>();

    // The above structures, if used correctly, enable this cache to exhibit LRU semantics across all
    // favicon payloads in the system, as well as enabling the dynamic selection from the cache of
    // the primary bitmap most suited to the requested size (in cases where multiple primary bitmaps
    // are provided by the underlying file format).

    // Current size, in bytes, of the bitmap data present in the cache.
    private final AtomicInteger mCurrentSize = new AtomicInteger(0);

    // The maximum quantity, in bytes, of bitmap data which may be stored in the cache.
    private final int mMaxSizeBytes;

    // Tracks the number of ongoing read operations. Enables the first one in to lock writers out and
    // the last one out to let them in.
    private final AtomicInteger mOngoingReads = new AtomicInteger(0);

    // Used to ensure transaction fairness - each txn acquires and releases this as the first operation.
    // The effect is an orderly, inexpensive ordering enforced on txns to prevent writer starvation.
    private final Semaphore mTurnSemaphore = new Semaphore(1);

    // A deviation from the usual MRSW solution - this semaphore is used to guard modification to the
    // ordering map. This allows for read transactions to update the most-recently-used value without
    // needing to take out the write lock.
    private final Semaphore mReorderingSemaphore = new Semaphore(1);

    // The semaphore one must acquire in order to perform a write.
    private final Semaphore mWriteLock = new Semaphore(1);

    /**
     * Called by txns performing only reads as they start. Prevents writer starvation with a turn
     * semaphore and locks writers out if this is the first concurrent reader txn starting up.
     */
    private void startRead() {
        mTurnSemaphore.acquireUninterruptibly();
        mTurnSemaphore.release();

        if (mOngoingReads.incrementAndGet() == 1) {
            // First one in. Wait for writers to finish and lock them out.
            mWriteLock.acquireUninterruptibly();
        }
    }

    /**
     * An alternative to startWrite to be used when in a read transaction and wanting to upgrade it
     * to a write transaction. Such a transaction should be terminated with finishWrite.
     */
    private void upgradeReadToWrite() {
        mTurnSemaphore.acquireUninterruptibly();
        if (mOngoingReads.decrementAndGet() == 0) {
            mWriteLock.release();
        }
        mWriteLock.acquireUninterruptibly();
    }

    /**
     * Called by transactions performing only reads as they finish. Ensures that if this is the last
     * concluding read transaction then then writers are subsequently allowed in.
     */
    private void finishRead() {
        if (mOngoingReads.decrementAndGet() == 0) {
            mWriteLock.release();
        }
    }

    /**
     * Called by writer transactions upon start. Ensures fairness and then obtains the write lock.
     * Upon return, no other txns will be executing concurrently.
     */
    private void startWrite() {
        mTurnSemaphore.acquireUninterruptibly();
        mWriteLock.acquireUninterruptibly();
    }

    /**
     * Called by a concluding write transaction - unlocks the structure.
     */
    private void finishWrite() {
        mTurnSemaphore.release();
        mWriteLock.release();
    }

    public FaviconCache(int maxSize, int maxWidthToCache) {
        mMaxSizeBytes = maxSize;
        mMaxCachedWidth = maxWidthToCache;
    }

    /**
     * Determine if the provided favicon URL is marked as a failure (Has failed to load before -
     * such icons get blacklisted for a time to prevent us endlessly retrying.)
     *
     * @param faviconURL Favicon URL to check if failed in memcache.
     * @return true if this favicon is blacklisted, false otherwise.
     */
    public boolean isFailedFavicon(String faviconURL) {
        if (faviconURL == null) {
            return true;
        }

        startRead();

        boolean isExpired = false;
        boolean isAborting = false;

        try {
            // If we don't have it in the cache, it certainly isn't a known failure.
            if (!mBackingMap.containsKey(faviconURL)) {
                return false;
            }

            FaviconsForURL container = mBackingMap.get(faviconURL);

            // If the has failed flag is not set, it's certainly not a known failure.
            if (!container.mHasFailed) {
                return false;
            }

            final long failureTimestamp = container.mDownloadTimestamp;

            // Calculate elapsed time since the failing download.
            final long failureDiff = System.currentTimeMillis() - failureTimestamp;

            // If long enough has passed, mark it as no longer a failure.
            if (failureDiff > FAILURE_RETRY_MILLISECONDS) {
                isExpired = true;
            } else {
                return true;
            }
        } catch (Exception unhandled) {
            // Handle any exception thrown and return the locks to a sensible state.
            finishRead();

            // Flag to prevent finally from doubly-unlocking.
            isAborting = true;
            Log.e(LOGTAG, "FaviconCache exception!", unhandled);
            return true;
        }  finally {
            if (!isAborting) {
                if (isExpired) {
                    // No longer expired.
                    upgradeReadToWrite();
                } else {
                    finishRead();
                }
            }
        }

        try {
            recordRemoved(mBackingMap.get(faviconURL));
            mBackingMap.remove(faviconURL);
            return false;
        } finally {
            finishWrite();
        }
    }

    /**
     * Mark the indicated page URL as a failed Favicon until the provided time.
     *
     * @param faviconURL Page URL for which a Favicon load has failed.
     */
    public void putFailed(String faviconURL) {
        startWrite();

        if (mBackingMap.containsKey(faviconURL)) {
            recordRemoved(mBackingMap.get(faviconURL));
        }

        FaviconsForURL container = new FaviconsForURL(0, true);
        mBackingMap.put(faviconURL, container);

        finishWrite();
    }

    /**
     * Fetch a Favicon for the given URL as close as possible to the size provided.
     * If an icon of the given size is already in the cache, it is returned.
     * If an icon of the given size is not in the cache but a larger unscaled image does exist in
     * the cache, we downscale the larger image to the target size and cache the result.
     * If there is no image of the required size, null is returned.
     *
     * @param faviconURL The URL for which a Favicon is desired. Must not be null.
     * @param targetSize The size of the desired favicon.
     * @return A favicon of the requested size for the requested URL, or null if none cached.
     */
    public Bitmap getFaviconForDimensions(String faviconURL, int targetSize) {
        if (faviconURL == null) {
            Log.e(LOGTAG, "You passed a null faviconURL to getFaviconForDimensions. Don't.");
            return null;
        }

        boolean doingWrites = false;
        boolean shouldComputeColour = false;
        boolean isAborting = false;
        final Bitmap newBitmap;
        final FaviconsForURL container;

        startRead();

        try {
            if (!mBackingMap.containsKey(faviconURL)) {
                return null;
            }

            container = mBackingMap.get(faviconURL);

            FaviconCacheElement cacheElement;

            int cacheElementIndex = container.getNextHighestIndex(targetSize);

            // cacheElementIndex now holds either the index of the next least largest bitmap from
            // targetSize, or -1 if targetSize > all bitmaps.
            if (cacheElementIndex != -1) {
                // If cacheElementIndex is not the sentinel value, then it is a valid index into mFavicons.
                cacheElement = container.mFavicons.get(cacheElementIndex);

                if (cacheElement.mInvalidated) {
                    return null;
                }

                // If we found exactly what we wanted - we're done.
                if (cacheElement.mImageSize == targetSize) {
                    setMostRecentlyUsed(cacheElement);
                    return cacheElement.mFaviconPayload;
                }
            } else {
                // We requested an image larger than all primaries. Set the element to start the search
                // from to the element beyond the end of the array, so the search runs backwards.
                cacheElementIndex = container.mFavicons.size();
            }

            // We did not find exactly what we wanted, but now have set cacheElementIndex to the index
            // where what we want should live in the list. We now request the next least larger primary
            // from the cache. We will downscale this to our target size.

            // If there is no such primary, we'll upscale the next least smaller one instead.
            cacheElement = container.getNextPrimary(cacheElementIndex);


            if (cacheElement == null) {
                // The primary has been invalidated! Fail! Need to get it back from the database.
                return null;
            }

            // Having got this far, we'll be needing to write the new secondary to the cache, which
            // involves us falling through to the next try block. This flag lets us do this (Other
            // paths prior to this end in returns.)
            doingWrites = true;

            // Scaling logic...
            Bitmap largestElementBitmap = cacheElement.mFaviconPayload;
            int largestSize = cacheElement.mImageSize;

            if (largestSize >= targetSize) {
                // The largest we have is larger than the target - downsize to target.
                newBitmap = Bitmap.createScaledBitmap(largestElementBitmap, targetSize, targetSize, true);
            } else {
                // Our largest primary is smaller than the desired size. Upscale by a maximum of 2x.
                // largestSize now reflects the maximum size we can upscale to.
                largestSize *= 2;

                if (largestSize >= targetSize) {
                    // Perfect! We can upscale by less than 2x and reach the needed size. Do it.
                    newBitmap = Bitmap.createScaledBitmap(largestElementBitmap, targetSize, targetSize, true);
                } else {
                    // We don't have enough information to make the target size look nonterrible. Best effort:
                    newBitmap = Bitmap.createScaledBitmap(largestElementBitmap, largestSize, largestSize, true);

                    shouldComputeColour = true;
                }
            }
        } catch (Exception unhandled) {
            isAborting = true;

            // Handle any exception thrown and return the locks to a sensible state.
            finishRead();

            // Flag to prevent finally from doubly-unlocking.
            Log.e(LOGTAG, "FaviconCache exception!", unhandled);
            return null;
        } finally {
            if (!isAborting) {
                if (doingWrites) {
                    upgradeReadToWrite();
                } else {
                    finishRead();
                }
            }
        }

        try {
            if (shouldComputeColour) {
                // And since we failed, we'll need the dominant colour.
                container.ensureDominantColor();
            }

            // While the image might not actually BE that size, we set the size field to the target
            // because this is the best image you can get for a request of that size using the Favicon
            // information provided by this website.
            // This way, subsequent requests hit straight away.
            FaviconCacheElement newElement = container.addSecondary(newBitmap, targetSize);

            setMostRecentlyUsed(newElement);

            mCurrentSize.addAndGet(newElement.sizeOf());
        } finally {
            finishWrite();
        }

        return newBitmap;
    }

    /**
     * Query the cache for the dominant colour stored for the Favicon URL provided, if any.
     *
     * @param key The URL of the Favicon for which a dominant colour is desired.
     * @return The cached dominant colour, or null if none is cached.
     */
    public int getDominantColor(String key) {
        startRead();

        try {
            if (!mBackingMap.containsKey(key)) {
                Log.w(LOGTAG, "Cannot compute dominant color of non-cached favicon " + key);
                finishRead();
                return 0xFFFFFF;
            }

            FaviconsForURL element = mBackingMap.get(key);

            return element.ensureDominantColor();
        } finally {
            finishRead();
        }
    }

    /**
     * Remove all payloads stored in the given container from the LRU cache. Must be called while
     * holding the write lock.
     *
     * @param wasRemoved The container to purge from the cache.
     */
    private void recordRemoved(FaviconsForURL wasRemoved) {
        // If there was an existing value, strip it from the insertion-order cache.
        if (wasRemoved == null) {
            return;
        }

        int sizeRemoved = 0;

        for (FaviconCacheElement e : wasRemoved.mFavicons) {
            sizeRemoved += e.sizeOf();
            mOrdering.remove(e);
        }

        mCurrentSize.addAndGet(-sizeRemoved);
    }

    private Bitmap produceCacheableBitmap(Bitmap favicon) {
        // Never cache the default Favicon, or the null Favicon.
        if (favicon == Favicons.sDefaultFavicon || favicon == null) {
            return null;
        }

        // Some sites serve up insanely huge Favicons (Seen 512x512 ones...)
        // While we want to cache nice big icons, we apply a limit based on screen density for the
        // sake of space.
        if (favicon.getWidth() > mMaxCachedWidth) {
            return Bitmap.createScaledBitmap(favicon, mMaxCachedWidth, mMaxCachedWidth, true);
        }
        return favicon;
    }

    /**
     * Set an existing element as the most recently used element. May be called from either type of
     * transaction.
     *
     * @param element The element that is to become the most recently used one.
     */
    private void setMostRecentlyUsed(FaviconCacheElement element) {
        mReorderingSemaphore.acquireUninterruptibly();
        mOrdering.remove(element);
        mOrdering.offer(element);
        mReorderingSemaphore.release();
    }

    /**
     * Add the provided bitmap to the cache as the only available primary for this URL.
     * Should never be called with scaled Favicons. The input is assumed to be an unscaled Favicon.
     *
     * @param faviconURL The URL of the Favicon being stored.
     * @param aFavicon The Favicon to store.
     */
    public void putSingleFavicon(String faviconURL, Bitmap aFavicon) {
        Bitmap favicon = produceCacheableBitmap(aFavicon);
        if (favicon == null) {
            return;
        }

        // Create a fresh container for the favicons associated with this URL. Allocate extra slots
        // in the underlying ArrayList in case multiple secondary favicons are later created.
        // Currently set to the number of favicon sizes used in the UI, plus 1, at time of writing.
        // Ought to be  tuned as things change for maximal performance.
        FaviconsForURL toInsert = new FaviconsForURL(NUM_FAVICON_SIZES);

        // Create the cache element for the single element we are inserting, and configure it.
        FaviconCacheElement newElement = toInsert.addPrimary(favicon);

        startWrite();
        try {
            // Set the new element as the most recently used one.
            setMostRecentlyUsed(newElement);

            mCurrentSize.addAndGet(newElement.sizeOf());

            // Update the value in the LruCache...
            FaviconsForURL wasRemoved;
            wasRemoved = mBackingMap.put(faviconURL, toInsert);

            recordRemoved(wasRemoved);
        } finally {
            finishWrite();
        }

        cullIfRequired();
    }

    /**
     * Set the collection of primary favicons for the given URL to the provided collection of bitmaps.
     *
     * @param faviconURL The URL from which the favicons originate.
     * @param favicons A List of favicons decoded from this URL.
     */
    public void putFavicons(String faviconURL, Iterator<Bitmap> favicons) {
        // We don't know how many icons we'll have - let's just take a guess.
        FaviconsForURL toInsert = new FaviconsForURL(5 * NUM_FAVICON_SIZES);
        int sizeGained = 0;

        while (favicons.hasNext()) {
            Bitmap favicon = produceCacheableBitmap(favicons.next());
            if (favicon == null) {
                continue;
            }

            FaviconCacheElement newElement = toInsert.addPrimary(favicon);
            sizeGained += newElement.sizeOf();
        }

        startRead();

        boolean abortingRead = false;

        // Not using setMostRecentlyUsed, because the elements are known to be new. This can be done
        // without taking the write lock, via the magic of the reordering semaphore.
        mReorderingSemaphore.acquireUninterruptibly();
        try {
            for (FaviconCacheElement newElement : toInsert.mFavicons) {
                mOrdering.offer(newElement);
            }
        } catch (Exception e) {
            abortingRead = true;
            mReorderingSemaphore.release();
            finishRead();

            Log.e(LOGTAG, "Favicon cache exception!", e);
            return;
        } finally {
            if (!abortingRead) {
                mReorderingSemaphore.release();
                upgradeReadToWrite();
            }
        }

        try {
            mCurrentSize.addAndGet(sizeGained);

            // Update the value in the LruCache...
            recordRemoved(mBackingMap.put(faviconURL, toInsert));
        } finally {
            finishWrite();
        }

        cullIfRequired();
    }

    /**
     * If cache too large, drop stuff from the cache to get the size back into the acceptable range.
     * Otherwise, do nothing.
     */
    private void cullIfRequired() {
        Log.d(LOGTAG, "Favicon cache fullness: " + mCurrentSize.get() + '/' + mMaxSizeBytes);

        if (mCurrentSize.get() <= mMaxSizeBytes) {
            return;
        }

        startWrite();
        try {
            while (mCurrentSize.get() > mMaxSizeBytes) {
                // Cull the least recently used element.

                FaviconCacheElement victim;
                victim = mOrdering.poll();

                mCurrentSize.addAndGet(-victim.sizeOf());
                victim.onEvictedFromCache();

                Log.d(LOGTAG, "After cull: " + mCurrentSize.get() + '/' + mMaxSizeBytes);
            }
        } finally {
            finishWrite();
        }
    }

    /**
     * Purge all elements from the FaviconCache. Handy if you want to reclaim some memory.
     */
    public void evictAll() {
        startWrite();

        try {
            mBackingMap.clear();
            mOrdering.clear();
        } finally {
            finishWrite();
        }
    }
}
