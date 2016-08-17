/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons.decoders;

import android.content.Context;
import android.graphics.Bitmap;
import android.util.SparseArray;

import java.util.Iterator;
import java.util.NoSuchElementException;

import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.R;

/**
 * Utility class for determining the region of a provided array which contains the largest bitmap,
 * assuming the provided array is a valid ICO and the bitmap desired is square, and for pruning
 * unwanted entries from ICO files, if desired.
 *
 * An ICO file is a container format that may hold up to 255 images in either BMP or PNG format.
 * A mixture of image types may not exist.
 *
 * The format consists of a header specifying the number, n,  of images, followed by the Icon Directory.
 *
 * The Icon Directory consists of n Icon Directory Entries, each 16 bytes in length, specifying, for
 * the corresponding image, the dimensions, colour information, payload size, and location in the file.
 *
 * All numerical fields follow a little-endian byte ordering.
 *
 * Header format:
 *
 *  0               1               2               3
 *  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |  Reserved field. Must be zero |  Type (1 for ICO, 2 for CUR)  |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |         Image count (n)       |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * The type field is expected to always be 1. CUR format images should not be used for Favicons.
 *
 *
 * Icon Directory Entry format:
 *
 *  0               1               2               3
 *  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |  Image width  | Image height  | Palette size  | Reserved (0)  |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |       Colour plane count      |         Bits per pixel        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                   Size of image data, in bytes                |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |      Start of image data, as an offset from start of file     |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * Image dimensions of zero are to be interpreted as image dimensions of 256.
 *
 * The palette size field records the number of colours in the stored BMP, if a palette is used. Zero
 * if the payload is a PNG or no palette is in use.
 *
 * The number of colour planes is, usually, 0 (Not in use) or 1. Values greater than 1 are to be
 * interpreted not as a colour plane count, but as a multiplying factor on the bits per pixel field.
 * (Apparently 65535 was not deemed a sufficiently large maximum value of bits per pixel.)
 *
 *
 * The Icon Directory consists of n-many Icon Directory Entries in sequence, with no gaps.
 *
 * This class is not thread safe.
 */
public class ICODecoder implements Iterable<Bitmap> {
    // The number of bytes that compacting will save for us to bother doing it.
    public static final int COMPACT_THRESHOLD = 4000;

    // Some geometry of an ICO file.
    public static final int ICO_HEADER_LENGTH_BYTES = 6;
    public static final int ICO_ICONDIRENTRY_LENGTH_BYTES = 16;

    // The buffer containing bytes to attempt to decode.
    private byte[] decodand;

    // The region of the decodand to decode.
    private int offset;
    private int len;

    IconDirectoryEntry[] iconDirectory;
    private boolean isValid;
    private boolean hasDecoded;
    private int largestFaviconSize;

    public ICODecoder(Context context, byte[] decodand, int offset, int len) {
        this.decodand = decodand;
        this.offset = offset;
        this.len = len;
        this.largestFaviconSize = context.getResources()
                .getDimensionPixelSize(R.dimen.favicon_largest_interesting_size);
    }

