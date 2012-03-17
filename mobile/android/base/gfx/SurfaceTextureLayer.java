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
import android.opengl.GLES20;
import android.util.Log;
import android.view.Surface;
import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

public class SurfaceTextureLayer extends Layer implements SurfaceTexture.OnFrameAvailableListener {
    private static final String LOGTAG = "SurfaceTextureLayer";
    private static final int LOCAL_GL_TEXTURE_EXTERNAL_OES = 0x00008d65; // This is only defined in API level 15 for some reason (Android 4.0.3)

    private final SurfaceTexture mSurfaceTexture;
    private final Surface mSurface;
    private int mTextureId;
    private boolean mHaveFrame;
    private float[] mTextureTransform = new float[16];

    private boolean mInverted;
    private boolean mNewInverted;
    private boolean mBlend;
    private boolean mNewBlend;

    private static int mProgram;
    private static int mPositionHandle;
    private static int mTextureHandle;
    private static int mSampleHandle;
    private static int mProjectionMatrixHandle;
    private static int mTextureMatrixHandle;

    private static final float[] PROJECTION_MATRIX = {
        2.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 2.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 1.0f
    };

    private static final String VERTEX_SHADER =
        "uniform mat4 projectionMatrix;\n" +
        "uniform mat4 textureMatrix;\n" +
        "attribute vec4 vPosition;\n" +
        "attribute vec4 aTexCoord;\n" +
        "varying vec2 vTexCoord;\n" +
        "void main() {\n" +
        "  gl_Position = projectionMatrix * vPosition;\n" +
        "  vTexCoord = (textureMatrix * vec4(aTexCoord.x, aTexCoord.y, 0.0, 1.0)).xy;\n" +
        "}\n";

    private static String FRAGMENT_SHADER_OES =
        "#extension GL_OES_EGL_image_external : require\n" +
        "precision mediump float;\n" +
        "varying vec2 vTexCoord; \n" +
        "uniform samplerExternalOES sTexture; \n" +
        "void main() {\n" +
        "  gl_FragColor = texture2D(sTexture, vTexCoord); \n" +
        "}\n";
  

    private static final float TEXTURE_MAP[] = {
                0.0f, 1.0f, // top left
                0.0f, 0.0f, // bottom left
                1.0f, 1.0f, // top right
                1.0f, 0.0f, // bottom right
    };

    private static final float TEXTURE_MAP_INVERTED[] = {
                0.0f, 0.0f, // bottom left
                0.0f, 1.0f, // top left
                1.0f, 0.0f, // bottom right
                1.0f, 1.0f, // top right
    };

    private SurfaceTextureLayer(int textureId) {
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
    }

    public static SurfaceTextureLayer create() {
        int textureId = TextureGenerator.get().take();
        if (textureId == 0)
            return null;

        return new SurfaceTextureLayer(textureId);
    }

    // For SurfaceTexture.OnFrameAvailableListener
    public void onFrameAvailable(SurfaceTexture texture) {
        mHaveFrame = true;
        GeckoApp.mAppContext.requestRender();
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

        return true;
    }

    private static boolean ensureProgram() {
        if (mProgram != 0)
            return true;

        int vertexShader = loadShader(GLES20.GL_VERTEX_SHADER, VERTEX_SHADER);
        int fragmentShader = loadShader(GLES20.GL_FRAGMENT_SHADER, FRAGMENT_SHADER_OES);

        mProgram = GLES20.glCreateProgram();
        GLES20.glAttachShader(mProgram, vertexShader);
        GLES20.glAttachShader(mProgram, fragmentShader);
        GLES20.glLinkProgram(mProgram);

        mPositionHandle = GLES20.glGetAttribLocation(mProgram, "vPosition");
        mTextureHandle = GLES20.glGetAttribLocation(mProgram, "aTexCoord");
        mSampleHandle = GLES20.glGetUniformLocation(mProgram, "sTexture");
        mProjectionMatrixHandle = GLES20.glGetUniformLocation(mProgram, "projectionMatrix");
        mTextureMatrixHandle = GLES20.glGetUniformLocation(mProgram, "textureMatrix");

        return mProgram != 0;
    }

