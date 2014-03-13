/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.favicons.cache;

import android.graphics.Bitmap;
import android.util.Log;
import org.mozilla.gecko.gfx.BitmapUtils;

import java.util.ArrayList;
import java.util.Collections;

public class FaviconsForURL {
    private static final String LOGTAG = "FaviconForURL";

    private volatile int dominantColor = -1;

    final long downloadTimestamp;
    final ArrayList<FaviconCacheElement> favicons;

    public final boolean hasFailed;

    public FaviconsForURL(int size) {
        this(size, false);
    }

    public FaviconsForURL(int size, boolean failed) {
        hasFailed = failed;
        downloadTimestamp = System.currentTimeMillis();
        favicons = new ArrayList<FaviconCacheElement>(size);
    }

    public FaviconCacheElement addSecondary(Bitmap favicon, int imageSize) {
        return addInternal(favicon, false, imageSize);
    }

    public FaviconCacheElement addPrimary(Bitmap favicon) {
        return addInternal(favicon, true, favicon.getWidth());
    }

    private FaviconCacheElement addInternal(Bitmap favicon, boolean isPrimary, int imageSize) {
        FaviconCacheElement c = new FaviconCacheElement(favicon, isPrimary, imageSize, this);

        int index = Collections.binarySearch(favicons, c);

        // We've already got an equivalent one. We don't care about this new one. This only occurs in certain obscure
        // case conditions.
        if (index >= 0) {
            return favicons.get(index);
        }

        // binarySearch returns -x - 1 where x is the insertion point of the element. Convert
        // this to the actual insertion point..
        index++;
        index = -index;
        favicons.add(index, c);

        return c;
    }

    /**
     * Get the index of the smallest image in this collection larger than or equal to
     * the given target size.
     *
     * @param targetSize Minimum size for the desired result.
     * @return The index of the smallest image larger than the target size, or -1 if none exists.
     */
    public int getNextHighestIndex(int targetSize) {
        // Create a dummy object to hold the target value for comparable.
        FaviconCacheElement dummy = new FaviconCacheElement(null, false, targetSize, null);

        int index = Collections.binarySearch(favicons, dummy);

        // The search routine returns the index of an element equal to dummy, if present.
        // Otherwise, it returns -x - 1, where x is the index in the ArrayList where dummy would be
        // inserted if the list were to remain sorted.
        if (index < 0) {
            index++;
            index = -index;
        }

        // index is now 'x', as described above.

        // The routine will return favicons.size() as the index iff dummy is larger than all elements
        // present (So the "index at which it should be inserted" is the index after the end.
        // In this case, we set the sentinel value -1 to indicate that we just requested something
        // larger than all primaries.
        if (index == favicons.size()) {
            index = -1;
        }

        return index;
    }

    /**
     * Get the next valid primary icon from this collection, starting at the given index.
     * If the appropriate icon is found, but is invalid, we return null - the proper response is to
     * reacquire the primary from the database.
     * If no icon is found, the search is repeated going backwards from the start index to find any
     * primary at all (The input index may be a secondary which is larger than the actual available
     * primary.)
     *
     * @param fromIndex The index into favicons from which to start the search.
     * @return The FaviconCacheElement of the next valid primary from the given index. If none exists,
     *         then returns the previous valid primary. If none exists, returns null (Insanity.).
     */
    public FaviconCacheElement getNextPrimary(final int fromIndex) {
        final int numIcons = favicons.size();

        int searchIndex = fromIndex;
        while (searchIndex < numIcons) {
            FaviconCacheElement element = favicons.get(searchIndex);

            if (element.isPrimary) {
                if (element.invalidated) {
                    // We return null here, despite the possible existence of other primaries,
                    // because we know the most suitable primary for this request exists, but is
                    // no longer in the cache. By returning null, we cause the caller to load the
                    // missing primary from the database and call again.
                    return null;
                }
                return element;
            }
            searchIndex++;
        }

        // No larger primary available. Let's look for smaller ones...
        searchIndex = fromIndex - 1;
        while (searchIndex >= 0) {
            FaviconCacheElement element = favicons.get(searchIndex);

            if (element.isPrimary) {
                if (element.invalidated) {
                    return null;
                }
                return element;
            }
            searchIndex--;
        }

        Log.e(LOGTAG, "No primaries found in Favicon cache structure. This is madness!");

        return null;
    }

    /**
     * Ensure the dominant colour field is populated for this favicon.
     */
    public int ensureDominantColor() {
        if (dominantColor == -1) {
            // Find a payload, any payload, that is not invalidated.
            for (FaviconCacheElement element : favicons) {
                if (!element.invalidated) {
                    dominantColor = BitmapUtils.getDominantColor(element.faviconPayload);
                    return dominantColor;
                }
            }
            dominantColor = 0xFFFFFF;
        }

        return dominantColor;
    }
}
