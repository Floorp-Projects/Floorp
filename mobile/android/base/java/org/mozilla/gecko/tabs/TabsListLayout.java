/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import org.mozilla.gecko.R;
import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.Context;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.helper.ItemTouchHelper;
import android.util.AttributeSet;
import android.view.View;

public class TabsListLayout extends TabsLayout {
    // Time to animate non-flinged tabs of screen, in milliseconds
    private static final int ANIMATION_DURATION = 250;

    // Time between starting successive tab animations in closeAllTabs.
    private static final int ANIMATION_CASCADE_DELAY = 75;

    private int closeAllAnimationCount;

    public TabsListLayout(Context context, AttributeSet attrs) {
        super(context, attrs, R.layout.tabs_list_item_view);

        setHasFixedSize(true);

        setLayoutManager(new LinearLayoutManager(context));

        // A TouchHelper handler for swipe to close.
        final TabsTouchHelperCallback callback = new TabsTouchHelperCallback(this);
        final ItemTouchHelper touchHelper = new ItemTouchHelper(callback);
        touchHelper.attachToRecyclerView(this);

        setItemAnimator(new TabsListLayoutAnimator(ANIMATION_DURATION));
    }

    @Override
    public void closeAll() {
        final int childCount = getChildCount();

        // Just close the panel if there are no tabs to close.
        if (childCount == 0) {
            autoHidePanel();
            return;
        }

        // Disable the view so that gestures won't interfere wth the tab close animation.
        setEnabled(false);

        // Delay starting each successive animation to create a cascade effect.
        int cascadeDelay = 0;
        closeAllAnimationCount = 0;
        for (int i = childCount - 1; i >= 0; i--) {
            final View view = getChildAt(i);
            if (view == null) {
                continue;
            }

            final PropertyAnimator animator = new PropertyAnimator(ANIMATION_DURATION);
            animator.attach(view, PropertyAnimator.Property.ALPHA, 0);

            animator.attach(view, PropertyAnimator.Property.TRANSLATION_X, view.getWidth());

            closeAllAnimationCount++;

            animator.addPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
                @Override
                public void onPropertyAnimationStart() {
                }

                @Override
                public void onPropertyAnimationEnd() {
                    closeAllAnimationCount--;
                    if (closeAllAnimationCount > 0) {
                        return;
                    }

                    // Hide the panel after the animation is done.
                    autoHidePanel();

                    // Re-enable the view after the animation is done.
                    TabsListLayout.this.setEnabled(true);

                    // Then actually close all the tabs.
                    closeAllTabs();
                }
            });

            ThreadUtils.postDelayedToUiThread(new Runnable() {
                @Override
                public void run() {
                    animator.start();
                }
            }, cascadeDelay);

            cascadeDelay += ANIMATION_CASCADE_DELAY;
        }
    }

    @Override
    protected boolean addAtIndexRequiresScroll(int index) {
        return index == 0 || index == getAdapter().getItemCount() - 1;
    }

    @Override
    public void onChildAttachedToWindow(View child) {
        // Make sure we reset any attributes that may have been animated in this child's previous
        // incarnation.
        child.setTranslationX(0);
        child.setTranslationY(0);
        child.setAlpha(1);
    }
}
