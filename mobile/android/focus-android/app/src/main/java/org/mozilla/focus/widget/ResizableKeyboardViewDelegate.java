/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.widget;


import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.content.res.TypedArray;
import android.graphics.Rect;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewTreeObserver;

import org.mozilla.focus.R;

/**
 * A helper class to implement a ViewGroup that resizes dynamically (by adding padding to the bottom)
 * based on whether a keyboard is visible or not.
 *
 * Implementation based on:
 * https://github.com/mikepenz/MaterialDrawer/blob/master/library/src/main/java/com/mikepenz/materialdrawer/util/KeyboardUtil.java
 *
 * An optional viewToHideWhenActivated can be set: this is a View that will be hidden when the keyboard
 * is showing. That can be useful for things like FABs that you don't need when someone is typing.
 *
 * A View using this delegate needs to forward the calls to onAttachedToWindow() and onDetachedFromWindow()
 * to this class.
 */
/* package */ class ResizableKeyboardViewDelegate {
    private final Rect rect;
    private final View delegateView;
    private View decorView;

    private int idOfViewToHide;
    private @Nullable View viewToHide;
    private boolean shouldAnimate;
    private boolean isAnimating;

    private ViewTreeObserver.OnGlobalLayoutListener layoutListener = new ViewTreeObserver.OnGlobalLayoutListener() {
        @Override
        public void onGlobalLayout() {
            if (isAnimating) {
                return;
            }

            int difference = calculateDifferenceBetweenHeightAndUsableArea();

            // If difference > 0, keyboard is showing.
            // If difference =< 0, keyboard is not showing or is in multiview mode.
            if (difference > 0) {
                // Keyboard showing -> Set difference has bottom padding.
                if (delegateView.getPaddingBottom() != difference) {
                    updateBottomPadding(difference);

                    if (viewToHide != null) {
                        viewToHide.setVisibility(View.GONE);
                    }
                }
            } else {
                // Keyboard not showing -> Reset bottom padding.
                if (delegateView.getPaddingBottom() != 0) {
                    updateBottomPadding(0);

                    if (viewToHide != null) {
                        viewToHide.setVisibility(View.VISIBLE);
                    }
                }
            }
        }
    };

    /* package */ ResizableKeyboardViewDelegate(@NonNull View view, @NonNull AttributeSet attrs) {
        this.delegateView = view;
        this.rect = new Rect();

        final TypedArray styleAttributeArray = view.getContext().getTheme().obtainStyledAttributes(
                attrs,
                R.styleable.ResizableKeyboardViewDelegate,
                0, 0);

        try {
            idOfViewToHide = styleAttributeArray.getResourceId(R.styleable.ResizableKeyboardViewDelegate_viewToHideWhenActivated, -1);
            shouldAnimate = styleAttributeArray.getBoolean(R.styleable.ResizableKeyboardViewDelegate_animate, false);
        } finally {
            styleAttributeArray.recycle();
        }
    }

    /* package */ void onAttachedToWindow() {
        delegateView.getViewTreeObserver().addOnGlobalLayoutListener(layoutListener);

        if (idOfViewToHide != -1) {
            viewToHide = delegateView.findViewById(idOfViewToHide);
        }
    }

    /* package */ void onDetachedFromWindow() {
        delegateView.getViewTreeObserver().removeOnGlobalLayoutListener(layoutListener);

        viewToHide = null;
    }

    /* package */ void reset() {
        updateBottomPadding(0);
    }

    private void updateBottomPadding(int value) {
        if (shouldAnimate) {
            animateBottomPaddingTo(value);
        } else {
            delegateView.setPadding(0, 0, 0, value);
        }
    }

    private void animateBottomPaddingTo(int value) {
        isAnimating = true;

        final ValueAnimator animator = ValueAnimator.ofInt(delegateView.getPaddingBottom(), value);
        animator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator animation) {
                delegateView.setPadding(0, 0, 0, (int) animation.getAnimatedValue());
            }
        });
        animator.setDuration(200);
        animator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                isAnimating = false;
            }
        });
        animator.start();
    }


    private int calculateDifferenceBetweenHeightAndUsableArea() {
        if (decorView == null) {
            decorView = delegateView.getRootView();
        }

        decorView.getWindowVisibleDisplayFrame(rect);

        return delegateView.getResources().getDisplayMetrics().heightPixels - rect.bottom;
    }
}
