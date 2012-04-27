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
    // Special case to show the page background color prior to painting a screenshot
    private boolean mIsSingleColor = true;
    // Force single color, needed for testing
    private boolean mForceSingleColor = false;
    // Cache the passed in background color to determine if we need to update
    // initialized to 0 so it lets the code run to set it to white on init
    private int mCurrentBackgroundColor = 0;

    public static int getMaxNumPixels() {
        return SCREENSHOT_SIZE_LIMIT;
    }

    public void reset() {
        mIsSingleColor = true;
        updateBackground(mForceSingleColor, Color.WHITE);
        setPaintMode(TileLayer.PaintMode.STRETCH);
    }

    void setBitmap(Bitmap bitmap) {
        if (mForceSingleColor)
            return;
        mImageSize = new IntSize(bitmap.getWidth(), bitmap.getHeight());
        int width = IntSize.nextPowerOfTwo(bitmap.getWidth());
        int height = IntSize.nextPowerOfTwo(bitmap.getHeight());
        mBufferSize = new IntSize(width, height);
        mImage.setBitmap(bitmap, width, height, CairoImage.FORMAT_RGB16_565);
        mIsSingleColor = false;
        setPaintMode(TileLayer.PaintMode.NORMAL);
    }

    public void updateBitmap(Bitmap bitmap, float x, float y, float width, float height) {
        mImage.updateBitmap(bitmap, x, y, width, height);
    }

    public static ScreenshotLayer create() {
        // 3 x 3 min for the single color case. Less than 3x3 will blend 
        // the colors from outside this single color block when scaled
        return ScreenshotLayer.create(new IntSize(3, 3));
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
        super(image, TileLayer.PaintMode.STRETCH);
        mBufferSize = size;
        mImage = image;
    }

    public boolean updateBackground(boolean showChecks, int color) {
        if (!showChecks) {
            mIsSingleColor = true;
            mForceSingleColor = true;
        } else {
            mForceSingleColor = false;
        }

        if (!mIsSingleColor || color == mCurrentBackgroundColor)
            return false;

        mCurrentBackgroundColor = color;

        /* mask each component of the 8888 color and bit shift to least 
         * sigificant. Then for red and blue multiply by (2^5 -1) and (2^6 - 1)
         * for green. Finally, divide by (2^8 - 1) for all color values. This
         * scales the 8 bit color values to 5 or 6 bits
         */
        int red =   ((color & 0x00FF0000 >> 16)* 31 / 255);
        int green = ((color & 0x0000FF00 >> 8) * 63 / 255);
        int blue =   (color & 0x000000FF) * 31 / 255;
        /* For the first byte left shift red by 3 positions such that it is the
         * top 5 bits, right shift green by 3 so its 3 most significant are the
         * 3 least significant. For the second byte, left shift green by 3 so 
         * its 3 least significant bits are the 3 most significant bits of the
         * byte. Finally, set the 5 least significant bits to blue's value. 
         */
        byte byte1 = (byte)((red << 3 | green >> 3) & 0x0000FFFF);
        byte byte2 = (byte)((green << 5 | blue) & 0x0000FFFF);
        mImage.mBuffer.put(1, byte1);
        mImage.mBuffer.put(0, byte2);
        mImage.mBuffer.put(3, byte1);
        mImage.mBuffer.put(2, byte2);
        mImage.mBuffer.put(5, byte1);
        mImage.mBuffer.put(4, byte2);
        mImage.mBuffer.put(mImageSize.width + 1, byte1);
        mImage.mBuffer.put(mImageSize.width + 0, byte2);
        mImage.mBuffer.put(mImageSize.width + 3, byte1);
        mImage.mBuffer.put(mImageSize.width + 2, byte2);
        mImage.mBuffer.put(mImageSize.width + 5, byte1);
        mImage.mBuffer.put(mImageSize.width + 4, byte2);
        return true;
    }

    /** A Cairo image that simply saves a buffer of pixel data. */
    static class ScreenshotImage extends CairoImage {
        ByteBuffer mBuffer;
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
