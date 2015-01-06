/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import org.mozilla.gecko.R;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Shader;
import android.util.AttributeSet;

/**
 * Fades the end of the text by gecko:fadeWidth amount,
 * if the text is too long and requires an ellipsis.
 *
 * This implementation is an improvement over Android's built-in fadingEdge
 * but potentially slower than the {@link org.mozilla.gecko.widget.FadedSingleColorTextView}.
 * It works for text of multiple colors but only one background color. It works by
 * drawing a gradient rectangle with the background color over the text, fading it out.
 */
public class FadedMultiColorTextView extends FadedTextView {
    private final ColorStateList fadeBackgroundColorList;

    private final Paint fadePaint;
    private FadedTextGradient backgroundGradient;

    public FadedMultiColorTextView(Context context, AttributeSet attrs) {
        super(context, attrs);

        fadePaint = new Paint();

        final TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.FadedMultiColorTextView);
        fadeBackgroundColorList =
                a.getColorStateList(R.styleable.FadedMultiColorTextView_fadeBackgroundColor);
        a.recycle();
    }

    @Override
    public void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        final boolean needsEllipsis = needsEllipsis();
        if (needsEllipsis) {
            final int right = getWidth() - getCompoundPaddingRight();
            final float left = right - fadeWidth;

            updateGradientShader(needsEllipsis, right);

            final float center = getHeight() / 2;

            // Shrink height of gradient to prevent it overlaying parent view border.
            final float top = center - getTextSize() + 1;
            final float bottom = center + getTextSize() - 1;

            canvas.drawRect(left, top, right, bottom, fadePaint);
        }
    }

    private void updateGradientShader(final boolean needsEllipsis, final int gradientEndRight) {
        final int backgroundColor =
                fadeBackgroundColorList.getColorForState(getDrawableState(), Color.RED);

        final boolean needsNewGradient = (backgroundGradient == null ||
                                          backgroundGradient.getBackgroundColor() != backgroundColor ||
                                          backgroundGradient.getEndRight() != gradientEndRight);

        if (needsEllipsis && needsNewGradient) {
            backgroundGradient = new FadedTextGradient(gradientEndRight, fadeWidth, backgroundColor);
            fadePaint.setShader(backgroundGradient);
        }
    }

    private static class FadedTextGradient extends LinearGradient {
        private final int endRight;
        private final int backgroundColor;

        public FadedTextGradient(final int gradientEndRight, final int fadeWidth,
                final int backgroundColor) {
            super(gradientEndRight - fadeWidth, 0, gradientEndRight, 0,
                  getColorWithZeroedAlpha(backgroundColor), backgroundColor, Shader.TileMode.CLAMP);

            this.endRight = gradientEndRight;
            this.backgroundColor = backgroundColor;
        }

        private static int getColorWithZeroedAlpha(final int color) {
            return Color.argb(0, Color.red(color), Color.green(color), Color.blue(color));
        }

        public int getEndRight() {
            return endRight;
        }

        public int getBackgroundColor() {
            return backgroundColor;
        }
    }
}
