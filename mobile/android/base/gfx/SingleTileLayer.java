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

import org.mozilla.gecko.gfx.CairoImage;
import org.mozilla.gecko.gfx.CairoUtils;
import org.mozilla.gecko.gfx.IntSize;
import org.mozilla.gecko.gfx.LayerController;
import org.mozilla.gecko.gfx.TileLayer;
import android.graphics.PointF;
import android.graphics.RectF;
import android.opengl.GLES20;
import android.util.Log;
import java.nio.FloatBuffer;
import javax.microedition.khronos.opengles.GL10;

/**
 * Encapsulates the logic needed to draw a single textured tile.
 *
 * TODO: Repeating textures really should be their own type of layer.
 */
public class SingleTileLayer extends TileLayer {
    public SingleTileLayer(CairoImage image) { this(false, image); }

    public SingleTileLayer(boolean repeat, CairoImage image) {
        super(repeat, image);
    }

    @Override
    public void draw(RenderContext context) {
        // mTextureIDs may be null here during startup if Layer.java's draw method
        // failed to acquire the transaction lock and call performUpdates.
        if (!initialized())
            return;

        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, getTextureID());

        RectF bounds;
        int[] cropRect;
        IntSize size = getSize();
        RectF viewport = context.viewport;

        if (repeats()) {
            bounds = new RectF(0.0f, 0.0f, viewport.width(), viewport.height());
            int width = Math.round(viewport.width());
            int height = Math.round(viewport.height());
            cropRect = new int[] { 0, 0, width, height };
        } else {
            bounds = getBounds(context, new FloatSize(size));
            cropRect = new int[] { 0, 0, size.width, size.height };
        }

        float height = bounds.height();
        float left = bounds.left - viewport.left;
        float top = viewport.height() - (bounds.top + height - viewport.top);

        float[] coords = {
            //x, y, z, texture_x, texture_y
            left/viewport.width(), top/viewport.height(), 0,
            cropRect[0]/(float)size.width, cropRect[1]/(float)size.height,

            left/viewport.width(), (top+height)/viewport.height(), 0,
            cropRect[0]/(float)size.width, cropRect[3]/(float)size.height,

            (left+bounds.width())/viewport.width(), top/viewport.height(), 0,
            cropRect[2]/(float)size.width, cropRect[1]/(float)size.height,

            (left+bounds.width())/viewport.width(), (top+height)/viewport.height(), 0,
            cropRect[2]/(float)size.width, cropRect[3]/(float)size.height
        };

        FloatBuffer coordBuffer = context.coordBuffer;
        int positionHandle = context.positionHandle;
        int textureHandle = context.textureHandle;

        // Make sure we are at position zero in the buffer in case other draw methods did not clean
        // up after themselves
        coordBuffer.position(0);
        coordBuffer.put(coords);

        // Vertex coordinates are x,y,z starting at position 0 into the buffer.
        coordBuffer.position(0);
        GLES20.glVertexAttribPointer(positionHandle, 3, GLES20.GL_FLOAT, false, 20, coordBuffer);

        // Texture coordinates are texture_x, texture_y starting at position 3 into the buffer.
        coordBuffer.position(3);
        GLES20.glVertexAttribPointer(textureHandle, 2, GLES20.GL_FLOAT, false, 20, coordBuffer);
        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
    }
}

