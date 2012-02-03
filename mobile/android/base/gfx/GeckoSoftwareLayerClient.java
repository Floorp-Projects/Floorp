/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick Walton <pcwalton@mozilla.com>
 *   Chris Lord <chrislord.net@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.gfx.CairoImage;
import org.mozilla.gecko.gfx.IntSize;
import org.mozilla.gecko.gfx.LayerClient;
import org.mozilla.gecko.gfx.LayerController;
import org.mozilla.gecko.gfx.LayerRenderer;
import org.mozilla.gecko.gfx.MultiTileLayer;
import org.mozilla.gecko.gfx.PointUtils;
import org.mozilla.gecko.gfx.WidgetTileLayer;
import org.mozilla.gecko.GeckoAppShell;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.RectF;
import android.util.Log;
import java.nio.ByteBuffer;

/**
 * Transfers a software-rendered Gecko to an ImageLayer so that it can be rendered by our
 * compositor.
 *
 * TODO: Throttle down Gecko's priority when we pan and zoom.
 */
public class GeckoSoftwareLayerClient extends GeckoLayerClient {
    private static final String LOGTAG = "GeckoSoftwareLayerClient";

    private int mFormat;
    private IntSize mViewportSize;
    private ByteBuffer mBuffer;

    /* The offset used to make sure tiles are snapped to the pixel grid */
    private Point mRenderOffset;

    private CairoImage mCairoImage;

    private static final IntSize TILE_SIZE = new IntSize(256, 256);

    // Whether or not the last paint we got used direct texturing
    private boolean mHasDirectTexture;

    public GeckoSoftwareLayerClient(Context context) {
        super(context);

        mFormat = CairoImage.FORMAT_RGB16_565;
        mRenderOffset = new Point(0, 0);

        mCairoImage = new CairoImage() {
            @Override
            public ByteBuffer getBuffer() { return mBuffer; }
            @Override
            public IntSize getSize() { return mBufferSize; }
            @Override
            public int getFormat() { return mFormat; }
        };
    }

    public int getWidth() {
        return mBufferSize.width;
    }

    public int getHeight() {
        return mBufferSize.height;
    }

    protected void finalize() throws Throwable {
        try {
            if (mBuffer != null)
                GeckoAppShell.freeDirectBuffer(mBuffer);
            mBuffer = null;
        } finally {
            super.finalize();
        }
    }

    @Override
    protected boolean handleDirectTextureChange(boolean hasDirectTexture) {
        if (mTileLayer != null && hasDirectTexture == mHasDirectTexture)
            return false;

        mHasDirectTexture = hasDirectTexture;

        if (mHasDirectTexture) {
            Log.i(LOGTAG, "Creating WidgetTileLayer");
            mTileLayer = new WidgetTileLayer(mCairoImage);
            mRenderOffset.set(0, 0);
        } else {
            Log.i(LOGTAG, "Creating MultiTileLayer");
            mTileLayer = new MultiTileLayer(mCairoImage, TILE_SIZE);
        }

        getLayerController().setRoot(mTileLayer);

        // Force a resize event to be sent because the results of this
        // are different depending on what tile system we're using
        sendResizeEventIfNecessary(true);

        return true;
    }

    @Override
    protected boolean shouldDrawProceed(int tileWidth, int tileHeight) {
        // Make sure the tile-size matches. If it doesn't, we could crash trying
        // to access invalid memory.
        if (mHasDirectTexture) {
            if (tileWidth != 0 || tileHeight != 0) {
                Log.e(LOGTAG, "Aborting draw, incorrect tile size of " + tileWidth + "x" +
                      tileHeight);
                return false;
            }
        } else {
            if (tileWidth != TILE_SIZE.width || tileHeight != TILE_SIZE.height) {
                Log.e(LOGTAG, "Aborting draw, incorrect tile size of " + tileWidth + "x" +
                      tileHeight);
                return false;
            }
        }

        return true;
    }

    @Override
    public boolean beginDrawing(int width, int height, int tileWidth, int tileHeight,
                                String metadata, boolean hasDirectTexture) {
        if (!super.beginDrawing(width, height, tileWidth, tileHeight, metadata,
                                hasDirectTexture)) {
            return false;
        }

        // We only need to set a render offset/allocate buffer memory if
        // we're using MultiTileLayer. Otherwise, just synchronise the
        // buffer size and return.
        if (!(mTileLayer instanceof MultiTileLayer)) {
            if (mBufferSize.width != width || mBufferSize.height != height)
                mBufferSize = new IntSize(width, height);
            return true;
        }

        // If the origin has changed, alter the rendering offset so that
        // rendering is snapped to the tile grid and clear the invalid area.
        boolean originChanged = true;
        Point origin = PointUtils.round(mNewGeckoViewport.getDisplayportOrigin());

        if (mGeckoViewport != null) {
            Point oldOrigin = PointUtils.round(mGeckoViewport.getDisplayportOrigin());
            originChanged = !origin.equals(oldOrigin);
        }

        if (originChanged) {
            Point tileOrigin = new Point((origin.x / TILE_SIZE.width) * TILE_SIZE.width,
                                         (origin.y / TILE_SIZE.height) * TILE_SIZE.height);
            mRenderOffset.set(origin.x - tileOrigin.x, origin.y - tileOrigin.y);
        }

        // If the window size has changed, reallocate the buffer to match.
        if (mBufferSize.width != width || mBufferSize.height != height) {
            mBufferSize = new IntSize(width, height);

            // We over-allocate to allow for the render offset. nsWindow
            // assumes that this will happen.
            IntSize realBufferSize = new IntSize(width + TILE_SIZE.width,
                                                 height + TILE_SIZE.height);

            // Reallocate the buffer if necessary
            int bpp = CairoUtils.bitsPerPixelForCairoFormat(mFormat) / 8;
            int size = realBufferSize.getArea() * bpp;
            if (mBuffer == null || mBuffer.capacity() != size) {
                // Free the old buffer
                if (mBuffer != null) {
                    GeckoAppShell.freeDirectBuffer(mBuffer);
                    mBuffer = null;
                }

                mBuffer = GeckoAppShell.allocateDirectBuffer(size);
            }
        }

        return true;
    }