    /**
     * Decode the Icon Directory for this ICO and store the result in iconDirectory.
     *
     * @return true if ICO decoding was considered to probably be a success, false if it certainly
     *         was a failure.
     */
    private boolean decodeIconDirectoryAndPossiblyPrune() {
        hasDecoded = true;

        // Fail if the end of the described range is out of bounds.
        if (offset + len > decodand.length) {
            return false;
        }

        // Fail if we don't have enough space for the header.
        if (len < ICO_HEADER_LENGTH_BYTES) {
            return false;
        }

        // Check that the reserved fields in the header are indeed zero, and that the type field
        // specifies ICO. If not, we've probably been given something that isn't really an ICO.
        if (decodand[offset] != 0 ||
            decodand[offset + 1] != 0 ||
            decodand[offset + 2] != 1 ||
            decodand[offset + 3] != 0) {
            return false;
        }

        // Here, and in many other places, byte values are ANDed with 0xFF. This is because Java
        // bytes are signed - to obtain a numerical value of a longer type which holds the unsigned
        // interpretation of the byte of interest, we do this.
        int numEncodedImages = (decodand[offset + 4] & 0xFF) |
                               (decodand[offset + 5] & 0xFF) << 8;


        // Fail if there are no images or the field is corrupt.
        if (numEncodedImages <= 0) {
            return false;
        }

        final int headerAndDirectorySize = ICO_HEADER_LENGTH_BYTES + (numEncodedImages * ICO_ICONDIRENTRY_LENGTH_BYTES);

        // Fail if there is not enough space in the buffer for the stated number of icondir entries,
        // let alone the data.
        if (len < headerAndDirectorySize) {
            return false;
        }

        // Put the pointer on the first byte of the first Icon Directory Entry.
        int bufferIndex = offset + ICO_HEADER_LENGTH_BYTES;

        // We now iterate over the Icon Directory, decoding each entry as we go. We also need to
        // discard all entries except one >= the maximum interesting size.

        // Size of the smallest image larger than the limit encountered.
        int minimumMaximum = Integer.MAX_VALUE;

        // Used to track the best entry for each size. The entries we want to keep.
        SparseArray<IconDirectoryEntry> preferenceArray = new SparseArray<IconDirectoryEntry>();

        for (int i = 0; i < numEncodedImages; i++, bufferIndex += ICO_ICONDIRENTRY_LENGTH_BYTES) {
            // Decode the Icon Directory Entry at this offset.
            IconDirectoryEntry newEntry = IconDirectoryEntry.createFromBuffer(decodand, offset, len, bufferIndex);
            newEntry.index = i;

            if (newEntry.isErroneous) {
                continue;
            }

            if (newEntry.width > largestFaviconSize) {
                // If we already have a smaller image larger than the maximum size of interest, we
                // don't care about the new one which is larger than the smallest image larger than
                // the maximum size.
                if (newEntry.width >= minimumMaximum) {
                    continue;
                }

                // Remove the previous minimum-maximum.
                preferenceArray.delete(minimumMaximum);

                minimumMaximum = newEntry.width;
            }

            IconDirectoryEntry oldEntry = preferenceArray.get(newEntry.width);
            if (oldEntry == null) {
                preferenceArray.put(newEntry.width, newEntry);
                continue;
            }

            if (oldEntry.compareTo(newEntry) < 0) {
                preferenceArray.put(newEntry.width, newEntry);
            }
        }

        final int count = preferenceArray.size();

        // Abort if no entries are desired (Perhaps all are corrupt?)
        if (count == 0) {
            return false;
        }

        // Allocate space for the icon directory entries in the decoded directory.
        iconDirectory = new IconDirectoryEntry[count];

        // The size of the data in the buffer that we find useful.
        int retainedSpace = ICO_HEADER_LENGTH_BYTES;

        for (int i = 0; i < count; i++) {
            IconDirectoryEntry e = preferenceArray.valueAt(i);
            retainedSpace += ICO_ICONDIRENTRY_LENGTH_BYTES + e.payloadSize;
            iconDirectory[i] = e;
        }

        isValid = true;

        // Set the number of images field in the buffer to reflect the number of retained entries.
        decodand[offset + 4] = (byte) iconDirectory.length;
        decodand[offset + 5] = (byte) (iconDirectory.length >>> 8);

        if ((len - retainedSpace) > COMPACT_THRESHOLD) {
            compactingCopy(retainedSpace);
        }

        return true;
    }

    /**
     * Copy the buffer into a new array of exactly the required size, omitting any unwanted data.
     */
    private void compactingCopy(int spaceRetained) {
        byte[] buf = new byte[spaceRetained];

        // Copy the header.
        System.arraycopy(decodand, offset, buf, 0, ICO_HEADER_LENGTH_BYTES);

        int headerPtr = ICO_HEADER_LENGTH_BYTES;

        int payloadPtr = ICO_HEADER_LENGTH_BYTES + (iconDirectory.length * ICO_ICONDIRENTRY_LENGTH_BYTES);

        int ind = 0;
        for (IconDirectoryEntry entry : iconDirectory) {
            // Copy this entry.
            System.arraycopy(decodand, offset + entry.getOffset(), buf, headerPtr, ICO_ICONDIRENTRY_LENGTH_BYTES);

            // Copy its payload.
            System.arraycopy(decodand, offset + entry.payloadOffset, buf, payloadPtr, entry.payloadSize);

            // Update the offset field.
            buf[headerPtr + 12] = (byte) payloadPtr;
            buf[headerPtr + 13] = (byte) (payloadPtr >>> 8);
            buf[headerPtr + 14] = (byte) (payloadPtr >>> 16);
            buf[headerPtr + 15] = (byte) (payloadPtr >>> 24);

            entry.payloadOffset = payloadPtr;
            entry.index = ind;

            payloadPtr += entry.payloadSize;
            headerPtr += ICO_ICONDIRENTRY_LENGTH_BYTES;
            ind++;
        }

        decodand = buf;
        offset = 0;
        len = spaceRetained;
    }

