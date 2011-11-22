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

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import javax.microedition.khronos.opengles.GL10;

/**
 * Draws a small rect. This is scaled to become a scrollbar.
 */
public class ScrollbarLayer extends SingleTileLayer {
    private static final int BAR_SIZE = 8;  // has to be power of 2

    /*
     * This awkward pattern is necessary due to Java's restrictions on when one can call superclass
     * constructors.
     */
    private ScrollbarLayer(CairoImage image) {
        super(image);
    }

    public static ScrollbarLayer create() {
        Bitmap bitmap = Bitmap.createBitmap(BAR_SIZE, BAR_SIZE, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);

        Paint painter = new Paint();
        painter.setColor(Color.argb(127, 0, 0, 0));
        canvas.drawRect(0.0f, 0.0f, BAR_SIZE, BAR_SIZE, painter);

        return new ScrollbarLayer(new BufferedCairoImage(bitmap));
    }

    void drawVertical(GL10 gl, IntSize screenSize, Rect pageRect) {
        float barStart = (float)screenSize.height * (float)(0 - pageRect.top) / pageRect.height();
        float barEnd = (float)screenSize.height * (float)(screenSize.height - pageRect.top) / pageRect.height();
        float scale = Math.max(1.0f, (barEnd - barStart) / BAR_SIZE);
        gl.glLoadIdentity();
        gl.glScalef(1.0f, scale, 1.0f);
        gl.glTranslatef(screenSize.width - BAR_SIZE, barStart / scale, 0.0f);
        draw(gl);
    }

    void drawHorizontal(GL10 gl, IntSize screenSize, Rect pageRect) {
        float barStart = (float)screenSize.width * (float)(0 - pageRect.left) / pageRect.width();
        float barEnd = (float)screenSize.width * (float)(screenSize.width - pageRect.left) / pageRect.width();
        float scale = Math.max(1.0f, (barEnd - barStart) / BAR_SIZE);
        gl.glLoadIdentity();
        gl.glScalef(scale, 1.0f, 1.0f);
        gl.glTranslatef(barStart / scale, screenSize.height - BAR_SIZE, 0.0f);
        draw(gl);
    }
}
