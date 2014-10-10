/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapShader;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.ComposeShader;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.Rect;
import android.graphics.Shader;
import android.graphics.drawable.Drawable;

/**
 * A special drawable used with lightweight themes. This draws a color 
 * (with an optional color-filter) and a bitmap (with a linear gradient 
 * to specify the alpha) in order.
 */
public class LightweightThemeDrawable extends Drawable {
    private Paint mPaint;
    private Paint mColorPaint;

    private Bitmap mBitmap;
    private Resources mResources;

    private int mStartColor;
    private int mEndColor;

    public LightweightThemeDrawable(Resources resources, Bitmap bitmap) {
        mBitmap = bitmap;
        mResources = resources;

        mPaint = new Paint();
        mPaint.setAntiAlias(true);
        mPaint.setStrokeWidth(0.0f);
    }

    @Override
    protected void onBoundsChange(Rect bounds) {
        super.onBoundsChange(bounds);
        initializeBitmapShader();
    }

    @Override
    public void draw(Canvas canvas) {
        // Draw the colors, if available.
        if (mColorPaint != null) {
            canvas.drawPaint(mColorPaint);
        }

        // Draw the bitmap.
        canvas.drawPaint(mPaint);
    }

    @Override
    public int getOpacity() {
        return PixelFormat.TRANSLUCENT;
    }

    @Override
    public void setAlpha(int alpha) {
        // A StateListDrawable will reset the alpha value with 255.
        // We cannot use to be the bitmap alpha.
        mPaint.setAlpha(alpha);
    }

    @Override
    public void setColorFilter(ColorFilter filter) {
        mPaint.setColorFilter(filter);
    }

    /**
     * Creates a paint that paint a particular color.
     *
     * @param color The color to be painted.
     */
    public void setColor(int color) {
        mColorPaint = new Paint(mPaint);
        mColorPaint.setColor(color);
    }

    /**
     * Creates a paint that paint a particular color, and a filter for the color.
     *
     * @param color The color to be painted.
     * @param filter The filter color to be applied using SRC_OVER mode.
     */
    public void setColorWithFilter(int color, int filter) {
        mColorPaint = new Paint(mPaint);
        mColorPaint.setColor(color);
        mColorPaint.setColorFilter(new PorterDuffColorFilter(filter, PorterDuff.Mode.SRC_OVER));
    }

    /**
     * Set the alpha for the linear gradient used with the bitmap's shader.
     *
     * @param startAlpha The starting alpha (0..255) value to be applied to the LinearGradient.
     * @param startAlpha The ending alpha (0..255) value to be applied to the LinearGradient.
     */
    public void setAlpha(int startAlpha, int endAlpha) {
        mStartColor = startAlpha << 24;
        mEndColor = endAlpha << 24;
        initializeBitmapShader();
    }

    private void initializeBitmapShader() {
        // A bitmap-shader to draw the bitmap.
        // Clamp mode will repeat the last row of pixels.
        // Hence its better to have an endAlpha of 0 for the linear-gradient.
        BitmapShader bitmapShader = new BitmapShader(mBitmap, Shader.TileMode.CLAMP, Shader.TileMode.CLAMP);

        // A linear-gradient to specify the opacity of the bitmap.
        LinearGradient gradient = new LinearGradient(0, 0, 0, mBitmap.getHeight(), mStartColor, mEndColor, Shader.TileMode.CLAMP);

        // Make a combined shader -- a performance win.
        // The linear-gradient is the 'SRC' and the bitmap-shader is the 'DST'.
        // Drawing the DST in the SRC will provide the opacity.
        mPaint.setShader(new ComposeShader(bitmapShader, gradient, PorterDuff.Mode.DST_IN));
    }
}
