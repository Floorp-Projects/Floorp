/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.mozglue.DirectBufferAllocator;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.util.Log;

import java.nio.ByteBuffer;

public class ScreenshotLayer extends SingleTileLayer {
    private static final int SCREENSHOT_SIZE_LIMIT = 1048576;
    private static final int BYTES_FOR_16BPP = 2;
    private ScreenshotImage mImage;
    // Size of the image buffer
    private IntSize mBufferSize;
    // The size of the bitmap painted in the buffer
    // (may be smaller than mBufferSize due to power of 2 padding)
    private IntSize mImageSize;
    // Whether we have an up-to-date image to draw
    private boolean mHasImage;
    private static String LOGTAG = "GeckoScreenshot";

    public static int getMaxNumPixels() {
        return SCREENSHOT_SIZE_LIMIT;
    }

    public void reset() {
        mHasImage = false;
    }

    void setBitmap(ByteBuffer data, int width, int height, Rect rect) throws IllegalArgumentException {
        mImageSize = new IntSize(width, height);
        if (IntSize.isPowerOfTwo(width) && IntSize.isPowerOfTwo(height)) {
            mBufferSize = mImageSize;
            mHasImage = true;
            mImage.setBitmap(data, width, height, CairoImage.FORMAT_RGB16_565, rect);
        } else {
            throw new IllegalArgumentException("### unexpected size in setBitmap: w="+width+" h="+height);
        }
    }
    
    void setBitmap(Bitmap bitmap) throws IllegalArgumentException {
        mImageSize = new IntSize(bitmap.getWidth(), bitmap.getHeight());
        int width = IntSize.nextPowerOfTwo(bitmap.getWidth());
        int height = IntSize.nextPowerOfTwo(bitmap.getHeight());
        mBufferSize = new IntSize(width, height);
        mImage.setBitmap(bitmap, width, height, CairoImage.FORMAT_RGB16_565);
        mHasImage = true;
    }

    public static ScreenshotLayer create() {
        return ScreenshotLayer.create(new IntSize(4, 4));
    }

    public static ScreenshotLayer create(IntSize size) {
        Bitmap bitmap = Bitmap.createBitmap(size.width, size.height, Bitmap.Config.RGB_565);
        ScreenshotLayer sl = create(bitmap);
        sl.reset();
        return sl;
    }

    public static ScreenshotLayer create(Bitmap bitmap) {
        IntSize size = new IntSize(bitmap.getWidth(), bitmap.getHeight());
        // allocate a buffer that can hold our max screenshot size
        ByteBuffer buffer = DirectBufferAllocator.allocate(SCREENSHOT_SIZE_LIMIT * BYTES_FOR_16BPP);
        // construct the screenshot layer
        ScreenshotLayer sl =  new ScreenshotLayer(new ScreenshotImage(buffer, size.width, size.height, CairoImage.FORMAT_RGB16_565), size);
        // paint the passed in bitmap into the buffer
        try {
            sl.setBitmap(bitmap);
        } catch (IllegalArgumentException ex) {
            Log.e(LOGTAG, "error setting bitmap: ", ex);
        }
        return sl;
    }

    private ScreenshotLayer(ScreenshotImage image, IntSize size) {
        super(image, TileLayer.PaintMode.NORMAL);
        mBufferSize = size;
        mImage = image;
    }

    @Override
    public void draw(RenderContext context) {
        if (mHasImage)
            super.draw(context);
    }

    /** A Cairo image that simply saves a buffer of pixel data. */
    static class ScreenshotImage extends CairoImage {
        private ByteBuffer mBuffer;
        private IntSize mSize;
        private int mFormat;

        /** Creates a buffered Cairo image from a byte buffer. */
        public ScreenshotImage(ByteBuffer inBuffer, int inWidth, int inHeight, int inFormat) {
            mBuffer = inBuffer;
            mSize = new IntSize(inWidth, inHeight);
            mFormat = inFormat;
        }

        @Override
        protected void finalize() throws Throwable {
            try {
                DirectBufferAllocator.free(mBuffer);
                mBuffer = null;
            } finally {
                super.finalize();
            }
        }

        void copyBuffer(ByteBuffer src, ByteBuffer dst, Rect rect, int stride) {
            int start = (rect.top * stride) + (rect.left * BYTES_FOR_16BPP);
            int end = ((rect.bottom - 1) * stride) + (rect.right * BYTES_FOR_16BPP);
            // clamp stuff just to be safe
            start = Math.max(0, Math.min(dst.limit(), Math.min(src.limit(), start)));
            end = Math.max(start, Math.min(dst.limit(), Math.min(src.capacity(), end)));
            dst.position(start);
            src.position(start).limit(end);
            dst.put(src);
        }

        synchronized void setBitmap(ByteBuffer data, int width, int height, int format, Rect rect) {
            mSize = new IntSize(width, height);
            mFormat = format;
            copyBuffer(data.asReadOnlyBuffer(), mBuffer.duplicate(), rect, width * BYTES_FOR_16BPP);
        }

        synchronized void setBitmap(Bitmap bitmap, int width, int height, int format) throws IllegalArgumentException {
            Bitmap tmp;
            mSize = new IntSize(width, height);
            mFormat = format;
            if (width == bitmap.getWidth() && height == bitmap.getHeight()) {
                tmp = bitmap;
                tmp.copyPixelsToBuffer(mBuffer.asIntBuffer());
            } else {
                throw new IllegalArgumentException("### unexpected size in setBitmap: w="+width+" h="+height);
            }
        }

        @Override
        synchronized public ByteBuffer getBuffer() { return mBuffer; }
        @Override
        synchronized public IntSize getSize() { return mSize; }
        @Override
        synchronized public int getFormat() { return mFormat; }
    }
}
