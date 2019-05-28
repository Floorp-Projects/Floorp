/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.widget;

import android.content.Context;
import androidx.coordinatorlayout.widget.CoordinatorLayout;
import android.util.AttributeSet;

/**
 * A CoordinatorLayout implementation that resizes dynamically based on whether a keyboard is visible or not.
 */
public class ResizableKeyboardCoordinatorLayout extends CoordinatorLayout {
    private final ResizableKeyboardViewDelegate delegate;

    public ResizableKeyboardCoordinatorLayout(Context context) {
        this(context, null);
    }

    public ResizableKeyboardCoordinatorLayout(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public ResizableKeyboardCoordinatorLayout(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);

        delegate = new ResizableKeyboardViewDelegate(this, attrs);
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();

        delegate.onAttachedToWindow();
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();

        delegate.onDetachedFromWindow();
    }
}
