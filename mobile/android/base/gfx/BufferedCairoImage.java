/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.GeckoAppShell;
import android.graphics.Bitmap;
import java.nio.ByteBuffer;

/** A Cairo image that simply saves a buffer of pixel data. */
public class BufferedCairoImage extends CairoImage {
    private ByteBuffer mBuffer;
    private IntSize mSize;
    private int mFormat;
    private boolean mNeedToFreeBuffer;

    /** Creates a buffered Cairo image from a byte buffer. */
    public BufferedCairoImage(ByteBuffer inBuffer, int inWidth, int inHeight, int inFormat) {
        setBuffer(inBuffer, inWidth, inHeight, inFormat);
    }

    /** Creates a buffered Cairo image from an Android bitmap. */
    public BufferedCairoImage(Bitmap bitmap) {
        setBitmap(bitmap);
    }

    private void freeBuffer() {
        if (mNeedToFreeBuffer && mBuffer != null)
            GeckoAppShell.freeDirectBuffer(mBuffer);
        mNeedToFreeBuffer = false;
        mBuffer = null;
    }

    protected void finalize() throws Throwable {
        try {
            freeBuffer();
        } finally {
            super.finalize();
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
        mNeedToFreeBuffer = true;

        int bpp = CairoUtils.bitsPerPixelForCairoFormat(mFormat);
        mBuffer = GeckoAppShell.allocateDirectBuffer(mSize.getArea() * bpp);
        bitmap.copyPixelsToBuffer(mBuffer.asIntBuffer());
    }
}

