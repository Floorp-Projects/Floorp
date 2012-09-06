/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Region;
import android.graphics.RegionIterator;
import android.opengl.GLES20;

import java.nio.FloatBuffer;

/**
 * Encapsulates the logic needed to draw a single textured tile.
 *
 * TODO: Repeating textures really should be their own type of layer.
 */
public class SingleTileLayer extends TileLayer {
    private static final String LOGTAG = "GeckoSingleTileLayer";

    private Rect mMask;

    // To avoid excessive GC, declare some objects here that would otherwise
    // be created and destroyed frequently during draw().
    private final RectF mBounds;
    private final RectF mTextureBounds;
    private final RectF mViewport;
    private final Rect mIntBounds;
    private final Rect mSubRect;
    private final RectF mSubRectF;
    private final Region mMaskedBounds;
    private final Rect mCropRect;
    private final RectF mObjRectF;
    private final float[] mCoords;

    public SingleTileLayer(CairoImage image) {
        this(false, image);
    }

    public SingleTileLayer(boolean repeat, CairoImage image) {
        this(image, repeat ? TileLayer.PaintMode.REPEAT : TileLayer.PaintMode.NORMAL);
    }

    public SingleTileLayer(CairoImage image, TileLayer.PaintMode paintMode) {
        super(image, paintMode);

        mBounds = new RectF();
        mTextureBounds = new RectF();
        mViewport = new RectF();
        mIntBounds = new Rect();
        mSubRect = new Rect();
        mSubRectF = new RectF();
        mMaskedBounds = new Region();
        mCropRect = new Rect();
        mObjRectF = new RectF();
        mCoords = new float[20];
    }

    /**
     * Set an area to mask out when rendering.
     */
    public void setMask(Rect aMaskRect) {
        mMask = aMaskRect;
    }

    @Override
    public void draw(RenderContext context) {
        // mTextureIDs may be null here during startup if Layer.java's draw method
        // failed to acquire the transaction lock and call performUpdates.
        if (!initialized())
            return;

        mViewport.set(context.viewport);

        if (repeats()) {
            // If we're repeating, we want to adjust the texture bounds so that
            // the texture repeats the correct number of times when drawn at
            // the size of the viewport.
            mBounds.set(getBounds(context));
            mTextureBounds.set(0.0f, 0.0f, mBounds.width(), mBounds.height());
            mBounds.set(0.0f, 0.0f, mViewport.width(), mViewport.height());
        } else if (stretches()) {
            // If we're stretching, we just want the bounds and texture bounds
            // to fit to the page.
            mBounds.set(context.pageRect);
            mTextureBounds.set(mBounds);
        } else {
            mBounds.set(getBounds(context));
            mTextureBounds.set(mBounds);
        }

        mBounds.roundOut(mIntBounds);
        mMaskedBounds.set(mIntBounds);
        if (mMask != null) {
            mMaskedBounds.op(mMask, Region.Op.DIFFERENCE);
            if (mMaskedBounds.isEmpty())
                return;
        }

        // XXX Possible optimisation here, form this array so we can draw it in
        //     a single call.
        RegionIterator i = new RegionIterator(mMaskedBounds);
        while (i.next(mSubRect)) {
            // Compensate for rounding errors at the edge of the tile caused by
            // the roundOut above
            mSubRectF.set(Math.max(mBounds.left, (float)mSubRect.left),
                          Math.max(mBounds.top, (float)mSubRect.top),
                          Math.min(mBounds.right, (float)mSubRect.right),
                          Math.min(mBounds.bottom, (float)mSubRect.bottom));

            // This is the left/top/right/bottom of the rect, relative to the
            // bottom-left of the layer, to use for texture coordinates.
            mCropRect.set(Math.round(mSubRectF.left - mBounds.left),
                          Math.round(mBounds.bottom - mSubRectF.top),
                          Math.round(mSubRectF.right - mBounds.left),
                          Math.round(mBounds.bottom - mSubRectF.bottom));

            mObjRectF.set(mSubRectF.left - mViewport.left,
                          mViewport.bottom - mSubRectF.bottom,
                          mSubRectF.right - mViewport.left,
                          mViewport.bottom - mSubRectF.top);

            fillRectCoordBuffer(mCoords, mObjRectF, mViewport.width(), mViewport.height(),
                                mCropRect, mTextureBounds.width(), mTextureBounds.height());

            FloatBuffer coordBuffer = context.coordBuffer;
            int positionHandle = context.positionHandle;
            int textureHandle = context.textureHandle;

            GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, getTextureID());

            // Make sure we are at position zero in the buffer
            coordBuffer.position(0);
            coordBuffer.put(mCoords);

            // Unbind any the current array buffer so we can use client side buffers
            GLES20.glBindBuffer(GLES20.GL_ARRAY_BUFFER, 0);

            // Vertex coordinates are x,y,z starting at position 0 into the buffer.
            coordBuffer.position(0);
            GLES20.glVertexAttribPointer(positionHandle, 3, GLES20.GL_FLOAT, false, 20, coordBuffer);

            // Texture coordinates are texture_x, texture_y starting at position 3 into the buffer.
            coordBuffer.position(3);
            GLES20.glVertexAttribPointer(textureHandle, 2, GLES20.GL_FLOAT, false, 20, coordBuffer);
            GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
        }
    }
}
