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
import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.Experiments;
import org.mozilla.gecko.mma.MmaDelegate;
import org.mozilla.gecko.preferences.GeckoPreferences;

/**
 * A container for the pager and the entire first run experience.
 * This is used for animation purposes.
 */
public class FirstrunAnimationContainer extends LinearLayout {
    // See bug 1330714. Need NON_PREF_PREFIX to set from distribution.
    public static final String PREF_FIRSTRUN_ENABLED_OLD = "startpane_enabled";
    // After 57, the pref name will be changed. Thus all user since 57 will check this new pref.
    public static final String PREF_FIRSTRUN_ENABLED = GeckoPreferences.NON_PREF_PREFIX + "startpane_enabled_after_57";

    public static interface OnFinishListener {
        public void onFinish();
    }

    private FirstrunPager pager;
    private boolean visible;
    private OnFinishListener onFinishListener;

    public FirstrunAnimationContainer(Context context) {
        this(context, null);
    }
    public FirstrunAnimationContainer(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public void load(Context appContext, FragmentManager fm) {
        visible = true;
        pager = (FirstrunPager) findViewById(R.id.firstrun_pager);
        pager.load(appContext, fm, new OnFinishListener() {
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

        MmaDelegate.track(MmaDelegate.DISMISS_ONBOARDING);

        visible = false;
        if (onFinishListener != null) {
            onFinishListener.onFinish();
        }
        animateHide();
    }

    private void animateHide() {
        final Animator alphaAnimator = ObjectAnimator.ofFloat(this, "alpha", 0);
        alphaAnimator.setDuration(150);
        alphaAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                FirstrunAnimationContainer.this.setVisibility(View.GONE);
            }
        });

        alphaAnimator.start();
    }

    public boolean showBrowserHint() {
        final int currentPage = pager.getCurrentItem();
        FirstrunPanel currentPanel = (FirstrunPanel) ((FirstrunPager.ViewPagerAdapter) pager.getAdapter()).getItem(currentPage);
        pager.cleanup();
        return currentPanel.shouldShowBrowserHint();
    }

    public void registerOnFinishListener(OnFinishListener listener) {
        this.onFinishListener = listener;
    }
}
