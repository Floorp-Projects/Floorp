/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.util.FloatUtils;

import android.graphics.Rect;
import android.graphics.RectF;

import java.nio.FloatBuffer;
import java.util.concurrent.locks.ReentrantLock;

public abstract class Layer {
    private final ReentrantLock mTransactionLock;
    private boolean mInTransaction;
    private Rect mNewPosition;
    private float mNewResolution;

    protected Rect mPosition;
    protected float mResolution;

    public Layer() {
        this(null);
    }

    public Layer(IntSize size) {
        mTransactionLock = new ReentrantLock();
        if (size == null) {
            mPosition = new Rect();
        } else {
            mPosition = new Rect(0, 0, size.width, size.height);
        }
        mResolution = 1.0f;
    }

    /**
     * Updates the layer. This returns false if there is still work to be done
     * after this update.
     */
    public final boolean update(RenderContext context) {
        if (mTransactionLock.isHeldByCurrentThread()) {
            throw new RuntimeException("draw() called while transaction lock held by this " +
                                       "thread?!");
        }

        if (mTransactionLock.tryLock()) {
            try {
                performUpdates(context);
                return true;
            } finally {
                mTransactionLock.unlock();
            }
        }

        return false;
    }

    /** Subclasses override this function to draw the layer. */
    public abstract void draw(RenderContext context);

    /** Given the intrinsic size of the layer, returns the pixel boundaries of the layer rect. */
    protected RectF getBounds(RenderContext context) {
        return RectUtils.scale(new RectF(mPosition), context.zoomFactor / mResolution);
    }

    /**
     * Call this before modifying the layer. Note that, for TileLayers, "modifying the layer"
     * includes altering the underlying CairoImage in any way. Thus you must call this function
     * before modifying the byte buffer associated with this layer.
     *
     * This function may block, so you should never call this on the main UI thread.
     */
    public void beginTransaction() {
        if (mTransactionLock.isHeldByCurrentThread())
            throw new RuntimeException("Nested transactions are not supported");
        mTransactionLock.lock();
        mInTransaction = true;
        mNewResolution = mResolution;
    }

    /** Call this when you're done modifying the layer. */
    public void endTransaction() {
        if (!mInTransaction)
            throw new RuntimeException("endTransaction() called outside a transaction");
        mInTransaction = false;
        mTransactionLock.unlock();
    }

    /** Returns true if the layer is currently in a transaction and false otherwise. */
    protected boolean inTransaction() {
        return mInTransaction;
    }

    /** Returns the current layer position. */
    public Rect getPosition() {
        return mPosition;
    }

    /** Sets the position. Only valid inside a transaction. */
    public void setPosition(Rect newPosition) {
        if (!mInTransaction)
            throw new RuntimeException("setPosition() is only valid inside a transaction");
        mNewPosition = newPosition;
    }

    /** Returns the current layer's resolution. */
    public float getResolution() {
        return mResolution;
    }

    /**
     * Sets the layer resolution. This value is used to determine how many pixels per
     * device pixel this layer was rendered at. This will be reflected by scaling by
     * the reciprocal of the resolution in the layer's transform() function.
     * Only valid inside a transaction. */
    public void setResolution(float newResolution) {
        if (!mInTransaction)
            throw new RuntimeException("setResolution() is only valid inside a transaction");
        mNewResolution = newResolution;
    }

    /**
     * Subclasses may override this method to perform custom layer updates. This will be called
     * with the transaction lock held. Subclass implementations of this method must call the
     * superclass implementation. Returns false if there is still work to be done after this
     * update is complete.
     */
    protected void performUpdates(RenderContext context) {
        if (mNewPosition != null) {
            mPosition = mNewPosition;
            mNewPosition = null;
        }
        if (mNewResolution != 0.0f) {
            mResolution = mNewResolution;
            mNewResolution = 0.0f;
        }
    }

    /**
     * This function fills in the provided <tt>dest</tt> array with values to render a texture.
     * The array is filled with 4 sets of {x, y, z, texture_x, texture_y} values (so 20 values
     * in total) corresponding to the corners of the rect.
     */
    protected final void fillRectCoordBuffer(float[] dest, RectF rect, float viewWidth, float viewHeight,
                                             Rect cropRect, float texWidth, float texHeight) {
        //x, y, z, texture_x, texture_y
        dest[0] = rect.left / viewWidth;
        dest[1] = rect.bottom / viewHeight;
        dest[2] = 0;
        dest[3] = cropRect.left / texWidth;
        dest[4] = cropRect.top / texHeight;

        dest[5] = rect.left / viewWidth;
        dest[6] = rect.top / viewHeight;
        dest[7] = 0;
        dest[8] = cropRect.left / texWidth;
        dest[9] = cropRect.bottom / texHeight;

        dest[10] = rect.right / viewWidth;
        dest[11] = rect.bottom / viewHeight;
        dest[12] = 0;
        dest[13] = cropRect.right / texWidth;
        dest[14] = cropRect.top / texHeight;

        dest[15] = rect.right / viewWidth;
        dest[16] = rect.top / viewHeight;
        dest[17] = 0;
        dest[18] = cropRect.right / texWidth;
        dest[19] = cropRect.bottom / texHeight;
    }

    public static class RenderContext {
        public final RectF viewport;
        public final RectF pageRect;
        public final float zoomFactor;
        public final int positionHandle;
        public final int textureHandle;
        public final FloatBuffer coordBuffer;

        public RenderContext(RectF aViewport, RectF aPageRect, float aZoomFactor,
                             int aPositionHandle, int aTextureHandle, FloatBuffer aCoordBuffer) {
            viewport = aViewport;
            pageRect = aPageRect;
            zoomFactor = aZoomFactor;
            positionHandle = aPositionHandle;
            textureHandle = aTextureHandle;
            coordBuffer = aCoordBuffer;
        }

        public boolean fuzzyEquals(RenderContext other) {
            if (other == null) {
                return false;
            }
            return RectUtils.fuzzyEquals(viewport, other.viewport)
                && RectUtils.fuzzyEquals(pageRect, other.pageRect)
                && FloatUtils.fuzzyEquals(zoomFactor, other.zoomFactor);
        }
    }
}

