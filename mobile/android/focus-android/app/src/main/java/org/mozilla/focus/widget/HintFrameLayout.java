/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.widget;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.support.annotation.Keep;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.widget.FrameLayout;

import org.mozilla.focus.R;

/**
 * This FrameLayout takes over drawing the "hint" of a InlineAutocompleteEditText child.
 * Additionally it allows animating the hint between center (animationOffset = 1.0) and left
 * aligned (animationOffset = 0.0).
 */
public class HintFrameLayout extends FrameLayout implements TextWatcher {
    private final Rect bounds;
    private final Paint paint;

    private String hint;
    private boolean drawHint;
    private float padding;
    private float animationOffset;

    public HintFrameLayout(Context context, AttributeSet attrs) {
        super(context, attrs);

        this.bounds = new Rect();
        this.paint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.LINEAR_TEXT_FLAG);

        setWillNotDraw(false);
    }

    @Keep
    public float getAnimationOffset() {
        return animationOffset;
    }

    @Keep
    public void setAnimationOffset(float animationOffset) {
        this.animationOffset = animationOffset;
        invalidate();
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        final InlineAutocompleteEditText editText = (InlineAutocompleteEditText) findViewById(R.id.url_edit);

        final CharSequence editTextHint = editText.getHint();
        if (TextUtils.isEmpty(editTextHint)) {
            return;
        }

        editText.addTextChangedListener(this);

        this.padding = editText.getPaddingStart();
        this.hint = editTextHint.toString();
        this.drawHint = TextUtils.isEmpty(editText.getText().toString());

        editText.setHint(null);

        paint.setColor(editText.getHintTextColors().getDefaultColor());
        paint.setTextSize(editText.getTextSize());
        paint.setTypeface(editText.getTypeface());
        paint.setTextScaleX(editText.getTextScaleX());

        paint.getTextBounds(this.hint, 0, this.hint.length(), bounds);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        if (!drawHint || TextUtils.isEmpty(hint)) {
            return;
        }

        final float y = ((float) canvas.getHeight() + bounds.height()) / 2;

        final float max = ((float) canvas.getWidth() - bounds.width()) / 2;
        final float min = padding;

        final float x = (max - min) * animationOffset + min;

        canvas.drawText(hint, x, y, paint);
    }

    @Override
    public void afterTextChanged(Editable s) {
        drawHint = s.length() == 0;
        invalidate();
    }

    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

    @Override
    public void onTextChanged(CharSequence s, int start, int before, int count) {}
}
