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
 *   James Willcox <jwillcox@mozilla.com>
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

import org.mozilla.gecko.GeckoApp;

import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.SurfaceTexture;
import android.opengl.GLES11;
import android.opengl.GLES11Ext;
import android.opengl.Matrix;
import android.util.Log;
import android.view.Surface;
import javax.microedition.khronos.opengles.GL10;
import javax.microedition.khronos.opengles.GL11Ext;
import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import android.hardware.Camera;

// TODO: Port this to GLES 2.0.
public class SurfaceTextureLayer extends Layer implements SurfaceTexture.OnFrameAvailableListener {
    private static final String LOGTAG = "SurfaceTextureLayer";
    private static final int LOCAL_GL_TEXTURE_EXTERNAL_OES = 0x00008d65; // This is only defined in API level 15 for some reason (Android 4.0.3)

    private final SurfaceTexture mSurfaceTexture;
    private final Surface mSurface;
    private int mTextureId;
    private boolean mHaveFrame;

    private boolean mInverted;
    private boolean mNewInverted;
    private boolean mBlend;
    private boolean mNewBlend;

    private FloatBuffer textureBuffer;
    private FloatBuffer textureBufferInverted;

    public SurfaceTextureLayer(int textureId) {
        mTextureId = textureId;
        mHaveFrame = true;
        mInverted = false;

        mSurfaceTexture = new SurfaceTexture(mTextureId);
        mSurfaceTexture.setOnFrameAvailableListener(this);

        Surface tmp = null;
        try {
            tmp = Surface.class.getConstructor(SurfaceTexture.class).newInstance(mSurfaceTexture); }
        catch (Exception ie) {
            Log.e(LOGTAG, "error constructing the surface", ie);
        }

        mSurface = tmp;

        float textureMap[] = {
                0.0f, 1.0f, // top left
                0.0f, 0.0f, // bottom left
                1.0f, 1.0f, // top right
                1.0f, 0.0f, // bottom right
        };

        textureBuffer = createBuffer(textureMap);

        float textureMapInverted[] = {
                0.0f, 0.0f, // bottom left
                0.0f, 1.0f, // top left
                1.0f, 0.0f, // bottom right
                1.0f, 1.0f, // top right
        };

        textureBufferInverted = createBuffer(textureMapInverted);
    }

    public static SurfaceTextureLayer create() {
        int textureId = TextureGenerator.get().take();
        if (textureId == 0)
            return null;

        return new SurfaceTextureLayer(textureId);
    }

    // For SurfaceTexture.OnFrameAvailableListener
    public void onFrameAvailable(SurfaceTexture texture) {
        // FIXME: for some reason this doesn't get called
        mHaveFrame = true;
        GeckoApp.mAppContext.requestRender();
    }

    private FloatBuffer createBuffer(float[] input) {
        // a float has 4 bytes so we allocate for each coordinate 4 bytes
        ByteBuffer byteBuffer = ByteBuffer.allocateDirect(input.length * 4);
        byteBuffer.order(ByteOrder.nativeOrder());

        FloatBuffer floatBuffer = byteBuffer.asFloatBuffer();
        floatBuffer.put(input);
        floatBuffer.position(0);

        return floatBuffer;
    }

    public void update(Rect position, float resolution, boolean inverted, boolean blend) {
        beginTransaction(); // this is called on the Gecko thread

        setPosition(position);
        setResolution(resolution);

        mNewInverted = inverted;
        mNewBlend = blend;

        endTransaction();
    }

    @Override
    protected void finalize() throws Throwable {
        if (mSurfaceTexture != null) {
            try {
                SurfaceTexture.class.getDeclaredMethod("release").invoke(mSurfaceTexture);
            } catch (NoSuchMethodException nsme) {
                Log.e(LOGTAG, "error finding release method on mSurfaceTexture", nsme);
            } catch (IllegalAccessException iae) {
                Log.e(LOGTAG, "error invoking release method on mSurfaceTexture", iae);
            } catch (Exception e) {
                Log.e(LOGTAG, "some other exception while invoking release method on mSurfaceTexture", e);
            }
        }
        if (mTextureId > 0)
            TextureReaper.get().add(mTextureId);
    }

