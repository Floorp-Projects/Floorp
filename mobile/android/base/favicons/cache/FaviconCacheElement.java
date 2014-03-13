/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.favicons.cache;

import android.graphics.Bitmap;

/**
 * Objects stored in the Favicon cache - allow for the bitmap to be tagged to indicate if it has
 * been scaled. Unscaled bitmaps are not included in the scaled-bitmap cache's size calculation.
 */
public class FaviconCacheElement implements Comparable<FaviconCacheElement> {
    // Was this Favicon computed via scaling another primary Favicon, or is this a primary Favicon?
    final boolean isPrimary;

    // The Favicon bitmap.
    Bitmap faviconPayload;

    // If set, faviconPayload is absent. Since the underlying ICO may contain multiple primary
    // payloads, primary payloads are never truly deleted from the cache, but instead have their
    // payload deleted and this flag set on their FaviconCacheElement. That way, the cache always
    // has a record of the existence of a primary payload, even if it is no longer in the cache.
    // This means that when a request comes in that will be best served using a primary that is in
    // the database but no longer cached, we know that it exists and can go get it (Useful when ICO
    // support is added).
    volatile boolean invalidated;

    final int imageSize;

    // Used for LRU pruning.
    final FaviconsForURL backpointer;

    public FaviconCacheElement(Bitmap payload, boolean primary, int size, FaviconsForURL backpointer) {
        this.faviconPayload = payload;
        this.isPrimary = primary;
        this.imageSize = size;
        this.backpointer = backpointer;
    }

    public FaviconCacheElement(Bitmap faviconPayload, boolean isPrimary, FaviconsForURL backpointer) {
        this.faviconPayload = faviconPayload;
        this.isPrimary = isPrimary;
        this.backpointer = backpointer;

        if (faviconPayload != null) {
            imageSize = faviconPayload.getWidth();
        } else {
            imageSize = 0;
        }
    }

    public int sizeOf() {
        if (invalidated) {
            return 0;
        }
        return faviconPayload.getRowBytes() * faviconPayload.getHeight();
    }

    /**
     * Establish an ordering on FaviconCacheElements based on size and validity. An element is
     * considered "greater than" another if it is valid and the other is not, or if it contains a
     * larger payload.
     *
     * @param another The FaviconCacheElement to compare to this one.
     * @return -1 if this element is less than the given one, 1 if the other one is larger than this
     *         and 0 if both are of equal value.
     */
    @Override
    public int compareTo(FaviconCacheElement another) {
        if (invalidated && !another.invalidated) {
            return -1;
        }

        if (!invalidated && another.invalidated) {
            return 1;
        }

        if (invalidated) {
            return 0;
        }

        final int w1 = imageSize;
        final int w2 = another.imageSize;
        if (w1 > w2) {
            return 1;
        } else if (w2 > w1) {
            return -1;
        }
        return 0;
    }

    /**
     * Called when this element is evicted from the cache.
     *
     * If primary, drop the payload and set invalid. If secondary, just unlink from parent node.
     */
    public void onEvictedFromCache() {
        if (isPrimary) {
            // So we keep a record of which primaries exist in the database for this URL, we
            // don't actually delete the entry for primaries. Instead, we delete their payload
            // and flag them as invalid. This way, we can later figure out that what a request
            // really want is one of the primaries that have been dropped from the cache, and we
            // can go get it.
            invalidated = true;
            faviconPayload = null;
        } else {
            // Secondaries don't matter - just delete them.
            if (backpointer == null) {
                return;
            }
            backpointer.favicons.remove(this);
        }
    }
}
