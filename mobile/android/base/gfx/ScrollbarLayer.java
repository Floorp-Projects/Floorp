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

import android.graphics.PointF;
import android.graphics.RectF;
import android.opengl.GLES11;
import android.opengl.GLES11Ext;
import android.util.Log;
import java.nio.ByteBuffer;
import javax.microedition.khronos.opengles.GL10;

/**
 * Draws a small rect. This is scaled to become a scrollbar.
 */
public class ScrollbarLayer extends TileLayer {
    private static final int BAR_SIZE = 8;  // has to be power of 2

    private final boolean mVertical;

    private ScrollbarLayer(CairoImage image, boolean vertical) {
        super(false, image);
        mVertical = vertical;
    }

    public static ScrollbarLayer create(boolean vertical) {
        ByteBuffer buffer = ByteBuffer.allocateDirect(4);
        buffer.put(3, (byte)127);   // Set A to 0.5; leave R/G/B at 0.
        CairoImage image = new BufferedCairoImage(buffer, 1, 1, CairoImage.FORMAT_ARGB32);
        return new ScrollbarLayer(image, vertical);
    }

    @Override
    public void draw(RenderContext context) {
        if (!initialized())
            return;

        try {
            GLES11.glEnable(GL10.GL_BLEND);

            RectF rect = mVertical ? getVerticalRect(context) : getHorizontalRect(context);
            GLES11.glBindTexture(GL10.GL_TEXTURE_2D, getTextureID());

            float y = context.viewport.height() - rect.bottom;
            GLES11Ext.glDrawTexfOES(rect.left, y, 0.0f, rect.width(), rect.height());
        } finally {
            GLES11.glDisable(GL10.GL_BLEND);
        }
    }

    private RectF getVerticalRect(RenderContext context) {
        RectF viewport = context.viewport;
        FloatSize pageSize = context.pageSize;
        float barStart = viewport.height() * viewport.top / pageSize.height;
        float barEnd = viewport.height() * viewport.bottom / pageSize.height;
        return new RectF(viewport.width() - BAR_SIZE, barStart, viewport.width(), barEnd);
    }

    private RectF getHorizontalRect(RenderContext context) {
        RectF viewport = context.viewport;
        FloatSize pageSize = context.pageSize;
        float barStart = viewport.width() * viewport.left / pageSize.width;
        float barEnd = viewport.width() * viewport.right / pageSize.width;
        return new RectF(barStart, viewport.height() - BAR_SIZE, barEnd, viewport.height());
    }
}