    @Override
    protected void updateLayerAfterDraw(Rect updatedRect) {
        if (!(mTileLayer instanceof MultiTileLayer)) {
            return;
        }

        updatedRect.offset(mRenderOffset.x, mRenderOffset.y);
        ((MultiTileLayer)mTileLayer).invalidate(updatedRect);
        ((MultiTileLayer)mTileLayer).setRenderOffset(mRenderOffset);
    }

    public ViewportMetrics getGeckoViewportMetrics() {
        // Return a copy, as we modify this inside the Gecko thread
        if (mGeckoViewport != null)
            return new ViewportMetrics(mGeckoViewport);
        return null;
    }

    public void copyPixelsFromMultiTileLayer(Bitmap target) {
        Canvas c = new Canvas(target);
        ByteBuffer tileBuffer = mBuffer.slice();
        int bpp = CairoUtils.bitsPerPixelForCairoFormat(mFormat) / 8;

        for (int y = 0; y <= mBufferSize.height; y += TILE_SIZE.height) {
            for (int x = 0; x <= mBufferSize.width; x += TILE_SIZE.width) {
                // Create a Bitmap from this tile
                Bitmap tile = Bitmap.createBitmap(TILE_SIZE.width, TILE_SIZE.height,
                                                  CairoUtils.cairoFormatTobitmapConfig(mFormat));
                tile.copyPixelsFromBuffer(tileBuffer.asIntBuffer());

                // Copy the tile to the master Bitmap and recycle it
                c.drawBitmap(tile, x - mRenderOffset.x, y - mRenderOffset.y, null);
                tile.recycle();

                // Progress the buffer to the next tile
                tileBuffer.position(TILE_SIZE.getArea() * bpp);
                tileBuffer = tileBuffer.slice();
            }
        }
    }

    public Bitmap getBitmap() {
        if (mTileLayer == null)
            return null;

        // Begin a tile transaction, otherwise the buffer can be destroyed while
        // we're reading from it.
        beginTransaction(mTileLayer);
        try {
            if (mBuffer == null || mBufferSize.width <= 0 || mBufferSize.height <= 0)
                return null;
            try {
                Bitmap b = null;

                if (mTileLayer instanceof MultiTileLayer) {
                    b = Bitmap.createBitmap(mBufferSize.width, mBufferSize.height,
                                            CairoUtils.cairoFormatTobitmapConfig(mFormat));
                    copyPixelsFromMultiTileLayer(b);
                } else {
                    Log.w(LOGTAG, "getBitmap() called on a layer (" + mTileLayer + ") we don't know how to get a bitmap from");
                }

                return b;
            } catch (OutOfMemoryError oom) {
                Log.w(LOGTAG, "Unable to create bitmap", oom);
                return null;
            }
        } finally {
            endTransaction(mTileLayer);
        }
    }

    /** Returns the back buffer. This function is for Gecko to use. */
    public ByteBuffer lockBuffer() {
        return mBuffer;
    }

    public Point getRenderOffset() {
        return mRenderOffset;
    }

    /**
     * Gecko calls this function to signal that it is done with the back buffer. After this call,
     * it is forbidden for Gecko to touch the buffer.
     */
    public void unlockBuffer() {
        /* no-op */
    }

    @Override
    protected IntSize getBufferSize() {
        // Round up depending on layer implementation to remove texture wastage
        if (!mHasDirectTexture) {
            // Round to the next multiple of the tile size
            return new IntSize(((mScreenSize.width + LayerController.MIN_BUFFER.width - 1) /
                                    TILE_SIZE.width + 1) * TILE_SIZE.width,
                               ((mScreenSize.height + LayerController.MIN_BUFFER.height - 1) /
                                    TILE_SIZE.height + 1) * TILE_SIZE.height);
        }

        int maxSize = getLayerController().getView().getMaxTextureSize();

        // XXX Integrate gralloc/tiling work to circumvent this
        if (mScreenSize.width > maxSize || mScreenSize.height > maxSize) {
            throw new RuntimeException("Screen size of " + mScreenSize +
                                       " larger than maximum texture size of " + maxSize);
        }

        // Round to next power of two until we have NPOT texture support, respecting maximum
        // texture size
        return new IntSize(Math.min(maxSize, IntSize.nextPowerOfTwo(mScreenSize.width +
                                             LayerController.MIN_BUFFER.width)),
                           Math.min(maxSize, IntSize.nextPowerOfTwo(mScreenSize.height +
                                             LayerController.MIN_BUFFER.height)));
    }

    @Override
    protected IntSize getTileSize() {
        // Round up depending on layer implementation to remove texture wastage
        return !mHasDirectTexture ? TILE_SIZE : new IntSize(0, 0);
    }
}

