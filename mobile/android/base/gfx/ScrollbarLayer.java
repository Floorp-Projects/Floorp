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
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kartikaya Gupta <kgupta@mozilla.com>
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

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PointF;
import android.graphics.PorterDuff;
import android.graphics.Rect;
import android.graphics.RectF;
import android.opengl.GLES20;
import android.util.Log;
import java.nio.ByteBuffer;
import java.nio.FloatBuffer;
import org.mozilla.gecko.FloatUtils;
import org.mozilla.gecko.GeckoAppShell;

/**
 * Draws a small rect. This is scaled to become a scrollbar.
 */
public class ScrollbarLayer extends TileLayer {
    public static final long FADE_DELAY = 500; // milliseconds before fade-out starts
    private static final float FADE_AMOUNT = 0.03f; // how much (as a percent) the scrollbar should fade per frame

    private static final int PADDING = 1;   // gap between scrollbar and edge of viewport
    private static final int BAR_SIZE = 6;
    private static final int CAP_RADIUS = (BAR_SIZE / 2);

    private final boolean mVertical;
    private final ByteBuffer mBuffer;
    private final Bitmap mBitmap;
    private final Canvas mCanvas;
    private float mOpacity;
    private boolean mFinalized = false;

    private LayerRenderer mRenderer;
    private int mProgram;
    private int mPositionHandle;
    private int mTextureHandle;
    private int mSampleHandle;
    private int mTMatrixHandle;
    private int mOpacityHandle;

    // Fragment shader used to draw the scroll-bar with opacity
    private static final String FRAGMENT_SHADER =
        "precision mediump float;\n" +
        "varying vec2 vTexCoord;\n" +
        "uniform sampler2D sTexture;\n" +
        "uniform float uOpacity;\n" +
        "void main() {\n" +
        "    gl_FragColor = texture2D(sTexture, vec2(vTexCoord.x, 1.0 - vTexCoord.y));\n" +
        "    gl_FragColor.a *= uOpacity;\n" +
        "}\n";

    // Dimensions of the texture image
    private static final float TEX_HEIGHT = 8.0f;
    private static final float TEX_WIDTH = 8.0f;

    // Texture coordinates for the scrollbar's body
    // We take a 1x1 pixel from the center of the image and scale it to become the bar
    private static final float[] BODY_TEX_COORDS = {
        // x, y
        CAP_RADIUS/TEX_WIDTH, CAP_RADIUS/TEX_HEIGHT,
        CAP_RADIUS/TEX_WIDTH, (CAP_RADIUS+1)/TEX_HEIGHT,
        (CAP_RADIUS+1)/TEX_WIDTH, CAP_RADIUS/TEX_HEIGHT,
        (CAP_RADIUS+1)/TEX_WIDTH, (CAP_RADIUS+1)/TEX_HEIGHT
    };

    // Texture coordinates for the top cap of the scrollbar
    private static final float[] TOP_CAP_TEX_COORDS = {
        // x, y
        0                 , 1.0f - CAP_RADIUS/TEX_HEIGHT,
        0                 , 1.0f,
        BAR_SIZE/TEX_WIDTH, 1.0f - CAP_RADIUS/TEX_HEIGHT,
        BAR_SIZE/TEX_WIDTH, 1.0f
    };

    // Texture coordinates for the bottom cap of the scrollbar
    private static final float[] BOT_CAP_TEX_COORDS = {
        // x, y
        0                 , 1.0f - BAR_SIZE/TEX_HEIGHT,
        0                 , 1.0f - CAP_RADIUS/TEX_HEIGHT,
        BAR_SIZE/TEX_WIDTH, 1.0f - BAR_SIZE/TEX_HEIGHT,
        BAR_SIZE/TEX_WIDTH, 1.0f - CAP_RADIUS/TEX_HEIGHT
    };

