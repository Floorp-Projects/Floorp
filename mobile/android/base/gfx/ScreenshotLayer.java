/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.gfx.BufferedCairoImage;
import org.mozilla.gecko.gfx.CairoImage;
import org.mozilla.gecko.gfx.FloatSize;
import org.mozilla.gecko.gfx.IntSize;
import org.mozilla.gecko.gfx.SingleTileLayer;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Region;
import android.graphics.RegionIterator;
import android.opengl.GLES20;
import android.util.Log;
import java.nio.ByteBuffer;
import java.nio.IntBuffer;
import java.nio.FloatBuffer;

public class ScreenshotLayer extends SingleTileLayer {
    private static final int SCREENSHOT_SIZE_LIMIT = 1048576;
    private ScreenshotImage mImage;
    // Size of the image buffer
    private IntSize mBufferSize;
    // The size of the bitmap painted in the buffer
    // (may be smaller than mBufferSize due to power of 2 padding)
    private IntSize mImageSize;
    // Whether we have an up-to-date image to draw
    private boolean mHasImage;

    public static int getMaxNumPixels() {
        return SCREENSHOT_SIZE_LIMIT;
    }

    public void reset() {
        mHasImage = false;
    }

    void setBitmap(Bitmap bitmap) {
        mImageSize = new IntSize(bitmap.getWidth(), bitmap.getHeight());
        int width = IntSize.nextPowerOfTwo(bitmap.getWidth());
        int height = IntSize.nextPowerOfTwo(bitmap.getHeight());
        mBufferSize = new IntSize(width, height);
        mImage.setBitmap(bitmap, width, height, CairoImage.FORMAT_RGB16_565);
        mHasImage = true;
    }

    public void updateBitmap(Bitmap bitmap, float x, float y, float width, float height) {
        mImage.updateBitmap(bitmap, x, y, width, height);
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
        ByteBuffer buffer = GeckoAppShell.allocateDirectBuffer(SCREENSHOT_SIZE_LIMIT * 2);
        // construct the screenshot layer
        ScreenshotLayer sl =  new ScreenshotLayer(new ScreenshotImage(buffer, size.width, size.height, CairoImage.FORMAT_RGB16_565), size);
        // paint the passed in bitmap into the buffer
        sl.setBitmap(bitmap);
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
            mBuffer = inBuffer; mSize = new IntSize(inWidth, inHeight); mFormat = inFormat;
        }

        @Override
        protected void finalize() throws Throwable {
            try {
                if (mBuffer != null)
                    GeckoAppShell.freeDirectBuffer(mBuffer);
            } finally {
                super.finalize();
            }
        }

        void setBitmap(Bitmap bitmap, int width, int height, int format) {
            Bitmap tmp;
            mSize = new IntSize(width, height);
            mFormat = format;
            if (width == bitmap.getWidth() && height == bitmap.getHeight()) {
                tmp = bitmap;
            } else {
                tmp = Bitmap.createBitmap(width, height, CairoUtils.cairoFormatTobitmapConfig(mFormat));
                new Canvas(tmp).drawBitmap(bitmap, 0.0f, 0.0f, new Paint());
            }
            tmp.copyPixelsToBuffer(mBuffer.asIntBuffer());
        }

        public void updateBitmap(Bitmap bitmap, float x, float y, float width, float height) {
            Bitmap tmp = Bitmap.createBitmap(mSize.width, mSize.height, CairoUtils.cairoFormatTobitmapConfig(mFormat));
            tmp.copyPixelsFromBuffer(mBuffer.asIntBuffer());
            Canvas c = new Canvas(tmp);
            c.drawBitmap(bitmap, x, y, new Paint());
            tmp.copyPixelsToBuffer(mBuffer.asIntBuffer());
        }

        @Override
        public ByteBuffer getBuffer() { return mBuffer; }
        @Override
        public IntSize getSize() { return mSize; }
        @Override
        public int getFormat() { return mFormat; }
    }
}
