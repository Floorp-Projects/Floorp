/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.util.FloatUtils;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.graphics.RectF;
import android.opengl.GLES20;
import android.util.Log;

import java.nio.FloatBuffer;
import java.nio.ByteBuffer;

public class ScrollbarLayer extends Layer {
    private static final String LOGTAG = "GeckoScrollbarLayer";

    public static final long FADE_DELAY = 500; // milliseconds before fade-out starts
    private static final float FADE_AMOUNT = 0.03f; // how much (as a percent) the scrollbar should fade per frame

    private final boolean mVertical;
    private float mOpacity;
    private final Rect mDirtyRect;
    private IntSize mSize;

    private int[] mTextureIDs;

    private final BufferedImage mImage;

    // To avoid excessive GC, declare some objects here that would otherwise
    // be created and destroyed frequently during draw().
    private final RectF mBarRectF;
    private final Rect mBarRect;
    private final float[] mCoords;
    private final RectF mCapRectF;

    private final LayerRenderer mRenderer;
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
        "    gl_FragColor = texture2D(sTexture, vTexCoord);\n" +
        "    gl_FragColor.a *= uOpacity;\n" +
        "}\n";

    // Dimensions of the texture bitmap (will always be power-of-two)
    private final int mTexWidth;
    private final int mTexHeight;
    // Some useful dimensions of the actual content in the bitmap
    private final int mBarWidth;
    private final int mCapLength;

    private final Rect mStartCapTexCoords;  // top/left endcap coordinates
    private final Rect mBodyTexCoords;      // 1-pixel slice of the texture to be stretched
    private final Rect mEndCapTexCoords;    // bottom/right endcap coordinates

    ScrollbarLayer(LayerRenderer renderer, Bitmap scrollbarImage, IntSize imageSize, boolean vertical) {
        super(new IntSize(scrollbarImage.getHeight(), scrollbarImage.getWidth()));
        mImage = new BufferedImage(scrollbarImage);
        mRenderer = renderer;
        mVertical = vertical;

        mBarRectF = new RectF();
        mBarRect = new Rect();
        mCoords = new float[20];
        mCapRectF = new RectF();
        mDirtyRect = new Rect();
        mSize = new IntSize(0, 0);

        mTexHeight = scrollbarImage.getHeight();
        mTexWidth = scrollbarImage.getWidth();

        if (mVertical) {
            mBarWidth = imageSize.width;
            mCapLength = imageSize.height / 2;
            mStartCapTexCoords = new Rect(0, mTexHeight - mCapLength, imageSize.width, mTexHeight);
            mBodyTexCoords = new Rect(0, mTexHeight - (mCapLength + 1), imageSize.width, mTexHeight - mCapLength);
            mEndCapTexCoords = new Rect(0, mTexHeight - imageSize.height, imageSize.width, mTexHeight - (mCapLength + 1));
        } else {
            mBarWidth = imageSize.height;
            mCapLength = imageSize.width / 2;
            mStartCapTexCoords = new Rect(0, mTexHeight - imageSize.height, mCapLength, mTexHeight);
            mBodyTexCoords = new Rect(mCapLength, mTexHeight - imageSize.height, mCapLength + 1, mTexHeight);
            mEndCapTexCoords = new Rect(mCapLength + 1, mTexHeight - imageSize.height, imageSize.width, mTexHeight);
        }
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

        if (mVertical) {
            getVerticalRect(context, mBarRectF);
        } else {
            getHorizontalRect(context, mBarRectF);
        }
        RectUtils.round(mBarRectF, mBarRect);

        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, getTextureID());

        float viewWidth = context.viewport.width();
        float viewHeight = context.viewport.height();

        mBarRectF.set(mBarRect.left, viewHeight - mBarRect.top, mBarRect.right, viewHeight - mBarRect.bottom);
        mBarRectF.offset(context.offset.x, -context.offset.y);

        // We take a 1-pixel slice from the center of the image and scale it to become the bar
        fillRectCoordBuffer(mCoords, mBarRectF, viewWidth, viewHeight, mBodyTexCoords, mTexWidth, mTexHeight);

        // Get the buffer and handles from the context
        FloatBuffer coordBuffer = context.coordBuffer;
        int positionHandle = mPositionHandle;
        int textureHandle = mTextureHandle;

        // Make sure we are at position zero in the buffer in case other draw methods did not
        // clean up after themselves
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

