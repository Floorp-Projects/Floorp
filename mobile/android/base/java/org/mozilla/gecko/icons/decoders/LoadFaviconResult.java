/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons.decoders;

import android.graphics.Bitmap;
import android.support.annotation.Nullable;
import android.util.Log;
import android.util.SparseArray;

import java.io.ByteArrayOutputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;

/**
 * Class representing the result of loading a favicon.
 * This operation will produce either a collection of favicons, a single favicon, or no favicon.
 * It is necessary to model single favicons differently to a collection of one favicon (An entity
 * that may not exist with this scheme) since the in-database representation of these things differ.
 * (In particular, collections of favicons are stored in encoded ICO format, whereas single icons are
 * stored as decoded bitmap blobs.)
 */
public class LoadFaviconResult {
    private static final String LOGTAG = "LoadFaviconResult";

    byte[] faviconBytes;
    int offset;
    int length;

    boolean isICO;
    Iterator<Bitmap> bitmapsDecoded;

    public Iterator<Bitmap> getBitmaps() {
        return bitmapsDecoded;
    }

    /**
     * Return a representation of this result suitable for storing in the database.
     *
     * @return A byte array containing the bytes from which this result was decoded,
     *         or null if re-encoding failed.
     */
    public byte[] getBytesForDatabaseStorage() {
        // Begin by normalising the buffer.
        if (offset != 0 || length != faviconBytes.length) {
            final byte[] normalised = new byte[length];
            System.arraycopy(faviconBytes, offset, normalised, 0, length);
            offset = 0;
            faviconBytes = normalised;
        }

        // For results containing multiple images, we store the result verbatim. (But cutting the
        // buffer to size first).
        // We may instead want to consider re-encoding the entire ICO as a collection of efficiently
        // encoded PNGs. This may not be worth the CPU time (Indeed, the encoding of single-image
        // favicons may also not be worth the time/space tradeoff.).
        if (isICO) {
            return faviconBytes;
        }

        // For results containing a single image, we re-encode the
        // result as a PNG in an effort to save space.
        final Bitmap favicon = ((FaviconDecoder.SingleBitmapIterator) bitmapsDecoded).peek();
        final ByteArrayOutputStream stream = new ByteArrayOutputStream();

        try {
            if (favicon.compress(Bitmap.CompressFormat.PNG, 100, stream)) {
                return stream.toByteArray();
            }
        } catch (OutOfMemoryError e) {
            Log.w(LOGTAG, "Out of memory re-compressing favicon.");
        }

        Log.w(LOGTAG, "Favicon re-compression failed.");
        return null;
    }

    @Nullable
    public Bitmap getBestBitmap(int targetWidthAndHeight) {
        final SparseArray<Bitmap> iconMap = new SparseArray<>();
        final List<Integer> sizes = new ArrayList<>();

        while (bitmapsDecoded.hasNext()) {
            final Bitmap b = bitmapsDecoded.next();

            // It's possible to receive null, most likely due to OOM or a zero-sized image,
            // from BitmapUtils.decodeByteArray(byte[], int, int, BitmapFactory.Options)
            if (b != null) {
                iconMap.put(b.getWidth(), b);
                sizes.add(b.getWidth());
            }
        }

        int bestSize = selectBestSizeFromList(sizes, targetWidthAndHeight);

        if (bestSize == -1) {
            // No icons found: this could occur if we weren't able to process any of the
            // supplied icons.
            return null;
        }

        return iconMap.get(bestSize);
    }

    /**
     * Select the closest icon size from a list of icon sizes.
     * We just find the first icon that is larger than the preferred size if available, or otherwise select the
     * largest icon (if all icons are smaller than the preferred size).
     *
     * @return The closest icon size, or -1 if no sizes are supplied.
     */
    public static int selectBestSizeFromList(final List<Integer> sizes, final int preferredSize) {
        if (sizes.isEmpty()) {
            // This isn't ideal, however current code assumes this as an error value for now.
            return -1;
        }

        Collections.sort(sizes);

        for (int size : sizes) {
            if (size >= preferredSize) {
                return size;
            }
        }

        // If all icons are smaller than the preferred size then we don't have an icon
        // selected yet, therefore just take the largest (last) icon.
        return sizes.get(sizes.size() - 1);
    }
}
