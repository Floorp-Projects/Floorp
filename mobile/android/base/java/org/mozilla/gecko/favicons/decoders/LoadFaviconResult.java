/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.favicons.decoders;

import android.graphics.Bitmap;
import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.util.Iterator;

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

}
