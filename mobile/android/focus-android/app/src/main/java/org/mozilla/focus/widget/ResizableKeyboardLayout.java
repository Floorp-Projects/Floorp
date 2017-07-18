/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Rect;
import android.support.annotation.Nullable;
import android.support.design.widget.CoordinatorLayout;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewTreeObserver;

import org.mozilla.focus.R;

/**
 * A CoordinatorLayout implementation that resizes dynamically (by adding padding to the bottom)
 * based on whether a keyboard is visible or not.
 *
 * Implementation based on:
 * https://github.com/mikepenz/MaterialDrawer/blob/master/library/src/main/java/com/mikepenz/materialdrawer/util/KeyboardUtil.java
 *
 * An optional viewToHideWhenActivated can be set: this is a View that will be hidden when the keyboard
 * is showing. That can be useful for things like FABs that you don't need when someone is typing.
 */
public class ResizableKeyboardLayout extends CoordinatorLayout {
    private final Rect rect;
    private View decorView;

    private final int idOfViewToHide;
    private @Nullable View viewToHide;

    public ResizableKeyboardLayout(Context context) {
        this(context, null);
    }

    public ResizableKeyboardLayout(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public ResizableKeyboardLayout(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);

        rect = new Rect();

        final TypedArray styleAttributeArray = getContext().getTheme().obtainStyledAttributes(
                attrs,
                R.styleable.ResizableKeyboardLayout,
                0, 0);

        try {
            idOfViewToHide = styleAttributeArray.getResourceId(R.styleable.ResizableKeyboardLayout_viewToHideWhenActivated, -1);
        } finally {
            styleAttributeArray.recycle();
        }
    }

    private ViewTreeObserver.OnGlobalLayoutListener layoutListener = new ViewTreeObserver.OnGlobalLayoutListener() {
        @Override
        public void onGlobalLayout() {
            int difference = calculateDifferenceBetweenHeightAndUsableArea();

            // If difference > 0, keyboard is showing. 
            // If difference =< 0, keyboard is not showing or is in multiview mode
            if (difference > 0) {
                // Keyboard showing -> Set difference has bottom padding.
                if (getPaddingBottom() != difference) {
                    setPadding(0, 0, 0, difference);

                    if (viewToHide != null) {
                        viewToHide.setVisibility(View.GONE);
                    }
                }
            } else {
                // Keyboard not showing -> Reset bottom padding.
                if (getPaddingBottom() != 0) {
                    setPadding(0, 0, 0, 0);

                    if (viewToHide != null) {
                        viewToHide.setVisibility(View.VISIBLE);
                    }
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

        if (idOfViewToHide != -1) {
            viewToHide = findViewById(idOfViewToHide);
        }
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();

        getViewTreeObserver().removeOnGlobalLayoutListener(layoutListener);

        viewToHide = null;
    }
}