    // Texture coordinates for the left cap of the scrollbar
    private static final float[] LEFT_CAP_TEX_COORDS = {
        // x, y
        0                   , 1.0f - BAR_SIZE/TEX_HEIGHT,
        0                   , 1.0f,
        CAP_RADIUS/TEX_WIDTH, 1.0f - BAR_SIZE/TEX_HEIGHT,
        CAP_RADIUS/TEX_WIDTH, 1.0f
    };

    // Texture coordinates for the right cap of the scrollbar
    private static final float[] RIGHT_CAP_TEX_COORDS = {
        // x, y
        CAP_RADIUS/TEX_WIDTH, 1.0f - BAR_SIZE/TEX_HEIGHT,
        CAP_RADIUS/TEX_WIDTH, 1.0f,
        BAR_SIZE/TEX_WIDTH  , 1.0f - BAR_SIZE/TEX_HEIGHT,
        BAR_SIZE/TEX_WIDTH  , 1.0f
    };

    private ScrollbarLayer(LayerRenderer renderer, CairoImage image, boolean vertical, ByteBuffer buffer) {
        super(image, TileLayer.PaintMode.NORMAL);
        mVertical = vertical;
        mBuffer = buffer;
        mRenderer = renderer;

        IntSize size = image.getSize();
        mBitmap = Bitmap.createBitmap(size.width, size.height, Bitmap.Config.ARGB_8888);
        mCanvas = new Canvas(mBitmap);

        // Paint a spot to use as the scroll indicator
        Paint foregroundPaint = new Paint();
        foregroundPaint.setAntiAlias(true);
        foregroundPaint.setStyle(Paint.Style.FILL);
        foregroundPaint.setColor(Color.argb(127, 0, 0, 0));

        mCanvas.drawColor(Color.argb(0, 0, 0, 0), PorterDuff.Mode.CLEAR);
        mCanvas.drawCircle(CAP_RADIUS, CAP_RADIUS, CAP_RADIUS, foregroundPaint);

        mBitmap.copyPixelsToBuffer(mBuffer.asIntBuffer());
    }

    protected void finalize() throws Throwable {
        try {
            if (!mFinalized && mBuffer != null)
                GeckoAppShell.freeDirectBuffer(mBuffer);
            mFinalized = true;
        } finally {
            super.finalize();
        }
    }

    public static ScrollbarLayer create(LayerRenderer renderer, boolean vertical) {
        // just create an empty image for now, it will get drawn
        // on demand anyway
        int imageSize = IntSize.nextPowerOfTwo(BAR_SIZE);
        ByteBuffer buffer = GeckoAppShell.allocateDirectBuffer(imageSize * imageSize * 4);
        CairoImage image = new BufferedCairoImage(buffer, imageSize, imageSize,
                                                  CairoImage.FORMAT_ARGB32);
        return new ScrollbarLayer(renderer, image, vertical, buffer);
    }

    private void createProgram() {
        int vertexShader = LayerRenderer.loadShader(GLES20.GL_VERTEX_SHADER,
                                                    LayerRenderer.DEFAULT_VERTEX_SHADER);
        int fragmentShader = LayerRenderer.loadShader(GLES20.GL_FRAGMENT_SHADER,
                                                      FRAGMENT_SHADER);

        mProgram = GLES20.glCreateProgram();
        GLES20.glAttachShader(mProgram, vertexShader);   // add the vertex shader to program
        GLES20.glAttachShader(mProgram, fragmentShader); // add the fragment shader to program
        GLES20.glLinkProgram(mProgram);                  // creates OpenGL program executables

        // Get handles to the shaders' vPosition, aTexCoord, sTexture, and uTMatrix members.
        mPositionHandle = GLES20.glGetAttribLocation(mProgram, "vPosition");
        mTextureHandle = GLES20.glGetAttribLocation(mProgram, "aTexCoord");
        mSampleHandle = GLES20.glGetUniformLocation(mProgram, "sTexture");
        mTMatrixHandle = GLES20.glGetUniformLocation(mProgram, "uTMatrix");
        mOpacityHandle = GLES20.glGetUniformLocation(mProgram, "uOpacity");
    }

