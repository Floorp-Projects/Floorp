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
import android.graphics.PointF;
import android.graphics.PorterDuff;
import android.graphics.Rect;
import android.graphics.RectF;
import android.opengl.GLES11;
import android.opengl.GLES11Ext;
import android.util.Log;
import java.nio.ByteBuffer;
import javax.microedition.khronos.opengles.GL10;
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

    private static final int[] CROPRECT_MIDPIXEL = new int[] { CAP_RADIUS, CAP_RADIUS, 1, 1 };
    private static final int[] CROPRECT_TOPCAP = new int[] { 0, CAP_RADIUS, BAR_SIZE, -CAP_RADIUS };
    private static final int[] CROPRECT_BOTTOMCAP = new int[] { 0, BAR_SIZE, BAR_SIZE, -CAP_RADIUS };
    private static final int[] CROPRECT_LEFTCAP = new int[] { 0, BAR_SIZE, CAP_RADIUS, -BAR_SIZE };
    private static final int[] CROPRECT_RIGHTCAP = new int[] { CAP_RADIUS, BAR_SIZE, CAP_RADIUS, -BAR_SIZE };

    private final boolean mVertical;
    private final ByteBuffer mBuffer;
    private final Bitmap mBitmap;
    private final Canvas mCanvas;
    private float mOpacity;
    private boolean mFinalized = false;

    private ScrollbarLayer(CairoImage image, boolean vertical, ByteBuffer buffer) {
        super(false, image);
        mVertical = vertical;
        mBuffer = buffer;

        IntSize size = image.getSize();
        mBitmap = Bitmap.createBitmap(size.width, size.height, Bitmap.Config.ARGB_8888);
        mCanvas = new Canvas(mBitmap);
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


    public static ScrollbarLayer create(boolean vertical) {
        // just create an empty image for now, it will get drawn
        // on demand anyway
        int imageSize = IntSize.nextPowerOfTwo(BAR_SIZE);
        ByteBuffer buffer = GeckoAppShell.allocateDirectBuffer(imageSize * imageSize * 4);
        CairoImage image = new BufferedCairoImage(buffer, imageSize, imageSize, CairoImage.FORMAT_ARGB32);
        return new ScrollbarLayer(image, vertical, buffer);
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
        beginTransaction();
        try {
            setOpacity(Math.max(mOpacity - FADE_AMOUNT, 0.0f));
            invalidate();
        } finally {
            endTransaction();
        }
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
        beginTransaction();
        try {
            setOpacity(1.0f);
            invalidate();
        } finally {
            endTransaction();
        }
        return true;
    }

    private void setOpacity(float opacity) {
        mOpacity = opacity;

        Paint foregroundPaint = new Paint();
        foregroundPaint.setAntiAlias(true);
        foregroundPaint.setStyle(Paint.Style.FILL);
        // use a (a,r,g,b) color of (127,0,0,0), and multiply the alpha by mOpacity for fading
        foregroundPaint.setColor(Color.argb((int)Math.round(mOpacity * 127), 0, 0, 0));

        mCanvas.drawColor(Color.argb(0, 0, 0, 0), PorterDuff.Mode.CLEAR);
        mCanvas.drawCircle(CAP_RADIUS, CAP_RADIUS, CAP_RADIUS, foregroundPaint);

        mBitmap.copyPixelsToBuffer(mBuffer.asIntBuffer());
    }

    @Override
    public void draw(RenderContext context) {
        if (!initialized())
            return;

        try {
            GLES11.glEnable(GL10.GL_BLEND);

            Rect rect = RectUtils.round(mVertical ? getVerticalRect(context) : getHorizontalRect(context));
            GLES11.glBindTexture(GL10.GL_TEXTURE_2D, getTextureID());

            float viewHeight = context.viewport.height();

            // for the body of the scrollbar, we take a 1x1 pixel from the center of the image
            // and scale it to become the bar
            GLES11.glTexParameteriv(GL10.GL_TEXTURE_2D, GLES11Ext.GL_TEXTURE_CROP_RECT_OES, CROPRECT_MIDPIXEL, 0);
            GLES11Ext.glDrawTexfOES(rect.left, viewHeight - rect.bottom, 0.0f, rect.width(), rect.height());

            if (mVertical) {
                // top endcap
                GLES11.glTexParameteriv(GL10.GL_TEXTURE_2D, GLES11Ext.GL_TEXTURE_CROP_RECT_OES, CROPRECT_TOPCAP, 0);
                GLES11Ext.glDrawTexfOES(rect.left, viewHeight - rect.top, 0.0f, BAR_SIZE, CAP_RADIUS);
                // bottom endcap
                GLES11.glTexParameteriv(GL10.GL_TEXTURE_2D, GLES11Ext.GL_TEXTURE_CROP_RECT_OES, CROPRECT_BOTTOMCAP, 0);
                GLES11Ext.glDrawTexfOES(rect.left, viewHeight - (rect.bottom + CAP_RADIUS), 0.0f, BAR_SIZE, CAP_RADIUS);
            } else {
                // left endcap
                GLES11.glTexParameteriv(GL10.GL_TEXTURE_2D, GLES11Ext.GL_TEXTURE_CROP_RECT_OES, CROPRECT_LEFTCAP, 0);
                GLES11Ext.glDrawTexfOES(rect.left - CAP_RADIUS, viewHeight - rect.bottom, 0.0f, CAP_RADIUS, BAR_SIZE);
                // right endcap
                GLES11.glTexParameteriv(GL10.GL_TEXTURE_2D, GLES11Ext.GL_TEXTURE_CROP_RECT_OES, CROPRECT_RIGHTCAP, 0);
                GLES11Ext.glDrawTexfOES(rect.right, viewHeight - rect.bottom, 0.0f, CAP_RADIUS, BAR_SIZE);
            }
        } finally {
            GLES11.glDisable(GL10.GL_BLEND);
        }
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
