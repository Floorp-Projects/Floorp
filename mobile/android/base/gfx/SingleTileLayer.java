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

import org.mozilla.gecko.gfx.CairoImage;
import org.mozilla.gecko.gfx.CairoUtils;
import org.mozilla.gecko.gfx.IntSize;
import org.mozilla.gecko.gfx.LayerController;
import org.mozilla.gecko.gfx.TileLayer;
import android.util.Log;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import javax.microedition.khronos.opengles.GL10;

/**
 * Encapsulates the logic needed to draw a single textured tile.
 */
public class SingleTileLayer extends TileLayer {
    private FloatBuffer mTexCoordBuffer, mVertexBuffer;

    private static final float[] VERTICES = {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 0.0f
    };

    private static final float[] TEX_COORDS = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f
    };

    public SingleTileLayer() { this(false); }

    public SingleTileLayer(boolean repeat) {
        super(repeat);

        mVertexBuffer = createBuffer(VERTICES);
        mTexCoordBuffer = createBuffer(TEX_COORDS);
    }

    @Override
    protected void onTileDraw(GL10 gl) {
        IntSize size = getSize();

        if (repeats()) {
            gl.glMatrixMode(GL10.GL_TEXTURE);
            gl.glPushMatrix();
            gl.glScalef(LayerController.TILE_WIDTH / size.width,
                        LayerController.TILE_HEIGHT / size.height,
                        1.0f);

            gl.glMatrixMode(GL10.GL_MODELVIEW);
            gl.glScalef(LayerController.TILE_WIDTH, LayerController.TILE_HEIGHT, 1.0f);
        } else {
            gl.glScalef(size.width, size.height, 1.0f);
        }

        gl.glBindTexture(GL10.GL_TEXTURE_2D, getTextureID());
        drawTriangles(gl, mVertexBuffer, mTexCoordBuffer, 4);

        if (repeats()) {
            gl.glMatrixMode(GL10.GL_TEXTURE);
            gl.glPopMatrix();
            gl.glMatrixMode(GL10.GL_MODELVIEW);
        }
    }
}

