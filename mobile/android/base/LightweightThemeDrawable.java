/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.LinearGradient;
import android.graphics.Path;
import android.graphics.PorterDuff.Mode;
import android.graphics.Rect;
import android.graphics.Shader;

public class LightweightThemeDrawable extends BitmapDrawable
                                      implements CanvasDelegate.DrawManager { 
    private static final String LOGTAG = "GeckoLightweightThemeDrawable";
    private Path mPath;
    private CanvasDelegate mCanvasDelegate;
    private Bitmap mBitmap;
    private int mStartColor;
    private int mEndColor;

    public LightweightThemeDrawable(Resources resources, Bitmap bitmap) {
        super(resources, bitmap);
        mBitmap = bitmap;

        mPath = new Path();
        mCanvasDelegate = new CanvasDelegate(this, Mode.DST_IN);
    }

    public void setAlpha(int startAlpha, int endAlpha) {
        mStartColor = startAlpha << 24;
        mEndColor = endAlpha << 24;
    }

    @Override
    protected void onBoundsChange(Rect bounds) {
        mCanvasDelegate.setShader(new LinearGradient(0, 0, 
                                                     0, mBitmap.getHeight(), 
                                                     mStartColor, mEndColor,
                                                     Shader.TileMode.CLAMP));

        mPath.addRect(0, 0, mBitmap.getWidth(), mBitmap.getHeight(), Path.Direction.CW);
    }

    @Override
    public void draw(Canvas canvas) {
        mCanvasDelegate.draw(canvas, mPath, canvas.getWidth(), canvas.getHeight());
    }

    @Override
    public void defaultDraw(Canvas canvas) {
        super.draw(canvas);
    }
}