    /**
     * Decode and return the bitmap represented by the given index in the Icon Directory, if valid.
     *
     * @param index The index into the Icon Directory of the image of interest.
     * @return The decoded Bitmap object for this image, or null if the entry is invalid or decoding
     *         fails.
     */
    public Bitmap decodeBitmapAtIndex(int index) {
        final IconDirectoryEntry iconDirEntry = iconDirectory[index];

        if (iconDirEntry.payloadIsPNG) {
            // PNG payload. Simply extract it and decode it.
            return BitmapUtils.decodeByteArray(decodand, offset + iconDirEntry.payloadOffset, iconDirEntry.payloadSize);
        }

        // The payload is a BMP, so we need to do some magic to get the decoder to do what we want.
        // We construct an ICO containing just the image we want, and let Android do the rest.
        byte[] decodeTarget = new byte[ICO_HEADER_LENGTH_BYTES + ICO_ICONDIRENTRY_LENGTH_BYTES + iconDirEntry.payloadSize];

        // Set the type field in the ICO header.
        decodeTarget[2] = 1;

        // Set the num-images field in the header to 1.
        decodeTarget[4] = 1;

        // Copy the ICONDIRENTRY we need into the new buffer.
        System.arraycopy(decodand, offset + iconDirEntry.getOffset(), decodeTarget, ICO_HEADER_LENGTH_BYTES, ICO_ICONDIRENTRY_LENGTH_BYTES);

        // Copy the payload into the new buffer.
        final int singlePayloadOffset =  ICO_HEADER_LENGTH_BYTES + ICO_ICONDIRENTRY_LENGTH_BYTES;
        System.arraycopy(decodand, offset + iconDirEntry.payloadOffset, decodeTarget, singlePayloadOffset, iconDirEntry.payloadSize);

        // Update the offset field of the ICONDIRENTRY to make the new ICO valid.
        decodeTarget[ICO_HEADER_LENGTH_BYTES + 12] = singlePayloadOffset;
        decodeTarget[ICO_HEADER_LENGTH_BYTES + 13] = (singlePayloadOffset >>> 8);
        decodeTarget[ICO_HEADER_LENGTH_BYTES + 14] = (singlePayloadOffset >>> 16);
        decodeTarget[ICO_HEADER_LENGTH_BYTES + 15] = (singlePayloadOffset >>> 24);

        // Decode the newly-constructed singleton-ICO.
        return BitmapUtils.decodeByteArray(decodeTarget);
    }

    /**
     * Fetch an iterator over the images in this ICO, or null if this ICO seems to be invalid.
     *
     * @return An iterator over the Bitmaps stored in this ICO, or null if decoding fails.
     */
    @Override
    public ICOIterator iterator() {
        // If a previous call to decode concluded this ICO is invalid, abort.
        if (hasDecoded && !isValid) {
            return null;
        }

        // If we've not been decoded before, but now fail to make any sense of the ICO, abort.
        if (!hasDecoded) {
            if (!decodeIconDirectoryAndPossiblyPrune()) {
                return null;
            }
        }

        // If decoding was a success, return an iterator over the images in this ICO.
        return new ICOIterator();
    }

    /**
     * Decode this ICO and return the result as a LoadFaviconResult.
     * @return A LoadFaviconResult representing the decoded ICO.
     */
    public LoadFaviconResult decode() {
        // The call to iterator returns null if decoding fails.
        Iterator<Bitmap> bitmaps = iterator();
        if (bitmaps == null) {
            return null;
        }

        LoadFaviconResult result = new LoadFaviconResult();

        result.bitmapsDecoded = bitmaps;
        result.faviconBytes = decodand;
        result.offset = offset;
        result.length = len;
        result.isICO = true;

        return result;
    }

    /**
     * Inner class to iterate over the elements in the ICO represented by the enclosing instance.
     */
    private class ICOIterator implements Iterator<Bitmap> {
        private int mIndex;

        @Override
        public boolean hasNext() {
            return mIndex < iconDirectory.length;
        }

        @Override
        public Bitmap next() {
            if (mIndex > iconDirectory.length) {
                throw new NoSuchElementException("No more elements in this ICO.");
            }
            return decodeBitmapAtIndex(mIndex++);
        }

        @Override
        public void remove() {
            if (iconDirectory[mIndex] == null) {
                throw new IllegalStateException("Remove already called for element " + mIndex);
            }
            iconDirectory[mIndex] = null;
        }
    }
}
