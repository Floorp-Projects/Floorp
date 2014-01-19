/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.favicons.decoders;

/**
 * Representation of an ICO file ICONDIRENTRY structure.
 */
public class IconDirectoryEntry implements Comparable<IconDirectoryEntry> {

    public static int sMaxBPP;

    int mWidth;
    int mHeight;
    int mPaletteSize;
    int mBitsPerPixel;
    int mPayloadSize;
    int mPayloadOffset;
    boolean mPayloadIsPNG;

    // Tracks the index in the Icon Directory of this entry. Useful only for pruning.
    int mIndex;
    boolean mIsErroneous;

    public IconDirectoryEntry(int width, int height, int paletteSize, int bitsPerPixel, int payloadSize, int payloadOffset, boolean payloadIsPNG) {
        mWidth = width;
        mHeight = height;
        mPaletteSize = paletteSize;
        mBitsPerPixel = bitsPerPixel;
        mPayloadSize = payloadSize;
        mPayloadOffset = payloadOffset;
        mPayloadIsPNG = payloadIsPNG;
    }

    /**
     * Method to get a dummy Icon Directory Entry with the Erroneous bit set.
     *
     * @return An erroneous placeholder Icon Directory Entry.
     */
    public static IconDirectoryEntry getErroneousEntry() {
        IconDirectoryEntry ret = new IconDirectoryEntry(-1, -1, -1, -1, -1, -1, false);
        ret.mIsErroneous = true;

        return ret;
    }

    /**
     * Create an IconDirectoryEntry object from a byte[]. Interprets the buffer starting at the given
     * offset as an IconDirectoryEntry and returns the result.
     *
     * @param buffer Byte array containing the icon directory entry to decode.
     * @param regionOffset Offset into the byte array of the valid region of the buffer.
     * @param regionLength Length of the valid region in the buffer.
     * @param entryOffset Offset of the icon directory entry to decode within the buffer.
     * @return An IconDirectoryEntry object representing the entry specified, or null if the entry
     *         is obviously invalid.
     */
    public static IconDirectoryEntry createFromBuffer(byte[] buffer, int regionOffset, int regionLength, int entryOffset) {
        // Verify that the reserved field is really zero.
        if (buffer[entryOffset + 3] != 0) {
            return getErroneousEntry();
        }

        // Verify that the entry points to a region that actually exists in the buffer, else bin it.
        int fieldPtr = entryOffset + 8;
        int entryLength = (buffer[fieldPtr] & 0xFF) |
                          (buffer[fieldPtr + 1] & 0xFF) << 8 |
                          (buffer[fieldPtr + 2] & 0xFF) << 16 |
                          (buffer[fieldPtr + 3] & 0xFF) << 24;

        // Advance to the offset field.
        fieldPtr += 4;

        int payloadOffset = (buffer[fieldPtr] & 0xFF) |
                            (buffer[fieldPtr + 1] & 0xFF) << 8 |
                            (buffer[fieldPtr + 2] & 0xFF) << 16 |
                            (buffer[fieldPtr + 3] & 0xFF) << 24;

        // Fail if the entry describes a region outside the buffer.
        if (payloadOffset < 0 || entryLength < 0 || payloadOffset + entryLength > regionOffset + regionLength) {
            return getErroneousEntry();
        }

        // Extract the image dimensions.
        int imageWidth = buffer[entryOffset] & 0xFF;
        int imageHeight = buffer[entryOffset+1] & 0xFF;

        // Because Microsoft, a size value of zero represents an image size of 256.
        if (imageWidth == 0) {
            imageWidth = 256;
        }

        if (imageHeight == 0) {
            imageHeight = 256;
        }

        // If the image uses a colour palette, this is the number of colours, otherwise this is zero.
        int paletteSize = buffer[entryOffset + 2] & 0xFF;

        // The plane count - usually 0 or 1. When > 1, taken as multiplier on bitsPerPixel.
        int colorPlanes = buffer[entryOffset + 4] & 0xFF;

        int bitsPerPixel = (buffer[entryOffset + 6] & 0xFF) |
                           (buffer[entryOffset + 7] & 0xFF) << 8;

        if (colorPlanes > 1) {
            bitsPerPixel *= colorPlanes;
        }

        // Look for PNG magic numbers at the start of the payload.
        boolean payloadIsPNG = FaviconDecoder.bufferStartsWith(buffer, FaviconDecoder.ImageMagicNumbers.PNG.value, regionOffset + payloadOffset);

        return new IconDirectoryEntry(imageWidth, imageHeight, paletteSize, bitsPerPixel, entryLength, payloadOffset, payloadIsPNG);
    }

    /**
     * Get the number of bytes from the start of the ICO file to the beginning of this entry.
     */
    public int getOffset() {
        return ICODecoder.ICO_HEADER_LENGTH_BYTES + (mIndex * ICODecoder.ICO_ICONDIRENTRY_LENGTH_BYTES);
    }

    @Override
    public int compareTo(IconDirectoryEntry another) {
        if (mWidth > another.mWidth) {
            return 1;
        }

        if (mWidth < another.mWidth) {
            return -1;
        }

        // Where both images exceed the max BPP, take the smaller of the two BPP values.
        if (mBitsPerPixel >= sMaxBPP && another.mBitsPerPixel >= sMaxBPP) {
            if (mBitsPerPixel < another.mBitsPerPixel) {
                return 1;
            }

            if (mBitsPerPixel > another.mBitsPerPixel) {
                return -1;
            }
        }

        // Otherwise, take the larger of the BPP values.
        if (mBitsPerPixel > another.mBitsPerPixel) {
            return 1;
        }

        if (mBitsPerPixel < another.mBitsPerPixel) {
            return -1;
        }

        // Prefer large palettes.
        if (mPaletteSize > another.mPaletteSize) {
            return 1;
        }

        if (mPaletteSize < another.mPaletteSize) {
            return -1;
        }

        // Prefer smaller payloads.
        if (mPayloadSize < another.mPayloadSize) {
            return 1;
        }

        if (mPayloadSize > another.mPayloadSize) {
            return -1;
        }

        // If all else fails, prefer PNGs over BMPs. They tend to be smaller.
        if (mPayloadIsPNG && !another.mPayloadIsPNG) {
            return 1;
        }

        if (!mPayloadIsPNG && another.mPayloadIsPNG) {
            return -1;
        }

        return 0;
    }

    public static void setMaxBPP(int maxBPP) {
        sMaxBPP = maxBPP;
    }

    @Override
    public String toString() {
        return "IconDirectoryEntry{" +
                "\nmWidth=" + mWidth +
                ", \nmHeight=" + mHeight +
                ", \nmPaletteSize=" + mPaletteSize +
                ", \nmBitsPerPixel=" + mBitsPerPixel +
                ", \nmPayloadSize=" + mPayloadSize +
                ", \nmPayloadOffset=" + mPayloadOffset +
                ", \nmPayloadIsPNG=" + mPayloadIsPNG +
                ", \nmIndex=" + mIndex +
                '}';
    }
}