    private void activateProgram() {
        // Add the program to the OpenGL environment
        GLES20.glUseProgram(mProgram);

        // Set the transformation matrix
        GLES20.glUniformMatrix4fv(mTMatrixHandle, 1, false,
                                  LayerRenderer.DEFAULT_TEXTURE_MATRIX, 0);

        // Enable the arrays from which we get the vertex and texture coordinates
        GLES20.glEnableVertexAttribArray(mPositionHandle);
        GLES20.glEnableVertexAttribArray(mTextureHandle);

        GLES20.glUniform1i(mSampleHandle, 0);
        GLES20.glUniform1f(mOpacityHandle, mOpacity);
    }

    private void deactivateProgram() {
        GLES20.glDisableVertexAttribArray(mTextureHandle);
        GLES20.glDisableVertexAttribArray(mPositionHandle);
        GLES20.glUseProgram(0);
    }

    /**
     * Decrease the opacity of the scrollbar by one frame's worth.
     * Return true if the opacity was decreased, or false if the scrollbars
     * are already fully faded out.
     */
    public boolean fade() {
        if (FloatUtils.fuzzyEquals(mOpacity, 0.0f)) {
            return false;
        }
        beginTransaction(); // called on compositor thread
        mOpacity = Math.max(mOpacity - FADE_AMOUNT, 0.0f);
        endTransaction();
        return true;
    }

    /**
     * Restore the opacity of the scrollbar to fully opaque.
     * Return true if the opacity was changed, or false if the scrollbars
     * are already fully opaque.
     */
    public boolean unfade() {
        if (FloatUtils.fuzzyEquals(mOpacity, 1.0f)) {
            return false;
        }
        beginTransaction(); // called on compositor thread
        mOpacity = 1.0f;
        endTransaction();
        return true;
    }

    @Override
    public void draw(RenderContext context) {
        if (!initialized())
            return;

        // Create the shader program, if necessary
        if (mProgram == 0) {
            createProgram();
        }

        // Enable the shader program
        mRenderer.deactivateDefaultProgram();
        activateProgram();

        GLES20.glEnable(GLES20.GL_BLEND);
        GLES20.glBlendFunc(GLES20.GL_SRC_ALPHA, GLES20.GL_ONE_MINUS_SRC_ALPHA);

        Rect rect = RectUtils.round(mVertical
                ? getVerticalRect(context)
                : getHorizontalRect(context));
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, getTextureID());

        float viewWidth = context.viewport.width();
        float viewHeight = context.viewport.height();

        float top = viewHeight - rect.top;
        float bot = viewHeight - rect.bottom;

        // Coordinates for the scrollbar's body combined with the texture coordinates
        float[] bodyCoords = {
            // x, y, z, texture_x, texture_y
            rect.left/viewWidth, bot/viewHeight, 0,
            BODY_TEX_COORDS[0], BODY_TEX_COORDS[1],

            rect.left/viewWidth, (bot+rect.height())/viewHeight, 0,
            BODY_TEX_COORDS[2], BODY_TEX_COORDS[3],

            (rect.left+rect.width())/viewWidth, bot/viewHeight, 0,
            BODY_TEX_COORDS[4], BODY_TEX_COORDS[5],

            (rect.left+rect.width())/viewWidth, (bot+rect.height())/viewHeight, 0,
            BODY_TEX_COORDS[6], BODY_TEX_COORDS[7]
        };

        // Get the buffer and handles from the context
        FloatBuffer coordBuffer = context.coordBuffer;
        int positionHandle = mPositionHandle;
        int textureHandle = mTextureHandle;

        // Make sure we are at position zero in the buffer in case other draw methods did not
        // clean up after themselves
        coordBuffer.position(0);
        coordBuffer.put(bodyCoords);

        // Unbind any the current array buffer so we can use client side buffers
        GLES20.glBindBuffer(GLES20.GL_ARRAY_BUFFER, 0);

        // Vertex coordinates are x,y,z starting at position 0 into the buffer.
        coordBuffer.position(0);
        GLES20.glVertexAttribPointer(positionHandle, 3, GLES20.GL_FLOAT, false, 20,
                coordBuffer);

