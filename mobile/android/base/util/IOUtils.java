/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.util.Log;

import java.io.IOException;
import java.io.InputStream;

/**
 * Static helper class containing useful methods for manipulating IO objects.
 */
public class IOUtils {
    private static final String LOGTAG = "GeckoIOUtils";

    /**
     * Represents the result of consuming an input stream, holding the returned data as well
     * as the length of the data returned.
     * The byte[] is not guaranteed to be trimmed to the size of the data acquired from the stream:
     * hence the need for the length field. This strategy avoids the need to copy the data into a
     * trimmed buffer after consumption.
     */
    public static class ConsumedInputStream {
        public final int consumedLength;
        // Only reassigned in getTruncatedData.
        private byte[] consumedData;

        public ConsumedInputStream(int consumedLength, byte[] consumedData) {
            this.consumedLength = consumedLength;
            this.consumedData = consumedData;
        }

        /**
         * Get the data trimmed to the length of the actual payload read, caching the result.
         */
        public byte[] getTruncatedData() {
            if (consumedData.length == consumedLength) {
                return consumedData;
            }

            consumedData = truncateBytes(consumedData, consumedLength);
            return consumedData;
        }

        public byte[] getData() {
            return consumedData;
        }
    }

    /**
     * Fully read an InputStream into a byte array.
     * @param iStream the InputStream to consume.
     * @param bufferSize The initial size of the buffer to allocate. It will be grown as
     *                   needed, but if the caller knows something about the InputStream then
     *                   passing a good value here can improve performance.
     */
    public static ConsumedInputStream readFully(InputStream iStream, int bufferSize) {
        // Allocate a buffer to hold the raw data downloaded.
        byte[] buffer = new byte[bufferSize];

        // The offset of the start of the buffer's free space.
        int bPointer = 0;

        // The quantity of bytes the last call to read yielded.
        int lastRead = 0;
        try {
            // Fully read the data into the buffer.
            while (lastRead != -1) {
                // Read as many bytes as are currently available into the buffer.
                lastRead = iStream.read(buffer, bPointer, buffer.length - bPointer);
                bPointer += lastRead;

                // If buffer has overflowed, double its size and carry on.
                if (bPointer == buffer.length) {
                    bufferSize *= 2;
                    byte[] newBuffer = new byte[bufferSize];

                    // Copy the contents of the old buffer into the new buffer.
                    System.arraycopy(buffer, 0, newBuffer, 0, buffer.length);
                    buffer = newBuffer;
                }
            }

            return new ConsumedInputStream(bPointer + 1, buffer);
        } catch (IOException e) {
            Log.e(LOGTAG, "Error consuming input stream.", e);
        } finally {
            try {
                iStream.close();
            } catch (IOException e) {
                Log.e(LOGTAG, "Error closing input stream.", e);
            }
        }

        return null;
    }

    /**
     * Truncate a given byte[] to a given length. Returns a new byte[] with the first length many
     * bytes of the input.
     */
    public static byte[] truncateBytes(byte[] bytes, int length) {
        byte[] newBytes = new byte[length];
        System.arraycopy(bytes, 0, newBytes, 0, length);

        return newBytes;
    }
}
