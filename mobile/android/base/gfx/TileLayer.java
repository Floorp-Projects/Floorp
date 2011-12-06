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
import android.util.Log;
import javax.microedition.khronos.opengles.GL10;
import javax.microedition.khronos.opengles.GL11Ext;
import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.util.ArrayList;

/**
 * Base class for tile layers, which encapsulate the logic needed to draw textured tiles in OpenGL
 * ES.
 */
public abstract class TileLayer extends Layer {
    private static final String LOGTAG = "GeckoTileLayer";

    private final ArrayList<Rect> mDirtyRects;
    private final CairoImage mImage;
    private final boolean mRepeat;
    private final IntSize mSize;
    private int[] mTextureIDs;

    public TileLayer(boolean repeat, CairoImage image) {
        mRepeat = repeat;
        mImage = image;
        mSize = new IntSize(image.getWidth(), image.getHeight());
        mDirtyRects = new ArrayList<Rect>();

        /*
         * Assert that the image has a power-of-two size. OpenGL ES < 2.0 doesn't support NPOT
         * textures and OpenGL ES doesn't seem to let us efficiently slice up a NPOT bitmap.
         */
        int width = mImage.getWidth(), height = mImage.getHeight();
        if ((width & (width - 1)) != 0 || (height & (height - 1)) != 0) {
            throw new RuntimeException("TileLayer: NPOT images are unsupported (dimensions are " +
                                       width + "x" + height + ")");
        }
    }

    public IntSize getSize() { return mSize; }

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
        mDirtyRects.add(rect);
    }

    public void invalidate() {
        invalidate(new Rect(0, 0, mSize.width, mSize.height));
    }

    @Override
    protected void performUpdates(GL10 gl) {
        super.performUpdates(gl);

        if (mTextureIDs == null) {
            uploadFullTexture(gl);
        } else {
            for (Rect dirtyRect : mDirtyRects)
                uploadDirtyRect(gl, dirtyRect);
        }

        mDirtyRects.clear();
    }

    private void uploadFullTexture(GL10 gl) {
        mTextureIDs = new int[1];
        gl.glGenTextures(mTextureIDs.length, mTextureIDs, 0);

        int cairoFormat = mImage.getFormat();
        CairoGLInfo glInfo = new CairoGLInfo(cairoFormat);

        bindAndSetGLParameters(gl);

        gl.glTexImage2D(gl.GL_TEXTURE_2D, 0, glInfo.internalFormat, mSize.width, mSize.height,
                        0, glInfo.format, glInfo.type, mImage.getBuffer());
    }

    private void uploadDirtyRect(GL10 gl, Rect dirtyRect) {
        if (mTextureIDs == null)
            throw new RuntimeException("uploadDirtyRect() called with null texture ID!");

        int width = mSize.width;
        int cairoFormat = mImage.getFormat();
        CairoGLInfo glInfo = new CairoGLInfo(cairoFormat);

        bindAndSetGLParameters(gl);

        /*
         * Upload the changed rect. We have to widen to the full width of the texture
         * because we can't count on the device having support for GL_EXT_unpack_subimage,
         * and going line-by-line is too slow.
         */
        Buffer viewBuffer = mImage.getBuffer().slice();
        int bpp = CairoUtils.bitsPerPixelForCairoFormat(cairoFormat) / 8;
        int position = dirtyRect.top * width * bpp;
        if (position > viewBuffer.limit()) {
            Log.e(LOGTAG, "### Position outside tile! " + dirtyRect.top);
            return;
        }

        viewBuffer.position(position);
        gl.glTexSubImage2D(gl.GL_TEXTURE_2D, 0, 0, dirtyRect.top, width, dirtyRect.height(),
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

    protected static FloatBuffer createBuffer(float[] values) {
        ByteBuffer byteBuffer = ByteBuffer.allocateDirect(values.length * 4);
        byteBuffer.order(ByteOrder.nativeOrder());

        FloatBuffer floatBuffer = byteBuffer.asFloatBuffer();
        floatBuffer.put(values);
        floatBuffer.position(0);
        return floatBuffer;
    }
}