        // Reset the position in the buffer for the next set of vertex and texture coordinates.
        coordBuffer.position(0);
        if (mVertical) {
            // top endcap
            mCapRectF.set(mBarRectF.left, mBarRectF.top + mCapLength, mBarRectF.right, mBarRectF.top);
        } else {
            // left endcap
            mCapRectF.set(mBarRectF.left - mCapLength, mBarRectF.bottom + mBarWidth, mBarRectF.left, mBarRectF.bottom);
        }

        fillRectCoordBuffer(mCoords, mCapRectF, viewWidth, viewHeight, mStartCapTexCoords, mTexWidth, mTexHeight);
        coordBuffer.put(mCoords);

        // Vertex coordinates are x,y,z starting at position 0 into the buffer.
        coordBuffer.position(0);
        GLES20.glVertexAttribPointer(positionHandle, 3, GLES20.GL_FLOAT, false, 20, coordBuffer);

        // Texture coordinates are texture_x, texture_y starting at position 3 into the buffer.
        coordBuffer.position(3);
        GLES20.glVertexAttribPointer(textureHandle, 2, GLES20.GL_FLOAT, false, 20, coordBuffer);

        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);

        // Reset the position in the buffer for the next set of vertex and texture coordinates.
        coordBuffer.position(0);
        if (mVertical) {
            // bottom endcap
            mCapRectF.set(mBarRectF.left, mBarRectF.bottom, mBarRectF.right, mBarRectF.bottom - mCapLength);
        } else {
            // right endcap
            mCapRectF.set(mBarRectF.right, mBarRectF.bottom + mBarWidth, mBarRectF.right + mCapLength, mBarRectF.bottom);
        }
        fillRectCoordBuffer(mCoords, mCapRectF, viewWidth, viewHeight, mEndCapTexCoords, mTexWidth, mTexHeight);
        coordBuffer.put(mCoords);

        // Vertex coordinates are x,y,z starting at position 0 into the buffer.
        coordBuffer.position(0);
        GLES20.glVertexAttribPointer(positionHandle, 3, GLES20.GL_FLOAT, false, 20, coordBuffer);

        // Texture coordinates are texture_x, texture_y starting at position 3 into the buffer.
        coordBuffer.position(3);
        GLES20.glVertexAttribPointer(textureHandle, 2, GLES20.GL_FLOAT, false, 20, coordBuffer);

        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);

        // Reset the position in the buffer for the next set of vertex and texture coordinates.
        coordBuffer.position(0);

        // Enable the default shader program again
        deactivateProgram();
        mRenderer.activateDefaultProgram();
    }

    private void getVerticalRect(RenderContext context, RectF dest) {
        RectF viewport = context.viewport;
        RectF pageRect = context.pageRect;
        float viewportHeight = viewport.height() - context.offset.y;
        float barStart = ((viewport.top - context.offset.y - pageRect.top) * (viewportHeight / pageRect.height())) + mCapLength;
        float barEnd = ((viewport.bottom - context.offset.y - pageRect.top) * (viewportHeight / pageRect.height())) - mCapLength;
        if (barStart > barEnd) {
            float middle = (barStart + barEnd) / 2.0f;
            barStart = barEnd = middle;
        }
        dest.set(viewport.width() - mBarWidth, barStart, viewport.width(), barEnd);
    }

    private void getHorizontalRect(RenderContext context, RectF dest) {
        RectF viewport = context.viewport;
        RectF pageRect = context.pageRect;
        float viewportWidth = viewport.width() - context.offset.x;
        float barStart = ((viewport.left - context.offset.x - pageRect.left) * (viewport.width() / pageRect.width())) + mCapLength;
        float barEnd = ((viewport.right - context.offset.x - pageRect.left) * (viewport.width() / pageRect.width())) - mCapLength;
        if (barStart > barEnd) {
            float middle = (barStart + barEnd) / 2.0f;
            barStart = barEnd = middle;
        }
        dest.set(barStart, viewport.height() - mBarWidth, barEnd, viewport.height());
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

        int imageFormat = mImage.getFormat();
        BufferedImageGLInfo glInfo = new BufferedImageGLInfo(imageFormat);

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
            throw new RuntimeException("Buffer/image size mismatch in ScrollbarLayer!");
        }
    }

    private void bindAndSetGLParameters() {
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, mTextureIDs[0]);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER,
                               GLES20.GL_LINEAR);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER,
                               GLES20.GL_LINEAR);

        int repeatMode = GLES20.GL_CLAMP_TO_EDGE;
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S, repeatMode);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T, repeatMode);
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
}
