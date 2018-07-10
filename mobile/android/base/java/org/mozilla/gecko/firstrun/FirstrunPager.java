/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.firstrun;

import android.content.Context;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentPagerAdapter;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;

import com.booking.rtlviewpager.RtlViewPager;

import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.home.HomePager.Decor;
import org.mozilla.gecko.home.TabMenuStrip;
import org.mozilla.gecko.restrictions.Restrictions;

import java.util.List;

/**
 * ViewPager containing for our first run pages.
 *
 * @see FirstrunPanel for the first run pages that are used in this pager.
 */
public class FirstrunPager extends RtlViewPager {

    private Context context;
    protected FirstrunPanel.PagerNavigation pagerNavigation;
    private Decor mDecor;

    public FirstrunPager(Context context) {
        this(context, null);
    }

    public FirstrunPager(Context context, AttributeSet attrs) {
        super(context, attrs);
        this.context = context;
    }

    @Override
    public void addView(View child, int index, ViewGroup.LayoutParams params) {
        if (child instanceof Decor) {
            ((RtlViewPager.LayoutParams) params).isDecor = true;
            mDecor = (Decor) child;
            mDecor.setOnTitleClickListener(new TabMenuStrip.OnTitleClickListener() {
                @Override
                public void onTitleClicked(int index) {
                    setCurrentItem(index, true);
                }
            });
        }

        super.addView(child, index, params);
    }

    public void load(Context appContext, FragmentManager fm, final boolean useLocalValues,
                     final FirstrunAnimationContainer.OnFinishListener onFinishListener) {
        final List<FirstrunPagerConfig.FirstrunPanelConfig> panels;

        if (Restrictions.isRestrictedProfile(appContext)) {
            panels = FirstrunPagerConfig.getRestricted(appContext);
        } else if (FirefoxAccounts.firefoxAccountsExist(appContext)) {
            panels = FirstrunPagerConfig.forFxAUser(appContext, useLocalValues);
        } else {
            panels = FirstrunPagerConfig.getDefault(appContext, useLocalValues);
        }

        setAdapter(new ViewPagerAdapter(fm, panels));
        this.pagerNavigation = new FirstrunPanel.PagerNavigation() {
            @Override
            public void next() {
                final int currentPage = FirstrunPager.this.getCurrentItem();
                if (currentPage < FirstrunPager.this.getAdapter().getCount() - 1) {
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
        addOnPageChangeListener(new OnPageChangeListener() {
            @Override
            public void onPageScrolled(int position, float positionOffset, int positionOffsetPixels) {
                mDecor.onPageScrolled(position, positionOffset, positionOffsetPixels);
            }

            @Override
            public void onPageSelected(int i) {
                mDecor.onPageSelected(i);
                Telemetry.sendUIEvent(TelemetryContract.Event.SHOW, TelemetryContract.Method.PANEL, "onboarding." + i);
            }

            @Override
            public void onPageScrollStateChanged(int i) {}
        });

        animateLoad();

        // Record telemetry for first onboarding panel, for baseline.
        Telemetry.sendUIEvent(TelemetryContract.Event.SHOW, TelemetryContract.Method.PANEL, "onboarding.0");
    }

    public void cleanup() {
        setAdapter(null);
    }

    private void animateLoad() {
        setTranslationY(500);
        setAlpha(0);

        final Animator translateAnimator = ObjectAnimator.ofFloat(this, "translationY", 0);
        translateAnimator.setDuration(400);

        final Animator alphaAnimator = ObjectAnimator.ofFloat(this, "alpha", 1);
        alphaAnimator.setStartDelay(200);
        alphaAnimator.setDuration(600);

        final AnimatorSet set = new AnimatorSet();
        set.playTogether(alphaAnimator, translateAnimator);
        set.setStartDelay(400);

        set.start();
    }

    protected class ViewPagerAdapter extends FragmentPagerAdapter {
        private final List<FirstrunPagerConfig.FirstrunPanelConfig> panels;
        private final Fragment[] fragments;

        public ViewPagerAdapter(FragmentManager fm, List<FirstrunPagerConfig.FirstrunPanelConfig> panels) {
            super(fm);
            this.panels = panels;
            this.fragments = new Fragment[panels.size()];
            for (FirstrunPagerConfig.FirstrunPanelConfig panel : panels) {
                mDecor.onAddPagerView(panel.getTitle());
            }

            if (panels.size() > 0) {
                mDecor.onPageSelected(0);
            }
        }

        @Override
        public Fragment getItem(int i) {
            Fragment fragment = this.fragments[i];
            if (fragment == null) {
                FirstrunPagerConfig.FirstrunPanelConfig panelConfig = panels.get(i);
                fragment = Fragment.instantiate(context, panelConfig.getClassname(), panelConfig.getArgs());
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
            // Unused now that we use TabMenuStrip.
            return panels.get(i).getTitle().toUpperCase();
        }
    }
}
