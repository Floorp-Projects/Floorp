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

import android.graphics.Rect;
import android.graphics.RectF;
import android.opengl.GLES20;
import android.util.Log;
import javax.microedition.khronos.opengles.GL10;
import javax.microedition.khronos.opengles.GL11Ext;
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
    private final CairoImage mImage;
    private final boolean mRepeat;
    private IntSize mSize;
    private Rect mValidTextureRect;
    private int[] mTextureIDs;

    public TileLayer(boolean repeat, CairoImage image) {
        mRepeat = repeat;
        mImage = image;
        mSize = new IntSize(0, 0);
        mValidTextureRect = new Rect();

        IntSize bufferSize = mImage.getSize();
        mDirtyRect = new Rect();
    }

    @Override
    public IntSize getSize() { return mImage.getSize(); }

    protected boolean repeats() { return mRepeat; }
    protected int getTextureID() { return mTextureIDs[0]; }
    protected boolean initialized() { return mImage != null && mTextureIDs != null; }

    @Override
    protected void finalize() throws Throwable {
        if (mTextureIDs != null)
            TextureReaper.get().add(mTextureIDs);
    }

    /**
     * Invalidates the given rect so that it will be uploaded again. Only valid inside a
     * transaction.
     */
    public void invalidate(Rect rect) {
        if (!inTransaction())
            throw new RuntimeException("invalidate() is only valid inside a transaction");
        mDirtyRect.union(rect);
    }

    public void invalidate() {
        IntSize bufferSize = mImage.getSize();
        invalidate(new Rect(0, 0, bufferSize.width, bufferSize.height));
    }

    public boolean isDirty() {
        return mImage.getSize().isPositive() && (mTextureIDs == null || !mDirtyRect.isEmpty());
    }

    private void validateTexture(GL10 gl) {
        /* Calculate the ideal texture size. This must be a power of two if
         * the texture is repeated or OpenGL ES 2.0 isn't supported, as
         * OpenGL ES 2.0 is required for NPOT texture support (without
         * extensions), but doesn't support repeating NPOT textures.
         *
         * XXX Currently, we don't pick a GLES 2.0 context, so always round.
         */
        IntSize bufferSize = mImage.getSize();
        IntSize textureSize = bufferSize;

        textureSize = bufferSize.nextPowerOfTwo();

        if (!textureSize.equals(mSize)) {
            mSize = textureSize;

            // Delete the old texture
            if (mTextureIDs != null) {
                TextureReaper.get().add(mTextureIDs);
                mTextureIDs = null;

                // Free the texture immediately, so we don't incur a
                // temporarily increased memory usage.
                TextureReaper.get().reap(gl);
            }
        }
    }

    /**
     * Tells the tile that its texture contents are invalid. This will also
     * clear any invalidated area.
     */
    public void invalidateTexture() {
        mValidTextureRect.setEmpty();
        mDirtyRect.setEmpty();
    }

    /**
     * Returns a handle to the valid texture area rectangle. Modifying this
     * Rect will modify the valid texture area for this layer.
     */
    public Rect getValidTextureArea() {
        return mValidTextureRect;
    }

    @Override
    protected boolean performUpdates(GL10 gl, RenderContext context) {
        super.performUpdates(gl, context);

        // Reallocate the texture if the size has changed
        validateTexture(gl);

        // Don't do any work if the image has an invalid size.
        if (!mImage.getSize().isPositive())
            return true;

        // Update the dirty region
        uploadDirtyRect(gl, mDirtyRect);
        mDirtyRect.setEmpty();

        return true;
    }

    private void uploadDirtyRect(GL10 gl, Rect dirtyRect) {
        IntSize bufferSize = mImage.getSize();
        Rect bufferRect = new Rect(0, 0, bufferSize.width, bufferSize.height);

        // Make sure the dirty region intersects with the buffer
        dirtyRect.intersect(bufferRect);

        // If we have nothing to upload, just return for now
        if (dirtyRect.isEmpty())
            return;

        // It's possible that the buffer will be null, check for that and return
        ByteBuffer imageBuffer = mImage.getBuffer();
        if (imageBuffer == null)
            return;

        // Mark the dirty region as valid. Note, we assume that the valid area
        // can be enclosed by a rectangle (ideally we'd use a Region, but it'd
        // be slower and it probably isn't necessary).
        mValidTextureRect.union(dirtyRect);

        boolean newlyCreated = false;

        if (mTextureIDs == null) {
            mTextureIDs = new int[1];
            gl.glGenTextures(mTextureIDs.length, mTextureIDs, 0);
            newlyCreated = true;
        }

        int cairoFormat = mImage.getFormat();
        CairoGLInfo glInfo = new CairoGLInfo(cairoFormat);

        bindAndSetGLParameters(gl);

        if (newlyCreated || dirtyRect.equals(bufferRect)) {
            if (mSize.equals(bufferSize)) {
                gl.glTexImage2D(gl.GL_TEXTURE_2D, 0, glInfo.internalFormat, mSize.width, mSize.height,
                                0, glInfo.format, glInfo.type, imageBuffer);
                return;
            } else {
                gl.glTexImage2D(gl.GL_TEXTURE_2D, 0, glInfo.internalFormat, mSize.width, mSize.height,
                                0, glInfo.format, glInfo.type, null);
                gl.glTexSubImage2D(gl.GL_TEXTURE_2D, 0, 0, 0, bufferSize.width, bufferSize.height,
                                   glInfo.format, glInfo.type, imageBuffer);
                return;
            }
        }

        /*
         * Upload the changed rect. We have to widen to the full width of the texture
         * because we can't count on the device having support for GL_EXT_unpack_subimage,
         * and going line-by-line is too slow.
         *
         * XXX We should still use GL_EXT_unpack_subimage when available.
         */
        Buffer viewBuffer = imageBuffer.slice();
        int bpp = CairoUtils.bitsPerPixelForCairoFormat(cairoFormat) / 8;
        int position = dirtyRect.top * bufferSize.width * bpp;
        if (position > viewBuffer.limit()) {
            Log.e(LOGTAG, "### Position outside tile! " + dirtyRect.top);
            return;
        }

        viewBuffer.position(position);
        gl.glTexSubImage2D(gl.GL_TEXTURE_2D, 0, 0, dirtyRect.top,
                           bufferSize.width, dirtyRect.height(),
                           glInfo.format, glInfo.type, viewBuffer);
    }

    private void bindAndSetGLParameters(GL10 gl) {
        gl.glBindTexture(GL10.GL_TEXTURE_2D, mTextureIDs[0]);
        gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_MIN_FILTER, GL10.GL_NEAREST);
        gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_MAG_FILTER, GL10.GL_LINEAR);

        int repeatMode = mRepeat ? GL10.GL_REPEAT : GL10.GL_CLAMP_TO_EDGE;
        gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_WRAP_S, repeatMode);
        gl.glTexParameterf(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_WRAP_T, repeatMode);
    }
}

