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

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.gfx.BufferedCairoImage;
import org.mozilla.gecko.gfx.CairoImage;
import org.mozilla.gecko.gfx.IntSize;
import org.mozilla.gecko.gfx.SingleTileLayer;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.util.Log;
import java.nio.ByteBuffer;
import java.nio.IntBuffer;

/**
 * Draws text on a layer. This is used for the frame rate meter.
 */
public class TextLayer extends SingleTileLayer {
    private final ByteBuffer mBuffer;
    private final IntSize mSize;
    private boolean mFinalized = false;

    /*
     * This awkward pattern is necessary due to Java's restrictions on when one can call superclass
     * constructors.
     */
    private TextLayer(ByteBuffer buffer, BufferedCairoImage image, IntSize size, String text) {
        super(false, image);
        mBuffer = buffer;
        mSize = size;
        renderText(text);
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

    public static TextLayer create(IntSize size, String text) {
        ByteBuffer buffer = GeckoAppShell.allocateDirectBuffer(size.width * size.height * 4);
        BufferedCairoImage image = new BufferedCairoImage(buffer, size.width, size.height,
                                                          CairoImage.FORMAT_ARGB32);
        return new TextLayer(buffer, image, size, text);
    }

    public void setText(String text) {
        renderText(text);
        invalidate();
    }

    private void renderText(String text) {
        Bitmap bitmap = Bitmap.createBitmap(mSize.width, mSize.height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);

        Paint textPaint = new Paint();
        textPaint.setAntiAlias(true);
        textPaint.setColor(Color.WHITE);
        textPaint.setFakeBoldText(true);
        textPaint.setTextSize(18.0f);
        textPaint.setTypeface(Typeface.DEFAULT_BOLD);
        float width = textPaint.measureText(text) + 18.0f;

        Paint backgroundPaint = new Paint();
        backgroundPaint.setColor(Color.argb(127, 0, 0, 0));
        canvas.drawRect(0.0f, 0.0f, width, 18.0f + 6.0f, backgroundPaint);

        canvas.drawText(text, 6.0f, 18.0f, textPaint);

        bitmap.copyPixelsToBuffer(mBuffer.asIntBuffer());
    }
}

