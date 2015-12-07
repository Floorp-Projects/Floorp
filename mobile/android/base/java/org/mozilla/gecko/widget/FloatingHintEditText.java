/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Paint.FontMetricsInt;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.widget.EditText;

import org.mozilla.gecko.R;

public class FloatingHintEditText extends EditText {
    private static enum Animation { NONE, SHRINK, GROW }
    private static final float HINT_SCALE = 0.6f;
    private static final int ANIMATION_STEPS = 6;

    private final Paint floatingHintPaint = new Paint();
    private final ColorStateList floatingHintColors;
    private final ColorStateList normalHintColors;
    private final int defaultFloatingHintColor;
    private final int defaultNormalHintColor;

    private boolean wasEmpty;

    private int animationFrame;
    private Animation animation = Animation.NONE;

    public FloatingHintEditText(Context context) {
        this(context, null);
    }

    public FloatingHintEditText(Context context, AttributeSet attrs) {
        this(context, attrs, R.attr.floatingHintEditTextStyle);
    }

    public FloatingHintEditText(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        floatingHintColors = getResources().getColorStateList(R.color.floating_hint_text);
        normalHintColors = getHintTextColors();
        defaultFloatingHintColor = floatingHintColors.getDefaultColor();
        defaultNormalHintColor = normalHintColors.getDefaultColor();
        wasEmpty = TextUtils.isEmpty(getText());
    }

    @Override
    public int getCompoundPaddingTop() {
        final FontMetricsInt metrics = getPaint().getFontMetricsInt();
        final int floatingHintHeight = (int) ((metrics.bottom - metrics.top) * HINT_SCALE);
        return super.getCompoundPaddingTop() + floatingHintHeight;
    }

    @Override
    protected void onTextChanged(CharSequence text, int start, int lengthBefore, int lengthAfter) {
        super.onTextChanged(text, start, lengthBefore, lengthAfter);

        final boolean isEmpty = TextUtils.isEmpty(getText());

        // The empty state hasn't changed, so the hint stays the same.
        if (wasEmpty == isEmpty) {
            return;
        }

        wasEmpty = isEmpty;

        // Don't animate if we aren't visible.
        if (!isShown()) {
            return;
        }

        if (isEmpty) {
            animation = Animation.GROW;

            // The TextView will show a hint since the field is empty, but since we're animating
            // from the floating hint, we don't want the normal hint to appear yet. We set it to
            // transparent here, then restore the hint color after the animation has finished.
            setHintTextColor(Color.TRANSPARENT);
        } else {
            animation = Animation.SHRINK;
        }
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        if (TextUtils.isEmpty(getHint())) {
            return;
        }

        final boolean isAnimating = (animation != Animation.NONE);

        // The large hint is drawn by Android, so do nothing.
        if (!isAnimating && TextUtils.isEmpty(getText())) {
            return;
        }

        final Paint paint = getPaint();
        final float hintPosX = getCompoundPaddingLeft() + getScrollX();
        final float normalHintPosY = getBaseline();
        final float floatingHintPosY = normalHintPosY + paint.getFontMetricsInt().top + getScrollY();
        final float normalHintSize = getTextSize();
        final float floatingHintSize = normalHintSize * HINT_SCALE;
        final int[] stateSet = getDrawableState();
        final int floatingHintColor = floatingHintColors.getColorForState(stateSet, defaultFloatingHintColor);

        floatingHintPaint.set(paint);

        // If we're not animating, we're showing the floating hint, so draw it and bail.
        if (!isAnimating) {
            drawHint(canvas, floatingHintSize, floatingHintColor, hintPosX, floatingHintPosY);
            return;
        }

        // We are animating, so draw the linearly interpolated frame.
        final int normalHintColor = normalHintColors.getColorForState(stateSet, defaultNormalHintColor);
        if (animation == Animation.SHRINK) {
            drawAnimationFrame(canvas, normalHintSize, floatingHintSize,
                    hintPosX, normalHintPosY, floatingHintPosY, normalHintColor, floatingHintColor);
        } else {
            drawAnimationFrame(canvas, floatingHintSize, normalHintSize,
                    hintPosX, floatingHintPosY, normalHintPosY, floatingHintColor, normalHintColor);
        }

        animationFrame++;

        if (animationFrame == ANIMATION_STEPS) {
            // After the grow animation has finished, restore the normal TextView hint color that we
            // removed in our onTextChanged listener.
            if (animation == Animation.GROW) {
                setHintTextColor(normalHintColors);
            }

            animation = Animation.NONE;
            animationFrame = 0;
        }

        invalidate();
    }

    private void drawAnimationFrame(Canvas canvas, float fromSize, float toSize,
                                    float hintPosX, float fromY, float toY, int fromColor, int toColor) {
        final float textSize = lerp(fromSize, toSize);
        final float hintPosY = lerp(fromY, toY);
        final int color = Color.rgb((int) lerp(Color.red(fromColor), Color.red(toColor)),
                                    (int) lerp(Color.green(fromColor), Color.green(toColor)),
                                    (int) lerp(Color.blue(fromColor), Color.blue(toColor)));
        drawHint(canvas, textSize, color, hintPosX, hintPosY);
    }

    private void drawHint(Canvas canvas, float textSize, int color, float x, float y) {
        floatingHintPaint.setTextSize(textSize);
        floatingHintPaint.setColor(color);
        canvas.drawText(getHint().toString(), x, y, floatingHintPaint);
    }

    private float lerp(float from, float to) {
        final float alpha = (float) animationFrame / (ANIMATION_STEPS - 1);
        return from * (1 - alpha) + to * alpha;
    }
}
