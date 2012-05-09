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
 *   Arkady Blyakher <rkadyb@mit.edu>
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

import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Region;
import android.opengl.GLES20;
import android.util.Log;
import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

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
        if (mTextureIDs != null)
            TextureReaper.get().add(mTextureIDs);
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

            /*
            int bpp = CairoUtils.bitsPerPixelForCairoFormat(cairoFormat)/8;
            ByteBuffer tempBuffer =
                GeckoAppShell.allocateDirectBuffer(mSize.width * mSize.height * bpp);
            for (int y = 0; y < bufferSize.height; y++) {
                tempBuffer.position(y * mSize.width * bpp);
                imageBuffer.limit((y + 1) * bufferSize.width * bpp);
                imageBuffer.position(y * bufferSize.width * bpp);
                tempBuffer.put(imageBuffer);
            }
            imageBuffer.position(0);
            tempBuffer.position(0);

            GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, glInfo.internalFormat, mSize.width,
                                mSize.height, 0, glInfo.format, glInfo.type, tempBuffer);
            GeckoAppShell.freeDirectBuffer(tempBuffer);
            */
        }
    }

    private void bindAndSetGLParameters() {
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, mTextureIDs[0]);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER,
                               GLES20.GL_NEAREST);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER,
                               GLES20.GL_LINEAR);

        int repeatMode = repeats() ? GLES20.GL_REPEAT : GLES20.GL_CLAMP_TO_EDGE;
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S, repeatMode);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T, repeatMode);
    }
}

