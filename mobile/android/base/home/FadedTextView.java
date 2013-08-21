/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Shader;
import android.util.AttributeSet;
import android.widget.TextView;

import org.mozilla.gecko.R;

/**
 * FadedTextView fades the ends of the text by fadeWidth amount,
 * if the text is too long and requires an ellipsis.
 */
public class FadedTextView extends TextView {

    // Width of the fade effect from end of the view.
    private int mFadeWidth;

    public FadedTextView(Context context) {
        this(context, null);
    }

    public FadedTextView(Context context, AttributeSet attrs) {
        this(context, attrs, android.R.attr.textViewStyle);
    }

    public FadedTextView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.FadedTextView);
        mFadeWidth = a.getDimensionPixelSize(R.styleable.FadedTextView_fadeWidth, 0);
        a.recycle();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void onDraw(Canvas canvas) {
        int width = getMeasuredWidth();

        // Layout doesn't return a proper width for getWidth().
        // Instead check the width of the first line, as we've restricted to just one line.
        if (getLayout().getLineWidth(0) > width) {
            int color = getCurrentTextColor();
            float stop = ((float) (width - mFadeWidth) / (float) width);
            LinearGradient gradient = new LinearGradient(0, 0, width, 0,
                                                         new int[] { color, color, 0x0 },
                                                         new float[] { 0, stop, 1.0f },
                                                         Shader.TileMode.CLAMP);
            getPaint().setShader(gradient);
        } else {
            getPaint().setShader(null);
        }

        // Do a default draw.
        super.onDraw(canvas);
    }
}
