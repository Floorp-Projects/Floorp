/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import org.mozilla.gecko.R;

import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.widget.TextView;

/**
 * Text view that correctly handles maxLines and ellipsizing for Android < 2.3.
 */
public class EllipsisTextView extends TextView {
    private final String ellipsis;

    private final int maxLines;
    private CharSequence originalText;

    public EllipsisTextView(Context context) {
        this(context, null);
    }

    public EllipsisTextView(Context context, AttributeSet attrs) {
        this(context, attrs, android.R.attr.textViewStyle);
    }

    public EllipsisTextView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        ellipsis = getResources().getString(R.string.ellipsis);

        TypedArray a = context.getTheme()
            .obtainStyledAttributes(attrs, R.styleable.EllipsisTextView, 0, 0);
        maxLines = a.getInteger(R.styleable.EllipsisTextView_ellipsizeAtLine, 1);
        a.recycle();
    }

    public void setOriginalText(CharSequence text) {
        originalText = text;
        setText(text);
    }

    @Override
    public void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);

        // There is extra space, start over with the original text
        if (getLineCount() < maxLines) {
            setText(originalText);
        }

        // If we are over the max line attribute, ellipsize
        if (getLineCount() > maxLines) {
            final int endIndex = getLayout().getLineEnd(maxLines - 1) - 1 - ellipsis.length();
            final String text = getText().subSequence(0, endIndex) + ellipsis;
            // Make sure that we don't change originalText
            setText(text);
        }
    }
}
