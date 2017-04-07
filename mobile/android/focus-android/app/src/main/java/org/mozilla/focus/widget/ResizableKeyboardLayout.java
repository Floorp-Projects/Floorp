/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.widget;

import android.content.Context;
import android.graphics.Rect;
import android.support.design.widget.CoordinatorLayout;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewTreeObserver;

/**
 * A CoordinatorLayout implementation that resizes dynamically (by adding padding to the bottom)
 * based on whether a keyboard is visible or not.
 *
 * Implementation based on:
 * https://github.com/mikepenz/MaterialDrawer/blob/master/library/src/main/java/com/mikepenz/materialdrawer/util/KeyboardUtil.java
 */
public class ResizableKeyboardLayout extends CoordinatorLayout {
    private Rect rect;
    private View decorView;

    public ResizableKeyboardLayout(Context context) {
        super(context);
        init();
    }

    public ResizableKeyboardLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public ResizableKeyboardLayout(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    private void init() {
        rect = new Rect();
    }

    private ViewTreeObserver.OnGlobalLayoutListener layoutListener = new ViewTreeObserver.OnGlobalLayoutListener() {
        @Override
        public void onGlobalLayout() {
            int difference = calculateDifferenceBetweenHeightAndUsableArea();

            if (difference != 0) {
                // Keyboard showing -> Set difference has bottom padding.
                if (getPaddingBottom() != difference) {
                    setPadding(0, 0, 0, difference);
                }
            } else {
                // Keyboard not showing -> Reset bottom padding.
                if (getPaddingBottom() != 0) {
                    setPadding(0, 0, 0, 0);
                }
            }
        }
    };

    private int calculateDifferenceBetweenHeightAndUsableArea() {
        if (decorView == null) {
            decorView = getRootView();
        }

        decorView.getWindowVisibleDisplayFrame(rect);

        return getResources().getDisplayMetrics().heightPixels - rect.bottom;
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();

        getViewTreeObserver().addOnGlobalLayoutListener(layoutListener);
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();

        getViewTreeObserver().removeOnGlobalLayoutListener(layoutListener);
    }
}
