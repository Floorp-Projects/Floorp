/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.graphics.Rect;
import android.opengl.GLES20;
import android.util.Log;

import java.nio.ByteBuffer;

/**
 * Base class for tile layers, which encapsulate the logic needed to draw textured tiles in OpenGL
 * ES.
 */
public abstract class TileLayer extends Layer {
    private static final String LOGTAG = "GeckoTileLayer";

    private final Rect mDirtyRect;
    private IntSize mSize;
    private int[] mTextureIDs;

    protected final CairoImage mImage;

    public enum PaintMode { NORMAL, REPEAT, STRETCH };
    private PaintMode mPaintMode;

    public TileLayer(CairoImage image, PaintMode paintMode) {
        super(image.getSize());

        mPaintMode = paintMode;
        mImage = image;
        mSize = new IntSize(0, 0);
        mDirtyRect = new Rect();
    }

    protected boolean repeats() { return mPaintMode == PaintMode.REPEAT; }
    protected boolean stretches() { return mPaintMode == PaintMode.STRETCH; }
    protected int getTextureID() { return mTextureIDs[0]; }
    protected boolean initialized() { return mImage != null && mTextureIDs != null; }

    @Override
    protected void finalize() throws Throwable {
        try {
            if (mTextureIDs != null)
                TextureReaper.get().add(mTextureIDs);
        } finally {
            super.finalize();
        }
    }

    public void destroy() {
        try {
            if (mImage != null) {
                mImage.destroy();
            }
        } catch (Exception ex) {
            Log.e(LOGTAG, "error clearing buffers: ", ex);
        }
    }

    public void setPaintMode(PaintMode mode) {
        mPaintMode = mode;
    }

    /**
     * Invalidates the entire buffer so that it will be uploaded again. Only valid inside a
     * transaction.
     */

    public void invalidate() {
        if (!inTransaction())
            throw new RuntimeException("invalidate() is only valid inside a transaction");
        IntSize bufferSize = mImage.getSize();
        mDirtyRect.set(0, 0, bufferSize.width, bufferSize.height);
    }

    private void validateTexture() {
        /* Calculate the ideal texture size. This must be a power of two if
         * the texture is repeated or OpenGL ES 2.0 isn't supported, as
         * OpenGL ES 2.0 is required for NPOT texture support (without
         * extensions), but doesn't support repeating NPOT textures.
         *
         * XXX Currently, we don't pick a GLES 2.0 context, so always round.
         */
        IntSize textureSize = mImage.getSize().nextPowerOfTwo();

        if (!textureSize.equals(mSize)) {
            mSize = textureSize;

            // Delete the old texture
            if (mTextureIDs != null) {
                TextureReaper.get().add(mTextureIDs);
                mTextureIDs = null;

                // Free the texture immediately, so we don't incur a
                // temporarily increased memory usage.
                TextureReaper.get().reap();
            }
        }
    }

    @Override
    protected void performUpdates(RenderContext context) {
        super.performUpdates(context);

        // Reallocate the texture if the size has changed
        validateTexture();

        // Don't do any work if the image has an invalid size.
        if (!mImage.getSize().isPositive())
            return;

        // If we haven't allocated a texture, assume the whole region is dirty
        if (mTextureIDs == null) {
            uploadFullTexture();
        } else {
            uploadDirtyRect(mDirtyRect);
        }

        mDirtyRect.setEmpty();
    }

    private void uploadFullTexture() {
        IntSize bufferSize = mImage.getSize();
        uploadDirtyRect(new Rect(0, 0, bufferSize.width, bufferSize.height));
    }

    private void uploadDirtyRect(Rect dirtyRect) {
        // If we have nothing to upload, just return for now
        if (dirtyRect.isEmpty())
            return;

        // It's possible that the buffer will be null, check for that and return
        ByteBuffer imageBuffer = mImage.getBuffer();
        if (imageBuffer == null)
            return;

        if (mTextureIDs == null) {
            mTextureIDs = new int[1];
            GLES20.glGenTextures(mTextureIDs.length, mTextureIDs, 0);
        }

        int cairoFormat = mImage.getFormat();
        CairoGLInfo glInfo = new CairoGLInfo(cairoFormat);

        bindAndSetGLParameters();

        // XXX TexSubImage2D is too broken to rely on on Adreno, and very slow
        //     on other chipsets, so we always upload the entire buffer.
        IntSize bufferSize = mImage.getSize();
        if (mSize.equals(bufferSize)) {
            GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, glInfo.internalFormat, mSize.width,
                                mSize.height, 0, glInfo.format, glInfo.type, imageBuffer);
        } else {
            // Our texture has been expanded to the next power of two.
            // XXX We probably never want to take this path, so throw an exception.
            throw new RuntimeException("Buffer/image size mismatch in TileLayer!");
        }
    }

    private void bindAndSetGLParameters() {
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, mTextureIDs[0]);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER,
                               GLES20.GL_LINEAR);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER,
                               GLES20.GL_LINEAR);

        int repeatMode = repeats() ? GLES20.GL_REPEAT : GLES20.GL_CLAMP_TO_EDGE;
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S, repeatMode);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T, repeatMode);
    }
}

