/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.mozglue.DirectBufferAllocator;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;

import java.nio.ByteBuffer;

/**
 * Draws text on a layer. This is used for the frame rate meter.
 */
public class TextLayer extends SingleTileLayer {
    private final ByteBuffer mBuffer;   // this buffer is owned by the BufferedCairoImage
    private final IntSize mSize;

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

    public static TextLayer create(IntSize size, String text) {
        ByteBuffer buffer = DirectBufferAllocator.allocate(size.width * size.height * 4);
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

