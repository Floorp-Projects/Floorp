/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.firstrun;

import android.content.Context;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentPagerAdapter;
import android.support.v4.view.ViewPager;
import android.util.AttributeSet;
import com.nineoldandroids.animation.Animator;
import com.nineoldandroids.animation.AnimatorSet;
import com.nineoldandroids.animation.ObjectAnimator;
import com.nineoldandroids.view.ViewHelper;

import org.mozilla.gecko.RestrictedProfiles;
import org.mozilla.gecko.animation.TransitionsTracker;

import java.util.List;

public class FirstrunPager extends ViewPager {

    private Context context;
    protected FirstrunPanel.PagerNavigation pagerNavigation;

    public FirstrunPager(Context context) {
        this(context, null);
    }

    public FirstrunPager(Context context, AttributeSet attrs) {
        super(context, attrs);
        this.context = context;
    }

    public void load(Context appContext, FragmentManager fm, final FirstrunPane.OnFinishListener onFinishListener) {
        final List<FirstrunPagerConfig.FirstrunPanelConfig> panels;

        if (RestrictedProfiles.isUserRestricted(context)) {
            panels = FirstrunPagerConfig.getRestricted();
        } else {
            panels = FirstrunPagerConfig.getDefault(appContext);
        }

        setAdapter(new ViewPagerAdapter(fm, panels));
        this.pagerNavigation = new FirstrunPanel.PagerNavigation() {
            @Override
            public void next() {
                final int currentPage = FirstrunPager.this.getCurrentItem();
                if (currentPage < FirstrunPager.this.getChildCount() - 1) {
                    FirstrunPager.this.setCurrentItem(currentPage + 1);
                }
            }

            @Override
            public void finish() {
                if (onFinishListener != null) {
                    onFinishListener.onFinish();
                }
            }
        };

        animateLoad();
    }

    public void hide() {
        setAdapter(null);
    }

    private void animateLoad() {
        ViewHelper.setTranslationY(this, 500);
        ViewHelper.setAlpha(this, 0);

        final Animator translateAnimator = ObjectAnimator.ofFloat(this, "translationY", 0);
        translateAnimator.setDuration(400);

        final Animator alphaAnimator = ObjectAnimator.ofFloat(this, "alpha", 1);
        alphaAnimator.setStartDelay(200);
        alphaAnimator.setDuration(600);

        final AnimatorSet set = new AnimatorSet();
        set.playTogether(alphaAnimator, translateAnimator);
        set.setStartDelay(400);
        TransitionsTracker.track(set);

        set.start();
    }

    private class ViewPagerAdapter extends FragmentPagerAdapter {
        private final List<FirstrunPagerConfig.FirstrunPanelConfig> panels;
        private final Fragment[] fragments;

        public ViewPagerAdapter(FragmentManager fm, List<FirstrunPagerConfig.FirstrunPanelConfig> panels) {
            super(fm);
            this.panels = panels;
            this.fragments = new Fragment[panels.size()];
        }

        @Override
        public Fragment getItem(int i) {
            Fragment fragment = this.fragments[i];
            if (fragment == null) {
                fragment = Fragment.instantiate(context, panels.get(i).getClassname());
                ((FirstrunPanel) fragment).setPagerNavigation(pagerNavigation);
                fragments[i] = fragment;
            }
            return fragment;
        }

        @Override
        public int getCount() {
            return panels.size();
        }

        @Override
        public CharSequence getPageTitle(int i) {
            return context.getString(panels.get(i).getTitleRes()).toUpperCase();
        }
    }
}