    @Override
    protected boolean performUpdates(RenderContext context) {
        super.performUpdates(context);

        mInverted = mNewInverted;
        mBlend = mNewBlend;

        GLES11.glEnable(LOCAL_GL_TEXTURE_EXTERNAL_OES);
        GLES11.glBindTexture(LOCAL_GL_TEXTURE_EXTERNAL_OES, mTextureId);
        mSurfaceTexture.updateTexImage();
        GLES11.glDisable(LOCAL_GL_TEXTURE_EXTERNAL_OES);

        // FIXME: we should return true and rely on onFrameAvailable, but
        // that isn't working for some reason
        return false;
    }

    private float mapToGLCoords(float input, float viewport, boolean flip) {
        if (flip) input = viewport - input;
        return ((input / viewport) * 2.0f) - 1.0f;
    }

    @Override
    public void draw(RenderContext context) {

        // Enable GL_TEXTURE_EXTERNAL_OES and bind our texture
        GLES11.glEnable(LOCAL_GL_TEXTURE_EXTERNAL_OES);
        GLES11.glBindTexture(LOCAL_GL_TEXTURE_EXTERNAL_OES, mTextureId);

        // Enable vertex and texture coordinate buffers
        GLES11.glEnableClientState(GL10.GL_VERTEX_ARRAY);
        GLES11.glEnableClientState(GL10.GL_TEXTURE_COORD_ARRAY);

        // Load whatever texture transform the SurfaceMatrix needs
        float[] matrix = new float[16];
        mSurfaceTexture.getTransformMatrix(matrix);
        GLES11.glMatrixMode(GLES11.GL_TEXTURE);
        GLES11.glLoadMatrixf(matrix, 0);

        // Figure out vertices to put the texture in the right spot on the screen
        RectF bounds = getBounds(context);
        RectF viewport = context.viewport;
        bounds.offset(-viewport.left, -viewport.top);

        float vertices[] = new float[8];

        // Bottom left
        vertices[0] = mapToGLCoords(bounds.left, viewport.width(), false);
        vertices[1] = mapToGLCoords(bounds.bottom, viewport.height(), true);

        // Top left
        vertices[2] = mapToGLCoords(bounds.left, viewport.width(), false);
        vertices[3] = mapToGLCoords(bounds.top, viewport.height(), true);

        // Bottom right
        vertices[4] = mapToGLCoords(bounds.right, viewport.width(), false);
        vertices[5] = mapToGLCoords(bounds.bottom, viewport.height(), true);

        // Top right
        vertices[6] = mapToGLCoords(bounds.right, viewport.width(), false);
        vertices[7] = mapToGLCoords(bounds.top, viewport.height(), true);

        // Set texture and vertex buffers
        GLES11.glVertexPointer(2, GL10.GL_FLOAT, 0, createBuffer(vertices));
        GLES11.glTexCoordPointer(2, GL10.GL_FLOAT, 0, mInverted ? textureBufferInverted : textureBuffer);

        if (mBlend) {
            GLES11.glEnable(GL10.GL_BLEND);
            GLES11.glBlendFunc(GL10.GL_ONE, GL10.GL_ONE_MINUS_SRC_ALPHA);
        }

        // Draw the vertices as triangle strip
        GLES11.glDrawArrays(GL10.GL_TRIANGLE_STRIP, 0, vertices.length / 2);

        // Clean up
        GLES11.glDisableClientState(GL10.GL_VERTEX_ARRAY);
        GLES11.glDisableClientState(GL10.GL_TEXTURE_COORD_ARRAY);
        GLES11.glDisable(LOCAL_GL_TEXTURE_EXTERNAL_OES);
        GLES11.glLoadIdentity();
    }

    public SurfaceTexture getSurfaceTexture() {
        return mSurfaceTexture;
    }

    public Surface getSurface() {
        return mSurface;
    }
}

