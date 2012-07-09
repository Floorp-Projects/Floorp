/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.gfx.FloatSize;
import android.graphics.PointF;
import android.graphics.RectF;
import android.util.Log;
import javax.microedition.khronos.opengles.GL10;
import java.nio.FloatBuffer;
import android.opengl.GLES20;

/**
 * Encapsulates the logic needed to draw a nine-patch bitmap using OpenGL ES.
 *
 * For more information on nine-patch bitmaps, see the following document:
 *   http://developer.android.com/guide/topics/graphics/2d-graphics.html#nine-patch
 */
public class NinePatchTileLayer extends TileLayer {
    private static final int PATCH_SIZE = 16;
    private static final int TEXTURE_SIZE = 64;

    public NinePatchTileLayer(CairoImage image) {
        super(image, TileLayer.PaintMode.NORMAL);
    }

    @Override
    public void draw(RenderContext context) {
        if (!initialized())
            return;

        GLES20.glBlendFunc(GLES20.GL_SRC_ALPHA, GLES20.GL_ONE_MINUS_SRC_ALPHA);
        GLES20.glEnable(GLES20.GL_BLEND);

        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, getTextureID());
        drawPatches(context);
    }

    private void drawPatches(RenderContext context) {
        /*
         * We divide the nine-patch bitmap up as follows:
         *
         *    +---+---+---+
         *    | 0 | 1 | 2 |
         *    +---+---+---+
         *    | 3 |   | 4 |
         *    +---+---+---+
         *    | 5 | 6 | 7 |
         *    +---+---+---+
         */

        // page is the rect of the "missing" center spot in the picture above
        RectF page = context.pageRect;

        drawPatch(context, 0, PATCH_SIZE * 3,                                              /* 0 */
                  page.left - PATCH_SIZE, page.top - PATCH_SIZE, PATCH_SIZE, PATCH_SIZE);
        drawPatch(context, PATCH_SIZE, PATCH_SIZE * 3,                                     /* 1 */
                  page.left, page.top - PATCH_SIZE, page.width(), PATCH_SIZE);
        drawPatch(context, PATCH_SIZE * 2, PATCH_SIZE * 3,                                 /* 2 */
                  page.right, page.top - PATCH_SIZE, PATCH_SIZE, PATCH_SIZE);
        drawPatch(context, 0, PATCH_SIZE * 2,                                              /* 3 */
                  page.left - PATCH_SIZE, page.top, PATCH_SIZE, page.height());
        drawPatch(context, PATCH_SIZE * 2, PATCH_SIZE * 2,                                 /* 4 */
                  page.right, page.top, PATCH_SIZE, page.height());
        drawPatch(context, 0, PATCH_SIZE,                                                  /* 5 */
                  page.left - PATCH_SIZE, page.bottom, PATCH_SIZE, PATCH_SIZE);
        drawPatch(context, PATCH_SIZE, PATCH_SIZE,                                         /* 6 */
                  page.left, page.bottom, page.width(), PATCH_SIZE);
        drawPatch(context, PATCH_SIZE * 2, PATCH_SIZE,                                     /* 7 */
                  page.right, page.bottom, PATCH_SIZE, PATCH_SIZE);
    }

    private void drawPatch(RenderContext context, int textureX, int textureY,
                           float tileX, float tileY, float tileWidth, float tileHeight) {
        RectF viewport = context.viewport;
        float viewportHeight = viewport.height();
        float drawX = tileX - viewport.left;
        float drawY = viewportHeight - (tileY + tileHeight - viewport.top);

        float[] coords = {
            //x, y, z, texture_x, texture_y
            drawX/viewport.width(), drawY/viewport.height(), 0,
            textureX/(float)TEXTURE_SIZE, textureY/(float)TEXTURE_SIZE,

            drawX/viewport.width(), (drawY+tileHeight)/viewport.height(), 0,
            textureX/(float)TEXTURE_SIZE, (textureY+PATCH_SIZE)/(float)TEXTURE_SIZE,

            (drawX+tileWidth)/viewport.width(), drawY/viewport.height(), 0,
            (textureX+PATCH_SIZE)/(float)TEXTURE_SIZE, textureY/(float)TEXTURE_SIZE,

            (drawX+tileWidth)/viewport.width(), (drawY+tileHeight)/viewport.height(), 0,
            (textureX+PATCH_SIZE)/(float)TEXTURE_SIZE, (textureY+PATCH_SIZE)/(float)TEXTURE_SIZE

        };

        // Get the buffer and handles from the context
        FloatBuffer coordBuffer = context.coordBuffer;
        int positionHandle = context.positionHandle;
        int textureHandle = context.textureHandle;

        // Make sure we are at position zero in the buffer in case other draw methods did not clean
        // up after themselves
        coordBuffer.position(0);
        coordBuffer.put(coords);

        // Unbind any the current array buffer so we can use client side buffers
        GLES20.glBindBuffer(GLES20.GL_ARRAY_BUFFER, 0);

        // Vertex coordinates are x,y,z starting at position 0 into the buffer.
        coordBuffer.position(0);
        GLES20.glVertexAttribPointer(positionHandle, 3, GLES20.GL_FLOAT, false, 20, coordBuffer);

        // Texture coordinates are texture_x, texture_y starting at position 3 into the buffer.
        coordBuffer.position(3);
        GLES20.glVertexAttribPointer(textureHandle, 2, GLES20.GL_FLOAT, false, 20, coordBuffer);

        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S,
                               GLES20.GL_CLAMP_TO_EDGE);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T,
                               GLES20.GL_CLAMP_TO_EDGE);

        // Use bilinear filtering for both magnification and minimization of the texture. This
        // applies only to the shadow layer so we do not incur a high overhead.
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER,
                               GLES20.GL_LINEAR);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER,
                               GLES20.GL_LINEAR);

        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
    }
}
