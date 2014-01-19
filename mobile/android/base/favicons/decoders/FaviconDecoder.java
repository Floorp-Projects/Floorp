/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.favicons.decoders;

import android.graphics.Bitmap;
import org.mozilla.gecko.gfx.BitmapUtils;

import java.util.Iterator;
import java.util.NoSuchElementException;

/**
 * Class providing static utility methods for decoding favicons.
 */
public class FaviconDecoder {
    static enum ImageMagicNumbers {
        // It is irritating that Java bytes are signed...
        PNG(new byte[] {(byte) (0x89 & 0xFF), 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a}),
        GIF(new byte[] {0x47, 0x49, 0x46, 0x38}),
        JPEG(new byte[] {-0x1, -0x28, -0x1, -0x20}),
        BMP(new byte[] {0x42, 0x4d}),
        WEB(new byte[] {0x57, 0x45, 0x42, 0x50, 0x0a});

        public byte[] value;

        private ImageMagicNumbers(byte[] value) {
            this.value = value;
        }
    }

    /**
     * Check for image format magic numbers of formats supported by Android.
     * @param buffer Byte buffer to check for magic numbers
     * @param offset Offset at which to look for magic numbers.
     * @return true if the buffer contains a bitmap decodable by Android (Or at least, a sequence
     *         starting with the magic numbers thereof). false otherwise.
     */
    private static boolean isDecodableByAndroid(byte[] buffer, int offset) {
        for (ImageMagicNumbers m : ImageMagicNumbers.values()) {
            if (bufferStartsWith(buffer, m.value, offset)) {
                return true;
            }
        }

        return false;
    }

    /**
     * Utility function to check for the existence of a test byte sequence at a given offset in a
     * buffer.
     *
     * @param buffer Byte buffer to search.
     * @param test Byte sequence to search for.
     * @param bufferOffset Index in input buffer to expect test sequence.
     * @return true if buffer contains the byte sequence given in test at offset bufferOffset, false
     *         otherwise.
     */
    static boolean bufferStartsWith(byte[] buffer, byte[] test, int bufferOffset) {
        if (buffer.length < test.length) {
            return false;
        }

        for (int i = 0; i < test.length; ++i) {
            if (buffer[bufferOffset + i] != test[i]) {
                return false;
            }
        }
        return true;
    }

    /**
     * Decode the favicon present in the region of the provided byte[] starting at offset and
     * proceeding for length bytes, if any. Returns either the resulting LoadFaviconResult or null if the
     * given range does not contain a bitmap we know how to decode.
     *
     * @param buffer Byte array containing the favicon to decode.
     * @param offset The index of the first byte in the array of the region of interest.
     * @param length The length of the region in the array to decode.
     * @return The decoded version of the bitmap in the described region, or null if none can be
     *         decoded.
     */
    public static LoadFaviconResult decodeFavicon(byte[] buffer, int offset, int length) {
        LoadFaviconResult result;
        if (isDecodableByAndroid(buffer, offset)) {
            result = new LoadFaviconResult();
            result.mOffset = offset;
            result.mLength = length;
            result.mIsICO = false;

            // We assume here that decodeByteArray doesn't hold on to the entire supplied
            // buffer -- worst case, each of our buffers will be twice the necessary size.
            result.mBitmapsDecoded = new SingleBitmapIterator(BitmapUtils.decodeByteArray(buffer, offset, length));
            result.mFaviconBytes = buffer;

            return result;
        }

        // If it's not decodable by Android, it might be an ICO. Let's try.
        ICODecoder decoder = new ICODecoder(buffer, offset, length);

        result = decoder.decode();

        if (result == null) {
            return null;
        }

        return result;
    }

    public static LoadFaviconResult decodeFavicon(byte[] buffer) {
        return decodeFavicon(buffer, 0, buffer.length);
    }

    /**
     * Iterator to hold a single bitmap.
     */
    static class SingleBitmapIterator implements Iterator<Bitmap> {
        private Bitmap mBitmap;

        public SingleBitmapIterator(Bitmap b) {
            mBitmap = b;
        }

        /**
         * Slightly cheating here - this iterator supports peeking (Handy in a couple of obscure
         * places where the runtime type of the Iterator under consideration is known and
         * destruction of it is discouraged.
         *
         * @return The bitmap carried by this SingleBitmapIterator.
         */
        public Bitmap peek() {
            return mBitmap;
        }

        @Override
        public boolean hasNext() {
            return mBitmap != null;
        }

        @Override
        public Bitmap next() {
            if (mBitmap == null) {
                throw new NoSuchElementException("Element already returned from SingleBitmapIterator.");
            }

            Bitmap ret = mBitmap;
            mBitmap = null;
            return ret;
        }

        @Override
        public void remove() {
            throw new UnsupportedOperationException("remove() not supported on SingleBitmapIterator.");
        }
    }
}
