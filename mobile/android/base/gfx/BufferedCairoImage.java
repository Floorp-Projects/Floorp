/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.mozglue.DirectBufferAllocator;

import android.graphics.Bitmap;
import android.util.Log;

import java.nio.ByteBuffer;

/** A Cairo image that simply saves a buffer of pixel data. */
public class BufferedCairoImage extends CairoImage {
    private ByteBuffer mBuffer;
    private IntSize mSize;
    private int mFormat;

    private static String LOGTAG = "GeckoBufferedCairoImage";

    /** Creates a buffered Cairo image from a byte buffer. */
    public BufferedCairoImage(ByteBuffer inBuffer, int inWidth, int inHeight, int inFormat) {
        setBuffer(inBuffer, inWidth, inHeight, inFormat);
    }

    /** Creates a buffered Cairo image from an Android bitmap. */
    public BufferedCairoImage(Bitmap bitmap) {
        setBitmap(bitmap);
    }

    private synchronized void freeBuffer() {
        mBuffer = DirectBufferAllocator.free(mBuffer);
    }

    @Override
    public void destroy() {
        try {
            freeBuffer();
        } catch (Exception ex) {
            Log.e(LOGTAG, "error clearing buffer: ", ex);
        }
    }

    @Override
    public ByteBuffer getBuffer() { return mBuffer; }
    @Override
    public IntSize getSize() { return mSize; }
    @Override
    public int getFormat() { return mFormat; }


    public void setBuffer(ByteBuffer buffer, int width, int height, int format) {
        freeBuffer();
        mBuffer = buffer;
        mSize = new IntSize(width, height);
        mFormat = format;
    }

    public void setBitmap(Bitmap bitmap) {
        mFormat = CairoUtils.bitmapConfigToCairoFormat(bitmap.getConfig());
        mSize = new IntSize(bitmap.getWidth(), bitmap.getHeight());

        int bpp = CairoUtils.bitsPerPixelForCairoFormat(mFormat);
        mBuffer = DirectBufferAllocator.allocate(mSize.getArea() * bpp);
        bitmap.copyPixelsToBuffer(mBuffer.asIntBuffer());
    }
}
