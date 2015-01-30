/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.mozglue.DirectBufferAllocator;

import android.graphics.Bitmap;
import android.util.Log;

import java.nio.ByteBuffer;

/** A buffered image that simply saves a buffer of pixel data. */
public class BufferedImage {
    private ByteBuffer mBuffer;
    private IntSize mSize;
    private int mFormat;

    private static final String LOGTAG = "GeckoBufferedImage";

    /** Creates an empty buffered image */
    public BufferedImage() {
        mSize = new IntSize(0, 0);
    }

    /** Creates a buffered image from an Android bitmap. */
    public BufferedImage(Bitmap bitmap) {
        mFormat = bitmapConfigToFormat(bitmap.getConfig());
        mSize = new IntSize(bitmap.getWidth(), bitmap.getHeight());

        int bpp = bitsPerPixelForFormat(mFormat);
        mBuffer = DirectBufferAllocator.allocate(mSize.getArea() * bpp);
        bitmap.copyPixelsToBuffer(mBuffer.asIntBuffer());
    }

    private synchronized void freeBuffer() {
        mBuffer = DirectBufferAllocator.free(mBuffer);
    }

    public void destroy() {
        try {
            freeBuffer();
        } catch (Exception ex) {
            Log.e(LOGTAG, "error clearing buffer: ", ex);
        }
    }

    public ByteBuffer getBuffer() { return mBuffer; }
    public IntSize getSize() { return mSize; }
    public int getFormat() { return mFormat; }

    public static final int FORMAT_INVALID = -1;
    public static final int FORMAT_ARGB32 = 0;
    public static final int FORMAT_RGB24 = 1;
    public static final int FORMAT_A8 = 2;
    public static final int FORMAT_A1 = 3;
    public static final int FORMAT_RGB16_565 = 4;

    private static int bitsPerPixelForFormat(int format) {
        switch (format) {
        case FORMAT_A1:          return 1;
        case FORMAT_A8:          return 8;
        case FORMAT_RGB16_565:   return 16;
        case FORMAT_RGB24:       return 24;
        case FORMAT_ARGB32:      return 32;
        default:
            throw new RuntimeException("Unknown Cairo format");
        }
    }

    private static int bitmapConfigToFormat(Bitmap.Config config) {
        if (config == null)
            return FORMAT_ARGB32;    /* Droid Pro fix. */

        switch (config) {
        case ALPHA_8:   return FORMAT_A8;
        case ARGB_4444: throw new RuntimeException("ARGB_444 unsupported");
        case ARGB_8888: return FORMAT_ARGB32;
        case RGB_565:   return FORMAT_RGB16_565;
        default:        throw new RuntimeException("Unknown Skia bitmap config");
        }
    }
}
