/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.firstrun;

import android.content.Context;
import android.support.v4.app.FragmentManager;
import android.util.AttributeSet;

import android.view.View;
import android.widget.LinearLayout;
import com.nineoldandroids.animation.Animator;
import com.nineoldandroids.animation.AnimatorListenerAdapter;
import com.nineoldandroids.animation.ObjectAnimator;
import org.mozilla.gecko.R;
import org.mozilla.gecko.animation.TransitionsTracker;

public class FirstrunPane extends LinearLayout {
    public static final String PREF_FIRSTRUN_ENABLED = "startpane_enabled";

    public static interface PagerNavigation {
        public void next();
        public void onFinish();
    }

    private FirstrunPager pager;
    private boolean visible;
    private PagerNavigation pagerNavigation;

    public FirstrunPane(Context context) {
        this(context, null);
    }
    public FirstrunPane(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public void load(Context appContext, FragmentManager fm) {
        visible = true;
        pager = (FirstrunPager) findViewById(R.id.firstrun_pager);
        pager.load(appContext, fm, new PagerNavigation() {
            @Override
            public void next() {
                final int currentPage = pager.getCurrentItem();
                if (currentPage < pager.getChildCount() - 1) {
                    pager.setCurrentItem(currentPage + 1);
                }
            }

            @Override
            public void onFinish() {
                hide();
            }
        });
    }

    public boolean isVisible() {
        return visible;
    }

    public void hide() {
        visible = false;
        pager.hide();
        if (pagerNavigation != null) {
            pagerNavigation.onFinish();
        }
        animateHide();
    }

    private void animateHide() {
        final Animator alphaAnimator = ObjectAnimator.ofFloat(this, "alpha", 0);
        alphaAnimator.setDuration(150);
        alphaAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                FirstrunPane.this.setVisibility(View.GONE);
            }
        });

        TransitionsTracker.track(alphaAnimator);

        alphaAnimator.start();
    }

    public void registerOnFinishListener(PagerNavigation listener) {
        this.pagerNavigation = listener;
    }
}
