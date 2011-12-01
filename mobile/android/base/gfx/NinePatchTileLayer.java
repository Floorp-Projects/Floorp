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

import org.mozilla.gecko.gfx.FloatSize;
import android.graphics.PointF;
import android.graphics.RectF;
import android.opengl.GLES11;
import android.opengl.GLES11Ext;
import android.util.Log;
import javax.microedition.khronos.opengles.GL10;
import java.nio.FloatBuffer;

/**
 * Encapsulates the logic needed to draw a nine-patch bitmap using OpenGL ES.
 *
 * For more information on nine-patch bitmaps, see the following document:
 *   http://developer.android.com/guide/topics/graphics/2d-graphics.html#nine-patch
 */
public class NinePatchTileLayer extends TileLayer {
    private static final int PATCH_SIZE = 16;
    private static final int TEXTURE_SIZE = 48;

    public NinePatchTileLayer(CairoImage image) {
        super(false, image);
    }

    @Override
    public void draw(RenderContext context) {
        if (!initialized())
            return;

        GLES11.glBlendFunc(GL10.GL_SRC_ALPHA, GL10.GL_ONE_MINUS_SRC_ALPHA);
        GLES11.glEnable(GL10.GL_BLEND);
        try {
            GLES11.glBindTexture(GL10.GL_TEXTURE_2D, getTextureID());
            drawPatches(context);
        } finally {
            GLES11.glDisable(GL10.GL_BLEND);
        }
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

        FloatSize size = context.pageSize;
        float width = size.width, height = size.height;

        drawPatch(context, 0, 0,                                                    /* 0 */
                  0.0f, 0.0f, PATCH_SIZE, PATCH_SIZE);
        drawPatch(context, PATCH_SIZE, 0,                                           /* 1 */
                  PATCH_SIZE, 0.0f, width, PATCH_SIZE);
        drawPatch(context, PATCH_SIZE * 2, 0,                                       /* 2 */
                  PATCH_SIZE + width, 0.0f, PATCH_SIZE, PATCH_SIZE);
        drawPatch(context, 0, PATCH_SIZE,                                           /* 3 */
                  0.0f, PATCH_SIZE, PATCH_SIZE, height);
        drawPatch(context, PATCH_SIZE * 2, PATCH_SIZE,                              /* 4 */
                  PATCH_SIZE + width, PATCH_SIZE, PATCH_SIZE, height);
        drawPatch(context, 0, PATCH_SIZE * 2,                                       /* 5 */
                  0.0f, PATCH_SIZE + height, PATCH_SIZE, PATCH_SIZE);
        drawPatch(context, PATCH_SIZE, PATCH_SIZE * 2,                              /* 6 */
                  PATCH_SIZE, PATCH_SIZE + height, width, PATCH_SIZE);
        drawPatch(context, PATCH_SIZE * 2, PATCH_SIZE * 2,                          /* 7 */
                  PATCH_SIZE + width, PATCH_SIZE + height, PATCH_SIZE, PATCH_SIZE);
    }

    private void drawPatch(RenderContext context, int textureX, int textureY, float tileX,
                           float tileY, float tileWidth, float tileHeight) {
        int[] cropRect = { textureX, textureY + PATCH_SIZE, PATCH_SIZE, -PATCH_SIZE };
        GLES11.glTexParameteriv(GL10.GL_TEXTURE_2D, GLES11Ext.GL_TEXTURE_CROP_RECT_OES, cropRect,
                                0);

        RectF viewport = context.viewport;
        float viewportHeight = viewport.height();
        float drawX = tileX - viewport.left - PATCH_SIZE;
        float drawY = viewportHeight - (tileY + tileHeight - viewport.top - PATCH_SIZE);
        GLES11Ext.glDrawTexfOES(drawX, drawY, 0.0f, tileWidth, tileHeight);
    }
}