        // Texture coordinates are texture_x, texture_y starting at position 3 into the buffer.
        coordBuffer.position(3);
        GLES20.glVertexAttribPointer(textureHandle, 2, GLES20.GL_FLOAT, false, 20,
                coordBuffer);

        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);

        // Reset the position in the buffer for the next set of vertex and texture coordinates.
        coordBuffer.position(0);

        if (mVertical) {
            // top endcap
            float[] topCap = {
                // x, y, z, texture_x, texture_y
                rect.left/viewWidth, top/viewHeight, 0,
                TOP_CAP_TEX_COORDS[0], TOP_CAP_TEX_COORDS[1],

                rect.left/viewWidth, (top+CAP_RADIUS)/viewHeight, 0,
                TOP_CAP_TEX_COORDS[2], TOP_CAP_TEX_COORDS[3],

                (rect.left+BAR_SIZE)/viewWidth, top/viewHeight, 0,
                TOP_CAP_TEX_COORDS[4], TOP_CAP_TEX_COORDS[5],

                (rect.left+BAR_SIZE)/viewWidth, (top+CAP_RADIUS)/viewHeight, 0,
                TOP_CAP_TEX_COORDS[6], TOP_CAP_TEX_COORDS[7]
            };

            coordBuffer.put(topCap);

            // Vertex coordinates are x,y,z starting at position 0 into the buffer.
            coordBuffer.position(0);
            GLES20.glVertexAttribPointer(positionHandle, 3, GLES20.GL_FLOAT, false, 20,
                    coordBuffer);

            // Texture coordinates are texture_x, texture_y starting at position 3 into the
            // buffer.
            coordBuffer.position(3);
            GLES20.glVertexAttribPointer(textureHandle, 2, GLES20.GL_FLOAT, false, 20,
                    coordBuffer);

            GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);

            // Reset the position in the buffer for the next set of vertex and texture
            // coordinates.
            coordBuffer.position(0);

            // bottom endcap
            float[] botCap = {
                // x, y, z, texture_x, texture_y
                rect.left/viewWidth, (bot-CAP_RADIUS)/viewHeight, 0,
                BOT_CAP_TEX_COORDS[0], BOT_CAP_TEX_COORDS[1],

                rect.left/viewWidth, (bot)/viewHeight, 0,
                BOT_CAP_TEX_COORDS[2], BOT_CAP_TEX_COORDS[3],

                (rect.left+BAR_SIZE)/viewWidth, (bot-CAP_RADIUS)/viewHeight, 0,
                BOT_CAP_TEX_COORDS[4], BOT_CAP_TEX_COORDS[5],

                (rect.left+BAR_SIZE)/viewWidth, (bot)/viewHeight, 0,
                BOT_CAP_TEX_COORDS[6], BOT_CAP_TEX_COORDS[7]
            };

            coordBuffer.put(botCap);

            // Vertex coordinates are x,y,z starting at position 0 into the buffer.
            coordBuffer.position(0);
            GLES20.glVertexAttribPointer(positionHandle, 3, GLES20.GL_FLOAT, false, 20,
                    coordBuffer);

            // Texture coordinates are texture_x, texture_y starting at position 3 into the
            // buffer.
            coordBuffer.position(3);
            GLES20.glVertexAttribPointer(textureHandle, 2, GLES20.GL_FLOAT, false, 20,
                    coordBuffer);

            GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);

            // Reset the position in the buffer for the next set of vertex and texture
            // coordinates.
            coordBuffer.position(0);
        } else {
            // left endcap
            float[] leftCap = {
                // x, y, z, texture_x, texture_y
                (rect.left-CAP_RADIUS)/viewWidth, bot/viewHeight, 0,
                LEFT_CAP_TEX_COORDS[0], LEFT_CAP_TEX_COORDS[1],
                (rect.left-CAP_RADIUS)/viewWidth, (bot+BAR_SIZE)/viewHeight, 0,
                LEFT_CAP_TEX_COORDS[2], LEFT_CAP_TEX_COORDS[3],
                (rect.left)/viewWidth, bot/viewHeight, 0, LEFT_CAP_TEX_COORDS[4],
                LEFT_CAP_TEX_COORDS[5],
                (rect.left)/viewWidth, (bot+BAR_SIZE)/viewHeight, 0,
                LEFT_CAP_TEX_COORDS[6], LEFT_CAP_TEX_COORDS[7]
            };

            coordBuffer.put(leftCap);

            // Vertex coordinates are x,y,z starting at position 0 into the buffer.
            coordBuffer.position(0);
            GLES20.glVertexAttribPointer(positionHandle, 3, GLES20.GL_FLOAT, false, 20,
                    coordBuffer);

            // Texture coordinates are texture_x, texture_y starting at position 3 into the
            // buffer.
            coordBuffer.position(3);
            GLES20.glVertexAttribPointer(textureHandle, 2, GLES20.GL_FLOAT, false, 20,
                    coordBuffer);

            GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);

            // Reset the position in the buffer for the next set of vertex and texture
            // coordinates.
            coordBuffer.position(0);

            // right endcap
            float[] rightCap = {
                // x, y, z, texture_x, texture_y
                rect.right/viewWidth, (bot)/viewHeight, 0,
                RIGHT_CAP_TEX_COORDS[0], RIGHT_CAP_TEX_COORDS[1],

                rect.right/viewWidth, (bot+BAR_SIZE)/viewHeight, 0,
                RIGHT_CAP_TEX_COORDS[2], RIGHT_CAP_TEX_COORDS[3],

                (rect.right+CAP_RADIUS)/viewWidth, (bot)/viewHeight, 0,
                RIGHT_CAP_TEX_COORDS[4], RIGHT_CAP_TEX_COORDS[5],

                (rect.right+CAP_RADIUS)/viewWidth, (bot+BAR_SIZE)/viewHeight, 0,
                RIGHT_CAP_TEX_COORDS[6], RIGHT_CAP_TEX_COORDS[7]
            };

            coordBuffer.put(rightCap);

            // Vertex coordinates are x,y,z starting at position 0 into the buffer.
            coordBuffer.position(0);
            GLES20.glVertexAttribPointer(positionHandle, 3, GLES20.GL_FLOAT, false, 20,
                    coordBuffer);

            // Texture coordinates are texture_x, texture_y starting at position 3 into the
            // buffer.
            coordBuffer.position(3);
            GLES20.glVertexAttribPointer(textureHandle, 2, GLES20.GL_FLOAT, false, 20,
                    coordBuffer);

            GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
        }

        // Enable the default shader program again
        deactivateProgram();
        mRenderer.activateDefaultProgram();
    }

    private RectF getVerticalRect(RenderContext context) {
        RectF viewport = context.viewport;
        FloatSize pageSize = context.pageSize;
        float barStart = (viewport.height() * viewport.top / pageSize.height) + CAP_RADIUS;
        float barEnd = (viewport.height() * viewport.bottom / pageSize.height) - CAP_RADIUS;
        if (barStart > barEnd) {
            float middle = (barStart + barEnd) / 2.0f;
            barStart = barEnd = middle;
        }
        float right = viewport.width() - PADDING;
        return new RectF(right - BAR_SIZE, barStart, right, barEnd);
    }

    private RectF getHorizontalRect(RenderContext context) {
        RectF viewport = context.viewport;
        FloatSize pageSize = context.pageSize;
        float barStart = (viewport.width() * viewport.left / pageSize.width) + CAP_RADIUS;
        float barEnd = (viewport.width() * viewport.right / pageSize.width) - CAP_RADIUS;
        if (barStart > barEnd) {
            float middle = (barStart + barEnd) / 2.0f;
            barStart = barEnd = middle;
        }
        float bottom = viewport.height() - PADDING;
        return new RectF(barStart, bottom - BAR_SIZE, barEnd, bottom);
    }
}
