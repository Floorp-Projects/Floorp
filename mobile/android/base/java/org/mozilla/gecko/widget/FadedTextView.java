/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.text.Layout;
import android.util.AttributeSet;

import org.mozilla.gecko.R;
import org.mozilla.gecko.widget.themed.ThemedTextView;

/**
 * An implementation of FadedTextView should fade the end of the text
 * by gecko:fadeWidth amount, if the text is too long and requires an ellipsis.
 */
public abstract class FadedTextView extends ThemedTextView {
    // Width of the fade effect from end of the view.
    protected final int fadeWidth;

    public FadedTextView(final Context context, final AttributeSet attrs) {
        super(context, attrs);

        setSingleLine(true);
        setEllipsize(null);

        final TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.FadedTextView);
        fadeWidth = a.getDimensionPixelSize(R.styleable.FadedTextView_fadeWidth, 0);
        a.recycle();
    }

    protected int getAvailableWidth() {
        return getWidth() - getCompoundPaddingLeft() - getCompoundPaddingRight();
    }

    protected boolean needsEllipsis() {
        final int width = getAvailableWidth();
        if (width <= 0) {
            return false;
        }

        final Layout layout = getLayout();
        return (layout != null && layout.getLineWidth(0) > width);
    }
}