    private static int loadShader(int type, String shaderCode) {
        int shader = GLES20.glCreateShader(type);
        GLES20.glShaderSource(shader, shaderCode);
        GLES20.glCompileShader(shader);
        return shader;
    }

    private static void activateProgram() {
        GLES20.glUseProgram(mProgram);
    }

    public static void deactivateProgram() {
        GLES20.glDisableVertexAttribArray(mTextureHandle);
        GLES20.glDisableVertexAttribArray(mPositionHandle);
        GLES20.glUseProgram(0);
    }

    @Override
    public void draw(RenderContext context) {
        if (!ensureProgram() || !mHaveFrame)
            return;

        RectF rect = getBounds(context);
        RectF viewport = context.viewport;
        rect.offset(-viewport.left, -viewport.top);

        float viewWidth = viewport.width();
        float viewHeight = viewport.height();

        float top = viewHeight - rect.top;
        float bot = viewHeight - rect.bottom;

        float[] textureCoords = mInverted ? TEXTURE_MAP_INVERTED : TEXTURE_MAP;

        // Coordinates for the scrollbar's body combined with the texture coordinates
        float[] coords = {
            // x, y, z, texture_x, texture_y
            rect.left/viewWidth, bot/viewHeight, 0,
            textureCoords[0], textureCoords[1],

            rect.left/viewWidth, (bot+rect.height())/viewHeight, 0,
            textureCoords[2], textureCoords[3],

            (rect.left+rect.width())/viewWidth, bot/viewHeight, 0,
            textureCoords[4], textureCoords[5],

            (rect.left+rect.width())/viewWidth, (bot+rect.height())/viewHeight, 0,
            textureCoords[6], textureCoords[7]
        };

        FloatBuffer coordBuffer = context.coordBuffer;
        coordBuffer.position(0);
        coordBuffer.put(coords);

        activateProgram();

        // Set the transformation matrix
        GLES20.glUniformMatrix4fv(mProjectionMatrixHandle, 1, false, PROJECTION_MATRIX, 0);

        // Enable the arrays from which we get the vertex and texture coordinates
        GLES20.glEnableVertexAttribArray(mPositionHandle);
        GLES20.glEnableVertexAttribArray(mTextureHandle);

        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glUniform1i(mSampleHandle, 0);
        GLES20.glBindTexture(LOCAL_GL_TEXTURE_EXTERNAL_OES, mTextureId);
      
        mSurfaceTexture.updateTexImage();
        mSurfaceTexture.getTransformMatrix(mTextureTransform);

        GLES20.glUniformMatrix4fv(mTextureMatrixHandle, 1, false, mTextureTransform, 0);

        // Vertex coordinates are x,y,z starting at position 0 into the buffer.
        coordBuffer.position(0);
        GLES20.glVertexAttribPointer(mPositionHandle, 3, GLES20.GL_FLOAT, false, 20,
                coordBuffer);

        // Texture coordinates are texture_x, texture_y starting at position 3 into the buffer.
        coordBuffer.position(3);
        GLES20.glVertexAttribPointer(mTextureHandle, 3, GLES20.GL_FLOAT, false, 20,
                coordBuffer);

        if (mBlend) {
            GLES20.glEnable(GLES20.GL_BLEND);
            GLES20.glBlendFunc(GLES20.GL_ONE, GLES20.GL_ONE_MINUS_SRC_ALPHA);
        }
        
        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);

        if (mBlend)
            GLES20.glDisable(GLES20.GL_BLEND);
        
        deactivateProgram();
    }

    public SurfaceTexture getSurfaceTexture() {
        return mSurfaceTexture;
    }

    public Surface getSurface() {
        return mSurface;
    }
}

